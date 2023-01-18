/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/css/css_to_length_conversion_data.h"

#include "third_party/blink/renderer/core/css/container_query.h"
#include "third_party/blink/renderer/core/css/container_query_evaluator.h"
#include "third_party/blink/renderer/core/css/css_resolution_units.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/layout_tree_builder_traversal.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/layout/adjust_for_absolute_zoom.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {

namespace {

absl::optional<double> FindSizeForContainerAxis(PhysicalAxes requested_axis,
                                                Element* context_element) {
  DCHECK(requested_axis == kPhysicalAxisHorizontal ||
         requested_axis == kPhysicalAxisVertical);

  ContainerSelector selector(requested_axis);

  for (Element* container =
           ContainerQueryEvaluator::FindContainer(context_element, selector);
       container; container = ContainerQueryEvaluator::FindContainer(
                      container->ParentOrShadowHostElement(), selector)) {
    auto* evaluator = container->GetContainerQueryEvaluator();
    if (!evaluator) {
      continue;
    }
    evaluator->SetReferencedByUnit();
    absl::optional<double> size = requested_axis == kPhysicalAxisHorizontal
                                      ? evaluator->Width()
                                      : evaluator->Height();
    if (!size.has_value()) {
      continue;
    }
    return size;
  }

  return absl::nullopt;
}

}  // namespace

CSSToLengthConversionData::FontSizes::FontSizes(float em,
                                                float rem,
                                                const Font* font,
                                                float font_zoom)
    : em_(em),
      rem_(rem),
      font_(font),
      root_font_(font),
      font_zoom_(font_zoom),
      root_font_zoom_(font_zoom) {
  DCHECK(font_);
}

CSSToLengthConversionData::FontSizes::FontSizes(float em,
                                                float rem,
                                                const Font* font,
                                                const Font* root_font,
                                                float font_zoom,
                                                float root_font_zoom)
    : em_(em),
      rem_(rem),
      font_(font),
      root_font_(root_font),
      font_zoom_(font_zoom),
      root_font_zoom_(root_font_zoom) {
  DCHECK(font_);
  DCHECK(root_font_);
}

CSSToLengthConversionData::FontSizes::FontSizes(const ComputedStyle* style,
                                                const ComputedStyle* root_style)
    : FontSizes(
          style->SpecifiedFontSize(),
          root_style ? root_style->SpecifiedFontSize() : 1.0f,
          &style->GetFont(),
          root_style ? &root_style->GetFont() : &style->GetFont(),
          style->EffectiveZoom(),
          root_style ? root_style->EffectiveZoom() : style->EffectiveZoom()) {}

float CSSToLengthConversionData::FontSizes::Ex(float zoom) const {
  DCHECK(font_);
  const SimpleFontData* font_data = font_->PrimaryFont();
  DCHECK(font_data);
  if (!font_data || !font_data->GetFontMetrics().HasXHeight()) {
    return em_ / 2.0f;
  }
  // Font-metrics-based units are pre-zoomed with a factor of `font_zoom_`,
  // we need to unzoom using that factor before applying the target zoom.
  return font_data->GetFontMetrics().XHeight() / font_zoom_ * zoom;
}

float CSSToLengthConversionData::FontSizes::Rex(float zoom) const {
  DCHECK(root_font_);
  const SimpleFontData* font_data = root_font_->PrimaryFont();
  DCHECK(font_data);
  if (!font_data || !font_data->GetFontMetrics().HasXHeight()) {
    return rem_ / 2.0f;
  }
  // Font-metrics-based units are pre-zoomed with a factor of `root_font_zoom_`,
  // we need to unzoom using that factor before applying the target zoom.
  return font_data->GetFontMetrics().XHeight() / root_font_zoom_ * zoom;
}

float CSSToLengthConversionData::FontSizes::Ch(float zoom) const {
  DCHECK(font_);
  const SimpleFontData* font_data = font_->PrimaryFont();
  DCHECK(font_data);
  // Font-metrics-based units are pre-zoomed with a factor of `font_zoom_`,
  // we need to unzoom using that factor before applying the target zoom.
  return font_data->GetFontMetrics().ZeroWidth() / font_zoom_ * zoom;
}

float CSSToLengthConversionData::FontSizes::Rch(float zoom) const {
  DCHECK(root_font_);
  const SimpleFontData* font_data = root_font_->PrimaryFont();
  DCHECK(font_data);
  // Font-metrics-based units are pre-zoomed with a factor of `root_font_zoom_`,
  // we need to unzoom using that factor before applying the target zoom.
  return font_data->GetFontMetrics().ZeroWidth() / root_font_zoom_ * zoom;
}

float CSSToLengthConversionData::FontSizes::Ic(float zoom) const {
  DCHECK(font_);
  const SimpleFontData* font_data = font_->PrimaryFont();
  DCHECK(font_data);
  absl::optional<float> full_width =
      font_data->GetFontMetrics().IdeographicFullWidth();
  if (!full_width.has_value()) {
    return Em(zoom);
  }
  // Font-metrics-based units are pre-zoomed with a factor of `font_zoom_`,
  // we need to unzoom using that factor before applying the target zoom.
  return full_width.value() / font_zoom_ * zoom;
}

float CSSToLengthConversionData::FontSizes::Ric(float zoom) const {
  DCHECK(root_font_);
  const SimpleFontData* font_data = root_font_->PrimaryFont();
  DCHECK(font_data);
  absl::optional<float> full_width =
      font_data->GetFontMetrics().IdeographicFullWidth();
  if (!full_width.has_value()) {
    return Rem(zoom);
  }
  // Font-metrics-based units are pre-zoomed with a factor of `font_zoom_`,
  // we need to unzoom using that factor before applying the target zoom.
  return full_width.value() / root_font_zoom_ * zoom;
}

CSSToLengthConversionData::LineHeightSize::LineHeightSize(
    const ComputedStyle& style,
    const ComputedStyle* root_style)
    : LineHeightSize(
          style.LineHeight(),
          root_style ? root_style->LineHeight() : style.LineHeight(),
          &style.GetFont(),
          root_style ? &root_style->GetFont() : &style.GetFont(),
          style.EffectiveZoom(),
          root_style ? root_style->EffectiveZoom() : style.EffectiveZoom()) {}

float CSSToLengthConversionData::LineHeightSize::Lh(float zoom) const {
  if (!font_) {
    return 0;
  }
  // Like font-metrics-based units, lh is also based on pre-zoomed font metrics.
  // We therefore need to unzoom using the font zoom before applying the target
  // zoom.
  return ComputedStyle::ComputedLineHeight(line_height_, *font_) / font_zoom_ *
         zoom;
}

float CSSToLengthConversionData::LineHeightSize::Rlh(float zoom) const {
  if (!root_font_) {
    return 0;
  }
  // Like font-metrics-based units, rlh is also based on pre-zoomed font
  // metrics. We therefore need to unzoom using the font zoom before applying
  // the target zoom.
  return ComputedStyle::ComputedLineHeight(root_line_height_, *root_font_) /
         root_font_zoom_ * zoom;
}

CSSToLengthConversionData::ViewportSize::ViewportSize(
    const LayoutView* layout_view) {
  if (layout_view) {
    gfx::SizeF large_size = layout_view->LargeViewportSizeForViewportUnits();
    large_width_ = large_size.width();
    large_height_ = large_size.height();

    gfx::SizeF small_size = layout_view->SmallViewportSizeForViewportUnits();
    small_width_ = small_size.width();
    small_height_ = small_size.height();

    gfx::SizeF dynamic_size =
        layout_view->DynamicViewportSizeForViewportUnits();
    dynamic_width_ = dynamic_size.width();
    dynamic_height_ = dynamic_size.height();
  }
}

CSSToLengthConversionData::ContainerSizes
CSSToLengthConversionData::ContainerSizes::PreCachedCopy() const {
  ContainerSizes copy = *this;
  copy.Width();
  copy.Height();
  DCHECK(!copy.context_element_ || copy.cached_width_.has_value());
  DCHECK(!copy.context_element_ || copy.cached_height_.has_value());
  // We don't need to keep the container since we eagerly fetched both values.
  copy.context_element_ = nullptr;
  return copy;
}

void CSSToLengthConversionData::ContainerSizes::Trace(Visitor* visitor) const {
  visitor->Trace(context_element_);
}

bool CSSToLengthConversionData::ContainerSizes::SizesEqual(
    const ContainerSizes& other) const {
  return (Width() == other.Width()) && (Height() == other.Height());
}

absl::optional<double> CSSToLengthConversionData::ContainerSizes::Width()
    const {
  CacheSizeIfNeeded(PhysicalAxes(kPhysicalAxisHorizontal), cached_width_);
  return cached_width_;
}

absl::optional<double> CSSToLengthConversionData::ContainerSizes::Height()
    const {
  CacheSizeIfNeeded(PhysicalAxes(kPhysicalAxisVertical), cached_height_);
  return cached_height_;
}

void CSSToLengthConversionData::ContainerSizes::CacheSizeIfNeeded(
    PhysicalAxes requested_axis,
    absl::optional<double>& cache) const {
  if ((cached_physical_axes_ & requested_axis) == requested_axis) {
    return;
  }
  cached_physical_axes_ |= requested_axis;
  cache = FindSizeForContainerAxis(requested_axis, context_element_);
}

CSSToLengthConversionData::CSSToLengthConversionData(
    WritingMode writing_mode,
    const FontSizes& font_sizes,
    const LineHeightSize& line_height_size,
    const ViewportSize& viewport_size,
    const ContainerSizes& container_sizes,
    float zoom,
    Flags& flags)
    : CSSLengthResolver(
          ClampTo<float>(zoom, std::numeric_limits<float>::denorm_min())),
      writing_mode_(writing_mode),
      font_sizes_(font_sizes),
      line_height_size_(line_height_size),
      viewport_size_(viewport_size),
      container_sizes_(container_sizes),
      flags_(&flags) {}

CSSToLengthConversionData::CSSToLengthConversionData(
    const ComputedStyle* element_style,
    const ComputedStyle* parent_style,
    const ComputedStyle* root_style,
    const LayoutView* layout_view,
    const ContainerSizes& container_sizes,
    float zoom,
    Flags& flags)
    : CSSToLengthConversionData(
          element_style->GetWritingMode(),
          FontSizes(element_style, root_style),
          LineHeightSize(parent_style ? *parent_style : *element_style,
                         root_style),
          ViewportSize(layout_view),
          container_sizes,
          zoom,
          flags) {}

float CSSToLengthConversionData::EmFontSize(float zoom) const {
  SetFlag(Flag::kEm);
  return font_sizes_.Em(zoom);
}

float CSSToLengthConversionData::RemFontSize(float zoom) const {
  SetFlag(Flag::kRootFontRelative);
  return font_sizes_.Rem(zoom);
}

float CSSToLengthConversionData::ExFontSize(float zoom) const {
  SetFlag(Flag::kGlyphRelative);
  return font_sizes_.Ex(zoom);
}

float CSSToLengthConversionData::RexFontSize(float zoom) const {
  // Need to mark the current element's ComputedStyle as having glyph relative
  // styles, even if it is not relative to the current element's font because
  // the invalidation that happens when a web font finishes loading for the root
  // element does not necessarily cause a style difference for the root element,
  // hence will not cause an invalidation of root font relative dependent
  // styles. See also Node::MarkSubtreeNeedsStyleRecalcForFontUpdates().
  SetFlag(Flag::kGlyphRelative);
  SetFlag(Flag::kRootFontRelative);
  return font_sizes_.Rex(zoom);
}

float CSSToLengthConversionData::ChFontSize(float zoom) const {
  SetFlag(Flag::kGlyphRelative);
  return font_sizes_.Ch(zoom);
}

float CSSToLengthConversionData::RchFontSize(float zoom) const {
  // Need to mark the current element's ComputedStyle as having glyph relative
  // styles, even if it is not relative to the current element's font because
  // the invalidation that happens when a web font finishes loading for the root
  // element does not necessarily cause a style difference for the root element,
  // hence will not cause an invalidation of root font relative dependent
  // styles. See also Node::MarkSubtreeNeedsStyleRecalcForFontUpdates().
  SetFlag(Flag::kGlyphRelative);
  SetFlag(Flag::kRootFontRelative);
  return font_sizes_.Rch(zoom);
}

float CSSToLengthConversionData::IcFontSize(float zoom) const {
  SetFlag(Flag::kGlyphRelative);
  return font_sizes_.Ic(zoom);
}

float CSSToLengthConversionData::RicFontSize(float zoom) const {
  // Need to mark the current element's ComputedStyle as having glyph relative
  // styles, even if it is not relative to the current element's font because
  // the invalidation that happens when a web font finishes loading for the root
  // element does not necessarily cause a style difference for the root element,
  // hence will not cause an invalidation of root font relative dependent
  // styles. See also Node::MarkSubtreeNeedsStyleRecalcForFontUpdates().
  SetFlag(Flag::kGlyphRelative);
  SetFlag(Flag::kRootFontRelative);
  return font_sizes_.Ric(zoom);
}

float CSSToLengthConversionData::LineHeight(float zoom) const {
  SetFlag(Flag::kGlyphRelative);
  SetFlag(Flag::kLineHeightRelative);
  return line_height_size_.Lh(zoom);
}

float CSSToLengthConversionData::RootLineHeight(float zoom) const {
  // Need to mark the current element's ComputedStyle as having glyph relative
  // styles, even if it is not relative to the current element's font because
  // the invalidation that happens when a web font finishes loading for the root
  // element does not necessarily cause a style difference for the root element,
  // hence will not cause an invalidation of root font relative dependent
  // styles. See also Node::MarkSubtreeNeedsStyleRecalcForFontUpdates().
  SetFlag(Flag::kGlyphRelative);
  SetFlag(Flag::kRootFontRelative);
  SetFlag(Flag::kLineHeightRelative);
  return line_height_size_.Rlh(zoom);
}

double CSSToLengthConversionData::ViewportWidth() const {
  SetFlag(Flag::kStaticViewport);
  return viewport_size_.LargeWidth();
}

double CSSToLengthConversionData::ViewportHeight() const {
  SetFlag(Flag::kStaticViewport);
  return viewport_size_.LargeHeight();
}

double CSSToLengthConversionData::SmallViewportWidth() const {
  SetFlag(Flag::kStaticViewport);
  return viewport_size_.SmallWidth();
}

double CSSToLengthConversionData::SmallViewportHeight() const {
  SetFlag(Flag::kStaticViewport);
  return viewport_size_.SmallHeight();
}

double CSSToLengthConversionData::LargeViewportWidth() const {
  SetFlag(Flag::kStaticViewport);
  return viewport_size_.LargeWidth();
}

double CSSToLengthConversionData::LargeViewportHeight() const {
  SetFlag(Flag::kStaticViewport);
  return viewport_size_.LargeHeight();
}

double CSSToLengthConversionData::DynamicViewportWidth() const {
  SetFlag(Flag::kDynamicViewport);
  return viewport_size_.DynamicWidth();
}

double CSSToLengthConversionData::DynamicViewportHeight() const {
  SetFlag(Flag::kDynamicViewport);
  return viewport_size_.DynamicHeight();
}

double CSSToLengthConversionData::ContainerWidth() const {
  SetFlag(Flag::kContainerRelative);
  return container_sizes_.Width().value_or(SmallViewportWidth());
}

double CSSToLengthConversionData::ContainerHeight() const {
  SetFlag(Flag::kContainerRelative);
  return container_sizes_.Height().value_or(SmallViewportHeight());
}

WritingMode CSSToLengthConversionData::GetWritingMode() const {
  return writing_mode_;
}

CSSToLengthConversionData::ContainerSizes
CSSToLengthConversionData::PreCachedContainerSizesCopy() const {
  SetFlag(Flag::kContainerRelative);
  return container_sizes_.PreCachedCopy();
}

}  // namespace blink
