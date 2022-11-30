// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_SVG_INLINE_TEXT_BOX_PAINTER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_SVG_INLINE_TEXT_BOX_PAINTER_H_

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/core/paint/svg_object_painter.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class AffineTransform;
class ComputedStyle;
class DocumentMarker;
class Font;
class LayoutSVGInlineText;
class SelectionBoundsRecorder;
class SVGInlineTextBox;
class TextRun;
struct PaintInfo;
struct PhysicalOffset;
struct SVGTextFragment;

struct SVGTextFragmentWithRange {
  DISALLOW_NEW();

 public:
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
  void Paint(const PaintInfo&, const PhysicalOffset& paint_offset);
  void PaintSelectionBackground(const PaintInfo&);
  void PaintTextMarkerForeground(const PaintInfo&,
                                 const PhysicalOffset&,
                                 const DocumentMarker&,
                                 const ComputedStyle&,
                                 const Font&);
  void PaintTextMarkerBackground(const PaintInfo&,
                                 const PhysicalOffset&,
                                 const DocumentMarker&,
                                 const ComputedStyle&,
                                 const Font&);

 private:
  bool ShouldPaintSelection(const PaintInfo&) const;
  void PaintTextFragments(const PaintInfo&, LayoutObject&);
  void PaintDecoration(const PaintInfo&,
                       TextDecorationLine,
                       const SVGTextFragment&);
  bool SetupTextPaint(const PaintInfo&,
                      const ComputedStyle&,
                      LayoutSVGResourceMode,
                      cc::PaintFlags&,
                      const AffineTransform*);
  void PaintText(const PaintInfo&,
                 TextRun&,
                 const SVGTextFragment&,
                 int start_position,
                 int end_position,
                 const cc::PaintFlags&);
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

  void RecordSelectionBoundsForRange(
      int start_position,
      int end_position,
      SelectionState selection_state,
      const ComputedStyle& style,
      PaintController& paint_controller,
      absl::optional<SelectionBoundsRecorder>& bounds_recorder);

  const SVGInlineTextBox& svg_inline_text_box_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_SVG_INLINE_TEXT_BOX_PAINTER_H_
