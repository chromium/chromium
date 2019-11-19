// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_SELECTION_ADJUSTER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_SELECTION_ADJUSTER_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/editing/forward.h"
#include "third_party/blink/renderer/core/editing/text_granularity.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

// |SelectionAdjuster| adjusts positions in |VisibleSelection| directly without
// calling |validate()|. Users of |SelectionAdjuster| should keep invariant of
// |VisibleSelection|, e.g. all positions are canonicalized.
class CORE_EXPORT SelectionAdjuster final {
  STATIC_ONLY(SelectionAdjuster);

 public:
  static SelectionInDOMTree AdjustSelectionRespectingGranularity(
      const SelectionInDOMTree&,
      TextGranularity);
  static SelectionInFlatTree AdjustSelectionRespectingGranularity(
      const SelectionInFlatTree&,
      TextGranularity);
  static SelectionInDOMTree AdjustSelectionToAvoidCrossingShadowBoundaries(
      const SelectionInDOMTree&);
  static SelectionInFlatTree AdjustSelectionToAvoidCrossingShadowBoundaries(
      const SelectionInFlatTree&);
  static SelectionInDOMTree AdjustSelectionToAvoidCrossingEditingBoundaries(
      const SelectionInDOMTree&);
  static SelectionInFlatTree AdjustSelectionToAvoidCrossingEditingBoundaries(
      const SelectionInFlatTree&);
  static SelectionInDOMTree AdjustSelectionType(const SelectionInDOMTree&);
  static SelectionInFlatTree AdjustSelectionType(const SelectionInFlatTree&);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_SELECTION_ADJUSTER_H_
