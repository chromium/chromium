// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/openxr/openxr_api_wrapper.h"

#include <stdint.h>
#include <algorithm>
#include <array>

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/functional/callback_helpers.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/typed_macros.h"
#include "components/viz/common/gpu/context_provider.h"
#include "device/vr/openxr/openxr_extension_helper.h"
#include "device/vr/openxr/openxr_graphics_binding.h"
#include "device/vr/openxr/openxr_input_helper.h"
#include "device/vr/openxr/openxr_util.h"
#include "device/vr/public/cpp/features.h"
#include "device/vr/public/mojom/xr_session.mojom.h"
#include "device/vr/test/test_hook.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "third_party/openxr/src/include/openxr/openxr.h"
#include "ui/gfx/geometry/angle_conversions.h"
#include "ui/gfx/geometry/point3_f.h"
#include "ui/gfx/geometry/quaternion.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/transform.h"

#if BUILDFLAG(IS_WIN)
#include <dxgi1_2.h>

#include "gpu/ipc/common/gpu_memory_buffer_impl_dxgi.h"
#endif
namespace device {

namespace {

// The primary view configuration is always enabled and active in OpenXR. We
// currently only support the stereo view configuration.
static constexpr XrViewConfigurationType kPrimaryViewConfiguration =
    XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
// Secondary view configurations that we currently support. The OpenXR runtime
// must also support these for them to be enabled. There can be an arbitrary
// number of secondary views enabled.
static constexpr std::array<XrViewConfigurationType, 1>
    kSecondaryViewConfigurations = {
        XR_VIEW_CONFIGURATION_TYPE_SECONDARY_MONO_FIRST_PERSON_OBSERVER_MSFT,
};

// The number of views in the primary view configuration. Each frame must
// return at least this number of views, in addition to any secondary views
// that are enabled and active.
static constexpr uint32_t kNumPrimaryViews = 2;

// Per the OpenXR 1.0 spec for the XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO
// view configuration: View index 0 must represent the left eye and view index
// 1 must represent the right eye.
static constexpr uint32_t kLeftView = 0;
static constexpr uint32_t kRightView = 1;
// Since kNumPrimaryViews is used to size a vector that uses
// kLeftView/kRightView as indices, ensure that kNumPrimaryViews is greater
// than the largest index.
static_assert(kRightView < kNumPrimaryViews,
              "kNumPrimaryViews must be greater than kRightView");

// We can get into a state where frames are not requested, such as when the
// visibility state is hidden. Since OpenXR events are polled at the beginning
// of a frame, polling would not occur in this state. To ensure events are
// occasionally polled, a timer loop run every kTimeBetweenPollingEvents to poll
// events if significant time has elapsed since the last time events were
// polled.
constexpr base::TimeDelta kTimeBetweenPollingEvents = base::Seconds(1);

mojom::XREye GetEyeFromIndex(int i) {
  if (i == kLeftView) {
    return mojom::XREye::kLeft;
  } else if (i == kRightView) {
    return mojom::XREye::kRight;
  } else {
    return mojom::XREye::kNone;
  }
}

const char* GetXrSessionStateName(XrSessionState state) {
  switch (state) {
    case XR_SESSION_STATE_UNKNOWN:
      return "Unknown";
    case XR_SESSION_STATE_IDLE:
      return "Idle";
    case XR_SESSION_STATE_READY:
      return "Ready";
    case XR_SESSION_STATE_SYNCHRONIZED:
      return "Synchronized";
    case XR_SESSION_STATE_VISIBLE:
      return "Visible";
    case XR_SESSION_STATE_FOCUSED:
      return "Focused";
    case XR_SESSION_STATE_STOPPING:
      return "Stopping";
    case XR_SESSION_STATE_LOSS_PENDING:
      return "Loss_Pending";
    case XR_SESSION_STATE_EXITING:
      return "Exiting";
    case XR_SESSION_STATE_MAX_ENUM:
      return "Max_Enum";
  }

  NOTREACHED();
  return "Unknown";
}

}  // namespace

std::unique_ptr<OpenXrApiWrapper> OpenXrApiWrapper::Create(
    XrInstance instance,
    OpenXrGraphicsBinding* graphics_binding) {
  std::unique_ptr<OpenXrApiWrapper> openxr =
      std::make_unique<OpenXrApiWrapper>();

  if (!openxr->Initialize(instance, graphics_binding)) {
    return nullptr;
  }

  return openxr;
}

// static
XrResult OpenXrApiWrapper::GetSystem(XrInstance instance, XrSystemId* system) {
  XrSystemGetInfo system_info = {XR_TYPE_SYSTEM_GET_INFO};
  system_info.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
  return xrGetSystem(instance, &system_info, system);
}

// static
std::vector<XrEnvironmentBlendMode> OpenXrApiWrapper::GetSupportedBlendModes(
    XrInstance instance,
    XrSystemId system) {
  // Query the list of supported environment blend modes for the current system.
  uint32_t blend_mode_count;
  const XrViewConfigurationType kSupportedViewConfiguration =
      XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
  if (XR_FAILED(xrEnumerateEnvironmentBlendModes(instance, system,
                                                 kSupportedViewConfiguration, 0,
                                                 &blend_mode_count, nullptr))) {
    return {};  // empty vector
  }

  std::vector<XrEnvironmentBlendMode> environment_blend_modes(blend_mode_count);
  if (XR_FAILED(xrEnumerateEnvironmentBlendModes(
          instance, system, kSupportedViewConfiguration, blend_mode_count,
          &blend_mode_count, environment_blend_modes.data()))) {
    return {};  // empty vector
  }

  return environment_blend_modes;
}

OpenXrApiWrapper::OpenXrApiWrapper() = default;

OpenXrApiWrapper::~OpenXrApiWrapper() {
  Uninitialize();
}

void OpenXrApiWrapper::Reset() {
  SetXrSessionState(XR_SESSION_STATE_UNKNOWN);
  anchor_manager_.reset();
  unbounded_space_ = XR_NULL_HANDLE;
  local_space_ = XR_NULL_HANDLE;
  stage_space_ = XR_NULL_HANDLE;
  view_space_ = XR_NULL_HANDLE;
  color_swapchain_ = XR_NULL_HANDLE;
  ReleaseColorSwapchainImages();
  session_ = XR_NULL_HANDLE;
  blend_mode_ = XR_ENVIRONMENT_BLEND_MODE_MAX_ENUM;
  stage_bounds_ = {};
  system_ = XR_NULL_SYSTEM_ID;
  instance_ = XR_NULL_HANDLE;
  stage_parameters_enabled_ = false;
  enabled_features_.clear();
  graphics_binding_ = nullptr;

  primary_view_config_ = OpenXrViewConfiguration();
  secondary_view_configs_.clear();

  frame_state_ = {};
  input_helper_.reset();

  on_session_started_callback_.Reset();
  on_session_ended_callback_.Reset();
  visibility_changed_callback_.Reset();
}

bool OpenXrApiWrapper::Initialize(XrInstance instance,
                                  OpenXrGraphicsBinding* graphics_binding) {
  Reset();

  if (!graphics_binding) {
    return false;
  }

  graphics_binding_ = graphics_binding;
  session_running_ = false;
  pending_frame_ = false;

  DCHECK(instance != XR_NULL_HANDLE);
  instance_ = instance;

  DCHECK(HasInstance());

  if (XR_FAILED(InitializeSystem())) {
    return false;
  }

  if (!graphics_binding_->Initialize(instance_, system_)) {
    return false;
  }

  DCHECK(IsInitialized());

  if (test_hook_) {
    // Allow our mock implementation of OpenXr to be controlled by tests.
    // The mock implementation of xrCreateInstance returns a pointer to the
    // service test hook (g_test_helper) as the instance.
    service_test_hook_ = reinterpret_cast<ServiceTestHook*>(instance_);
    service_test_hook_->SetTestHook(test_hook_);

    test_hook_->AttachCurrentThread();
  }

  return true;
}

bool OpenXrApiWrapper::IsInitialized() const {
  return HasInstance() && HasSystem();
}

void OpenXrApiWrapper::Uninitialize() {
  // The instance is owned by the OpenXRDevice, so don't destroy it here.

  // Destroying an session in OpenXr also destroys all child objects of that
  // instance (including the swapchain, and spaces objects),
  // so they don't need to be manually destroyed.
  if (HasSession()) {
    xrDestroySession(session_);
  }

  if (test_hook_)
    test_hook_->DetachCurrentThread();

  if (on_session_ended_callback_) {
    on_session_ended_callback_.Run(ExitXrPresentReason::kOpenXrUninitialize);
  }

  // If we haven't reported that the session started yet, we need to report
  // that it failed, so that the browser doesn't think there's still a pending
  // session request, and can try again (though it may not recover).
  if (on_session_started_callback_) {
    std::move(on_session_started_callback_).Run(XR_ERROR_INITIALIZATION_FAILED);
  }

  Reset();
  session_running_ = false;
  pending_frame_ = false;
}

bool OpenXrApiWrapper::HasInstance() const {
  return instance_ != XR_NULL_HANDLE;
}

bool OpenXrApiWrapper::HasSystem() const {
  return system_ != XR_NULL_SYSTEM_ID && primary_view_config_.Initialized();
}

bool OpenXrApiWrapper::HasBlendMode() const {
  return blend_mode_ != XR_ENVIRONMENT_BLEND_MODE_MAX_ENUM;
}

bool OpenXrApiWrapper::HasSession() const {
  return session_ != XR_NULL_HANDLE;
}

bool OpenXrApiWrapper::HasColorSwapChain() const {
  return color_swapchain_ != XR_NULL_HANDLE && graphics_binding_ &&
         graphics_binding_->GetSwapChainImages().size() > 0;
}

bool OpenXrApiWrapper::HasSpace(XrReferenceSpaceType type) const {
  switch (type) {
    case XR_REFERENCE_SPACE_TYPE_LOCAL:
      return local_space_ != XR_NULL_HANDLE;
    case XR_REFERENCE_SPACE_TYPE_VIEW:
      return view_space_ != XR_NULL_HANDLE;
    case XR_REFERENCE_SPACE_TYPE_STAGE:
      return stage_space_ != XR_NULL_HANDLE;
    case XR_REFERENCE_SPACE_TYPE_UNBOUNDED_MSFT:
      return unbounded_space_ != XR_NULL_HANDLE;
    default:
      NOTREACHED();
      return false;
  }
}

bool OpenXrApiWrapper::HasFrameState() const {
  return frame_state_.type == XR_TYPE_FRAME_STATE;
}

XrResult OpenXrApiWrapper::InitializeViewConfig(
    XrViewConfigurationType type,
    OpenXrViewConfiguration& view_config) {
  std::vector<XrViewConfigurationView> view_properties;
  RETURN_IF_XR_FAILED(GetPropertiesForViewConfig(type, view_properties));
  view_config.Initialize(type, std::move(view_properties));

  return XR_SUCCESS;
}

XrResult OpenXrApiWrapper::GetPropertiesForViewConfig(
    XrViewConfigurationType type,
    std::vector<XrViewConfigurationView>& view_properties) const {
  uint32_t view_count;
  RETURN_IF_XR_FAILED(xrEnumerateViewConfigurationViews(
      instance_, system_, type, 0, &view_count, nullptr));

  view_properties.resize(view_count, {XR_TYPE_VIEW_CONFIGURATION_VIEW});
  RETURN_IF_XR_FAILED(
      xrEnumerateViewConfigurationViews(instance_, system_, type, view_count,
                                        &view_count, view_properties.data()));

  return XR_SUCCESS;
}

XrResult OpenXrApiWrapper::InitializeSystem() {
  DCHECK(HasInstance());
  DCHECK(!HasSystem());

  RETURN_IF_XR_FAILED(GetSystem(instance_, &system_));

  RETURN_IF_XR_FAILED(
      InitializeViewConfig(kPrimaryViewConfiguration, primary_view_config_));
  DCHECK_EQ(primary_view_config_.Properties().size(), kNumPrimaryViews);
  // The primary view configuration is the only one initially active
  primary_view_config_.SetActive(true);

  // Get the list of secondary view configurations that both we and the OpenXR
  // runtime support.
  uint32_t view_config_count;
  RETURN_IF_XR_FAILED(xrEnumerateViewConfigurations(
      instance_, system_, 0, &view_config_count, nullptr));

  std::vector<XrViewConfigurationType> view_config_types(view_config_count);
  RETURN_IF_XR_FAILED(xrEnumerateViewConfigurations(
      instance_, system_, view_config_count, &view_config_count,
      view_config_types.data()));

  for (const auto& view_config_type : kSecondaryViewConfigurations) {
    if (base::Contains(view_config_types, view_config_type)) {
      OpenXrViewConfiguration view_config;
      RETURN_IF_XR_FAILED(InitializeViewConfig(view_config_type, view_config));
      secondary_view_configs_.emplace(view_config_type, std::move(view_config));
    }
  }

  bool swapchain_size_updated = RecomputeSwapchainSizeAndViewports();
  DCHECK(swapchain_size_updated);

  return XR_SUCCESS;
}

device::mojom::XREnvironmentBlendMode OpenXrApiWrapper::GetMojoBlendMode(
    XrEnvironmentBlendMode xr_blend_mode) {
  switch (xr_blend_mode) {
    case XR_ENVIRONMENT_BLEND_MODE_OPAQUE:
      return device::mojom::XREnvironmentBlendMode::kOpaque;
    case XR_ENVIRONMENT_BLEND_MODE_ADDITIVE:
      return device::mojom::XREnvironmentBlendMode::kAdditive;
    case XR_ENVIRONMENT_BLEND_MODE_ALPHA_BLEND:
      return device::mojom::XREnvironmentBlendMode::kAlphaBlend;
    case XR_ENVIRONMENT_BLEND_MODE_MAX_ENUM:
      NOTREACHED();
  };
  return device::mojom::XREnvironmentBlendMode::kOpaque;
}

device::mojom::XREnvironmentBlendMode
OpenXrApiWrapper::PickEnvironmentBlendModeForSession(
    device::mojom::XRSessionMode session_mode) {
  DCHECK(HasInstance());
  std::vector<XrEnvironmentBlendMode> supported_blend_modes =
      GetSupportedBlendModes(instance_, system_);

  DCHECK(supported_blend_modes.size() > 0);

  blend_mode_ = supported_blend_modes[0];

  switch (session_mode) {
    case device::mojom::XRSessionMode::kImmersiveVr:
      if (base::Contains(supported_blend_modes,
                         XR_ENVIRONMENT_BLEND_MODE_OPAQUE))
        blend_mode_ = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
      break;
    case device::mojom::XRSessionMode::kImmersiveAr:
      // Prefer Alpha Blend when both Alpha Blend and Additive modes are
      // supported. This only concerns video see through devices with an
      // Additive compatibility mode
      if (base::Contains(supported_blend_modes,
                         XR_ENVIRONMENT_BLEND_MODE_ALPHA_BLEND)) {
        blend_mode_ = XR_ENVIRONMENT_BLEND_MODE_ALPHA_BLEND;
      } else if (base::Contains(supported_blend_modes,
                                XR_ENVIRONMENT_BLEND_MODE_ADDITIVE)) {
        blend_mode_ = XR_ENVIRONMENT_BLEND_MODE_ADDITIVE;
      }
      break;
    case device::mojom::XRSessionMode::kInline:
      NOTREACHED();
  }

  return GetMojoBlendMode(blend_mode_);
}

OpenXrAnchorManager* OpenXrApiWrapper::GetOrCreateAnchorManager(
    const OpenXrExtensionHelper& extension_helper) {
  if (session_ && !anchor_manager_) {
    anchor_manager_ = std::make_unique<OpenXrAnchorManager>(
        extension_helper, session_, local_space_);
  }
  return anchor_manager_.get();
}

bool OpenXrApiWrapper::UpdateAndGetSessionEnded() {
  // Ensure we have the latest state from the OpenXR runtime.
  if (XR_FAILED(ProcessEvents())) {
    DCHECK(!session_running_);
  }

  // This object is initialized at creation and uninitialized when the OpenXR
  // session has ended. Once uninitialized, this object is never re-initialized.
  // If a new session is requested by WebXR, a new object is created.
  return !IsInitialized();
}

OpenXRSceneUnderstandingManager*
OpenXrApiWrapper::GetOrCreateSceneUnderstandingManager(
    const OpenXrExtensionHelper& extension_helper) {
  if (session_ && !scene_understanding_manager_) {
    scene_understanding_manager_ =
        std::make_unique<OpenXRSceneUnderstandingManager>(
            extension_helper, session_, local_space_);
  }
  return scene_understanding_manager_.get();
}

// Callers of this function must check the XrResult return value and destroy
// this OpenXrApiWrapper object on failure to clean up any intermediate
// objects that may have been created before the failure.
XrResult OpenXrApiWrapper::InitSession(
    const std::unordered_set<mojom::XRSessionFeature>& enabled_features,
    const OpenXrExtensionHelper& extension_helper,
    SessionStartedCallback on_session_started_callback,
    SessionEndedCallback on_session_ended_callback,
    VisibilityChangedCallback visibility_changed_callback) {
  DCHECK(IsInitialized());

  enabled_features_ = enabled_features;

  // These are the only features that use stage parameters. If none of them were
  // requested for the session, we can avoid querying this every frame.
  stage_parameters_enabled_ = base::ranges::any_of(
      enabled_features_, [](mojom::XRSessionFeature feature) {
        return feature == mojom::XRSessionFeature::REF_SPACE_LOCAL_FLOOR ||
               feature == mojom::XRSessionFeature::REF_SPACE_BOUNDED_FLOOR ||
               feature == mojom::XRSessionFeature::ANCHORS;
      });

  on_session_started_callback_ = std::move(on_session_started_callback);
  on_session_ended_callback_ = std::move(on_session_ended_callback);
  visibility_changed_callback_ = std::move(visibility_changed_callback);

  RETURN_IF_XR_FAILED(CreateSession());
  RETURN_IF_XR_FAILED(CreateSwapchain());
  RETURN_IF_XR_FAILED(
      CreateSpace(XR_REFERENCE_SPACE_TYPE_LOCAL, &local_space_));
  RETURN_IF_XR_FAILED(CreateSpace(XR_REFERENCE_SPACE_TYPE_VIEW, &view_space_));

  // It's ok if stage_space_ fails since not all OpenXR devices are required to
  // support this reference space.
  CreateSpace(XR_REFERENCE_SPACE_TYPE_STAGE, &stage_space_);
  UpdateStageBounds();

  if (extension_helper.ExtensionEnumeration()->ExtensionSupported(
          XR_MSFT_UNBOUNDED_REFERENCE_SPACE_EXTENSION_NAME)) {
    RETURN_IF_XR_FAILED(
        CreateSpace(XR_REFERENCE_SPACE_TYPE_UNBOUNDED_MSFT, &unbounded_space_));
  }

  RETURN_IF_XR_FAILED(OpenXRInputHelper::CreateOpenXRInputHelper(
      instance_, system_, extension_helper, session_, local_space_,
      &input_helper_));

  // Make sure all of the objects we initialized are there.
  DCHECK(HasSession());
  DCHECK(HasColorSwapChain());
  DCHECK(HasSpace(XR_REFERENCE_SPACE_TYPE_LOCAL));
  DCHECK(HasSpace(XR_REFERENCE_SPACE_TYPE_VIEW));
  DCHECK(input_helper_);

  EnsureEventPolling();

  return XR_SUCCESS;
}

XrResult OpenXrApiWrapper::CreateSession() {
  DCHECK(!HasSession());
  DCHECK(IsInitialized());

  XrSessionCreateInfo session_create_info = {XR_TYPE_SESSION_CREATE_INFO};
  session_create_info.next = graphics_binding_->GetSessionCreateInfo();
  session_create_info.systemId = system_;

  return xrCreateSession(instance_, &session_create_info, &session_);
}

XrResult OpenXrApiWrapper::CreateSwapchain() {
  // TODO(https://crbug.com/1454936): Move CreateSwapchain (and related methods)
  // to the `OpenXrGraphicsBinding` instead of here.
  DCHECK(IsInitialized());
  DCHECK(HasSession());
  DCHECK(!HasColorSwapChain());
  DCHECK(graphics_binding_->GetSwapChainImages().empty());

  XrSwapchainCreateInfo swapchain_create_info = {XR_TYPE_SWAPCHAIN_CREATE_INFO};
  swapchain_create_info.arraySize = 1;
  swapchain_create_info.format =
      graphics_binding_->GetSwapchainFormat(session_);

  auto swapchain_image_size = graphics_binding_->GetSwapchainImageSize();
  swapchain_create_info.width = swapchain_image_size.width();
  swapchain_create_info.height = swapchain_image_size.height();
  swapchain_create_info.mipCount = 1;
  swapchain_create_info.faceCount = 1;
  swapchain_create_info.sampleCount = GetRecommendedSwapchainSampleCount();
  swapchain_create_info.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;

  XrSwapchain color_swapchain;
  RETURN_IF_XR_FAILED(
      xrCreateSwapchain(session_, &swapchain_create_info, &color_swapchain));

  color_swapchain_ = color_swapchain;

  RETURN_IF_XR_FAILED(
      graphics_binding_->EnumerateSwapchainImages(color_swapchain_));

  CreateSharedMailboxes();

  return XR_SUCCESS;
}

// Recomputes the size of the swapchain - the swapchain includes the primary
// views (left and right), as well as any active secondary views. Secondary
// views are only included when the OpenXR runtime reports that they're active.
// It's valid for a secondary view configuration to be enabled but not active.
// The viewports of all active views are also computed. The primary views are
// always at the beginning of the texture, followed by active secondary views.
// Unlike OpenXR which has separate swapchains for each view configuration,
// WebXR exposes a single framebuffer for all views, so we need to keep track of
// the viewports ourselves.
// Returns whether the swapchain size has changed.
bool OpenXrApiWrapper::RecomputeSwapchainSizeAndViewports() {
  uint32_t total_width = 0;
  uint32_t total_height = 0;
  for (const auto& view_properties : primary_view_config_.Properties()) {
    total_width += view_properties.recommendedImageRectWidth;
    total_height =
        std::max(total_height, view_properties.recommendedImageRectHeight);
  }
  primary_view_config_.SetViewport(0, 0, total_width, total_height);

  if (base::Contains(enabled_features_,
                     mojom::XRSessionFeature::SECONDARY_VIEWS)) {
    for (auto& secondary_view_config : secondary_view_configs_) {
      OpenXrViewConfiguration& view_config = secondary_view_config.second;
      if (view_config.Active()) {
        uint32_t view_width = 0;
        uint32_t view_height = 0;
        for (const auto& view_properties : view_config.Properties()) {
          view_width += view_properties.recommendedImageRectWidth;
          view_height =
              std::max(view_height, view_properties.recommendedImageRectHeight);
        }
        view_config.SetViewport(total_width, 0, view_width, view_height);
        total_width += view_width;
        total_height = std::max(total_height, view_height);
      }
    }
  }

  auto swapchain_image_size = graphics_binding_->GetSwapchainImageSize();
  if (swapchain_image_size.width() != static_cast<int>(total_width) ||
      swapchain_image_size.height() != static_cast<int>(total_height)) {
    graphics_binding_->SetSwapchainImageSize(
        gfx::Size(total_width, total_height));
    return true;
  }

  return false;
}

XrSpace OpenXrApiWrapper::GetReferenceSpace(
    device::mojom::XRReferenceSpaceType type) const {
  switch (type) {
    case device::mojom::XRReferenceSpaceType::kLocal:
      return local_space_;
    case device::mojom::XRReferenceSpaceType::kViewer:
      return view_space_;
    case device::mojom::XRReferenceSpaceType::kBoundedFloor:
      return stage_space_;
    case device::mojom::XRReferenceSpaceType::kUnbounded:
      return unbounded_space_;
      // Ignore local-floor as that has no direct space
    case device::mojom::XRReferenceSpaceType::kLocalFloor:
      return XR_NULL_HANDLE;
  }
}

// Based on the capabilities of the system and runtime, determine whether
// to use shared images to draw into OpenXR swap chain buffers.
bool OpenXrApiWrapper::ShouldCreateSharedImages() const {
  // TODO(https://crbug.com/1454940): Investigate moving the remaining Windows-
  // only checks out of this class and into the GraphicsBinding.
#if BUILDFLAG(IS_WIN)
  // ANGLE's render_to_texture extension on Windows fails to render correctly
  // for EGL images. Until that is fixed, we need to disable shared images if
  // CanEnableAntiAliasing is true.
  if (CanEnableAntiAliasing()) {
    return false;
  }

  // Since WebGL renders upside down, sharing images means the XR runtime
  // needs to be able to consume upside down images and flip them internally.
  // If it is unable to (fovMutable == XR_FALSE), we must gracefully fallback
  // to copying textures.
  XrViewConfigurationProperties view_configuration_props = {
      XR_TYPE_VIEW_CONFIGURATION_PROPERTIES};
  if (XR_FAILED(xrGetViewConfigurationProperties(instance_, system_,
                                                 primary_view_config_.Type(),
                                                 &view_configuration_props)) ||
      (view_configuration_props.fovMutable == XR_FALSE)) {
    return false;
  }
#endif

  return graphics_binding_->CanUseSharedImages();
}

void OpenXrApiWrapper::OnContextProviderCreated(
    scoped_refptr<viz::ContextProvider> context_provider) {
  // TODO(https://crbug.com/1454935): Move `context_provider_` to
  // `OpenXrGraphicsBinding`.
  // We need to store the context provider because the shared mailboxes are
  // re-created when secondary view configurations become active or non active.
  context_provider_ = std::move(context_provider);

  // Recreate shared mailboxes for the swapchain if necessary.
  CreateSharedMailboxes();
}

void OpenXrApiWrapper::OnContextProviderLost() {
  if (context_provider_ && graphics_binding_) {
    // Mark the shared mailboxes as invalid since the underlying GPU process
    // associated with them has gone down.
    for (SwapChainInfo& info : graphics_binding_->GetSwapChainImages()) {
      info.Clear();
    }
    context_provider_ = nullptr;
  }
}

void OpenXrApiWrapper::ReleaseColorSwapchainImages() {
  if (context_provider_ && graphics_binding_) {
    gpu::SharedImageInterface* shared_image_interface =
        context_provider_->SharedImageInterface();
    for (SwapChainInfo& info : graphics_binding_->GetSwapChainImages()) {
      if (shared_image_interface && !info.mailbox_holder.mailbox.IsZero() &&
          info.mailbox_holder.sync_token.HasData()) {
        shared_image_interface->DestroySharedImage(
            info.mailbox_holder.sync_token, info.mailbox_holder.mailbox);
      }
      info.Clear();
    }
  }

  if (graphics_binding_) {
    graphics_binding_->ClearSwapChainImages();
  }
}

void OpenXrApiWrapper::CreateSharedMailboxes() {
  if (!context_provider_ || !ShouldCreateSharedImages()) {
    return;
  }

  gpu::SharedImageInterface* shared_image_interface =
      context_provider_->SharedImageInterface();
  // Create the MailboxHolders for each texture in the swap chain
  graphics_binding_->CreateSharedImages(shared_image_interface);
}

bool OpenXrApiWrapper::IsUsingSharedImages() const {
  if (!graphics_binding_) {
    return false;
  }

  const auto swapchain_info = graphics_binding_->GetSwapChainImages();
  return ((swapchain_info.size() > 1) &&
          !swapchain_info[0].mailbox_holder.mailbox.IsZero());
}

XrResult OpenXrApiWrapper::CreateSpace(XrReferenceSpaceType type,
                                       XrSpace* space) {
  DCHECK(HasSession());
  DCHECK(!HasSpace(type));

  XrReferenceSpaceCreateInfo space_create_info = {
      XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
  space_create_info.referenceSpaceType = type;
  space_create_info.poseInReferenceSpace = PoseIdentity();

  return xrCreateReferenceSpace(session_, &space_create_info, space);
}

XrResult OpenXrApiWrapper::BeginSession() {
  DCHECK(HasSession());
  DCHECK(on_session_started_callback_);

  XrSessionBeginInfo session_begin_info = {XR_TYPE_SESSION_BEGIN_INFO};
  session_begin_info.primaryViewConfigurationType = primary_view_config_.Type();

  XrSecondaryViewConfigurationSessionBeginInfoMSFT secondary_view_config_info =
      {XR_TYPE_SECONDARY_VIEW_CONFIGURATION_SESSION_BEGIN_INFO_MSFT};
  std::vector<XrViewConfigurationType> secondary_view_config_types;
  if (base::Contains(enabled_features_,
                     mojom::XRSessionFeature::SECONDARY_VIEWS)) {
    secondary_view_config_types.reserve(secondary_view_configs_.size());
    for (const auto& secondary_view_config : secondary_view_configs_) {
      secondary_view_config_types.emplace_back(secondary_view_config.first);
    }
    secondary_view_config_info.viewConfigurationCount =
        secondary_view_config_types.size();
    secondary_view_config_info.enabledViewConfigurationTypes =
        secondary_view_config_types.data();
    session_begin_info.next = &secondary_view_config_info;
  }

  XrResult xr_result = xrBeginSession(session_, &session_begin_info);
  if (XR_SUCCEEDED(xr_result))
    session_running_ = true;

  std::move(on_session_started_callback_).Run(xr_result);

  return xr_result;
}

XrResult OpenXrApiWrapper::BeginFrame() {
  DCHECK(HasSession());
  DCHECK(HasColorSwapChain());

  if (!session_running_)
    return XR_ERROR_SESSION_NOT_RUNNING;

  XrFrameWaitInfo wait_frame_info = {XR_TYPE_FRAME_WAIT_INFO};
  XrFrameState frame_state = {XR_TYPE_FRAME_STATE};

  XrSecondaryViewConfigurationFrameStateMSFT secondary_view_frame_states = {
      XR_TYPE_SECONDARY_VIEW_CONFIGURATION_FRAME_STATE_MSFT};
  std::vector<XrSecondaryViewConfigurationStateMSFT>
      secondary_view_config_states;
  if (base::Contains(enabled_features_,
                     mojom::XRSessionFeature::SECONDARY_VIEWS)) {
    secondary_view_config_states.resize(
        secondary_view_configs_.size(),
        {XR_TYPE_SECONDARY_VIEW_CONFIGURATION_STATE_MSFT});
    secondary_view_frame_states.viewConfigurationCount =
        secondary_view_config_states.size();
    secondary_view_frame_states.viewConfigurationStates =
        secondary_view_config_states.data();
    frame_state.next = &secondary_view_frame_states;
  }

  RETURN_IF_XR_FAILED(xrWaitFrame(session_, &wait_frame_info, &frame_state));
  frame_state_ = frame_state;

  if (base::Contains(enabled_features_,
                     mojom::XRSessionFeature::SECONDARY_VIEWS)) {
    RETURN_IF_XR_FAILED(
        UpdateSecondaryViewConfigStates(secondary_view_config_states));
  }

  XrFrameBeginInfo begin_frame_info = {XR_TYPE_FRAME_BEGIN_INFO};
  RETURN_IF_XR_FAILED(xrBeginFrame(session_, &begin_frame_info));
  pending_frame_ = true;

  RETURN_IF_XR_FAILED(graphics_binding_->ActivateSwapchainImage(
      color_swapchain_, context_provider_->SharedImageInterface()));

  RETURN_IF_XR_FAILED(UpdateViewConfigurations());

  return XR_SUCCESS;
}

XrResult OpenXrApiWrapper::UpdateViewConfigurations() {
  // While secondary views are only active when reported by the OpenXR runtime,
  // the primary view configuration must always be active.
  DCHECK(primary_view_config_.Active());
  DCHECK(!primary_view_config_.Viewport().IsEmpty());

  RETURN_IF_XR_FAILED(
      LocateViews(XR_REFERENCE_SPACE_TYPE_LOCAL, primary_view_config_));
  RETURN_IF_XR_FAILED(PrepareViewConfigForRender(primary_view_config_));

  if (base::Contains(enabled_features_,
                     mojom::XRSessionFeature::SECONDARY_VIEWS)) {
    for (auto& view_config : secondary_view_configs_) {
      OpenXrViewConfiguration& config = view_config.second;
      if (config.Active()) {
        RETURN_IF_XR_FAILED(LocateViews(XR_REFERENCE_SPACE_TYPE_LOCAL, config));
        RETURN_IF_XR_FAILED(PrepareViewConfigForRender(config));
      }
    }
  }

  return XR_SUCCESS;
}

// Updates the states of secondary views, which can become active or inactive on
// each frame. If the state of any secondary views have changed, the size of the
// swapchain has also likely changed, so re-create the swapchain.
XrResult OpenXrApiWrapper::UpdateSecondaryViewConfigStates(
    const std::vector<XrSecondaryViewConfigurationStateMSFT>& states) {
  DCHECK(base::Contains(enabled_features_,
                        mojom::XRSessionFeature::SECONDARY_VIEWS));

  bool state_changed = false;
  for (const XrSecondaryViewConfigurationStateMSFT& state : states) {
    DCHECK(
        base::Contains(secondary_view_configs_, state.viewConfigurationType));
    OpenXrViewConfiguration& view_config =
        secondary_view_configs_.at(state.viewConfigurationType);

    if (view_config.Active() != state.active) {
      state_changed = true;
      view_config.SetActive(state.active);
      if (view_config.Active()) {
        // When a secondary view configuration is activated, its properties
        // (such as recommended width/height) may have changed, so re-query the
        // properties.
        std::vector<XrViewConfigurationView> view_properties;
        RETURN_IF_XR_FAILED(GetPropertiesForViewConfig(
            state.viewConfigurationType, view_properties));
        view_config.SetProperties(std::move(view_properties));
      }
    }
  }

  // If the state of any secondary views have changed, the size of the swapchain
  // has likely changed. If the swapchain size has changed, we need to re-create
  // the swapchain.
  if (state_changed && RecomputeSwapchainSizeAndViewports()) {
    if (color_swapchain_) {
      RETURN_IF_XR_FAILED(xrDestroySwapchain(color_swapchain_));
      color_swapchain_ = XR_NULL_HANDLE;
      ReleaseColorSwapchainImages();
    }
    RETURN_IF_XR_FAILED(CreateSwapchain());
  }

  return XR_SUCCESS;
}

// Sets the layers for each view in the view configuration, which are submitted
// back to OpenXR on xrEndFrame. This is where we specify where in the texture
// each view is, as well as the properties of the views.
XrResult OpenXrApiWrapper::PrepareViewConfigForRender(
    OpenXrViewConfiguration& view_config) {
  DCHECK(view_config.Active());

  uint32_t x_offset = view_config.Viewport().x();
  for (uint32_t view_index = 0; view_index < view_config.Views().size();
       view_index++) {
    const XrView& view = view_config.Views()[view_index];

    XrCompositionLayerProjectionView& projection_view =
        view_config.GetProjectionView(view_index);
    const XrViewConfigurationView& properties =
        view_config.Properties()[view_index];
    projection_view.type = XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW;
    projection_view.pose = view.pose;
    projection_view.fov.angleLeft = view.fov.angleLeft;
    projection_view.fov.angleRight = view.fov.angleRight;
    projection_view.subImage.swapchain = color_swapchain_;
    // Since we're in double wide mode, the texture array only has one texture
    // and is always index 0. If secondary views are enabled, those views are
    // also in this same texture array.
    projection_view.subImage.imageArrayIndex = 0;
    projection_view.subImage.imageRect.extent.width =
        properties.recommendedImageRectWidth;
    projection_view.subImage.imageRect.extent.height =
        properties.recommendedImageRectHeight;
    projection_view.subImage.imageRect.offset.x = x_offset;
    x_offset += properties.recommendedImageRectWidth;

    if (IsUsingSharedImages()) {
      // WebGL layers always give us flipped content. We need to instruct OpenXR
      // to flip the content before showing it to the user. Some XR runtimes
      // are able to efficiently do this as part of existing post processing
      // steps.
      projection_view.subImage.imageRect.offset.y = 0;
      projection_view.fov.angleUp = view.fov.angleDown;
      projection_view.fov.angleDown = view.fov.angleUp;
    } else {
      projection_view.subImage.imageRect.offset.y =
          graphics_binding_->GetSwapchainImageSize().height() -
          properties.recommendedImageRectHeight;
      projection_view.fov.angleUp = view.fov.angleUp;
      projection_view.fov.angleDown = view.fov.angleDown;
    }
  }

  return XR_SUCCESS;
}

XrResult OpenXrApiWrapper::EndFrame() {
  DCHECK(pending_frame_);
  DCHECK(HasBlendMode());
  DCHECK(HasSession());
  DCHECK(HasColorSwapChain());
  DCHECK(HasSpace(XR_REFERENCE_SPACE_TYPE_LOCAL));
  DCHECK(HasFrameState());

  RETURN_IF_XR_FAILED(
      graphics_binding_->ReleaseActiveSwapchainImage(color_swapchain_));

  // Each view configuration has its own layer, which was populated in
  // PrepareViewConfigForRender. These layers are all put into XrFrameEndInfo
  // and passed to xrEndFrame.
  OpenXrLayers layers(local_space_, blend_mode_,
                      primary_view_config_.ProjectionViews());

  // Gather all the layers for active secondary views.
  if (base::Contains(enabled_features_,
                     mojom::XRSessionFeature::SECONDARY_VIEWS)) {
    for (const auto& secondary_view_config : secondary_view_configs_) {
      const OpenXrViewConfiguration& view_config = secondary_view_config.second;
      if (view_config.Active()) {
        layers.AddSecondaryLayerForType(view_config.Type(),
                                        view_config.ProjectionViews());
      }
    }
  }

  XrFrameEndInfo end_frame_info = {XR_TYPE_FRAME_END_INFO};
  end_frame_info.layerCount = layers.PrimaryLayerCount();
  end_frame_info.layers = layers.PrimaryLayerData();
  end_frame_info.displayTime = frame_state_.predictedDisplayTime;
  end_frame_info.environmentBlendMode = blend_mode_;

  XrSecondaryViewConfigurationFrameEndInfoMSFT secondary_view_end_frame_info = {
      XR_TYPE_SECONDARY_VIEW_CONFIGURATION_FRAME_END_INFO_MSFT};
  if (layers.SecondaryConfigCount() > 0) {
    secondary_view_end_frame_info.viewConfigurationCount =
        layers.SecondaryConfigCount();
    secondary_view_end_frame_info.viewConfigurationLayersInfo =
        layers.SecondaryConfigData();

    end_frame_info.next = &secondary_view_end_frame_info;
  }

  RETURN_IF_XR_FAILED(xrEndFrame(session_, &end_frame_info));
  pending_frame_ = false;

  return XR_SUCCESS;
}

bool OpenXrApiWrapper::HasPendingFrame() const {
  return pending_frame_;
}

XrResult OpenXrApiWrapper::LocateViews(
    XrReferenceSpaceType space_type,
    OpenXrViewConfiguration& view_config) const {
  DCHECK(HasSession());

  XrViewState view_state = {XR_TYPE_VIEW_STATE};
  XrViewLocateInfo view_locate_info = {XR_TYPE_VIEW_LOCATE_INFO};
  view_locate_info.viewConfigurationType = view_config.Type();
  view_locate_info.displayTime = frame_state_.predictedDisplayTime;

  switch (space_type) {
    case XR_REFERENCE_SPACE_TYPE_LOCAL:
      view_locate_info.space = local_space_;
      break;
    case XR_REFERENCE_SPACE_TYPE_VIEW:
      view_locate_info.space = view_space_;
      break;
    case XR_REFERENCE_SPACE_TYPE_STAGE:
    case XR_REFERENCE_SPACE_TYPE_UNBOUNDED_MSFT:
    case XR_REFERENCE_SPACE_TYPE_COMBINED_EYE_VARJO:
    case XR_REFERENCE_SPACE_TYPE_MAX_ENUM:
    case XR_REFERENCE_SPACE_TYPE_LOCAL_FLOOR_EXT:
      NOTREACHED();
  }

  // Initialize the XrView objects' type field to XR_TYPE_VIEW. xrLocateViews
  // fails validation if this isn't set.
  std::vector<XrView> new_views(view_config.Views().size(), {XR_TYPE_VIEW});
  uint32_t view_count;
  RETURN_IF_XR_FAILED(xrLocateViews(session_, &view_locate_info, &view_state,
                                    new_views.size(), &view_count,
                                    new_views.data()));
  DCHECK_EQ(view_count, view_config.Views().size());

  // If the position or orientation is not valid, don't update the views so that
  // the previous valid views are used instead.
  if ((view_state.viewStateFlags & XR_VIEW_STATE_POSITION_VALID_BIT) &&
      (view_state.viewStateFlags & XR_VIEW_STATE_ORIENTATION_VALID_BIT)) {
    view_config.SetViews(std::move(new_views));
  }

  return XR_SUCCESS;
}

// Returns the next predicted display time in nanoseconds.
XrTime OpenXrApiWrapper::GetPredictedDisplayTime() const {
  DCHECK(IsInitialized());
  DCHECK(HasFrameState());

  return frame_state_.predictedDisplayTime;
}

mojom::XRViewPtr OpenXrApiWrapper::CreateView(
    const OpenXrViewConfiguration& view_config,
    uint32_t view_index,
    mojom::XREye eye,
    uint32_t x_offset) const {
  const XrView& xr_view = view_config.Views()[view_index];

  mojom::XRViewPtr view = mojom::XRView::New();
  view->eye = eye;
  view->mojo_from_view = XrPoseToGfxTransform(xr_view.pose);

  view->field_of_view = mojom::VRFieldOfView::New();
  view->field_of_view->up_degrees = gfx::RadToDeg(xr_view.fov.angleUp);
  view->field_of_view->down_degrees = gfx::RadToDeg(-xr_view.fov.angleDown);
  view->field_of_view->left_degrees = gfx::RadToDeg(-xr_view.fov.angleLeft);
  view->field_of_view->right_degrees = gfx::RadToDeg(xr_view.fov.angleRight);

  view->viewport = gfx::Rect(
      x_offset, 0,
      view_config.Properties()[view_index].recommendedImageRectWidth,
      view_config.Properties()[view_index].recommendedImageRectHeight);

  view->is_first_person_observer =
      view_config.Type() ==
      XR_VIEW_CONFIGURATION_TYPE_SECONDARY_MONO_FIRST_PERSON_OBSERVER_MSFT;

  return view;
}

std::vector<mojom::XRViewPtr> OpenXrApiWrapper::GetViews() const {
  // Since WebXR expects all views to be defined in a single swapchain texture,
  // we need to compute where in the texture each view begins. Each view is
  // located horizontally one after another, starting with the primary views,
  // followed by the secondary views. x_offset keeps track of where the next
  // view begins.
  uint32_t x_offset = primary_view_config_.Viewport().x();

  std::vector<mojom::XRViewPtr> views;
  for (size_t i = 0; i < primary_view_config_.Views().size(); i++) {
    views.emplace_back(
        CreateView(primary_view_config_, i, GetEyeFromIndex(i), x_offset));
    x_offset += primary_view_config_.Properties()[i].recommendedImageRectWidth;
  }

  if (base::Contains(enabled_features_,
                     mojom::XRSessionFeature::SECONDARY_VIEWS)) {
    for (const auto& secondary_view_config : secondary_view_configs_) {
      const OpenXrViewConfiguration& view_config = secondary_view_config.second;
      if (view_config.Active()) {
        x_offset = view_config.Viewport().x();
        for (size_t i = 0; i < view_config.Views().size(); i++) {
          views.emplace_back(
              CreateView(view_config, i, mojom::XREye::kNone, x_offset));
          x_offset += view_config.Properties()[i].recommendedImageRectWidth;
        }
      }
    }
  }

  return views;
}

std::vector<mojom::XRViewPtr> OpenXrApiWrapper::GetDefaultViews() const {
  DCHECK(IsInitialized());

  std::vector<XrViewConfigurationView> view_properties =
      primary_view_config_.Properties();
  CHECK_EQ(view_properties.size(), kNumPrimaryViews);

  std::vector<mojom::XRViewPtr> views(view_properties.size());
  uint32_t x_offset = 0;

  for (uint32_t i = 0; i < views.size(); i++) {
    views[i] = mojom::XRView::New();
    mojom::XRView* view = views[i].get();

    view->eye = GetEyeFromIndex(i);
    view->viewport =
        gfx::Rect(x_offset, 0, view_properties[i].recommendedImageRectWidth,
                  view_properties[i].recommendedImageRectHeight);
    view->field_of_view = mojom::VRFieldOfView::New(45.0f, 45.0f, 45.0f, 45.0f);

    x_offset += view_properties[i].recommendedImageRectWidth;
  }

  return views;
}

mojom::VRPosePtr OpenXrApiWrapper::GetViewerPose() const {
  XrSpaceLocation local_from_viewer = {XR_TYPE_SPACE_LOCATION};
  if (XR_FAILED(xrLocateSpace(view_space_, local_space_,
                              frame_state_.predictedDisplayTime,
                              &local_from_viewer))) {
    // We failed to locate the space, so just return nullptr to indicate that
    // we don't have tracking.
    return nullptr;
  }

  const auto& pose_state = local_from_viewer.locationFlags;
  const bool orientation_valid =
      pose_state & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT;
  const bool orientation_tracked =
      pose_state & XR_SPACE_LOCATION_ORIENTATION_TRACKED_BIT;
  const bool position_valid = pose_state & XR_SPACE_LOCATION_POSITION_VALID_BIT;
  const bool position_tracked =
      pose_state & XR_SPACE_LOCATION_POSITION_TRACKED_BIT;

  // emulated_position indicates when there is a fallback from a fully-tracked
  // (i.e. 6DOF) type case to some form of orientation-only type tracking
  // (i.e. 3DOF/IMU type sensors)
  // Thus we have to make sure orientation is tracked to send up a valid pose;
  // but we can send up a non tracked position, we just have to indicate that it
  // is emulated.
  const bool can_send_orientation = orientation_valid && orientation_tracked;
  const bool can_send_position = position_valid;

  // If we'd end up leaving both pose and orientation unset just return nullptr.
  if (!can_send_orientation && !can_send_position) {
    return nullptr;
  }

  mojom::VRPosePtr pose = mojom::VRPose::New();
  if (can_send_orientation) {
    pose->orientation = gfx::Quaternion(local_from_viewer.pose.orientation.x,
                                        local_from_viewer.pose.orientation.y,
                                        local_from_viewer.pose.orientation.z,
                                        local_from_viewer.pose.orientation.w);
  }

  if (can_send_position) {
    pose->position = gfx::Point3F(local_from_viewer.pose.position.x,
                                  local_from_viewer.pose.position.y,
                                  local_from_viewer.pose.position.z);
  }

  // Position is emulated if it isn't tracked.
  pose->emulated_position = !position_tracked;

  return pose;
}

std::vector<mojom::XRInputSourceStatePtr> OpenXrApiWrapper::GetInputState(
    bool hand_input_enabled) {
  return input_helper_->GetInputState(hand_input_enabled,
                                      GetPredictedDisplayTime());
}

void OpenXrApiWrapper::EnsureEventPolling() {
  // Events are usually processed at the beginning of a frame. When frames
  // aren't being requested, this timer loop ensures OpenXR events are
  // occasionally polled while OpenXR is active.
  if (IsInitialized()) {
    if (XR_FAILED(ProcessEvents())) {
      DCHECK(!session_running_);
    }

    // Verify that OpenXR is still active after processing events.
    if (IsInitialized()) {
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&OpenXrApiWrapper::EnsureEventPolling,
                         weak_ptr_factory_.GetWeakPtr()),
          kTimeBetweenPollingEvents);
    }
  }
}

XrResult OpenXrApiWrapper::ProcessEvents() {
  // If we've received an exit gesture from any of the input sources, end the
  // session.
  if (input_helper_->ReceivedExitGesture()) {
    XrResult xr_result = xrEndSession(session_);
    Uninitialize();
    return xr_result;
  }

  XrEventDataBuffer event_data{XR_TYPE_EVENT_DATA_BUFFER};
  XrResult xr_result = xrPollEvent(instance_, &event_data);

  while (XR_SUCCEEDED(xr_result) && xr_result != XR_EVENT_UNAVAILABLE) {
    if (event_data.type == XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED) {
      XrEventDataSessionStateChanged* session_state_changed =
          reinterpret_cast<XrEventDataSessionStateChanged*>(&event_data);
      // We only have will only have one session and we should make sure the
      // session that is having state_changed event is ours.
      DCHECK(session_state_changed->session == session_);
      SetXrSessionState(session_state_changed->state);
      switch (session_state_changed->state) {
        case XR_SESSION_STATE_READY:
          xr_result = BeginSession();
          break;
        case XR_SESSION_STATE_STOPPING:
          xr_result = xrEndSession(session_);
          Uninitialize();
          return xr_result;
        case XR_SESSION_STATE_SYNCHRONIZED:
          visibility_changed_callback_.Run(
              device::mojom::XRVisibilityState::HIDDEN);
          break;
        case XR_SESSION_STATE_VISIBLE:
          visibility_changed_callback_.Run(
              device::mojom::XRVisibilityState::VISIBLE_BLURRED);
          break;
        case XR_SESSION_STATE_FOCUSED:
          visibility_changed_callback_.Run(
              device::mojom::XRVisibilityState::VISIBLE);
          break;
        case XR_SESSION_STATE_EXITING:
          Uninitialize();
          return xr_result;
        default:
          break;
      }
    } else if (event_data.type == XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING) {
      DCHECK(session_ != XR_NULL_HANDLE);
      // TODO(https://crbug.com/1335240): Properly handle Instance Loss Pending.
      LOG(ERROR) << "Received Instance Loss Event";
      TRACE_EVENT_INSTANT0("xr", "InstanceLossPendingEvent",
                           TRACE_EVENT_SCOPE_THREAD);
      Uninitialize();
      return XR_ERROR_INSTANCE_LOST;
    } else if (event_data.type ==
               XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING) {
      XrEventDataReferenceSpaceChangePending* reference_space_change_pending =
          reinterpret_cast<XrEventDataReferenceSpaceChangePending*>(
              &event_data);
      DCHECK(reference_space_change_pending->session == session_);
      // TODO(crbug.com/1015049)
      // Currently WMR only throw reference space change event for stage.
      // Other runtimes may decide to do it differently.
      if (reference_space_change_pending->referenceSpaceType ==
          XR_REFERENCE_SPACE_TYPE_STAGE) {
        UpdateStageBounds();
      }
    } else if (event_data.type ==
               XR_TYPE_EVENT_DATA_INTERACTION_PROFILE_CHANGED) {
      XrEventDataInteractionProfileChanged* interaction_profile_changed =
          reinterpret_cast<XrEventDataInteractionProfileChanged*>(&event_data);
      DCHECK_EQ(interaction_profile_changed->session, session_);
      xr_result = input_helper_->OnInteractionProfileChanged();
    } else {
      DVLOG(1) << __func__ << " Unhandled event type: " << event_data.type;
      TRACE_EVENT_INSTANT1("xr", "UnandledXrEvent", TRACE_EVENT_SCOPE_THREAD,
                           "type", event_data.type);
    }

    if (XR_FAILED(xr_result)) {
      TRACE_EVENT_INSTANT2("xr", "EventProcessingFailed",
                           TRACE_EVENT_SCOPE_THREAD, "type", event_data.type,
                           "xr_result", xr_result);
      Uninitialize();
      return xr_result;
    }

    event_data.type = XR_TYPE_EVENT_DATA_BUFFER;
    xr_result = xrPollEvent(instance_, &event_data);
  }

  // This catches the error where we failed to poll events only.
  if (XR_FAILED(xr_result)) {
    TRACE_EVENT_INSTANT1("xr", "EventPollingFailed", TRACE_EVENT_SCOPE_THREAD,
                         "xr_result", xr_result);
    Uninitialize();
  }
  return xr_result;
}

uint32_t OpenXrApiWrapper::GetRecommendedSwapchainSampleCount() const {
  DCHECK(IsInitialized());

  return base::ranges::min_element(
             primary_view_config_.Properties(), {},
             [](const XrViewConfigurationView& view) {
               return view.recommendedSwapchainSampleCount;
             })
      ->recommendedSwapchainSampleCount;
}

bool OpenXrApiWrapper::CanEnableAntiAliasing() const {
  return primary_view_config_.CanEnableAntiAliasing();
}

// stage bounds is fixed unless we received event
// XrEventDataReferenceSpaceChangePending
XrResult OpenXrApiWrapper::UpdateStageBounds() {
  DCHECK(HasSession());

  XrResult xr_result = XR_SUCCESS;

  if (StageParametersEnabled()) {
    xr_result = xrGetReferenceSpaceBoundsRect(
        session_, XR_REFERENCE_SPACE_TYPE_STAGE, &stage_bounds_);
    if (XR_FAILED(xr_result)) {
      stage_bounds_.height = 0;
      stage_bounds_.width = 0;
    }
  }

  return xr_result;
}

bool OpenXrApiWrapper::GetStageParameters(XrExtent2Df& stage_bounds,
                                          gfx::Transform& local_from_stage) {
  DCHECK(HasSession());

  if (!HasSpace(XR_REFERENCE_SPACE_TYPE_LOCAL))
    return false;

  if (!HasSpace(XR_REFERENCE_SPACE_TYPE_STAGE))
    return false;

  stage_bounds = stage_bounds_;

  XrSpaceLocation local_from_stage_location = {XR_TYPE_SPACE_LOCATION};
  if (XR_FAILED(xrLocateSpace(stage_space_, local_space_,
                              frame_state_.predictedDisplayTime,
                              &local_from_stage_location)) ||
      !IsPoseValid(local_from_stage_location.locationFlags)) {
    return false;
  }

  // Convert the orientation and translation given by runtime into a
  // transformation matrix.
  gfx::DecomposedTransform local_from_stage_decomp;
  local_from_stage_decomp.quaternion =
      gfx::Quaternion(local_from_stage_location.pose.orientation.x,
                      local_from_stage_location.pose.orientation.y,
                      local_from_stage_location.pose.orientation.z,
                      local_from_stage_location.pose.orientation.w);
  local_from_stage_decomp.translate[0] =
      local_from_stage_location.pose.position.x;
  local_from_stage_decomp.translate[1] =
      local_from_stage_location.pose.position.y;
  local_from_stage_decomp.translate[2] =
      local_from_stage_location.pose.position.z;

  local_from_stage = gfx::Transform::Compose(local_from_stage_decomp);
  return true;
}

void OpenXrApiWrapper::SetXrSessionState(XrSessionState new_state) {
  if (session_state_ == new_state)
    return;

  const char* old_state_name = GetXrSessionStateName(session_state_);
  const char* new_state_name = GetXrSessionStateName(new_state);
  DVLOG(1) << __func__ << " Transitioning from: " << old_state_name
           << " to: " << new_state_name;

  if (session_state_ != XR_SESSION_STATE_UNKNOWN) {
    TRACE_EVENT_NESTABLE_ASYNC_END1("xr", "XRSessionState", this, "state",
                                    old_state_name);
  }

  if (new_state != XR_SESSION_STATE_UNKNOWN) {
    TRACE_EVENT_NESTABLE_ASYNC_BEGIN1("xr", "XRSessionState", this, "state",
                                      new_state_name);
  }

  session_state_ = new_state;
}

bool OpenXrApiWrapper::StageParametersEnabled() const {
  return stage_parameters_enabled_;
}

VRTestHook* OpenXrApiWrapper::test_hook_ = nullptr;
ServiceTestHook* OpenXrApiWrapper::service_test_hook_ = nullptr;
void OpenXrApiWrapper::SetTestHook(VRTestHook* hook) {
  // This may be called from any thread - tests are responsible for
  // maintaining thread safety, typically by not changing the test hook
  // while presenting.
  test_hook_ = hook;
  if (service_test_hook_) {
    service_test_hook_->SetTestHook(test_hook_);
  }
}

}  // namespace device
