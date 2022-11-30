// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/invalidation/invalidation_flags.h"

namespace blink {

void InvalidationFlags::Merge(const InvalidationFlags& other) {
  invalidate_custom_pseudo_ |= other.invalidate_custom_pseudo_;
  tree_boundary_crossing_ |= other.tree_boundary_crossing_;
  insertion_point_crossing_ |= other.insertion_point_crossing_;
  whole_subtree_invalid_ |= other.whole_subtree_invalid_;
  invalidates_slotted_ |= other.invalidates_slotted_;
  invalidates_parts_ |= other.invalidates_parts_;
}

bool InvalidationFlags::operator==(const InvalidationFlags& other) const {
  return invalidate_custom_pseudo_ == other.invalidate_custom_pseudo_ &&
         tree_boundary_crossing_ == other.tree_boundary_crossing_ &&
         insertion_point_crossing_ == other.insertion_point_crossing_ &&
         whole_subtree_invalid_ == other.whole_subtree_invalid_ &&
         invalidates_slotted_ == other.invalidates_slotted_ &&
         invalidates_parts_ == other.invalidates_parts_;
}

}  // namespace blink
