// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#define _USE_MATH_DEFINES  // For VC++ to get M_PI. This has to be first.

#include "device/vr/openxr/openxr_view_configuration.h"

#include <algorithm>
#include <cmath>

#include "base/check.h"
#include "base/check_op.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/system/sys_info.h"
#include "build/build_config.h"
#include "device/vr/public/cpp/switches.h"
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

// TODO(crbug.com/529457611): Windows and Linux do not support framebuffer
// scaling.
constexpr bool kSupportsViewportScaling =
    !BUILDFLAG(IS_WIN) && !BUILDFLAG(IS_LINUX);

constexpr base::ByteSize kLowMemoryThreshold = base::GiBU(8);
constexpr double kLowMemoryDefaultMaxScaleFactor = 1.5f;
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

  CalculateViewportScaledProperties();
}
OpenXrViewProperties::~OpenXrViewProperties() = default;

void OpenXrViewProperties::CalculateViewportScaledProperties() {
  // Clamp texture sizes based on GL texture limits and number of views.
  uint32_t clamped_recommended_width =
      ClampWidth(xr_properties_.recommendedImageRectWidth);
  uint32_t clamped_recommended_height =
      ClampHeight(xr_properties_.recommendedImageRectHeight);

  // If viewport scaling isn't supported, just use the recommended width/height.
  if constexpr (!kSupportsViewportScaling) {
    viewport_scaled_width_ = clamped_recommended_width;
    viewport_scaled_height_ = clamped_recommended_height;
    return;
  }

  uint32_t clamped_max_width = ClampWidth(xr_properties_.maxImageRectWidth);
  uint32_t clamped_max_height = ClampHeight(xr_properties_.maxImageRectHeight);

  // Determine what scale factor will be applied to the recommended width and
  // height to report the maximum allowed width/height to the page. The inverse
  // of this will be reported to the page as the `defaultFrameBufferScale`,
  // since that is the actual recommendation, but if the page then sets their
  // framebuffer scale to 1.0 they'd receive this maximum texture size. By
  // computing the scale factor this way, we ensure that the aspect ratio of the
  // recommended width/height are preserved.
  // Start by computing the absolute largest scale factor that can be applied
  // (e.g. the scale factor that will max out the recommended width or height
  // first when applied).
  double scale_factor = std::min(
      static_cast<double>(clamped_max_width) / clamped_recommended_width,
      static_cast<double>(clamped_max_height) / clamped_recommended_height);
  DVLOG(1) << __func__ << " initial scale_factor=" << scale_factor;

  // We absolutely cannot go over the current scale_factor due to hardware
  // limitations, but if there's a value set from the command line, don't use
  // our default logic for determining the scale factor to apply.
  // In android_browsertests, OpenXrViewProperties is included in the standalone
  // mock OpenXR shared library where the command line singleton is not
  // initialized. Verify that the command line is initialized before attempting
  // to query switches.
  if (base::CommandLine::InitializedForCurrentProcess() &&
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kWebXrMaxFramebufferScale)) {
    std::string switch_value =
        base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            switches::kWebXrMaxFramebufferScale);
    double command_line_scale_limit;
    if (base::StringToDouble(switch_value, &command_line_scale_limit) &&
        command_line_scale_limit > 0.0) {
      DVLOG(1) << __func__ << " command line switch "
               << switches::kWebXrMaxFramebufferScale << "="
               << command_line_scale_limit
               << " computed scale_factor=" << scale_factor;
      scale_factor = std::min(scale_factor, command_line_scale_limit);
    }
  } else {
    // Limit max framebuffer scale on low-memory devices.
    // Note that `AmountOfTotalPhysicalMemory` also tries to query the command
    // line and can crash on some configurations.
    if (base::CommandLine::InitializedForCurrentProcess() &&
        base::SysInfo::AmountOfTotalPhysicalMemory() <= kLowMemoryThreshold) {
      scale_factor = std::min(scale_factor, kLowMemoryDefaultMaxScaleFactor);
    }
  }

  // Compute final viewport dimensions by scaling recommended bounds by
  // scale_factor.
  viewport_scaled_width_ =
      ClampWidth(std::round(clamped_recommended_width * scale_factor));
  viewport_scaled_height_ =
      ClampHeight(std::round(clamped_recommended_height * scale_factor));

  DVLOG(1) << __func__ << " final scale_factor=" << scale_factor
           << " viewport_scaled_width_=" << viewport_scaled_width_
           << " viewport_scaled_height_=" << viewport_scaled_height_;
}

uint32_t OpenXrViewProperties::ClampWidth(uint32_t val) const {
  return std::min(
      val, static_cast<uint32_t>(max_texture_size_.width()) / view_count_);
}

uint32_t OpenXrViewProperties::ClampHeight(uint32_t val) const {
  return std::min(val, static_cast<uint32_t>(max_texture_size_.height()));
}

uint32_t OpenXrViewProperties::Width() const {
  return viewport_scaled_width_;
}

uint32_t OpenXrViewProperties::Height() const {
  return viewport_scaled_height_;
}

uint32_t OpenXrViewProperties::RecommendedSwapchainSampleCount() const {
  return xr_properties_.recommendedSwapchainSampleCount;
}

float OpenXrViewProperties::RecommendedViewportScale() const {
  // Width() and Height() *should* return the same values as the ClampWidth and
  // ClampHeight calls on the recommended values, meaning that the calculations
  // work out to 1.0, but due to floating point precision and to avoid needless
  // calculations, just return 1 directly if viewport scaling isn't supported.
  if constexpr (!kSupportsViewportScaling) {
    return 1.0f;
  }

  float width_scale =
      static_cast<float>(ClampWidth(xr_properties_.recommendedImageRectWidth)) /
      Width();
  float height_scale = static_cast<float>(ClampHeight(
                           xr_properties_.recommendedImageRectHeight)) /
                       Height();
  return std::min(width_scale, height_scale);
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
