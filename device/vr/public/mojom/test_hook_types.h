// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_PUBLIC_MOJOM_TEST_HOOK_TYPES_H_
#define DEVICE_VR_PUBLIC_MOJOM_TEST_HOOK_TYPES_H_

#include <array>
#include <cstdint>
#include <optional>

#include "base/component_export.h"
#include "device/vr/public/mojom/browser_test_interfaces.mojom-shared.h"
#include "device/vr/public/mojom/xr_hand_tracking_data.mojom-shared.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/transform.h"

namespace device {

inline constexpr uint32_t kMaxControllers = 5;
inline constexpr unsigned int kMaxNumAxes = 5;
inline constexpr unsigned int kNumJointsForTest =
    static_cast<unsigned int>(device::mojom::XRHandJoint::kMaxValue) + 1;
inline constexpr unsigned int kNumVisibilityMaskVerticesForTest = 6;
inline constexpr unsigned int kNumVisibilityMaskIndicesForTest = 3;

struct Color {
  unsigned char r;
  unsigned char g;
  unsigned char b;
  unsigned char a;
};

enum class XrEye {
  kLeft = 0,
  kRight = 1,
  kNone = 2,
};

struct ViewData {
  Color color;
  XrEye eye;
  gfx::Rect viewport;
  char raw_buffer[256];  // Can encode raw data here.
};

struct DeviceConfig {
  float interpupillary_distance;
};

// These are largely the same as the OpenVR button/axis constants, but kept
// separate so they're more runtime-agnostic.
enum XrButtonId {
  kSystem = 0,
  kMenu = 1,
  kGrip = 2,
  kDpadLeft = 3,
  kDpadUp = 4,
  kDpadRight = 5,
  kDpadDown = 6,
  kA = 7,
  kB = 8,
  kX = 9,
  kY = 10,
  kThumbRest = 11,
  kShoulder = 12,
  kProximitySensor = 31,
  kAxisTrackpad = 32,
  kAxisTrigger = 33,
  kAxisThumbstick = 34,
  kAxisTertiary = 35,
  kAxisQuaternary = 36,
  kMax = 64
};

enum XrAxisType {
  kNone = 0,
  kTrackpad = 1,
  kJoystick = 2,
  kTrigger = 3,
};

inline uint64_t XrButtonMaskFromId(XrButtonId id) {
  return 1ull << id;
}

inline unsigned int XrAxisOffsetFromId(XrButtonId id) {
  DCHECK(XrButtonId::kAxisTrackpad <= id &&
         id < XrButtonId::kAxisTrackpad + kMaxNumAxes);
  return static_cast<unsigned int>(id) -
         static_cast<unsigned int>(XrButtonId::kAxisTrackpad);
}
struct ControllerAxisData {
  float x = 0.0f;
  float y = 0.0f;
  unsigned int axis_type = 0;
};

enum ControllerRole {
  kControllerRoleInvalid,  // Test hook should ignore this controller.
  kControllerRoleLeft,
  kControllerRoleRight,
  kControllerRoleVoice  // Simulates voice input such as saying "select" in WMR.
};

struct XRHandJointData {
  mojom::XRHandJoint joint;
  // The transform of pose of this joint in mojo space.
  std::optional<gfx::Transform> mojo_from_joint;
  // The radius of the joint in meters.
  float radius = 0;
};

// Overall struct for all data that may be sent up for a controller per frame.
// Note that some complex types (e.g. vectors), cannot be used, since this data
// ends up being transferred out of the browser process and into a separate DLL.
// This means that we need PODs that do not have complex destructors that would
// cause issues with data being freed in a different heap than where it was
// allocated.
struct COMPONENT_EXPORT(VR_PUBLIC_TEST_TYPEMAPS) ControllerFrameData {
  unsigned int packet_number = 0;
  uint64_t buttons_pressed = 0;
  uint64_t buttons_touched = 0;
  uint64_t supported_buttons = 0;
  std::array<ControllerAxisData, kMaxNumAxes> axis_data;
  std::optional<gfx::Transform> pose_data;
  ControllerRole role = kControllerRoleInvalid;
  std::array<XRHandJointData, kNumJointsForTest> hand_data;
  bool has_hand_data = false;
  bool is_valid = false;

  ControllerFrameData();
  ~ControllerFrameData();
  ControllerFrameData(const ControllerFrameData& other);
  ControllerFrameData& operator=(const ControllerFrameData& other);
  ControllerFrameData& operator=(ControllerFrameData&& other);
};

struct COMPONENT_EXPORT(VR_PUBLIC_TEST_TYPEMAPS) VisibilityMaskData {
  std::array<float, kNumVisibilityMaskVerticesForTest> vertices;
  std::array<uint32_t, kNumVisibilityMaskIndicesForTest> indices;
};

}  // namespace device

#endif  // DEVICE_VR_PUBLIC_MOJOM_TEST_HOOK_TYPES_H_
