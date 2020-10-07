// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_PAINT_CHUNK_SUBSET_RECORDER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_PAINT_CHUNK_SUBSET_RECORDER_H_

#include "third_party/blink/renderer/platform/graphics/paint/paint_chunk_subset.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_controller.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

// This is for pre-CompositeAfterPaint to get the PaintChunkSubset containing
// paint chunks created during painting in the scope of an instance of this
// class.
class PaintChunkSubsetRecorder {
  STACK_ALLOCATED();

 public:
  explicit PaintChunkSubsetRecorder(PaintController& paint_controller)
      : paint_controller_(paint_controller),
        begin_chunk_index_(paint_controller.NewPaintChunkCount()) {
    DCHECK(!RuntimeEnabledFeatures::CompositeAfterPaintEnabled());
    paint_controller.SetWillForceNewChunk(true);
  }

  PaintChunkSubset Get() {
    paint_controller_.SetWillForceNewChunk(true);
    return PaintChunkSubset(paint_controller_.GetNewPaintArtifactShared(),
                            begin_chunk_index_,
                            paint_controller_.NewPaintChunkCount());
  }

 private:
  PaintController& paint_controller_;
  wtf_size_t begin_chunk_index_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_PAINT_CHUNK_SUBSET_RECORDER_H_
