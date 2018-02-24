#ifndef NEL_GIBBS_FIELD_H_
#define NEL_GIBBS_FIELD_H_

#include <core/random.h>
#include <math/log.h>
#include "position.h"
#include "energy_functions.h"

namespace nel {

using namespace core;

template<typename Map>
class gibbs_field
{
	Map& map;
	position* patch_positions;
	unsigned int patch_count;

	unsigned int n;
	unsigned int item_type_count;

	typedef typename Map::patch_type patch_type;
	typedef typename Map::item_type item_type;

public:
	/* NOTE: `patches` and `patch_positions` are used directly, and not copied */
	gibbs_field(Map& map, position* patch_positions, unsigned int patch_count,
			unsigned int n, unsigned int item_type_count) :
		map(map), patch_positions(patch_positions), patch_count(patch_count),
		n(n), item_type_count(item_type_count) { }

	~gibbs_field() { }

	template<typename RNGType>
	void sample(RNGType& rng) {
		for (unsigned int i = 0; i < patch_count * n * n; i++)
			sample_cell(rng, patch_positions[sample_uniform(rng, patch_count)], {sample_uniform(rng, n), sample_uniform(rng, n)});
	}

private:
	template<typename RNGType>
	inline unsigned int sample_uniform(RNGType& rng, unsigned int n) {
		return rng() % n;
	}

	template<typename RNGType>
	inline void sample_cell(RNGType& rng,
			const position& patch_position,
			const position& position_within_patch)
	{
		patch_type* neighborhood[4];
		position neighbor_positions[4];
		position world_position = patch_position * n + position_within_patch;

		unsigned int patch_index;
		unsigned int neighbor_count = map.get_neighborhood(world_position, neighborhood, neighbor_positions, patch_index);

		float* log_probabilities = (float*) malloc(sizeof(float) * (item_type_count + 1));
		unsigned int old_item_index = 0, old_item_type = item_type_count;
		for (unsigned int i = 0; i < item_type_count; i++) {
			/* compute the energy contribution of this cell when the item type is `i` */
			log_probabilities[i] = map.intensity(world_position, i);
			for (unsigned int j = 0; j < neighbor_count; j++) {
				const array<item_type>& items = neighborhood[j]->items;
				for (unsigned int m = 0; m < items.length; m++) {
					if (items[m].location == world_position) {
						old_item_index = m;
						old_item_type = items[m].item_type;
						continue; /* ignore the current position */
					}

					log_probabilities[i] += map.interaction(world_position, items[m].location, i, items[m].item_type);
				}
			}
		}

		log_probabilities[item_type_count] = 0.0;
		normalize_exp(log_probabilities, item_type_count + 1);
		float random = (float) rng() / engine.max();
		unsigned int sampled_item_type = select_categorical(
				log_probabilities, random, item_type_count + 1);
		free(log_probabilities);

		patch_type& current_patch = *neighborhood[patch_index];
		if (old_item_type == sampled_item_type) {
			/* the Gibbs step didn't change anything */
			return;
		} if (old_item_type < item_type_count) {
			/* remove the old item position */
			current_patch.items.remove(old_item_index);
		} if (sampled_item_type < item_type_count) {
			/* add the new item position */
			current_patch.items.add({sampled_item_type, world_position, 0, 0});
		}
	}
};

} /* namespace nel */

#endif /* NEL_GIBBS_FIELD_H_ */
