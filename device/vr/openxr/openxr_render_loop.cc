// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/openxr/openxr_render_loop.h"

#include "device/vr/openxr/openxr_api_wrapper.h"
#include "device/vr/openxr/openxr_input_helper.h"
#include "device/vr/util/stage_utils.h"
#include "device/vr/util/transform_utils.h"
#include "ui/gfx/geometry/angle_conversions.h"
#include "ui/gfx/transform.h"
#include "ui/gfx/transform_util.h"

namespace device {

OpenXrRenderLoop::OpenXrRenderLoop(
    base::RepeatingCallback<void(mojom::VRDisplayInfoPtr)>
        on_display_info_changed,
    XrInstance instance,
    const OpenXrExtensionHelper& extension_helper)
    : XRCompositorCommon(),
      instance_(instance),
      extension_helper_(extension_helper),
      on_display_info_changed_(std::move(on_display_info_changed)) {
  DCHECK(instance_ != XR_NULL_HANDLE);
}

OpenXrRenderLoop::~OpenXrRenderLoop() {
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
  InitializeDisplayInfo();

  return true;
}

void OpenXrRenderLoop::StopRuntime() {
  // Has to reset input_helper_ before reset openxr_. If we destroy openxr_
  // first, input_helper_destructor will try to call the actual openxr runtime
  // rather than the mock in tests.
  input_helper_.reset();
  openxr_ = nullptr;
  current_display_info_ = nullptr;
  texture_helper_.Reset();
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

}  // namespace device
