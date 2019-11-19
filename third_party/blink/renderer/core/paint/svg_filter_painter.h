// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_SVG_FILTER_PAINTER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_SVG_FILTER_PAINTER_H_

#include <memory>
#include "base/macros.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_controller.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class LayoutObject;
class LayoutSVGResourceFilter;

class SVGFilterRecordingContext {
  USING_FAST_MALLOC(SVGFilterRecordingContext);

 public:
  explicit SVGFilterRecordingContext(GraphicsContext& initial_context)
      : initial_context_(initial_context) {}

  GraphicsContext* BeginContent();
  sk_sp<PaintRecord> EndContent(const FloatRect&);
  void Abort();

  GraphicsContext& PaintingContext() const { return initial_context_; }

 private:
  std::unique_ptr<PaintController> paint_controller_;
  std::unique_ptr<GraphicsContext> context_;
  GraphicsContext& initial_context_;
  DISALLOW_COPY_AND_ASSIGN(SVGFilterRecordingContext);
};

class SVGFilterPainter {
  STACK_ALLOCATED();

 public:
  SVGFilterPainter(LayoutSVGResourceFilter& filter) : filter_(filter) {}

  // Returns the context that should be used to paint the filter contents, or
  // null if the content should not be recorded.
  GraphicsContext* PrepareEffect(const LayoutObject&,
                                 SVGFilterRecordingContext&);
  void FinishEffect(const LayoutObject&, SVGFilterRecordingContext&);

 private:
  LayoutSVGResourceFilter& filter_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_SVG_FILTER_PAINTER_H_
