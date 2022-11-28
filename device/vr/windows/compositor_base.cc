// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/windows/compositor_base.h"

#include "base/bind.h"
#include "base/trace_event/trace_event.h"
#include "base/types/cxx23_to_underlying.h"
#include "build/build_config.h"
#include "components/viz/common/gpu/context_provider.h"
#include "ui/gfx/geometry/angle_conversions.h"
#include "ui/gfx/geometry/transform.h"

#if BUILDFLAG(IS_WIN)
#include "device/vr/windows/d3d11_texture_helper.h"
#endif

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

mojom::XRFrameDataPtr XRDeviceAbstraction::GetNextFrameData() {
  return nullptr;
}
void XRDeviceAbstraction::OnSessionStart() {}
void XRDeviceAbstraction::HandleDeviceLost() {}
bool XRDeviceAbstraction::HasSessionEnded() {
  return false;
}
void XRDeviceAbstraction::OnLayerBoundsChanged() {}
device::mojom::XREnvironmentBlendMode
XRDeviceAbstraction::GetEnvironmentBlendMode(
    device::mojom::XRSessionMode session_mode) {
  return device::mojom::XREnvironmentBlendMode::kOpaque;
}
device::mojom::XRInteractionMode XRDeviceAbstraction::GetInteractionMode(
    device::mojom::XRSessionMode session_mode) {
  return device::mojom::XRInteractionMode::kWorldSpace;
}
bool XRDeviceAbstraction::CanEnableAntiAliasing() const {
  return true;
}

XRCompositorCommon::OutstandingFrame::OutstandingFrame() = default;
XRCompositorCommon::OutstandingFrame::~OutstandingFrame() = default;

XRCompositorCommon::XRCompositorCommon()
    : base::Thread("WindowsXRCompositor"),
      main_thread_task_runner_(
          base::SingleThreadTaskRunner::GetCurrentDefault()),
      webxr_js_time_(kSlidingAverageSize),
      webxr_gpu_time_(kSlidingAverageSize) {
  DCHECK(main_thread_task_runner_);
}

XRCompositorCommon::~XRCompositorCommon() {
  // Since we derive from base::Thread, all derived classes must call Stop() in
  // their destructor so the thread gets torn down before any members that may
  // be in use on the thread get torn down.
}

void XRCompositorCommon::ClearPendingFrame() {
  // Notify the derived class first so it can clear its pending frame before
  // potentially starting a new frame with delayed_get_frame_data_callback_.
  ClearPendingFrameInternal();

  pending_frame_.reset();
  // Send frame data to outstanding requests.
  if (delayed_get_frame_data_callback_ &&
      (webxr_visible_ || on_webxr_submitted_)) {
    // If WebXR is not visible, but the browser wants to know when it submits a
    // frame, we allow the renderer to receive poses.
    std::move(delayed_get_frame_data_callback_).Run();
  }
}

bool XRCompositorCommon::IsUsingSharedImages() const {
  return false;
}

void XRCompositorCommon::SubmitFrameMissing(int16_t frame_index,
                                            const gpu::SyncToken& sync_token) {
  TRACE_EVENT_INSTANT0("xr", "SubmitFrameMissing", TRACE_EVENT_SCOPE_THREAD);
  if (pending_frame_) {
    // WebXR for this frame is hidden.
    pending_frame_->waiting_for_webxr_ = false;
  }
  webxr_has_pose_ = false;
  MaybeCompositeAndSubmit();
}

void XRCompositorCommon::SubmitFrame(int16_t frame_index,
                                     const gpu::MailboxHolder& mailbox,
                                     base::TimeDelta time_waited) {
  NOTREACHED();
}

void XRCompositorCommon::SubmitFrameDrawnIntoTexture(
    int16_t frame_index,
    const gpu::SyncToken& sync_token,
    base::TimeDelta time_waited) {
  NOTREACHED();
}

#if BUILDFLAG(IS_WIN)
void XRCompositorCommon::SubmitFrameWithTextureHandle(
    int16_t frame_index,
    mojo::PlatformHandle texture_handle,
    const gpu::SyncToken& sync_token) {
  TRACE_EVENT1("xr", "SubmitFrameWithTextureHandle", "frameIndex", frame_index);
  webxr_has_pose_ = false;
  // Tell the browser that WebXR has submitted a frame.
  if (on_webxr_submitted_)
    std::move(on_webxr_submitted_).Run();

  if (!pending_frame_ ||
      pending_frame_->render_info_->frame_id != frame_index) {
    // We weren't expecting a submitted frame.  This can happen if WebXR was
    // hidden by an overlay for some time.
    if (submit_client_) {
      submit_client_->OnSubmitFrameTransferred(false);
      submit_client_->OnSubmitFrameRendered();
      TRACE_EVENT1("xr", "SubmitFrameTransferred", "success", false);
    }
    return;
  }

  pending_frame_->waiting_for_webxr_ = false;
  pending_frame_->submit_frame_time_ = base::TimeTicks::Now();

  base::win::ScopedHandle scoped_handle = texture_handle.is_valid()
                                              ? texture_handle.TakeHandle()
                                              : base::win::ScopedHandle();
  texture_helper_.SetSourceTexture(std::move(scoped_handle), sync_token,
                                   left_webxr_bounds_, right_webxr_bounds_);
  pending_frame_->webxr_submitted_ = true;

  // Regardless of success - try to composite what we have.
  MaybeCompositeAndSubmit();
}
#endif

void XRCompositorCommon::CleanUp() {
  submit_client_.reset();
  webxr_has_pose_ = false;
  presentation_receiver_.reset();
  frame_data_receiver_.reset();
  overlay_receiver_.reset();
  input_event_listener_.reset();
  StopRuntime();
}

void XRCompositorCommon::RequestOverlay(
    mojo::PendingReceiver<mojom::ImmersiveOverlay> receiver) {
  overlay_receiver_.reset();
  overlay_receiver_.Bind(std::move(receiver));

  // WebXR is visible and overlay hidden by default until the overlay overrides
  // this.
  SetOverlayAndWebXRVisibility(false, true);
}

bool XRCompositorCommon::UsesInputEventing() {
  // By default we don't use input eventing.  Any subclass that does will need
  // to override this.
  return false;
}

void XRCompositorCommon::UpdateLayerBounds(int16_t frame_id,
                                           const gfx::RectF& left_bounds,
                                           const gfx::RectF& right_bounds,
                                           const gfx::Size& source_size) {
  // Bounds are updated instantly, rather than waiting for frame_id.  This works
  // since blink always passes the current frame_id when updating the bounds.
  // Ignoring the frame_id keeps the logic simpler, so this can more easily
  // merge with vr_shell_gl eventually.
  left_webxr_bounds_ = left_bounds;
  right_webxr_bounds_ = right_bounds;

  // Swap top/bottom to account for differences between D3D and GL coordinates.
  left_webxr_bounds_.set_y(
      1 - (left_webxr_bounds_.y() + left_webxr_bounds_.height()));
  right_webxr_bounds_.set_y(
      1 - (right_webxr_bounds_.y() + right_webxr_bounds_.height()));

  source_size_ = source_size;

  OnLayerBoundsChanged();
}

void XRCompositorCommon::RequestSession(
    base::RepeatingCallback<void(mojom::XRVisibilityState)>
        on_visibility_state_changed,
    mojom::XRRuntimeSessionOptionsPtr options,
    RequestSessionCallback callback) {
  webxr_has_pose_ = false;
  presentation_receiver_.reset();
  frame_data_receiver_.reset();

  EnableSupportedFeatures(options->required_features,
                          options->optional_features);

  // Call the subclass's StartRuntime method. Upon completion, StartRuntime will
  // call the callback passed to its first parameter, start_runtime_callback.
  // XRCompositorCommon::StartRuntimeFinish. We setup BindOnce such that all of
  // the parameters give to us here in XRCompositorCommon::RequestSession are
  // passed through to StartRuntimeFinish so that it can finish the job.
  StartRuntime(base::BindOnce(&XRCompositorCommon::StartRuntimeFinish,
                              base::Unretained(this),
                              std::move(on_visibility_state_changed),
                              std::move(options), std::move(callback)));
}

void XRCompositorCommon::StartRuntimeFinish(
    base::RepeatingCallback<void(mojom::XRVisibilityState)>
        on_visibility_state_changed,
    mojom::XRRuntimeSessionOptionsPtr options,
    RequestSessionCallback callback,
    bool success) {
  if (!success) {
    TRACE_EVENT_INSTANT0("xr", "Failed to start runtime",
                         TRACE_EVENT_SCOPE_THREAD);
    main_thread_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), false, nullptr));
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

  if (IsUsingSharedImages()) {
    transport_options->transport_method =
        device::mojom::XRPresentationTransportMethod::DRAW_INTO_TEXTURE_MAILBOX;
  } else {
    transport_options->transport_method =
        device::mojom::XRPresentationTransportMethod::SUBMIT_AS_TEXTURE_HANDLE;
  }

  // Only set boolean options that we need. Default is false, and we should be
  // able to safely ignore ones that our implementation doesn't care about.
  transport_options->wait_for_transfer_notification = true;

  OnSessionStart();

  auto submit_frame_sink = device::mojom::XRPresentationConnection::New();
  submit_frame_sink->provider =
      presentation_receiver_.BindNewPipeAndPassRemote();
  submit_frame_sink->client_receiver =
      submit_client_.BindNewPipeAndPassReceiver();
  submit_frame_sink->transport_options = std::move(transport_options);

  auto session = device::mojom::XRSession::New();
  session->data_provider = frame_data_receiver_.BindNewPipeAndPassRemote();
  session->submit_frame_sink = std::move(submit_frame_sink);

  session->enabled_features.insert(session->enabled_features.end(),
                                   enabled_features_.begin(),
                                   enabled_features_.end());

  session->device_config = device::mojom::XRSessionDeviceConfig::New();
  session->device_config->uses_input_eventing = UsesInputEventing();
  session->device_config->enable_anti_aliasing = CanEnableAntiAliasing();
  session->device_config->views = GetDefaultViews();
  session->enviroment_blend_mode = GetEnvironmentBlendMode(options->mode);
  session->interaction_mode = GetInteractionMode(options->mode);

  main_thread_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), true, std::move(session)));
  is_presenting_ = true;

  texture_helper_.SetSourceAndOverlayVisible(webxr_visible_, overlay_visible_);
}

void XRCompositorCommon::ExitPresent(ExitXrPresentReason reason) {
  TRACE_EVENT_INSTANT1("xr", "ExitPresent", TRACE_EVENT_SCOPE_THREAD, "reason",
                       base::to_underlying(reason));
  if (!is_presenting_)
    return;

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

  texture_helper_.SetSourceAndOverlayVisible(false, false);

  // Don't call StopRuntime until this thread has finished the rest of the work.
  // This is to prevent the OpenXrApiWrapper from being deleted before its
  // cleanup work has finished.
  task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&XRCompositorCommon::StopRuntime, base::Unretained(this)));
}

void XRCompositorCommon::SetVisibilityState(
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

const mojom::VRStageParametersPtr&
XRCompositorCommon::GetCurrentStageParameters() const {
  return current_stage_parameters_;
}

void XRCompositorCommon::SetStageParameters(
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

void XRCompositorCommon::Init() {}

void XRCompositorCommon::StartPendingFrame() {
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

void XRCompositorCommon::GetFrameData(
    mojom::XRFrameDataRequestOptionsPtr options,
    mojom::XRFrameDataProvider::GetFrameDataCallback callback) {
  TRACE_EVENT0("xr", "GetFrameData");
  if (HasSessionEnded()) {
    ExitPresent(ExitXrPresentReason::kGetFrameAfterSessionEnded);
    return;
  }

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
    delayed_get_frame_data_callback_ = base::BindOnce(
        &XRCompositorCommon::GetFrameData, base::Unretained(this),
        std::move(options), std::move(callback));
    return;
  }

  StartPendingFrame();
  webxr_has_pose_ = true;
  pending_frame_->webxr_has_pose_ = true;
  pending_frame_->sent_frame_data_time_ = base::TimeTicks::Now();

  // TODO(https://crbug.com/1218135): The lack of frame_data_ here indicates
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
      FROM_HERE, base::BindOnce(&XRCompositorCommon::SendFrameData,
                                base::Unretained(this), std::move(callback),
                                std::move(pending_frame_->frame_data_)));

  next_frame_id_ += 1;
  if (next_frame_id_ < 0) {
    next_frame_id_ = 0;
  }
}

void XRCompositorCommon::SetInputSourceButtonListener(
    mojo::PendingAssociatedRemote<device::mojom::XRInputSourceButtonListener>
        input_listener_remote) {
  DCHECK(UsesInputEventing());
  input_event_listener_.reset();
  input_event_listener_.Bind(std::move(input_listener_remote));
}

void XRCompositorCommon::SendFrameData(
    XRFrameDataProvider::GetFrameDataCallback callback,
    mojom::XRFrameDataPtr frame_data) {
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

void XRCompositorCommon::GetEnvironmentIntegrationProvider(
    mojo::PendingAssociatedReceiver<
        device::mojom::XREnvironmentIntegrationProvider> environment_provider) {
  // Environment integration is not supported. This call should not
  // be made on this device.
  mojo::ReportBadMessage("Environment integration is not supported.");
}

void XRCompositorCommon::SubmitOverlayTexture(
    int16_t frame_id,
    mojo::PlatformHandle texture_handle,
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

#if BUILDFLAG(IS_WIN)
  texture_helper_.SetOverlayTexture(texture_handle.TakeHandle(), sync_token,
                                    left_bounds, right_bounds);
  pending_frame_->overlay_submitted_ = true;

  // Regardless of success - try to composite what we have.
  MaybeCompositeAndSubmit();
#endif
}

void XRCompositorCommon::RequestNextOverlayPose(
    RequestNextOverlayPoseCallback callback) {
  // We will only request poses while the overlay is visible.
  DCHECK(overlay_visible_);
  TRACE_EVENT_INSTANT0("xr", "RequestOverlayPose", TRACE_EVENT_SCOPE_THREAD);

  // Ensure we have a pending frame.
  StartPendingFrame();
  pending_frame_->overlay_has_pose_ = true;
  std::move(callback).Run(pending_frame_->render_info_->Clone());
}

void XRCompositorCommon::SetOverlayAndWebXRVisibility(bool overlay_visible,
                                                      bool webxr_visible) {
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

  // Update texture helper.
  texture_helper_.SetSourceAndOverlayVisible(webxr_visible, overlay_visible);

  // Maybe composite and submit if we have a pending that is now valid to
  // submit.
  MaybeCompositeAndSubmit();
}

void XRCompositorCommon::RequestNotificationOnWebXrSubmitted(
    RequestNotificationOnWebXrSubmittedCallback callback) {
  on_webxr_submitted_ = std::move(callback);
}

void XRCompositorCommon::MaybeCompositeAndSubmit() {
  if (!pending_frame_) {
    // There is no outstanding frame, nor frame to composite, but there may be
    // pending GetFrameData calls, so ClearPendingFrame() to respond to them.
    ClearPendingFrame();
    return;
  }

  // Check if we have obtained all layers (overlay and webxr) that we need.
  if (pending_frame_->waiting_for_webxr_ ||
      pending_frame_->waiting_for_overlay_) {
    // Haven't received submits from all layers.
    return;
  }

  bool no_submit = false;
  if (!(pending_frame_->webxr_submitted_ && webxr_visible_) &&
      !(pending_frame_->overlay_submitted_ && overlay_visible_)) {
    // Nothing visible was submitted - we can't composite/submit to headset.
    no_submit = true;
  }

  bool copy_successful;

  // If so, tell texture helper to composite, then grab the output texture, and
  // submit. If we submitted, set up the next frame, and send outstanding pose
  // requests.
  if (no_submit) {
    copy_successful = false;
    texture_helper_.CleanupNoSubmit();
  } else {
    copy_successful = texture_helper_.UpdateBackbufferSizes() &&
                      texture_helper_.CompositeToBackBuffer();
    if (copy_successful) {
      pending_frame_->frame_ready_time_ = base::TimeTicks::Now();
      if (!SubmitCompositedFrame()) {
        ExitPresent(ExitXrPresentReason::kSubmitFrameFailed);
        // ExitPresent() clears pending_frame_, so return here to avoid
        // accessing it below.
        return;
      }
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

}  // namespace device
