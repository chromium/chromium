// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/openvr/test/test_helper.h"
#include "base/debug/debugger.h"
#include "base/logging.h"
#include "base/threading/thread_restrictions.h"
#include "device/vr/openvr/test/test_hook.h"
#include "third_party/openvr/src/headers/openvr.h"
#include "third_party/openvr/src/src/ivrclientcore.h"

#include <D3D11_1.h>
#include <DXGI1_4.h>
#include <wrl.h>
#include <memory>

namespace vr {

void TestHelper::TestFailure() {
  NOTREACHED();
}

void TestHelper::OnPresentedFrame(ID3D11Texture2D* texture,
                                  const VRTextureBounds_t* bounds,
                                  EVREye eye) {
  // Early-out if there is nobody listening.
  bool is_hooked = false;
  lock_.Acquire();
  if (test_hook_) {
    is_hooked = true;
  }
  lock_.Release();
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

  desc.Width = 1;
  desc.Height = buffer_size_pixels;
  desc.MiscFlags = 0;
  desc.BindFlags = 0;
  desc.Usage = D3D11_USAGE_STAGING;
  desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
  HRESULT hr = device->CreateTexture2D(&desc, nullptr, &texture_copy);
  if (FAILED(hr)) {
    TestFailure();
    return;
  }

  D3D11_BOX box = {0, 0, 0, buffer_size_pixels, 1, 1};  // a 1-pixel box
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
  lock_.Acquire();
  if (test_hook_)
    test_hook_->OnFrameSubmitted(frame_data);
  lock_.Release();

  context->Unmap(texture_copy.Get(), 0);
}

namespace {
vr::TrackedDevicePose_t TranslatePose(device::PoseFrameData pose) {
  vr::TrackedDevicePose_t ret = {};

  for (int i = 0; i < 4; ++i) {
    for (int j = 0; j < 3; ++j) {
      ret.mDeviceToAbsoluteTracking.m[j][i] = pose.device_to_origin[j * 4 + i];
    }
  }

  ret.vVelocity = {0, 0, 0};
  ret.vAngularVelocity = {0, 0, 0};
  ret.eTrackingResult = TrackingResult_Running_OK;
  ret.bPoseIsValid = pose.is_valid;
  ret.bDeviceIsConnected = true;

  return ret;
}

}  // namespace

float TestHelper::GetInterpupillaryDistance() {
  lock_.Acquire();
  if (test_hook_) {
    auto config = test_hook_->WaitGetDeviceConfig();
    lock_.Release();
    return config.interpupillary_distance;
  }
  lock_.Release();
  return 0.1f;
}

ProjectionRaw TestHelper::GetProjectionRaw(bool left) {
  lock_.Acquire();
  if (test_hook_) {
    auto config = test_hook_->WaitGetDeviceConfig();
    ProjectionRaw ret = {};
    float* projection = left ? config.viewport_left : config.viewport_right;
    ret.projection[0] = projection[0];
    ret.projection[1] = projection[1];
    ret.projection[2] = projection[2];
    ret.projection[3] = projection[3];
    lock_.Release();
    return ret;
  }
  lock_.Release();
  return {{1, 1, 1, 1}};
}

vr::TrackedDevicePose_t TestHelper::GetPose(bool presenting) {
  lock_.Acquire();
  if (test_hook_) {
    auto ret = TranslatePose(presenting ? test_hook_->WaitGetPresentingPose()
                                        : test_hook_->WaitGetMagicWindowPose());
    lock_.Release();
    return ret;
  }
  lock_.Release();

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
  lock_.Acquire();
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
      prop_value = static_cast<vr::EVRControllerAxisType>(
          controller_data.axis_data[prop - vr::Prop_Axis0Type_Int32].axis_type);
      break;
    }
    default:
      ret = vr::TrackedProp_UnknownProperty;
  }
  lock_.Release();
  return ret;
}

vr::ETrackedPropertyError TestHelper::GetUint64TrackedDeviceProperty(
    unsigned int index,
    ETrackedDeviceProperty prop,
    uint64_t& prop_value) {
  vr::ETrackedPropertyError ret = vr::TrackedProp_Success;
  prop_value = 0;
  lock_.Acquire();
  switch (prop) {
    case vr::Prop_SupportedButtons_Uint64: {
      auto controller_data = test_hook_->WaitGetControllerData(index);
      if (!controller_data.is_valid) {
        ret = vr::TrackedProp_WrongDeviceClass;
        break;
      }
      prop_value = controller_data.supported_buttons;
      break;
    }
    default:
      ret = vr::TrackedProp_UnknownProperty;
  }
  lock_.Release();
  return ret;
}

vr::ETrackedControllerRole TestHelper::GetControllerRoleForTrackedDeviceIndex(
    unsigned int index) {
  vr::ETrackedControllerRole ret = vr::TrackedControllerRole_Invalid;
  lock_.Acquire();
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
  lock_.Release();
  return ret;
}

vr::ETrackedDeviceClass TestHelper::GetTrackedDeviceClass(unsigned int index) {
  vr::ETrackedDeviceClass tracked_class = vr::TrackedDeviceClass_Invalid;
  lock_.Acquire();
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
  lock_.Release();
  return tracked_class;
}

bool TestHelper::GetControllerState(unsigned int index,
                                    vr::VRControllerState_t* controller_state) {
  lock_.Acquire();
  if (test_hook_) {
    auto controller_data = test_hook_->WaitGetControllerData(index);
    lock_.Release();
    controller_state->unPacketNum = controller_data.packet_number;
    controller_state->ulButtonPressed = controller_data.buttons_pressed;
    controller_state->ulButtonTouched = controller_data.buttons_touched;
    for (unsigned int i = 0; i < device::kMaxNumAxes; ++i) {
      controller_state->rAxis[i].x = controller_data.axis_data[i].x;
      controller_state->rAxis[i].y = controller_data.axis_data[i].y;
    }
    return controller_data.is_valid;
  }
  lock_.Release();
  return false;
}

bool TestHelper::GetControllerPose(unsigned int index,
                                   vr::TrackedDevicePose_t* controller_pose) {
  lock_.Acquire();
  if (test_hook_) {
    auto controller_data = test_hook_->WaitGetControllerData(index);
    lock_.Release();
    *controller_pose = TranslatePose(controller_data.pose_data);
    return controller_data.is_valid && controller_data.pose_data.is_valid;
  }
  lock_.Release();
  return false;
}

void TestHelper::SetTestHook(device::OpenVRTestHook* hook) {
  lock_.Acquire();
  test_hook_ = hook;
  lock_.Release();
}

}  // namespace vr