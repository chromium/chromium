// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_MEDIA_VALUES_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_MEDIA_VALUES_H_

#include "third_party/blink/public/mojom/manifest/display_mode.mojom-shared.h"
#include "third_party/blink/public/platform/pointer_properties.h"
#include "third_party/blink/public/platform/shape_properties.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/css_primitive_value.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

class Document;
class CSSPrimitiveValue;
class LocalFrame;
enum class CSSValueID;
enum class ColorSpaceGamut;
enum class PreferredColorScheme;
enum class ForcedColors;
enum class NavigationControls;

PreferredColorScheme CSSValueIDToPreferredColorScheme(CSSValueID id);

class CORE_EXPORT MediaValues : public GarbageCollected<MediaValues> {
 public:
  virtual ~MediaValues() = default;
  virtual void Trace(blink::Visitor* visitor) {}

  static MediaValues* CreateDynamicIfFrameExists(LocalFrame*);
  virtual MediaValues* Copy() const = 0;

  static bool ComputeLengthImpl(double value,
                                CSSPrimitiveValue::UnitType,
                                unsigned default_font_size,
                                double viewport_width,
                                double viewport_height,
                                double& result);
  template <typename T>
  static bool ComputeLength(double value,
                            CSSPrimitiveValue::UnitType type,
                            unsigned default_font_size,
                            double viewport_width,
                            double viewport_height,
                            T& result) {
    double temp_result;
    if (!ComputeLengthImpl(value, type, default_font_size, viewport_width,
                           viewport_height, temp_result))
      return false;
    result = clampTo<T>(temp_result);
    return true;
  }
  virtual bool ComputeLength(double value,
                             CSSPrimitiveValue::UnitType,
                             int& result) const = 0;
  virtual bool ComputeLength(double value,
                             CSSPrimitiveValue::UnitType,
                             double& result) const = 0;

  virtual double ViewportWidth() const = 0;
  virtual double ViewportHeight() const = 0;
  virtual int DeviceWidth() const = 0;
  virtual int DeviceHeight() const = 0;
  virtual float DevicePixelRatio() const = 0;
  virtual int ColorBitsPerComponent() const = 0;
  virtual int MonochromeBitsPerComponent() const = 0;
  virtual PointerType PrimaryPointerType() const = 0;
  virtual int AvailablePointerTypes() const = 0;
  virtual HoverType PrimaryHoverType() const = 0;
  virtual int AvailableHoverTypes() const = 0;
  virtual bool ThreeDEnabled() const = 0;
  virtual bool InImmersiveMode() const = 0;
  virtual const String MediaType() const = 0;
  virtual blink::mojom::DisplayMode DisplayMode() const = 0;
  virtual bool StrictMode() const = 0;
  virtual Document* GetDocument() const = 0;
  virtual bool HasValues() const = 0;

  virtual void OverrideViewportDimensions(double width, double height) = 0;
  virtual DisplayShape GetDisplayShape() const = 0;
  virtual ColorSpaceGamut ColorGamut() const = 0;
  virtual PreferredColorScheme GetPreferredColorScheme() const = 0;
  virtual bool PrefersReducedMotion() const = 0;
  virtual ForcedColors GetForcedColors() const = 0;
  virtual NavigationControls GetNavigationControls() const = 0;

 protected:
  static double CalculateViewportWidth(LocalFrame*);
  static double CalculateViewportHeight(LocalFrame*);
  static int CalculateDeviceWidth(LocalFrame*);
  static int CalculateDeviceHeight(LocalFrame*);
  static bool CalculateStrictMode(LocalFrame*);
  static float CalculateDevicePixelRatio(LocalFrame*);
  static int CalculateColorBitsPerComponent(LocalFrame*);
  static int CalculateMonochromeBitsPerComponent(LocalFrame*);
  static int CalculateDefaultFontSize(LocalFrame*);
  static const String CalculateMediaType(LocalFrame*);
  static blink::mojom::DisplayMode CalculateDisplayMode(LocalFrame*);
  static bool CalculateThreeDEnabled(LocalFrame*);
  static bool CalculateInImmersiveMode(LocalFrame*);
  static PointerType CalculatePrimaryPointerType(LocalFrame*);
  static int CalculateAvailablePointerTypes(LocalFrame*);
  static HoverType CalculatePrimaryHoverType(LocalFrame*);
  static int CalculateAvailableHoverTypes(LocalFrame*);
  static DisplayShape CalculateDisplayShape(LocalFrame*);
  static ColorSpaceGamut CalculateColorGamut(LocalFrame*);
  static PreferredColorScheme CalculatePreferredColorScheme(LocalFrame*);
  static bool CalculatePrefersReducedMotion(LocalFrame*);
  static ForcedColors CalculateForcedColors();
  static NavigationControls CalculateNavigationControls(LocalFrame*);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_MEDIA_VALUES_H_
