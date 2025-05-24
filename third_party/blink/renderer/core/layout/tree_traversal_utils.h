// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_TREE_TRAVERSAL_UTILS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_TREE_TRAVERSAL_UTILS_H_

#include "third_party/blink/renderer/platform/geometry/physical_offset.h"

namespace blink {

class LayoutInline;
class PhysicalBoxFragment;

enum PhysicalFragmentTraversalOptions {
  kFragmentTraversalOptionNone = 0,

  // Look for culled inlines and call BoxFragmentDescendantsCulledInlineCallback
  // when found.
  //
  // TODO(crbug.com/406288653): Get rid of this.
  kFragmentTraversalOptionCulledInlines = 1,
};

class PhysicalFragmentTraversalListener {
 public:
  // How to proceed after having processed a fragment.
  enum NextStep {
    // Continue traversal normally.
    kContinue,
    // Skip any children, don't call HandleExit(), then continue traversal
    // normally.
    kSkipChildren,
  };

  // Call when entering a fragment, before descending into the subtree.
  virtual NextStep HandleEntry(const PhysicalBoxFragment&,
                               PhysicalOffset,
                               bool is_first_for_node) {
    return kContinue;
  }

  // Call when exiting a fragment, after having walked the subtree.
  virtual void HandleExit(const PhysicalBoxFragment&, PhysicalOffset) {}

  // TODO(crbug.com/406288653): Get rid of this.
  virtual void HandleCulledInline(const LayoutInline& culled_inline,
                                  bool is_first_for_node) {}
};

// Visit every box fragment descendant in the subtree, depth-first, from left to
// right, including those inside inline formatting contexts (FragmentItem), and
// invoke the listener for each. Fragments that both have PhysicalBoxFragment
// children and an inline formatting context (rare) will walk the
// PhysicalBoxFragment children first. For each descendant visited, when
// entering it, HandleEntry() will be called, and its return value determines
// how to proceed with the traversal afterwards. As an added bonus, mostly
// thanks to crbug.com/406288653 , culled inlines may also be visited, via
// HandleCulledInline().
void ForAllBoxFragmentDescendants(const PhysicalBoxFragment&,
                                  PhysicalFragmentTraversalOptions,
                                  PhysicalFragmentTraversalListener&);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_TREE_TRAVERSAL_UTILS_H_
