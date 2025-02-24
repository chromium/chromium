// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CANVAS_INTERVENTIONS_CANVAS_INTERVENTIONS_HELPER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CANVAS_INTERVENTIONS_CANVAS_INTERVENTIONS_HELPER_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/platform/graphics/static_bitmap_image.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

class ExecutionContext;

class CORE_EXPORT CanvasInterventionsHelper
    : public GarbageCollected<CanvasInterventionsHelper>,
      public Supplement<ExecutionContext> {
 public:
  static const char kSupplementName[];

  static CanvasInterventionsHelper* Create(ExecutionContext* execution_context);

  explicit CanvasInterventionsHelper(ExecutionContext& execution_context);
  virtual ~CanvasInterventionsHelper() = default;

  // If allowed, performs noising on a copy of the snapshot StaticBitmapImage
  // and returns the noised snapshot, otherwise it will return the original
  // inputted snapshot.
  scoped_refptr<StaticBitmapImage> MaybeGetNoisedSnapshot(
      scoped_refptr<StaticBitmapImage> input_snapshot);

  void Trace(Visitor* visitor) const override {
    visitor->Trace(execution_context_);
    Supplement<ExecutionContext>::Trace(visitor);
  }

 private:
  // Returns true when all criteria to apply noising are met. Currently this
  // entails that the CanvasInterventions RuntimeEnabledFeature is enabled or
  // force enabled.
  bool ShouldApplyNoise() const;

  // Uses the source_pixels to generate a noised version of the pixels, and
  // overrides source_pixels in place with the noised version. Returns true if
  // pixels were noised.
  bool MaybeNoisePixels(base::span<uint8_t> source_pixels,
                        uint32_t sw,
                        uint32_t sh);

  ExecutionContext* GetExecutionContext() const {
    return execution_context_.Get();
  }

  Member<ExecutionContext> execution_context_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CANVAS_INTERVENTIONS_CANVAS_INTERVENTIONS_HELPER_H_
