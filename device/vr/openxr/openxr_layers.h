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
class OpenXrGraphicsBinding;

// A wrapper around all of the layers to be submitted to a certain frame. Each
// frame creates its own OpenXrLayers object and populates it with all the
// layers of active view configurations. This information is passed into
// xrEndFrame to complete the frame.
class OpenXrLayers {
 public:
  OpenXrLayers(XrSpace space,
               XrEnvironmentBlendMode blend_mode,
               const OpenXrGraphicsBinding& graphics_binding,
               const std::vector<XrCompositionLayerProjectionView>&
                   primary_projection_views);
  ~OpenXrLayers();

  void AddSecondaryLayerForType(
      const OpenXrGraphicsBinding& graphics_binding,
      XrViewConfigurationType type,
      const std::vector<XrCompositionLayerProjectionView>& projection_views);

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
  void InitializeLayer(
      const OpenXrGraphicsBinding& graphics_binding,
      const std::vector<XrCompositionLayerProjectionView>& projection_views,
      XrCompositionLayerProjection& layer);

  XrSpace space_ = XR_NULL_HANDLE;
  XrEnvironmentBlendMode blend_mode_ = XR_ENVIRONMENT_BLEND_MODE_MAX_ENUM;

  // In OpenXR, it is possible to have multiple layers, as well as multiple
  // types of layers (such as projection and quad layers). We currently only
  // support a single projection layer. XrCompositionLayerBaseHeader* is needed
  // because xrEndFrame expects an array containing pointers of all the layers.
  XrCompositionLayerProjection primary_projection_layer_;

  // The layers for secondary view configurations. We currently only support a
  // single layer per view configuration, so each element in this vector is the
  // layer for a specific view configuration.
  std::vector<std::unique_ptr<XrCompositionLayerProjection>>
      secondary_projection_layers_;

  // Pointers to the corresponding layer in primary_projection_layer_,
  // quad_layers cylinder_layers_, equirect_layers_ and cube_layers. This field
  // is not vector<raw_ptr<...>> due to interaction with third_party api.
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
};
}  // namespace device

#endif  // DEVICE_VR_OPENXR_OPENXR_LAYERS_H_
