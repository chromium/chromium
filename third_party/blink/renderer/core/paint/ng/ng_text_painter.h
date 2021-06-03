// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_NG_NG_TEXT_PAINTER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_NG_NG_TEXT_PAINTER_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/paint/text_painter_base.h"
#include "third_party/blink/renderer/platform/fonts/ng_text_fragment_paint_info.h"
#include "third_party/blink/renderer/platform/graphics/dom_node_id.h"

namespace blink {

class LayoutSVGInlineText;
class NGFragmentItem;
struct NGTextFragmentPaintInfo;

// Text painter for LayoutNG, logic shared between legacy layout and LayoutNG
// is implemented in the TextPainterBase base class.
// Operates on NGPhysicalTextFragments and only paints text and decorations.
// Border painting etc is handled by the NGTextFragmentPainter class.
// TODO(layout-dev): Does this distinction make sense?
class CORE_EXPORT NGTextPainter : public TextPainterBase {
  STACK_ALLOCATED();

 public:
  class SvgTextPaintState final {
   public:
    SvgTextPaintState(const LayoutSVGInlineText&,
                      const ComputedStyle&,
                      bool is_rendering_clip_path_as_mask_image);

    const LayoutSVGInlineText& InlineText() const;
    const ComputedStyle& Style() const;
    bool IsPaintingSelection() const;
    bool IsRenderingClipPathAsMaskImage() const;

    AffineTransform& EnsureShaderTransform();
    const AffineTransform* GetShaderTransform() const;

   private:
    const LayoutSVGInlineText& layout_svg_inline_text_;
    const ComputedStyle& style_;
    absl::optional<AffineTransform> shader_transform_;
    bool is_painting_selection_ = false;
    bool is_rendering_clip_path_as_mask_image_ = false;
    friend class NGTextPainter;
  };

  NGTextPainter(GraphicsContext& context,
                const Font& font,
                const NGTextFragmentPaintInfo& fragment_paint_info,
                const IntRect& visual_rect,
                const PhysicalOffset& text_origin,
                const PhysicalRect& text_frame_rect,
                bool horizontal)
      : TextPainterBase(context,
                        font,
                        text_origin,
                        text_frame_rect,
                        horizontal),
        fragment_paint_info_(fragment_paint_info),
        visual_rect_(visual_rect) {}
  ~NGTextPainter() = default;

  void ClipDecorationsStripe(float upper,
                             float stripe_width,
                             float dilation) override;
  void Paint(unsigned start_offset,
             unsigned end_offset,
             unsigned length,
             const TextPaintStyle&,
             DOMNodeId,
             ShadowMode = kBothShadowsAndTextProper);

  void PaintSelectedText(unsigned start_offset,
                         unsigned end_offset,
                         unsigned length,
                         const TextPaintStyle& text_style,
                         const TextPaintStyle& selection_style,
                         const PhysicalRect& selection_rect,
                         DOMNodeId node_id);

  void PaintDecorationsExceptLineThrough(
      const NGFragmentItem& text_item,
      const PaintInfo& paint_info,
      const ComputedStyle& style,
      const TextPaintStyle& text_style,
      const PhysicalRect& decoration_rect,
      const absl::optional<AppliedTextDecoration>& selection_decoration,
      bool* has_line_through_decoration);

  void PaintDecorationsOnlyLineThrough(
      const NGFragmentItem& text_item,
      const PaintInfo& paint_info,
      const ComputedStyle& style,
      const TextPaintStyle& text_style,
      const PhysicalRect& decoration_rect,
      const absl::optional<AppliedTextDecoration>& selection_decoration);

  SvgTextPaintState& SetSvgState(const LayoutSVGInlineText&,
                                 const ComputedStyle&,
                                 bool is_rendering_clip_path_as_mask_image);
  SvgTextPaintState* GetSvgState();

 private:
  template <PaintInternalStep step>
  void PaintInternalFragment(unsigned from, unsigned to, DOMNodeId node_id);

  template <PaintInternalStep step>
  void PaintInternal(unsigned start_offset,
                     unsigned end_offset,
                     unsigned truncation_point,
                     DOMNodeId node_id);

  void PaintSvgTextFragment(DOMNodeId node_id);

  NGTextFragmentPaintInfo fragment_paint_info_;
  const IntRect& visual_rect_;
  absl::optional<SvgTextPaintState> svg_text_paint_state_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_NG_NG_TEXT_PAINTER_H_
