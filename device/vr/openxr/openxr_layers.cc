// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/openxr/openxr_layers.h"

#include "device/vr/openxr/openxr_graphics_binding.h"

namespace device {
OpenXrLayers::OpenXrLayers(XrSpace space,
                           XrEnvironmentBlendMode blend_mode,
                           const OpenXrGraphicsBinding& graphics_binding,
                           const std::vector<XrCompositionLayerProjectionView>&
                               primary_projection_views)
    : space_(space), blend_mode_(blend_mode) {
  InitializeLayer(graphics_binding, primary_projection_views,
                  primary_projection_layer_);
}

OpenXrLayers::~OpenXrLayers() = default;

void OpenXrLayers::AddSecondaryLayerForType(
    const OpenXrGraphicsBinding& graphics_binding,
    XrViewConfigurationType type,
    const std::vector<XrCompositionLayerProjectionView>& projection_views) {
  secondary_projection_layers_.emplace_back();
  InitializeLayer(graphics_binding, projection_views,
                  secondary_projection_layers_.back());
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
    const OpenXrGraphicsBinding& graphics_binding,
    const std::vector<XrCompositionLayerProjectionView>& projection_views,
    XrCompositionLayerProjection& layer) {
  layer.type = XR_TYPE_COMPOSITION_LAYER_PROJECTION;
  layer.layerFlags = 0;
  layer.space = space_;
  layer.viewCount = projection_views.size();
  layer.views = projection_views.data();

  // GraphicsBinding::MaybeFlipLayer may modify `layer.next`.
  layer.next = nullptr;
  graphics_binding.MaybeFlipLayer(layer);

  if (blend_mode_ == XR_ENVIRONMENT_BLEND_MODE_ALPHA_BLEND) {
    layer.layerFlags |= XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
  }
}

}  // namespace device
