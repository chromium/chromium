// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_TREE_TRAVERSAL_UTILS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_TREE_TRAVERSAL_UTILS_H_

#include "base/functional/function_ref.h"

namespace blink {

class PhysicalBoxFragment;

// How to proceed after having processed a fragment (via the callback).
enum class FragmentTraversalNextStep {
  // Continue traversal normally.
  kContinue,
  // Skip any children, then continue traversal normally.
  kSkipChildren,
};

// Visit every box fragment descendant in the subtree, depth-first, from left to
// right, including those inside inline formatting contexts (FragmentItem), and
// invoke the callback for each. Fragments that both have PhysicalBoxFragment
// children and an inline formatting context (rare) will walk the
// PhysicalBoxFragment children first. For each descendant visited, the
// specified callback will be called, and its return value determines how to
// proceed with the traversal afterwards.
void ForAllBoxFragmentDescendants(
    const PhysicalBoxFragment&,
    base::FunctionRef<FragmentTraversalNextStep(const PhysicalBoxFragment&)>);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_TREE_TRAVERSAL_UTILS_H_
