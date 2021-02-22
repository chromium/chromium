// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/optional.h"

#include "device/vr/openxr/openxr_render_loop.h"

#include <d3d11_4.h>

#include "components/viz/common/gpu/context_provider.h"
#include "device/vr/openxr/openxr_api_wrapper.h"
#include "device/vr/openxr/openxr_input_helper.h"
#include "device/vr/openxr/openxr_util.h"
#include "device/vr/util/stage_utils.h"
#include "device/vr/util/transform_utils.h"
#include "gpu/command_buffer/client/context_support.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "mojo/public/cpp/bindings/message.h"
#include "ui/gfx/geometry/angle_conversions.h"
#include "ui/gfx/gpu_fence.h"
#include "ui/gfx/transform.h"
#include "ui/gfx/transform_util.h"

namespace device {

OpenXrRenderLoop::OpenXrRenderLoop(
    base::RepeatingCallback<void(mojom::VRDisplayInfoPtr)>
        on_display_info_changed,
    VizContextProviderFactoryAsync context_provider_factory_async,
    XrInstance instance,
    const OpenXrExtensionHelper& extension_helper)
    : XRCompositorCommon(),
      instance_(instance),
      extension_helper_(extension_helper),
      on_display_info_changed_(std::move(on_display_info_changed)),
      context_provider_factory_async_(
          std::move(context_provider_factory_async)) {
  DCHECK(instance_ != XR_NULL_HANDLE);
}

void OpenXrRenderLoop::DisposeActiveAnchorCallbacks() {
  for (auto& create_anchor : create_anchor_requests_) {
    create_anchor.TakeCallback().Run(mojom::CreateAnchorResult::FAILURE, 0);
  }
}

OpenXrRenderLoop::~OpenXrRenderLoop() {
  DisposeActiveAnchorCallbacks();
  Stop();
}

mojom::XRFrameDataPtr OpenXrRenderLoop::GetNextFrameData() {
  mojom::XRFrameDataPtr frame_data = mojom::XRFrameData::New();
  frame_data->frame_id = next_frame_id_;

  const bool anchors_enabled = base::Contains(
      enabled_features_, device::mojom::XRSessionFeature::ANCHORS);

  const bool hand_input_enabled = base::Contains(
      enabled_features_, device::mojom::XRSessionFeature::HAND_INPUT);

  Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
  gpu::MailboxHolder mailbox_holder;
  if (XR_FAILED(openxr_->BeginFrame(&texture, &mailbox_holder))) {
    return frame_data;
  }

  texture_helper_.SetBackbuffer(std::move(texture));
  if (!mailbox_holder.mailbox.IsZero()) {
    frame_data->buffer_holder = mailbox_holder;
  }

  frame_data->time_delta =
      base::TimeDelta::FromNanoseconds(openxr_->GetPredictedDisplayTime());

  frame_data->input_state = openxr_->GetInputState(hand_input_enabled);

  frame_data->pose = mojom::VRPose::New();

  base::Optional<gfx::Quaternion> orientation;
  base::Optional<gfx::Point3F> position;
  if (XR_SUCCEEDED(openxr_->GetHeadPose(
          &orientation, &position, &frame_data->pose->emulated_position))) {
    if (orientation.has_value())
      frame_data->pose->orientation = orientation;

    if (position.has_value())
      frame_data->pose->position = position;
  }

  UpdateStageParameters();

  bool updated_eye_parameters = UpdateEyeParameters();

  if (updated_eye_parameters) {
    frame_data->left_eye = current_display_info_->left_eye.Clone();
    frame_data->right_eye = current_display_info_->right_eye.Clone();

    main_thread_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(on_display_info_changed_,
                                  current_display_info_.Clone()));
  }

  if (anchors_enabled) {
    OpenXrAnchorManager* anchor_manager =
        openxr_->GetOrCreateAnchorManager(extension_helper_);

    ProcessCreateAnchorRequests(anchor_manager,
                                frame_data->input_state.value());

    if (anchor_manager) {
      frame_data->anchors_data = anchor_manager->GetCurrentAnchorsData(
          openxr_->GetPredictedDisplayTime());
    }
  }

  return frame_data;
}

// StartRuntime is called by XRCompositorCommon::RequestSession. When the
// runtime is fully started, start_runtime_callback.Run must be called with a
// success boolean, or false on failure. OpenXrRenderLoop::StartRuntime waits
// until the Viz context provider is fully started before running
// start_runtime_callback.
void OpenXrRenderLoop::StartRuntime(
    StartRuntimeCallback start_runtime_callback) {
  DCHECK(instance_ != XR_NULL_HANDLE);
  DCHECK(!openxr_);
  DCHECK(!current_display_info_);

  // The new wrapper object is stored in a temporary variable instead of
  // openxr_ so that the local unique_ptr cleans up the object if starting
  // a session fails. openxr_ is set later in this method once we know
  // starting the session succeeds.
  std::unique_ptr<OpenXrApiWrapper> openxr =
      OpenXrApiWrapper::Create(instance_);
  if (!openxr)
    return std::move(start_runtime_callback).Run(false);

  SessionEndedCallback on_session_ended_callback = base::BindRepeating(
      &OpenXrRenderLoop::ExitPresent, weak_ptr_factory_.GetWeakPtr());
  VisibilityChangedCallback on_visibility_state_changed = base::BindRepeating(
      &OpenXrRenderLoop::SetVisibilityState, weak_ptr_factory_.GetWeakPtr());

  texture_helper_.SetUseBGRA(true);
  LUID luid;
  if (XR_FAILED(openxr->GetLuid(&luid, extension_helper_)) ||
      !texture_helper_.SetAdapterLUID(luid) ||
      !texture_helper_.EnsureInitialized() ||
      XR_FAILED(openxr->InitSession(texture_helper_.GetDevice(),
                                    extension_helper_,
                                    std::move(on_session_ended_callback),
                                    std::move(on_visibility_state_changed)))) {
    texture_helper_.Reset();
    return std::move(start_runtime_callback).Run(false);
  }

  // Starting session succeeded so we can set the member variable.
  // Any additional code added below this should never fail.
  openxr_ = std::move(openxr);
  texture_helper_.SetDefaultSize(openxr_->GetViewSize());

  InitializeDisplayInfo();

  StartContextProviderIfNeeded(std::move(start_runtime_callback));
}

void OpenXrRenderLoop::StopRuntime() {
  // Has to reset input_helper_ before reset openxr_. If we destroy openxr_
  // first, input_helper_destructor will try to call the actual openxr runtime
  // rather than the mock in tests.
  DisposeActiveAnchorCallbacks();
  openxr_ = nullptr;
  current_display_info_ = nullptr;
  texture_helper_.Reset();
  context_provider_.reset();
}

void OpenXrRenderLoop::EnableSupportedFeatures(
    const std::vector<device::mojom::XRSessionFeature>& required_features,
    const std::vector<device::mojom::XRSessionFeature>& optional_features) {
  const bool anchors_supported =
      extension_helper_.ExtensionEnumeration()->ExtensionSupported(
          XR_MSFT_SPATIAL_ANCHOR_EXTENSION_NAME);
  const bool hand_input_supported =
      extension_helper_.ExtensionEnumeration()->ExtensionSupported(
          kMSFTHandInteractionExtensionName);

  // Filter out features that are requested but not supported
  auto required_extension_enabled_filter =
      [anchors_supported,
       hand_input_supported](device::mojom::XRSessionFeature feature) {
        if (feature == device::mojom::XRSessionFeature::ANCHORS &&
            !anchors_supported) {
          return false;
        } else if (feature == device::mojom::XRSessionFeature::HAND_INPUT &&
                   !hand_input_supported) {
          return false;
        }
        return true;
      };

  enabled_features_.clear();
  // Currently, the initial filtering of supported devices happens on the
  // browser side (BrowserXRRuntimeImpl::SupportsFeature()), so if we have
  // reached this point, it is safe to assume that all requested features are
  // enabled.
  // TODO(https://crbug.com/995377): revisit the approach when the bug is fixed.
  std::copy(required_features.begin(), required_features.end(),
            std::inserter(enabled_features_, enabled_features_.begin()));
  std::copy_if(optional_features.begin(), optional_features.end(),
               std::inserter(enabled_features_, enabled_features_.begin()),
               required_extension_enabled_filter);
}

device::mojom::XREnvironmentBlendMode OpenXrRenderLoop::GetEnvironmentBlendMode(
    device::mojom::XRSessionMode session_mode) {
  return openxr_->PickEnvironmentBlendModeForSession(session_mode);
}

device::mojom::XRInteractionMode OpenXrRenderLoop::GetInteractionMode(
    device::mojom::XRSessionMode session_mode) {
  return device::mojom::XRInteractionMode::kWorldSpace;
}

bool OpenXrRenderLoop::CanEnableAntiAliasing() const {
  return openxr_->CanEnableAntiAliasing();
}

void OpenXrRenderLoop::OnSessionStart() {
  LogViewerType(VrViewerType::OPENXR_UNKNOWN);
}

bool OpenXrRenderLoop::HasSessionEnded() {
  return openxr_ && openxr_->UpdateAndGetSessionEnded();
}

bool OpenXrRenderLoop::SubmitCompositedFrame() {
  return XR_SUCCEEDED(openxr_->EndFrame());
}

void OpenXrRenderLoop::ClearPendingFrameInternal() {
  // Complete the frame if OpenXR has started one with BeginFrame. This also
  // releases the swapchain image that was acquired in BeginFrame so that the
  // next frame can acquire it.
  if (openxr_->HasPendingFrame() && XR_FAILED(openxr_->EndFrame())) {
    // The start of the next frame will detect that the session has ended via
    // HasSessionEnded and will exit presentation.
    StopRuntime();
    return;
  }
}

bool OpenXrRenderLoop::IsUsingSharedImages() const {
  return openxr_->IsUsingSharedImages();
}

void OpenXrRenderLoop::SubmitFrameDrawnIntoTexture(
    int16_t frame_index,
    const gpu::SyncToken& sync_token,
    base::TimeDelta time_waited) {
  gpu::gles2::GLES2Interface* gl = context_provider_->ContextGL();
  gl->WaitSyncTokenCHROMIUM(sync_token.GetConstData());
  const GLuint id = gl->CreateGpuFenceCHROMIUM();
  context_provider_->ContextSupport()->GetGpuFence(
      id, base::BindOnce(&OpenXrRenderLoop::OnWebXrTokenSignaled,
                         weak_ptr_factory_.GetWeakPtr(), frame_index, id));
}

void OpenXrRenderLoop::OnWebXrTokenSignaled(
    int16_t frame_index,
    GLuint id,
    std::unique_ptr<gfx::GpuFence> gpu_fence) {
  Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device =
      texture_helper_.GetDevice();
  Microsoft::WRL::ComPtr<ID3D11Device5> d3d11_device5;
  HRESULT hr = d3d11_device.As(&d3d11_device5);
  if (FAILED(hr)) {
    DLOG(ERROR) << "Unable to retrieve ID3D11Device5 interface " << std::hex
                << hr;
    return;
  }

  Microsoft::WRL::ComPtr<ID3D11Fence> d3d11_fence;
  hr = d3d11_device5->OpenSharedFence(
      gpu_fence->GetGpuFenceHandle().owned_handle.Get(),
      IID_PPV_ARGS(&d3d11_fence));
  if (FAILED(hr)) {
    DLOG(ERROR) << "Unable to open a shared fence " << std::hex << hr;
    return;
  }

  Microsoft::WRL::ComPtr<ID3D11DeviceContext> d3d11_device_context;
  d3d11_device5->GetImmediateContext(&d3d11_device_context);

  Microsoft::WRL::ComPtr<ID3D11DeviceContext4> d3d11_device_context4;
  hr = d3d11_device_context.As(&d3d11_device_context4);
  if (FAILED(hr)) {
    DLOG(ERROR) << "Unable to retrieve ID3D11DeviceContext4 interface "
                << std::hex << hr;
    return;
  }

  hr = d3d11_device_context4->Wait(d3d11_fence.Get(), 1);
  if (FAILED(hr)) {
    DLOG(ERROR) << "Unable to Wait on D3D11 fence " << std::hex << hr;
    return;
  }

  SubmitFrameWithTextureHandle(frame_index, mojo::PlatformHandle());

  if (openxr_) {
    // In order for the fence to be respected by the system, it needs to stick
    // around until the next time the texture comes up for use. To avoid needing
    // to remember the swap chain index, use frame_index %
    // color_swapchain_images_.size() to keep them separated from one another.
    openxr_->StoreFence(std::move(d3d11_fence), frame_index);
  }

  gpu::gles2::GLES2Interface* gl = context_provider_->ContextGL();
  gl->DestroyGpuFenceCHROMIUM(id);
}

// Return true if display info has changed.
void OpenXrRenderLoop::InitializeDisplayInfo() {
  if (!current_display_info_) {
    current_display_info_ = mojom::VRDisplayInfo::New();
    current_display_info_->right_eye = mojom::VREyeParameters::New();
    current_display_info_->left_eye = mojom::VREyeParameters::New();
  }

  gfx::Size view_size = openxr_->GetViewSize();
  current_display_info_->left_eye->render_width = view_size.width();
  current_display_info_->right_eye->render_width = view_size.width();
  current_display_info_->left_eye->render_height = view_size.height();
  current_display_info_->right_eye->render_height = view_size.height();

  // display info can't be send without fov info because of the mojo definition.
  current_display_info_->left_eye->field_of_view =
      mojom::VRFieldOfView::New(45.0f, 45.0f, 45.0f, 45.0f);
  current_display_info_->right_eye->field_of_view =
      current_display_info_->left_eye->field_of_view.Clone();

  main_thread_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(on_display_info_changed_, current_display_info_.Clone()));
}

// return true if either left_eye or right_eye updated.
bool OpenXrRenderLoop::UpdateEyeParameters() {
  bool changed = false;

  XrView left;
  XrView right;
  openxr_->GetHeadFromEyes(&left, &right);
  gfx::Size view_size = openxr_->GetViewSize();

  changed |= UpdateEye(left, view_size, &current_display_info_->left_eye);

  changed |= UpdateEye(right, view_size, &current_display_info_->right_eye);

  return changed;
}

bool OpenXrRenderLoop::UpdateEye(const XrView& view_head,
                                 const gfx::Size& view_size,
                                 mojom::VREyeParametersPtr* eye) const {
  bool changed = false;

  gfx::Transform head_from_eye = XrPoseToGfxTransform(view_head.pose);
  if ((*eye)->head_from_eye != head_from_eye) {
    (*eye)->head_from_eye = head_from_eye;
    changed = true;
  }

  if ((*eye)->render_width != static_cast<uint32_t>(view_size.width())) {
    (*eye)->render_width = static_cast<uint32_t>(view_size.width());
    changed = true;
  }

  if ((*eye)->render_height != static_cast<uint32_t>(view_size.height())) {
    (*eye)->render_height = static_cast<uint32_t>(view_size.height());
    changed = true;
  }

  mojom::VRFieldOfViewPtr fov =
      mojom::VRFieldOfView::New(gfx::RadToDeg(view_head.fov.angleUp),
                                gfx::RadToDeg(-view_head.fov.angleDown),
                                gfx::RadToDeg(-view_head.fov.angleLeft),
                                gfx::RadToDeg(view_head.fov.angleRight));
  if (!(*eye)->field_of_view || !fov->Equals(*(*eye)->field_of_view)) {
    (*eye)->field_of_view = std::move(fov);
    changed = true;
  }

  return changed;
}

void OpenXrRenderLoop::UpdateStageParameters() {
  XrExtent2Df stage_bounds;
  gfx::Transform local_from_stage;
  if (openxr_->GetStageParameters(&stage_bounds, &local_from_stage)) {
    mojom::VRStageParametersPtr stage_parameters =
        mojom::VRStageParameters::New();
    // mojo_from_local is identity, as is stage_from_floor, so we can directly
    // assign local_from_stage and mojo_from_floor.
    stage_parameters->mojo_from_floor = local_from_stage;
    stage_parameters->bounds = vr_utils::GetStageBoundsFromSize(
        stage_bounds.width, stage_bounds.height);
    SetStageParameters(std::move(stage_parameters));
  } else {
    SetStageParameters(nullptr);
  }
}

void OpenXrRenderLoop::GetEnvironmentIntegrationProvider(
    mojo::PendingAssociatedReceiver<
        device::mojom::XREnvironmentIntegrationProvider> environment_provider) {
  DVLOG(2) << __func__;

  environment_receiver_.reset();
  environment_receiver_.Bind(std::move(environment_provider));
}

void OpenXrRenderLoop::SubscribeToHitTest(
    mojom::XRNativeOriginInformationPtr native_origin_information,
    const std::vector<mojom::EntityTypeForHitTest>& entity_types,
    mojom::XRRayPtr ray,
    mojom::XREnvironmentIntegrationProvider::SubscribeToHitTestCallback
        callback) {
  mojo::ReportBadMessage(
      "OpenXrRenderLoop::SubscribeToHitTest not yet implemented");
}

void OpenXrRenderLoop::SubscribeToHitTestForTransientInput(
    const std::string& profile_name,
    const std::vector<mojom::EntityTypeForHitTest>& entity_types,
    mojom::XRRayPtr ray,
    mojom::XREnvironmentIntegrationProvider::
        SubscribeToHitTestForTransientInputCallback callback) {
  mojo::ReportBadMessage(
      "OpenXrRenderLoop::SubscribeToHitTestForTransientInput not yet "
      "implemented");
}

void OpenXrRenderLoop::UnsubscribeFromHitTest(uint64_t subscription_id) {
  mojo::ReportBadMessage(
      "OpenXrRenderLoop::UnsubscribeFromHitTest not yet implemented");
}

base::Optional<OpenXrRenderLoop::XrLocation>
OpenXrRenderLoop::GetXrLocationFromReferenceSpace(
    const mojom::XRNativeOriginInformation& native_origin_information,
    const gfx::Transform& native_origin_from_anchor) const {
  // Floor corresponds to offset from local * local, so we must apply the
  // offset to get the correct pose in the local space.
  auto type = native_origin_information.get_reference_space_type();
  if (type == device::mojom::XRReferenceSpaceType::kLocalFloor) {
    const mojom::VRStageParametersPtr& current_stage_parameters =
        GetCurrentStageParameters();
    if (!current_stage_parameters) {
      return base::nullopt;
    }
    return XrLocation{
        GfxTransformToXrPose(current_stage_parameters->mojo_from_floor *
                             native_origin_from_anchor),
        openxr_->GetReferenceSpace(
            device::mojom::XRReferenceSpaceType::kLocal)};
  }

  return XrLocation{GfxTransformToXrPose(native_origin_from_anchor),
                    openxr_->GetReferenceSpace(type)};
}

base::Optional<OpenXrRenderLoop::XrLocation>
OpenXrRenderLoop::GetXrLocationFromNativeOriginInformation(
    const OpenXrAnchorManager* anchor_manager,
    const mojom::XRNativeOriginInformation& native_origin_information,
    const gfx::Transform& native_origin_from_anchor,
    const std::vector<mojom::XRInputSourceStatePtr>& input_state) const {
  switch (native_origin_information.which()) {
    case mojom::XRNativeOriginInformation::Tag::INPUT_SOURCE_ID:
      // Currently unimplemented as only anchors are supported and are never
      // created relative to input sources
      return base::nullopt;
    case mojom::XRNativeOriginInformation::Tag::REFERENCE_SPACE_TYPE:
      return GetXrLocationFromReferenceSpace(native_origin_information,
                                             native_origin_from_anchor);
    case mojom::XRNativeOriginInformation::Tag::PLANE_ID:
    case mojom::XRNativeOriginInformation::Tag::HAND_JOINT_SPACE_INFO:
      // Unsupported for now
      return base::nullopt;
    case mojom::XRNativeOriginInformation::Tag::ANCHOR_ID:
      return XrLocation{GfxTransformToXrPose(native_origin_from_anchor),
                        anchor_manager->GetAnchorSpace(AnchorId(
                            native_origin_information.get_anchor_id()))};
  }
}

void OpenXrRenderLoop::CreateAnchor(
    mojom::XRNativeOriginInformationPtr native_origin_information,
    const device::Pose& native_origin_from_anchor,
    CreateAnchorCallback callback) {
  create_anchor_requests_.emplace_back(*native_origin_information,
                                       native_origin_from_anchor.ToTransform(),
                                       std::move(callback));
}

void OpenXrRenderLoop::ProcessCreateAnchorRequests(
    OpenXrAnchorManager* anchor_manager,
    const std::vector<mojom::XRInputSourceStatePtr>& input_state) {
  for (auto& request : create_anchor_requests_) {
    base::Optional<XrLocation> anchor_location =
        GetXrLocationFromNativeOriginInformation(
            anchor_manager, request.GetNativeOriginInformation(),
            request.GetNativeOriginFromAnchor(), input_state);
    if (!anchor_location.has_value()) {
      request.TakeCallback().Run(device::mojom::CreateAnchorResult::FAILURE, 0);
      continue;
    }

    AnchorId anchor_id = kInvalidAnchorId;
    if (openxr_->HasFrameState()) {
      XrTime display_time = openxr_->GetPredictedDisplayTime();
      anchor_id = anchor_manager->CreateAnchor(
          anchor_location->pose, anchor_location->space, display_time);
    }

    if (anchor_id.is_null()) {
      request.TakeCallback().Run(device::mojom::CreateAnchorResult::FAILURE, 0);
    } else {
      request.TakeCallback().Run(device::mojom::CreateAnchorResult::SUCCESS,
                                 anchor_id.GetUnsafeValue());
    }
  }
  create_anchor_requests_.clear();
}

void OpenXrRenderLoop::CreatePlaneAnchor(
    mojom::XRNativeOriginInformationPtr native_origin_information,
    const device::Pose& native_origin_from_anchor,
    uint64_t plane_id,
    CreatePlaneAnchorCallback callback) {
  mojo::ReportBadMessage(
      "OpenXrRenderLoop::CreatePlaneAnchor not yet implemented");
}

void OpenXrRenderLoop::DetachAnchor(uint64_t anchor_id) {
  OpenXrAnchorManager* anchor_manager =
      openxr_->GetOrCreateAnchorManager(extension_helper_);
  if (!anchor_manager) {
    return;
  }
  anchor_manager->DetachAnchor(AnchorId(anchor_id));
}

void OpenXrRenderLoop::StartContextProviderIfNeeded(
    StartRuntimeCallback start_runtime_callback) {
  DCHECK(task_runner()->BelongsToCurrentThread());
  DCHECK_EQ(context_provider_, nullptr);
  // We could arrive here in scenarios where we've shutdown the render loop or
  // runtime. In that case, there is no need to start the context provider.
  // If openxr_ has been torn down the context provider is unnecessary as
  // there is nothing to connect to the GPU process.
  if (openxr_) {
    main_thread_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            context_provider_factory_async_,
            base::BindOnce(&OpenXrRenderLoop::OnContextProviderCreated,
                           weak_ptr_factory_.GetWeakPtr(),
                           std::move(start_runtime_callback)),
            task_runner()));
  }
}

// viz::ContextLostObserver Implementation.
// Called on the render loop thread.
void OpenXrRenderLoop::OnContextLost() {
  DCHECK(task_runner()->BelongsToCurrentThread());
  DCHECK_NE(context_provider_, nullptr);

  // Avoid OnContextLost getting called multiple times by removing
  // the observer right away.
  context_provider_->RemoveObserver(this);

  // Destroying the context provider in the OpenXrRenderLoop::OnContextLost
  // callback leads to UAF deep inside the GpuChannel callback code. To avoid
  // UAF, post a task to ourselves which does the real context lost work. Pass
  // the context_provider_ as a parameters to the callback to avoid the invalid
  // one getting used on the context thread.
  task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&OpenXrRenderLoop::OnContextLostCallback,
                                weak_ptr_factory_.GetWeakPtr(),
                                std::move(context_provider_)));
}

// Called on the render loop thread as a continuation of OnContextLost.
void OpenXrRenderLoop::OnContextLostCallback(
    scoped_refptr<viz::ContextProvider> lost_context_provider) {
  DCHECK(task_runner()->BelongsToCurrentThread());
  DCHECK_EQ(context_provider_, nullptr);

  // Context providers are required to be released on the context thread they
  // were bound to.
  lost_context_provider.reset();

  StartContextProviderIfNeeded(base::DoNothing());
}

// OpenXrRenderLoop::StartContextProvider queues a task on the main thread's
// task runner to run IsolatedXRRuntimeProvider::CreateContextProviderAsync.
// When CreateContextProviderAsync finishes creating the Viz context provider,
// it will queue a task onto the render loop's task runner to run
// OnContextProviderCreated, passing it the newly created context provider.
// StartContextProvider uses BindOnce to passthrough the start_runtime_callback
// given to it from it's caller OnContextProviderCreated must run the
// start_runtime_callback, passing true on successful call to
// BindToCurrentThread and false if not.
void OpenXrRenderLoop::OnContextProviderCreated(
    StartRuntimeCallback start_runtime_callback,
    scoped_refptr<viz::ContextProvider> context_provider) {
  DCHECK(task_runner()->BelongsToCurrentThread());
  DCHECK_EQ(context_provider_, nullptr);

  const gpu::ContextResult context_result =
      context_provider->BindToCurrentThread();
  if (context_result != gpu::ContextResult::kSuccess) {
    std::move(start_runtime_callback).Run(false);
    return;
  }

  if (openxr_) {
    openxr_->CreateSharedMailboxes(context_provider.get());
  }

  context_provider->AddObserver(this);
  context_provider_ = std::move(context_provider);

  std::move(start_runtime_callback).Run(true);
}

}  // namespace device
