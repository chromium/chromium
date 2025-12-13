// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/openxr/openxr_layers.h"

#include "base/notreached.h"
#include "device/vr/openxr/openxr_api_wrapper.h"
#include "device/vr/openxr/openxr_composition_layer.h"
#include "device/vr/openxr/openxr_util.h"
#include "ui/gfx/geometry/decomposed_transform.h"

namespace device {

namespace {

template <typename XrLayerDataType>
void FillSubImage(XrLayerDataType& xr_layer,
                  const OpenXrCompositionLayer& layer,
                  const XrEyeVisibility eye) {
  const gfx::Rect info = layer.GetSubImageViewport(eye);

  xr_layer.subImage.swapchain = layer.color_swapchain();
  xr_layer.subImage.imageArrayIndex = 0;
  xr_layer.subImage.imageRect.offset = {
      .x = info.x(),
      .y = info.y(),
  };
  xr_layer.subImage.imageRect.extent = {
      .width = info.width(),
      .height = info.height(),
  };
}

const gfx::Transform GetNativeOriginFromLayer(
    const OpenXrCompositionLayer& layer) {
  switch (layer.type()) {
    case OpenXrCompositionLayer::Type::kProjection:
    case OpenXrCompositionLayer::Type::kCube:
      return gfx::Transform();
    case OpenXrCompositionLayer::Type::kQuad:
      return layer.mutable_data()
          .layer_data->get_quad()
          ->native_origin_from_layer;
    case OpenXrCompositionLayer::Type::kCylinder:
      return layer.mutable_data()
          .layer_data->get_cylinder()
          ->native_origin_from_layer;
    case OpenXrCompositionLayer::Type::kEquirect:
      return layer.mutable_data()
          .layer_data->get_equirect()
          ->native_origin_from_layer;
  }
}

XrCompositionLayerProjection BuildProjectionLayerData(
    const XrLocation& location,
    const OpenXrCompositionLayer& layer,
    const std::vector<XrCompositionLayerProjectionView>& projection_views,
    const void* xr_next_struct) {
  // Zero-initialize everything.
  XrCompositionLayerProjection projection{XR_TYPE_COMPOSITION_LAYER_PROJECTION};
  projection.next = xr_next_struct;
  projection.space = location.space;
  projection.viewCount = projection_views.size();
  projection.views = projection_views.data();
  if (layer.mutable_data().blend_texture_source_alpha) {
    projection.layerFlags |=
        XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
  }
  return projection;
}

XrCompositionLayerQuad BuildQuadLayerData(const XrLocation& location,
                                          const OpenXrCompositionLayer& layer,
                                          const XrEyeVisibility eye,
                                          const void* xr_next_struct) {
  CHECK(layer.mutable_data().layer_data->is_quad());
  const auto& layer_specific_data =
      *layer.mutable_data().layer_data->get_quad();

  // Zero-initialize everything.
  XrCompositionLayerQuad quad{XR_TYPE_COMPOSITION_LAYER_QUAD};
  quad.next = xr_next_struct;
  quad.space = location.space;
  quad.eyeVisibility = eye;
  quad.size = {
      .width = layer_specific_data.width,
      .height = layer_specific_data.height,
  };
  quad.pose = location.pose;
  if (layer.mutable_data().blend_texture_source_alpha) {
    quad.layerFlags |= XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
  }
  FillSubImage(quad, layer, eye);

  return quad;
}

XrCompositionLayerCylinderKHR BuildCylinderLayerData(
    const XrLocation& location,
    const OpenXrCompositionLayer& layer,
    const XrEyeVisibility eye,
    const void* xr_next_struct) {
  CHECK(layer.mutable_data().layer_data->is_cylinder());
  const auto& layer_specific_data =
      *layer.mutable_data().layer_data->get_cylinder();

  // Zero-initialize everything.
  XrCompositionLayerCylinderKHR cylinder{
      XR_TYPE_COMPOSITION_LAYER_CYLINDER_KHR};
  cylinder.next = xr_next_struct;
  cylinder.space = location.space;
  cylinder.eyeVisibility = eye;
  cylinder.radius = layer_specific_data.radius;
  cylinder.centralAngle = layer_specific_data.central_angle;
  cylinder.aspectRatio = layer_specific_data.aspect_ratio;
  cylinder.pose = location.pose;
  if (layer.mutable_data().blend_texture_source_alpha) {
    cylinder.layerFlags |= XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
  }
  FillSubImage(cylinder, layer, eye);

  return cylinder;
}

XrCompositionLayerEquirect2KHR BuildEquirectLayerData(
    const XrLocation& location,
    const OpenXrCompositionLayer& layer,
    const XrEyeVisibility eye,
    const void* xr_next_struct) {
  CHECK(layer.mutable_data().layer_data->is_equirect());
  const auto& layer_specific_data =
      *layer.mutable_data().layer_data->get_equirect();

  // Zero-initialize everything.
  XrCompositionLayerEquirect2KHR equirect{
      XR_TYPE_COMPOSITION_LAYER_EQUIRECT2_KHR};
  equirect.next = xr_next_struct;
  equirect.space = location.space;
  equirect.eyeVisibility = eye;
  equirect.radius = layer_specific_data.radius;
  equirect.centralHorizontalAngle =
      layer_specific_data.central_horizontal_angle;
  equirect.upperVerticalAngle = layer_specific_data.upper_vertical_angle;
  equirect.lowerVerticalAngle = layer_specific_data.lower_vertical_angle;
  equirect.pose = location.pose;
  if (layer.mutable_data().blend_texture_source_alpha) {
    equirect.layerFlags |= XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
  }
  FillSubImage(equirect, layer, eye);

  return equirect;
}

XrCompositionLayerCubeKHR BuildCubeLayerData(
    const XrLocation& location,
    const OpenXrCompositionLayer& layer,
    const void* xr_next_struct) {
  CHECK(layer.mutable_data().layer_data->is_cube());
  const auto& layer_specific_data =
      *layer.mutable_data().layer_data->get_cube();

  // Zero-initialize everything.
  XrCompositionLayerCubeKHR cube{XR_TYPE_COMPOSITION_LAYER_CUBE_KHR};
  cube.next = xr_next_struct;
  cube.space = location.space;
  cube.swapchain = layer.color_swapchain();
  cube.orientation =
      GfxQuaternionToXrQuaternion(layer_specific_data.orientation);

  if (layer.mutable_data().blend_texture_source_alpha) {
    cube.layerFlags |= XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
  }

  return cube;
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
    case OpenXrCompositionLayer::Type::kCube:
      return reinterpret_cast<XrCompositionLayerBaseHeader*>(
          &xr_layer_union.cube);
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
    OpenXrApiWrapper* openxr,
    const OpenXrCompositionLayer& layer,
    std::vector<XrCompositionLayerProjectionView> projection_views,
    const void* xr_next_struct) {
  // The layer has no content to show.
  if (layer.needs_redraw()) {
    return;
  }

  std::optional<XrLocation> location =
      openxr->GetXrLocationFromNativeOriginInformation(
          layer.native_origin_information(), GetNativeOriginFromLayer(layer));
  if (!location) {
    // There is no information on where to show the layer.
    return;
  }

  for (XrEyeVisibility eye : layer.GetXrEyesForComposition()) {
    // There is no need for per-eye sub-image composition for the projection
    // layer.
    if (layer.type() == OpenXrCompositionLayer::Type::kProjection) {
      CHECK_EQ(eye, XR_EYE_VISIBILITY_BOTH);
    }

    XrCompositionLayerUnion xr_layer_union;
    switch (layer.type()) {
      case OpenXrCompositionLayer::Type::kProjection:
        CHECK(!projection_views.empty());
        xr_layer_union.projection = BuildProjectionLayerData(
            *location, layer, projection_views, xr_next_struct);
        projection_views_pool_.push_back(std::move(projection_views));
        break;
      case OpenXrCompositionLayer::Type::kQuad:
        xr_layer_union.quad =
            BuildQuadLayerData(*location, layer, eye, xr_next_struct);
        break;
      case OpenXrCompositionLayer::Type::kCylinder:
        xr_layer_union.cylinder =
            BuildCylinderLayerData(*location, layer, eye, xr_next_struct);
        break;
      case OpenXrCompositionLayer::Type::kEquirect:
        xr_layer_union.equirect =
            BuildEquirectLayerData(*location, layer, eye, xr_next_struct);
        break;
      case OpenXrCompositionLayer::Type::kCube:
        xr_layer_union.cube =
            BuildCubeLayerData(*location, layer, xr_next_struct);
        break;
      default:
        NOTREACHED();
    }

    composition_layers_.push_back(
        std::make_unique<XrCompositionLayerUnion>(xr_layer_union));
    primary_composition_layers_.push_back(
        GetLayerHeaderFromUnion(*composition_layers_.back(), layer));
  }
}

}  // namespace device
