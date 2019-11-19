// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_frame_provider.h"

#include "build/build_config.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/dom/frame_request_callback_collection.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/imagebitmap/image_bitmap.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/modules/xr/xr.h"
#include "third_party/blink/renderer/modules/xr/xr_plane_detection_state.h"
#include "third_party/blink/renderer/modules/xr/xr_session.h"
#include "third_party/blink/renderer/modules/xr/xr_viewport.h"
#include "third_party/blink/renderer/modules/xr/xr_webgl_layer.h"
#include "third_party/blink/renderer/modules/xr/xr_world_tracking_state.h"
#include "third_party/blink/renderer/platform/graphics/gpu/xr_frame_transport.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/transforms/transformation_matrix.h"

#include "ui/display/display.h"

namespace blink {

namespace {

class XRFrameProviderRequestCallback
    : public FrameRequestCallbackCollection::FrameCallback {
 public:
  explicit XRFrameProviderRequestCallback(XRFrameProvider* frame_provider)
      : frame_provider_(frame_provider) {}
  ~XRFrameProviderRequestCallback() override = default;
  void Invoke(double high_res_time_ms) override {
    frame_provider_->OnNonImmersiveVSync(high_res_time_ms);
  }

  void Trace(blink::Visitor* visitor) override {
    visitor->Trace(frame_provider_);

    FrameRequestCallbackCollection::FrameCallback::Trace(visitor);
  }

  Member<XRFrameProvider> frame_provider_;
};

}  // namespace

XRFrameProvider::XRFrameProvider(XR* xr)
    : xr_(xr), last_has_focus_(xr->IsFrameFocused()) {
  frame_transport_ = MakeGarbageCollected<XRFrameTransport>();
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

    immersive_data_provider_.Bind(std::move(session_ptr->data_provider));
    immersive_data_provider_.set_disconnect_handler(
        WTF::Bind(&XRFrameProvider::OnProviderConnectionError,
                  WrapWeakPersistent(this), WrapWeakPersistent(session)));

    immersive_presentation_provider_.Bind(
        std::move(session_ptr->submit_frame_sink->provider));
    immersive_presentation_provider_.set_disconnect_handler(
        WTF::Bind(&XRFrameProvider::OnProviderConnectionError,
                  WrapWeakPersistent(this), WrapWeakPersistent(session)));

    frame_transport_->BindSubmitFrameClient(
        std::move(session_ptr->submit_frame_sink->client_receiver));
    frame_transport_->SetTransportOptions(
        std::move(session_ptr->submit_frame_sink->transport_options));
    frame_transport_->PresentChange();
  } else {
    // If a non-immersive session doesn't have a data provider, we don't
    // need to store a reference to it.
    if (!session_ptr->data_provider) {
      return;
    }

    mojo::Remote<device::mojom::blink::XRFrameDataProvider> data_provider;
    data_provider.Bind(std::move(session_ptr->data_provider));
    data_provider.set_disconnect_handler(
        WTF::Bind(&XRFrameProvider::OnProviderConnectionError,
                  WrapWeakPersistent(this), WrapWeakPersistent(session)));

    non_immersive_data_providers_.insert(session, std::move(data_provider));
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
    immersive_frame_pose_ = nullptr;
    is_immersive_frame_position_emulated_ = false;

    frame_transport_ = MakeGarbageCollected<XRFrameTransport>();

    // When we no longer have an active immersive session schedule all the
    // outstanding frames that were requested while the immersive session was
    // active.
    if (requesting_sessions_.size() > 0) {
      for (auto& session : requesting_sessions_) {
        RequestNonImmersiveFrameData(session.key.Get());
      }

      ScheduleNonImmersiveFrame(nullptr);
    }
  } else {
    non_immersive_data_providers_.erase(session);
    requesting_sessions_.erase(session);
  }
}

// Schedule a session to be notified when the next XR frame is available.
void XRFrameProvider::RequestFrame(XRSession* session) {
  TRACE_EVENT0("gpu", __FUNCTION__);
  DCHECK(session);

  auto options = device::mojom::blink::XRFrameDataRequestOptions::New(
      session->worldTrackingState()->planeDetectionState()->enabled());

  // Immersive frame logic.
  if (session->immersive()) {
    ScheduleImmersiveFrame(std::move(options));
    return;
  }

  // Non-immersive frame logic.

  // Duplicate frame requests are treated as a no-op.
  if (requesting_sessions_.Contains(session)) {
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

  immersive_data_provider_->GetFrameData(
      std::move(options), WTF::Bind(&XRFrameProvider::OnImmersiveFrameData,
                                    WrapWeakPersistent(this)));
}

void XRFrameProvider::ScheduleNonImmersiveFrame(
    device::mojom::blink::XRFrameDataRequestOptionsPtr options) {
  TRACE_EVENT0("gpu", __FUNCTION__);
  DCHECK(!immersive_session_)
      << "Scheduling should be done via the exclusive session if present.";

  if (pending_non_immersive_vsync_)
    return;

  LocalFrame* frame = xr_->GetFrame();
  if (!frame)
    return;

  // TODO(https://crbug.com/856224) Review the lifetime of this object and
  // ensure that doc can never be null and remove this check.
  Document* doc = frame->GetDocument();
  if (!doc)
    return;

  pending_non_immersive_vsync_ = true;

  // Calls |OnNonImmersiveVSync|
  doc->RequestAnimationFrame(
      MakeGarbageCollected<XRFrameProviderRequestCallback>(this));
}

void XRFrameProvider::OnImmersiveFrameData(
    device::mojom::blink::XRFrameDataPtr data) {
  TRACE_EVENT0("gpu", __FUNCTION__);
  DVLOG(2) << __FUNCTION__;
  if (!data) {
    return;
  }

  // We may have lost the immersive session since the last VSync request.
  if (!immersive_session_) {
    return;
  }

  LocalFrame* frame = xr_->GetFrame();
  if (!frame)
    return;
  Document* doc = frame->GetDocument();
  if (!doc)
    return;

  base::TimeTicks monotonic_time_now = base::TimeTicks() + data->time_delta;
  double high_res_now_ms =
      doc->Loader()
          ->GetTiming()
          .MonotonicTimeToZeroBasedDocumentTime(monotonic_time_now)
          .InMillisecondsF();

  immersive_frame_pose_ = std::move(data->pose);
  if (immersive_frame_pose_) {
    is_immersive_frame_position_emulated_ =
        immersive_frame_pose_->emulated_position;
  } else {
    is_immersive_frame_position_emulated_ = true;
  }

  frame_id_ = data->frame_id;
  buffer_mailbox_holder_ = data->buffer_holder;

  pending_immersive_vsync_ = false;

  // Post a task to handle scheduled animations after the current
  // execution context finishes, so that we yield to non-mojo tasks in
  // between frames. Executing mojo tasks back to back within the same
  // execution context caused extreme input delay due to processing
  // multiple frames without yielding, see crbug.com/701444.
  //
  // Used kInternalMedia since 1) this is not spec-ed and 2) this is media
  // related then tasks should not be throttled or frozen in background tabs.
  frame->GetTaskRunner(blink::TaskType::kInternalMedia)
      ->PostTask(FROM_HERE, WTF::Bind(&XRFrameProvider::ProcessScheduledFrame,
                                      WrapWeakPersistent(this), std::move(data),
                                      high_res_now_ms));
}

void XRFrameProvider::OnNonImmersiveVSync(double high_res_now_ms) {
  TRACE_EVENT0("gpu", __FUNCTION__);
  DVLOG(2) << __FUNCTION__;

  pending_non_immersive_vsync_ = false;

  // Suppress non-immersive vsyncs when there's an immersive session active.
  if (immersive_session_)
    return;

  LocalFrame* frame = xr_->GetFrame();
  if (!frame)
    return;

  frame->GetTaskRunner(blink::TaskType::kInternalMedia)
      ->PostTask(FROM_HERE,
                 WTF::Bind(&XRFrameProvider::ProcessScheduledFrame,
                           WrapWeakPersistent(this), nullptr, high_res_now_ms));
}

void XRFrameProvider::OnNonImmersiveFrameData(
    XRSession* session,
    device::mojom::blink::XRFrameDataPtr frame_data) {
  TRACE_EVENT0("gpu", __FUNCTION__);
  DVLOG(2) << __FUNCTION__;

  // TODO(https://crbug.com/837834): add unit tests for this code path.

  LocalFrame* frame = xr_->GetFrame();
  if (!frame)
    return;
  Document* doc = frame->GetDocument();
  if (!doc)
    return;

  // Look up the request for this session. The session may have ended between
  // when the request was sent and this callback, so skip it in that case.
  auto request = requesting_sessions_.find(session);
  if (request == requesting_sessions_.end()) {
    return;
  }

  if (frame_data) {
    request->value = std::move(frame_data->pose);
  } else {
    // Unexpectedly didn't get frame data, and we don't have a timestamp.
    // Try to request a regular animation frame to avoid getting stuck.
    DVLOG(1) << __FUNCTION__ << ": NO FRAME DATA!";
    request->value = nullptr;
    doc->RequestAnimationFrame(
        MakeGarbageCollected<XRFrameProviderRequestCallback>(this));
  }
}

void XRFrameProvider::RequestNonImmersiveFrameData(XRSession* session) {
  DCHECK(session);
  DCHECK(!session->immersive());
  DCHECK(!immersive_session_);

  // The requesting_sessions_ entry for this session must have already
  // been created in |RequestFrame|.
  auto request = requesting_sessions_.find(session);
  DCHECK(request != requesting_sessions_.end());

  auto provider = non_immersive_data_providers_.find(session);
  if (provider == non_immersive_data_providers_.end()) {
    request->value = nullptr;
  } else {
    auto& data_provider = provider->value;
    auto options = device::mojom::blink::XRFrameDataRequestOptions::New(
        session->worldTrackingState()->planeDetectionState()->enabled());

    data_provider->GetFrameData(
        std::move(options),
        WTF::Bind(&XRFrameProvider::OnNonImmersiveFrameData,
                  WrapWeakPersistent(this), WrapWeakPersistent(session)));
  }
}

void XRFrameProvider::ProcessScheduledFrame(
    device::mojom::blink::XRFrameDataPtr frame_data,
    double high_res_now_ms) {
  DVLOG(2) << __FUNCTION__;

  TRACE_EVENT2("gpu", "XRFrameProvider::ProcessScheduledFrame", "frame",
               frame_id_, "timestamp", high_res_now_ms);

  if (!xr_->IsFrameFocused() && !immersive_session_) {
    return;  // Not currently focused, so we won't expose poses (except to
             // immersive sessions).
  }

  if (immersive_session_) {
    // Check if immersive session is still valid, it may have ended and be
    // waiting for shutdown acknowledgement.
    if (immersive_session_->ended()) {
      return;
    }

    // We need to ensure that pose data is valid for the duration of the frame,
    // because input events may call into |session.end()| which will destroy
    // this data otherwise. Move the data into local scope here so that it can't
    // be destroyed.
    auto frame_pose = std::move(immersive_frame_pose_);

    // Prior to updating input source state, update the state needed to create
    // presentation frame as newly created presentation frame will get passed to
    // the input source select[/start/end] events.
    immersive_session_->UpdatePresentationFrameState(
        high_res_now_ms, frame_pose, frame_data, frame_id_,
        is_immersive_frame_position_emulated_);

    // Check if immersive session is still set as OnInputStateChange may have
    // allowed a ForceEndSession to be triggered.
    if (!immersive_session_ || immersive_session_->ended())
      return;

    if (frame_pose && frame_pose->pose_reset) {
      immersive_session_->OnPoseReset();
    }

    // Check if immersive session is still set as OnPoseReset may have allowed a
    // ForceEndSession to be triggered.
    if (!immersive_session_ || immersive_session_->ended()) {
      return;
    }

    // If there's an immersive session active only process its frame.
#if DCHECK_IS_ON()
    // Sanity check: if drawing into a shared buffer, the optional mailbox
    // holder must be present. Exception is the first immersive frame after a
    // transition where the frame ID wasn't set yet. In that case, drawing can
    // proceed, but the result will be discarded in SubmitWebGLLayer().
    if (frame_transport_->DrawingIntoSharedBuffer() && frame_id_ >= 0) {
      DCHECK(buffer_mailbox_holder_);
    }
#endif
    if (frame_data && (frame_data->left_eye || frame_data->right_eye)) {
      immersive_session_->UpdateEyeParameters(frame_data->left_eye,
                                              frame_data->right_eye);
    }

    if (frame_data && frame_data->stage_parameters_updated) {
      immersive_session_->UpdateStageParameters(frame_data->stage_parameters);
    }

    immersive_session_->OnFrame(high_res_now_ms, buffer_mailbox_holder_);
  } else {
    // In the process of fulfilling the frame requests for each session they are
    // extremely likely to request another frame. Work off of a separate list
    // from the requests to prevent infinite loops.
    decltype(requesting_sessions_) processing_sessions;
    swap(requesting_sessions_, processing_sessions);

    // Inform sessions with a pending request of the new frame
    for (auto& request : processing_sessions) {
      XRSession* session = request.key.Get();

      // If the session was terminated between requesting and now, we shouldn't
      // process anything further.
      if (session->ended())
        continue;

      const auto& frame_pose = request.value;

      // Prior to updating input source state, update the state needed to create
      // presentation frame as newly created presentation frame will get passed
      // to the input source select[/start/end] events.
      session->UpdatePresentationFrameState(
          high_res_now_ms, frame_pose, frame_data, frame_id_,
          true /* Non-immersive positions are always emulated */);

      // If the input state change caused this session to end, we should stop
      // processing.
      if (session->ended())
        continue;

      if (frame_pose && frame_pose->pose_reset) {
        session->OnPoseReset();
      }

      // If the pose reset caused us to end, we should stop processing.
      if (session->ended())
        continue;

      session->OnFrame(high_res_now_ms, base::nullopt);
    }
  }
}

void XRFrameProvider::SubmitWebGLLayer(XRWebGLLayer* layer, bool was_changed) {
  DCHECK(layer);
  DCHECK(immersive_session_);
  DCHECK(layer->session() == immersive_session_);
  if (!immersive_presentation_provider_.is_bound())
    return;

  TRACE_EVENT1("gpu", "XRFrameProvider::SubmitWebGLLayer", "frame", frame_id_);

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
    return;
  }

  frame_transport_->FramePreImage(webgl_context->ContextGL());

  std::unique_ptr<viz::SingleReleaseCallback> image_release_callback;

  if (frame_transport_->DrawingIntoSharedBuffer()) {
    // Image is written to shared buffer already. Just submit with a
    // placeholder.
    scoped_refptr<Image> image_ref = nullptr;
    DVLOG(3) << __FUNCTION__ << ": FrameSubmit for SharedBuffer mode";
    frame_transport_->FrameSubmit(immersive_presentation_provider_.get(),
                                  webgl_context->ContextGL(), webgl_context,
                                  std::move(image_ref),
                                  std::move(image_release_callback), frame_id_);
    return;
  }

  scoped_refptr<StaticBitmapImage> image_ref =
      layer->TransferToStaticBitmapImage(&image_release_callback);

  if (!image_ref)
    return;

  // Hardware-accelerated rendering should always be texture backed. Ensure this
  // is the case, don't attempt to render if using an unexpected drawing path.
  if (!image_ref->IsTextureBacked()) {
    NOTREACHED() << "WebXR requires hardware-accelerated rendering to texture";
    return;
  }

  frame_transport_->FrameSubmit(immersive_presentation_provider_.get(),
                                webgl_context->ContextGL(), webgl_context,
                                std::move(image_ref),
                                std::move(image_release_callback), frame_id_);

  // Reset our frame id, since anything we'd want to do (resizing/etc) can
  // no-longer happen to this frame.
  frame_id_ = -1;
}

// TODO(bajones): This only works because we're restricted to a single layer at
// the moment. Will need an overhaul when we get more robust layering support.
void XRFrameProvider::UpdateWebGLLayerViewports(XRWebGLLayer* layer) {
  DCHECK(layer->session() == immersive_session_);
  DCHECK(immersive_presentation_provider_);

  XRViewport* left = layer->GetViewportForEye(XRView::kEyeLeft);
  XRViewport* right = layer->GetViewportForEye(XRView::kEyeRight);
  float width = layer->framebufferWidth();
  float height = layer->framebufferHeight();

  // We may only have one eye view, i.e. in smartphone immersive AR mode.
  // Use all-zero bounds for unused views.
  WebFloatRect left_coords =
      left ? WebFloatRect(
                 static_cast<float>(left->x()) / width,
                 static_cast<float>(height - (left->y() + left->height())) /
                     height,
                 static_cast<float>(left->width()) / width,
                 static_cast<float>(left->height()) / height)
           : WebFloatRect();
  WebFloatRect right_coords =
      right ? WebFloatRect(
                  static_cast<float>(right->x()) / width,
                  static_cast<float>(height - (right->y() + right->height())) /
                      height,
                  static_cast<float>(right->width()) / width,
                  static_cast<float>(right->height()) / height)
            : WebFloatRect();

  immersive_presentation_provider_->UpdateLayerBounds(
      frame_id_, left_coords, right_coords, WebSize(width, height));
}

void XRFrameProvider::Dispose() {
  DVLOG(2) << __func__;
  immersive_presentation_provider_.reset();
  immersive_data_provider_.reset();
  if (immersive_session_)
    immersive_session_->ForceEnd(XRSession::ShutdownPolicy::kImmediate);
  // TODO(bajones): Do something for outstanding frame requests?
}

void XRFrameProvider::Trace(blink::Visitor* visitor) {
  visitor->Trace(xr_);
  visitor->Trace(frame_transport_);
  visitor->Trace(immersive_session_);
  visitor->Trace(non_immersive_data_providers_);
  visitor->Trace(requesting_sessions_);
}

}  // namespace blink
