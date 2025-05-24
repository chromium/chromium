// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_TREE_COUNTING_CHECKER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_TREE_COUNTING_CHECKER_H_

#include "third_party/blink/renderer/core/animation/css_interpolation_type.h"

namespace blink {

// Conversion checker used to track whether an interpolation rely on tree
// counting functions like sibling-index() and sibling-count(). Such functions
// resolve in MaybeConvertValue() and inserting/removing siblings of the
// animated element may need re-resolving of keyframe property values.
class TreeCountingChecker : public CSSInterpolationType::CSSConversionChecker {
 public:
  TreeCountingChecker(unsigned nth_child_index, unsigned nth_last_child_index)
      : nth_child_index_(nth_child_index),
        nth_last_child_index_(nth_last_child_index) {}

  static TreeCountingChecker* Create(const CSSLengthResolver& length_resolver);

  bool IsValid(const StyleResolverState& state,
               const InterpolationValue& underlying) const final;

 private:
  // The value currently used for sibling-index().
  unsigned nth_child_index_;
  // In combination with nth_child_index_, the nth_last_child_index_ is used to
  // detect any changes in sibling-count().
  unsigned nth_last_child_index_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_TREE_COUNTING_CHECKER_H_
