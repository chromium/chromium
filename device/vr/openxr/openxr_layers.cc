// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/openxr/openxr_layers.h"

#include "base/notreached.h"
#include "device/vr/openxr/openxr_composition_layer.h"
#include "device/vr/openxr/openxr_util.h"
#include "ui/gfx/geometry/decomposed_transform.h"

namespace device {

namespace {

template <typename XrLayerDataType>
void FillSubImage(XrLayerDataType& xr_layer,
                  const OpenXrCompositionLayer& layer) {
  xr_layer.subImage.swapchain = layer.color_swapchain();
  xr_layer.subImage.imageArrayIndex = 0;
  xr_layer.subImage.imageRect.offset = {
      .x = 0,
      .y = 0,
  };
  xr_layer.subImage.imageRect.extent = {
      .width = static_cast<int>(layer.read_only_data().texture_width),
      .height = static_cast<int>(layer.read_only_data().texture_height),
  };
}

XrCompositionLayerProjection BuildProjectionLayerData(
    const OpenXrCompositionLayer& layer,
    const std::vector<XrCompositionLayerProjectionView>& projection_views,
    const void* xr_next_struct) {
  // Zero-initialize everything.
  XrCompositionLayerProjection projection{XR_TYPE_COMPOSITION_LAYER_PROJECTION};
  projection.next = xr_next_struct;
  projection.space = layer.space();
  projection.viewCount = projection_views.size();
  projection.views = projection_views.data();
  if (layer.mutable_data().blend_texture_source_alpha) {
    projection.layerFlags |=
        XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
  }
  return projection;
}

XrCompositionLayerQuad BuildQuadLayerData(const OpenXrCompositionLayer& layer,
                                          const void* xr_next_struct) {
  CHECK(layer.mutable_data().layer_data->is_quad());
  const auto& layer_specific_data =
      *layer.mutable_data().layer_data->get_quad();

  // Zero-initialize everything.
  XrCompositionLayerQuad quad{XR_TYPE_COMPOSITION_LAYER_QUAD};
  quad.next = xr_next_struct;
  quad.space = layer.space();
  quad.size = {
      .width = layer_specific_data.width,
      .height = layer_specific_data.height,
  };
  quad.pose = GfxTransformToXrPose(layer_specific_data.transform);
  if (layer.mutable_data().blend_texture_source_alpha) {
    quad.layerFlags |= XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
  }
  FillSubImage(quad, layer);

  return quad;
}

XrCompositionLayerCylinderKHR BuildCylinderLayerData(
    const OpenXrCompositionLayer& layer,
    const void* xr_next_struct) {
  CHECK(layer.mutable_data().layer_data->is_cylinder());
  const auto& layer_specific_data =
      *layer.mutable_data().layer_data->get_cylinder();

  // Zero-initialize everything.
  XrCompositionLayerCylinderKHR cylinder{
      XR_TYPE_COMPOSITION_LAYER_CYLINDER_KHR};
  cylinder.next = xr_next_struct;
  cylinder.space = layer.space();
  cylinder.radius = layer_specific_data.radius;
  cylinder.centralAngle = layer_specific_data.central_angle;
  cylinder.aspectRatio = layer_specific_data.aspect_ratio;
  cylinder.pose = GfxTransformToXrPose(layer_specific_data.transform);
  if (layer.mutable_data().blend_texture_source_alpha) {
    cylinder.layerFlags |= XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
  }
  FillSubImage(cylinder, layer);

  return cylinder;
}

XrCompositionLayerEquirect2KHR BuildEquirectLayerData(
    const OpenXrCompositionLayer& layer,
    const void* xr_next_struct) {
  CHECK(layer.mutable_data().layer_data->is_equirect());
  const auto& layer_specific_data =
      *layer.mutable_data().layer_data->get_equirect();

  // Zero-initialize everything.
  XrCompositionLayerEquirect2KHR equirect{
      XR_TYPE_COMPOSITION_LAYER_EQUIRECT2_KHR};
  equirect.next = xr_next_struct;
  equirect.space = layer.space();
  equirect.radius = layer_specific_data.radius;
  equirect.centralHorizontalAngle =
      layer_specific_data.central_horizontal_angle;
  equirect.upperVerticalAngle = layer_specific_data.upper_vertical_angle;
  equirect.lowerVerticalAngle = layer_specific_data.lower_vertical_angle;
  equirect.pose = GfxTransformToXrPose(layer_specific_data.transform);
  if (layer.mutable_data().blend_texture_source_alpha) {
    equirect.layerFlags |= XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
  }
  FillSubImage(equirect, layer);

  return equirect;
}

}  // namespace

// static
XrCompositionLayerBaseHeader* OpenXrLayers::GetLayerHeaderFromUnion(
    OpenXrLayers::XrCompositionLayerUnion& xr_layer_union,
    const OpenXrCompositionLayer& layer) {
  switch (layer.type()) {
    case OpenXrCompositionLayer::Type::kProjection:
      return reinterpret_cast<XrCompositionLayerBaseHeader*>(
          &xr_layer_union.projection);
    case OpenXrCompositionLayer::Type::kQuad:
      return reinterpret_cast<XrCompositionLayerBaseHeader*>(
          &xr_layer_union.quad);
    case OpenXrCompositionLayer::Type::kCylinder:
      return reinterpret_cast<XrCompositionLayerBaseHeader*>(
          &xr_layer_union.cylinder);
    case OpenXrCompositionLayer::Type::kEquirect:
      return reinterpret_cast<XrCompositionLayerBaseHeader*>(
          &xr_layer_union.equirect);
    default:
      NOTREACHED();
  }
}

OpenXrLayers::OpenXrLayers() = default;
OpenXrLayers::~OpenXrLayers() = default;

void OpenXrLayers::AddBaseLayer(
    XrSpace space,
    std::vector<XrCompositionLayerProjectionView> projection_views,
    const void* xr_next_struct) {
  InitializeBaseLayer(space, base_layer_, std::move(projection_views),
                      xr_next_struct);
  primary_composition_layers_.push_back(
      reinterpret_cast<XrCompositionLayerBaseHeader*>(&base_layer_));
}

void OpenXrLayers::AddSecondaryLayerForType(
    XrSpace space,
    XrViewConfigurationType type,
    XrEnvironmentBlendMode blend_mode,
    std::vector<XrCompositionLayerProjectionView> projection_views,
    const void* xr_next_struct) {
  secondary_projection_layers_.push_back(
      std::make_unique<XrCompositionLayerProjection>());
  InitializeBaseLayer(space, *secondary_projection_layers_.back(),
                      std::move(projection_views), xr_next_struct);
  secondary_composition_layers_.push_back(
      reinterpret_cast<XrCompositionLayerBaseHeader*>(
          secondary_projection_layers_.back().get()));

  secondary_layer_info_.emplace_back();
  XrSecondaryViewConfigurationLayerInfoMSFT& layer_info =
      secondary_layer_info_.back();
  layer_info.type = XR_TYPE_SECONDARY_VIEW_CONFIGURATION_LAYER_INFO_MSFT;
  layer_info.viewConfigurationType = type;
  layer_info.environmentBlendMode = blend_mode;
  layer_info.layerCount = 1;
  layer_info.layers = &secondary_composition_layers_.back();
}

void OpenXrLayers::InitializeBaseLayer(
    XrSpace space,
    XrCompositionLayerProjection& layer,
    std::vector<XrCompositionLayerProjectionView>&& projection_views,
    const void* xr_next_struct) {
  layer.type = XR_TYPE_COMPOSITION_LAYER_PROJECTION;
  layer.layerFlags = 0;
  layer.space = space;
  layer.viewCount = projection_views.size();
  layer.views = projection_views.data();
  layer.next = xr_next_struct;
  // Always set this flag for the base layer. For VR mode, we have already
  // set environmentBlendMode.
  layer.layerFlags |= XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;

  projection_views_pool_.push_back(std::move(projection_views));
}

void OpenXrLayers::AddCompositionLayer(
    const OpenXrCompositionLayer& layer,
    std::vector<XrCompositionLayerProjectionView> projection_views,
    const void* xr_next_struct) {
  // Layer requested in the middle of the request animation frame request
  if (!layer.is_rendered()) {
    return;
  }

  XrCompositionLayerUnion xr_layer_union;
  switch (layer.type()) {
    case OpenXrCompositionLayer::Type::kProjection:
      CHECK(!projection_views.empty());
      xr_layer_union.projection =
          BuildProjectionLayerData(layer, projection_views, xr_next_struct);
      projection_views_pool_.push_back(std::move(projection_views));
      break;
    case OpenXrCompositionLayer::Type::kQuad:
      xr_layer_union.quad = BuildQuadLayerData(layer, xr_next_struct);
      break;
    case OpenXrCompositionLayer::Type::kCylinder:
      xr_layer_union.cylinder = BuildCylinderLayerData(layer, xr_next_struct);
      break;
    case OpenXrCompositionLayer::Type::kEquirect:
      xr_layer_union.equirect = BuildEquirectLayerData(layer, xr_next_struct);
      break;
    default:
      NOTREACHED();
  }

  composition_layers_.push_back(
      std::make_unique<XrCompositionLayerUnion>(xr_layer_union));
  primary_composition_layers_.push_back(
      GetLayerHeaderFromUnion(*composition_layers_.back(), layer));
}

}  // namespace device
