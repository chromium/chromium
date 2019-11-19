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

#include "third_party/blink/renderer/core/css/css_resolution_units.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {

CSSToLengthConversionData::FontSizes::FontSizes(float em,
                                                float rem,
                                                const Font* font,
                                                float zoom)
    : em_(em), rem_(rem), font_(font), zoom_(zoom) {
  // FIXME: Improve RAII of StyleResolverState to use const Font&.
  DCHECK(font_);
}

CSSToLengthConversionData::FontSizes::FontSizes(const ComputedStyle* style,
                                                const ComputedStyle* root_style)
    : FontSizes(style->SpecifiedFontSize(),
                root_style ? root_style->SpecifiedFontSize() : 1.0f,
                &style->GetFont(),
                style->EffectiveZoom()) {}

float CSSToLengthConversionData::FontSizes::Ex() const {
  DCHECK(font_);
  const SimpleFontData* font_data = font_->PrimaryFont();
  DCHECK(font_data);
  if (!font_data || !font_data->GetFontMetrics().HasXHeight())
    return em_ / 2.0f;
  return font_data->GetFontMetrics().XHeight();
}

float CSSToLengthConversionData::FontSizes::Ch() const {
  DCHECK(font_);
  const SimpleFontData* font_data = font_->PrimaryFont();
  DCHECK(font_data);
  return font_data ? font_data->GetFontMetrics().ZeroWidth() : 0;
}

float CSSToLengthConversionData::FontSizes::Zoom() const {
  return zoom_ ? zoom_ : 1;
}

CSSToLengthConversionData::ViewportSize::ViewportSize(
    const LayoutView* layout_view)
    : size_(layout_view ? layout_view->ViewportSizeForViewportUnits()
                        : DoubleSize()) {}

CSSToLengthConversionData::CSSToLengthConversionData(
    const ComputedStyle* style,
    const FontSizes& font_sizes,
    const ViewportSize& viewport_size,
    float zoom)
    : style_(style),
      font_sizes_(font_sizes),
      viewport_size_(viewport_size),
      zoom_(clampTo<float>(zoom, std::numeric_limits<float>::denorm_min())) {}

CSSToLengthConversionData::CSSToLengthConversionData(
    const ComputedStyle* style,
    const ComputedStyle* root_style,
    const LayoutView* layout_view,
    float zoom)
    : CSSToLengthConversionData(style,
                                FontSizes(style, root_style),
                                ViewportSize(layout_view),
                                zoom) {}

double CSSToLengthConversionData::ViewportWidthPercent() const {
  // FIXME: Remove style_ from this class. Plumb viewport and rem unit
  // information through as output parameters on functions involved in length
  // resolution.
  if (style_)
    const_cast<ComputedStyle*>(style_)->SetHasViewportUnits(true);
  return viewport_size_.Width() / 100;
}
double CSSToLengthConversionData::ViewportHeightPercent() const {
  if (style_)
    const_cast<ComputedStyle*>(style_)->SetHasViewportUnits(true);
  return viewport_size_.Height() / 100;
}
double CSSToLengthConversionData::ViewportMinPercent() const {
  if (style_)
    const_cast<ComputedStyle*>(style_)->SetHasViewportUnits(true);
  return std::min(viewport_size_.Width(), viewport_size_.Height()) / 100;
}
double CSSToLengthConversionData::ViewportMaxPercent() const {
  if (style_)
    const_cast<ComputedStyle*>(style_)->SetHasViewportUnits(true);
  return std::max(viewport_size_.Width(), viewport_size_.Height()) / 100;
}

float CSSToLengthConversionData::RemFontSize() const {
  if (style_)
    const_cast<ComputedStyle*>(style_)->SetHasRemUnits();
  return font_sizes_.Rem();
}

float CSSToLengthConversionData::ExFontSize() const {
  if (style_)
    const_cast<ComputedStyle*>(style_)->SetHasGlyphRelativeUnits();
  return font_sizes_.Ex();
}

float CSSToLengthConversionData::ChFontSize() const {
  if (style_)
    const_cast<ComputedStyle*>(style_)->SetHasGlyphRelativeUnits();
  return font_sizes_.Ch();
}

double CSSToLengthConversionData::ZoomedComputedPixels(
    double value,
    CSSPrimitiveValue::UnitType type) const {
  // The logic in this function is duplicated in MediaValues::ComputeLength()
  // because MediaValues::ComputeLength() needs nearly identical logic, but we
  // haven't found a way to make ZoomedComputedPixels() more generic (to solve
  // both cases) without hurting performance.
  switch (type) {
    case CSSPrimitiveValue::UnitType::kPixels:
    case CSSPrimitiveValue::UnitType::kUserUnits:
      return value * Zoom();

    case CSSPrimitiveValue::UnitType::kCentimeters:
      return value * kCssPixelsPerCentimeter * Zoom();

    case CSSPrimitiveValue::UnitType::kMillimeters:
      return value * kCssPixelsPerMillimeter * Zoom();

    case CSSPrimitiveValue::UnitType::kQuarterMillimeters:
      return value * kCssPixelsPerQuarterMillimeter * Zoom();

    case CSSPrimitiveValue::UnitType::kInches:
      return value * kCssPixelsPerInch * Zoom();

    case CSSPrimitiveValue::UnitType::kPoints:
      return value * kCssPixelsPerPoint * Zoom();

    case CSSPrimitiveValue::UnitType::kPicas:
      return value * kCssPixelsPerPica * Zoom();

    case CSSPrimitiveValue::UnitType::kViewportWidth:
      return value * ViewportWidthPercent() * Zoom();

    case CSSPrimitiveValue::UnitType::kViewportHeight:
      return value * ViewportHeightPercent() * Zoom();

    case CSSPrimitiveValue::UnitType::kViewportMin:
      return value * ViewportMinPercent() * Zoom();

    case CSSPrimitiveValue::UnitType::kViewportMax:
      return value * ViewportMaxPercent() * Zoom();

    // We do not apply the zoom factor when we are computing the value of the
    // font-size property. The zooming for font sizes is much more complicated,
    // since we have to worry about enforcing the minimum font size preference
    // as well as enforcing the implicit "smart minimum."
    case CSSPrimitiveValue::UnitType::kEms:
    case CSSPrimitiveValue::UnitType::kQuirkyEms:
      return value * EmFontSize() * Zoom();

    case CSSPrimitiveValue::UnitType::kExs:
      return value * ExFontSize();

    case CSSPrimitiveValue::UnitType::kRems:
      return value * RemFontSize() * Zoom();

    case CSSPrimitiveValue::UnitType::kChs:
      return value * ChFontSize();

    default:
      NOTREACHED();
      return 0;
  }
}

}  // namespace blink
