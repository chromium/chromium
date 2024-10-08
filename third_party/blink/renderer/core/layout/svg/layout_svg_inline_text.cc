/*
 * Copyright (C) 2006 Oliver Hunt <ojh16@student.canterbury.ac.nz>
 * Copyright (C) 2006 Apple Computer Inc.
 * Copyright (C) 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2008 Rob Buis <buis@kde.org>
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
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
 */

#include "third_party/blink/renderer/core/layout/svg/layout_svg_inline_text.h"

#include "third_party/blink/renderer/core/css/css_font_selector.h"
#include "third_party/blink/renderer/core/css/font_size_functions.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/position_with_affinity.h"
#include "third_party/blink/renderer/core/editing/text_affinity.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/layout/inline/fragment_item.h"
#include "third_party/blink/renderer/core/layout/inline/inline_cursor.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_text.h"
#include "third_party/blink/renderer/core/layout/svg/svg_layout_support.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"

namespace blink {

// Turn tabs, newlines and carriage returns into spaces. In the future this
// should be removed in favor of letting the generic white-space code handle
// this.
static String NormalizeWhitespace(String string) {
  String new_string = string.Replace('\t', ' ');
  new_string = new_string.Replace('\n', ' ');
  new_string = new_string.Replace('\r', ' ');
  return new_string;
}

LayoutSVGInlineText::LayoutSVGInlineText(Node* n, String string)
    : LayoutText(n, NormalizeWhitespace(std::move(string))),
      scaling_factor_(1) {}

void LayoutSVGInlineText::TextDidChange() {
  NOT_DESTROYED();
  SetTextInternal(NormalizeWhitespace(TransformedText()));
  LayoutText::TextDidChange();
  LayoutSVGText::NotifySubtreeStructureChanged(
      this, layout_invalidation_reason::kTextChanged);

  if (StyleRef().UsedUserModify() != EUserModify::kReadOnly)
    UseCounter::Count(GetDocument(), WebFeature::kSVGTextEdited);
}

void LayoutSVGInlineText::StyleDidChange(StyleDifference diff,
                                         const ComputedStyle* old_style) {
  NOT_DESTROYED();
  LayoutText::StyleDidChange(diff, old_style);
  UpdateScaledFont();

  const bool new_collapse = StyleRef().ShouldCollapseWhiteSpaces();
  const bool old_collapse = old_style && old_style->ShouldCollapseWhiteSpaces();
  if (old_collapse != new_collapse) {
    ForceSetText(OriginalText());
    return;
  }

  if (!diff.NeedsFullLayout())
    return;

  // The text metrics may be influenced by style changes.
  if (auto* ng_text = LayoutSVGText::LocateLayoutSVGTextAncestor(this)) {
    ng_text->SetNeedsTextMetricsUpdate();
    ng_text->SetNeedsLayoutAndFullPaintInvalidation(
        layout_invalidation_reason::kStyleChange);
  }
}

bool LayoutSVGInlineText::IsFontFallbackValid() const {
  return LayoutText::IsFontFallbackValid() && ScaledFont().IsFallbackValid();
}

void LayoutSVGInlineText::InvalidateSubtreeLayoutForFontUpdates() {
  NOT_DESTROYED();
  if (!IsFontFallbackValid()) {
    LayoutSVGText::NotifySubtreeStructureChanged(
        this, layout_invalidation_reason::kFontsChanged);
  }
  LayoutText::InvalidateSubtreeLayoutForFontUpdates();
}

PhysicalRect LayoutSVGInlineText::PhysicalLinesBoundingBox() const {
  NOT_DESTROYED();
  return PhysicalRect();
}

gfx::RectF LayoutSVGInlineText::ObjectBoundingBox() const {
  NOT_DESTROYED();
  DCHECK(IsInLayoutNGInlineFormattingContext());

  gfx::RectF bounds;
  InlineCursor cursor;
  cursor.MoveTo(*this);
  for (; cursor; cursor.MoveToNextForSameLayoutObject()) {
    const FragmentItem& item = *cursor.CurrentItem();
    if (item.IsSvgText()) {
      bounds.Union(cursor.Current().ObjectBoundingBox(cursor));
    }
  }
  return bounds;
}

PositionWithAffinity LayoutSVGInlineText::PositionForPoint(
    const PhysicalOffset& point) const {
  NOT_DESTROYED();
  DCHECK_GE(GetDocument().Lifecycle().GetState(),
            DocumentLifecycle::kPrePaintClean);

  DCHECK(IsInLayoutNGInlineFormattingContext());
  InlineCursor cursor;
  cursor.MoveTo(*this);
  InlineCursor last_hit_cursor;
  PhysicalOffset last_hit_transformed_point;
  LayoutUnit closest_distance = LayoutUnit::Max();
  for (; cursor; cursor.MoveToNextForSameLayoutObject()) {
    PhysicalOffset transformed_point =
        cursor.CurrentItem()->MapPointInContainer(point);
    PhysicalRect item_rect = cursor.Current().RectInContainerFragment();
    LayoutUnit distance;
    if (!item_rect.Contains(transformed_point) ||
        !cursor.PositionForPointInChild(transformed_point)) {
      distance = item_rect.SquaredDistanceTo(transformed_point);
    }
    // Intentionally apply '<=', not '<', because we'd like to choose a later
    // item.
    if (distance <= closest_distance) {
      closest_distance = distance;
      last_hit_cursor = cursor;
      last_hit_transformed_point = transformed_point;
    }
  }
  if (last_hit_cursor) {
    auto position_with_affinity =
        last_hit_cursor.PositionForPointInChild(last_hit_transformed_point);
    // Note: Due by Bidi adjustment, |position_with_affinity| isn't relative
    // to this.
    return AdjustForEditingBoundary(position_with_affinity);
  }
  return CreatePositionWithAffinity(0);
}

void LayoutSVGInlineText::UpdateScaledFont() {
  NOT_DESTROYED();
  ComputeNewScaledFontForStyle(*this, scaling_factor_, scaled_font_);
}

void LayoutSVGInlineText::ComputeNewScaledFontForStyle(
    const LayoutObject& layout_object,
    float& scaling_factor,
    Font& scaled_font) {
  const ComputedStyle& style = layout_object.StyleRef();

  // Alter font-size to the right on-screen value to avoid scaling the glyphs
  // themselves, except when GeometricPrecision is specified.
  scaling_factor =
      SVGLayoutSupport::CalculateScreenFontSizeScalingFactor(&layout_object);
  if (!scaling_factor) {
    scaling_factor = 1;
    scaled_font = style.GetFont();
    return;
  }

  const FontDescription& unscaled_font_description = style.GetFontDescription();
  if (unscaled_font_description.TextRendering() == kGeometricPrecision)
    scaling_factor = 1;

  Document& document = layout_object.GetDocument();
  float scaled_font_size = FontSizeFunctions::GetComputedSizeFromSpecifiedSize(
      &document, scaling_factor, unscaled_font_description.IsAbsoluteSize(),
      unscaled_font_description.SpecifiedSize(), kDoNotApplyMinimumForFontSize);
  if (scaled_font_size == unscaled_font_description.ComputedSize()) {
    scaled_font = style.GetFont();
    return;
  }

  FontDescription font_description = unscaled_font_description;
  font_description.SetComputedSize(scaled_font_size);
  const float zoom = style.EffectiveZoom();
  font_description.SetLetterSpacing(font_description.LetterSpacing() *
                                    scaling_factor / zoom);
  font_description.SetWordSpacing(font_description.WordSpacing() *
                                  scaling_factor / zoom);

  scaled_font =
      Font(font_description, document.GetStyleEngine().GetFontSelector());
}

gfx::RectF LayoutSVGInlineText::VisualRectInLocalSVGCoordinates() const {
  NOT_DESTROYED();
  return Parent()->VisualRectInLocalSVGCoordinates();
}

}  // namespace blink
