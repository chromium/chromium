// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/openvr/openvr_api_wrapper.h"

#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "device/vr/test/test_hook.h"
#include "base/trace_event/trace_event.h"

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

vr::IVROverlay* OpenVRWrapper::GetOverlay() {
  DCHECK(current_task_runner_->BelongsToCurrentThread());
  return overlay_;
}

vr::VROverlayHandle_t OpenVRWrapper::GetOverlayHandle() {
  DCHECK(current_task_runner_->BelongsToCurrentThread());
  return m_vargglesOverlay;
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
      vr::VR_Init(&init_error, vr::EVRApplicationType::VRApplication_Overlay);
      
  TRACE_EVENT1("gpu", "OpenVR VR_Init 1", "system", (void *)system_);
  TRACE_EVENT1("gpu", "OpenVR VR_Init 2", "init_error", (void *)init_error);

  if (init_error != vr::VRInitError_None) {
    LOG(ERROR) << vr::VR_GetVRInitErrorAsEnglishDescription(init_error);
    any_initialized_ = false;
    return false;
  }

  current_task_runner_ = base::ThreadTaskRunnerHandle::Get();

  if (for_rendering) {
    compositor_ = vr::VRCompositor();
    overlay_ = vr::VROverlay();
    
    
    
    
    
    
    
    
    
    const char* k_pchVargglesOverlayKey = "metachromium.varggles";
    const char* k_pchVargglesOverlayName = "varggles";
    const float k_fOverlayWidthInMeters = 3.f;
    // const vr::ETrackingUniverseOrigin c_eTrackingOrigin = vr::TrackingUniverseStanding;
    const vr::HmdMatrix34_t m_vargglesOverlayTransform{
            {
                    {1, 0, 0, 0},
                    {0, 1, 0, 0},
                    {0, 0, 1, -1}
            }
    };

    // vr::VROverlayHandle_t m_vargglesOverlay = vr::k_ulOverlayHandleInvalid;
    /// glm::mat4 m_vargglesLookRotation;

    // uint64_t m_lastFrameIndex = 0;
    // int m_framesSkipped = 0;
    // int64_t m_updatePosesTimeoutMillis = 10;
    
    
    
    
    
    
    
    
    // create, early out if error
    if (vr::VROverlay()->CreateOverlay(
                    k_pchVargglesOverlayKey,
                    k_pchVargglesOverlayName,
                    &m_vargglesOverlay)
            != vr::VROverlayError_None)
    {
            TRACE_EVENT0("gpu", "ERROR: CreateOverlay failed");
            m_vargglesOverlay = vr::k_ulOverlayHandleInvalid;
            // return;
    }

    if (vr::VROverlay()->SetOverlayFlag(
                    m_vargglesOverlay,
                    vr::VROverlayFlags_Panorama,
                    false)
            != vr::VROverlayError_None)
    {
            TRACE_EVENT0("gpu", "ERROR: StereoPanorama failed");
            // return;
    }

    if (vr::VROverlay()->SetOverlayFlag(
                    m_vargglesOverlay,
                    vr::VROverlayFlags_StereoPanorama,
                    true)
            != vr::VROverlayError_None)
    {
            TRACE_EVENT0("gpu", "ERROR: StereoPanorama failed");
            // return;
    }

    if (vr::VROverlay()->SetOverlayWidthInMeters(m_vargglesOverlay, k_fOverlayWidthInMeters)
            != vr::VROverlayError_None)
    {
            TRACE_EVENT0("gpu", "ERROR: SetOverlayWidth failed");
            // return;
    }

    if (vr::VROverlay()->SetOverlayTransformTrackedDeviceRelative(
                    m_vargglesOverlay,
                    vr::k_unTrackedDeviceIndex_Hmd,
                    &m_vargglesOverlayTransform)
            != vr::VROverlayError_None)
    {
            TRACE_EVENT0("gpu", "ERROR: SetOverlayTransform failed");
            // return;
    }
  }
  
  TRACE_EVENT0("gpu", "OpenVR VR_Init 3");

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
  
  TRACE_EVENT0("gpu", "OpenVR VR_Init 4");

  return true;
}

void OpenVRWrapper::Uninitialize() {
  TRACE_EVENT0("gpu", "OpenVR VR_Shutdown 1");
  DCHECK(initialized_);
  initialized_ = false;
  system_ = nullptr;
  compositor_ = nullptr;
  service_test_hook_ = nullptr;
  current_task_runner_ = nullptr;
  if (test_hook_)
    test_hook_->DetachCurrentThread();
  vr::VR_Shutdown();

  TRACE_EVENT0("gpu", "OpenVR VR_Shutdown 2");

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
