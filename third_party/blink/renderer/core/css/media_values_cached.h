// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_MEDIA_VALUES_CACHED_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_MEDIA_VALUES_CACHED_H_

#include "third_party/blink/public/common/css/forced_colors.h"
#include "third_party/blink/public/common/css/navigation_controls.h"
#include "third_party/blink/public/mojom/css/preferred_color_scheme.mojom-blink.h"
#include "third_party/blink/public/mojom/css/preferred_contrast.mojom-blink.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom-blink.h"
#include "third_party/blink/public/mojom/webpreferences/web_preferences.mojom-blink.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/css_primitive_value.h"
#include "third_party/blink/renderer/core/css/media_values.h"
#include "third_party/blink/renderer/platform/graphics/color_space_gamut.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier.h"
#include "ui/base/pointer/pointer_device.h"

namespace blink {

class CORE_EXPORT MediaValuesCached final : public MediaValues {
 public:
  struct CORE_EXPORT MediaValuesCachedData final {
    DISALLOW_NEW();
    // Members variables must be thread safe, since they're copied to the parser
    // thread
    double viewport_width = 0;
    double viewport_height = 0;
    int device_width = 0;
    int device_height = 0;
    float device_pixel_ratio = 1.0;
    int color_bits_per_component = 24;
    int monochrome_bits_per_component = 0;
    mojom::blink::PointerType primary_pointer_type =
        mojom::blink::PointerType::kPointerNone;
    // Bitmask of |ui::PointerType|
    int available_pointer_types = ui::POINTER_TYPE_NONE;
    mojom::blink::HoverType primary_hover_type =
        mojom::blink::HoverType::kHoverNone;
    // Bitmask of |ui::HoverType|
    int available_hover_types = ui::HOVER_TYPE_NONE;
    int default_font_size = 16;
    bool three_d_enabled = false;
    bool immersive_mode = false;
    bool strict_mode = true;
    String media_type;
    mojom::blink::DisplayMode display_mode =
        mojom::blink::DisplayMode::kBrowser;
    ColorSpaceGamut color_gamut = ColorSpaceGamut::kUnknown;
    mojom::blink::PreferredColorScheme preferred_color_scheme =
        mojom::blink::PreferredColorScheme::kLight;
    mojom::blink::PreferredContrast preferred_contrast =
        mojom::blink::PreferredContrast::kNoPreference;
    bool prefers_reduced_motion = false;
    bool prefers_reduced_data = false;
    ForcedColors forced_colors = ForcedColors::kNone;
    NavigationControls navigation_controls = NavigationControls::kNone;
    ScreenSpanning screen_spanning = ScreenSpanning::kNone;
    DevicePosture device_posture = DevicePosture::kNoFold;

    MediaValuesCachedData();
    explicit MediaValuesCachedData(Document&);

    MediaValuesCachedData DeepCopy() const {
      MediaValuesCachedData data;
      data.viewport_width = viewport_width;
      data.viewport_height = viewport_height;
      data.device_width = device_width;
      data.device_height = device_height;
      data.device_pixel_ratio = device_pixel_ratio;
      data.color_bits_per_component = color_bits_per_component;
      data.monochrome_bits_per_component = monochrome_bits_per_component;
      data.primary_pointer_type = primary_pointer_type;
      data.available_pointer_types = available_pointer_types;
      data.primary_hover_type = primary_hover_type;
      data.available_hover_types = available_hover_types;
      data.default_font_size = default_font_size;
      data.three_d_enabled = three_d_enabled;
      data.immersive_mode = immersive_mode;
      data.strict_mode = strict_mode;
      data.media_type = media_type.IsolatedCopy();
      data.display_mode = display_mode;
      data.color_gamut = color_gamut;
      data.preferred_color_scheme = preferred_color_scheme;
      data.preferred_contrast = preferred_contrast;
      data.prefers_reduced_motion = prefers_reduced_motion;
      data.prefers_reduced_data = prefers_reduced_data;
      data.forced_colors = forced_colors;
      data.navigation_controls = navigation_controls;
      data.screen_spanning = screen_spanning;
      data.device_posture = device_posture;
      return data;
    }
  };

  MediaValuesCached();
  explicit MediaValuesCached(LocalFrame*);
  explicit MediaValuesCached(const MediaValuesCachedData&);

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
  mojom::blink::HoverType PrimaryHoverType() const override;
  int AvailableHoverTypes() const override;
  bool ThreeDEnabled() const override;
  bool InImmersiveMode() const override;
  bool StrictMode() const override;
  Document* GetDocument() const override;
  bool HasValues() const override;
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
  DevicePosture GetDevicePosture() const override;

  void OverrideViewportDimensions(double width, double height) override;

 protected:
  MediaValuesCachedData data_;
};

}  // namespace blink

namespace WTF {

template <>
struct CrossThreadCopier<blink::MediaValuesCached::MediaValuesCachedData> {
  typedef blink::MediaValuesCached::MediaValuesCachedData Type;
  static Type Copy(const Type& data) { return data.DeepCopy(); }
};

}  // namespace WTF

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_MEDIA_VALUES_CACHED_H_
