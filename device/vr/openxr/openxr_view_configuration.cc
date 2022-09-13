// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/openxr/openxr_view_configuration.h"

#include "base/check_op.h"
#include "third_party/openxr/src/include/openxr/openxr.h"

namespace device {

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
  properties_ = std::move(properties);
  local_from_view_.resize(properties_.size());
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

const std::vector<XrViewConfigurationView>&
OpenXrViewConfiguration::Properties() const {
  return properties_;
}

void OpenXrViewConfiguration::SetProperties(
    std::vector<XrViewConfigurationView> properties) {
  // The number of views in a view configuration should not change throughout
  // the lifetime of the OpenXR instance.
  DCHECK_EQ(properties.size(), properties_.size());
  properties_ = std::move(properties);
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
