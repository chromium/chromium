// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <D3D11_1.h>
#include <DXGI1_4.h>
#include <memory>

#include "device/vr/openvr/test/test_helper.h"
#include "device/vr/test/test_hook.h"
#include "device/vr/windows/d3d11_device_helpers.h"
#include "third_party/openvr/src/headers/openvr.h"
#include "third_party/openvr/src/src/ivrclientcore.h"

namespace vr {

class TestVRSystem : public IVRSystem {
 public:
  void GetRecommendedRenderTargetSize(uint32_t* width,
                                      uint32_t* height) override;
  HmdMatrix44_t GetProjectionMatrix(EVREye eye,
                                    float near_z,
                                    float far_z) override {
    NOTIMPLEMENTED();
    return {};
  }
  void GetProjectionRaw(EVREye eye,
                        float* left,
                        float* right,
                        float* top,
                        float* bottom) override;
  bool ComputeDistortion(
      EVREye eye,
      float u,
      float v,
      DistortionCoordinates_t* distortion_coordinates) override {
    NOTIMPLEMENTED();
    return false;
  }
  HmdMatrix34_t GetEyeToHeadTransform(EVREye eye) override;
  bool GetTimeSinceLastVsync(float* seconds_since_last_vsync,
                             uint64_t* frame_counter) override {
    NOTIMPLEMENTED();
    return false;
  }
  int32_t GetD3D9AdapterIndex() override {
    NOTIMPLEMENTED();
    return 0;
  }
  void GetDXGIOutputInfo(int32_t* adapter_index) override;
  bool IsDisplayOnDesktop() override {
    NOTIMPLEMENTED();
    return false;
  }
  bool SetDisplayVisibility(bool is_visible_on_desktop) override {
    NOTIMPLEMENTED();
    return false;
  }
  void GetDeviceToAbsoluteTrackingPose(
      ETrackingUniverseOrigin origin,
      float predicted_seconds_to_photons_from_now,
      VR_ARRAY_COUNT(tracked_device_pose_array_count)
          TrackedDevicePose_t* tracked_device_pose_array,
      uint32_t tracked_device_pose_array_count) override;
  void ResetSeatedZeroPose() override { NOTIMPLEMENTED(); }
  HmdMatrix34_t GetSeatedZeroPoseToStandingAbsoluteTrackingPose() override;
  HmdMatrix34_t GetRawZeroPoseToStandingAbsoluteTrackingPose() override {
    NOTIMPLEMENTED();
    return {};
  }
  uint32_t GetSortedTrackedDeviceIndicesOfClass(
      ETrackedDeviceClass tracked_device_class,
      VR_ARRAY_COUNT(tracked_device_index_array_count)
          TrackedDeviceIndex_t* tracked_device_index_array,
      uint32_t tracked_device_index_array_count,
      TrackedDeviceIndex_t unRelativeToTrackedDeviceIndex =
          k_unTrackedDeviceIndex_Hmd) override {
    NOTIMPLEMENTED();
    return 0;
  }
  EDeviceActivityLevel GetTrackedDeviceActivityLevel(
      TrackedDeviceIndex_t device_id) override {
    NOTIMPLEMENTED();
    return k_EDeviceActivityLevel_Unknown;
  }
  void ApplyTransform(TrackedDevicePose_t* output_pose,
                      const TrackedDevicePose_t* tracked_device_pose,
                      const HmdMatrix34_t* transform) override {
    NOTIMPLEMENTED();
  }
  TrackedDeviceIndex_t GetTrackedDeviceIndexForControllerRole(
      ETrackedControllerRole device_type) override {
    NOTIMPLEMENTED();
    return 0;
  }
  ETrackedControllerRole GetControllerRoleForTrackedDeviceIndex(
      TrackedDeviceIndex_t device_index) override;
  ETrackedDeviceClass GetTrackedDeviceClass(
      TrackedDeviceIndex_t device_index) override;
  bool IsTrackedDeviceConnected(TrackedDeviceIndex_t device_index) override {
    NOTIMPLEMENTED();
    return false;
  }
  bool GetBoolTrackedDeviceProperty(
      TrackedDeviceIndex_t device_index,
      ETrackedDeviceProperty prop,
      ETrackedPropertyError* error = 0L) override {
    NOTIMPLEMENTED();
    return false;
  }
  float GetFloatTrackedDeviceProperty(
      TrackedDeviceIndex_t device_index,
      ETrackedDeviceProperty prop,
      ETrackedPropertyError* error = 0L) override;
  int32_t GetInt32TrackedDeviceProperty(
      TrackedDeviceIndex_t device_index,
      ETrackedDeviceProperty prop,
      ETrackedPropertyError* error = 0L) override;
  uint64_t GetUint64TrackedDeviceProperty(
      TrackedDeviceIndex_t device_index,
      ETrackedDeviceProperty prop,
      ETrackedPropertyError* error = 0L) override;
  HmdMatrix34_t GetMatrix34TrackedDeviceProperty(
      TrackedDeviceIndex_t device_index,
      ETrackedDeviceProperty prop,
      ETrackedPropertyError* error = 0L) override {
    NOTIMPLEMENTED();
    return {};
  }
  uint32_t GetStringTrackedDeviceProperty(
      TrackedDeviceIndex_t device_index,
      ETrackedDeviceProperty prop,
      VR_OUT_STRING() char* value,
      uint32_t buffer_size,
      ETrackedPropertyError* error = 0L) override;
  const char* GetPropErrorNameFromEnum(ETrackedPropertyError error) override {
    NOTIMPLEMENTED();
    return nullptr;
  }
  bool PollNextEvent(VREvent_t* event, uint32_t vr_event) override;
  bool PollNextEventWithPose(
      ETrackingUniverseOrigin origin,
      VREvent_t* event,
      uint32_t vr_event,
      TrackedDevicePose_t* tracked_device_pose) override {
    NOTIMPLEMENTED();
    return false;
  }
  const char* GetEventTypeNameFromEnum(EVREventType type) override {
    NOTIMPLEMENTED();
    return nullptr;
  }
  HiddenAreaMesh_t GetHiddenAreaMesh(
      EVREye eye,
      EHiddenAreaMeshType type = k_eHiddenAreaMesh_Standard) override {
    NOTIMPLEMENTED();
    return {};
  }
  bool GetControllerState(TrackedDeviceIndex_t controller_device_index,
                          VRControllerState_t* controller_state,
                          uint32_t controller_state_size) override;
  bool GetControllerStateWithPose(
      ETrackingUniverseOrigin origin,
      TrackedDeviceIndex_t device_controller_index,
      VRControllerState_t* controller_state,
      uint32_t controller_state_size,
      TrackedDevicePose_t* tracked_device_pose) override;
  void TriggerHapticPulse(TrackedDeviceIndex_t device_index,
                          uint32_t axis_id,
                          unsigned short duration_micro_sec) override {
    NOTIMPLEMENTED();
  }
  const char* GetButtonIdNameFromEnum(EVRButtonId button_id) override {
    NOTIMPLEMENTED();
    return nullptr;
  }
  const char* GetControllerAxisTypeNameFromEnum(
      EVRControllerAxisType axis_type) override {
    NOTIMPLEMENTED();
    return nullptr;
  }
  bool CaptureInputFocus() override {
    NOTIMPLEMENTED();
    return false;
  }
  void ReleaseInputFocus() override { NOTIMPLEMENTED(); }
  bool IsInputFocusCapturedByAnotherProcess() override {
    NOTIMPLEMENTED();
    return false;
  }
  uint32_t DriverDebugRequest(TrackedDeviceIndex_t device_index,
                              const char* request,
                              char* response_buffer,
                              uint32_t response_buffer_size) override {
    NOTIMPLEMENTED();
    return 0;
  }
  EVRFirmwareError PerformFirmwareUpdate(
      TrackedDeviceIndex_t device_index) override {
    NOTIMPLEMENTED();
    return VRFirmwareError_None;
  }
  void AcknowledgeQuit_Exiting() override { NOTIMPLEMENTED(); }
  void AcknowledgeQuit_UserPrompt() override { NOTIMPLEMENTED(); }
};

class TestVRCompositor : public IVRCompositor {
 public:
  void SetTrackingSpace(ETrackingUniverseOrigin origin) override;
  ETrackingUniverseOrigin GetTrackingSpace() override {
    NOTIMPLEMENTED();
    return TrackingUniverseSeated;
  }
  EVRCompositorError WaitGetPoses(VR_ARRAY_COUNT(render_pose_array_count)
                                      TrackedDevicePose_t* render_pose_array,
                                  uint32_t render_pose_array_count,
                                  VR_ARRAY_COUNT(game_pose_array_count)
                                      TrackedDevicePose_t* game_pose_array,
                                  uint32_t game_pose_array_count) override;
  EVRCompositorError GetLastPoses(VR_ARRAY_COUNT(render_pose_array_count)
                                      TrackedDevicePose_t* render_pose_array,
                                  uint32_t render_pose_array_count,
                                  VR_ARRAY_COUNT(game_pose_array_count)
                                      TrackedDevicePose_t* game_pose_array,
                                  uint32_t game_pose_array_count) override {
    NOTIMPLEMENTED();
    return VRCompositorError_None;
  }
  EVRCompositorError GetLastPoseForTrackedDeviceIndex(
      TrackedDeviceIndex_t device_index,
      TrackedDevicePose_t* output_pose,
      TrackedDevicePose_t* output_game_pose) override {
    NOTIMPLEMENTED();
    return VRCompositorError_None;
  }
  EVRCompositorError Submit(
      EVREye eye,
      const Texture_t* texture,
      const VRTextureBounds_t* bounds = 0,
      EVRSubmitFlags submit_flags = Submit_Default) override;
  void ClearLastSubmittedFrame() override { NOTIMPLEMENTED(); }
  void PostPresentHandoff() override;
  bool GetFrameTiming(Compositor_FrameTiming* timing,
                      uint32_t frames_ago = 0) override;
  uint32_t GetFrameTimings(Compositor_FrameTiming* timing,
                           uint32_t frames) override {
    NOTIMPLEMENTED();
    return 0;
  }
  float GetFrameTimeRemaining() override {
    NOTIMPLEMENTED();
    return 0;
  }
  void GetCumulativeStats(Compositor_CumulativeStats* stats,
                          uint32_t stats_size_in_bytes) override {
    NOTIMPLEMENTED();
  }
  void FadeToColor(float seconds,
                   float red,
                   float green,
                   float blue,
                   float alpha,
                   bool background = false) override {
    NOTIMPLEMENTED();
  }
  HmdColor_t GetCurrentFadeColor(bool background = false) override {
    NOTIMPLEMENTED();
    return {};
  }
  void FadeGrid(float seconds, bool fade_in) override { NOTIMPLEMENTED(); }
  float GetCurrentGridAlpha() override {
    NOTIMPLEMENTED();
    return 0;
  }
  EVRCompositorError SetSkyboxOverride(VR_ARRAY_COUNT(texture_count)
                                           const Texture_t* textures,
                                       uint32_t texture_count) override {
    NOTIMPLEMENTED();
    return VRCompositorError_None;
  }
  void ClearSkyboxOverride() override { NOTIMPLEMENTED(); }
  void CompositorBringToFront() override { NOTIMPLEMENTED(); }
  void CompositorGoToBack() override { NOTIMPLEMENTED(); }
  void CompositorQuit() override { NOTIMPLEMENTED(); }
  bool IsFullscreen() override {
    NOTIMPLEMENTED();
    return false;
  }
  uint32_t GetCurrentSceneFocusProcess() override {
    NOTIMPLEMENTED();
    return 0;
  }
  uint32_t GetLastFrameRenderer() override {
    NOTIMPLEMENTED();
    return 0;
  }
  bool CanRenderScene() override {
    NOTIMPLEMENTED();
    return false;
  }
  void ShowMirrorWindow() override { NOTIMPLEMENTED(); }
  void HideMirrorWindow() override { NOTIMPLEMENTED(); }
  bool IsMirrorWindowVisible() override {
    NOTIMPLEMENTED();
    return false;
  }
  void CompositorDumpImages() override { NOTIMPLEMENTED(); }
  bool ShouldAppRenderWithLowResources() override {
    NOTIMPLEMENTED();
    return false;
  }
  void ForceInterleavedReprojectionOn(bool override) override {
    NOTIMPLEMENTED();
  }
  void ForceReconnectProcess() override { NOTIMPLEMENTED(); }
  void SuspendRendering(bool suspend) override;
  EVRCompositorError GetMirrorTextureD3D11(
      EVREye eye,
      void* d3d11_device_or_resource,
      void** d3d11_shader_resource_view) override {
    NOTIMPLEMENTED();
    return VRCompositorError_None;
  }
  void ReleaseMirrorTextureD3D11(void* d3d11_shader_resource_view) override {
    NOTIMPLEMENTED();
  }
  EVRCompositorError GetMirrorTextureGL(
      EVREye eye,
      glUInt_t* texture_id,
      glSharedTextureHandle_t* shared_texture_handle) override {
    NOTIMPLEMENTED();
    return VRCompositorError_None;
  }
  bool ReleaseSharedGLTexture(
      glUInt_t texture_id,
      glSharedTextureHandle_t shared_texture_handle) override {
    NOTIMPLEMENTED();
    return false;
  }
  void LockGLSharedTextureForAccess(
      glSharedTextureHandle_t shared_texture_handle) override {
    NOTIMPLEMENTED();
  }
  void UnlockGLSharedTextureForAccess(
      glSharedTextureHandle_t shared_texture_handle) override {
    NOTIMPLEMENTED();
  }
  uint32_t GetVulkanInstanceExtensionsRequired(VR_OUT_STRING() char* value,
                                               uint32_t buffer_size) override {
    NOTIMPLEMENTED();
    return 0;
  }
  uint32_t GetVulkanDeviceExtensionsRequired(
      VkPhysicalDevice_T* physical_device,
      VR_OUT_STRING() char* value,
      uint32_t buffer_size) override {
    NOTIMPLEMENTED();
    return 0;
  }
};

class TestVRClientCore : public IVRClientCore {
 public:
  EVRInitError Init(EVRApplicationType application_type) override;
  void Cleanup() override;
  EVRInitError IsInterfaceVersionValid(const char* interface_version) override;
  void* GetGenericInterface(const char* name_and_version,
                            EVRInitError* error) override;
  bool BIsHmdPresent() override;
  const char* GetEnglishStringForHmdError(EVRInitError error) override {
    NOTIMPLEMENTED();
    return nullptr;
  }
  const char* GetIDForVRInitError(EVRInitError error) override {
    NOTIMPLEMENTED();
    return nullptr;
  }
};

TestHelper g_test_helper;
TestVRSystem g_system;
TestVRCompositor g_compositor;
TestVRClientCore g_loader;

EVRInitError TestVRClientCore::Init(EVRApplicationType application_type) {
  return VRInitError_None;
}

void TestVRClientCore::Cleanup() {
}

EVRInitError TestVRClientCore::IsInterfaceVersionValid(
    const char* interface_version) {
  return VRInitError_None;
}

void* TestVRClientCore::GetGenericInterface(const char* name_and_version,
                                            EVRInitError* error) {
  *error = VRInitError_None;
  if (strcmp(name_and_version, IVRSystem_Version) == 0)
    return static_cast<IVRSystem*>(&g_system);
  if (strcmp(name_and_version, IVRCompositor_Version) == 0)
    return static_cast<IVRCompositor*>(&g_compositor);
  if (strcmp(name_and_version, device::kChromeOpenVRTestHookAPI) == 0)
    return static_cast<device::ServiceTestHook*>(&g_test_helper);

  *error = VRInitError_Init_InvalidInterface;
  return nullptr;
}

bool TestVRClientCore::BIsHmdPresent() {
  return true;
}

void TestVRSystem::GetRecommendedRenderTargetSize(uint32_t* width,
                                                  uint32_t* height) {
  *width = 1024;
  *height = 768;
}

void TestVRSystem::GetDXGIOutputInfo(int32_t* adapter_index) {
  GetD3D11_1AdapterIndex(adapter_index);
}

void TestVRSystem::GetProjectionRaw(EVREye eye,
                                    float* left,
                                    float* right,
                                    float* top,
                                    float* bottom) {
  auto proj = g_test_helper.GetProjectionRaw(eye == EVREye::Eye_Left);
  *left = proj.projection[0];
  *right = proj.projection[1];
  *top = proj.projection[2];
  *bottom = proj.projection[3];
}

HmdMatrix34_t TestVRSystem::GetEyeToHeadTransform(EVREye eye) {
  HmdMatrix34_t ret = {};
  ret.m[0][0] = 1;
  ret.m[1][1] = 1;
  ret.m[2][2] = 1;
  float ipd = g_test_helper.GetInterpupillaryDistance();
  ret.m[0][3] = ((eye == Eye_Left) ? 1 : -1) * ipd / 2;
  return ret;
}

void TestVRSystem::GetDeviceToAbsoluteTrackingPose(
    ETrackingUniverseOrigin origin,
    float predicted_seconds_to_photons_from_now,
    VR_ARRAY_COUNT(tracked_device_pose_array_count)
        TrackedDevicePose_t* tracked_device_pose_array,
    uint32_t tracked_device_pose_array_count) {
  TrackedDevicePose_t pose = g_test_helper.GetPose(false /* presenting pose */);
  tracked_device_pose_array[0] = pose;
  for (unsigned int i = 1; i < tracked_device_pose_array_count; ++i) {
    TrackedDevicePose_t pose = {};
    tracked_device_pose_array[i] = pose;
  }
}

bool TestVRSystem::PollNextEvent(VREvent_t*, unsigned int) {
  return false;
}

bool TestVRSystem::GetControllerState(
    TrackedDeviceIndex_t controller_device_index,
    VRControllerState_t* controller_state,
    uint32_t controller_state_size) {
  return g_test_helper.GetControllerState(controller_device_index,
                                          controller_state);
}

bool TestVRSystem::GetControllerStateWithPose(
    ETrackingUniverseOrigin origin,
    TrackedDeviceIndex_t controller_device_index,
    VRControllerState_t* controller_state,
    uint32_t controller_state_size,
    TrackedDevicePose_t* tracked_device_pose) {
  g_test_helper.GetControllerState(controller_device_index, controller_state);
  return g_test_helper.GetControllerPose(controller_device_index,
                                         tracked_device_pose);
}

uint32_t TestVRSystem::GetStringTrackedDeviceProperty(
    TrackedDeviceIndex_t device_index,
    ETrackedDeviceProperty prop,
    VR_OUT_STRING() char* value,
    uint32_t buffer_size,
    ETrackedPropertyError* error) {
  if (error) {
    *error = TrackedProp_Success;
  }
  sprintf_s(value, buffer_size, "test-value");
  return 11;
}

float TestVRSystem::GetFloatTrackedDeviceProperty(
    TrackedDeviceIndex_t device_index,
    ETrackedDeviceProperty prop,
    ETrackedPropertyError* error) {
  if (error) {
    *error = TrackedProp_Success;
  }
  switch (prop) {
    case Prop_UserIpdMeters_Float:
      return g_test_helper.GetInterpupillaryDistance();
    default:
      NOTIMPLEMENTED();
  }
  return 0;
}

int32_t TestVRSystem::GetInt32TrackedDeviceProperty(
    TrackedDeviceIndex_t device_index,
    ETrackedDeviceProperty prop,
    ETrackedPropertyError* error) {
  int32_t ret;
  ETrackedPropertyError err =
      g_test_helper.GetInt32TrackedDeviceProperty(device_index, prop, ret);
  if (err != TrackedProp_Success) {
    NOTIMPLEMENTED();
  }
  if (error) {
    *error = err;
  }
  return ret;
}

uint64_t TestVRSystem::GetUint64TrackedDeviceProperty(
    TrackedDeviceIndex_t device_index,
    ETrackedDeviceProperty prop,
    ETrackedPropertyError* error) {
  uint64_t ret;
  ETrackedPropertyError err =
      g_test_helper.GetUint64TrackedDeviceProperty(device_index, prop, ret);
  if (err != TrackedProp_Success) {
    NOTIMPLEMENTED();
  }
  if (error) {
    *error = err;
  }
  return ret;
}

HmdMatrix34_t TestVRSystem::GetSeatedZeroPoseToStandingAbsoluteTrackingPose() {
  HmdMatrix34_t ret = {};
  ret.m[0][0] = 1;
  ret.m[1][1] = 1;
  ret.m[2][2] = 1;
  ret.m[1][3] = 1.2f;
  return ret;
}

ETrackedControllerRole TestVRSystem::GetControllerRoleForTrackedDeviceIndex(
    TrackedDeviceIndex_t device_index) {
  return g_test_helper.GetControllerRoleForTrackedDeviceIndex(device_index);
}

ETrackedDeviceClass TestVRSystem::GetTrackedDeviceClass(
    TrackedDeviceIndex_t device_index) {
  return g_test_helper.GetTrackedDeviceClass(device_index);
}

void TestVRCompositor::SuspendRendering(bool suspend) {}

void TestVRCompositor::SetTrackingSpace(ETrackingUniverseOrigin) {}

EVRCompositorError TestVRCompositor::WaitGetPoses(TrackedDevicePose_t* poses1,
                                                  unsigned int count1,
                                                  TrackedDevicePose_t* poses2,
                                                  unsigned int count2) {
  TrackedDevicePose_t pose;
  for (unsigned int i = 0; i < count1; ++i) {
    if (i != vr::k_unTrackedDeviceIndex_Hmd) {
      VRControllerState_t controller_state;
      g_test_helper.GetControllerPose(i, &pose);
      pose.bDeviceIsConnected =
          g_test_helper.GetControllerState(i, &controller_state);
    } else {
      pose = g_test_helper.GetPose(true /* presenting pose */);
      pose.bDeviceIsConnected = true;
    }
    poses1[i] = pose;
  }

  for (unsigned int i = 0; i < count2; ++i) {
    if (i != vr::k_unTrackedDeviceIndex_Hmd) {
      VRControllerState_t controller_state;
      g_test_helper.GetControllerPose(i, &pose);
      pose.bDeviceIsConnected =
          g_test_helper.GetControllerState(i, &controller_state);
    } else {
      pose = g_test_helper.GetPose(true /* presenting pose */);
      pose.bDeviceIsConnected = true;
    }
    poses2[i] = pose;
  }

  return VRCompositorError_None;
}

EVRCompositorError TestVRCompositor::Submit(EVREye eye,
                                            Texture_t const* texture,
                                            VRTextureBounds_t const* bounds,
                                            EVRSubmitFlags) {
  g_test_helper.OnPresentedFrame(
      reinterpret_cast<ID3D11Texture2D*>(texture->handle), bounds, eye);
  return VRCompositorError_None;
}

void TestVRCompositor::PostPresentHandoff() {}

bool TestVRCompositor::GetFrameTiming(Compositor_FrameTiming*, unsigned int) {
  return false;
}

}  // namespace vr

extern "C" {
__declspec(dllexport) void* VRClientCoreFactory(const char* interface_name,
                                                int* return_code) {
  return &vr::g_loader;
}
}
