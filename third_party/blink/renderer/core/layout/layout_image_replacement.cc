// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/layout_image_replacement.h"

#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/core/image_replacement/document_image_replacements.h"
#include "third_party/blink/renderer/core/image_replacement/image_replacement.h"

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
  }
}

}  // namespace blink
