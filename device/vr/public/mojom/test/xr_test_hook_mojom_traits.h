// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_PUBLIC_MOJOM_TEST_XR_TEST_HOOK_MOJOM_TRAITS_H_
#define DEVICE_VR_PUBLIC_MOJOM_TEST_XR_TEST_HOOK_MOJOM_TRAITS_H_

#include "device/gamepad/public/mojom/gamepad.mojom.h"
#include "device/vr/public/mojom/test/controller_frame_data.h"
#include "device/vr/public/mojom/test/device_config.h"
#include "device/vr/public/mojom/test/layer_data.h"
#include "device/vr/public/mojom/test/view_data.h"
#include "device/vr/public/mojom/test/xr_test_hook.test-mojom.h"
#include "device/vr/public/mojom/vr_service.mojom.h"
#include "device/vr/public/mojom/xr_hand_tracking_data.mojom.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "skia/public/mojom/skcolor_mojom_traits.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/mojom/geometry_mojom_traits.h"
#include "ui/gfx/mojom/transform_mojom_traits.h"

namespace mojo {

template <>
struct EnumTraits<device_test::mojom::LayerType, device::LayerType> {
  static device_test::mojom::LayerType ToMojom(device::LayerType input) {
    switch (input) {
      case device::LayerType::kQuad:
        return device_test::mojom::LayerType::QUAD;
      case device::LayerType::kCylinder:
        return device_test::mojom::LayerType::CYLINDER;
      case device::LayerType::kEquirect:
        return device_test::mojom::LayerType::EQUIRECT;
      case device::LayerType::kCube:
        return device_test::mojom::LayerType::CUBE;
      case device::LayerType::kNone:
        return device_test::mojom::LayerType::NONE;
    }
    NOTREACHED();
    return device_test::mojom::LayerType::NONE;
  }

  static device::LayerType FromMojom(device_test::mojom::LayerType input) {
    switch (input) {
      case device_test::mojom::LayerType::QUAD:
        return device::LayerType::kQuad;
      case device_test::mojom::LayerType::CYLINDER:
        return device::LayerType::kCylinder;
      case device_test::mojom::LayerType::EQUIRECT:
        return device::LayerType::kEquirect;
      case device_test::mojom::LayerType::CUBE:
        return device::LayerType::kCube;
      case device_test::mojom::LayerType::NONE:
        return device::LayerType::kNone;
    }
    NOTREACHED();
  }
};

template <>
struct StructTraits<device_test::mojom::LayerDataDataView, device::LayerData> {
  static device::LayerType type(const device::LayerData& layer_data) {
    return layer_data.type;
  }
  static const std::vector<SkColor>& face_colors(
      const device::LayerData& layer_data) {
    return layer_data.face_colors;
  }

  static bool Read(device_test::mojom::LayerDataDataView data,
                   device::LayerData* out) {
    if (!data.ReadType(&out->type)) {
      return false;
    }
    if (!data.ReadFaceColors(&out->face_colors)) {
      return false;
    }
    return true;
  }
};

template <>
struct StructTraits<device_test::mojom::ViewDataDataView, device::ViewData> {
  static SkColor color(const device::ViewData& view_data) {
    return view_data.color;
  }
  static device::mojom::XREye eye(const device::ViewData& view_data) {
    return view_data.eye;
  }
  static const gfx::Rect& viewport(const device::ViewData& view_data) {
    return view_data.viewport;
  }

  static bool Read(device_test::mojom::ViewDataDataView data,
                   device::ViewData* out) {
    if (!data.ReadColor(&out->color)) {
      return false;
    }
    if (!data.ReadEye(&out->eye)) {
      return false;
    }
    if (!data.ReadViewport(&out->viewport)) {
      return false;
    }
    return true;
  }
};

template <>
struct StructTraits<device_test::mojom::DeviceConfigDataView,
                    device::DeviceConfig> {
  static float interpupillary_distance(const device::DeviceConfig& config) {
    return config.interpupillary_distance;
  }

  static bool Read(device_test::mojom::DeviceConfigDataView data,
                   device::DeviceConfig* out) {
    out->interpupillary_distance = data.interpupillary_distance();
    return true;
  }
};

template <>
struct StructTraits<device_test::mojom::ControllerFrameDataDataView,
                    device::ControllerFrameData> {
  static device::mojom::XRHandedness handedness(
      const device::ControllerFrameData& data) {
    return data.handedness;
  }
  static const device::mojom::GamepadPtr& gamepad(
      const device::ControllerFrameData& data) {
    return data.gamepad;
  }
  static const std::optional<gfx::Transform>& pose_data(
      const device::ControllerFrameData& data) {
    return data.pose_data;
  }
  static const device::mojom::XRHandTrackingDataPtr& hand_data(
      const device::ControllerFrameData& data) {
    return data.hand_data;
  }
  static bool is_valid(const device::ControllerFrameData& data) {
    return data.is_valid;
  }

  static bool Read(device_test::mojom::ControllerFrameDataDataView data,
                   device::ControllerFrameData* out) {
    if (!data.ReadHandedness(&out->handedness)) {
      return false;
    }
    if (!data.ReadGamepad(&out->gamepad)) {
      return false;
    }
    if (!data.ReadPoseData(&out->pose_data)) {
      return false;
    }
    if (!data.ReadHandData(&out->hand_data)) {
      return false;
    }
    out->is_valid = data.is_valid();
    return true;
  }
};

}  // namespace mojo

#endif  // DEVICE_VR_PUBLIC_MOJOM_TEST_XR_TEST_HOOK_MOJOM_TRAITS_H_
