// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_SVG_INLINE_TEXT_BOX_PAINTER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_SVG_INLINE_TEXT_BOX_PAINTER_H_

#include "third_party/blink/renderer/core/layout/svg/layout_svg_resource_paint_server.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class AffineTransform;
class DocumentMarker;
class Font;
struct PaintInfo;
class LayoutPoint;
class LayoutSVGInlineText;
class ComputedStyle;
class SVGInlineTextBox;
struct SVGTextFragment;
class TextMarkerBase;
class TextRun;

struct SVGTextFragmentWithRange {
  SVGTextFragmentWithRange(const SVGTextFragment& fragment,
                           int start_position,
                           int end_position)
      : fragment(fragment),
        start_position(start_position),
        end_position(end_position) {}
  const SVGTextFragment& fragment;
  int start_position;
  int end_position;
};

class SVGInlineTextBoxPainter {
  STACK_ALLOCATED();

 public:
  SVGInlineTextBoxPainter(const SVGInlineTextBox& svg_inline_text_box)
      : svg_inline_text_box_(svg_inline_text_box) {}
  void Paint(const PaintInfo&, const LayoutPoint& paint_offset);
  void PaintSelectionBackground(const PaintInfo&);
  void PaintTextMarkerForeground(const PaintInfo&,
                                 const LayoutPoint&,
                                 const TextMarkerBase&,
                                 const ComputedStyle&,
                                 const Font&);
  void PaintTextMarkerBackground(const PaintInfo&,
                                 const LayoutPoint&,
                                 const TextMarkerBase&,
                                 const ComputedStyle&,
                                 const Font&);

 private:
  bool ShouldPaintSelection(const PaintInfo&) const;
  void PaintTextFragments(const PaintInfo&, LayoutObject&);
  void PaintDecoration(const PaintInfo&,
                       TextDecoration,
                       const SVGTextFragment&);
  bool SetupTextPaint(const PaintInfo&,
                      const ComputedStyle&,
                      LayoutSVGResourceMode,
                      PaintFlags&,
                      const AffineTransform*);
  void PaintText(const PaintInfo&,
                 TextRun&,
                 const SVGTextFragment&,
                 int start_position,
                 int end_position,
                 const PaintFlags&);
  void PaintText(const PaintInfo&,
                 const ComputedStyle&,
                 const ComputedStyle& selection_style,
                 const SVGTextFragment&,
                 LayoutSVGResourceMode,
                 bool should_paint_selection,
                 const AffineTransform*);
  Vector<SVGTextFragmentWithRange> CollectTextMatches(
      const DocumentMarker&) const;
  Vector<SVGTextFragmentWithRange> CollectFragmentsInRange(
      int start_position,
      int end_position) const;
  LayoutObject& InlineLayoutObject() const;
  LayoutObject& ParentInlineLayoutObject() const;
  LayoutSVGInlineText& InlineText() const;

  const SVGInlineTextBox& svg_inline_text_box_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_SVG_INLINE_TEXT_BOX_PAINTER_H_
