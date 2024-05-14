// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_length_resolver.h"

#include "third_party/blink/renderer/core/css/css_resolution_units.h"

namespace blink {

double CSSLengthResolver::ViewportWidthPercent() const {
  return ViewportWidth() / 100;
}

double CSSLengthResolver::ViewportHeightPercent() const {
  return ViewportHeight() / 100;
}

double CSSLengthResolver::ViewportInlineSizePercent() const {
  return (IsHorizontalWritingMode() ? ViewportWidth() : ViewportHeight()) / 100;
}

double CSSLengthResolver::ViewportBlockSizePercent() const {
  return (IsHorizontalWritingMode() ? ViewportHeight() : ViewportWidth()) / 100;
}

double CSSLengthResolver::ViewportMinPercent() const {
  return std::min(ViewportWidth(), ViewportHeight()) / 100;
}

double CSSLengthResolver::ViewportMaxPercent() const {
  return std::max(ViewportWidth(), ViewportHeight()) / 100;
}

double CSSLengthResolver::SmallViewportWidthPercent() const {
  return SmallViewportWidth() / 100;
}

double CSSLengthResolver::SmallViewportHeightPercent() const {
  return SmallViewportHeight() / 100;
}

double CSSLengthResolver::SmallViewportInlineSizePercent() const {
  return (IsHorizontalWritingMode() ? SmallViewportWidth()
                                    : SmallViewportHeight()) /
         100;
}

double CSSLengthResolver::SmallViewportBlockSizePercent() const {
  return (IsHorizontalWritingMode() ? SmallViewportHeight()
                                    : SmallViewportWidth()) /
         100;
}

double CSSLengthResolver::SmallViewportMinPercent() const {
  return std::min(SmallViewportWidth(), SmallViewportHeight()) / 100;
}

double CSSLengthResolver::SmallViewportMaxPercent() const {
  return std::max(SmallViewportWidth(), SmallViewportHeight()) / 100;
}

double CSSLengthResolver::LargeViewportWidthPercent() const {
  return LargeViewportWidth() / 100;
}

double CSSLengthResolver::LargeViewportHeightPercent() const {
  return LargeViewportHeight() / 100;
}

double CSSLengthResolver::LargeViewportInlineSizePercent() const {
  return (IsHorizontalWritingMode() ? LargeViewportWidth()
                                    : LargeViewportHeight()) /
         100;
}

double CSSLengthResolver::LargeViewportBlockSizePercent() const {
  return (IsHorizontalWritingMode() ? LargeViewportHeight()
                                    : LargeViewportWidth()) /
         100;
}

double CSSLengthResolver::LargeViewportMinPercent() const {
  return std::min(LargeViewportWidth(), LargeViewportHeight()) / 100;
}

double CSSLengthResolver::LargeViewportMaxPercent() const {
  return std::max(LargeViewportWidth(), LargeViewportHeight()) / 100;
}

double CSSLengthResolver::DynamicViewportWidthPercent() const {
  return DynamicViewportWidth() / 100;
}

double CSSLengthResolver::DynamicViewportHeightPercent() const {
  return DynamicViewportHeight() / 100;
}

double CSSLengthResolver::DynamicViewportInlineSizePercent() const {
  return (IsHorizontalWritingMode() ? DynamicViewportWidth()
                                    : DynamicViewportHeight()) /
         100;
}

double CSSLengthResolver::DynamicViewportBlockSizePercent() const {
  return (IsHorizontalWritingMode() ? DynamicViewportHeight()
                                    : DynamicViewportWidth()) /
         100;
}

double CSSLengthResolver::DynamicViewportMinPercent() const {
  return std::min(DynamicViewportWidth(), DynamicViewportHeight()) / 100;
}

double CSSLengthResolver::DynamicViewportMaxPercent() const {
  return std::max(DynamicViewportWidth(), DynamicViewportHeight()) / 100;
}

double CSSLengthResolver::ContainerWidthPercent() const {
  return ContainerWidth() / 100;
}

double CSSLengthResolver::ContainerHeightPercent() const {
  return ContainerHeight() / 100;
}

double CSSLengthResolver::ContainerInlineSizePercent() const {
  return IsHorizontalWritingMode() ? ContainerWidthPercent()
                                   : ContainerHeightPercent();
}

double CSSLengthResolver::ContainerBlockSizePercent() const {
  return IsHorizontalWritingMode() ? ContainerHeightPercent()
                                   : ContainerWidthPercent();
}

double CSSLengthResolver::ContainerMinPercent() const {
  return std::min(ContainerWidthPercent(), ContainerHeightPercent());
}

double CSSLengthResolver::ContainerMaxPercent() const {
  return std::max(ContainerWidthPercent(), ContainerHeightPercent());
}

double CSSLengthResolver::ZoomedComputedPixels(
    double value,
    CSSPrimitiveValue::UnitType type) const {
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

    case CSSPrimitiveValue::UnitType::kViewportInlineSize:
      return value * ViewportInlineSizePercent() * Zoom();

    case CSSPrimitiveValue::UnitType::kViewportBlockSize:
      return value * ViewportBlockSizePercent() * Zoom();

    case CSSPrimitiveValue::UnitType::kViewportMin:
      return value * ViewportMinPercent() * Zoom();

    case CSSPrimitiveValue::UnitType::kViewportMax:
      return value * ViewportMaxPercent() * Zoom();

    case CSSPrimitiveValue::UnitType::kSmallViewportWidth:
      return value * SmallViewportWidthPercent() * Zoom();

    case CSSPrimitiveValue::UnitType::kSmallViewportHeight:
      return value * SmallViewportHeightPercent() * Zoom();

    case CSSPrimitiveValue::UnitType::kSmallViewportInlineSize:
      return value * SmallViewportInlineSizePercent() * Zoom();

    case CSSPrimitiveValue::UnitType::kSmallViewportBlockSize:
      return value * SmallViewportBlockSizePercent() * Zoom();

    case CSSPrimitiveValue::UnitType::kSmallViewportMin:
      return value * SmallViewportMinPercent() * Zoom();

    case CSSPrimitiveValue::UnitType::kSmallViewportMax:
      return value * SmallViewportMaxPercent() * Zoom();

    case CSSPrimitiveValue::UnitType::kLargeViewportWidth:
      return value * LargeViewportWidthPercent() * Zoom();

    case CSSPrimitiveValue::UnitType::kLargeViewportHeight:
      return value * LargeViewportHeightPercent() * Zoom();

    case CSSPrimitiveValue::UnitType::kLargeViewportInlineSize:
      return value * LargeViewportInlineSizePercent() * Zoom();

    case CSSPrimitiveValue::UnitType::kLargeViewportBlockSize:
      return value * LargeViewportBlockSizePercent() * Zoom();

    case CSSPrimitiveValue::UnitType::kLargeViewportMin:
      return value * LargeViewportMinPercent() * Zoom();

    case CSSPrimitiveValue::UnitType::kLargeViewportMax:
      return value * LargeViewportMaxPercent() * Zoom();

    case CSSPrimitiveValue::UnitType::kDynamicViewportWidth:
      return value * DynamicViewportWidthPercent() * Zoom();

    case CSSPrimitiveValue::UnitType::kDynamicViewportHeight:
      return value * DynamicViewportHeightPercent() * Zoom();

    case CSSPrimitiveValue::UnitType::kDynamicViewportInlineSize:
      return value * DynamicViewportInlineSizePercent() * Zoom();

    case CSSPrimitiveValue::UnitType::kDynamicViewportBlockSize:
      return value * DynamicViewportBlockSizePercent() * Zoom();

    case CSSPrimitiveValue::UnitType::kDynamicViewportMin:
      return value * DynamicViewportMinPercent() * Zoom();

    case CSSPrimitiveValue::UnitType::kDynamicViewportMax:
      return value * DynamicViewportMaxPercent() * Zoom();

    case CSSPrimitiveValue::UnitType::kContainerWidth:
      return value * ContainerWidthPercent() * Zoom();

    case CSSPrimitiveValue::UnitType::kContainerHeight:
      return value * ContainerHeightPercent() * Zoom();

    case CSSPrimitiveValue::UnitType::kContainerInlineSize:
      return value * ContainerInlineSizePercent() * Zoom();

    case CSSPrimitiveValue::UnitType::kContainerBlockSize:
      return value * ContainerBlockSizePercent() * Zoom();

    case CSSPrimitiveValue::UnitType::kContainerMin:
      return value * ContainerMinPercent() * Zoom();

    case CSSPrimitiveValue::UnitType::kContainerMax:
      return value * ContainerMaxPercent() * Zoom();

    // Note that functions for font-relative units already account for the
    // zoom factor.
    case CSSPrimitiveValue::UnitType::kEms:
    case CSSPrimitiveValue::UnitType::kQuirkyEms:
      return value * EmFontSize(Zoom());

    case CSSPrimitiveValue::UnitType::kExs:
      return value * ExFontSize(Zoom());

    case CSSPrimitiveValue::UnitType::kRexs:
      return value * RexFontSize(Zoom());

    case CSSPrimitiveValue::UnitType::kRems:
      return value * RemFontSize(Zoom());

    case CSSPrimitiveValue::UnitType::kChs:
      return value * ChFontSize(Zoom());

    case CSSPrimitiveValue::UnitType::kRchs:
      return value * RchFontSize(Zoom());

    case CSSPrimitiveValue::UnitType::kIcs:
      return value * IcFontSize(Zoom());

    case CSSPrimitiveValue::UnitType::kRics:
      return value * RicFontSize(Zoom());

    case CSSPrimitiveValue::UnitType::kLhs:
      return value * LineHeight(Zoom());

    case CSSPrimitiveValue::UnitType::kRlhs:
      return value * RootLineHeight(Zoom());

    case CSSPrimitiveValue::UnitType::kCaps:
      return value * CapFontSize(Zoom());

    case CSSPrimitiveValue::UnitType::kRcaps:
      return value * RcapFontSize(Zoom());

    default:
      NOTREACHED_IN_MIGRATION();
      return 0;
  }
}

}  // namespace blink
