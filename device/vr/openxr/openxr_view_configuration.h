// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_OPENXR_OPENXR_VIEW_CONFIGURATION_H_
#define DEVICE_VR_OPENXR_OPENXR_VIEW_CONFIGURATION_H_

#include <vector>

#include "device/vr/public/mojom/vr_service.mojom-forward.h"
#include "third_party/openxr/src/include/openxr/openxr.h"
#include "ui/gfx/geometry/rect.h"

namespace device {
// -------- Constants/Helpers used for working with ViewConfigurations --------

// The primary view configuration is always enabled and active in OpenXR. We
// currently only support the stereo view configuration.
static constexpr XrViewConfigurationType kPrimaryViewConfiguration =
    XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;

// Secondary view configurations that we currently support. The OpenXR runtime
// must also support these for them to be enabled. There can be an arbitrary
// number of secondary views enabled.
static constexpr std::array<XrViewConfigurationType, 1>
    kSecondaryViewConfigurations = {
        XR_VIEW_CONFIGURATION_TYPE_SECONDARY_MONO_FIRST_PERSON_OBSERVER_MSFT,
};

// The number of views in the primary view configuration. Each frame must return
// at least this number of views, in addition to any secondary views that are
// enabled and active.
static constexpr uint32_t kNumPrimaryViews = 2;

// Per the OpenXR 1.0 spec for the XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO
// view configuration: View index 0 must represent the left eye and view index 1
// must represent the right eye.
static constexpr uint32_t kLeftView = 0;
static constexpr uint32_t kRightView = 1;
// Since kNumPrimaryViews is used to size a vector that uses
// kLeftView/kRightView as indices, ensure that kNumPrimaryViews is greater than
// the largest index.
static_assert(kRightView < kNumPrimaryViews,
              "kNumPrimaryViews must be greater than kRightView");

mojom::XREye GetEyeFromIndex(int i);
// ------ End constants/helpers used for working with ViewConfigurations ------

// A helper class to abstract away decisions that may need to be made about how
// to use/consume/interpret the properties.
class OpenXrViewProperties {
 public:
  OpenXrViewProperties(XrViewConfigurationView xr_properties,
                       uint32_t view_count);
  ~OpenXrViewProperties();

  uint32_t Width() const;
  uint32_t Height() const;
  uint32_t RecommendedSwapchainSampleCount() const;
  uint32_t MaxSwapchainSampleCount() const;

  XrViewConfigurationView GetPropertiesForTest() const {
    return xr_properties_;
  }

 private:
  XrViewConfigurationView xr_properties_;

  // Unused on some platforms, but leaving in to simplify the usage and not
  // introduce per-platform constructors or a factory method.
  [[maybe_unused]] uint32_t view_count_;
};

// Stores information about an OpenXR view configuration that is available in
// the OpenXR runtime, as well as any properties associated with the view
// configuration. These are intiialized at the beginning of a session and
// updated on each frame.
class OpenXrViewConfiguration {
 public:
  OpenXrViewConfiguration();
  OpenXrViewConfiguration(XrViewConfigurationType type,
                          bool active,
                          uint32_t num_views,
                          uint32_t dimension,
                          uint32_t swap_count);
  OpenXrViewConfiguration(OpenXrViewConfiguration&&);
  OpenXrViewConfiguration(const OpenXrViewConfiguration&);
  OpenXrViewConfiguration& operator=(const OpenXrViewConfiguration&);
  ~OpenXrViewConfiguration();

  void Initialize(XrViewConfigurationType type,
                  std::vector<XrViewConfigurationView> properties);
  bool Initialized() const;

  XrViewConfigurationType Type() const;

  bool Active() const;
  void SetActive(bool active);

  const gfx::Rect& Viewport() const;
  void SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height);

  const std::vector<OpenXrViewProperties>& Properties() const;
  void SetProperties(std::vector<XrViewConfigurationView> properties);

  const std::vector<XrView>& Views() const;
  void SetViews(std::vector<XrView> views);

  const std::vector<XrCompositionLayerProjectionView>& ProjectionViews() const;
  XrCompositionLayerProjectionView& GetProjectionView(uint32_t view_index);

  bool CanEnableAntiAliasing() const;

 private:
  XrViewConfigurationType type_ = XR_VIEW_CONFIGURATION_TYPE_MAX_ENUM;
  bool active_ = false;
  bool initialized_ = false;

  // The viewport is only set when active_ is true. Otherwise, the viewport is
  // meaningless. This is the viewport of this entire view configuration within
  // the single OpenXR texture.
  gfx::Rect viewport_;
  std::vector<OpenXrViewProperties> properties_;

  std::vector<XrView> local_from_view_;
  std::vector<XrCompositionLayerProjectionView> projection_views_;
};

}  // namespace device

#endif  // DEVICE_VR_OPENXR_OPENXR_VIEW_CONFIGURATION_H_
