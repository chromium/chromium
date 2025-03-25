// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CANVAS_INTERVENTIONS_CANVAS_INTERVENTIONS_HELPER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CANVAS_INTERVENTIONS_CANVAS_INTERVENTIONS_HELPER_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_rendering_context.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource_host.h"
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
  enum class CanvasInterventionType {
    kNone,
    kNoise,
  };

  static const char kSupplementName[];

  static CanvasInterventionsHelper* Create(ExecutionContext* execution_context);

  explicit CanvasInterventionsHelper(ExecutionContext& execution_context);
  virtual ~CanvasInterventionsHelper() = default;

  // If allowed, performs noising on a copy of the snapshot StaticBitmapImage
  // and returns the noised snapshot, otherwise it will return the original
  // inputted snapshot.
  static bool MaybeNoiseSnapshot(CanvasRenderingContext* rendering_context,
                                 ExecutionContext* execution_context,
                                 scoped_refptr<StaticBitmapImage>& snapshot,
                                 RasterMode raster_mode);

  void Trace(Visitor* visitor) const override {
    visitor->Trace(execution_context_);
    Supplement<ExecutionContext>::Trace(visitor);
  }

 private:
  ExecutionContext* GetExecutionContext() const {
    return execution_context_.Get();
  }

  Member<ExecutionContext> execution_context_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CANVAS_INTERVENTIONS_CANVAS_INTERVENTIONS_HELPER_H_
