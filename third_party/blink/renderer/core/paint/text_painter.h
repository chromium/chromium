// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TEXT_PAINTER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TEXT_PAINTER_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/style_variant.h"
#include "third_party/blink/renderer/core/paint/line_relative_rect.h"
#include "third_party/blink/renderer/core/paint/text_painter_base.h"
#include "third_party/blink/renderer/platform/fonts/text_fragment_paint_info.h"
#include "third_party/blink/renderer/platform/graphics/dom_node_id.h"

namespace blink {

class FragmentItem;
class LayoutObject;
class LayoutSVGInlineText;
struct AutoDarkMode;
struct TextFragmentPaintInfo;

// Text painter for LayoutNG, logic shared between legacy layout and LayoutNG
// is implemented in the TextPainterBase base class.
// Operates on PhysicalTextFragments and only paints text and decorations.
// Border painting etc is handled by the TextFragmentPainter class.
// TODO(layout-dev): Does this distinction make sense?
class CORE_EXPORT TextPainter : public TextPainterBase {
  STACK_ALLOCATED();

 public:
  class SvgTextPaintState final {
    STACK_ALLOCATED();

   public:
    SvgTextPaintState(const LayoutSVGInlineText&,
                      const ComputedStyle&,
                      StyleVariant style_variant,
                      PaintFlags paint_flags);
    SvgTextPaintState(const LayoutSVGInlineText&,
                      const ComputedStyle&,
                      Color text_match_color);

    const LayoutSVGInlineText& InlineText() const;
    const LayoutObject& TextDecorationObject() const;
    const ComputedStyle& Style() const;
    bool IsPaintingSelection() const;
    PaintFlags GetPaintFlags() const;
    bool IsRenderingClipPathAsMaskImage() const;
    bool IsPaintingTextMatch() const;
    // This is callable only if IsPaintingTextMatch().
    Color TextMatchColor() const;

    AffineTransform& EnsureShaderTransform();
    const AffineTransform* GetShaderTransform() const;

   private:
    const LayoutSVGInlineText& layout_svg_inline_text_;
    const ComputedStyle& style_;
    absl::optional<AffineTransform> shader_transform_;
    absl::optional<Color> text_match_color_;
    StyleVariant style_variant_ = StyleVariant::kStandard;
    PaintFlags paint_flags_ = PaintFlag::kNoFlag;
    bool is_painting_selection_ = false;
    friend class TextPainter;
  };

  TextPainter(GraphicsContext& context,
              const Font& font,
              const gfx::Rect& visual_rect,
              const LineRelativeOffset& text_origin,
              InlinePaintContext* inline_context,
              bool horizontal)
      : TextPainterBase(context, font, text_origin, inline_context, horizontal),
        visual_rect_(visual_rect) {
    DCHECK(inline_context_);
  }
  ~TextPainter() = default;

  void Paint(const TextFragmentPaintInfo& fragment_paint_info,
             const TextPaintStyle&,
             DOMNodeId,
             const AutoDarkMode& auto_dark_mode,
             ShadowMode = kBothShadowsAndTextProper);

  void PaintSelectedText(const TextFragmentPaintInfo& fragment_paint_info,
                         unsigned selection_start,
                         unsigned selection_end,
                         const TextPaintStyle& text_style,
                         const TextPaintStyle& selection_style,
                         const LineRelativeRect& selection_rect,
                         DOMNodeId node_id,
                         const AutoDarkMode& auto_dark_mode);

  void PaintDecorationsExceptLineThrough(
      const TextFragmentPaintInfo& fragment_paint_info,
      const FragmentItem& text_item,
      const PaintInfo& paint_info,
      const ComputedStyle& style,
      const TextPaintStyle& text_style,
      TextDecorationInfo& decoration_info,
      TextDecorationLine lines_to_paint);

  void PaintDecorationsOnlyLineThrough(const FragmentItem& text_item,
                                       const PaintInfo& paint_info,
                                       const ComputedStyle& style,
                                       const TextPaintStyle& text_style,
                                       TextDecorationInfo& decoration_info);

  SvgTextPaintState& SetSvgState(const LayoutSVGInlineText&,
                                 const ComputedStyle&,
                                 StyleVariant style_variant,
                                 PaintFlags paint_flags);
  SvgTextPaintState& SetSvgState(const LayoutSVGInlineText& svg_inline_text,
                                 const ComputedStyle& style,
                                 Color text_match_color);
  SvgTextPaintState* GetSvgState();

 protected:
  void ClipDecorationsStripe(const TextFragmentPaintInfo&,
                             float upper,
                             float stripe_width,
                             float dilation) override;

 private:
  template <PaintInternalStep step>
  void PaintInternalFragment(const TextFragmentPaintInfo&,
                             DOMNodeId node_id,
                             const AutoDarkMode& auto_dark_mode);

  void PaintSvgTextFragment(const TextFragmentPaintInfo&,
                            DOMNodeId node_id,
                            const AutoDarkMode& auto_dark_mode);
  void PaintSvgDecorationsExceptLineThrough(
      const TextFragmentPaintInfo&,
      const TextDecorationOffset& decoration_offset,
      TextDecorationInfo& decoration_info,
      TextDecorationLine lines_to_paint,
      const PaintInfo& paint_info,
      const TextPaintStyle& text_style);
  void PaintSvgDecorationsOnlyLineThrough(TextDecorationInfo& decoration_info,
                                          const PaintInfo& paint_info,
                                          const TextPaintStyle& text_style);

  const gfx::Rect visual_rect_;
  absl::optional<SvgTextPaintState> svg_text_paint_state_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TEXT_PAINTER_H_
