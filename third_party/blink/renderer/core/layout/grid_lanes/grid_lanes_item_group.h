// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GRID_LANES_GRID_LANES_ITEM_GROUP_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GRID_LANES_GRID_LANES_ITEM_GROUP_H_

#include "third_party/blink/renderer/core/layout/grid/grid_item.h"
#include "third_party/blink/renderer/core/style/grid_area.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"

namespace blink {

// From https://drafts.csswg.org/css-grid-3/#track-sizing-performance:
//   "Separate all the grid-lanes items into item groups, according to the
//   following properties: the span of the item, the placement of the item
//   (i.e., which tracks it is allowed to be placed in), and the item’s
//   baseline-sharing group."
//
// This class represents the properties that define an item group and will be
// used as the key for grouping items within a hash table.
class GridLanesItemGroupProperties {
  DISALLOW_NEW();

 public:
  explicit GridLanesItemGroupProperties() = default;
  explicit GridLanesItemGroupProperties(HashTableDeletedValueType)
      : is_deleted_(true) {}

  explicit GridLanesItemGroupProperties(
      const GridSpan& item_span,
      std::optional<BaselineGroup> baseline_group = std::nullopt)
      : item_span_(item_span), baseline_group_(baseline_group) {}

  bool operator==(const GridLanesItemGroupProperties& other) const {
    return is_deleted_ == other.is_deleted_ && item_span_ == other.item_span_ &&
           baseline_group_ == other.baseline_group_;
  }

  unsigned GetHash() const {
    if (!item_span_) {
      // The default and "deleted" instances have the same initial values, so we
      // provide them with a different hash value to avoid collisions.
      return is_deleted_ ? std::numeric_limits<unsigned>::max() : 0;
    }
    unsigned hash = item_span_->GetHash();
    if (baseline_group_) {
      // The baseline group must be incorporated into the hash to ensure items
      // with the same span but different baseline groups are placed in
      // separate groups and avoid collisions.
      hash = HashInts(hash, static_cast<unsigned>(*baseline_group_));
    }
    return hash;
  }

  bool IsHashTableDeletedValue() const { return is_deleted_; }

  const GridSpan& Span() const {
    DCHECK(item_span_);
    return *item_span_;
  }

 private:
  FRIEND_TEST_ALL_PREFIXES(GridLanesLayoutAlgorithmTest,
                           CollectGridLanesItemGroupsWithBaseline);

  // `HashTraits` requires a way to create a "deleted value". In this class it's
  // the same as the default value but has this flag set to `true`.
  bool is_deleted_{false};

  std::optional<GridSpan> item_span_;
  // The baseline-sharing group property that items in this group share. Items
  // are grouped by this value: `std::nullopt` groups items that do not
  // participate in baseline alignment, `kMajor` groups first baseline aligned
  // items, and `kMinor` groups last baseline aligned items.
  std::optional<BaselineGroup> baseline_group_;
};

struct GridLanesItemGroup {
  DISALLOW_NEW();

  void Trace(Visitor* visitor) const { visitor->Trace(items); }

  GridItems::GridItemDataVector items;
  GridLanesItemGroupProperties properties;
};

using GridLanesItemGroupMap =
    HeapHashMap<GridLanesItemGroupProperties, GridItems::GridItemDataVector>;
using GridLanesItemGroups = HeapVector<GridLanesItemGroup, 16>;

template <>
struct HashTraits<GridLanesItemGroupProperties>
    : SimpleClassHashTraits<GridLanesItemGroupProperties> {};

}  // namespace blink

WTF_ALLOW_MOVE_INIT_AND_COMPARE_WITH_MEM_FUNCTIONS(blink::GridLanesItemGroup)

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GRID_LANES_GRID_LANES_ITEM_GROUP_H_
