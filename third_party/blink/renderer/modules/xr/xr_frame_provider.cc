// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_frame_provider.h"

#include <utility>

#include "base/not_fatal_until.h"
#include "build/build_config.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/dom/frame_request_callback_collection.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/navigator.h"
#include "third_party/blink/renderer/core/imagebitmap/image_bitmap.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/modules/xr/xr_session.h"
#include "third_party/blink/renderer/modules/xr/xr_system.h"
#include "third_party/blink/renderer/modules/xr/xr_viewport.h"
#include "third_party/blink/renderer/modules/xr/xr_webgl_layer.h"
#include "third_party/blink/renderer/platform/graphics/gpu/xr_frame_transport.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "ui/display/display.h"
#include "ui/gfx/geometry/transform.h"

namespace blink {

namespace {

class XRFrameProviderRequestCallback : public FrameCallback {
 public:
  explicit XRFrameProviderRequestCallback(XRFrameProvider* frame_provider)
      : frame_provider_(frame_provider) {}
  ~XRFrameProviderRequestCallback() override = default;
  void Invoke(double high_res_time_ms) override {
    frame_provider_->OnNonImmersiveVSync(high_res_time_ms);
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(frame_provider_);
    FrameCallback::Trace(visitor);
  }

  Member<XRFrameProvider> frame_provider_;
};

}  // namespace

XRFrameProvider::XRFrameProvider(XRSystem* xr)
    : xr_(xr),
      frame_transport_(MakeGarbageCollected<XRFrameTransport>(
          xr->GetExecutionContext(),
          xr->GetExecutionContext()->GetTaskRunner(
              TaskType::kMiscPlatformAPI))),
      immersive_data_provider_(xr->GetExecutionContext()),
      immersive_presentation_provider_(xr->GetExecutionContext()),
      last_has_focus_(xr->IsFrameFocused()) {}

void XRFrameProvider::AddImmersiveSessionObserver(
    ImmersiveSessionObserver* observer) {
  immersive_observers_.insert(observer);
}

void XRFrameProvider::OnSessionStarted(
    XRSession* session,
    device::mojom::blink::XRSessionPtr session_ptr) {
  DCHECK(session);

  if (session->immersive()) {
    // Ensure we can only have one immersive session at a time.
    DCHECK(!immersive_session_);
    DCHECK(session_ptr->data_provider);
    DCHECK(session_ptr->submit_frame_sink);
    immersive_session_ = session;

    for (auto& observer : immersive_observers_) {
      observer->OnImmersiveSessionStart();
    }

    immersive_data_provider_.Bind(
        std::move(session_ptr->data_provider),
        xr_->GetExecutionContext()->GetTaskRunner(TaskType::kMiscPlatformAPI));
    immersive_data_provider_.set_disconnect_handler(
        WTF::BindOnce(&XRFrameProvider::OnProviderConnectionError,
                      WrapWeakPersistent(this), WrapWeakPersistent(session)));

    immersive_presentation_provider_.Bind(
        std::move(session_ptr->submit_frame_sink->provider),
        xr_->GetExecutionContext()->GetTaskRunner(TaskType::kMiscPlatformAPI));
    immersive_presentation_provider_.set_disconnect_handler(
        WTF::BindOnce(&XRFrameProvider::OnProviderConnectionError,
                      WrapWeakPersistent(this), WrapWeakPersistent(session)));

    frame_transport_->RegisterFrameRenderedCallback(WTF::BindRepeating(
        &XRFrameProvider::OnRenderComplete, WrapWeakPersistent(this)));

    frame_transport_->BindSubmitFrameClient(
        std::move(session_ptr->submit_frame_sink->client_receiver));
    frame_transport_->SetTransportOptions(
        std::move(session_ptr->submit_frame_sink->transport_options));
    frame_transport_->PresentChange();

    last_frame_statistics_sent_time_ = base::TimeTicks::Now();


    repeating_timer_.Start(FROM_HERE, base::Seconds(1),
                           WTF::BindRepeating(&XRFrameProvider::SendFrameData,
                                              WrapWeakPersistent(this)));
  } else {
    // If a non-immersive session doesn't have a data provider, we don't
    // need to store a reference to it.
    if (!session_ptr->data_provider) {
      return;
    }

    HeapMojoRemote<device::mojom::blink::XRFrameDataProvider> data_provider(
        xr_->GetExecutionContext());
    data_provider.Bind(
        std::move(session_ptr->data_provider),
        xr_->GetExecutionContext()->GetTaskRunner(TaskType::kMiscPlatformAPI));
    data_provider.set_disconnect_handler(
        WTF::BindOnce(&XRFrameProvider::OnProviderConnectionError,
                      WrapWeakPersistent(this), WrapWeakPersistent(session)));

    non_immersive_data_providers_.insert(
        session, WrapDisallowNew(std::move(data_provider)));
  }
}

void XRFrameProvider::OnFocusChanged() {
  bool focus = xr_->IsFrameFocused();

  // If we are gaining focus, schedule a frame for magic window.  This accounts
  // for skipping RAFs in ProcessScheduledFrame.  Only do this when there are
  // magic window sessions but no immersive session. Note that immersive
  // sessions don't stop scheduling RAFs when focus is lost, so there is no need
  // to schedule immersive frames when focus is acquired.
  if (focus && !last_has_focus_ && requesting_sessions_.size() > 0 &&
      !immersive_session_) {
    ScheduleNonImmersiveFrame(nullptr);
  }
  last_has_focus_ = focus;
}

// Ends the immersive session when the presentation or immersive data provider
// got disconnected.
void XRFrameProvider::OnProviderConnectionError(XRSession* session) {
  DVLOG(2) << __func__;
  // This will call into |OnSessionEnded|, unless it has already ended.
  session->ForceEnd(XRSession::ShutdownPolicy::kImmediate);
}

void XRFrameProvider::OnSessionEnded(XRSession* session) {
  DVLOG(2) << __func__;
  if (session->immersive()) {
    DCHECK(session == immersive_session_);

    immersive_session_ = nullptr;
    pending_immersive_vsync_ = false;
    frame_id_ = -1;
    immersive_presentation_provider_.reset();
    immersive_data_provider_.reset();

    first_immersive_frame_time_ = std::nullopt;
    first_immersive_frame_time_delta_ = std::nullopt;

    frame_transport_ = MakeGarbageCollected<XRFrameTransport>(
        session->GetExecutionContext(),
        session->GetExecutionContext()->GetTaskRunner(
            TaskType::kMiscPlatformAPI));

    repeating_timer_.Stop();
    for (auto& observer : immersive_observers_) {
      observer->OnImmersiveSessionEnd();
    }
  } else {
    non_immersive_data_providers_.erase(session);
    requesting_sessions_.erase(session);
  }
}

void XRFrameProvider::RestartNonImmersiveFrameLoop() {
  // When we no longer have an active immersive session schedule all the
  // outstanding frames that were requested while the immersive session was
  // active.
  if (immersive_session_ || requesting_sessions_.size() == 0)
    return;

  for (auto& session : requesting_sessions_) {
    RequestNonImmersiveFrameData(session.key.Get());
  }

  ScheduleNonImmersiveFrame(nullptr);
}

// Schedule a session to be notified when the next XR frame is available.
void XRFrameProvider::RequestFrame(XRSession* session) {
  DVLOG(3) << __FUNCTION__;
  TRACE_EVENT0("gpu", __FUNCTION__);
  DCHECK(session);

  auto options = device::mojom::blink::XRFrameDataRequestOptions::New();
  options->include_lighting_estimation_data = session->LightEstimationEnabled();
  options->stage_parameters_id = session->StageParametersId();

  // Immersive frame logic.
  if (session->immersive()) {
    ScheduleImmersiveFrame(std::move(options));
    return;
  }

  // Non-immersive frame logic.

  // Duplicate frame requests are treated as a no-op.
  if (requesting_sessions_.Contains(session)) {
    DVLOG(2) << __FUNCTION__ << ": session requested duplicate frame";
    return;
  }

  requesting_sessions_.insert(session, nullptr);

  // If there's an active immersive session save the request but suppress
  // processing it until the immersive session is no longer active.
  if (immersive_session_) {
    return;
  }

  RequestNonImmersiveFrameData(session);
  ScheduleNonImmersiveFrame(std::move(options));
}

void XRFrameProvider::ScheduleImmersiveFrame(
    device::mojom::blink::XRFrameDataRequestOptionsPtr options) {
  TRACE_EVENT0("gpu", __FUNCTION__);
  if (pending_immersive_vsync_)
    return;

  pending_immersive_vsync_ = true;
  frame_data_time_.StartTimer();
  immersive_data_provider_->GetFrameData(
      std::move(options), WTF::BindOnce(&XRFrameProvider::OnImmersiveFrameData,
                                        WrapWeakPersistent(this)));
}

void XRFrameProvider::ScheduleNonImmersiveFrame(
    device::mojom::blink::XRFrameDataRequestOptionsPtr options) {
  DVLOG(3) << __FUNCTION__;
  TRACE_EVENT0("gpu", __FUNCTION__);

  DCHECK(!immersive_session_)
      << "Scheduling should be done via the exclusive session if present.";

  if (pending_non_immersive_vsync_) {
    DVLOG(3) << __FUNCTION__ << ": non immersive vsync already pending";
    return;
  }

  LocalDOMWindow* window = xr_->DomWindow();
  if (!window)
    return;

  pending_non_immersive_vsync_ = true;

  // Calls |OnNonImmersiveVSync|
  window->document()->RequestAnimationFrame(
      MakeGarbageCollected<XRFrameProviderRequestCallback>(this));
}

void XRFrameProvider::OnImmersiveFrameData(
    device::mojom::blink::XRFrameDataPtr data) {
  frame_data_time_.StopTimer();
  TRACE_EVENT0("gpu", __FUNCTION__);
  if (data.is_null()) {
    DVLOG(2) << __func__ << ": no data, current frame_id=" << frame_id_;
  } else {
    DVLOG(2) << __func__ << ": have data, frame_id=" << data->frame_id;
  }

  // We may have lost the immersive session since the last VSync request.
  if (!immersive_session_) {
    DVLOG(1) << __func__ << ": immersive session lost";
    return;
  }

  if (!data) {
    DVLOG(3) << __func__ << ": frame data not received, re-requesting frame";
    // We have not received any frame data from the device. We could try to run
    // an XR animation frame [1], but that may cause issues with APIs that
    // receive state updates via XRFrameData (e.g. anchors, planes) for
    // maintaining their current state - they would behave as if all entities
    // lost tracking and got removed on the device. Let's behave as if we have
    // not received anything from the device and request a new frame.
    //
    // [1]  https://immersive-web.github.io/webxr/#xr-animation-frame)

    pending_immersive_vsync_ = false;
    RequestFrame(immersive_session_);
    return;
  }

  LocalDOMWindow* window = xr_->DomWindow();
  if (!window) {
    DVLOG(3) << __func__ << ": unable to get local DOM window!";
    return;
  }

  // Note: The |high_res_now_ms| is computed based on frame time returned from
  // immersive frame and eventually passed in to requestAnimationFrame callback
  // as `DOMHighResTimeStamp time`. This means that in case of immersive frames,
  // the `now` is the same as `frameTime` in XR animation frame algorithm [1].
  //
  // [1] https://immersive-web.github.io/webxr/#xr-animation-frame
  double high_res_now_ms = UpdateImmersiveFrameTime(window, *data);

  frame_id_ = data->frame_id;
  if (data->buffer_shared_image.has_value()) {
    buffer_shared_image_ = gpu::ClientSharedImage::ImportUnowned(
        data->buffer_shared_image.value());
    buffer_sync_token_ = data->buffer_sync_token.value();
  }

  if (data->camera_image_buffer_shared_image.has_value()) {
    camera_image_shared_image_ = gpu::ClientSharedImage::ImportUnowned(
        data->camera_image_buffer_shared_image.value());
    camera_image_sync_token_ = data->camera_image_buffer_sync_token.value();
  }

  pending_immersive_vsync_ = false;

  for (auto& observer : immersive_observers_) {
    observer->OnImmersiveFrame();
  }

  // Post a task to handle scheduled animations after the current
  // execution context finishes, so that we yield to non-mojo tasks in
  // between frames. Executing mojo tasks back to back within the same
  // execution context caused extreme input delay due to processing
  // multiple frames without yielding, see crbug.com/701444.
  //
  // Used kInternalMedia since 1) this is not spec-ed and 2) this is media
  // related then tasks should not be throttled or frozen in background tabs.
  window->GetTaskRunner(blink::TaskType::kInternalMedia)
      ->PostTask(
          FROM_HERE,
          WTF::BindOnce(&XRFrameProvider::ProcessScheduledFrame,
                        WrapWeakPersistent(this), std::move(data),
                        high_res_now_ms, ScheduledFrameType::kImmersive));
}

void XRFrameProvider::OnNonImmersiveVSync(double high_res_now_ms) {
  TRACE_EVENT0("gpu", __FUNCTION__);
  DVLOG(2) << __FUNCTION__;

  pending_non_immersive_vsync_ = false;

  // Suppress non-immersive vsyncs when there's an immersive session active.
  if (immersive_session_)
    return;

  LocalDOMWindow* window = xr_->DomWindow();
  if (!window)
    return;

  window->GetTaskRunner(blink::TaskType::kInternalMedia)
      ->PostTask(FROM_HERE,
                 WTF::BindOnce(&XRFrameProvider::ProcessScheduledFrame,
                               WrapWeakPersistent(this), nullptr,
                               high_res_now_ms, ScheduledFrameType::kInline));
}

void XRFrameProvider::OnNonImmersiveFrameData(
    XRSession* session,
    device::mojom::blink::XRFrameDataPtr frame_data) {
  TRACE_EVENT0("gpu", __FUNCTION__);
  DVLOG(2) << __FUNCTION__;

  // TODO(https://crbug.com/837834): add unit tests for this code path.
  LocalDOMWindow* window = xr_->DomWindow();
  if (!window)
    return;

  // Look up the request for this session. The session may have ended between
  // when the request was sent and this callback, so skip it in that case.
  auto request = requesting_sessions_.find(session);
  if (request == requesting_sessions_.end()) {
    DVLOG(3) << __FUNCTION__
             << ": request corresponding to received frame data not found";
    if (!session->ended()) {
      DVLOG(2) << __FUNCTION__
               << ": the session's frame data provider missed the vsync";
    }

    return;
  }

  if (frame_data) {
    DVLOG(3) << __FUNCTION__ << ": frame data for session stored";
    request->value = std::move(frame_data);
  } else {
    // Unexpectedly didn't get frame data, and we don't have a timestamp.
    // Try to request a regular animation frame to avoid getting stuck.
    DVLOG(1) << __FUNCTION__ << ": NO FRAME DATA!";
    request->value = nullptr;
    window->document()->RequestAnimationFrame(
        MakeGarbageCollected<XRFrameProviderRequestCallback>(this));
  }
}

void XRFrameProvider::RequestNonImmersiveFrameData(XRSession* session) {
  DVLOG(3) << __FUNCTION__;

  DCHECK(session);
  DCHECK(!session->immersive());
  DCHECK(!immersive_session_);

  // The requesting_sessions_ entry for this session must have already
  // been created in |RequestFrame|.
  auto request = requesting_sessions_.find(session);
  CHECK(request != requesting_sessions_.end(), base::NotFatalUntil::M130);

  auto provider = non_immersive_data_providers_.find(session);
  if (provider == non_immersive_data_providers_.end()) {
    request->value = nullptr;
  } else {
    auto& data_provider = provider->value->Value();
    auto options = device::mojom::blink::XRFrameDataRequestOptions::New();
    options->include_lighting_estimation_data =
        session->LightEstimationEnabled();
    options->stage_parameters_id = session->StageParametersId();

    data_provider->GetFrameData(
        std::move(options),
        WTF::BindOnce(&XRFrameProvider::OnNonImmersiveFrameData,
                      WrapWeakPersistent(this), WrapWeakPersistent(session)));
  }
}

void XRFrameProvider::ProcessScheduledFrame(
    device::mojom::blink::XRFrameDataPtr frame_data,
    double high_res_now_ms,
    ScheduledFrameType frame_type) {
  DVLOG(2) << __FUNCTION__ << ": frame_id_=" << frame_id_
           << ", high_res_now_ms=" << high_res_now_ms;

  TRACE_EVENT2("gpu", "XRFrameProvider::ProcessScheduledFrame", "frame",
               frame_id_, "timestamp", high_res_now_ms);

  LocalDOMWindow* window = xr_->DomWindow();
  if (!window)
    return;

  if (!xr_->IsFrameFocused() && !immersive_session_) {
    return;  // Not currently focused, so we won't expose poses (except to
             // immersive sessions).
  }

  if (immersive_session_) {
    if (frame_type != ScheduledFrameType::kImmersive) {
      DVLOG(1)
          << __func__
          << " Attempted to process non-immersive scheduled frame as immersive";
      return;
    }

    // Check if immersive session is still valid, it may have ended and be
    // waiting for shutdown acknowledgement.
    if (immersive_session_->ended()) {
      return;
    }

    bool emulated_position = false;
    if (frame_data && frame_data->mojo_from_viewer) {
      DVLOG(3) << __func__ << ": pose available, emulated_position="
               << frame_data->mojo_from_viewer->emulated_position;
      emulated_position = frame_data->mojo_from_viewer->emulated_position;
    } else {
      DVLOG(2) << __func__ << ": emulating immersive frame position";
      emulated_position = true;
    }

    immersive_session_->UpdatePresentationFrameState(
        high_res_now_ms, std::move(frame_data), frame_id_, emulated_position);

    // Check if immersive session is still set as any events dispatched to the
    // page may have allowed a ForceEndSession to be triggered.
    if (!immersive_session_ || immersive_session_->ended()) {
      return;
    }

#if DCHECK_IS_ON()
    // Sanity check: if drawing into a shared buffer, the optional shared image
    // must be present. Exception is the first immersive frame after a
    // transition where the frame ID wasn't set yet. In that case, drawing can
    // proceed, but the result will be discarded in SubmitWebGLLayer().
    if (frame_transport_->DrawingIntoSharedBuffer() && frame_id_ >= 0) {
      DCHECK(buffer_shared_image_);
    }
#endif

    // TODO(crbug.com/1494911): Use ClientSharedImage to replace all mailbox
    // holder usage along the call hierarchy starting at XRSession::OnFrame().
    std::optional<gpu::MailboxHolder> buffer_mailbox_holder;
    if (buffer_shared_image_) {
      buffer_mailbox_holder = gpu::MailboxHolder{
          buffer_shared_image_->mailbox(), buffer_sync_token_, GL_TEXTURE_2D};
    }
    std::optional<gpu::MailboxHolder> camera_image_mailbox_holder;
    if (camera_image_shared_image_) {
      camera_image_mailbox_holder =
          gpu::MailboxHolder{camera_image_shared_image_->mailbox(),
                             camera_image_sync_token_, GL_TEXTURE_2D};
    }
    // Run immersive_session_->OnFrame() in a posted task to ensure that
    // createAnchor promises get a chance to run - the presentation frame state
    // is already updated.
    window->GetTaskRunner(blink::TaskType::kInternalMedia)
        ->PostTask(FROM_HERE,
                   WTF::BindOnce(&XRSession::OnFrame,
                                 WrapWeakPersistent(immersive_session_.Get()),
                                 high_res_now_ms, buffer_mailbox_holder,
                                 camera_image_mailbox_holder));
  } else {
    // In the process of fulfilling the frame requests for each session they are
    // extremely likely to request another frame. Work off of a separate list
    // from the requests to prevent infinite loops.
    decltype(requesting_sessions_) processing_sessions;

    DVLOG(3) << __FUNCTION__ << ": clearing requesting_sessions_";
    swap(requesting_sessions_, processing_sessions);

    // Inform sessions with a pending request of the new frame
    for (auto& request : processing_sessions) {
      XRSession* session = request.key.Get();

      // If the session was terminated between requesting and now, we shouldn't
      // process anything further.
      if (session->ended()) {
        continue;
      }

      session->UpdatePresentationFrameState(
          high_res_now_ms, std::move(request.value), frame_id_,
          true /* Non-immersive positions are always emulated */);

      // If any events dispatched to the page caused this session to end, we
      // should stop processing.
      if (session->ended()) {
        continue;
      }

      // Run session->OnFrame() in a posted task to ensure that createAnchor
      // promises get a chance to run - the presentation frame state is already
      // updated.
      // Note that rather than call session->OnFrame() directly, we dispatch to
      // a helper method who can determine if the state requirements are still
      // met that would allow the frame to be served.
      window->GetTaskRunner(blink::TaskType::kInternalMedia)
          ->PostTask(FROM_HERE,
                     WTF::BindOnce(&XRFrameProvider::OnPreDispatchInlineFrame,
                                   WrapWeakPersistent(this),
                                   WrapWeakPersistent(session), high_res_now_ms,
                                   std::nullopt, std::nullopt));
    }
  }
}

void XRFrameProvider::OnPreDispatchInlineFrame(
    XRSession* session,
    double timestamp,
    const std::optional<gpu::MailboxHolder>& output_mailbox_holder,
    const std::optional<gpu::MailboxHolder>& camera_image_mailbox_holder) {
  // Do nothing if the session was cleaned up or ended before we were schedueld.
  if (!session || session->ended())
    return;

  // If we have an immersive session, we shouldn't serve frames to the inline
  // session; however, we need to ensure that we don't stall out its frame loop,
  // so add a new frame request to get served after the immersive session exits.
  if (immersive_session_) {
    RequestFrame(session);
    return;
  }

  // If we still have the session and don't have an immersive session, then we
  // should serve the frame.
  session->OnFrame(timestamp, output_mailbox_holder,
                   camera_image_mailbox_holder);
}

double XRFrameProvider::UpdateImmersiveFrameTime(
    LocalDOMWindow* window,
    const device::mojom::blink::XRFrameData& data) {
  DVLOG(3) << __func__;

  // `data.time_delta` is in unspecified base. Because of that, we capture the
  // `time_delta` of the first frame we see (this gives us device_t_0), along
  // with the time we saw it (renderer_t_0). That allows us to translate from
  // device time to renderer time, so when we see frame N, we perform:
  // renderer_t_N = renderer_t_0 + (device_t_N - device_t_0)

  if (!first_immersive_frame_time_) {
    DCHECK(!first_immersive_frame_time_delta_);

    // This is the first time we got a frame data from an immersive session.
    // Let's capture device_t_0 and renderer_t_0.

    first_immersive_frame_time_ = base::TimeTicks::Now();
    first_immersive_frame_time_delta_ = data.time_delta;
  }

  // (device_t_N - device_t_0) is:
  base::TimeDelta current_frame_time_from_first_frame =
      data.time_delta - *first_immersive_frame_time_delta_;
  // renderer_t_N is then:
  base::TimeTicks current_frame_time =
      *first_immersive_frame_time_ + current_frame_time_from_first_frame;

  double high_res_now_ms =
      window->document()
          ->Loader()
          ->GetTiming()
          .MonotonicTimeToZeroBasedDocumentTime(current_frame_time)
          .InMillisecondsF();

  return high_res_now_ms;
}

void XRFrameProvider::SubmitWebGLLayer(XRWebGLLayer* layer, bool was_changed) {
  DCHECK(layer);
  DCHECK(immersive_session_);
  DCHECK(layer->session() == immersive_session_);
  if (!immersive_presentation_provider_.is_bound())
    return;

  TRACE_EVENT1("gpu", "XRFrameProvider::SubmitWebGLLayer", "frame", frame_id_);
  DVLOG(3) << __func__ << ": frame=" << frame_id_;

  WebGLRenderingContextBase* webgl_context = layer->context();

  if (frame_id_ < 0) {
    // There is no valid frame_id_, and the browser side is not currently
    // expecting a frame to be submitted. That can happen for the first
    // immersive frame if the animation loop submits without a preceding
    // immersive GetFrameData response, in that case frame_id_ is -1 (see
    // https://crbug.com/855722).
    return;
  }

  if (!was_changed) {
    // Just tell the device side that there was no submitted frame instead of
    // executing the implicit end-of-frame submit.
    frame_transport_->FrameSubmitMissing(immersive_presentation_provider_.get(),
                                         webgl_context->ContextGL(), frame_id_);
    dropped_frames_++;

    return;
  }

  frame_transport_->FramePreImage(webgl_context->ContextGL());

  if (frame_transport_->DrawingIntoSharedBuffer()) {
    // Image is written to shared buffer already. Just submit with a
    // placeholder.
    scoped_refptr<Image> image_ref;
    DVLOG(3) << __FUNCTION__ << ": FrameSubmit for SharedBuffer mode";
    bool succeeded = frame_transport_->FrameSubmit(
        immersive_presentation_provider_.get(), webgl_context->ContextGL(),
        webgl_context->SharedImageInterface(), webgl_context,
        std::move(image_ref), frame_id_);
    succeeded ? num_frames_++ : dropped_frames_++;
    if (succeeded) {
      submit_frame_time_.StartTimer();
    }

    return;
  }

  scoped_refptr<StaticBitmapImage> image_ref =
      layer->TransferToStaticBitmapImage();

  if (!image_ref)
    return;

  // Hardware-accelerated rendering should always be texture backed. Ensure this
  // is the case, don't attempt to render if using an unexpected drawing path.
  if (!image_ref->IsTextureBacked()) {
    NOTREACHED_IN_MIGRATION()
        << "WebXR requires hardware-accelerated rendering to texture";
    return;
  }

  bool succeeded = frame_transport_->FrameSubmit(
      immersive_presentation_provider_.get(), webgl_context->ContextGL(),
      webgl_context->SharedImageInterface(), webgl_context,
      std::move(image_ref), frame_id_);

  succeeded ? num_frames_++ : dropped_frames_++;
  if (succeeded) {
    submit_frame_time_.StartTimer();
  }

  // Reset our frame id, since anything we'd want to do (resizing/etc) can
  // no-longer happen to this frame.
  frame_id_ = -1;
}

// TODO(bajones): This only works because we're restricted to a single layer at
// the moment. Will need an overhaul when we get more robust layering support.
void XRFrameProvider::UpdateWebGLLayerViewports(XRWebGLLayer* layer) {
  DCHECK(layer->session() == immersive_session_);
  DCHECK(immersive_presentation_provider_.is_bound());

  XRViewport* left =
      layer->GetViewportForEye(device::mojom::blink::XREye::kLeft);
  XRViewport* right =
      layer->GetViewportForEye(device::mojom::blink::XREye::kRight);
  float width = layer->framebufferWidth();
  float height = layer->framebufferHeight();

  // We may only have one eye view, i.e. in smartphone immersive AR mode.
  // Use all-zero bounds for unused views.
  gfx::RectF left_coords =
      left ? gfx::RectF(
                 static_cast<float>(left->x()) / width,
                 static_cast<float>(height - (left->y() + left->height())) /
                     height,
                 static_cast<float>(left->width()) / width,
                 static_cast<float>(left->height()) / height)
           : gfx::RectF();
  gfx::RectF right_coords =
      right ? gfx::RectF(
                  static_cast<float>(right->x()) / width,
                  static_cast<float>(height - (right->y() + right->height())) /
                      height,
                  static_cast<float>(right->width()) / width,
                  static_cast<float>(right->height()) / height)
            : gfx::RectF();

  immersive_presentation_provider_->UpdateLayerBounds(
      frame_id_, left_coords, right_coords, gfx::Size(width, height));
}

void XRFrameProvider::Dispose() {
  DVLOG(2) << __func__;
  immersive_presentation_provider_.reset();
  immersive_data_provider_.reset();
  if (immersive_session_)
    immersive_session_->ForceEnd(XRSession::ShutdownPolicy::kImmediate);
  // TODO(bajones): Do something for outstanding frame requests?
}

void XRFrameProvider::SendFrameData() {
  if (!immersive_session_) {
    return;
  }

  device::mojom::blink::XrFrameStatisticsPtr xr_frame_stat =
      device::mojom::blink::XrFrameStatistics::New();

  xr_frame_stat->trace_id = immersive_session_->GetTraceId();

  base::TimeTicks now = base::TimeTicks::Now();
  xr_frame_stat->duration = now - last_frame_statistics_sent_time_;
  last_frame_statistics_sent_time_ = now;

  xr_frame_stat->num_frames = num_frames_;
  xr_frame_stat->dropped_frames = dropped_frames_;

  num_frames_ = 0;
  dropped_frames_ = 0;

  xr_frame_stat->frame_data_time = frame_data_time_.TakeAverageMicroseconds();

  xr_frame_stat->page_animation_frame_time =
      immersive_session()->TakeAnimationFrameTimerAverage();

  xr_frame_stat->submit_frame_time =
      submit_frame_time_.TakeAverageMicroseconds();

  if (xr_->GetWebXrInternalsRendererListener()) {
    xr_->GetWebXrInternalsRendererListener()->OnFrameData(
        std::move(xr_frame_stat));
  }
}

void XRFrameProvider::OnRenderComplete() {
  submit_frame_time_.StopTimer();
}

void XRFrameProvider::Trace(Visitor* visitor) const {
  visitor->Trace(xr_);
  visitor->Trace(frame_transport_);
  visitor->Trace(immersive_session_);
  visitor->Trace(immersive_data_provider_);
  visitor->Trace(immersive_presentation_provider_);
  visitor->Trace(non_immersive_data_providers_);
  visitor->Trace(requesting_sessions_);
  visitor->Trace(immersive_observers_);
}

}  // namespace blink
