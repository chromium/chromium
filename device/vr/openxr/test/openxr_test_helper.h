// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_OPENXR_TEST_OPENXR_TEST_HELPER_H_
#define DEVICE_VR_OPENXR_TEST_OPENXR_TEST_HELPER_H_

#include <d3d11.h>
#include <unknwn.h>
#include <wrl.h>
#include <array>
#include <queue>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "base/optional.h"
#include "base/synchronization/lock.h"
#include "device/vr/test/test_hook.h"
#include "third_party/openxr/src/include/openxr/openxr.h"

namespace gfx {
class Transform;
}  // namespace gfx

class OpenXrTestHelper : public device::ServiceTestHook {
 public:
  OpenXrTestHelper();
  ~OpenXrTestHelper();
  void Reset();
  void TestFailure();

  // TestHookRegistration
  void SetTestHook(device::VRTestHook* hook) final;

  // Helper methods called by the mock OpenXR runtime. These methods will
  // call back into the test hook, thus communicating with the test object
  // on the browser process side.
  void OnPresentedFrame();

  // Helper methods called by the mock OpenXR runtime to query or set the
  // state of the runtime.

  XrSystemId GetSystemId();
  XrSwapchain GetSwapchain();
  XrInstance CreateInstance();
  XrResult GetActionStateFloat(XrAction action, XrActionStateFloat* data) const;
  XrResult GetActionStateBoolean(XrAction action,
                                 XrActionStateBoolean* data) const;
  XrResult GetActionStateVector2f(XrAction action,
                                  XrActionStateVector2f* data) const;
  XrResult GetActionStatePose(XrAction action, XrActionStatePose* data) const;
  XrSpace CreateReferenceSpace(XrReferenceSpaceType type);
  XrAction CreateAction(XrActionSet action_set,
                        const XrActionCreateInfo& create_info);
  XrActionSet CreateActionSet(const XrActionSetCreateInfo& createInfo);
  XrSpace CreateActionSpace(XrAction);
  XrPath GetPath(const char* path_string);
  XrPath GetCurrentInteractionProfile();

  XrResult GetSession(XrSession* session);
  XrResult BeginSession();
  XrResult EndSession();

  XrResult BindActionAndPath(XrActionSuggestedBinding binding);

  void SetD3DDevice(ID3D11Device* d3d_device);
  XrResult SyncActionData(XrActionSet action_set);
  const std::vector<Microsoft::WRL::ComPtr<ID3D11Texture2D>>&
  GetSwapchainTextures() const;
  void LocateSpace(XrSpace space, XrPosef* pose);
  std::string PathToString(XrPath path) const;
  bool UpdateData();
  bool UpdateViewFOV(XrView views[], uint32_t size);

  uint32_t NextSwapchainImageIndex();
  XrTime NextPredictedDisplayTime();

  void UpdateEventQueue();
  XrResult PollEvent(XrEventDataBuffer* event_data);

  // Methods that validate the parameter with the current state of the runtime.
  XrResult ValidateAction(XrAction action) const;
  XrResult ValidateActionCreateInfo(
      const XrActionCreateInfo& create_info) const;
  XrResult ValidateActionSet(XrActionSet action_set) const;
  XrResult ValidateActionSetCreateInfo(
      const XrActionSetCreateInfo& create_info) const;
  XrResult ValidateActionSpaceCreateInfo(
      const XrActionSpaceCreateInfo& create_info) const;
  XrResult ValidateInstance(XrInstance instance) const;
  XrResult ValidateSystemId(XrSystemId system_id) const;
  XrResult ValidateSession(XrSession session) const;
  XrResult ValidateSwapchain(XrSwapchain swapchain) const;
  XrResult ValidateSpace(XrSpace space) const;
  XrResult ValidatePath(XrPath path) const;
  XrResult ValidatePredictedDisplayTime(XrTime time) const;
  XrResult ValidateXrPosefIsIdentity(const XrPosef& pose) const;
  XrResult ValidateViews(uint32_t view_capacity_input, XrView* views) const;

  // Properties of the mock OpenXR runtime that does not change are created
  // as static variables.
  static uint32_t NumExtensionsSupported();
  static uint32_t NumViews();
  static const char* kExtensions[];
  static const uint32_t kDimension;
  static const uint32_t kSwapCount;
  static const uint32_t kMinSwapchainBuffering;
  static const uint32_t kViewCount;
  static const XrViewConfigurationView kViewConfigView;
  static XrViewConfigurationView kViewConfigurationViews[];
  static const XrViewConfigurationType kViewConfigurationType;
  static const XrEnvironmentBlendMode kEnvironmentBlendMode;

 private:
  struct ActionProperties {
    XrPath binding;
    XrActionType type;
    ActionProperties() : binding(XR_NULL_PATH), type(XR_ACTION_TYPE_MAX_ENUM) {}
  };

  XrResult UpdateAction(XrAction action);
  void SetSessionState(XrSessionState state);
  base::Optional<gfx::Transform> GetPose();
  device::ControllerFrameData GetControllerDataFromPath(
      std::string path_string) const;

  bool create_fake_instance_;

  // Properties of the mock OpenXR runtime that doesn't change throughout the
  // lifetime of the instance. However, these aren't static because they are
  // initialized to an invalid value and set to their actual value in their
  // respective Get*/Create* functions. This allows these variables to be used
  // to validate that they were queried before being used.
  XrSystemId system_id_;
  XrSession session_;
  XrSessionState session_state_;
  XrSwapchain swapchain_;

  // Properties that changes depending on the state of the runtime.
  Microsoft::WRL::ComPtr<ID3D11Device> d3d_device_;
  std::vector<Microsoft::WRL::ComPtr<ID3D11Texture2D>> textures_arr_;
  uint32_t acquired_swapchain_texture_;
  uint32_t next_space_;
  XrTime next_predicted_display_time_;

  // paths_ is used to keep tracked of strings that already has a corresponding
  // path.
  std::vector<std::string> paths_;
  // ActionProperties has an
  // index_in_action_state_arr member which can help index into the following
  // *_action_states_ vector and retrieve data.
  std::unordered_map<XrAction, ActionProperties> actions_;
  std::unordered_map<XrSpace, XrAction> action_spaces_;
  std::unordered_map<XrSpace, std::string> reference_spaces_;
  std::unordered_map<XrActionSet, std::vector<XrAction>> action_sets_;
  std::unordered_map<XrAction, XrActionStateFloat> float_action_states_;
  std::unordered_map<XrAction, XrActionStateBoolean> boolean_action_states_;
  std::unordered_map<XrAction, XrActionStateVector2f> v2f_action_states_;
  std::unordered_map<XrAction, XrActionStatePose> pose_action_state_;

  // action_names_, action_localized_names_, action_set_names_,
  // action_set_localized_names_ are used to make sure that there won't be any
  // duplicate which is specified in the spec. They are all independent.
  std::unordered_set<std::string> action_names_;
  std::unordered_set<std::string> action_localized_names_;
  std::unordered_set<std::string> action_set_names_;
  std::unordered_set<std::string> action_set_localized_names_;

  std::array<device::ControllerFrameData, device::kMaxTrackedDevices> data_arr_;

  std::queue<XrEventDataBuffer> event_queue_;

  device::VRTestHook* test_hook_ GUARDED_BY(lock_) = nullptr;
  base::Lock lock_;
};

#endif  // DEVICE_VR_OPENXR_TEST_OPENXR_TEST_HELPER_H_
