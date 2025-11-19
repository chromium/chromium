// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_OPENXR_OPENXR_LAYERS_H_
#define DEVICE_VR_OPENXR_OPENXR_LAYERS_H_

#include <memory>
#include <vector>

#include "base/memory/raw_ptr_exclusion.h"
#include "third_party/openxr/src/include/openxr/openxr.h"

namespace device {
class OpenXrCompositionLayer;
class OpenXrApiWrapper;

// A wrapper around all of the layers to be submitted to a certain frame. Each
// frame creates its own OpenXrLayers object and populates it with all the
// layers of active view configurations. This information is passed into
// xrEndFrame to complete the frame.
class OpenXrLayers {
 public:
  OpenXrLayers();
  ~OpenXrLayers();

  void AddBaseLayer(
      XrSpace space,
      std::vector<XrCompositionLayerProjectionView> primary_projection_views,
      const void* xr_next_struct);

  void AddCompositionLayer(
      OpenXrApiWrapper* openxr,
      const OpenXrCompositionLayer& layer,
      std::vector<XrCompositionLayerProjectionView> projection_views,
      const void* xr_next_struct);

  void AddSecondaryLayerForType(
      XrSpace space,
      XrViewConfigurationType type,
      XrEnvironmentBlendMode blend_mode,
      std::vector<XrCompositionLayerProjectionView> projection_views,
      const void* xr_next_struct);

  uint32_t PrimaryLayerCount() const {
    return primary_composition_layers_.size();
  }

  const XrCompositionLayerBaseHeader* const* PrimaryLayerData() const {
    return primary_composition_layers_.data();
  }

  uint32_t SecondaryConfigCount() const { return secondary_layer_info_.size(); }

  const XrSecondaryViewConfigurationLayerInfoMSFT* SecondaryConfigData() const {
    return secondary_layer_info_.data();
  }

 private:
  union XrCompositionLayerUnion {
    XrCompositionLayerProjection projection;
    XrCompositionLayerQuad quad;
    XrCompositionLayerCylinderKHR cylinder;
    XrCompositionLayerEquirect2KHR equirect;
    XrCompositionLayerCubeKHR cube;
  };

  static XrCompositionLayerBaseHeader* GetLayerHeaderFromUnion(
      OpenXrLayers::XrCompositionLayerUnion& layer_union,
      const OpenXrCompositionLayer& layer);

  void InitializeBaseLayer(
      XrSpace space,
      XrCompositionLayerProjection& layer,
      std::vector<XrCompositionLayerProjectionView>&& projection_views,
      const void* xr_next_struct);

  // The base layer is used when there is no layers created by client.
  XrCompositionLayerProjection base_layer_;

  std::vector<std::unique_ptr<XrCompositionLayerUnion>> composition_layers_;

  // The layers for secondary view configurations. We currently only support a
  // single layer per view configuration, so each element in this vector is the
  // layer for a specific view configuration.
  std::vector<std::unique_ptr<XrCompositionLayerProjection>>
      secondary_projection_layers_;

  // Pointers to the corresponding layer in base_layer_ and
  // secondary_projection_layers_. quad_layers cylinder_layers_,
  // equirect_layers_ and cube_layers. This field is not vector<raw_ptr<...>>
  // due to interaction with third_party api.
  RAW_PTR_EXCLUSION std::vector<XrCompositionLayerBaseHeader*>
      primary_composition_layers_;

  // Pointers to the corresponding layer in secondary_projection_layers_.
  // This field is not vector<raw_ptr<...>> due to interaction with third_party
  // api.
  RAW_PTR_EXCLUSION std::vector<XrCompositionLayerBaseHeader*>
      secondary_composition_layers_;

  // The secondary view configuration layer info containing the data above,
  // which is passed to xrEndFrame.
  std::vector<XrSecondaryViewConfigurationLayerInfoMSFT> secondary_layer_info_;

  // Keep all projection views for projection layers excluding the base layer,
  // so that the XrCompositionLayerProjectionView pointer in
  // XrCompositionLayerProjection will be valid.
  std::vector<std::vector<XrCompositionLayerProjectionView>>
      projection_views_pool_;
};
}  // namespace device

#endif  // DEVICE_VR_OPENXR_OPENXR_LAYERS_H_
