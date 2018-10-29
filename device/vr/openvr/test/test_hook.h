// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_OPENVR_TEST_TEST_HOOK_H_
#define DEVICE_VR_OPENVR_TEST_TEST_HOOK_H_

#include <cstdint>

namespace device {

// Update this string whenever either interface changes.
constexpr char kChromeOpenVRTestHookAPI[] = "ChromeTestHook_2";
constexpr unsigned int kMaxTrackedDevices = 64;
constexpr unsigned int kMaxNumAxes = 5;

struct Color {
  unsigned char r;
  unsigned char g;
  unsigned char b;
  unsigned char a;
};

struct Viewport {
  float left, right, top, bottom;
};

struct SubmittedFrameData {
  Color color;

  bool left_eye;

  Viewport viewport;
  unsigned int image_width;
  unsigned int image_height;

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
  kControllerRoleInvalid,
  kControllerRoleLeft,
  kControllerRoleRight
};

struct ControllerFrameData {
  unsigned int packet_number = 0;
  uint64_t buttons_pressed = 0;
  uint64_t buttons_touched = 0;
  uint64_t supported_buttons = 0;
  ControllerAxisData axis_data[kMaxNumAxes];
  PoseFrameData pose_data = {};
  ControllerRole role = kControllerRoleInvalid;
  bool is_valid = false;
};

// Tests may implement this, and register it to control behavior of OpenVR.
class OpenVRTestHook {
 public:
  virtual void OnFrameSubmitted(SubmittedFrameData frame_data) = 0;
  virtual DeviceConfig WaitGetDeviceConfig() = 0;
  virtual PoseFrameData WaitGetPresentingPose() = 0;
  virtual PoseFrameData WaitGetMagicWindowPose() = 0;
  virtual ControllerRole WaitGetControllerRoleForTrackedDeviceIndex(
      unsigned int index) = 0;
  virtual TrackedDeviceClass WaitGetTrackedDeviceClass(unsigned int index) = 0;
  virtual ControllerFrameData WaitGetControllerData(unsigned int index) = 0;

  virtual void AttachCurrentThread() = 0;
  virtual void DetachCurrentThread() = 0;
};

class TestHookRegistration {
 public:
  virtual void SetTestHook(OpenVRTestHook*) = 0;
};

}  // namespace device

#endif  // DEVICE_VR_OPENVR_TEST_TEST_HOOK_H_