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
#include "base/stl_util.h"
#include "base/synchronization/lock.h"
#include "device/vr/test/test_hook.h"
#include "third_party/openxr/src/include/openxr/openxr.h"
#include "third_party/openxr/src/include/openxr/openxr_platform.h"

namespace gfx {
class Transform;
}  // namespace gfx

namespace interaction_profile {
constexpr char kMicrosoftMotionControllerInteractionProfile[] =
    "/interaction_profiles/microsoft/motion_controller";

constexpr char kKHRSimpleControllerInteractionProfile[] =
    "/interaction_profiles/khr/simple_controller";

constexpr char kOculusTouchControllerInteractionProfile[] =
    "/interaction_profiles/oculus/touch_controller";

}  // namespace interaction_profile

class OpenXrTestHelper : public device::ServiceTestHook {
 public:
  OpenXrTestHelper();
  ~OpenXrTestHelper();

  // Because the test helper isn't intended to be recreated, even if an instance
  // is destroyed, this should be called whenever a session is/would have been
  // terminated regardless of the path it took to be terminated; otherwise, it
  // may not be possible to request a new session.
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
  XrResult CreateAction(XrActionSet action_set,
                        const XrActionCreateInfo& create_info,
                        XrAction* action);
  XrActionSet CreateActionSet(const XrActionSetCreateInfo& createInfo);
  XrResult CreateActionSpace(
      const XrActionSpaceCreateInfo& action_space_create_info,
      XrSpace* space);
  XrPath GetPath(std::string path_string);
  XrPath GetCurrentInteractionProfile();

  XrResult GetSession(XrSession* session);
  XrResult BeginSession();
  XrResult EndSession();
  XrResult BeginFrame();
  XrResult EndFrame();
  XrSession session() { return session_; }

  XrResult BindActionAndPath(XrPath interaction_profile_path,
                             XrActionSuggestedBinding binding);

  void SetD3DDevice(ID3D11Device* d3d_device);
  XrResult AttachActionSets(const XrSessionActionSetsAttachInfo& attach_info);
  uint32_t AttachedActionSetsSize() const;
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
  XrResult ValidateActionSetNotAttached(XrActionSet action_set) const;
  XrResult ValidateActionSpaceCreateInfo(
      const XrActionSpaceCreateInfo& create_info) const;
  XrResult ValidateInstance(XrInstance instance) const;
  XrResult ValidateSystemId(XrSystemId system_id) const;
  XrResult ValidateSession(XrSession session) const;
  XrResult ValidateSwapchain(XrSwapchain swapchain) const;
  XrResult ValidateSpace(XrSpace space) const;
  XrResult ValidatePath(XrPath path) const;
  XrResult ValidatePredictedDisplayTime(XrTime time) const;
  XrResult ValidateXrCompositionLayerProjection(
      const XrCompositionLayerProjection& projection_layer) const;
  XrResult ValidateXrCompositionLayerProjectionView(
      const XrCompositionLayerProjectionView& projection_view) const;
  XrResult ValidateXrPosefIsIdentity(const XrPosef& pose) const;
  XrResult ValidateViews(uint32_t view_capacity_input, XrView* views) const;

  // Properties of the mock OpenXR runtime that do not change are created
  static constexpr const char* const kExtensions[] = {
      XR_KHR_D3D11_ENABLE_EXTENSION_NAME,
      XR_EXT_WIN32_APPCONTAINER_COMPATIBLE_EXTENSION_NAME};
  static constexpr uint32_t kDimension = 128;
  static constexpr uint32_t kSwapCount = 1;
  static constexpr uint32_t kMinSwapchainBuffering = 3;
  static constexpr uint32_t kViewCount = 2;
  static constexpr XrViewConfigurationView kViewConfigView = {
      XR_TYPE_VIEW_CONFIGURATION_VIEW,
      nullptr,
      kDimension,
      kDimension,
      kDimension,
      kDimension,
      kSwapCount,
      kSwapCount};
  static constexpr XrViewConfigurationView kViewConfigurationViews[] = {
      kViewConfigView, kViewConfigView};
  static constexpr XrViewConfigurationType kViewConfigurationType =
      XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
  static constexpr XrEnvironmentBlendMode kEnvironmentBlendMode =
      XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
  static constexpr const char* kLocalReferenceSpacePath =
      "/reference_space/local";
  static constexpr const char* kStageReferenceSpacePath =
      "/reference_space/stage";
  static constexpr const char* kViewReferenceSpacePath =
      "/reference_space/view";
  static constexpr const char* kUnboundedReferenceSpacePath =
      "/reference_space/unbounded";

  static constexpr uint32_t kNumExtensionsSupported = base::size(kExtensions);
  static constexpr uint32_t kNumViews = base::size(kViewConfigurationViews);

 private:
  struct ActionProperties {
    std::unordered_map<XrPath, XrPath> profile_binding_map;
    XrActionType type;
    ActionProperties();
    ~ActionProperties();
    ActionProperties(const ActionProperties& other);
  };

  void CopyTextureDataIntoFrameData(device::SubmittedFrameData* data,
                                    bool left);
  XrResult UpdateAction(XrAction action);
  void SetSessionState(XrSessionState state);
  base::Optional<gfx::Transform> GetPose();
  device::ControllerFrameData GetControllerDataFromPath(
      std::string path_string) const;
  void UpdateInteractionProfile(
      device_test::mojom::InteractionProfileType type);
  bool IsSessionRunning() const;

  // Properties of the mock OpenXR runtime that doesn't change throughout the
  // lifetime of the instance. However, these aren't static because they are
  // initialized to an invalid value and set to their actual value in their
  // respective Get*/Create* functions. This allows these variables to be used
  // to validate that they were queried before being used.
  XrSystemId system_id_;
  XrSession session_;
  XrSwapchain swapchain_;

  // Properties that changes depending on the state of the runtime.
  XrSessionState session_state_;
  bool frame_begin_;
  Microsoft::WRL::ComPtr<ID3D11Device> d3d_device_;
  std::vector<Microsoft::WRL::ComPtr<ID3D11Texture2D>> textures_arr_;
  uint32_t acquired_swapchain_texture_;
  uint32_t next_space_;
  XrTime next_predicted_display_time_;
  std::string interaction_profile_;

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
  std::unordered_map<XrActionSet, std::vector<XrAction>> attached_action_sets_;
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
