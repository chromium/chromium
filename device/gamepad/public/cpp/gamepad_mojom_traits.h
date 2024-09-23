// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#ifndef DEVICE_GAMEPAD_PUBLIC_CPP_GAMEPAD_MOJOM_TRAITS_H_
#define DEVICE_GAMEPAD_PUBLIC_CPP_GAMEPAD_MOJOM_TRAITS_H_

#include <stddef.h>

#include "base/component_export.h"
#include "base/containers/span.h"
#include "device/gamepad/public/cpp/gamepad.h"
#include "device/gamepad/public/mojom/gamepad.mojom-shared.h"
#include "mojo/public/cpp/bindings/array_traits_span.h"
#include "mojo/public/cpp/bindings/struct_traits.h"

namespace mojo {

template <>
struct COMPONENT_EXPORT(GAMEPAD_SHARED_TRAITS)
    StructTraits<device::mojom::GamepadQuaternionDataView,
                 device::GamepadQuaternion> {
  static bool IsNull(const device::GamepadQuaternion& r) { return !r.not_null; }
  static void SetToNull(device::GamepadQuaternion* out);
  static float x(const device::GamepadQuaternion& r) { return r.x; }
  static float y(const device::GamepadQuaternion& r) { return r.y; }
  static float z(const device::GamepadQuaternion& r) { return r.z; }
  static float w(const device::GamepadQuaternion& r) { return r.w; }
  static bool Read(device::mojom::GamepadQuaternionDataView data,
                   device::GamepadQuaternion* out);
};

template <>
struct COMPONENT_EXPORT(GAMEPAD_SHARED_TRAITS)
    StructTraits<device::mojom::GamepadVectorDataView, device::GamepadVector> {
  static bool IsNull(const device::GamepadVector& r) { return !r.not_null; }
  static void SetToNull(device::GamepadVector* out);
  static float x(const device::GamepadVector& r) { return r.x; }
  static float y(const device::GamepadVector& r) { return r.y; }
  static float z(const device::GamepadVector& r) { return r.z; }
  static bool Read(device::mojom::GamepadVectorDataView data,
                   device::GamepadVector* out);
};

template <>
struct COMPONENT_EXPORT(GAMEPAD_SHARED_TRAITS)
    StructTraits<device::mojom::GamepadButtonDataView, device::GamepadButton> {
  static bool pressed(const device::GamepadButton& r) { return r.pressed; }
  static bool touched(const device::GamepadButton& r) { return r.touched; }
  static double value(const device::GamepadButton& r) { return r.value; }
  static bool Read(device::mojom::GamepadButtonDataView data,
                   device::GamepadButton* out);
};

template <>
struct COMPONENT_EXPORT(GAMEPAD_SHARED_TRAITS)
    EnumTraits<device::mojom::GamepadHapticActuatorType,
               device::GamepadHapticActuatorType> {
  static device::mojom::GamepadHapticActuatorType ToMojom(
      device::GamepadHapticActuatorType input);
  static bool FromMojom(device::mojom::GamepadHapticActuatorType input,
                        device::GamepadHapticActuatorType* output);
};

template <>
struct COMPONENT_EXPORT(GAMEPAD_SHARED_TRAITS)
    StructTraits<device::mojom::GamepadHapticActuatorDataView,
                 device::GamepadHapticActuator> {
  static bool IsNull(const device::GamepadHapticActuator& r) {
    return !r.not_null;
  }
  static void SetToNull(device::GamepadHapticActuator* out);
  static const device::GamepadHapticActuatorType& type(
      const device::GamepadHapticActuator& r) {
    return r.type;
  }
  static bool Read(device::mojom::GamepadHapticActuatorDataView data,
                   device::GamepadHapticActuator* out);
};

template <>
struct COMPONENT_EXPORT(GAMEPAD_SHARED_TRAITS)
    StructTraits<device::mojom::GamepadPoseDataView, device::GamepadPose> {
  static bool IsNull(const device::GamepadPose& r) { return !r.not_null; }
  static void SetToNull(device::GamepadPose* out);
  static const device::GamepadQuaternion& orientation(
      const device::GamepadPose& r) {
    return r.orientation;
  }
  static const device::GamepadVector& position(const device::GamepadPose& r) {
    return r.position;
  }
  static const device::GamepadVector& angular_velocity(
      const device::GamepadPose& r) {
    return r.angular_velocity;
  }
  static const device::GamepadVector& linear_velocity(
      const device::GamepadPose& r) {
    return r.linear_velocity;
  }
  static const device::GamepadVector& angular_acceleration(
      const device::GamepadPose& r) {
    return r.angular_acceleration;
  }
  static const device::GamepadVector& linear_acceleration(
      const device::GamepadPose& r) {
    return r.linear_acceleration;
  }
  static bool Read(device::mojom::GamepadPoseDataView data,
                   device::GamepadPose* out);
};

template <>
struct COMPONENT_EXPORT(GAMEPAD_SHARED_TRAITS)
    EnumTraits<device::mojom::GamepadMapping, device::GamepadMapping> {
  static device::mojom::GamepadMapping ToMojom(device::GamepadMapping input);
  static bool FromMojom(device::mojom::GamepadMapping input,
                        device::GamepadMapping* output);
};

template <>
struct COMPONENT_EXPORT(GAMEPAD_SHARED_TRAITS)
    StructTraits<device::mojom::GamepadTouchDataView, device::GamepadTouch> {
  static uint32_t touch_id(const device::GamepadTouch& r) { return r.touch_id; }
  static uint32_t surface_id(const device::GamepadTouch& r) {
    return r.surface_id;
  }
  static bool has_surface_dimensions(const device::GamepadTouch& r) {
    return r.has_surface_dimensions;
  }

  static double x(const device::GamepadTouch& r) { return r.x; }
  static double y(const device::GamepadTouch& r) { return r.y; }

  static uint32_t surface_width(const device::GamepadTouch& r) {
    return r.surface_width;
  }
  static uint32_t surface_height(const device::GamepadTouch& r) {
    return r.surface_height;
  }

  static bool Read(device::mojom::GamepadTouchDataView data,
                   device::GamepadTouch* out);
};

template <>
struct COMPONENT_EXPORT(GAMEPAD_SHARED_TRAITS)
    EnumTraits<device::mojom::GamepadHand, device::GamepadHand> {
  static device::mojom::GamepadHand ToMojom(device::GamepadHand input);
  static bool FromMojom(device::mojom::GamepadHand input,
                        device::GamepadHand* output);
};

template <>
struct COMPONENT_EXPORT(GAMEPAD_SHARED_TRAITS)
    StructTraits<device::mojom::GamepadDataView, device::Gamepad> {
  static bool connected(const device::Gamepad& r) { return r.connected; }
  static int64_t timestamp(const device::Gamepad& r) { return r.timestamp; }
  static base::span<const double> axes(const device::Gamepad& r) {
    return base::make_span(r.axes, r.axes_length);
  }
  static base::span<const device::GamepadButton> buttons(
      const device::Gamepad& r) {
    return base::make_span(r.buttons, r.buttons_length);
  }
  static const device::GamepadHapticActuator& vibration_actuator(
      const device::Gamepad& r) {
    return r.vibration_actuator;
  }
  static const device::GamepadPose& pose(const device::Gamepad& r) {
    return r.pose;
  }
  static base::span<const device::GamepadTouch> touch_events(
      const device::Gamepad& r) {
    return base::make_span(r.touch_events, r.touch_events_length);
  }
  static const device::GamepadHand& hand(const device::Gamepad& r) {
    return r.hand;
  }
  static uint32_t display_id(const device::Gamepad& r) { return r.display_id; }

  static base::span<const uint16_t> id(const device::Gamepad& r);
  static const device::GamepadMapping& mapping(const device::Gamepad& r) {
    return r.mapping;
  }
  static bool Read(device::mojom::GamepadDataView data, device::Gamepad* out);
};

}  // namespace mojo

#endif  // DEVICE_GAMEPAD_PUBLIC_CPP_GAMEPAD_MOJOM_TRAITS_H_
