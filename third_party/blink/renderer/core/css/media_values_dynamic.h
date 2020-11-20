// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_MEDIA_VALUES_DYNAMIC_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_MEDIA_VALUES_DYNAMIC_H_

#include "third_party/blink/renderer/core/css/media_values.h"

namespace blink {

class Document;

class CORE_EXPORT MediaValuesDynamic : public MediaValues {
 public:
  static MediaValues* Create(Document&);
  static MediaValues* Create(LocalFrame*);

  MediaValuesDynamic(LocalFrame*);
  MediaValuesDynamic(LocalFrame*,
                     bool overridden_viewport_dimensions,
                     double viewport_width,
                     double viewport_height);

  MediaValues* Copy() const override;
  bool ComputeLength(double value,
                     CSSPrimitiveValue::UnitType,
                     int& result) const override;
  bool ComputeLength(double value,
                     CSSPrimitiveValue::UnitType,
                     double& result) const override;

  double ViewportWidth() const override;
  double ViewportHeight() const override;
  int DeviceWidth() const override;
  int DeviceHeight() const override;
  float DevicePixelRatio() const override;
  int ColorBitsPerComponent() const override;
  int MonochromeBitsPerComponent() const override;
  mojom::blink::PointerType PrimaryPointerType() const override;
  int AvailablePointerTypes() const override;
  ui::HoverType PrimaryHoverType() const override;
  int AvailableHoverTypes() const override;
  bool ThreeDEnabled() const override;
  bool InImmersiveMode() const override;
  bool StrictMode() const override;
  const String MediaType() const override;
  blink::mojom::DisplayMode DisplayMode() const override;
  ColorSpaceGamut ColorGamut() const override;
  mojom::blink::PreferredColorScheme GetPreferredColorScheme() const override;
  mojom::blink::PreferredContrast GetPreferredContrast() const override;
  bool PrefersReducedMotion() const override;
  bool PrefersReducedData() const override;
  ForcedColors GetForcedColors() const override;
  NavigationControls GetNavigationControls() const override;
  ScreenSpanning GetScreenSpanning() const override;
  Document* GetDocument() const override;
  bool HasValues() const override;
  void OverrideViewportDimensions(double width, double height) override;

  void Trace(Visitor*) const override;

 protected:
  Member<LocalFrame> frame_;
  bool viewport_dimensions_overridden_;
  double viewport_width_override_;
  double viewport_height_override_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_MEDIA_VALUES_DYNAMIC_H_
