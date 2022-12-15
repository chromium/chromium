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
  Element* container = ContainerQueryEvaluator::FindContainer(
      context_element, ContainerSelector(requested_axis));
  if (!container)
    return absl::nullopt;
  auto* evaluator = container->GetContainerQueryEvaluator();
  if (!evaluator)
    return absl::nullopt;
  evaluator->SetReferencedByUnit();
  if (requested_axis == kPhysicalAxisHorizontal)
    return evaluator->Width();
  DCHECK_EQ(requested_axis, kPhysicalAxisVertical);
  return evaluator->Height();
}

void SetHasContainerRelativeUnits(const ComputedStyle* style) {
  const_cast<ComputedStyle*>(style)->SetHasContainerRelativeUnits();
  const_cast<ComputedStyle*>(style)->SetDependsOnSizeContainerQueries(true);
}

}  // namespace

CSSToLengthConversionData::FontSizes::FontSizes(float em,
                                                float rem,
                                                const Font* font,
                                                float font_zoom)
    : em_(em), rem_(rem), font_(font), font_zoom_(font_zoom) {
  DCHECK(font_);
}

CSSToLengthConversionData::FontSizes::FontSizes(const ComputedStyle* style,
                                                const ComputedStyle* root_style)
    : FontSizes(style->SpecifiedFontSize(),
                root_style ? root_style->SpecifiedFontSize() : 1.0f,
                &style->GetFont(),
                style->EffectiveZoom()) {}

float CSSToLengthConversionData::FontSizes::Ex(float zoom) const {
  DCHECK(font_);
  const SimpleFontData* font_data = font_->PrimaryFont();
  DCHECK(font_data);
  if (!font_data || !font_data->GetFontMetrics().HasXHeight())
    return em_ / 2.0f;
  // Font-metrics-based units are pre-zoomed with a factor of `font_zoom_`,
  // we need to unzoom using that factor before applying the target zoom.
  return font_data->GetFontMetrics().XHeight() / font_zoom_ * zoom;
}

float CSSToLengthConversionData::FontSizes::Ch(float zoom) const {
  DCHECK(font_);
  const SimpleFontData* font_data = font_->PrimaryFont();
  DCHECK(font_data);
  // Font-metrics-based units are pre-zoomed with a factor of `font_zoom_`,
  // we need to unzoom using that factor before applying the target zoom.
  return font_data->GetFontMetrics().ZeroWidth() / font_zoom_ * zoom;
}

float CSSToLengthConversionData::FontSizes::Ic(float zoom) const {
  DCHECK(font_);
  const SimpleFontData* font_data = font_->PrimaryFont();
  DCHECK(font_data);
  absl::optional<float> full_width =
      font_data->GetFontMetrics().IdeographicFullWidth();
  if (!full_width.has_value())
    return Em(zoom);
  // Font-metrics-based units are pre-zoomed with a factor of `font_zoom_`,
  // we need to unzoom using that factor before applying the target zoom.
  return full_width.value() / font_zoom_ * zoom;
}

CSSToLengthConversionData::LineHeightSize::LineHeightSize(
    const ComputedStyle& style)
    : LineHeightSize(style.LineHeight(),
                     &style.GetFont(),
                     style.EffectiveZoom()) {}

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
  if ((cached_physical_axes_ & requested_axis) == requested_axis)
    return;
  cached_physical_axes_ |= requested_axis;
  cache = FindSizeForContainerAxis(requested_axis, context_element_);
}

CSSToLengthConversionData::CSSToLengthConversionData(
    const ComputedStyle* element_style,
    WritingMode writing_mode,
    const FontSizes& font_sizes,
    const LineHeightSize& line_height_size,
    const ViewportSize& viewport_size,
    const ContainerSizes& container_sizes,
    float zoom)
    : CSSLengthResolver(
          ClampTo<float>(zoom, std::numeric_limits<float>::denorm_min())),
      style_(element_style),
      writing_mode_(writing_mode),
      font_sizes_(font_sizes),
      line_height_size_(line_height_size),
      viewport_size_(viewport_size),
      container_sizes_(container_sizes) {}

CSSToLengthConversionData::CSSToLengthConversionData(
    const ComputedStyle* element_style,
    const ComputedStyle* parent_style,
    const ComputedStyle* root_style,
    const LayoutView* layout_view,
    const ContainerSizes& container_sizes,
    float zoom)
    : CSSToLengthConversionData(
          element_style,
          element_style->GetWritingMode(),
          FontSizes(element_style, root_style),
          LineHeightSize(parent_style ? *parent_style : *element_style),
          ViewportSize(layout_view),
          container_sizes,
          zoom) {}

float CSSToLengthConversionData::EmFontSize(float zoom) const {
  // FIXME: Remove style_ from this class. Plumb viewport and font unit
  // information through as output parameters on functions involved in length
  // resolution.
  if (style_)
    const_cast<ComputedStyle*>(style_)->SetHasEmUnits();
  return font_sizes_.Em(zoom);
}

float CSSToLengthConversionData::RemFontSize(float zoom) const {
  if (style_)
    const_cast<ComputedStyle*>(style_)->SetHasRemUnits();
  return font_sizes_.Rem(zoom);
}

float CSSToLengthConversionData::ExFontSize(float zoom) const {
  if (style_)
    const_cast<ComputedStyle*>(style_)->SetHasGlyphRelativeUnits();
  return font_sizes_.Ex(zoom);
}

float CSSToLengthConversionData::ChFontSize(float zoom) const {
  if (style_)
    const_cast<ComputedStyle*>(style_)->SetHasGlyphRelativeUnits();
  return font_sizes_.Ch(zoom);
}

float CSSToLengthConversionData::IcFontSize(float zoom) const {
  if (style_)
    const_cast<ComputedStyle*>(style_)->SetHasGlyphRelativeUnits();
  return font_sizes_.Ic(zoom);
}

float CSSToLengthConversionData::LineHeight(float zoom) const {
  if (style_) {
    const_cast<ComputedStyle*>(style_)->SetHasGlyphRelativeUnits();
    const_cast<ComputedStyle*>(style_)->SetHasLineHeightRelativeUnits();
  }
  return line_height_size_.Lh(zoom);
}

double CSSToLengthConversionData::ViewportWidth() const {
  if (style_)
    const_cast<ComputedStyle*>(style_)->SetHasStaticViewportUnits();
  return viewport_size_.LargeWidth();
}

double CSSToLengthConversionData::ViewportHeight() const {
  if (style_)
    const_cast<ComputedStyle*>(style_)->SetHasStaticViewportUnits();
  return viewport_size_.LargeHeight();
}

double CSSToLengthConversionData::SmallViewportWidth() const {
  if (style_)
    const_cast<ComputedStyle*>(style_)->SetHasStaticViewportUnits();
  return viewport_size_.SmallWidth();
}

double CSSToLengthConversionData::SmallViewportHeight() const {
  if (style_)
    const_cast<ComputedStyle*>(style_)->SetHasStaticViewportUnits();
  return viewport_size_.SmallHeight();
}

double CSSToLengthConversionData::LargeViewportWidth() const {
  if (style_)
    const_cast<ComputedStyle*>(style_)->SetHasStaticViewportUnits();
  return viewport_size_.LargeWidth();
}

double CSSToLengthConversionData::LargeViewportHeight() const {
  if (style_)
    const_cast<ComputedStyle*>(style_)->SetHasStaticViewportUnits();
  return viewport_size_.LargeHeight();
}

double CSSToLengthConversionData::DynamicViewportWidth() const {
  if (style_)
    const_cast<ComputedStyle*>(style_)->SetHasDynamicViewportUnits();
  return viewport_size_.DynamicWidth();
}

double CSSToLengthConversionData::DynamicViewportHeight() const {
  if (style_)
    const_cast<ComputedStyle*>(style_)->SetHasDynamicViewportUnits();
  return viewport_size_.DynamicHeight();
}

double CSSToLengthConversionData::ContainerWidth() const {
  if (style_)
    SetHasContainerRelativeUnits(style_);
  return container_sizes_.Width().value_or(SmallViewportWidth());
}

double CSSToLengthConversionData::ContainerHeight() const {
  if (style_)
    SetHasContainerRelativeUnits(style_);
  return container_sizes_.Height().value_or(SmallViewportHeight());
}

WritingMode CSSToLengthConversionData::GetWritingMode() const {
  return writing_mode_;
}

CSSToLengthConversionData::ContainerSizes
CSSToLengthConversionData::PreCachedContainerSizesCopy() const {
  SetHasContainerRelativeUnits(style_);
  return container_sizes_.PreCachedCopy();
}

}  // namespace blink
