// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/windows/compositor_base.h"

#include "base/bind.h"
#include "base/trace_event/trace_event.h"
#include "ui/gfx/geometry/angle_conversions.h"
#include "ui/gfx/transform.h"

#if defined(OS_WIN)
#include "device/vr/windows/d3d11_texture_helper.h"
#endif

namespace {
// Number of frames to use for sliding averages for pose timings,
// as used for estimating prediction times.
constexpr unsigned kSlidingAverageSize = 5;
}  // namespace

namespace device {

mojom::XRFrameDataPtr XRDeviceAbstraction::GetNextFrameData() {
  return nullptr;
}
void XRDeviceAbstraction::OnSessionStart() {}
void XRDeviceAbstraction::HandleDeviceLost() {}
bool XRDeviceAbstraction::PreComposite() {
  return true;
}
bool XRDeviceAbstraction::HasSessionEnded() {
  return false;
}
void XRDeviceAbstraction::OnLayerBoundsChanged() {}

XRCompositorCommon::OutstandingFrame::OutstandingFrame() = default;
XRCompositorCommon::OutstandingFrame::~OutstandingFrame() = default;

XRCompositorCommon::XRCompositorCommon()
    : base::Thread("WindowsXRCompositor"),
      main_thread_task_runner_(base::ThreadTaskRunnerHandle::Get()),
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
  pending_frame_.reset();
  // Send frame data to outstanding requests.
  if (delayed_get_frame_data_callback_ &&
      (webxr_visible_ || on_webxr_submitted_)) {
    // If WebXR is not visible, but the browser wants to know when it submits a
    // frame, we allow the renderer to receive poses.
    std::move(delayed_get_frame_data_callback_).Run();
  }
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
  // Not currently implemented for Windows.
  NOTREACHED();
}

void XRCompositorCommon::SubmitFrameWithTextureHandle(
    int16_t frame_index,
    mojo::ScopedHandle texture_handle) {
  TRACE_EVENT1("xr", "SubmitFrameWithTextureHandle", "frameIndex", frame_index);
  webxr_has_pose_ = false;
  // Tell the browser that WebXR has submitted a frame.
  if (on_webxr_submitted_)
    std::move(on_webxr_submitted_).Run();

  if (!pending_frame_ || pending_frame_->frame_data_->frame_id != frame_index) {
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

#if defined(OS_WIN)
  MojoPlatformHandle platform_handle;
  platform_handle.struct_size = sizeof(platform_handle);
  MojoResult result = MojoUnwrapPlatformHandle(texture_handle.release().value(),
                                               nullptr, &platform_handle);
  if (result == MOJO_RESULT_OK) {
    texture_helper_.SetSourceTexture(
        base::win::ScopedHandle(
            reinterpret_cast<HANDLE>(platform_handle.value)),
        left_webxr_bounds_, right_webxr_bounds_);
    pending_frame_->webxr_submitted_ = true;
  }

  // Regardless of success - try to composite what we have.
  MaybeCompositeAndSubmit();
#endif
}

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
    base::OnceCallback<void()> on_presentation_ended,
    base::RepeatingCallback<void(mojom::XRVisibilityState)>
        on_visibility_state_changed,
    mojom::XRRuntimeSessionOptionsPtr options,
    RequestSessionCallback callback) {
  DCHECK(options->immersive);
  webxr_has_pose_ = false;
  presentation_receiver_.reset();
  frame_data_receiver_.reset();

  if (!StartRuntime()) {
    TRACE_EVENT_INSTANT0("xr", "Failed to start runtime",
                         TRACE_EVENT_SCOPE_THREAD);
    main_thread_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), false, nullptr));
    return;
  }

  // If on_presentation_ended_ is not already null, we won't call to notify the
  // runtime that that session has completed.  This is ok because the XRRuntime
  // knows it has requested a new session, and isn't expecting that callback to
  // be called.
  on_presentation_ended_ = std::move(on_presentation_ended);

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
  transport_options->transport_method =
      device::mojom::XRPresentationTransportMethod::SUBMIT_AS_TEXTURE_HANDLE;
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
  session->uses_input_eventing = UsesInputEventing();

  main_thread_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), true, std::move(session)));
  is_presenting_ = true;

  texture_helper_.SetSourceAndOverlayVisible(webxr_visible_, overlay_visible_);
}

void XRCompositorCommon::ExitPresent() {
  TRACE_EVENT_INSTANT0("xr", "ExitPresent", TRACE_EVENT_SCOPE_THREAD);
  is_presenting_ = false;
  webxr_has_pose_ = false;
  presentation_receiver_.reset();
  frame_data_receiver_.reset();
  submit_client_.reset();
  StopRuntime();

  pending_frame_.reset();
  delayed_get_frame_data_callback_.Reset();

  // Reset webxr_visible_ for subsequent presentations.
  webxr_visible_ = true;

  // Kill outstanding overlays:
  overlay_visible_ = false;
  overlay_receiver_.reset();

  texture_helper_.SetSourceAndOverlayVisible(false, false);

  if (on_presentation_ended_) {
    main_thread_task_runner_->PostTask(FROM_HERE,
                                       std::move(on_presentation_ended_));
  }
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

void XRCompositorCommon::Init() {}

void XRCompositorCommon::StartPendingFrame() {
  if (!pending_frame_) {
    pending_frame_.emplace();
    pending_frame_->waiting_for_webxr_ = webxr_visible_;
    pending_frame_->waiting_for_overlay_ = overlay_visible_;
    pending_frame_->frame_data_ = GetNextFrameData();
    // pending_frame_->frame_data_ should never be null
    DCHECK(pending_frame_->frame_data_);
  }
}

void XRCompositorCommon::GetFrameData(
    mojom::XRFrameDataRequestOptionsPtr options,
    mojom::XRFrameDataProvider::GetFrameDataCallback callback) {
  TRACE_EVENT0("xr", "GetFrameData");
  if (HasSessionEnded()) {
    ExitPresent();
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
      mojo::ReportBadMessage("Multiple outstanding GetFrameData calls");
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

  // Yield here to let the event queue process pending mojo messages,
  // specifically the next gamepad callback request that's likely to
  // have been sent during WaitGetPoses.
  task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&XRCompositorCommon::SendFrameData, base::Unretained(this),
                     std::move(callback), pending_frame_->frame_data_.Clone()));

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
    mojo::ScopedHandle texture_handle,
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

#if defined(OS_WIN)
  MojoPlatformHandle platform_handle;
  platform_handle.struct_size = sizeof(platform_handle);
  MojoResult result = MojoUnwrapPlatformHandle(texture_handle.release().value(),
                                               nullptr, &platform_handle);
  if (result == MOJO_RESULT_OK) {
    texture_helper_.SetOverlayTexture(
        base::win::ScopedHandle(
            reinterpret_cast<HANDLE>(platform_handle.value)),
        left_bounds, right_bounds);
    pending_frame_->overlay_submitted_ = true;
  } else {
    std::move(overlay_submit_callback_).Run(false);
  }

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
  std::move(callback).Run(pending_frame_->frame_data_.Clone());
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
                      PreComposite() && texture_helper_.CompositeToBackBuffer();
    if (copy_successful) {
      pending_frame_->frame_ready_time_ = base::TimeTicks::Now();
      if (!SubmitCompositedFrame()) {
        ExitPresent();
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
