// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/openvr/openvr_api_wrapper.h"

#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "device/vr/test/test_hook.h"

namespace device {

OpenVRWrapper::OpenVRWrapper(bool for_rendering) {
  initialized_ = Initialize(for_rendering);
}

OpenVRWrapper::~OpenVRWrapper() {
  if (initialized_)
    Uninitialize();
}

vr::IVRCompositor* OpenVRWrapper::GetCompositor() {
  DCHECK(current_task_runner_->BelongsToCurrentThread());
  return compositor_;
}

vr::IVRSystem* OpenVRWrapper::GetSystem() {
  DCHECK(current_task_runner_->BelongsToCurrentThread());
  return system_;
}

void OpenVRWrapper::SetTestHook(VRTestHook* hook) {
  // This may be called from any thread - tests are responsible for
  // maintaining thread safety, typically by not changing the test hook
  // while presenting.
  test_hook_ = hook;
  if (service_test_hook_) {
    service_test_hook_->SetTestHook(test_hook_);
  }
}

bool OpenVRWrapper::Initialize(bool for_rendering) {
  DCHECK(!any_initialized_);
  any_initialized_ = true;

  // device can only be used on this thread once initailized
  vr::EVRInitError init_error = vr::VRInitError_None;
  system_ =
      vr::VR_Init(&init_error, vr::EVRApplicationType::VRApplication_Scene);

  if (init_error != vr::VRInitError_None) {
    LOG(ERROR) << vr::VR_GetVRInitErrorAsEnglishDescription(init_error);
    any_initialized_ = false;
    return false;
  }

  current_task_runner_ = base::ThreadTaskRunnerHandle::Get();

  if (for_rendering) {
    compositor_ = vr::VRCompositor();
  }

  if (test_hook_) {
    // Allow our mock implementation of OpenVR to be controlled by tests.
    // Note that SetTestHook must be called before CreateDevice, or
    // service_test_hook_s will remain null.  This is a good pattern for
    // tests anyway, since the alternative is we start mocking part-way through
    // using the device, and end up with race conditions for when we started
    // controlling things.
    vr::EVRInitError eError;
    service_test_hook_ = static_cast<ServiceTestHook*>(
        vr::VR_GetGenericInterface(kChromeOpenVRTestHookAPI, &eError));
    if (service_test_hook_) {
      service_test_hook_->SetTestHook(test_hook_);
      test_hook_->AttachCurrentThread();
    }
  }

  return true;
}

void OpenVRWrapper::Uninitialize() {
  DCHECK(initialized_);
  initialized_ = false;
  system_ = nullptr;
  compositor_ = nullptr;
  service_test_hook_ = nullptr;
  current_task_runner_ = nullptr;
  if (test_hook_)
    test_hook_->DetachCurrentThread();
  vr::VR_Shutdown();

  any_initialized_ = false;
}

VRTestHook* OpenVRWrapper::test_hook_ = nullptr;
bool OpenVRWrapper::any_initialized_ = false;
ServiceTestHook* OpenVRWrapper::service_test_hook_ = nullptr;

std::string GetOpenVRString(vr::IVRSystem* vr_system,
                            vr::TrackedDeviceProperty prop,
                            uint32_t device_index) {
  std::string out;

  vr::TrackedPropertyError error = vr::TrackedProp_Success;
  char openvr_string[vr::k_unMaxPropertyStringSize];
  vr_system->GetStringTrackedDeviceProperty(
      device_index, prop, openvr_string, vr::k_unMaxPropertyStringSize, &error);

  if (error == vr::TrackedProp_Success)
    out = openvr_string;

  return out;
}

}  // namespace device
