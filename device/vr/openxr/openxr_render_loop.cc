// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/openxr/openxr_render_loop.h"

#include <optional>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/ranges/algorithm.h"
#include "base/task/bind_post_task.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "base/types/cxx23_to_underlying.h"
#include "build/build_config.h"
#include "components/viz/common/gpu/context_provider.h"
#include "device/vr/openxr/exit_xr_present_reason.h"
#include "device/vr/openxr/openxr_api_wrapper.h"
#include "device/vr/openxr/openxr_input_helper.h"
#include "device/vr/openxr/openxr_util.h"
#include "device/vr/util/stage_utils.h"
#include "gpu/command_buffer/client/context_support.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "mojo/public/cpp/bindings/message.h"
#include "third_party/openxr/src/include/openxr/openxr.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/geometry/transform_util.h"
#include "ui/gfx/gpu_fence.h"

namespace {
// Number of frames to use for sliding averages for pose timings,
// as used for estimating prediction times.
constexpr unsigned kSlidingAverageSize = 5;

device::mojom::XRRenderInfoPtr GetRenderInfo(
    const device::mojom::XRFrameData& frame_data) {
  device::mojom::XRRenderInfoPtr result = device::mojom::XRRenderInfo::New();

  result->frame_id = frame_data.frame_id;
  result->mojo_from_viewer = frame_data.mojo_from_viewer.Clone();

  for (size_t i = 0; i < frame_data.views.size(); i++) {
    result->views.push_back(frame_data.views[i]->Clone());
  }

  return result;
}

}  // namespace

namespace device {

OpenXrRenderLoop::OutstandingFrame::OutstandingFrame() = default;
OpenXrRenderLoop::OutstandingFrame::~OutstandingFrame() = default;

OpenXrRenderLoop::OpenXrRenderLoop(
    VizContextProviderFactoryAsync context_provider_factory_async,
    XrInstance instance,
    const OpenXrExtensionHelper& extension_helper,
    OpenXrPlatformHelper* platform_helper)
    : XRThread("OpenXrRenderLoop"),
      main_thread_task_runner_(
          base::SingleThreadTaskRunner::GetCurrentDefault()),
      instance_(instance),
      extension_helper_(extension_helper),
      context_provider_factory_async_(
          std::move(context_provider_factory_async)),
      webxr_js_time_(kSlidingAverageSize),
      webxr_gpu_time_(kSlidingAverageSize),
      platform_helper_(platform_helper) {
  DCHECK(main_thread_task_runner_);
  DCHECK(instance_ != XR_NULL_HANDLE);
}

OpenXrRenderLoop::~OpenXrRenderLoop() {
  Stop();
}

void OpenXrRenderLoop::ExitPresent(ExitXrPresentReason reason) {
  DVLOG(1) << __func__ << " reason=" << base::to_underlying(reason);
  TRACE_EVENT_INSTANT1("xr", "ExitPresent", TRACE_EVENT_SCOPE_THREAD, "reason",
                       base::to_underlying(reason));
  if (!is_presenting_) {
    return;
  }

  is_presenting_ = false;
  webxr_has_pose_ = false;
  presentation_receiver_.reset();
  frame_data_receiver_.reset();
  submit_client_.reset();

  pending_frame_.reset();
  delayed_get_frame_data_callback_.Reset();

  // Reset webxr_visible_ for subsequent presentations.
  webxr_visible_ = true;

  // Kill outstanding overlays:
  overlay_visible_ = false;
  overlay_receiver_.reset();

  graphics_binding_->SetOverlayAndWebXrVisibility(false, false);

  // Don't call StopRuntime until this thread has finished the rest of the work.
  // This is to prevent the OpenXrApiWrapper from being deleted before its
  // cleanup work has finished.
  // If we're called as a result of the OpenXrApiWrapper being destroyed, we may
  // not have a task_runner anymore. Note that this appears to predominantly be
  // the case on Android, where the task_runner() is destroyed before
  // `StopRuntime` is called (which can lead to us being called), as Windows
  // appears to stop/destroy the task runner after calling `StopRuntime`.
  // Our `StopRuntime` method itself is resilient to multiple/re-entrant calls,
  // so simply validate the task runner rather than something more involved to
  // prevent that.
  if (task_runner()) {
    task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&OpenXrRenderLoop::StopRuntime, base::Unretained(this)));
  }
}

void OpenXrRenderLoop::GetFrameData(
    mojom::XRFrameDataRequestOptionsPtr options,
    mojom::XRFrameDataProvider::GetFrameDataCallback callback) {
  TRACE_EVENT0("xr", "GetFrameData");
  if (HasSessionEnded()) {
    ExitPresent(ExitXrPresentReason::kGetFrameAfterSessionEnded);
    return;
  }

  // HasSessionEnded() may do some work that alters the state of
  // `is_presenting_`, in which case we'll get the ExitPresent log to know that
  // we've ignored this request; but otherwise, we should log the rest of the
  // state after that.
  DVLOG(3) << __func__ << " is_presenting_=" << is_presenting_
           << " webxr_visible=" << webxr_visible_
           << " on_webxr_submitted_=" << !!on_webxr_submitted_
           << " webxr_has_pose_=" << webxr_has_pose_;

  if (!is_presenting_) {
    return;
  }

  // If we've already given out a pose for the current frame, or aren't visible,
  // delay giving out a pose until the next frame we are visible.
  // However, if we aren't visible and the browser is waiting to learn that
  // WebXR has submitted a frame, we can give out a pose as though we are
  // visible.
  if ((!webxr_visible_ && !on_webxr_submitted_) || webxr_has_pose_) {
    // There should only be one outstanding GetFrameData call at a time.  We
    // shouldn't get new ones until this resolves or presentation ends/restarts.
    if (delayed_get_frame_data_callback_) {
      frame_data_receiver_.ReportBadMessage(
          "Multiple outstanding GetFrameData calls");
      return;
    }
    delayed_get_frame_data_callback_ =
        base::BindOnce(&OpenXrRenderLoop::GetFrameData, base::Unretained(this),
                       std::move(options), std::move(callback));
    return;
  }

  StartPendingFrame();
  webxr_has_pose_ = true;
  pending_frame_->webxr_has_pose_ = true;
  pending_frame_->sent_frame_data_time_ = base::TimeTicks::Now();

  // TODO(crbug.com/40771470): The lack of frame_data_ here indicates
  // that we probably should have deferred this call, but it matches the
  // behavior from before the stage parameters were updated in this function and
  // avoids a crash. Likely the deferral above should check if we're awaiting
  // either the webxr or overlay submit.
  if (pending_frame_->frame_data_) {
    // If the stage parameters have been updated since the last frame that was
    // sent, send the updated values.
    pending_frame_->frame_data_->stage_parameters_id = stage_parameters_id_;
    if (options->stage_parameters_id != stage_parameters_id_) {
      pending_frame_->frame_data_->stage_parameters =
          current_stage_parameters_.Clone();
    }
  } else {
    TRACE_EVENT0("xr", "GetFrameData Missing FrameData");
  }

  // Yield here to let the event queue process pending mojo messages,
  // specifically the next gamepad callback request that's likely to
  // have been sent during WaitGetPoses.
  task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&OpenXrRenderLoop::SendFrameData,
                                base::Unretained(this), std::move(callback),
                                std::move(pending_frame_->frame_data_)));

  next_frame_id_ += 1;
  if (next_frame_id_ < 0) {
    next_frame_id_ = 0;
  }
}

void OpenXrRenderLoop::RequestSession(
    base::RepeatingCallback<void(mojom::XRVisibilityState)>
        on_visibility_state_changed,
    mojom::XRRuntimeSessionOptionsPtr options,
    RequestSessionCallback callback) {
  webxr_has_pose_ = false;
  presentation_receiver_.reset();
  frame_data_receiver_.reset();
  request_session_callback_ =
      base::BindPostTask(main_thread_task_runner_, std::move(callback));

  StartRuntime(std::move(on_visibility_state_changed), std::move(options));
}

void OpenXrRenderLoop::SetVisibilityState(
    mojom::XRVisibilityState visibility_state) {
  if (visibility_state_ != visibility_state) {
    visibility_state_ = visibility_state;
    if (on_visibility_state_changed_) {
      main_thread_task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(on_visibility_state_changed_, visibility_state));
    }
  }
}

void OpenXrRenderLoop::SetStageParameters(
    mojom::VRStageParametersPtr stage_parameters) {
  // If the stage parameters are identical no need to update them.
  if ((!current_stage_parameters_ && !stage_parameters) ||
      (current_stage_parameters_ && stage_parameters &&
       current_stage_parameters_.Equals(stage_parameters))) {
    return;
  }

  // If they have changed, increment the ID and save the new parameters.
  stage_parameters_id_++;
  current_stage_parameters_ = std::move(stage_parameters);
}

#if BUILDFLAG(IS_WIN)
void OpenXrRenderLoop::SubmitFrameWithTextureHandle(
    int16_t frame_index,
    mojo::PlatformHandle texture_handle,
    const gpu::SyncToken& sync_token) {
  DVLOG(3) << __func__ << " frame_index=" << frame_index;
  TRACE_EVENT1("xr", "SubmitFrameWithTextureHandle", "frameIndex", frame_index);
  if (!MarkFrameSubmitted(frame_index)) {
    return;
  }

  graphics_binding_->SetWebXrTexture(std::move(texture_handle), sync_token,
                                     left_webxr_bounds_, right_webxr_bounds_);

  // Regardless of success - try to composite what we have.
  MaybeCompositeAndSubmit();
}
#endif

void OpenXrRenderLoop::CleanUp() {
  DVLOG(1) << __func__;
  submit_client_.reset();
  webxr_has_pose_ = false;
  presentation_receiver_.reset();
  frame_data_receiver_.reset();
  overlay_receiver_.reset();
  environment_receiver_.reset();
  StopRuntime();
}

void OpenXrRenderLoop::ClearPendingFrame() {
  // Complete the frame if OpenXR has started one with BeginFrame. This also
  // releases the swapchain image that was acquired in BeginFrame so that the
  // next frame can acquire it.
  if (openxr_->HasPendingFrame() && XR_FAILED(openxr_->EndFrame())) {
    // The start of the next frame will detect that the session has ended via
    // HasSessionEnded and will exit presentation.
    ExitPresent(ExitXrPresentReason::kXrEndFrameFailed);
    return;
  }

  pending_frame_.reset();
  // Send frame data to outstanding requests.
  if (delayed_get_frame_data_callback_ &&
      (webxr_visible_ || on_webxr_submitted_)) {
    // If WebXR is not visible, but the browser wants to know when it submits a
    // frame, we allow the renderer to receive poses.
    std::move(delayed_get_frame_data_callback_).Run();
  }
}

void OpenXrRenderLoop::StartPendingFrame() {
  DVLOG(3) << __func__ << " pending_frame_=" << pending_frame_.has_value();
  if (!pending_frame_) {
    pending_frame_.emplace();
    pending_frame_->waiting_for_webxr_ = webxr_visible_;
    pending_frame_->waiting_for_overlay_ = overlay_visible_;
    pending_frame_->frame_data_ = GetNextFrameData();
    // GetNextFrameData() should never return null:
    DCHECK(pending_frame_->frame_data_);
    pending_frame_->render_info_ = GetRenderInfo(*pending_frame_->frame_data_);
  }
}

void OpenXrRenderLoop::StartRuntimeFinish(
    base::RepeatingCallback<void(mojom::XRVisibilityState)>
        on_visibility_state_changed,
    mojom::XRRuntimeSessionOptionsPtr options,
    bool success) {
  if (!success) {
    TRACE_EVENT_INSTANT0("xr", "Failed to start runtime",
                         TRACE_EVENT_SCOPE_THREAD);
    MaybeRejectSessionCallback();
    return;
  }

  on_visibility_state_changed_ = std::move(on_visibility_state_changed);

  // Queue up a notification to the requester of the current visibility state,
  // so that it can be initialized to the right value.
  if (on_visibility_state_changed_) {
    main_thread_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(on_visibility_state_changed_, visibility_state_));
  }

  device::mojom::XRPresentationTransportOptionsPtr transport_options =
      device::mojom::XRPresentationTransportOptions::New();

  if (graphics_binding_->IsUsingSharedImages()) {
    transport_options->transport_method =
        device::mojom::XRPresentationTransportMethod::DRAW_INTO_TEXTURE_MAILBOX;
  } else if constexpr (BUILDFLAG(IS_WIN)) {
    transport_options->transport_method =
        device::mojom::XRPresentationTransportMethod::SUBMIT_AS_TEXTURE_HANDLE;
  } else {
    transport_options->transport_method =
        device::mojom::XRPresentationTransportMethod::SUBMIT_AS_MAILBOX_HOLDER;
  }

  // Only set boolean options that we need. Default is false, and we should be
  // able to safely ignore ones that our implementation doesn't care about.
  transport_options->wait_for_transfer_notification = true;

  LogViewerType(VrViewerType::OPENXR_UNKNOWN);

  auto submit_frame_sink = device::mojom::XRPresentationConnection::New();
  submit_frame_sink->provider =
      presentation_receiver_.BindNewPipeAndPassRemote();
  submit_frame_sink->client_receiver =
      submit_client_.BindNewPipeAndPassReceiver();
  submit_frame_sink->transport_options = std::move(transport_options);

  auto session = device::mojom::XRSession::New();
  session->data_provider = frame_data_receiver_.BindNewPipeAndPassRemote();
  session->submit_frame_sink = std::move(submit_frame_sink);

  const auto& enabled_features = openxr_->GetEnabledFeatures();
  session->enabled_features.insert(session->enabled_features.end(),
                                   enabled_features.begin(),
                                   enabled_features.end());

  session->device_config = device::mojom::XRSessionDeviceConfig::New();
  session->device_config->enable_anti_aliasing =
      openxr_->CanEnableAntiAliasing();
  session->device_config->views = openxr_->GetDefaultViews();
  if (auto* depth = openxr_->GetDepthSensor(); depth) {
    session->device_config->depth_configuration = depth->GetDepthConfig();
  }

  session->enviroment_blend_mode =
      openxr_->PickEnvironmentBlendModeForSession(options->mode);
  session->interaction_mode = device::mojom::XRInteractionMode::kWorldSpace;

  mojo::PendingRemote<mojom::ImmersiveOverlay> overlay_remote;

  overlay_receiver_.reset();
  overlay_remote = overlay_receiver_.BindNewPipeAndPassRemote();

  CHECK(request_session_callback_);
  std::move(request_session_callback_)
      .Run(true, std::move(session), std::move(overlay_remote));
  is_presenting_ = true;

  graphics_binding_->SetOverlayAndWebXrVisibility(overlay_visible_,
                                                  webxr_visible_);
}

void OpenXrRenderLoop::MaybeCompositeAndSubmit() {
  DVLOG(3) << __func__;
  if (!pending_frame_) {
    // There is no outstanding frame, nor frame to composite, but there may be
    // pending GetFrameData calls, so ClearPendingFrame() to respond to them.
    ClearPendingFrame();
    return;
  }

  // Check if we have obtained all layers (overlay and webxr) that we need.
  if (pending_frame_->waiting_for_webxr_ ||
      pending_frame_->waiting_for_overlay_) {
    DVLOG(3) << __func__ << "Waiting for additional layers, waiting_for_webxr_="
             << pending_frame_->waiting_for_webxr_
             << " waiting_for_overlay=" << pending_frame_->waiting_for_overlay_;
    // Haven't received submits from all layers.
    return;
  }

  bool copy_successful = false;
  bool has_webxr_content = pending_frame_->webxr_submitted_ && webxr_visible_;
  bool has_overlay_content =
      pending_frame_->overlay_submitted_ && overlay_visible_;
  bool can_submit = has_webxr_content || has_overlay_content;

  // Tell texture helper to composite, then grab the output texture, and submit.
  // If we submitted, set up the next frame, and send outstanding pose requests.
  if (can_submit) {
    copy_successful = graphics_binding_->Render(context_provider_);
  } else {
    graphics_binding_->CleanupWithoutSubmit();
  }

  // A copy can only be successful if we actually tried to submit.
  if (copy_successful) {
    pending_frame_->frame_ready_time_ = base::TimeTicks::Now();
    if (!SubmitCompositedFrame()) {
      ExitPresent(ExitXrPresentReason::kSubmitFrameFailed);
      // ExitPresent() clears pending_frame_, so return here to avoid
      // accessing it below.
      return;
    }
  }

  if (pending_frame_->webxr_submitted_ && copy_successful) {
    // We've submitted a frame.
    webxr_js_time_.AddSample(pending_frame_->submit_frame_time_ -
                             pending_frame_->sent_frame_data_time_);
    webxr_gpu_time_.AddSample(pending_frame_->frame_ready_time_ -
                              pending_frame_->submit_frame_time_);

    TRACE_EVENT_INSTANT2(
        "gpu", "WebXR frame time (ms)", TRACE_EVENT_SCOPE_THREAD, "javascript",
        webxr_js_time_.GetAverage().InMillisecondsF(), "rendering",
        webxr_gpu_time_.GetAverage().InMillisecondsF());
    fps_meter_.AddFrame(base::TimeTicks::Now());
    TRACE_COUNTER1("gpu", "WebXR FPS", fps_meter_.GetFPS());
  }

  if (pending_frame_->webxr_submitted_ && submit_client_) {
    // Tell WebVR that we are done with the texture (if we got a texture)
    submit_client_->OnSubmitFrameTransferred(copy_successful);
    submit_client_->OnSubmitFrameRendered();
    TRACE_EVENT1("xr", "SubmitFrameTransferred", "success", copy_successful);
  }

  if (pending_frame_->overlay_submitted_ && overlay_submit_callback_) {
    // Tell the browser/overlay that we are done with its texture so it can be
    // reused.
    std::move(overlay_submit_callback_).Run(copy_successful);
  }

  ClearPendingFrame();
}

bool OpenXrRenderLoop::MarkFrameSubmitted(int16_t frame_index) {
  DVLOG(3) << __func__;
  webxr_has_pose_ = false;
  // Tell the browser that WebXR has submitted a frame.
  if (on_webxr_submitted_) {
    std::move(on_webxr_submitted_).Run();
  }

  if (!pending_frame_ ||
      pending_frame_->render_info_->frame_id != frame_index) {
    // We weren't expecting a submitted frame.  This can happen if WebXR was
    // hidden by an overlay for some time.
    if (submit_client_) {
      submit_client_->OnSubmitFrameTransferred(false);
      submit_client_->OnSubmitFrameRendered();
      TRACE_EVENT1("xr", "SubmitFrameTransferred", "success", false);
    }
    return false;
  }

  pending_frame_->waiting_for_webxr_ = false;
  pending_frame_->webxr_submitted_ = true;
  pending_frame_->submit_frame_time_ = base::TimeTicks::Now();

  return true;
}

void OpenXrRenderLoop::SubmitFrameMissing(int16_t frame_index,
                                          const gpu::SyncToken& sync_token) {
  DVLOG(3) << __func__ << " frame_index=" << frame_index;
  TRACE_EVENT_INSTANT0("xr", "SubmitFrameMissing", TRACE_EVENT_SCOPE_THREAD);
  if (pending_frame_) {
    // WebXR for this frame is hidden.
    pending_frame_->waiting_for_webxr_ = false;
  }
  webxr_has_pose_ = false;
  MaybeCompositeAndSubmit();
}

void OpenXrRenderLoop::UpdateLayerBounds(int16_t frame_id,
                                         const gfx::RectF& left_bounds,
                                         const gfx::RectF& right_bounds,
                                         const gfx::Size& source_size) {
  // Bounds are updated instantly, rather than waiting for frame_id.  This works
  // since blink always passes the current frame_id when updating the bounds.
  // Ignoring the frame_id keeps the logic simpler, so this can more easily
  // merge with vr_shell_gl eventually.
  left_webxr_bounds_ = left_bounds;
  right_webxr_bounds_ = right_bounds;

  // Swap top/bottom to account for differences between mojom and GL
  // coordinates.
  left_webxr_bounds_.set_y(
      1 - (left_webxr_bounds_.y() + left_webxr_bounds_.height()));
  right_webxr_bounds_.set_y(
      1 - (right_webxr_bounds_.y() + right_webxr_bounds_.height()));

  source_size_ = source_size;

  graphics_binding_->SetTransferSize(source_size);
}

void OpenXrRenderLoop::SubmitOverlayTexture(
    int16_t frame_id,
    gfx::GpuMemoryBufferHandle texture,
    const gpu::SyncToken& sync_token,
    const gfx::RectF& left_bounds,
    const gfx::RectF& right_bounds,
    SubmitOverlayTextureCallback overlay_submit_callback) {
  TRACE_EVENT_INSTANT0("xr", "SubmitOverlay", TRACE_EVENT_SCOPE_THREAD);
  DCHECK(overlay_visible_);
  overlay_submit_callback_ = std::move(overlay_submit_callback);
  if (!pending_frame_) {
    // We may stop presenting while there is a pending SubmitOverlayTexture
    // outstanding.  If we get an overlay submit we weren't expecting, just
    // ignore it.
    DCHECK(!is_presenting_);
    std::move(overlay_submit_callback_).Run(false);
    return;
  }

  pending_frame_->waiting_for_overlay_ = false;

  graphics_binding_->SetOverlayTexture(std::move(texture), sync_token,
                                       left_bounds, right_bounds);
  pending_frame_->overlay_submitted_ = true;

  // Regardless of success - try to composite what we have.
  MaybeCompositeAndSubmit();
}

void OpenXrRenderLoop::RequestNextOverlayPose(
    RequestNextOverlayPoseCallback callback) {
  DVLOG(3) << __func__;
  // We will only request poses while the overlay is visible.
  DCHECK(overlay_visible_);
  TRACE_EVENT_INSTANT0("xr", "RequestOverlayPose", TRACE_EVENT_SCOPE_THREAD);

  // Ensure we have a pending frame.
  StartPendingFrame();
  pending_frame_->overlay_has_pose_ = true;
  std::move(callback).Run(pending_frame_->render_info_->Clone());
}

void OpenXrRenderLoop::SetOverlayAndWebXRVisibility(bool overlay_visible,
                                                    bool webxr_visible) {
  DVLOG(1) << __func__ << " overlay_visible=" << overlay_visible
           << " webxr_visible=" << webxr_visible;
  TRACE_EVENT_INSTANT2("xr", "SetOverlayAndWebXRVisibility",
                       TRACE_EVENT_SCOPE_THREAD, "overlay", overlay_visible,
                       "webxr", webxr_visible);
  // Update state.
  webxr_visible_ = webxr_visible;
  overlay_visible_ = overlay_visible;
  if (pending_frame_) {
    pending_frame_->waiting_for_webxr_ =
        pending_frame_->waiting_for_webxr_ && webxr_visible;
    pending_frame_->waiting_for_overlay_ =
        pending_frame_->waiting_for_overlay_ && overlay_visible;
  }

  graphics_binding_->SetOverlayAndWebXrVisibility(overlay_visible,
                                                  webxr_visible);

  // Maybe composite and submit if we have a pending that is now valid to
  // submit.
  MaybeCompositeAndSubmit();
}

void OpenXrRenderLoop::RequestNotificationOnWebXrSubmitted(
    RequestNotificationOnWebXrSubmittedCallback callback) {
  on_webxr_submitted_ = std::move(callback);
}

void OpenXrRenderLoop::SendFrameData(
    XRFrameDataProvider::GetFrameDataCallback callback,
    mojom::XRFrameDataPtr frame_data) {
  DVLOG(3) << __func__;
  TRACE_EVENT0("xr", "SendFrameData");

  // This method represents a call from the renderer process. If our visibility
  // state is hidden, we should avoid handing "sensitive" information, like the
  // pose back up to the renderer. Note that this check is done here as other
  // methods (RequestNextOverlayPose) represent a call from the browser process,
  // which should receive the pose.
  bool is_visible =
      (visibility_state_ != device::mojom::XRVisibilityState::HIDDEN);

  // We have posted a message to allow other calls to get through, and now state
  // may have changed.  WebXR may not be presenting any more, or may be hidden.
  std::move(callback).Run(is_presenting_ && is_visible &&
                                  (webxr_visible_ || on_webxr_submitted_)
                              ? std::move(frame_data)
                              : mojom::XRFrameData::New());
}

mojom::XRFrameDataPtr OpenXrRenderLoop::GetNextFrameData() {
  DVLOG(3) << __func__;
  mojom::XRFrameDataPtr frame_data = mojom::XRFrameData::New();
  frame_data->frame_id = next_frame_id_;

  if (XR_FAILED(openxr_->BeginFrame())) {
    return frame_data;
  }

  // TODO(crbug.com/40909689): Make SwapchainInfo purely internal to the
  // graphics bindings so that this isn't necessary here.
  const auto& swap_chain_info = graphics_binding_->GetActiveSwapchainImage();
  if (swap_chain_info.shared_image) {
    frame_data->buffer_shared_image = swap_chain_info.shared_image->Export();
    frame_data->buffer_sync_token = swap_chain_info.sync_token;
  }

  const XrTime frame_time = openxr_->GetPredictedDisplayTime();

  frame_data->time_delta = base::Nanoseconds(frame_time);
  frame_data->views = openxr_->GetViews();
  frame_data->input_state = openxr_->GetInputState();

  frame_data->mojo_from_viewer = openxr_->GetViewerPose();

  UpdateStageParameters();

  if (openxr_->HasFrameState()) {
    OpenXrAnchorManager* anchor_manager = openxr_->GetAnchorManager();

    if (anchor_manager) {
      frame_data->anchors_data = anchor_manager->ProcessAnchorsForFrame(
          openxr_.get(), current_stage_parameters_,
          frame_data->input_state.value(), frame_time);
    }

    OpenXrLightEstimator* light_estimator = openxr_->GetLightEstimator();

    if (light_estimator) {
      frame_data->light_estimation_data =
          light_estimator->GetLightEstimate(frame_time);
    }
  }

  OpenXRSceneUnderstandingManager* scene_understanding_manager =
      openxr_->GetSceneUnderstandingManager();

  if (scene_understanding_manager && frame_data->mojo_from_viewer->position &&
      frame_data->mojo_from_viewer->orientation) {
    scene_understanding_manager->OnFrameUpdate(frame_time);
    device::Pose mojo_from_viewer(*frame_data->mojo_from_viewer->position,
                                  *frame_data->mojo_from_viewer->orientation);
    // Get results for hit test subscriptions.
    frame_data->hit_test_subscription_results =
        scene_understanding_manager->GetHitTestResults(
            mojo_from_viewer.ToTransform(), frame_data->input_state.value());
  }

  OpenXrDepthSensor* depth_sensor = openxr_->GetDepthSensor();
  if (depth_sensor) {
    depth_sensor->PopulateDepthData(frame_time, frame_data->views);
  }

  return frame_data;
}

// StartRuntime is called by OpenXrRenderLoop::RequestSession. When the
// runtime is fully started, start_runtime_callback.Run must be called with a
// success boolean, or false on failure. OpenXrRenderLoop::StartRuntime waits
// until the Viz context provider is fully started before running
// start_runtime_callback.
void OpenXrRenderLoop::StartRuntime(
    base::RepeatingCallback<void(mojom::XRVisibilityState)>
        on_visibility_state_changed,
    mojom::XRRuntimeSessionOptionsPtr options) {
  DCHECK(instance_ != XR_NULL_HANDLE);
  DCHECK(!openxr_);

  graphics_binding_ = platform_helper_->GetGraphicsBinding();

  if (!graphics_binding_) {
    DVLOG(1) << "Could not create graphics binding";
    // We aren't actually presenting yet; so ExitPresent won't clean us up.
    // Still call it to log the failure reason; but also explicitly call
    // StopRuntime, which should be resilient to duplicate calls.
    ExitPresent(ExitXrPresentReason::kStartRuntimeFailed);
    StopRuntime();
    MaybeRejectSessionCallback();
    return;
  }

  openxr_ = OpenXrApiWrapper::Create(instance_, graphics_binding_.get());
  if (!openxr_) {
    DVLOG(1) << __func__ << " Could not create OpenXrApiWrapper";
    MaybeRejectSessionCallback();
    return;
  }

  SessionStartedCallback on_session_started_callback = base::BindOnce(
      &OpenXrRenderLoop::OnOpenXrSessionStarted, weak_ptr_factory_.GetWeakPtr(),
      std::move(on_visibility_state_changed));
  SessionEndedCallback on_session_ended_callback = base::BindRepeating(
      &OpenXrRenderLoop::ExitPresent, weak_ptr_factory_.GetWeakPtr());
  VisibilityChangedCallback on_visibility_state_changed_callback =
      base::BindRepeating(&OpenXrRenderLoop::SetVisibilityState,
                          weak_ptr_factory_.GetWeakPtr());
  if (XR_FAILED(openxr_->InitSession(
          std::move(options), *extension_helper_,
          std::move(on_session_started_callback),
          std::move(on_session_ended_callback),
          std::move(on_visibility_state_changed_callback)))) {
    DVLOG(1) << __func__ << " InitSession Failed";
    // We aren't actually presenting yet; so ExitPresent won't clean us up.
    // Still call it to log the failure reason; but also explicitly call
    // StopRuntime, which should be resilient to duplicate calls.
    ExitPresent(ExitXrPresentReason::kStartRuntimeFailed);
    StopRuntime();
  }
}

void OpenXrRenderLoop::MaybeRejectSessionCallback() {
  if (request_session_callback_) {
    std::move(request_session_callback_)
        .Run(false, nullptr, mojo::PendingRemote<mojom::ImmersiveOverlay>());
  }
}

void OpenXrRenderLoop::OnOpenXrSessionStarted(
    base::RepeatingCallback<void(mojom::XRVisibilityState)>
        on_visibility_state_changed,
    mojom::XRRuntimeSessionOptionsPtr options,
    XrResult result) {
  DVLOG(1) << __func__ << " Result: " << result;
  if (XR_FAILED(result)) {
    // We aren't actually presenting yet; so ExitPresent won't clean us up.
    // Still call it to log the failure reason; but also explicitly call
    // StopRuntime, which should be resilient to duplicate calls.
    ExitPresent(ExitXrPresentReason::kOpenXrStartFailed);

    // We're only called from the OpenXrApiWrapper, which StopRuntime will
    // destroy. To prevent some re-entrant behavior, yield to let it finish
    // anything it's doing from before it called us before we stop the runtime.
    task_runner()->PostTask(FROM_HERE,
                            base::BindOnce(&OpenXrRenderLoop::StopRuntime,
                                           weak_ptr_factory_.GetWeakPtr()));

    // Technically until the StopRuntime task is called we can't service another
    // session request, which theoretically could come in once we reject the
    // session callback. Post a task to run it so that it runs after StopRuntime
    // to avoid this potential (albeit unlikely) race.
    // `MaybeRejectSessionCallback` will ensure it's run on the appropriate
    // thread. base::Unretained is safe here since we own and ensure the
    // task_runner() stops before destruction.
    task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&OpenXrRenderLoop::MaybeRejectSessionCallback,
                                  base::Unretained(this)));

    return;
  }

  StartContextProviderIfNeeded(base::BindOnce(
      &OpenXrRenderLoop::StartRuntimeFinish, weak_ptr_factory_.GetWeakPtr(),
      std::move(on_visibility_state_changed), std::move(options)));
}

void OpenXrRenderLoop::StopRuntime() {
  openxr_ = nullptr;
  // Need to destroy the graphics binding after the OpenXrApiWrapper, which
  // depends on it.
  graphics_binding_.reset();
  context_provider_.reset();
}

bool OpenXrRenderLoop::HasSessionEnded() {
  return openxr_ && openxr_->UpdateAndGetSessionEnded();
}

bool OpenXrRenderLoop::SubmitCompositedFrame() {
  return XR_SUCCEEDED(openxr_->EndFrame());
}

void OpenXrRenderLoop::SubmitFrame(int16_t frame_index,
                                   const gpu::MailboxHolder& mailbox,
                                   base::TimeDelta time_waited) {
  DVLOG(3) << __func__ << " frame_index=" << frame_index;
  CHECK(!graphics_binding_->IsUsingSharedImages());
  DCHECK(BUILDFLAG(IS_ANDROID));
  // TODO(crbug.com/40917172): Support non-shared buffer mode.
  SubmitFrameMissing(frame_index, mailbox.sync_token);
}

void OpenXrRenderLoop::SubmitFrameDrawnIntoTexture(
    int16_t frame_index,
    const gpu::SyncToken& sync_token,
    base::TimeDelta time_waited) {
  DVLOG(3) << __func__ << " frame_index=" << frame_index;
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
  // openxr_ and context_provider can be nullptr if we receive
  // OnWebXrTokenSignaled after the session has ended. Ensure we don't crash in
  // that case.
  if (!openxr_ || !context_provider_) {
    return;
  }

  if (!graphics_binding_->WaitOnFence(*gpu_fence)) {
    return;
  }

  // TODO(crbug.com/40917174): Unify OpenXr Rendering paths.
#if BUILDFLAG(IS_WIN)
  SubmitFrameWithTextureHandle(frame_index, mojo::PlatformHandle(),
                               gpu::SyncToken());
#elif BUILDFLAG(IS_ANDROID)
  MarkFrameSubmitted(frame_index);
  MaybeCompositeAndSubmit();
#endif

  // Calling SubmitFrameWithTextureHandle can cause openxr_ and
  // context_provider_ to become nullptr if we decide to stop the runtime.
  if (context_provider_) {
    gpu::gles2::GLES2Interface* gl = context_provider_->ContextGL();
    gl->DestroyGpuFenceCHROMIUM(id);
  }
}

void OpenXrRenderLoop::UpdateStageParameters() {
  std::vector<gfx::Point3F> stage_bounds;
  gfx::Transform local_from_stage;
  if (openxr_->GetStageParameters(stage_bounds, local_from_stage)) {
    mojom::VRStageParametersPtr stage_parameters =
        mojom::VRStageParameters::New();
    // mojo_from_local is currently identity.
    gfx::Transform mojo_from_local;
    // stage_from_floor is identity.
    gfx::Transform stage_from_floor;
    stage_parameters->mojo_from_floor =
        mojo_from_local * local_from_stage * stage_from_floor;
    stage_parameters->bounds = std::move(stage_bounds);
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
  DVLOG(2) << __func__ << ": ray origin=" << ray->origin.ToString()
           << ", ray direction=" << ray->direction.ToString();

  OpenXRSceneUnderstandingManager* scene_understanding_manager =
      openxr_->GetSceneUnderstandingManager();

  if (!scene_understanding_manager) {
    std::move(callback).Run(
        device::mojom::SubscribeToHitTestResult::FAILURE_GENERIC, 0);
    return;
  }

  std::optional<HitTestSubscriptionId> maybe_subscription_id =
      scene_understanding_manager->SubscribeToHitTest(
          std::move(native_origin_information), entity_types, std::move(ray));

  if (!maybe_subscription_id) {
    std::move(callback).Run(
        device::mojom::SubscribeToHitTestResult::FAILURE_GENERIC, 0);
    return;
  }

  DVLOG(2) << __func__ << ": subscription_id=" << *maybe_subscription_id;
  std::move(callback).Run(device::mojom::SubscribeToHitTestResult::SUCCESS,
                          maybe_subscription_id->GetUnsafeValue());
}

void OpenXrRenderLoop::SubscribeToHitTestForTransientInput(
    const std::string& profile_name,
    const std::vector<mojom::EntityTypeForHitTest>& entity_types,
    mojom::XRRayPtr ray,
    mojom::XREnvironmentIntegrationProvider::
        SubscribeToHitTestForTransientInputCallback callback) {
  DVLOG(2) << __func__ << ": ray origin=" << ray->origin.ToString()
           << ", ray direction=" << ray->direction.ToString();

  OpenXRSceneUnderstandingManager* scene_understanding_manager =
      openxr_->GetSceneUnderstandingManager();

  if (!scene_understanding_manager) {
    std::move(callback).Run(
        device::mojom::SubscribeToHitTestResult::FAILURE_GENERIC, 0);
    return;
  }

  std::optional<HitTestSubscriptionId> maybe_subscription_id =
      scene_understanding_manager->SubscribeToHitTestForTransientInput(
          profile_name, entity_types, std::move(ray));

  if (!maybe_subscription_id) {
    std::move(callback).Run(
        device::mojom::SubscribeToHitTestResult::FAILURE_GENERIC, 0);
    return;
  }

  DVLOG(2) << __func__ << ": subscription_id=" << *maybe_subscription_id;
  std::move(callback).Run(device::mojom::SubscribeToHitTestResult::SUCCESS,
                          maybe_subscription_id->GetUnsafeValue());
}

void OpenXrRenderLoop::UnsubscribeFromHitTest(uint64_t subscription_id) {
  DVLOG(2) << __func__;
  OpenXRSceneUnderstandingManager* scene_understanding_manager =
      openxr_->GetSceneUnderstandingManager();
  if (scene_understanding_manager)
    scene_understanding_manager->UnsubscribeFromHitTest(
        HitTestSubscriptionId(subscription_id));
}

void OpenXrRenderLoop::CreateAnchor(
    mojom::XRNativeOriginInformationPtr native_origin_information,
    const device::Pose& native_origin_from_anchor,
    CreateAnchorCallback callback) {
  OpenXrAnchorManager* anchor_manager = openxr_->GetAnchorManager();
  if (!anchor_manager) {
    return;
  }
  anchor_manager->AddCreateAnchorRequest(*native_origin_information,
                                         native_origin_from_anchor,
                                         std::move(callback));
}

void OpenXrRenderLoop::CreatePlaneAnchor(
    mojom::XRNativeOriginInformationPtr native_origin_information,
    const device::Pose& native_origin_from_anchor,
    uint64_t plane_id,
    CreatePlaneAnchorCallback callback) {
  environment_receiver_.ReportBadMessage(
      "OpenXrRenderLoop::CreatePlaneAnchor not yet implemented");
}

void OpenXrRenderLoop::DetachAnchor(uint64_t anchor_id) {
  OpenXrAnchorManager* anchor_manager = openxr_->GetAnchorManager();
  if (!anchor_manager) {
    return;
  }
  anchor_manager->DetachAnchor(AnchorId(anchor_id));
}

void OpenXrRenderLoop::StartContextProviderIfNeeded(
    ContextProviderAcquiredCallback callback) {
  DCHECK(task_runner()->BelongsToCurrentThread());
  DCHECK_EQ(context_provider_, nullptr);
  // We could arrive here in scenarios where we've shutdown the render loop or
  // runtime. In that case, there is no need to start the context provider.
  // If openxr_ has been torn down the context provider is unnecessary as
  // there is nothing to connect to the GPU process.
  if (openxr_) {
    auto created_callback =
        base::BindOnce(&OpenXrRenderLoop::OnContextProviderCreated,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback));

    main_thread_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            context_provider_factory_async_,
            base::BindPostTask(task_runner(), std::move(created_callback))));
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

  if (openxr_) {
    openxr_->OnContextProviderLost();
  }

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
// given to it from it's caller. OnContextProviderCreated must run the
// start_runtime_callback, passing true on successful call to
// BindToCurrentSequence and false if not.
void OpenXrRenderLoop::OnContextProviderCreated(
    ContextProviderAcquiredCallback start_runtime_callback,
    scoped_refptr<viz::ContextProvider> context_provider) {
  DCHECK(task_runner()->BelongsToCurrentThread());
  DCHECK_EQ(context_provider_, nullptr);

  const gpu::ContextResult context_result =
      context_provider->BindToCurrentSequence();
  if (context_result != gpu::ContextResult::kSuccess) {
    DVLOG(1) << __func__ << " Could not get context provider";
    std::move(start_runtime_callback).Run(false);
    return;
  }

  if (openxr_) {
    openxr_->OnContextProviderCreated(context_provider);
  }

  context_provider->AddObserver(this);
  context_provider_ = std::move(context_provider);

  std::move(start_runtime_callback).Run(true);
}

}  // namespace device
