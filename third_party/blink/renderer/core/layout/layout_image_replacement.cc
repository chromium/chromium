// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/layout_image_replacement.h"

#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/core/image_replacement/document_image_replacements.h"
#include "third_party/blink/renderer/core/image_replacement/image_replacement.h"
#include "third_party/blink/renderer/core/paint/paint_auto_dark_mode.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_recorder.h"

namespace blink {

LayoutImageReplacement::LayoutImageReplacement(HTMLImageElement* element)
    : LayoutImage(element) {}

LayoutImageReplacement::~LayoutImageReplacement() = default;

void LayoutImageReplacement::Trace(Visitor* visitor) const {
  visitor->Trace(children_);
  LayoutImage::Trace(visitor);
}

bool LayoutImageReplacement::IsChildAllowed(LayoutObject* child,
                                            const ComputedStyle& style) const {
  NOT_DESTROYED();
  return child->IsLayoutIFrame();
}

void LayoutImageReplacement::PaintReplaced(
    const PaintInfo& paint_info,
    const PhysicalOffset& paint_offset) const {
  NOT_DESTROYED();
  auto& image = To<HTMLImageElement>(*GetNode());
  const auto* replacements =
      DocumentImageReplacements::FromIfExists(image.GetDocument());
  DCHECK(replacements);
  const auto* replacement = replacements->GetImageReplacement(&image);
  if (!replacement || replacement->ShouldPaintOriginalImage()) {
    LayoutImage::PaintReplaced(paint_info, paint_offset);
  } else {
    // Draw a transparent rectangle to ensure the paint chunk has non-empty
    // bounds. This marks `cc::Layer::draws_content()` as true and prevents the
    // compositor from dropping its `tracked_element_rects` metadata.
    // TODO(b:523314065): We should consider consolidate the tracking of the
    // element rects and the region capture together.
    if (DrawingRecorder::UseCachedDrawingIfPossible(paint_info.context, *this,
                                                    paint_info.phase)) {
      return;
    }
    const PhysicalRect content_rect = PhysicalContentBoxRect() + paint_offset;
    DrawingRecorder recorder(paint_info.context, *this, paint_info.phase,
                             ToEnclosingRect(content_rect));
    paint_info.context.FillRect(
        ToPixelSnappedRect(content_rect), Color::kTransparent,
        PaintAutoDarkMode(StyleRef(),
                          DarkModeFilter::ElementRole::kBackground));
  }
}

}  // namespace blink
