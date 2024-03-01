// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_DOCUMENT_MARKER_PAINTER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_DOCUMENT_MARKER_PAINTER_H_

#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class ComputedStyle;
class Document;
class GraphicsContext;
class LayoutUnit;
class StyleableMarker;
struct LineRelativeRect;
struct PhysicalOffset;

// Document marker painter for both LayoutNG and legacy layout.
// This paints text decorations for spell/grammer check, find-in-page, and
// input method.
class DocumentMarkerPainter {
  STATIC_ONLY(DocumentMarkerPainter);

 public:
  static void PaintStyleableMarkerUnderline(GraphicsContext& context,
                                            const PhysicalOffset& box_origin,
                                            const StyleableMarker& marker,
                                            const ComputedStyle& style,
                                            const Document& document,
                                            const LineRelativeRect& marker_rect,
                                            LayoutUnit logical_height,
                                            bool in_dark_mode);
  static bool ShouldPaintMarkerUnderline(const StyleableMarker& marker);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_DOCUMENT_MARKER_PAINTER_H_
