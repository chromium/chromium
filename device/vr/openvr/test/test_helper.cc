// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/openvr/test/test_helper.h"

#include <map>
#include <memory>

#include "base/debug/debugger.h"
#include "base/logging.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_restrictions.h"
#include "device/vr/test/test_hook.h"
#include "third_party/openvr/src/headers/openvr.h"
#include "third_party/openvr/src/src/ivrclientcore.h"

#include <D3D11_1.h>
#include <DXGI1_4.h>
#include <wrl.h>

namespace vr {

void TestHelper::TestFailure() {
  NOTREACHED();
}

void TestHelper::OnPresentedFrame(ID3D11Texture2D* texture,
                                  const VRTextureBounds_t* bounds,
                                  EVREye eye) {
  // Early-out if there is nobody listening.
  bool is_hooked = false;
  {
    base::AutoLock auto_lock(lock_);
    if (test_hook_) {
      is_hooked = true;
    }
  }

  if (!is_hooked)
    return;

  device::SubmittedFrameData frame_data = {};
  frame_data.left_eye = (eye == Eye_Left);
  frame_data.viewport = {bounds->uMin, bounds->uMax, bounds->vMin,
                         bounds->vMax};

  Microsoft::WRL::ComPtr<ID3D11Device> device;
  texture->GetDevice(&device);

  Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
  device->GetImmediateContext(&context);

  // We copy the submitted texture to a new texture, so we can map it, and
  // read back pixel data.
  Microsoft::WRL::ComPtr<ID3D11Texture2D> texture_copy;
  D3D11_TEXTURE2D_DESC desc;
  texture->GetDesc(&desc);

  frame_data.image_width = desc.Width;
  frame_data.image_height = desc.Height;

  size_t buffer_size = sizeof(device::SubmittedFrameData::raw_buffer);
  size_t buffer_size_pixels = buffer_size / sizeof(device::Color);

  desc.Width = buffer_size_pixels;
  desc.Height = 1;
  desc.MiscFlags = 0;
  desc.BindFlags = 0;
  desc.Usage = D3D11_USAGE_STAGING;
  desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
  HRESULT hr = device->CreateTexture2D(&desc, nullptr, &texture_copy);
  if (FAILED(hr)) {
    TestFailure();
    return;
  }

  // A strip of pixels along the top of the texture, however many will fit into
  // our buffer.
  D3D11_BOX box = {0, 0, 0, buffer_size_pixels, 1, 1};
  context->CopySubresourceRegion(texture_copy.Get(), 0, 0, 0, 0, texture, 0,
                                 &box);

  D3D11_MAPPED_SUBRESOURCE map_data = {};
  hr = context->Map(texture_copy.Get(), 0, D3D11_MAP_READ, 0, &map_data);
  if (FAILED(hr)) {
    TestFailure();
    return;
  }

  // We have a 1-pixel image.  Give it to the test hook.
  device::Color* color = reinterpret_cast<device::Color*>(map_data.pData);
  frame_data.color = color[0];
  memcpy(&frame_data.raw_buffer, map_data.pData, buffer_size);
  {
    base::AutoLock auto_lock(lock_);
    if (test_hook_)
      test_hook_->OnFrameSubmitted(frame_data);
  }

  context->Unmap(texture_copy.Get(), 0);
}

namespace {
vr::TrackedDevicePose_t TranslatePose(device::PoseFrameData pose) {
  vr::TrackedDevicePose_t ret = {};

  // We're given the pose in column-major order, with the translation component
  // in the 4th column. OpenVR uses a 3x4 matrix instead of a 4x4, so copying
  // as-is will chop off the translation. So, transpose while copying.
  for (int col = 0; col < 4; ++col) {
    for (int row = 0; row < 3; ++row) {
      ret.mDeviceToAbsoluteTracking.m[row][col] =
          pose.device_to_origin[col * 4 + row];
    }
  }

  ret.vVelocity = {0, 0, 0};
  ret.vAngularVelocity = {0, 0, 0};
  ret.eTrackingResult = TrackingResult_Running_OK;
  ret.bPoseIsValid = pose.is_valid;
  ret.bDeviceIsConnected = true;

  return ret;
}

static const std::map<device::XrButtonId, vr::EVRButtonId>
    xr_to_openvr_button_map = {
        {device::XrButtonId::kSystem, vr::EVRButtonId::k_EButton_System},
        {device::XrButtonId::kMenu, vr::EVRButtonId::k_EButton_ApplicationMenu},
        {device::XrButtonId::kGrip, vr::EVRButtonId::k_EButton_Grip},
        {device::XrButtonId::kDpadLeft, vr::EVRButtonId::k_EButton_DPad_Left},
        {device::XrButtonId::kDpadUp, vr::EVRButtonId::k_EButton_DPad_Up},
        {device::XrButtonId::kDpadRight, vr::EVRButtonId::k_EButton_DPad_Right},
        {device::XrButtonId::kDpadDown, vr::EVRButtonId::k_EButton_DPad_Down},
        {device::XrButtonId::kA, vr::EVRButtonId::k_EButton_A},
        {device::XrButtonId::kProximitySensor,
         vr::EVRButtonId::k_EButton_ProximitySensor},
        {device::XrButtonId::kAxisTrackpad, vr::EVRButtonId::k_EButton_Axis0},
        {device::XrButtonId::kAxisTrigger,
         vr::EVRButtonId::k_EButton_SteamVR_Trigger},
        {device::XrButtonId::kAxisThumbstick, vr::EVRButtonId::k_EButton_Axis2},
        {device::XrButtonId::kAxisTertiary, vr::EVRButtonId::k_EButton_Axis3},
        {device::XrButtonId::kAxisQuaternary, vr::EVRButtonId::k_EButton_Axis4},
};

// Translates the platform-agnostic button masks to the OpenVR-specific button
// masks. The platform-agnostic ones were based off OpenVR, so this actually
// does little to nothing.
uint64_t TranslateButtonMask(uint64_t xr_mask) {
  uint64_t ret = 0;
  for (const auto& pair : xr_to_openvr_button_map) {
    // Bitwise-and the complete mask with the button-specific mask, shift it all
    // the way to the right, then shift it to the left however much it needs to
    // be in the correct location for OpenVR. Then, add that new button-specific
    // mask to the complete mask that we'll be returning.
    ret |= ((xr_mask & device::XrButtonMaskFromId(pair.first)) >> pair.first)
           << pair.second;
  }
  return ret;
}

// Translates the platform-agnostic axis types to the OpenVR-specific axis
// types. The platform-agnostic ones were based off OpenVR, so this actually
// does little to nothing.
vr::EVRControllerAxisType TranslateAxisType(device::XrAxisType type) {
  switch (type) {
    case device::XrAxisType::kNone:
      return vr::EVRControllerAxisType::k_eControllerAxis_None;
    case device::XrAxisType::kTrackpad:
      return vr::EVRControllerAxisType::k_eControllerAxis_TrackPad;
    case device::XrAxisType::kJoystick:
      return vr::EVRControllerAxisType::k_eControllerAxis_Joystick;
    case device::XrAxisType::kTrigger:
      return vr::EVRControllerAxisType::k_eControllerAxis_Trigger;
  }
}

vr::EVRControllerAxisType TranslateAxisType(unsigned int type) {
  return TranslateAxisType(static_cast<device::XrAxisType>(type));
}

}  // namespace

float TestHelper::GetInterpupillaryDistance() {
  base::AutoLock auto_lock(lock_);
  if (test_hook_) {
    auto config = test_hook_->WaitGetDeviceConfig();
    return config.interpupillary_distance;
  }
  return 0.1f;
}

ProjectionRaw TestHelper::GetProjectionRaw(bool left) {
  base::AutoLock auto_lock(lock_);
  if (test_hook_) {
    auto config = test_hook_->WaitGetDeviceConfig();
    ProjectionRaw ret = {};
    float* projection = left ? config.viewport_left : config.viewport_right;
    ret.projection[0] = projection[0];
    ret.projection[1] = projection[1];
    ret.projection[2] = projection[2];
    ret.projection[3] = projection[3];
    return ret;
  }
  return {{1, 1, 1, 1}};
}

vr::TrackedDevicePose_t TestHelper::GetPose(bool presenting) {
  base::AutoLock auto_lock(lock_);
  if (test_hook_) {
    auto ret = TranslatePose(presenting ? test_hook_->WaitGetPresentingPose()
                                        : test_hook_->WaitGetMagicWindowPose());
    return ret;
  }

  device::PoseFrameData pose = {};
  pose.is_valid = true;
  pose.device_to_origin[0] = 1;
  pose.device_to_origin[5] = 1;
  pose.device_to_origin[10] = 1;
  return TranslatePose(pose);
}

vr::ETrackedPropertyError TestHelper::GetInt32TrackedDeviceProperty(
    unsigned int index,
    ETrackedDeviceProperty prop,
    int32_t& prop_value) {
  vr::ETrackedPropertyError ret = vr::TrackedProp_Success;
  prop_value = 0;
  base::AutoLock auto_lock(lock_);
  switch (prop) {
    case vr::Prop_Axis0Type_Int32:
    case vr::Prop_Axis1Type_Int32:
    case vr::Prop_Axis2Type_Int32:
    case vr::Prop_Axis3Type_Int32:
    case vr::Prop_Axis4Type_Int32: {
      auto controller_data = test_hook_->WaitGetControllerData(index);
      if (!controller_data.is_valid) {
        ret = vr::TrackedProp_WrongDeviceClass;
        break;
      }
      prop_value = TranslateAxisType(
          controller_data.axis_data[prop - vr::Prop_Axis0Type_Int32].axis_type);
      break;
    }
    default:
      ret = vr::TrackedProp_UnknownProperty;
  }
  return ret;
}

vr::ETrackedPropertyError TestHelper::GetUint64TrackedDeviceProperty(
    unsigned int index,
    ETrackedDeviceProperty prop,
    uint64_t& prop_value) {
  vr::ETrackedPropertyError ret = vr::TrackedProp_Success;
  prop_value = 0;
  base::AutoLock auto_lock(lock_);
  switch (prop) {
    case vr::Prop_SupportedButtons_Uint64: {
      auto controller_data = test_hook_->WaitGetControllerData(index);
      if (!controller_data.is_valid) {
        ret = vr::TrackedProp_WrongDeviceClass;
        break;
      }
      prop_value = TranslateButtonMask(controller_data.supported_buttons);
      break;
    }
    default:
      ret = vr::TrackedProp_UnknownProperty;
  }
  return ret;
}

vr::ETrackedControllerRole TestHelper::GetControllerRoleForTrackedDeviceIndex(
    unsigned int index) {
  vr::ETrackedControllerRole ret = vr::TrackedControllerRole_Invalid;
  base::AutoLock auto_lock(lock_);
  if (test_hook_) {
    switch (test_hook_->WaitGetControllerRoleForTrackedDeviceIndex(index)) {
      case device::kControllerRoleInvalid:
        break;
      case device::kControllerRoleLeft:
        ret = vr::TrackedControllerRole_LeftHand;
        break;
      case device::kControllerRoleRight:
        ret = vr::TrackedControllerRole_RightHand;
        break;
      default:
        NOTREACHED();
    }
  }
  return ret;
}

vr::ETrackedDeviceClass TestHelper::GetTrackedDeviceClass(unsigned int index) {
  vr::ETrackedDeviceClass tracked_class = vr::TrackedDeviceClass_Invalid;
  base::AutoLock auto_lock(lock_);
  if (test_hook_) {
    switch (test_hook_->WaitGetTrackedDeviceClass(index)) {
      case device::kTrackedDeviceInvalid:
        break;
      case device::kTrackedDeviceHmd:
        tracked_class = vr::TrackedDeviceClass_HMD;
        break;
      case device::kTrackedDeviceController:
        tracked_class = vr::TrackedDeviceClass_Controller;
        break;
      case device::kTrackedDeviceGenericTracker:
        tracked_class = vr::TrackedDeviceClass_GenericTracker;
        break;
      case device::kTrackedDeviceTrackingReference:
        tracked_class = vr::TrackedDeviceClass_TrackingReference;
        break;
      default:
        NOTREACHED();
    }
  }
  return tracked_class;
}

bool TestHelper::GetControllerState(unsigned int index,
                                    vr::VRControllerState_t* controller_state) {
  base::AutoLock auto_lock(lock_);
  if (test_hook_) {
    auto controller_data = test_hook_->WaitGetControllerData(index);
    controller_state->unPacketNum = controller_data.packet_number;
    controller_state->ulButtonPressed =
        TranslateButtonMask(controller_data.buttons_pressed);
    controller_state->ulButtonTouched =
        TranslateButtonMask(controller_data.buttons_touched);
    for (unsigned int i = 0; i < device::kMaxNumAxes; ++i) {
      // Invert the y axis because -1 is up in the Gamepad API, but down in WMR.
      controller_state->rAxis[i].x = controller_data.axis_data[i].x;
      controller_state->rAxis[i].y = -controller_data.axis_data[i].y;
    }
    return controller_data.is_valid;
  }
  return false;
}

bool TestHelper::GetControllerPose(unsigned int index,
                                   vr::TrackedDevicePose_t* controller_pose) {
  base::AutoLock auto_lock(lock_);
  if (test_hook_) {
    auto controller_data = test_hook_->WaitGetControllerData(index);
    *controller_pose = TranslatePose(controller_data.pose_data);
    return controller_data.is_valid && controller_data.pose_data.is_valid;
  }
  return false;
}

void TestHelper::SetTestHook(device::VRTestHook* hook) {
  base::AutoLock auto_lock(lock_);
  test_hook_ = hook;
}

}  // namespace vr