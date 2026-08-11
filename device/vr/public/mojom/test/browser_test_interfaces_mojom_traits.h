// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_PUBLIC_MOJOM_TEST_BROWSER_TEST_INTERFACES_MOJOM_TRAITS_H_
#define DEVICE_VR_PUBLIC_MOJOM_TEST_BROWSER_TEST_INTERFACES_MOJOM_TRAITS_H_

#include "device/vr/public/mojom/test/browser_test_interfaces.mojom.h"
#include "device/vr/public/mojom/test/controller_frame_data.h"
#include "device/vr/public/mojom/test/device_config.h"
#include "device/vr/public/mojom/test/layer_data.h"
#include "device/vr/public/mojom/test/view_data.h"
#include "device/vr/public/mojom/vr_service.mojom.h"
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
struct EnumTraits<device_test::mojom::ControllerRole, device::ControllerRole> {
  static device_test::mojom::ControllerRole ToMojom(
      device::ControllerRole input) {
    switch (input) {
      case device::ControllerRole::kControllerRoleLeft:
        return device_test::mojom::ControllerRole::kControllerRoleLeft;
      case device::ControllerRole::kControllerRoleRight:
        return device_test::mojom::ControllerRole::kControllerRoleRight;
      case device::ControllerRole::kControllerRoleInvalid:
        return device_test::mojom::ControllerRole::kControllerRoleInvalid;
      case device::ControllerRole::kControllerRoleVoice:
        return device_test::mojom::ControllerRole::kControllerRoleVoice;
    }
    NOTREACHED();
    return device_test::mojom::ControllerRole::kControllerRoleInvalid;
  }

  static device::ControllerRole FromMojom(
      device_test::mojom::ControllerRole input) {
    switch (input) {
      case device_test::mojom::ControllerRole::kControllerRoleLeft:
        return device::ControllerRole::kControllerRoleLeft;
      case device_test::mojom::ControllerRole::kControllerRoleRight:
        return device::ControllerRole::kControllerRoleRight;
      case device_test::mojom::ControllerRole::kControllerRoleInvalid:
        return device::ControllerRole::kControllerRoleInvalid;
      case device_test::mojom::ControllerRole::kControllerRoleVoice:
        return device::ControllerRole::kControllerRoleVoice;
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
struct StructTraits<device_test::mojom::ControllerAxisDataDataView,
                    device::ControllerAxisData> {
  static float x(const device::ControllerAxisData& axis_data) {
    return axis_data.x;
  }
  static float y(const device::ControllerAxisData& axis_data) {
    return axis_data.y;
  }
  static uint8_t axis_type(const device::ControllerAxisData& axis_data) {
    return axis_data.axis_type;
  }

  static bool Read(device_test::mojom::ControllerAxisDataDataView data,
                   device::ControllerAxisData* out) {
    out->x = data.x();
    out->y = data.y();
    out->axis_type = data.axis_type();
    return true;
  }
};

template <>
struct StructTraits<device::mojom::XRHandJointDataDataView,
                    device::XRHandJointData> {
  static device::mojom::XRHandJoint joint(
      const device::XRHandJointData& joint_data) {
    return joint_data.joint;
  }
  static const std::optional<gfx::Transform>& mojo_from_joint(
      const device::XRHandJointData& joint_data) {
    return joint_data.mojo_from_joint;
  }
  static float radius(const device::XRHandJointData& joint_data) {
    return joint_data.radius;
  }

  static bool Read(device::mojom::XRHandJointDataDataView data,
                   device::XRHandJointData* out) {
    if (!data.ReadJoint(&out->joint)) {
      return false;
    }
    if (!data.ReadMojoFromJoint(&out->mojo_from_joint)) {
      return false;
    }
    out->radius = data.radius();
    return true;
  }
};

template <>
struct StructTraits<
    device::mojom::XRHandTrackingDataDataView,
    std::array<device::XRHandJointData, device::kNumJointsForTest>> {
  static const std::array<device::XRHandJointData, device::kNumJointsForTest>&
  hand_joint_data(const std::array<device::XRHandJointData,
                                   device::kNumJointsForTest>& hand_data) {
    return hand_data;
  }

  static bool Read(
      device::mojom::XRHandTrackingDataDataView data,
      std::array<device::XRHandJointData, device::kNumJointsForTest>* out) {
    return data.ReadHandJointData(out);
  }
};

template <>
struct StructTraits<device_test::mojom::ControllerFrameDataDataView,
                    device::ControllerFrameData> {
  static uint32_t packet_number(const device::ControllerFrameData& data) {
    return data.packet_number;
  }
  static uint64_t buttons_pressed(const device::ControllerFrameData& data) {
    return data.buttons_pressed;
  }
  static uint64_t buttons_touched(const device::ControllerFrameData& data) {
    return data.buttons_touched;
  }
  static uint64_t supported_buttons(const device::ControllerFrameData& data) {
    return data.supported_buttons;
  }
  static const std::array<device::ControllerAxisData, device::kMaxNumAxes>&
  axis_data(const device::ControllerFrameData& data) {
    return data.axis_data;
  }
  static const std::optional<gfx::Transform>& pose_data(
      const device::ControllerFrameData& data) {
    return data.pose_data;
  }
  static device::ControllerRole role(const device::ControllerFrameData& data) {
    return data.role;
  }
  static std::optional<
      std::array<device::XRHandJointData, device::kNumJointsForTest>>
  hand_data(const device::ControllerFrameData& data) {
    if (!data.has_hand_data) {
      return std::nullopt;
    }

    return data.hand_data;
  }
  static bool is_valid(const device::ControllerFrameData& data) {
    return data.is_valid;
  }

  static bool Read(device_test::mojom::ControllerFrameDataDataView data,
                   device::ControllerFrameData* out) {
    out->packet_number = data.packet_number();
    out->buttons_pressed = data.buttons_pressed();
    out->buttons_touched = data.buttons_touched();
    out->supported_buttons = data.supported_buttons();
    if (!data.ReadAxisData(&out->axis_data)) {
      return false;
    }
    if (!data.ReadPoseData(&out->pose_data)) {
      return false;
    }
    if (!data.ReadRole(&out->role)) {
      return false;
    }

    std::optional<
        std::array<device::XRHandJointData, device::kNumJointsForTest>>
        maybe_hand_data;
    if (!data.ReadHandData(&maybe_hand_data)) {
      return false;
    }
    out->has_hand_data = maybe_hand_data.has_value();
    if (maybe_hand_data) {
      out->hand_data = *maybe_hand_data;
    }
    out->is_valid = data.is_valid();
    return true;
  }
};

}  // namespace mojo

#endif  // DEVICE_VR_PUBLIC_MOJOM_TEST_BROWSER_TEST_INTERFACES_MOJOM_TRAITS_H_
