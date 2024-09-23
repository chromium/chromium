// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_OPENXR_TEST_OPENXR_TEST_HELPER_H_
#define DEVICE_VR_OPENXR_TEST_OPENXR_TEST_HELPER_H_

#include <array>
#include <optional>
#include <queue>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "base/memory/raw_ptr_exclusion.h"
#include "base/synchronization/lock.h"
#include "device/vr/openxr/openxr_platform.h"
#include "device/vr/openxr/openxr_view_configuration.h"
#include "device/vr/test/test_hook.h"
#include "third_party/openxr/src/include/openxr/openxr.h"

#if BUILDFLAG(IS_WIN)
#include <wrl.h>
#endif

namespace gfx {
class Transform;
}  // namespace gfx

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
  XrSystemProperties GetSystemProperties();

  XrSwapchain CreateSwapchain();
  XrResult DestroySwapchain(XrSwapchain);
  XrInstance CreateInstance();
  XrResult DestroyInstance(XrInstance instance);
  XrResult CreateSession(XrSession* session);
  XrResult DestroySession(XrSession session);
  XrResult GetActionStateFloat(XrAction action, XrActionStateFloat* data) const;
  XrResult GetActionStateBoolean(XrAction action,
                                 XrActionStateBoolean* data) const;
  XrResult GetActionStateVector2f(XrAction action,
                                  XrActionStateVector2f* data) const;
  XrResult GetActionStatePose(XrAction action, XrActionStatePose* data) const;
  XrResult CreateActionSpace(
      const XrActionSpaceCreateInfo& action_space_create_info,
      XrSpace* space);
  XrSpace CreateReferenceSpace(XrReferenceSpaceType type);
  XrResult DestroySpace(XrSpace space);
  XrResult CreateAction(XrActionSet action_set,
                        const XrActionCreateInfo& create_info,
                        XrAction* action);
  XrActionSet CreateActionSet(const XrActionSetCreateInfo& create_info);
  XrResult DestroyActionSet(XrActionSet action_set);
  XrPath GetPath(std::string path_string);
  XrPath GetCurrentInteractionProfile();
  XrHandTrackerEXT CreateHandTracker(XrHandEXT hand);
  XrResult DestroyHandTracker(XrHandTrackerEXT hand_tracker);

  device::OpenXrViewConfiguration& GetViewConfigInfo(
      XrViewConfigurationType view_config);
  std::vector<XrViewConfigurationType> SupportedViewConfigs() const;
  XrResult GetSecondaryConfigStates(
      uint32_t count,
      XrSecondaryViewConfigurationStateMSFT* states) const;
  XrViewConfigurationType PrimaryViewConfig() const;

  XrResult BeginSession(
      const std::vector<XrViewConfigurationType>& view_configs);
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
  bool UpdateViews(XrViewConfigurationType view_config_type,
                   XrView views[],
                   uint32_t size);

  uint32_t NextSwapchainImageIndex();
  XrTime NextPredictedDisplayTime();

  void UpdateEventQueue();
  XrResult PollEvent(XrEventDataBuffer* event_data);

  void LocateJoints(XrHandTrackerEXT hand_tracker,
                    const XrHandJointsLocateInfoEXT* locate_info,
                    XrHandJointLocationsEXT* locations);

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
  XrResult ValidateHandTracker(XrHandTrackerEXT hand_tracker) const;
  XrResult ValidateInstance(XrInstance instance) const;
  XrResult ValidateSystemId(XrSystemId system_id) const;
  XrResult ValidateSession(XrSession session) const;
  XrResult ValidateSwapchain(XrSwapchain swapchain) const;
  XrResult ValidateSpace(XrSpace space) const;
  XrResult ValidatePath(XrPath path) const;
  XrResult ValidatePredictedDisplayTime(XrTime time) const;
  XrResult ValidateXrCompositionLayerProjection(
      XrViewConfigurationType view_config,
      const XrCompositionLayerProjection& projection_layer);
  XrResult ValidateXrPosefIsIdentity(const XrPosef& pose) const;
  XrResult ValidateViews(uint32_t view_capacity_input, XrView* views) const;
  XrResult ValidateViewConfigType(XrViewConfigurationType view_config) const;

  // Properties of the mock OpenXR runtime that do not change are created
  static constexpr const char* const kExtensions[] = {
      XR_KHR_D3D11_ENABLE_EXTENSION_NAME,
      XR_EXT_WIN32_APPCONTAINER_COMPATIBLE_EXTENSION_NAME,
      XR_EXT_SAMSUNG_ODYSSEY_CONTROLLER_EXTENSION_NAME,
      XR_EXT_HP_MIXED_REALITY_CONTROLLER_EXTENSION_NAME,
      XR_MSFT_HAND_INTERACTION_EXTENSION_NAME,
      XR_EXT_HAND_INTERACTION_EXTENSION_NAME,
      XR_FB_HAND_TRACKING_MESH_EXTENSION_NAME,
      XR_HTC_VIVE_COSMOS_CONTROLLER_INTERACTION_EXTENSION_NAME,
      XR_MSFT_SECONDARY_VIEW_CONFIGURATION_EXTENSION_NAME,
      XR_EXT_HAND_TRACKING_EXTENSION_NAME,
  };

  static constexpr uint32_t kPrimaryViewDimension = 128;
  static constexpr uint32_t kSecondaryViewDimension = 64;

  static constexpr uint32_t kSwapCount = 1;
  static constexpr uint32_t kMinSwapchainBuffering = 3;

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
  static constexpr XrSystemProperties kSystemProperties = {
      XR_TYPE_SYSTEM_PROPERTIES, nullptr,           0, 0xBADFACE, "Test System",
      {2048, 2048, 1},           {XR_TRUE, XR_TRUE}};

  static constexpr uint32_t kNumExtensionsSupported = std::size(kExtensions);

  static constexpr XrSpaceLocationFlags kValidTrackedPoseFlags =
      XR_SPACE_LOCATION_ORIENTATION_VALID_BIT |
      XR_SPACE_LOCATION_POSITION_VALID_BIT |
      XR_SPACE_LOCATION_ORIENTATION_TRACKED_BIT |
      XR_SPACE_LOCATION_POSITION_TRACKED_BIT;

 private:
  struct ActionProperties {
    std::unordered_map<XrPath, XrPath> profile_binding_map;
    XrActionType type;
    ActionProperties();
    ~ActionProperties();
    ActionProperties(const ActionProperties& other);
  };

  void CopyTextureDataIntoFrameData(uint32_t x_start, device::ViewData& data);
  void ReinitializeTextures();
  void CreateTextures(uint32_t width, uint32_t height);
  void AddDimensions(const device::OpenXrViewConfiguration& view_config,
                     uint32_t& width,
                     uint32_t& height) const;
  XrResult UpdateAction(XrAction action);
  void SetSessionState(XrSessionState state);
  std::optional<gfx::Transform> GetPose();
  std::optional<device::DeviceConfig> GetDeviceConfig();
  device::ControllerFrameData GetControllerDataFromPath(
      std::string path_string) const;
  void UpdateInteractionProfile(
      device::mojom::OpenXrInteractionProfileType type);
  bool IsSessionRunning() const;
  XrResult ValidateXrCompositionLayerProjectionView(
      const XrCompositionLayerProjectionView& projection_view,
      uint32_t view_count,
      uint32_t index);
  bool GetCanCreateSession();
  std::optional<gfx::Transform> GetTransformForSpace(XrSpace space);

  // Properties of the mock OpenXR runtime that doesn't change throughout the
  // lifetime of the instance. However, these aren't static because they are
  // initialized to an invalid value and set to their actual value in their
  // respective Get*/Create* functions. This allows these variables to be used
  // to validate that they were queried before being used.
  XrSystemId system_id_;
  XrSession session_;
  XrSwapchain swapchain_;
  XrHandTrackerEXT left_hand_;
  XrHandTrackerEXT right_hand_;

  // Properties that changes depending on the state of the runtime.
  uint32_t frame_count_ = 0;
  XrSessionState session_state_;
  bool frame_begin_;
  Microsoft::WRL::ComPtr<ID3D11Device> d3d_device_;
  std::vector<Microsoft::WRL::ComPtr<ID3D11Texture2D>> textures_arr_;
  uint32_t acquired_swapchain_texture_;
  uint32_t next_handle_;
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

  // View configurations that were requested by the client when the session
  // begins. There should only be one supported primary view configurations and
  // any number of secondary view configurations.
  std::vector<XrViewConfigurationType> view_configs_enabled_;

  // All view configurations that we currently support, including ones not
  // requested by the client.
  std::unordered_map<XrViewConfigurationType, device::OpenXrViewConfiguration>
      primary_configs_supported_;
  std::unordered_map<XrViewConfigurationType, device::OpenXrViewConfiguration>
      secondary_configs_supported_;

  std::array<device::ControllerFrameData, device::kMaxTrackedDevices> data_arr_;

  std::queue<XrEventDataBuffer> event_queue_;

  // This field is not a raw_ptr<> because it was filtered by the rewriter for:
  // #global-scope
  RAW_PTR_EXCLUSION device::VRTestHook* test_hook_ GUARDED_BY(lock_) = nullptr;
  base::Lock lock_;
};

#endif  // DEVICE_VR_OPENXR_TEST_OPENXR_TEST_HELPER_H_
