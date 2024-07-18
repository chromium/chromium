// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#define _USE_MATH_DEFINES  // For VC++ to get M_PI. This has to be first.

#include "device/vr/openxr/openxr_view_configuration.h"

#include <cmath>

#include "base/check_op.h"
#include "base/ranges/algorithm.h"
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
    uint32_t view_count)
    : xr_properties_(xr_properties), view_count_(view_count) {}
OpenXrViewProperties::~OpenXrViewProperties() = default;

uint32_t OpenXrViewProperties::Width() const {
  if constexpr (BUILDFLAG(IS_ANDROID)) {
    // TODO(crbug.com/40948737): Devise a more robust way of calculating
    // the max size and per view width. (e.g. (viewWidth/totalWidth) *
    // maxWidth).
    constexpr uint32_t kMaxImageWidth = 4096;
    return std::min(xr_properties_.recommendedImageRectWidth,
                    kMaxImageWidth / view_count_);
  }

  return xr_properties_.recommendedImageRectWidth;
}

uint32_t OpenXrViewProperties::Height() const {
  return xr_properties_.recommendedImageRectHeight;
}

uint32_t OpenXrViewProperties::RecommendedSwapchainSampleCount() const {
  return xr_properties_.recommendedSwapchainSampleCount;
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

  Initialize(type, std::move(view_properties));
  SetActive(active);
}

void OpenXrViewConfiguration::Initialize(
    XrViewConfigurationType type,
    std::vector<XrViewConfigurationView> properties) {
  DCHECK(!initialized_);
  DCHECK(!properties.empty());

  type_ = type;
  active_ = false;
  viewport_ = gfx::Rect();
  SetProperties(std::move(properties));
  local_from_view_.resize(properties_.size(), kDefaultView);
  projection_views_.resize(properties_.size());

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
    std::vector<XrViewConfigurationView> properties) {
  // The number of views in a view configuration should not change throughout
  // the lifetime of the OpenXR instance.
  CHECK(properties_.empty() || properties.size() == properties_.size());
  uint32_t size = properties.size();
  properties_.clear();
  properties_.reserve(size);
  base::ranges::transform(properties, std::back_inserter(properties_),
                          [size](const XrViewConfigurationView& view) {
                            return OpenXrViewProperties(view, size);
                          });
}

const std::vector<XrView>& OpenXrViewConfiguration::Views() const {
  return local_from_view_;
}

void OpenXrViewConfiguration::SetViews(std::vector<XrView> views) {
  DCHECK_EQ(views.size(), local_from_view_.size());
  local_from_view_ = std::move(views);
}

const std::vector<XrCompositionLayerProjectionView>&
OpenXrViewConfiguration::ProjectionViews() const {
  return projection_views_;
}

XrCompositionLayerProjectionView& OpenXrViewConfiguration::GetProjectionView(
    uint32_t view_index) {
  DCHECK_LT(view_index, projection_views_.size());
  return projection_views_[view_index];
}

bool OpenXrViewConfiguration::CanEnableAntiAliasing() const {
  // From the OpenXR Spec:
  // maxSwapchainSampleCount is the maximum number of sub-data element samples
  // supported for swapchain images that will be rendered into for this view.
  //
  // To ease the workload on low end devices, we disable anti-aliasing when the
  // max sample count is 1.
  return base::ranges::all_of(properties_,
                              [](const OpenXrViewProperties& view) {
                                return view.MaxSwapchainSampleCount() > 1;
                              });
}

OpenXrLayers::OpenXrLayers(XrSpace space,
                           XrEnvironmentBlendMode blend_mode,
                           const std::vector<XrCompositionLayerProjectionView>&
                               primary_projection_views)
    : space_(space), blend_mode_(blend_mode) {
  InitializeLayer(primary_projection_views, primary_projection_layer_);
}

OpenXrLayers::~OpenXrLayers() = default;

void OpenXrLayers::AddSecondaryLayerForType(
    XrViewConfigurationType type,
    const std::vector<XrCompositionLayerProjectionView>& projection_views) {
  secondary_projection_layers_.emplace_back();
  InitializeLayer(projection_views, secondary_projection_layers_.back());
  secondary_composition_layers_.push_back(
      reinterpret_cast<XrCompositionLayerBaseHeader*>(
          &secondary_projection_layers_.back()));

  secondary_layer_info_.emplace_back();
  XrSecondaryViewConfigurationLayerInfoMSFT& layer_info =
      secondary_layer_info_.back();
  layer_info.type = XR_TYPE_SECONDARY_VIEW_CONFIGURATION_LAYER_INFO_MSFT;
  layer_info.viewConfigurationType = type;
  layer_info.environmentBlendMode = blend_mode_;
  layer_info.layerCount = 1;
  layer_info.layers = &secondary_composition_layers_.back();
}

void OpenXrLayers::InitializeLayer(
    const std::vector<XrCompositionLayerProjectionView>& projection_views,
    XrCompositionLayerProjection& layer) {
  layer.type = XR_TYPE_COMPOSITION_LAYER_PROJECTION;
  layer.next = nullptr;
  layer.layerFlags = 0;
  layer.space = space_;
  layer.viewCount = projection_views.size();
  layer.views = projection_views.data();

  if (blend_mode_ == XR_ENVIRONMENT_BLEND_MODE_ALPHA_BLEND) {
    layer.layerFlags |= XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
  }
}

}  // namespace device
