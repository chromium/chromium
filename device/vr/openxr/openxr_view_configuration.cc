// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#define _USE_MATH_DEFINES  // For VC++ to get M_PI. This has to be first.

#include "device/vr/openxr/openxr_view_configuration.h"

#include <algorithm>
#include <cmath>

#include "base/check_op.h"
#include "build/build_config.h"
#include "device/vr/public/mojom/vr_service.mojom.h"
#include "third_party/openxr/src/include/openxr/openxr.h"

namespace device {

namespace {
// This default isn't necessarily suitable for rendering, but it avoids a rare
// situation where if we cannot locate views on the first frame, xrEndFrame will
// return XR_ERROR_POSE_INVALID which will esesntially terminate the session.
constexpr float kDefaultFov = M_PI / 2.0f;
constexpr XrView kDefaultView{
    XR_TYPE_VIEW,
    /*next=*/nullptr,
    /*pose=*/{{0, 0, 0, 1}, {0, 0, 0}},
    /*fov=*/{kDefaultFov, kDefaultFov, kDefaultFov, kDefaultFov}};
}  // namespace

mojom::XREye GetEyeFromIndex(int i) {
  if (i == kLeftView) {
    return mojom::XREye::kLeft;
  } else if (i == kRightView) {
    return mojom::XREye::kRight;
  } else {
    return mojom::XREye::kNone;
  }
}

OpenXrViewProperties::OpenXrViewProperties(
    XrViewConfigurationView xr_properties,
    uint32_t view_count,
    gfx::Size max_texture_size)
    : xr_properties_(xr_properties),
      view_count_(view_count),
      max_texture_size_(max_texture_size) {
  DVLOG(1) << __func__ << " view_count_" << view_count_
           << " maxImageRectWidth=" << xr_properties_.maxImageRectWidth
           << " maxImageRectHeight=" << xr_properties_.maxImageRectHeight
           << " recommendedImageRectWidth="
           << xr_properties_.recommendedImageRectWidth
           << " recommendedImageRectHeight="
           << xr_properties_.recommendedImageRectHeight;
}
OpenXrViewProperties::~OpenXrViewProperties() = default;

uint32_t OpenXrViewProperties::ClampWidth(uint32_t val) const {
  return std::min(
      val, static_cast<uint32_t>(max_texture_size_.width()) / view_count_);
}

uint32_t OpenXrViewProperties::ClampHeight(uint32_t val) const {
  return std::min(val, static_cast<uint32_t>(max_texture_size_.height()));
}

uint32_t OpenXrViewProperties::Width() const {
  // TODO(crbug.com/40918787):Windows cannot support framebuffer scaling, so
  // must use the recommended size.
  if constexpr (BUILDFLAG(IS_WIN)) {
    return ClampWidth(xr_properties_.recommendedImageRectWidth);
  } else {
    return ClampWidth(xr_properties_.maxImageRectWidth);
  }
}

uint32_t OpenXrViewProperties::Height() const {
  // TODO(crbug.com/40918787):Windows cannot support framebuffer scaling, so
  // must use the recommended size.
  if constexpr (BUILDFLAG(IS_WIN)) {
    return ClampHeight(xr_properties_.recommendedImageRectHeight);
  } else {
    return ClampHeight(xr_properties_.maxImageRectHeight);
  }
}

uint32_t OpenXrViewProperties::RecommendedSwapchainSampleCount() const {
  return xr_properties_.recommendedSwapchainSampleCount;
}

float OpenXrViewProperties::RecommendedViewportScale() const {
  // TODO(crbug.com/40918787):Windows cannot support framebuffer scaling, so
  // must use the recommended size.
  if constexpr (BUILDFLAG(IS_WIN)) {
    return 1.0f;
  } else {
    float width_scale = static_cast<float>(ClampWidth(
                            xr_properties_.recommendedImageRectWidth)) /
                        Width();
    float height_scale = static_cast<float>(ClampHeight(
                             xr_properties_.recommendedImageRectHeight)) /
                         Height();
    return std::min(width_scale, height_scale);
  }
}

uint32_t OpenXrViewProperties::MaxSwapchainSampleCount() const {
  return xr_properties_.maxSwapchainSampleCount;
}

OpenXrViewConfiguration::OpenXrViewConfiguration() = default;
OpenXrViewConfiguration::OpenXrViewConfiguration(OpenXrViewConfiguration&&) =
    default;
OpenXrViewConfiguration::OpenXrViewConfiguration(
    const OpenXrViewConfiguration&) = default;
OpenXrViewConfiguration& OpenXrViewConfiguration::operator=(
    const OpenXrViewConfiguration&) = default;
OpenXrViewConfiguration::~OpenXrViewConfiguration() = default;

// Used only for testing - initializes an OpenXR view configuration that the
// mock OpenXR runtime supports.
OpenXrViewConfiguration::OpenXrViewConfiguration(XrViewConfigurationType type,
                                                 bool active,
                                                 uint32_t num_views,
                                                 uint32_t dimension,
                                                 uint32_t swap_count) {
  const XrViewConfigurationView kViewConfigurationView = {
      XR_TYPE_VIEW_CONFIGURATION_VIEW,
      nullptr,
      dimension,
      dimension,
      dimension,
      dimension,
      swap_count,
      swap_count};

  std::vector<XrViewConfigurationView> view_properties(num_views);
  for (uint32_t i = 0; i < num_views; i++) {
    view_properties[i] = kViewConfigurationView;
  }

  // We do n-wide textures, so each view can fill the full height, but we need
  // n*dimensions for width.
  Initialize(type, std::move(view_properties),
             gfx::Size(/*width=*/dimension * num_views, /*height=*/dimension));
  SetActive(active);
}

void OpenXrViewConfiguration::Initialize(
    XrViewConfigurationType type,
    std::vector<XrViewConfigurationView> properties,
    gfx::Size max_texture_size) {
  DCHECK(!initialized_);
  DCHECK(!properties.empty());

  type_ = type;
  active_ = false;
  viewport_ = gfx::Rect();
  SetProperties(std::move(properties), max_texture_size);
  local_from_view_.resize(properties_.size(), kDefaultView);

  initialized_ = true;
}

bool OpenXrViewConfiguration::Initialized() const {
  return initialized_;
}

XrViewConfigurationType OpenXrViewConfiguration::Type() const {
  return type_;
}

void OpenXrViewConfiguration::SetActive(bool active) {
  active_ = active;
  if (!active_) {
    viewport_ = gfx::Rect();
  }
}

bool OpenXrViewConfiguration::Active() const {
  return active_;
}

const gfx::Rect& OpenXrViewConfiguration::Viewport() const {
  return viewport_;
}

void OpenXrViewConfiguration::SetViewport(uint32_t x,
                                          uint32_t y,
                                          uint32_t width,
                                          uint32_t height) {
  viewport_ = gfx::Rect(x, y, width, height);
}

const std::vector<OpenXrViewProperties>& OpenXrViewConfiguration::Properties()
    const {
  return properties_;
}

void OpenXrViewConfiguration::SetProperties(
    std::vector<XrViewConfigurationView> properties,
    gfx::Size max_texture_size) {
  // The number of views in a view configuration should not change throughout
  // the lifetime of the OpenXR instance.
  CHECK(properties_.empty() || properties.size() == properties_.size());
  uint32_t size = properties.size();
  properties_.clear();
  properties_.reserve(size);
  std::ranges::transform(
      properties, std::back_inserter(properties_),
      [size, max_texture_size](const XrViewConfigurationView& view) {
        return OpenXrViewProperties(view, size, max_texture_size);
      });
}

const std::vector<XrView>& OpenXrViewConfiguration::Views() const {
  return local_from_view_;
}

void OpenXrViewConfiguration::SetViews(std::vector<XrView> views) {
  DCHECK_EQ(views.size(), local_from_view_.size());
  local_from_view_ = std::move(views);
}

bool OpenXrViewConfiguration::CanEnableAntiAliasing() const {
  // From the OpenXR Spec:
  // maxSwapchainSampleCount is the maximum number of sub-data element samples
  // supported for swapchain images that will be rendered into for this view.
  //
  // To ease the workload on low end devices, we disable anti-aliasing when the
  // max sample count is 1.
  return std::ranges::all_of(properties_, [](const OpenXrViewProperties& view) {
    return view.MaxSwapchainSampleCount() > 1;
  });
}

}  // namespace device
