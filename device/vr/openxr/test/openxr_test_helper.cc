// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "device/vr/openxr/test/openxr_test_helper.h"

#include <cmath>
#include <limits>

#include "base/containers/contains.h"
#include "device/vr/openxr/openxr_interaction_profile_paths.h"
#include "device/vr/openxr/openxr_platform.h"
#include "device/vr/openxr/openxr_util.h"
#include "device/vr/openxr/openxr_view_configuration.h"
#include "third_party/openxr/src/src/common/hex_and_handles.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/geometry/transform_util.h"

namespace {
bool PathContainsString(const std::string& path, const std::string& s) {
  return base::Contains(path, s);
}

device::XrEye GetEyeForIndex(uint32_t index, uint32_t num_views) {
  DCHECK_LE(num_views, 2u);

  if (num_views == 1) {
    // Per WebXR spec, the eye for the first person observer view is none.
    return device::XrEye::kNone;
  }

  // Per OpenXR spec, the left eye is at index 0 and the right eye at index 1
  // for the XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO view configuration.
  return (index == 0) ? device::XrEye::kLeft : device::XrEye::kRight;
}

int GetOffsetMultiplierForIndex(uint32_t index) {
  return ((index % 2 == 0) ? 1 : -1);
}

}  // namespace

OpenXrTestHelper::ActionProperties::ActionProperties()
    : type(XR_ACTION_TYPE_MAX_ENUM) {}

OpenXrTestHelper::ActionProperties::~ActionProperties() = default;

OpenXrTestHelper::ActionProperties::ActionProperties(
    const ActionProperties& other) {
  this->type = other.type;
  this->profile_binding_map = other.profile_binding_map;
}

OpenXrTestHelper::OpenXrTestHelper()
    // since openxr_statics is created first, so the first instance returned
    // should be a fake one since openxr_statics does not need to use
    // test_hook_;
    : system_id_(0),
      session_(XR_NULL_HANDLE),
      swapchain_(XR_NULL_HANDLE),
      frame_count_(0),
      session_state_(XR_SESSION_STATE_UNKNOWN),
      frame_begin_(false),
      acquired_swapchain_texture_(0),
      next_handle_(0),
      next_predicted_display_time_(0),
      interaction_profile_(device::kMicrosoftMotionInteractionProfilePath) {
  // We currently only support one primary and one secondary view configs, but
  // there will likely be more added in the future to support various devices.
  // Add new ones here for testing.

  // Per spec, the primary view configuration is always active.
  primary_configs_supported_.insert(
      {XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,
       device::OpenXrViewConfiguration(
           XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, true /*active*/,
           2 /*num_views*/, kPrimaryViewDimension, kSwapCount)});

  // Mark all secondary views as inactive initially until a certain number of
  // frames has passed, so we can to test the process of switching states. It's
  // a common scenario for secondary view configurations to become active or
  // inactive during a session.
  secondary_configs_supported_.insert(
      {XR_VIEW_CONFIGURATION_TYPE_SECONDARY_MONO_FIRST_PERSON_OBSERVER_MSFT,
       device::OpenXrViewConfiguration(
           XR_VIEW_CONFIGURATION_TYPE_SECONDARY_MONO_FIRST_PERSON_OBSERVER_MSFT,
           false /*active*/, 1 /*num_views*/, kSecondaryViewDimension,
           kSwapCount)});
}

OpenXrTestHelper::~OpenXrTestHelper() = default;

void OpenXrTestHelper::Reset() {
  session_ = XR_NULL_HANDLE;
  swapchain_ = XR_NULL_HANDLE;
  session_state_ = XR_SESSION_STATE_UNKNOWN;

  system_id_ = 0;
  frame_begin_ = false;
  d3d_device_ = nullptr;
  acquired_swapchain_texture_ = 0;
  next_handle_ = 0;
  next_predicted_display_time_ = 0;

  // vectors
  textures_arr_.clear();
  paths_.clear();

  // unordered_maps
  actions_.clear();
  action_spaces_.clear();
  reference_spaces_.clear();
  action_sets_.clear();
  attached_action_sets_.clear();
  float_action_states_.clear();
  boolean_action_states_.clear();
  v2f_action_states_.clear();
  pose_action_state_.clear();

  // unordered_sets
  action_names_.clear();
  action_localized_names_.clear();
  action_set_names_.clear();
  action_set_localized_names_.clear();
}

void OpenXrTestHelper::TestFailure() {
  NOTREACHED();
}

void OpenXrTestHelper::SetTestHook(device::VRTestHook* hook) {
  base::AutoLock auto_lock(lock_);
  test_hook_ = hook;
}

void OpenXrTestHelper::OnPresentedFrame() {
  DCHECK_NE(textures_arr_.size(), 0ull);

  std::vector<device::ViewData> submitted_views;
  uint32_t current_x = 0;

  for (XrViewConfigurationType view_config_type : view_configs_enabled_) {
    const device::OpenXrViewConfiguration& view_config =
        GetViewConfigInfo(view_config_type);
    if (view_config.Active()) {
      const std::vector<device::OpenXrViewProperties>& view_properties =
          view_config.Properties();
      for (uint32_t i = 0; i < view_properties.size(); i++) {
        const device::OpenXrViewProperties& properties = view_properties[i];
        device::ViewData& data = submitted_views.emplace_back();
        data.viewport =
            gfx::Rect(current_x, 0, properties.Width(), properties.Height());
        data.eye = GetEyeForIndex(i, view_properties.size());

        CopyTextureDataIntoFrameData(current_x, data);
        current_x += properties.Width();
      }
    }
  }

  base::AutoLock auto_lock(lock_);
  if (!test_hook_)
    return;

  test_hook_->OnFrameSubmitted(submitted_views);
}

void OpenXrTestHelper::CopyTextureDataIntoFrameData(uint32_t x_start,
                                                    device::ViewData& data) {
  DCHECK(d3d_device_);
  DCHECK_NE(textures_arr_.size(), 0ull);
  Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
  d3d_device_->GetImmediateContext(&context);

  constexpr uint32_t buffer_size = sizeof(device::ViewData::raw_buffer);
  constexpr uint32_t buffer_size_pixels = buffer_size / sizeof(device::Color);

  // We copy the submitted texture to a new texture, so we can map it, and
  // read back pixel data.
  auto desc = CD3D11_TEXTURE2D_DESC();
  desc.ArraySize = 1;
  desc.Width = buffer_size_pixels;
  desc.Height = 1;
  desc.MipLevels = 1;
  desc.SampleDesc.Count = 1;
  desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  desc.Usage = D3D11_USAGE_STAGING;
  desc.BindFlags = 0;
  desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

  Microsoft::WRL::ComPtr<ID3D11Texture2D> texture_destination;
  HRESULT hr =
      d3d_device_->CreateTexture2D(&desc, nullptr, &texture_destination);
  DCHECK_EQ(hr, S_OK);

  // A strip of pixels along the top of the texture, however many will fit into
  // our buffer.
  D3D11_BOX box{x_start, 0, 0, x_start + buffer_size_pixels, 1, 1};
  context->CopySubresourceRegion(
      texture_destination.Get(), 0, 0, 0, 0,
      textures_arr_[acquired_swapchain_texture_].Get(), 0, &box);

  D3D11_MAPPED_SUBRESOURCE map_data = {};
  hr = context->Map(texture_destination.Get(), 0, D3D11_MAP_READ, 0, &map_data);
  DCHECK_EQ(hr, S_OK);
  // We have a 1-pixel image, so store it in the provided ViewData
  // along with the raw data.
  device::Color* color = static_cast<device::Color*>(map_data.pData);
  data.color = color[0];
  memcpy(&data.raw_buffer, map_data.pData, buffer_size);

  context->Unmap(texture_destination.Get(), 0);
}

XrSystemId OpenXrTestHelper::GetSystemId() {
  system_id_ = 1;
  return system_id_;
}

XrSystemProperties OpenXrTestHelper::GetSystemProperties() {
  return kSystemProperties;
}

XrResult OpenXrTestHelper::CreateSession(XrSession* session) {
  RETURN_IF(session_state_ != XR_SESSION_STATE_UNKNOWN,
            XR_ERROR_VALIDATION_FAILURE,
            "SessionState is not unknown before xrCreateSession");
  session_ = TreatIntegerAsHandle<XrSession>(++next_handle_);
  *session = session_;
  SetSessionState(XR_SESSION_STATE_IDLE);
  if (GetCanCreateSession()) {
    SetSessionState(XR_SESSION_STATE_READY);
  } else {
    SetSessionState(XR_SESSION_STATE_EXITING);
  }
  return XR_SUCCESS;
}

XrResult OpenXrTestHelper::DestroySession(XrSession session) {
  RETURN_IF_XR_FAILED(ValidateSession(session));

  // Clear the test helper state so that tests can request multiple sessions.
  Reset();

  return XR_SUCCESS;
}

XrSwapchain OpenXrTestHelper::CreateSwapchain() {
  // Our OpenXR backend currently only creates one swapchain at a time, so any
  // previously created swapchain must have been destroyed.
  DCHECK_EQ(swapchain_, static_cast<XrSwapchain>(XR_NULL_HANDLE));
  swapchain_ = TreatIntegerAsHandle<XrSwapchain>(++next_handle_);
  return swapchain_;
}

XrResult OpenXrTestHelper::DestroySwapchain(XrSwapchain swapchain) {
  RETURN_IF_XR_FAILED(ValidateSwapchain(swapchain));
  swapchain_ = XR_NULL_HANDLE;
  return XR_SUCCESS;
}

XrInstance OpenXrTestHelper::CreateInstance() {
  return reinterpret_cast<XrInstance>(this);
}

XrResult OpenXrTestHelper::DestroyInstance(XrInstance instance) {
  RETURN_IF_XR_FAILED(ValidateInstance(instance));
  // Though Reset() primarily clears variables relating to being able to create
  // a new session, some tests may instead destroy the device (to simulate a
  // crash or simply removing the headset). It is impossible to keep an active
  // session with a destroyed instance, so this ensures that the test helper is
  // setup to allow a new session to be requested.
  Reset();
  return XR_SUCCESS;
}

XrResult OpenXrTestHelper::GetActionStateFloat(XrAction action,
                                               XrActionStateFloat* data) const {
  RETURN_IF_XR_FAILED(ValidateAction(action));
  const ActionProperties& cur_action_properties = actions_.at(action);
  RETURN_IF(cur_action_properties.type != XR_ACTION_TYPE_FLOAT_INPUT,
            XR_ERROR_ACTION_TYPE_MISMATCH, "XrActionStateFloat type mismatch");
  RETURN_IF(data == nullptr, XR_ERROR_VALIDATION_FAILURE,
            "XrActionStateFloat is nullptr");
  *data = float_action_states_.at(action);
  return XR_SUCCESS;
}

XrResult OpenXrTestHelper::GetActionStateBoolean(
    XrAction action,
    XrActionStateBoolean* data) const {
  RETURN_IF_XR_FAILED(ValidateAction(action));
  const ActionProperties& cur_action_properties = actions_.at(action);
  RETURN_IF(cur_action_properties.type != XR_ACTION_TYPE_BOOLEAN_INPUT,
            XR_ERROR_ACTION_TYPE_MISMATCH,
            "GetActionStateBoolean type mismatch");
  RETURN_IF(data == nullptr, XR_ERROR_VALIDATION_FAILURE,
            "XrActionStateBoolean is nullptr");
  *data = boolean_action_states_.at(action);
  return XR_SUCCESS;
}

XrResult OpenXrTestHelper::GetActionStateVector2f(
    XrAction action,
    XrActionStateVector2f* data) const {
  RETURN_IF_XR_FAILED(ValidateAction(action));
  const ActionProperties& cur_action_properties = actions_.at(action);
  RETURN_IF(cur_action_properties.type != XR_ACTION_TYPE_VECTOR2F_INPUT,
            XR_ERROR_ACTION_TYPE_MISMATCH,
            "GetActionStateVector2f type mismatch");
  RETURN_IF(data == nullptr, XR_ERROR_VALIDATION_FAILURE,
            "XrActionStateVector2f is nullptr");
  *data = v2f_action_states_.at(action);
  return XR_SUCCESS;
}

XrResult OpenXrTestHelper::GetActionStatePose(XrAction action,
                                              XrActionStatePose* data) const {
  RETURN_IF_XR_FAILED(ValidateAction(action));
  const ActionProperties& cur_action_properties = actions_.at(action);
  RETURN_IF(cur_action_properties.type != XR_ACTION_TYPE_POSE_INPUT,
            XR_ERROR_ACTION_TYPE_MISMATCH,
            "GetActionStateVector2f type mismatch");
  RETURN_IF(data == nullptr, XR_ERROR_VALIDATION_FAILURE,
            "XrActionStatePose is nullptr");
  *data = pose_action_state_.at(action);
  return XR_SUCCESS;
}

XrSpace OpenXrTestHelper::CreateReferenceSpace(XrReferenceSpaceType type) {
  XrSpace cur_space = TreatIntegerAsHandle<XrSpace>(++next_handle_);
  switch (type) {
    case XR_REFERENCE_SPACE_TYPE_VIEW:
      reference_spaces_[cur_space] = kViewReferenceSpacePath;
      break;
    case XR_REFERENCE_SPACE_TYPE_LOCAL:
      reference_spaces_[cur_space] = kLocalReferenceSpacePath;
      break;
    case XR_REFERENCE_SPACE_TYPE_STAGE:
      reference_spaces_[cur_space] = kStageReferenceSpacePath;
      break;
    case XR_REFERENCE_SPACE_TYPE_UNBOUNDED_MSFT:
      reference_spaces_[cur_space] = kUnboundedReferenceSpacePath;
      break;
    default:
      NOTREACHED() << "Unsupported XrReferenceSpaceType: " << type;
  }
  return cur_space;
}

XrResult OpenXrTestHelper::CreateActionSpace(
    const XrActionSpaceCreateInfo& action_space_create_info,
    XrSpace* space) {
  RETURN_IF_XR_FAILED(ValidateActionSpaceCreateInfo(action_space_create_info));
  *space = TreatIntegerAsHandle<XrSpace>(++next_handle_);
  action_spaces_[*space] = action_space_create_info.action;
  return XR_SUCCESS;
}

XrResult OpenXrTestHelper::DestroySpace(XrSpace space) {
  RETURN_IF_XR_FAILED(ValidateSpace(space));
  reference_spaces_.erase(space) || action_spaces_.erase(space);
  return XR_SUCCESS;
}

XrResult OpenXrTestHelper::CreateAction(XrActionSet action_set,
                                        const XrActionCreateInfo& create_info,
                                        XrAction* action) {
  RETURN_IF_XR_FAILED(ValidateActionSet(action_set));
  RETURN_IF_XR_FAILED(ValidateActionSetNotAttached(action_set));
  RETURN_IF_XR_FAILED(ValidateActionCreateInfo(create_info));
  action_names_.emplace(create_info.actionName);
  action_localized_names_.emplace(create_info.localizedActionName);
  // The OpenXR Loader will return an error if the action handle is 0.
  XrAction cur_action = TreatIntegerAsHandle<XrAction>(++next_handle_);
  ActionProperties cur_action_properties;
  cur_action_properties.type = create_info.actionType;
  switch (create_info.actionType) {
    case XR_ACTION_TYPE_FLOAT_INPUT: {
      float_action_states_[cur_action];
      break;
    }
    case XR_ACTION_TYPE_BOOLEAN_INPUT: {
      boolean_action_states_[cur_action];
      break;
    }
    case XR_ACTION_TYPE_VECTOR2F_INPUT: {
      v2f_action_states_[cur_action];
      break;
    }
    case XR_ACTION_TYPE_POSE_INPUT: {
      pose_action_state_[cur_action];
      break;
    }
    default: {
      LOG(ERROR)
          << __FUNCTION__
          << "This type of Action is not supported by test at the moment";
    }
  }

  action_sets_[action_set].push_back(cur_action);
  actions_[cur_action] = cur_action_properties;
  RETURN_IF(action == nullptr, XR_ERROR_VALIDATION_FAILURE,
            "XrAction is nullptr");
  *action = cur_action;
  return XR_SUCCESS;
}

XrActionSet OpenXrTestHelper::CreateActionSet(
    const XrActionSetCreateInfo& create_info) {
  action_set_names_.emplace(create_info.actionSetName);
  action_set_localized_names_.emplace(create_info.localizedActionSetName);
  // The OpenXR Loader will return an error if the action set handle is 0.
  XrActionSet cur_action_set =
      TreatIntegerAsHandle<XrActionSet>(++next_handle_);
  action_sets_[cur_action_set];
  return cur_action_set;
}

XrResult OpenXrTestHelper::DestroyActionSet(XrActionSet action_set) {
  RETURN_IF_XR_FAILED(ValidateActionSet(action_set));
  action_sets_.erase(action_set);
  return XR_SUCCESS;
}

XrPath OpenXrTestHelper::GetPath(std::string path_string) {
  for (auto it = paths_.begin(); it != paths_.end(); it++) {
    if (it->compare(path_string) == 0) {
      return it - paths_.begin() + 1;
    }
  }
  paths_.emplace_back(path_string);
  // path can't be 0 since 0 is reserved for XR_NULL_HANDLE
  return paths_.size();
}

XrPath OpenXrTestHelper::GetCurrentInteractionProfile() {
  return GetPath(interaction_profile_);
}

XrHandTrackerEXT OpenXrTestHelper::CreateHandTracker(XrHandEXT hand) {
  switch (hand) {
    case XR_HAND_LEFT_EXT:
      DCHECK_EQ(left_hand_, static_cast<XrHandTrackerEXT>(XR_NULL_HANDLE));
      left_hand_ = TreatIntegerAsHandle<XrHandTrackerEXT>(++next_handle_);
      return left_hand_;
    case XR_HAND_RIGHT_EXT:
      DCHECK_EQ(right_hand_, static_cast<XrHandTrackerEXT>(XR_NULL_HANDLE));
      right_hand_ = TreatIntegerAsHandle<XrHandTrackerEXT>(++next_handle_);
      return right_hand_;
    default:
      NOTREACHED();
  }
}

XrResult OpenXrTestHelper::DestroyHandTracker(XrHandTrackerEXT hand_tracker) {
  RETURN_IF_XR_FAILED(ValidateHandTracker(hand_tracker));
  if (left_hand_ == hand_tracker) {
    left_hand_ = XR_NULL_HANDLE;
  } else if (right_hand_ == hand_tracker) {
    right_hand_ = XR_NULL_HANDLE;
  }

  return XR_SUCCESS;
}

device::OpenXrViewConfiguration& OpenXrTestHelper::GetViewConfigInfo(
    XrViewConfigurationType view_config) {
  const auto& primary_config = primary_configs_supported_.find(view_config);
  if (primary_config != primary_configs_supported_.end()) {
    return primary_config->second;
  }

  const auto& secondary_config = secondary_configs_supported_.find(view_config);
  // The view configuration type should have been validated by the caller.
  CHECK(secondary_config != secondary_configs_supported_.end());

  return secondary_config->second;
}

std::vector<XrViewConfigurationType> OpenXrTestHelper::SupportedViewConfigs()
    const {
  std::vector<XrViewConfigurationType> view_configs;
  for (auto& view_config : primary_configs_supported_) {
    view_configs.push_back(view_config.first);
  }
  for (auto& view_config : secondary_configs_supported_) {
    view_configs.push_back(view_config.first);
  }

  return view_configs;
}

XrResult OpenXrTestHelper::GetSecondaryConfigStates(
    uint32_t count,
    XrSecondaryViewConfigurationStateMSFT* states) const {
  // The number of secondary view configurations is the number of total views
  // minus the primary view configuration (there is always exactly one primary
  // config)
  RETURN_IF(count != view_configs_enabled_.size() - 1,
            XR_ERROR_SIZE_INSUFFICIENT,
            "XrSecondaryViewConfigurationFrameStateMSFT "
            "viewConfigurationCount insufficient");

  // Start at 1, since the primary view is always added first in BeginSession.
  for (uint32_t i = 1; i < view_configs_enabled_.size(); i++) {
    const device::OpenXrViewConfiguration& view_config =
        secondary_configs_supported_.find(view_configs_enabled_[i])->second;
    states[i - 1] = {XR_TYPE_SECONDARY_VIEW_CONFIGURATION_STATE_MSFT, nullptr,
                     view_config.Type(), view_config.Active()};
  }

  return XR_SUCCESS;
}

XrViewConfigurationType OpenXrTestHelper::PrimaryViewConfig() const {
  return view_configs_enabled_[0];
}

XrResult OpenXrTestHelper::BeginSession(
    const std::vector<XrViewConfigurationType>& view_configs) {
  RETURN_IF(IsSessionRunning(), XR_ERROR_SESSION_RUNNING,
            "Session is already running");
  RETURN_IF(session_state_ != XR_SESSION_STATE_READY,
            XR_ERROR_SESSION_NOT_READY,
            "Session is not XR_ERROR_SESSION_NOT_READY");

  // xrBeginSession in the fake OpenXR runtime should have added the primary
  // view configuration first.
  if (!base::Contains(primary_configs_supported_, view_configs[0])) {
    return XR_ERROR_VIEW_CONFIGURATION_TYPE_UNSUPPORTED;
  }

  // Add the primary view configuration first - this assumption is made in other
  // areas of the code.
  view_configs_enabled_.push_back(view_configs[0]);

  // Process the rest of the view configurations, which should all be secondary.
  for (uint32_t i = 1; i < view_configs.size(); i++) {
    if (!base::Contains(secondary_configs_supported_, view_configs[i])) {
      return XR_ERROR_VIEW_CONFIGURATION_TYPE_UNSUPPORTED;
    }

    // Check for additional primary view configuration.
    if (base::Contains(primary_configs_supported_, view_configs[i])) {
      return XR_ERROR_VALIDATION_FAILURE;
    }

    // Check for duplicates.
    if (base::Contains(view_configs_enabled_, view_configs[i])) {
      return XR_ERROR_VALIDATION_FAILURE;
    }

    view_configs_enabled_.push_back(view_configs[i]);
  }

  SetSessionState(XR_SESSION_STATE_FOCUSED);
  return XR_SUCCESS;
}

XrResult OpenXrTestHelper::EndSession() {
  // Per OpenXR 1.0 spec: "An application can only call xrEndSession when the
  // session is in the XR_SESSION_STATE_STOPPING state"
  RETURN_IF(session_state_ != XR_SESSION_STATE_STOPPING,
            XR_ERROR_SESSION_NOT_STOPPING,
            "Session state is not XR_ERROR_SESSION_NOT_STOPPING");
  SetSessionState(XR_SESSION_STATE_IDLE);
  return XR_SUCCESS;
}

XrResult OpenXrTestHelper::BeginFrame() {
  if (!IsSessionRunning()) {
    return XR_ERROR_SESSION_NOT_RUNNING;
  }

  if (frame_begin_) {
    return XR_FRAME_DISCARDED;
  }
  frame_begin_ = true;
  return XR_SUCCESS;
}

XrResult OpenXrTestHelper::EndFrame() {
  if (!IsSessionRunning()) {
    return XR_ERROR_SESSION_NOT_RUNNING;
  }

  if (!frame_begin_) {
    return XR_ERROR_CALL_ORDER_INVALID;
  }

  frame_begin_ = false;

  ++frame_count_;
  if (frame_count_ % 10 == 0 && view_configs_enabled_.size() > 1) {
    // Flip the active state of all secondary views every 10 frames
    for (uint32_t i = 1; i < view_configs_enabled_.size(); i++) {
      device::OpenXrViewConfiguration& view_config =
          secondary_configs_supported_.find(view_configs_enabled_[i])->second;
      view_config.SetActive(!view_config.Active());
    }

    // Re-create the D3D textures, since adding or removing a view will change
    // the width and height of the overall texture.
    ReinitializeTextures();
  }

  return XR_SUCCESS;
}

XrResult OpenXrTestHelper::BindActionAndPath(XrPath interaction_profile_path,
                                             XrActionSuggestedBinding binding) {
  ActionProperties& current_action = actions_[binding.action];
  current_action.profile_binding_map[interaction_profile_path] =
      binding.binding;
  return XR_SUCCESS;
}

void OpenXrTestHelper::AddDimensions(
    const device::OpenXrViewConfiguration& view_config,
    uint32_t& width,
    uint32_t& height) const {
  const std::vector<device::OpenXrViewProperties>& views =
      view_config.Properties();
  for (const device::OpenXrViewProperties& view : views) {
    width += view.Width();
    height = std::max(height, view.Height());
  }
}

void OpenXrTestHelper::ReinitializeTextures() {
  DCHECK(d3d_device_);

  uint32_t total_width = 0;
  uint32_t total_height = 0;

  // The first view config in the enabled list should always be the primary
  // view configuration.
  const auto primary =
      primary_configs_supported_.find(view_configs_enabled_[0]);
  CHECK(primary != primary_configs_supported_.end());
  AddDimensions(primary->second, total_width, total_height);

  // Add secondary views
  for (uint32_t i = 1; i < view_configs_enabled_.size(); i++) {
    // There shouldn't be any more primary views enabled.
    DCHECK(primary_configs_supported_.find(view_configs_enabled_[i]) ==
           primary_configs_supported_.end());
    const auto secondary =
        secondary_configs_supported_.find(view_configs_enabled_[i]);
    CHECK(secondary != secondary_configs_supported_.end());
    if (secondary->second.Active()) {
      AddDimensions(secondary->second, total_width, total_height);
    }
  }

  CreateTextures(total_width, total_height);
}

void OpenXrTestHelper::CreateTextures(uint32_t width, uint32_t height) {
  DCHECK(d3d_device_);
  textures_arr_.clear();

  D3D11_TEXTURE2D_DESC desc{};
  desc.Width = width;
  desc.Height = height;
  desc.MipLevels = 1;
  desc.ArraySize = 1;
  desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  desc.SampleDesc.Count = 1;
  desc.Usage = D3D11_USAGE_DEFAULT;
  desc.BindFlags = D3D11_BIND_RENDER_TARGET;

  for (uint32_t i = 0; i < kMinSwapchainBuffering; i++) {
    Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
    HRESULT hr = d3d_device_->CreateTexture2D(&desc, nullptr, &texture);
    DCHECK_EQ(hr, S_OK);

    textures_arr_.push_back(texture);
  }
}

void OpenXrTestHelper::SetD3DDevice(ID3D11Device* d3d_device) {
  DCHECK_EQ(d3d_device_, nullptr);
  DCHECK_NE(d3d_device, nullptr);
  d3d_device_ = d3d_device;

  // The device is set when the session is created. However, the view
  // configurations to enable are not specified until a session begins, so we
  // should use the default primary dimensions to create the textures. The width
  // is multiplied by 2 because WebXR uses a single double wide texture.
  CreateTextures(kPrimaryViewDimension * 2, kPrimaryViewDimension);
}

XrResult OpenXrTestHelper::AttachActionSets(
    const XrSessionActionSetsAttachInfo& attach_info) {
  RETURN_IF(attach_info.type != XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO,
            XR_ERROR_VALIDATION_FAILURE,
            "XrSessionActionSetsAttachInfo type invalid");
  RETURN_IF(attach_info.next != nullptr, XR_ERROR_VALIDATION_FAILURE,
            "XrSessionActionSetsAttachInfo next is not nullptr");
  if (attached_action_sets_.size() != 0) {
    return XR_ERROR_ACTIONSETS_ALREADY_ATTACHED;
  }

  for (uint32_t i = 0; i < attach_info.countActionSets; i++) {
    XrActionSet action_set = attach_info.actionSets[i];
    RETURN_IF_XR_FAILED(ValidateActionSet(action_set));
    attached_action_sets_[action_set] = action_sets_[action_set];
  }

  return XR_SUCCESS;
}

uint32_t OpenXrTestHelper::AttachedActionSetsSize() const {
  return attached_action_sets_.size();
}

XrResult OpenXrTestHelper::SyncActionData(XrActionSet action_set) {
  RETURN_IF_XR_FAILED(ValidateActionSet(action_set));
  RETURN_IF(ValidateActionSetNotAttached(action_set) !=
                XR_ERROR_ACTIONSETS_ALREADY_ATTACHED,
            XR_ERROR_ACTIONSET_NOT_ATTACHED,
            "XrActionSet has to be attached to the session before sync");
  const std::vector<XrAction>& actions = action_sets_[action_set];
  for (uint32_t i = 0; i < actions.size(); i++) {
    RETURN_IF_XR_FAILED(UpdateAction(actions[i]));
  }
  return XR_SUCCESS;
}

XrResult OpenXrTestHelper::UpdateAction(XrAction action) {
  RETURN_IF_XR_FAILED(ValidateAction(action));
  ActionProperties& cur_action_properties = actions_[action];
  XrPath interaction_profile_path = GetPath(interaction_profile_);

  if (cur_action_properties.profile_binding_map.count(
          interaction_profile_path) == 0) {
    // Only update actions that have binding for current interaction_profile_
    return XR_SUCCESS;
  }

  XrPath action_path =
      cur_action_properties.profile_binding_map[interaction_profile_path];
  std::string path_string = PathToString(action_path);

  bool support_path =
      PathContainsString(path_string, "/user/hand/left/input") ||
      PathContainsString(path_string, "/user/hand/right/input");
  RETURN_IF_FALSE(
      support_path, XR_ERROR_VALIDATION_FAILURE,
      "UpdateAction this action has a path that is not supported by test now");

  device::ControllerFrameData data = GetControllerDataFromPath(path_string);

  switch (cur_action_properties.type) {
    case XR_ACTION_TYPE_FLOAT_INPUT: {
      if (!(PathContainsString(path_string, "/trigger") ||
            PathContainsString(path_string, "/squeeze") ||
            PathContainsString(path_string, "/force") ||
            PathContainsString(path_string, "/value"))) {
        NOTREACHED() << "Found path with unsupported float action: "
                     << path_string;
      }
      float_action_states_[action].isActive = data.is_valid;
      break;
    }
    case XR_ACTION_TYPE_BOOLEAN_INPUT: {
      device::XrButtonId button_id = device::kMax;
      if (PathContainsString(path_string, "/trackpad/")) {
        button_id = device::kAxisTrackpad;
      } else if (PathContainsString(path_string, "/thumbstick/")) {
        button_id = device::kAxisThumbstick;
      } else if (PathContainsString(path_string, "/trigger/")) {
        button_id = device::kAxisTrigger;
      } else if (PathContainsString(path_string, "/squeeze/")) {
        button_id = device::kGrip;
      } else if (PathContainsString(path_string, "/menu/")) {
        button_id = device::kMenu;
      } else if (PathContainsString(path_string, "/select/")) {
        // for WMR simple controller select is mapped to test type trigger
        button_id = device::kAxisTrigger;
      } else if (PathContainsString(path_string, "/thumbrest/")) {
        button_id = device::kThumbRest;
      } else if (PathContainsString(path_string, "/a/")) {
        button_id = device::kA;
      } else if (PathContainsString(path_string, "/b/")) {
        button_id = device::kB;
      } else if (PathContainsString(path_string, "/x/")) {
        button_id = device::kX;
      } else if (PathContainsString(path_string, "/y/")) {
        button_id = device::kY;
      } else if (PathContainsString(path_string, "/shoulder/")) {
        button_id = device::kShoulder;
      } else if (PathContainsString(path_string, "/pinch_ext/")) {
        button_id = device::kAxisTrigger;
      } else if (PathContainsString(path_string, "/grasp_ext/")) {
        button_id = device::kGrip;
      } else {
        NOTREACHED() << "Unrecognized boolean button: " << path_string;
      }
      uint64_t button_mask = XrButtonMaskFromId(button_id);

      // This bool pressed is needed because XrActionStateBoolean.currentState
      // is XrBool32 which is uint32_t. And XrActionStateBoolean.currentState
      // won't behave correctly if we try to set it using an uint64_t value like
      // button_mask, like: boolean_action_states_[].currentState =
      // data.buttons_pressed & button_mask
      boolean_action_states_[action].isActive = data.is_valid;
      bool button_supported = data.supported_buttons & button_mask;

      if (PathContainsString(path_string, "/value") ||
          PathContainsString(path_string, "/click")) {
        bool pressed = data.buttons_pressed & button_mask;
        boolean_action_states_[action].currentState =
            button_supported && pressed;
      } else if (PathContainsString(path_string, "/touch")) {
        bool touched = data.buttons_touched & button_mask;
        boolean_action_states_[action].currentState =
            button_supported && touched;
      } else {
        NOTREACHED() << "Boolean actions only supports path string ends with "
                        "value, click, or touch";
      }
      break;
    }
    case XR_ACTION_TYPE_VECTOR2F_INPUT: {
      device::XrButtonId button_id = device::kMax;
      if (PathContainsString(path_string, "/trackpad")) {
        button_id = device::kAxisTrackpad;
      } else if (PathContainsString(path_string, "/thumbstick")) {
        button_id = device::kAxisThumbstick;
      } else {
        NOTREACHED() << "Path is " << path_string
                     << "But only Trackpad and thumbstick has 2d vector action";
      }
      uint64_t axis_mask = XrAxisOffsetFromId(button_id);
      v2f_action_states_[action].currentState.x = data.axis_data[axis_mask].x;
      // we have to negate y because webxr has different direction for y than
      // openxr
      v2f_action_states_[action].currentState.y = -data.axis_data[axis_mask].y;
      v2f_action_states_[action].isActive = data.is_valid;
      break;
    }
    case XR_ACTION_TYPE_POSE_INPUT: {
      pose_action_state_[action].isActive = data.is_valid;
      break;
    }
    default: {
      RETURN_IF_FALSE(false, XR_ERROR_VALIDATION_FAILURE,
                      "UpdateAction does not support this type of action");
      break;
    }
  }

  return XR_SUCCESS;
}

void OpenXrTestHelper::SetSessionState(XrSessionState state) {
  session_state_ = state;
  XrEventDataBuffer event_data;
  XrEventDataSessionStateChanged* event_data_ptr =
      reinterpret_cast<XrEventDataSessionStateChanged*>(&event_data);

  event_data_ptr->type = XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED;
  event_data_ptr->session = session_;
  event_data_ptr->state = session_state_;
  event_data_ptr->time = next_predicted_display_time_;

  event_queue_.push(event_data);
}

XrResult OpenXrTestHelper::PollEvent(XrEventDataBuffer* event_data) {
  RETURN_IF(event_data == nullptr, XR_ERROR_VALIDATION_FAILURE,
            "XrEventDataBuffer is nullptr");
  RETURN_IF_FALSE(event_data->type == XR_TYPE_EVENT_DATA_BUFFER,
                  XR_ERROR_VALIDATION_FAILURE,
                  "xrPollEvent event_data type invalid");
  UpdateEventQueue();
  if (!event_queue_.empty()) {
    *event_data = event_queue_.front();
    event_queue_.pop();
    return XR_SUCCESS;
  }

  return XR_EVENT_UNAVAILABLE;
}

const std::vector<Microsoft::WRL::ComPtr<ID3D11Texture2D>>&
OpenXrTestHelper::GetSwapchainTextures() const {
  return textures_arr_;
}

uint32_t OpenXrTestHelper::NextSwapchainImageIndex() {
  acquired_swapchain_texture_ =
      (acquired_swapchain_texture_ + 1) % textures_arr_.size();
  return acquired_swapchain_texture_;
}

XrTime OpenXrTestHelper::NextPredictedDisplayTime() {
  return ++next_predicted_display_time_;
}

void OpenXrTestHelper::UpdateEventQueue() {
  base::AutoLock auto_lock(lock_);
  if (test_hook_) {
    device_test::mojom::EventData data = {};
    do {
      data = test_hook_->WaitGetEventData();
      if (data.type == device_test::mojom::EventType::kSessionLost) {
        SetSessionState(XR_SESSION_STATE_STOPPING);
      } else if (data.type ==
                 device_test::mojom::EventType::kVisibilityVisibleBlurred) {
        // WebXR Visible-Blurred map to OpenXR Visible
        SetSessionState(XR_SESSION_STATE_VISIBLE);
      } else if (data.type == device_test::mojom::EventType::kInstanceLost) {
        XrEventDataBuffer event_data = {
            XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING};
        event_queue_.push(event_data);
      } else if (data.type ==
                 device_test::mojom::EventType::kInteractionProfileChanged) {
        UpdateInteractionProfile(data.interaction_profile);
        XrEventDataBuffer event_data;
        XrEventDataInteractionProfileChanged* interaction_profile_changed =
            reinterpret_cast<XrEventDataInteractionProfileChanged*>(
                &event_data);
        interaction_profile_changed->type =
            XR_TYPE_EVENT_DATA_INTERACTION_PROFILE_CHANGED;
        interaction_profile_changed->session = session_;
        event_queue_.push(event_data);
      } else if (data.type != device_test::mojom::EventType::kNoEvent) {
        NOTREACHED() << "Event changed event type not implemented for test";
      }
    } while (data.type != device_test::mojom::EventType::kNoEvent);
  }
}

std::optional<gfx::Transform> OpenXrTestHelper::GetPose() {
  base::AutoLock lock(lock_);
  if (test_hook_) {
    device::PoseFrameData pose_data = test_hook_->WaitGetPresentingPose();
    if (pose_data.is_valid) {
      return PoseFrameDataToTransform(pose_data);
    }
  }
  return std::nullopt;
}

std::optional<device::DeviceConfig> OpenXrTestHelper::GetDeviceConfig() {
  base::AutoLock lock(lock_);
  if (test_hook_) {
    return test_hook_->WaitGetDeviceConfig();
  }
  return std::nullopt;
}

bool OpenXrTestHelper::GetCanCreateSession() {
  base::AutoLock lock(lock_);
  if (test_hook_) {
    return test_hook_->WaitGetCanCreateSession();
  }

  // In the absence of a test hook telling us that we can't create a session;
  // assume that we can, as there's enough of a default implementation to do so.
  return true;
}

device::ControllerFrameData OpenXrTestHelper::GetControllerDataFromPath(
    std::string path_string) const {
  device::ControllerRole role;
  if (PathContainsString(path_string, "/user/hand/left/")) {
    role = device::kControllerRoleLeft;
  } else if (PathContainsString(path_string, "/user/hand/right/")) {
    role = device::kControllerRoleRight;
  } else {
    NOTREACHED()
        << "Currently Path should belong to either left or right, received: "
        << path_string;
  }
  device::ControllerFrameData data;
  for (uint32_t i = 0; i < data_arr_.size(); i++) {
    if (data_arr_[i].role == role) {
      data = data_arr_[i];
    }
  }
  return data;
}

bool OpenXrTestHelper::IsSessionRunning() const {
  return session_state_ == XR_SESSION_STATE_SYNCHRONIZED ||
         session_state_ == XR_SESSION_STATE_VISIBLE ||
         session_state_ == XR_SESSION_STATE_FOCUSED;
}

void OpenXrTestHelper::UpdateInteractionProfile(
    device::mojom::OpenXrInteractionProfileType type) {
  switch (type) {
    case device::mojom::OpenXrInteractionProfileType::kMicrosoftMotion:
      interaction_profile_ = device::kMicrosoftMotionInteractionProfilePath;
      break;
    case device::mojom::OpenXrInteractionProfileType::kKHRSimple:
      interaction_profile_ = device::kKHRSimpleInteractionProfilePath;
      break;
    case device::mojom::OpenXrInteractionProfileType::kOculusTouch:
      interaction_profile_ = device::kOculusTouchInteractionProfilePath;
      break;
    case device::mojom::OpenXrInteractionProfileType::kValveIndex:
      interaction_profile_ = device::kValveIndexInteractionProfilePath;
      break;
    case device::mojom::OpenXrInteractionProfileType::kHTCVive:
      interaction_profile_ = device::kHTCViveInteractionProfilePath;
      break;
    case device::mojom::OpenXrInteractionProfileType::kSamsungOdyssey:
      interaction_profile_ = device::kSamsungOdysseyInteractionProfilePath;
      break;
    case device::mojom::OpenXrInteractionProfileType::kHPReverbG2:
      interaction_profile_ = device::kHPReverbG2InteractionProfilePath;
      break;
    case device::mojom::OpenXrInteractionProfileType::kHandSelectGrasp:
      interaction_profile_ = device::kHandSelectGraspInteractionProfilePath;
      break;
    case device::mojom::OpenXrInteractionProfileType::kViveCosmos:
      interaction_profile_ = device::kHTCViveCosmosInteractionProfilePath;
      break;
    case device::mojom::OpenXrInteractionProfileType::kExtHand:
      interaction_profile_ = device::kExtHandInteractionProfilePath;
      break;
    case device::mojom::OpenXrInteractionProfileType::kInvalid:
    case device::mojom::OpenXrInteractionProfileType::kMetaHandAim:
      NOTREACHED() << "Invalid EventData interaction_profile type";
  }
}

void OpenXrTestHelper::LocateJoints(
    XrHandTrackerEXT hand_tracker,
    const XrHandJointsLocateInfoEXT* locate_info,
    XrHandJointLocationsEXT* locations) {
  DCHECK_NE(locations, nullptr);
  locations->isActive = false;
  std::string controller_string =
      left_hand_ == hand_tracker ? "/user/hand/left/" : "/user/hand/right/";
  const auto& controller =
      GetControllerDataFromPath(std::move(controller_string));
  if (!controller.has_hand_data) {
    return;
  }

  // Our test/mojom interface sends the "palm" joint separate from the rest of
  // the finger joints, and thus sends one less joint than we need to populate.
  if (std::size(controller.hand_data) + 1 > locations->jointCount) {
    return;
  }

  base::span<XrHandJointLocationEXT> out_locations{locations->jointLocations,
                                                   locations->jointCount};
  if (controller.pose_data.is_valid) {
    auto& palm_location = out_locations[0];
    palm_location.locationFlags = kValidTrackedPoseFlags;
    palm_location.radius = 1.0f;
    palm_location.pose = device::GfxTransformToXrPose(
        PoseFrameDataToTransform(controller.pose_data));
  }
  for (const auto& data : controller.hand_data) {
    if (!data.mojo_from_joint) {
      // If we're missing the pose, don't fill in any data about this joint.
      continue;
    }
    // The OpenXR joints and mojom joints have the same base number offset by 1.
    auto& joint_location = out_locations[static_cast<uint32_t>(data.joint) + 1];
    joint_location.locationFlags = kValidTrackedPoseFlags;
    joint_location.radius = data.radius;
    joint_location.pose =
        device::GfxTransformToXrPose(data.mojo_from_joint.value());
  }

  locations->isActive = true;
}

std::optional<gfx::Transform> OpenXrTestHelper::GetTransformForSpace(
    XrSpace space) {
  std::optional<gfx::Transform> transform = std::nullopt;

  if (reference_spaces_.count(space) == 1) {
    if (reference_spaces_.at(space).compare(kStageReferenceSpacePath) == 0) {
      // This locate space call wants the transform from local to stage which we
      // only need to give it identity matrix.
      transform = gfx::Transform();
    } else if (reference_spaces_.at(space).compare(kViewReferenceSpacePath) ==
               0) {
      // This locate space call wants the transform of the head pose.
      transform = GetPose();
    } else {
      NOTREACHED()
          << "Only locate reference space for local and view are implemented";
    }
  } else if (action_spaces_.count(space) == 1) {
    XrAction cur_action = action_spaces_.at(space);
    ActionProperties cur_action_properties = actions_[cur_action];
    std::string path_string =
        PathToString(cur_action_properties
                         .profile_binding_map[GetPath(interaction_profile_)]);
    device::ControllerFrameData data =
        GetControllerDataFromPath(std::move(path_string));
    if (data.pose_data.is_valid) {
      transform = PoseFrameDataToTransform(data.pose_data);
    }
  } else {
    NOTREACHED() << "Locate Space only supports reference space or action "
                    "space for controller";
  }

  return transform;
}

void OpenXrTestHelper::LocateSpace(XrSpace space, XrPosef* pose) {
  DCHECK_NE(pose, nullptr);
  if (auto transform = GetTransformForSpace(space); transform) {
    *pose = device::GfxTransformToXrPose(transform.value());
  } else {
    *pose = device::PoseIdentity();
  }
}

std::string OpenXrTestHelper::PathToString(XrPath path) const {
  return paths_[path - 1];
}

bool OpenXrTestHelper::UpdateData() {
  base::AutoLock auto_lock(lock_);
  if (test_hook_) {
    for (uint32_t i = 0; i < device::kMaxTrackedDevices; i++) {
      data_arr_[i] = test_hook_->WaitGetControllerData(i);
    }
    return true;
  }
  return false;
}

bool OpenXrTestHelper::UpdateViews(XrViewConfigurationType view_config_type,
                                   XrView views[],
                                   uint32_t size) {
  device::OpenXrViewConfiguration& view_config =
      GetViewConfigInfo(view_config_type);
  RETURN_IF(size != view_config.Views().size(), XR_ERROR_VALIDATION_FAILURE,
            "UpdateViews mismatched number of views");
  RETURN_IF(size != 1 && size != 2, XR_ERROR_VALIDATION_FAILURE,
            "UpdateViews only supports view configurations with 1 or 2 views");

  std::optional<gfx::Transform> pose = GetPose();
  std::optional<device::DeviceConfig> config = GetDeviceConfig();

  if (!pose.has_value() && !config.has_value()) {
    return true;
  }

  for (uint32_t i = 0; i < size; i++) {
    if (pose.has_value()) {
      views[i].pose = device::GfxTransformToXrPose(*pose);
    }
    if (config.has_value()) {
      // For view configurations with 2 views, assume they are the left and
      // right eye and set the X offset from zero to be half the IPD.
      // View configurations with 1 view are not necessarily always at zero, so
      // just also use half the IPD as an arbitrary offset to avoid adding
      // additional logic to set to zero.
      views[i].pose.position.x =
          config->interpupillary_distance / 2 * GetOffsetMultiplierForIndex(i);
    }
  }

  return true;
}

XrResult OpenXrTestHelper::ValidateAction(XrAction action) const {
  RETURN_IF(actions_.count(action) != 1, XR_ERROR_HANDLE_INVALID,
            "ValidateAction: Invalid Action");
  return XR_SUCCESS;
}

XrResult OpenXrTestHelper::ValidateActionCreateInfo(
    const XrActionCreateInfo& create_info) const {
  RETURN_IF(create_info.type != XR_TYPE_ACTION_CREATE_INFO,
            XR_ERROR_VALIDATION_FAILURE,
            "ValidateActionCreateInfo type invalid");
  RETURN_IF(create_info.next != nullptr, XR_ERROR_VALIDATION_FAILURE,
            "ValidateActionCreateInfo next is not nullptr");
  RETURN_IF(create_info.actionName[0] == '\0', XR_ERROR_NAME_INVALID,
            "ValidateActionCreateInfo actionName invalid");
  RETURN_IF(create_info.actionType == XR_ACTION_TYPE_MAX_ENUM,
            XR_ERROR_VALIDATION_FAILURE,
            "ValidateActionCreateInfo action type invalid");
  RETURN_IF(create_info.localizedActionName[0] == '\0',
            XR_ERROR_LOCALIZED_NAME_INVALID,
            "ValidateActionCreateInfo localizedActionName invalid");
  RETURN_IF(action_names_.count(create_info.actionName) != 0,
            XR_ERROR_NAME_DUPLICATED,
            "ValidateActionCreateInfo actionName duplicate");
  RETURN_IF(action_localized_names_.count(create_info.localizedActionName) != 0,
            XR_ERROR_LOCALIZED_NAME_DUPLICATED,
            "ValidateActionCreateInfo localizedActionName duplicate");
  RETURN_IF_FALSE(create_info.countSubactionPaths == 0 &&
                      create_info.subactionPaths == nullptr,
                  XR_ERROR_VALIDATION_FAILURE,
                  "ValidateActionCreateInfo has subactionPaths which is not "
                  "supported by current version of test.");
  return XR_SUCCESS;
}

XrResult OpenXrTestHelper::ValidateActionSet(XrActionSet action_set) const {
  RETURN_IF_FALSE(action_sets_.count(action_set), XR_ERROR_HANDLE_INVALID,
                  "ValidateActionSet: Invalid action_set");
  return XR_SUCCESS;
}

XrResult OpenXrTestHelper::ValidateActionSetCreateInfo(
    const XrActionSetCreateInfo& create_info) const {
  RETURN_IF(create_info.type != XR_TYPE_ACTION_SET_CREATE_INFO,
            XR_ERROR_VALIDATION_FAILURE,
            "ValidateActionSetCreateInfo type invalid");
  RETURN_IF(create_info.actionSetName[0] == '\0', XR_ERROR_NAME_INVALID,
            "ValidateActionSetCreateInfo actionSetName invalid");
  RETURN_IF(create_info.localizedActionSetName[0] == '\0',
            XR_ERROR_LOCALIZED_NAME_INVALID,
            "ValidateActionSetCreateInfo localizedActionSetName invalid");
  RETURN_IF(action_set_names_.count(create_info.actionSetName) != 0,
            XR_ERROR_NAME_DUPLICATED,
            "ValidateActionSetCreateInfo actionSetName duplicate");
  RETURN_IF(action_set_localized_names_.count(
                create_info.localizedActionSetName) != 0,
            XR_ERROR_LOCALIZED_NAME_DUPLICATED,
            "ValidateActionSetCreateInfo localizedActionSetName duplicate");
  RETURN_IF(create_info.priority != 0, XR_ERROR_VALIDATION_FAILURE,
            "ValidateActionSetCreateInfo has priority which is not supported "
            "by current version of test.");
  return XR_SUCCESS;
}

XrResult OpenXrTestHelper::ValidateActionSetNotAttached(
    XrActionSet action_set) const {
  if (attached_action_sets_.count(action_set) == 1)
    return XR_ERROR_ACTIONSETS_ALREADY_ATTACHED;
  return XR_SUCCESS;
}

XrResult OpenXrTestHelper::ValidateActionSpaceCreateInfo(
    const XrActionSpaceCreateInfo& create_info) const {
  RETURN_IF(create_info.type != XR_TYPE_ACTION_SPACE_CREATE_INFO,
            XR_ERROR_VALIDATION_FAILURE,
            "ValidateActionSpaceCreateInfo type invalid");
  RETURN_IF(create_info.next != nullptr, XR_ERROR_VALIDATION_FAILURE,
            "ValidateActionSpaceCreateInfo next is not nullptr");
  RETURN_IF_XR_FAILED(ValidateAction(create_info.action));
  ActionProperties cur_action_properties = actions_.at(create_info.action);
  if (cur_action_properties.type != XR_ACTION_TYPE_POSE_INPUT) {
    return XR_ERROR_ACTION_TYPE_MISMATCH;
  }
  RETURN_IF(create_info.subactionPath != XR_NULL_PATH,
            XR_ERROR_VALIDATION_FAILURE,
            "ValidateActionSpaceCreateInfo subactionPath != XR_NULL_PATH");
  RETURN_IF_XR_FAILED(ValidateXrPosefIsIdentity(create_info.poseInActionSpace));
  return XR_SUCCESS;
}

XrResult OpenXrTestHelper::ValidateHandTracker(
    XrHandTrackerEXT hand_tracker) const {
  RETURN_IF(left_hand_ == XR_NULL_HANDLE && right_hand_ == XR_NULL_HANDLE,
            XR_ERROR_HANDLE_INVALID, "No Hand Tracker has been created");
  RETURN_IF(left_hand_ != hand_tracker && right_hand_ != hand_tracker,
            XR_ERROR_HANDLE_INVALID, "Hand Tracker invalid");
  return XR_SUCCESS;
}

XrResult OpenXrTestHelper::ValidateInstance(XrInstance instance) const {
  // The Fake OpenXr Runtime returns this global OpenXrTestHelper object as the
  // instance value on xrCreateInstance.

  RETURN_IF(reinterpret_cast<OpenXrTestHelper*>(instance) != this,
            XR_ERROR_HANDLE_INVALID, "XrInstance invalid");

  return XR_SUCCESS;
}

XrResult OpenXrTestHelper::ValidateSystemId(XrSystemId system_id) const {
  RETURN_IF(system_id_ == 0, XR_ERROR_SYSTEM_INVALID,
            "XrSystemId has not been queried");
  RETURN_IF(system_id != system_id_, XR_ERROR_SYSTEM_INVALID,
            "XrSystemId invalid");

  return XR_SUCCESS;
}

XrResult OpenXrTestHelper::ValidateSession(XrSession session) const {
  RETURN_IF(session_ == XR_NULL_HANDLE, XR_ERROR_HANDLE_INVALID,
            "XrSession has not been queried");
  RETURN_IF(session != session_, XR_ERROR_HANDLE_INVALID, "XrSession invalid");

  return XR_SUCCESS;
}

XrResult OpenXrTestHelper::ValidateSwapchain(XrSwapchain swapchain) const {
  RETURN_IF(swapchain_ == XR_NULL_HANDLE, XR_ERROR_HANDLE_INVALID,
            "XrSwapchain has not been queried");
  RETURN_IF(swapchain != swapchain_, XR_ERROR_HANDLE_INVALID,
            "XrSwapchain invalid");

  return XR_SUCCESS;
}

XrResult OpenXrTestHelper::ValidateSpace(XrSpace space) const {
  RETURN_IF(space == XR_NULL_HANDLE, XR_ERROR_HANDLE_INVALID,
            "XrSpace has not been queried");
  RETURN_IF(
      reference_spaces_.count(space) != 1 && action_spaces_.count(space) != 1,
      XR_ERROR_HANDLE_INVALID, space);

  return XR_SUCCESS;
}

XrResult OpenXrTestHelper::ValidatePath(XrPath path) const {
  RETURN_IF(path > paths_.size(), XR_ERROR_PATH_INVALID, "XrPath invalid");
  return XR_SUCCESS;
}

XrResult OpenXrTestHelper::ValidatePredictedDisplayTime(XrTime time) const {
  RETURN_IF(time == 0, XR_ERROR_VALIDATION_FAILURE,
            "XrTime has not been queried");
  RETURN_IF(time > next_predicted_display_time_, XR_ERROR_VALIDATION_FAILURE,
            "XrTime predicted display time invalid");

  return XR_SUCCESS;
}

XrResult OpenXrTestHelper::ValidateXrCompositionLayerProjection(
    XrViewConfigurationType view_config,
    const XrCompositionLayerProjection& projection_layer) {
  // The caller should have validated the view configuration.
  RETURN_IF(projection_layer.type != XR_TYPE_COMPOSITION_LAYER_PROJECTION,
            XR_ERROR_LAYER_INVALID,
            "XrCompositionLayerProjection type invalid");
  RETURN_IF(projection_layer.next != nullptr, XR_ERROR_VALIDATION_FAILURE,
            "XrCompositionLayerProjection next is not nullptr");
  RETURN_IF(projection_layer.layerFlags != 0, XR_ERROR_VALIDATION_FAILURE,
            "XrCompositionLayerProjection layerflag is not 0");
  RETURN_IF(reference_spaces_.count(projection_layer.space) != 1,
            XR_ERROR_VALIDATION_FAILURE,
            "XrCompositionLayerProjection space is not reference space");
  std::string space_path = reference_spaces_.at(projection_layer.space);
  RETURN_IF(space_path.compare(kLocalReferenceSpacePath) != 0,
            XR_ERROR_VALIDATION_FAILURE,
            "XrCompositionLayerProjection space is not local space");
  RETURN_IF(projection_layer.views == nullptr, XR_ERROR_VALIDATION_FAILURE,
            "XrCompositionLayerProjection view is nullptr");

  const device::OpenXrViewConfiguration& config =
      GetViewConfigInfo(view_config);

  RETURN_IF(projection_layer.viewCount != config.Views().size(),
            XR_ERROR_VALIDATION_FAILURE,
            "XrCompositionLayerProjection viewCount invalid");

  for (uint32_t j = 0; j < projection_layer.viewCount; j++) {
    const XrCompositionLayerProjectionView& projection_view =
        projection_layer.views[j];
    RETURN_IF_XR_FAILED(ValidateXrCompositionLayerProjectionView(
        projection_view, projection_layer.viewCount, j));
  }

  return XR_SUCCESS;
}

XrResult OpenXrTestHelper::ValidateXrCompositionLayerProjectionView(
    const XrCompositionLayerProjectionView& view,
    uint32_t view_count,
    uint32_t index) {
  DCHECK_LE(view_count, 2u);
  DCHECK_LE(index, 2u);
  RETURN_IF(view.type != XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW,
            XR_ERROR_VALIDATION_FAILURE,
            "XrCompositionLayerProjectionView type invalid");
  RETURN_IF(view.next != nullptr, XR_ERROR_VALIDATION_FAILURE,
            "XrCompositionLayerProjectionView next is not nullptr");

  return XR_SUCCESS;
}

XrResult OpenXrTestHelper::ValidateXrPosefIsIdentity(
    const XrPosef& pose) const {
  XrPosef identity = device::PoseIdentity();
  bool is_identity = true;
  is_identity &= pose.orientation.x == identity.orientation.x;
  is_identity &= pose.orientation.y == identity.orientation.y;
  is_identity &= pose.orientation.z == identity.orientation.z;
  is_identity &= pose.orientation.w == identity.orientation.w;
  is_identity &= pose.position.x == identity.position.x;
  is_identity &= pose.position.y == identity.position.y;
  is_identity &= pose.position.z == identity.position.z;
  RETURN_IF_FALSE(is_identity, XR_ERROR_VALIDATION_FAILURE,
                  "XrPosef is not an identity");

  return XR_SUCCESS;
}

XrResult OpenXrTestHelper::ValidateViews(uint32_t view_capacity_input,
                                         XrView* views) const {
  RETURN_IF(views == nullptr, XR_ERROR_VALIDATION_FAILURE, "XrView is nullptr");
  for (uint32_t i = 0; i < view_capacity_input; i++) {
    XrView view = views[i];
    RETURN_IF_FALSE(view.type == XR_TYPE_VIEW, XR_ERROR_VALIDATION_FAILURE,
                    "XrView type invalid");
    RETURN_IF(view.next != nullptr, XR_ERROR_VALIDATION_FAILURE,
              "XrView next is not nullptr");
  }

  return XR_SUCCESS;
}

XrResult OpenXrTestHelper::ValidateViewConfigType(
    XrViewConfigurationType view_config) const {
  RETURN_IF(!base::Contains(primary_configs_supported_, view_config) &&
                !base::Contains(secondary_configs_supported_, view_config),
            XR_ERROR_VIEW_CONFIGURATION_TYPE_UNSUPPORTED,
            "XrViewConfigurationType unsupported");

  return XR_SUCCESS;
}
