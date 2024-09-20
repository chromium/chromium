// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#ifndef DEVICE_VR_TEST_TEST_HOOK_H_
#define DEVICE_VR_TEST_TEST_HOOK_H_

#include <cstdint>

#include "base/check.h"
#include "base/component_export.h"
#include "device/vr/public/mojom/browser_test_interfaces.mojom.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/transform.h"

namespace device {

constexpr unsigned int kMaxTrackedDevices = 64;
constexpr unsigned int kMaxNumAxes = 5;
constexpr unsigned int kNumJointsForTest =
    static_cast<unsigned int>(device::mojom::XRHandJoint::kMaxValue) + 1;

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

enum class XrEye {
  kLeft = 0,
  kRight = 1,
  kNone = 2,
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

struct Color {
  unsigned char r;
  unsigned char g;
  unsigned char b;
  unsigned char a;
};

struct ViewData {
  Color color;
  XrEye eye;
  gfx::Rect viewport;
  char raw_buffer[256];  // Can encode raw data here.
};

struct PoseFrameData {
  float device_to_origin[16];
  bool is_valid;
};

struct DeviceConfig {
  float interpupillary_distance;
  float viewport_left[4];   // raw projection left {left, right, top, bottom}
  float viewport_right[4];  // raw projection right {left, right, top, bottom}
};

struct ControllerAxisData {
  float x = 0.0f;
  float y = 0.0f;
  unsigned int axis_type = 0;
};

enum TrackedDeviceClass {
  kTrackedDeviceInvalid,
  kTrackedDeviceHmd,
  kTrackedDeviceController,
  kTrackedDeviceGenericTracker,
  kTrackedDeviceTrackingReference,
  kTrackedDeviceDisplayRedirect
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
// Note that some complex times (e.g. vectors), cannot be used, since this data
// ends up being transferred out of the browser process and into a separate DLL.
// This means that we need PODs that do not have complex destructors that would
// cause issues with data being freed in a different heap than where it was
// allocated.
struct COMPONENT_EXPORT(VR_TEST_HOOK) ControllerFrameData {
  unsigned int packet_number = 0;
  uint64_t buttons_pressed = 0;
  uint64_t buttons_touched = 0;
  uint64_t supported_buttons = 0;
  ControllerAxisData axis_data[kMaxNumAxes];
  PoseFrameData pose_data = {};
  ControllerRole role = kControllerRoleInvalid;
  XRHandJointData hand_data[kNumJointsForTest];
  bool has_hand_data = false;
  bool is_valid = false;

  ControllerFrameData();
  ~ControllerFrameData();
  ControllerFrameData(const ControllerFrameData& other);
  ControllerFrameData& operator=(const ControllerFrameData& other);
  ControllerFrameData& operator=(ControllerFrameData&& other);
};

inline gfx::Transform PoseFrameDataToTransform(PoseFrameData data) {
  // The gfx::Transform constructor takes arguments in row-major order, but
  // we're given data in column-major order. Construct in column-major order and
  // transpose since it looks cleaner than manually transposing the arguments
  // passed to the constructor.
  float* t = data.device_to_origin;
  return gfx::Transform::ColMajor(t[0], t[1], t[2], t[3], t[4], t[5], t[6],
                                  t[7], t[8], t[9], t[10], t[11], t[12], t[13],
                                  t[14], t[15]);
}

// Tests may implement this, and register it to control behavior of VR runtime.
class VRTestHook {
 public:
  virtual void OnFrameSubmitted(const std::vector<ViewData>& frame_data) = 0;
  virtual DeviceConfig WaitGetDeviceConfig() = 0;
  virtual PoseFrameData WaitGetPresentingPose() = 0;
  virtual PoseFrameData WaitGetMagicWindowPose() = 0;
  virtual ControllerRole WaitGetControllerRoleForTrackedDeviceIndex(
      unsigned int index) = 0;
  virtual TrackedDeviceClass WaitGetTrackedDeviceClass(unsigned int index) = 0;
  virtual ControllerFrameData WaitGetControllerData(unsigned int index) = 0;
  virtual device_test::mojom::EventData WaitGetEventData() = 0;
  virtual bool WaitGetCanCreateSession() = 0;

  virtual void AttachCurrentThread() = 0;
  virtual void DetachCurrentThread() = 0;
};

class ServiceTestHook {
 public:
  virtual void SetTestHook(VRTestHook*) = 0;
};

}  // namespace device

#endif  // DEVICE_VR_TEST_TEST_HOOK_H_
