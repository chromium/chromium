/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "third_party/blink/renderer/core/layout/layout_text_combine.h"

#include "base/stl_util.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"

namespace blink {

const float kTextCombineMargin = 1.1f;  // Allow em + 10% margin

LayoutTextCombine::LayoutTextCombine(Node* node,
                                     scoped_refptr<StringImpl> string)
    : LayoutText(node, std::move(string)),
      combined_text_width_(0),
      scale_x_(1.0f),
      is_combined_(false) {}

void LayoutTextCombine::StyleDidChange(StyleDifference diff,
                                       const ComputedStyle* old_style) {
  LayoutText::StyleDidChange(diff, old_style);
  UpdateIsCombined();
  if (!IsCombined())
    return;

  // We need to call LayoutText::StyleDidChange before updating combined text
  // font because StyleDidChange may change the text through text-transform.
  UpdateFontStyleForCombinedText();
}

void LayoutTextCombine::TextDidChange() {
  LayoutText::TextDidChange();

  bool was_combined = IsCombined();
  UpdateIsCombined();

  // SetTextInternal may be called on construction for applying text-transform
  // in which case Parent() is nullptr. However, was_combined should be false
  // since it initially is.
  DCHECK(!was_combined || Parent());

  if (was_combined) {
    // Re-set the ComputedStyle from the parent to base the measurements in
    // UpdateFontStyleForCombinedText on the original font and not what was
    // previously set for combined text. If IsCombined() is now false, we are
    // simply resetting the style to the parent style.
    SetStyle(Parent()->Style());
  } else if (IsCombined()) {
    // If the text was previously not combined, SetStyle would have been a no-op
    // since the before and after style would be the same ComputedStyle
    // instance and StyleDidChange would not be called. Instead, call
    // UpdateFontStyleForCombinedText directly.
    UpdateFontStyleForCombinedText();
  }
}

float LayoutTextCombine::Width(unsigned from,
                               unsigned length,
                               const Font& font,
                               LayoutUnit x_position,
                               TextDirection direction,
                               HashSet<const SimpleFontData*>* fallback_fonts,
                               FloatRect* glyph_bounds,
                               float) const {
  if (!length)
    return 0;

  if (HasEmptyText())
    return 0;

  if (is_combined_)
    return font.GetFontDescription().ComputedSize();

  return LayoutText::Width(from, length, font, x_position, direction,
                           fallback_fonts, glyph_bounds);
}

void ScaleHorizontallyAndTranslate(GraphicsContext& context,
                                   float scale_x,
                                   float center_x,
                                   float offset_x,
                                   float offset_y) {
  context.ConcatCTM(AffineTransform(
      scale_x, 0, 0, 1, center_x * (1.0f - scale_x) + offset_x * scale_x,
      offset_y));
}

void LayoutTextCombine::TransformToInlineCoordinates(GraphicsContext& context,
                                                     const LayoutRect& box_rect,
                                                     bool clip) const {
  DCHECK(is_combined_);

  // No transform needed if we don't have a font.
  if (!StyleRef().GetFont().PrimaryFont())
    return;

  // On input, the |boxRect| is:
  // 1. Horizontal flow, rotated from the main vertical flow coordinate using
  //    TextPainter::rotation().
  // 2. height() is cell-height, which includes internal leading. This equals
  //    to A+D, and to em+internal leading.
  // 3. width() is the same as m_combinedTextWidth.
  // 4. Left is (right-edge - height()).
  // 5. Top is where char-top (not include internal leading) should be.
  // See https://support.microsoft.com/en-us/kb/32667.
  // We move it so that it comes to the center of em excluding internal
  // leading.

  float cell_height = box_rect.Height();
  float internal_leading =
      StyleRef().GetFont().PrimaryFont()->InternalLeading();
  float offset_y = -internal_leading / 2;
  float width;
  if (scale_x_ >= 1.0f) {
    // Fast path, more than 90% of cases
    DCHECK_EQ(scale_x_, 1.0f);
    float offset_x = (cell_height - combined_text_width_) / 2;
    context.ConcatCTM(AffineTransform::Translation(offset_x, offset_y));
    width = box_rect.Width();
  } else {
    DCHECK_GE(scale_x_, 0.0f);
    float center_x = box_rect.X() + cell_height / 2;
    width = combined_text_width_ / scale_x_;
    float offset_x = (cell_height - width) / 2;
    ScaleHorizontallyAndTranslate(context, scale_x_, center_x, offset_x,
                                  offset_y);
  }

  if (clip)
    context.Clip(FloatRect(box_rect.X(), box_rect.Y(), width, cell_height));
}

void LayoutTextCombine::UpdateIsCombined() {
  // CSS3 spec says text-combine works only in vertical writing mode.
  is_combined_ = !StyleRef().IsHorizontalWritingMode()
                 // Nothing to combine.
                 && !HasEmptyText();
}

void LayoutTextCombine::UpdateFontStyleForCombinedText() {
  DCHECK(is_combined_);

  scoped_refptr<ComputedStyle> style = ComputedStyle::Clone(StyleRef());
  SetStyleInternal(style);

  unsigned offset = 0;
  TextRun run = ConstructTextRun(style->GetFont(), this, offset, TextLength(),
                                 *style, style->Direction());
  FontDescription description = style->GetFont().GetFontDescription();
  float em_width = description.ComputedSize();
  if (!EnumHasFlags(style->TextDecorationsInEffect(),
                    TextDecoration::kUnderline | TextDecoration::kOverline))
    em_width *= kTextCombineMargin;

  // We are going to draw combined text horizontally.
  description.SetOrientation(FontOrientation::kHorizontal);
  combined_text_width_ = style->GetFont().Width(run);

  FontSelector* font_selector = style->GetFont().GetFontSelector();

  // Need to change font orientation to horizontal.
  bool should_update_font = style->SetFontDescription(description);

  if (combined_text_width_ <= em_width) {
    scale_x_ = 1.0f;
  } else {
    // Need to try compressed glyphs.
    static const FontWidthVariant kWidthVariants[] = {kHalfWidth, kThirdWidth,
                                                      kQuarterWidth};
    for (size_t i = 0; i < base::size(kWidthVariants); ++i) {
      description.SetWidthVariant(kWidthVariants[i]);
      Font compressed_font = Font(description);
      compressed_font.Update(font_selector);
      float run_width = compressed_font.Width(run);
      if (run_width <= em_width) {
        combined_text_width_ = run_width;

        // Replace my font with the new one.
        should_update_font = style->SetFontDescription(description);
        break;
      }
    }

    // If width > ~1em, shrink to fit within ~1em, otherwise render without
    // scaling (no expansion).
    // https://drafts.csswg.org/css-writing-modes/#text-combine-compression
    if (combined_text_width_ > em_width) {
      scale_x_ = em_width / combined_text_width_;
      combined_text_width_ = em_width;
    } else {
      scale_x_ = 1.0f;
    }
  }

  if (should_update_font)
    style->GetFont().Update(font_selector);
}

}  // namespace blink
