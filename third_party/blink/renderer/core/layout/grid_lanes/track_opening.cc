// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/grid_lanes/track_opening.h"

#include "third_party/blink/renderer/core/layout/grid/grid_item.h"

namespace blink {

AlignmentCandidate::AlignmentCandidate(GridItemData* item,
                                       AlignmentSpace* alignment_space,
                                       LayoutUnit original_stacking_size,
                                       wtf_size_t item_index,
                                       GridLayoutSubtree* layout_subtree,
                                       const ItemIndexPath* item_index_path)
    : item(item),
      alignment_space(alignment_space),
      layout_subtree(layout_subtree),
      original_stacking_size(original_stacking_size) {
  CHECK(alignment_space);
  if (item_index_path) {
    CHECK(!item_index_path->empty());
    this->item_index_path = *item_index_path;
  } else {
    CHECK_NE(item_index, kNotFound);
    this->item_index_path.push_back(item_index);
  }
}

void AlignmentCandidate::Trace(Visitor* visitor) const {
  visitor->Trace(item);
  visitor->Trace(alignment_space);
  visitor->Trace(layout_subtree);
}

void TrackOpening::Trace(Visitor* visitor) const {
  visitor->Trace(alignment_candidate);
}

}  // namespace blink
