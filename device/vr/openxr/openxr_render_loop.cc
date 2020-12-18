// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/optional.h"

#include "device/vr/openxr/openxr_render_loop.h"

#include "components/viz/common/gpu/context_provider.h"
#include "device/vr/openxr/openxr_api_wrapper.h"
#include "device/vr/openxr/openxr_input_helper.h"
#include "device/vr/util/stage_utils.h"
#include "device/vr/util/transform_utils.h"
#include "mojo/public/cpp/bindings/message.h"
#include "ui/gfx/geometry/angle_conversions.h"

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

  Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
  if (XR_FAILED(openxr_->BeginFrame(&texture))) {
    return frame_data;
  }

  texture_helper_.SetBackbuffer(texture.Get());

  frame_data->time_delta =
      base::TimeDelta::FromNanoseconds(openxr_->GetPredictedDisplayTime());

  frame_data->input_state =
      input_helper_->GetInputState(openxr_->GetPredictedDisplayTime());

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

  if (anchors_enabled_) {
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

bool OpenXrRenderLoop::StartRuntime() {
  DCHECK(instance_ != XR_NULL_HANDLE);
  DCHECK(!openxr_);
  DCHECK(!input_helper_);
  DCHECK(!current_display_info_);

  // The new wrapper object is stored in a temporary variable instead of
  // openxr_ so that the local unique_ptr cleans up the object if starting
  // a session fails. openxr_ is set later in this method once we know
  // starting the session succeeds.
  std::unique_ptr<OpenXrApiWrapper> openxr =
      OpenXrApiWrapper::Create(instance_);
  if (!openxr)
    return false;

  texture_helper_.SetUseBGRA(true);
  LUID luid;
  if (XR_FAILED(openxr->GetLuid(&luid, extension_helper_)) ||
      !texture_helper_.SetAdapterLUID(luid) ||
      !texture_helper_.EnsureInitialized() ||
      XR_FAILED(openxr->InitSession(texture_helper_.GetDevice(), &input_helper_,
                                    extension_helper_))) {
    texture_helper_.Reset();
    return false;
  }

  // Starting session succeeded so we can set the member variable.
  // Any additional code added below this should never fail.
  openxr_ = std::move(openxr);
  texture_helper_.SetDefaultSize(openxr_->GetViewSize());

  openxr_->RegisterInteractionProfileChangeCallback(
      base::BindRepeating(&OpenXRInputHelper::OnInteractionProfileChanged,
                          input_helper_->GetWeakPtr()));
  openxr_->RegisterVisibilityChangeCallback(base::BindRepeating(
      &OpenXrRenderLoop::SetVisibilityState, weak_ptr_factory_.GetWeakPtr()));
  openxr_->RegisterOnSessionEndedCallback(base::BindRepeating(
      &OpenXrRenderLoop::ExitPresent, weak_ptr_factory_.GetWeakPtr()));
  InitializeDisplayInfo();

  // TODO(https://crbug.com/1131616): In a subsequent change, refactor
  // StartContextProviderIfNeeded such that we do not start the session until
  // the context provider has been created.
  StartContextProviderIfNeeded();

  return true;
}

void OpenXrRenderLoop::StopRuntime() {
  // Has to reset input_helper_ before reset openxr_. If we destroy openxr_
  // first, input_helper_destructor will try to call the actual openxr runtime
  // rather than the mock in tests.
  DisposeActiveAnchorCallbacks();
  input_helper_.reset();
  openxr_ = nullptr;
  current_display_info_ = nullptr;
  texture_helper_.Reset();
}

void OpenXrRenderLoop::EnableSupportedFeatures(
    const std::vector<device::mojom::XRSessionFeature>& requiredFeatures,
    const std::vector<device::mojom::XRSessionFeature>& optionalFeatures) {
  const bool anchors_supported =
      extension_helper_.ExtensionEnumeration()->ExtensionSupported(
          XR_MSFT_SPATIAL_ANCHOR_EXTENSION_NAME);
  // Filter out features that are requested but not supported
  auto required_extension_enabled_filter =
      [anchors_supported](device::mojom::XRSessionFeature feature) {
        if (feature == device::mojom::XRSessionFeature::ANCHORS &&
            !anchors_supported) {
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
  std::copy(requiredFeatures.begin(), requiredFeatures.end(),
            std::inserter(enabled_features_, enabled_features_.begin()));
  std::copy_if(optionalFeatures.begin(), optionalFeatures.end(),
               std::inserter(enabled_features_, enabled_features_.begin()),
               required_extension_enabled_filter);

  // Cache feature support
  const bool anchors_requested =
      enabled_features_.count(device::mojom::XRSessionFeature::ANCHORS) != 0;
  anchors_enabled_ = anchors_requested && anchors_supported;
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

bool OpenXrRenderLoop::PreComposite() {
  return true;
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

  // TODO(crbug.com/999573): Query eye-to-head transform from the device and use
  // that instead of just building a transformation matrix from the translation
  // component.
  gfx::Transform head_from_eye = vr_utils::MakeTranslationTransform(
      view_head.pose.position.x, view_head.pose.position.y,
      view_head.pose.position.z);

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

void OpenXrRenderLoop::StartContextProviderIfNeeded() {
  DCHECK(task_runner()->BelongsToCurrentThread());
  // We could arrive here in scenarios where we've shutdown the render loop.
  // In that case, there is no need to start the context provider.
  if (!context_provider_ && !HasSessionEnded()) {
    main_thread_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            context_provider_factory_async_,
            base::BindOnce(&OpenXrRenderLoop::OnContextProviderCreated,
                           weak_ptr_factory_.GetWeakPtr()),
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

// Called on the render loop thread as a continuation of OnContextLost
void OpenXrRenderLoop::OnContextLostCallback(
    scoped_refptr<viz::ContextProvider> context_provider) {
  DCHECK(task_runner()->BelongsToCurrentThread());
  DCHECK_EQ(context_provider_, nullptr);

  // context_provider is required to be released on the context thread it was
  // bound to.
  context_provider.reset();

  StartContextProviderIfNeeded();
}

// Called on the render loop thread by IsolatedXRRuntimeProvider when it has
// finished creating the context provider.
void OpenXrRenderLoop::OnContextProviderCreated(
    scoped_refptr<viz::ContextProvider> context_provider) {
  DCHECK(task_runner()->BelongsToCurrentThread());
  DCHECK_EQ(context_provider_, nullptr);

  const gpu::ContextResult context_result =
      context_provider->BindToCurrentThread();
  if (context_result != gpu::ContextResult::kSuccess) {
    // TODO(https://crbug.com/1131616): Handle this by creating the context
    // provider again.
    return;
  }

  context_provider->AddObserver(this);
  context_provider_ = std::move(context_provider);
}

}  // namespace device
