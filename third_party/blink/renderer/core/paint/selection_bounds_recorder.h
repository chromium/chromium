// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_SELECTION_BOUNDS_RECORDER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_SELECTION_BOUNDS_RECORDER_H_

#include "third_party/blink/renderer/core/layout/api/selection_state.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_rect.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class FrameSelection;
class PaintController;

// This class is used for recording painted selection bounds when
// CompositeAfterPaint is enabled. Based on the SelectionState and provided
// |selection_rect|, records the appropriate bounds via the paint controller.
// These bounds are consumed at composition time by PaintArtifactCompositor and
// pushed to the LayerTreeHost. All of the work happens in the destructor to
// ensure this information recorded after any painting is completed, even if
// a cached drawing is re-used.
class SelectionBoundsRecorder {
  STACK_ALLOCATED();

 public:
  SelectionBoundsRecorder(SelectionState state,
                          PhysicalRect selection_rect,
                          PaintController& paint_controller);

  ~SelectionBoundsRecorder();

  static bool ShouldRecordSelection(const FrameSelection&, SelectionState);

 private:
  const SelectionState state_;
  PhysicalRect selection_rect_;
  PaintController& paint_controller_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_SELECTION_BOUNDS_RECORDER_H_
