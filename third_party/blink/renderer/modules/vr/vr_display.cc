// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/vr/vr_display.h"

#include "base/auto_reset.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/core/css/css_property_value_set.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/dom/frame_request_callback_collection.h"
#include "third_party/blink/renderer/core/dom/scripted_animation_controller.h"
#include "third_party/blink/renderer/core/dom/user_gesture_indicator.h"
#include "third_party/blink/renderer/core/frame/frame.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/use_counter.h"
#include "third_party/blink/renderer/core/imagebitmap/image_bitmap.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/paint/compositing/paint_layer_compositor.h"
#include "third_party/blink/renderer/modules/event_target_modules.h"
#include "third_party/blink/renderer/modules/vr/navigator_vr.h"
#include "third_party/blink/renderer/modules/vr/vr_controller.h"
#include "third_party/blink/renderer/modules/vr/vr_display_capabilities.h"
#include "third_party/blink/renderer/modules/vr/vr_eye_parameters.h"
#include "third_party/blink/renderer/modules/vr/vr_frame_data.h"
#include "third_party/blink/renderer/modules/vr/vr_layer_init.h"
#include "third_party/blink/renderer/modules/vr/vr_pose.h"
#include "third_party/blink/renderer/modules/vr/vr_stage_parameters.h"
#include "third_party/blink/renderer/modules/webgl/webgl_rendering_context_base.h"
#include "third_party/blink/renderer/platform/histogram.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/wtf/time.h"

#include <array>
#include "third_party/blink/renderer/core/execution_context/execution_context.h"

namespace blink {

namespace {

// Threshold for rejecting stored non-immersive poses as being too old.
// If it's exceeded, defer non-immersive rAF callback execution until
// a fresh pose is received.
constexpr WTF::TimeDelta kNonImmersivePoseAgeThreshold =
    WTF::TimeDelta::FromMilliseconds(250);

VREye StringToVREye(const String& which_eye) {
  if (which_eye == "left")
    return kVREyeLeft;
  if (which_eye == "right")
    return kVREyeRight;
  return kVREyeNone;
}

class VRDisplayFrameRequestCallback
    : public FrameRequestCallbackCollection::FrameCallback {
 public:
  explicit VRDisplayFrameRequestCallback(VRDisplay* vr_display)
      : vr_display_(vr_display) {}
  ~VRDisplayFrameRequestCallback() override = default;
  void Invoke(double high_res_time_ms) override {
    if (Id() != vr_display_->PendingNonImmersiveVSyncId())
      return;
    TimeTicks monotonic_time;
    if (!vr_display_->GetDocument() || !vr_display_->GetDocument()->Loader()) {
      monotonic_time = WTF::CurrentTimeTicks();
    } else {
      // Convert document-zero time back to monotonic time.
      TimeTicks reference_monotonic_time = vr_display_->GetDocument()
                                               ->Loader()
                                               ->GetTiming()
                                               .ReferenceMonotonicTime();
      monotonic_time = reference_monotonic_time +
                       TimeDelta::FromMillisecondsD(high_res_time_ms);
    }
    vr_display_->OnNonImmersiveVSync(monotonic_time);
  }

  void Trace(blink::Visitor* visitor) override {
    visitor->Trace(vr_display_);

    FrameRequestCallbackCollection::FrameCallback::Trace(visitor);
  }

  Member<VRDisplay> vr_display_;
};

}  // namespace

SessionClientBinding::SessionClientBinding(
    VRDisplay* display,
    SessionClientBinding::SessionBindingType immersive,
    device::mojom::blink::XRSessionClientRequest request)
    : display_(display),
      is_immersive_(immersive ==
                    SessionClientBinding::SessionBindingType::kImmersive),
      client_binding_(this, std::move(request)){};

SessionClientBinding::~SessionClientBinding() = default;

void SessionClientBinding::Close() {
  DCHECK(client_binding_);
  client_binding_.Close();
}
void SessionClientBinding::OnChanged(
    device::mojom::blink::VRDisplayInfoPtr ptr) {
  display_->OnChanged(std::move(ptr), is_immersive_);
};
void SessionClientBinding::OnExitPresent() {
  display_->OnExitPresent(is_immersive_);
};
void SessionClientBinding::OnBlur() {
  display_->OnBlur(is_immersive_);
};
void SessionClientBinding::OnFocus() {
  display_->OnFocus(is_immersive_);
};
void SessionClientBinding::Trace(blink::Visitor* visitor) {
  visitor->Trace(display_);
}

VRDisplay::VRDisplay(NavigatorVR* navigator_vr,
                     device::mojom::blink::XRDevicePtr device)
    : PausableObject(navigator_vr->GetDocument()),
      navigator_vr_(navigator_vr),
      capabilities_(new VRDisplayCapabilities()),
      device_ptr_(std::move(device)),
      display_client_binding_(this) {
  PauseIfNeeded();  // Initialize SuspendabaleObject.

  // Request a non-immersive session immediately as WebVR 1.1 expects to be able
  // to get non-immersive poses as soon as the display is returned.
  device::mojom::blink::XRSessionOptionsPtr options =
      device::mojom::blink::XRSessionOptions::New();
  options->immersive = false;
  // Set in_on_display_activate to true, this will prevent the request present
  // from being logged.
  // TODO(http://crbug.com/842025): clean up the logging when refactors are
  // complete.
  device_ptr_->RequestSession(
      std::move(options), true,
      WTF::Bind(&VRDisplay::OnNonImmersiveSessionRequestReturned,
                WrapPersistent(this)));
}

VRDisplay::~VRDisplay() = default;

void VRDisplay::Pause() {}

void VRDisplay::Unpause() {
  RequestVSync();
}

VRController* VRDisplay::Controller() {
  return navigator_vr_->Controller();
}

void VRDisplay::Update(const device::mojom::blink::VRDisplayInfoPtr& display) {
  display_name_ = display->displayName;
  is_connected_ = true;

  capabilities_->SetHasPosition(display->capabilities->hasPosition);
  capabilities_->SetHasExternalDisplay(
      display->capabilities->hasExternalDisplay);
  capabilities_->SetCanPresent(display->capabilities->canPresent);
  capabilities_->SetMaxLayers(display->capabilities->canPresent ? 1 : 0);

  // Clear eye parameters to prevent them from getting stale.
  eye_parameters_left_.Clear();
  eye_parameters_right_.Clear();

  bool is_valid = false;
  if (capabilities_->canPresent()) {
    DCHECK_GT(display->leftEye->renderWidth, 0u);
    is_valid = true;

    eye_parameters_left_ = new VREyeParameters(
        display->leftEye, display->webvr_default_framebuffer_scale);
    eye_parameters_right_ = new VREyeParameters(
        display->rightEye, display->webvr_default_framebuffer_scale);
  }

  bool need_on_present_change = false;
  if (is_presenting_ && is_valid && !is_valid_device_for_presenting_) {
    need_on_present_change = true;
  }
  is_valid_device_for_presenting_ = is_valid;

  if (!display->stageParameters.is_null()) {
    if (!stage_parameters_)
      stage_parameters_ = new VRStageParameters();
    stage_parameters_->Update(display->stageParameters);
  } else {
    stage_parameters_ = nullptr;
  }

  if (need_on_present_change) {
    OnPresentChange();
  }
}

bool VRDisplay::getFrameData(VRFrameData* frame_data) {
  if (!did_log_getFrameData_ && GetDocument() &&
      GetDocument()->IsInMainFrame()) {
    did_log_getFrameData_ = true;

    ukm::builders::XR_WebXR(GetDocument()->UkmSourceID())
        .SetDidRequestPose(1)
        .Record(GetDocument()->UkmRecorder());
  }

  if (!FocusedOrPresenting() || !frame_pose_ || display_blurred_)
    return false;

  if (!frame_data)
    return false;

  if (!in_animation_frame_) {
    Document* doc = navigator_vr_->GetDocument();
    if (doc) {
      doc->AddConsoleMessage(
          ConsoleMessage::Create(kRenderingMessageSource, kWarningMessageLevel,
                                 "getFrameData must be called within a "
                                 "VRDisplay.requestAnimationFrame callback."));
    }
    return false;
  }

  if (depth_near_ == depth_far_)
    return false;

  return frame_data->Update(frame_pose_, eye_parameters_left_,
                            eye_parameters_right_, depth_near_, depth_far_);
}

VREyeParameters* VRDisplay::getEyeParameters(const String& which_eye) {
  if (!capabilities_->canPresent())
    return nullptr;

  switch (StringToVREye(which_eye)) {
    case kVREyeLeft:
      return eye_parameters_left_;
    case kVREyeRight:
      return eye_parameters_right_;
    default:
      return nullptr;
  }
}

void VRDisplay::RequestVSync() {
  DVLOG(2) << __FUNCTION__
           << " start: pending_vrdisplay_raf_=" << pending_vrdisplay_raf_
           << " in_animation_frame_=" << in_animation_frame_
           << " did_submit_this_frame_=" << did_submit_this_frame_
           << " pending_non_immersive_vsync_=" << pending_non_immersive_vsync_
           << " pending_presenting_vsync_=" << pending_presenting_vsync_;
  if (!pending_vrdisplay_raf_)
    return;
  Document* doc = navigator_vr_->GetDocument();
  if (!doc || !device_ptr_)
    return;
  if (display_blurred_)
    return;

  if (is_presenting_) {
    DCHECK(vr_presentation_provider_.is_bound());

    if (pending_presenting_vsync_)
      return;

    pending_non_immersive_vsync_ = false;
    pending_presenting_vsync_ = true;
    vr_presentation_data_provider_->GetFrameData(
        WTF::Bind(&VRDisplay::OnPresentingVSync, WrapWeakPersistent(this)));

    DVLOG(2) << __FUNCTION__ << " done: pending_presenting_vsync_="
             << pending_presenting_vsync_;
  } else {
    // Check if non_immersive_provider_, if not then we are not fully
    // initialized, or we do not support non-immersive, so don't request the
    // vsync. If and when non_immersive_provider_ is set it will run this code
    // again.
    if (!non_immersive_provider_)
      return;
    if (pending_non_immersive_vsync_)
      return;
    non_immersive_vsync_waiting_for_pose_.Reset();
    non_immersive_pose_request_time_ = WTF::CurrentTimeTicks();
    non_immersive_provider_->GetFrameData(WTF::Bind(
        &VRDisplay::OnNonImmersiveFrameData, WrapWeakPersistent(this)));
    pending_non_immersive_vsync_ = true;
    pending_non_immersive_vsync_id_ =
        doc->RequestAnimationFrame(new VRDisplayFrameRequestCallback(this));
    DVLOG(2) << __FUNCTION__ << " done: pending_non_immersive_vsync_="
             << pending_non_immersive_vsync_;
  }
}

int VRDisplay::requestAnimationFrame(V8FrameRequestCallback* callback) {
  DVLOG(2) << __FUNCTION__;
  Document* doc = this->GetDocument();
  if (!doc)
    return 0;
  pending_vrdisplay_raf_ = true;

  RequestVSync();

  FrameRequestCallbackCollection::V8FrameCallback* frame_callback =
      FrameRequestCallbackCollection::V8FrameCallback::Create(callback);
  frame_callback->SetUseLegacyTimeBase(false);
  return EnsureScriptedAnimationController(doc).RegisterCallback(
      frame_callback);
}

void VRDisplay::cancelAnimationFrame(int id) {
  DVLOG(2) << __FUNCTION__;
  if (!scripted_animation_controller_)
    return;
  scripted_animation_controller_->CancelCallback(id);
}

void VRDisplay::OnBlur(bool is_immersive) {
  // TODO(http://crbug.com/845283) When cleaning up the Blur events, determine
  // whether we should react to blur events from both immersive and non-
  // immersive sessions.
  DVLOG(1) << __FUNCTION__;
  display_blurred_ = true;
  navigator_vr_->EnqueueVREvent(
      VRDisplayEvent::Create(EventTypeNames::vrdisplayblur, this, ""));
}

void VRDisplay::OnFocus(bool is_immersive) {
  // TODO(http://crbug.com/845283) When cleaning up the Blur events, determine
  // whether we should react to blur events from both immersive and non-
  // immersive sessions.
  DVLOG(1) << __FUNCTION__;
  display_blurred_ = false;
  RequestVSync();

  navigator_vr_->EnqueueVREvent(
      VRDisplayEvent::Create(EventTypeNames::vrdisplayfocus, this, ""));
}

void ReportPresentationResult(PresentationResult result) {
  // Note that this is called twice for each call to requestPresent -
  // one to declare that requestPresent was called, and one for the
  // result.
  DEFINE_STATIC_LOCAL(
      EnumerationHistogram, vr_presentation_result_histogram,
      ("VRDisplayPresentResult",
       static_cast<int>(PresentationResult::kPresentationResultMax)));
  vr_presentation_result_histogram.Count(static_cast<int>(result));
}

ScriptPromise VRDisplay::requestPresent(ScriptState* script_state,
                                        const HeapVector<VRLayerInit>& layers) {
  DVLOG(1) << __FUNCTION__;
  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  UseCounter::Count(execution_context, WebFeature::kVRRequestPresent);
  if (!execution_context->IsSecureContext()) {
    UseCounter::Count(execution_context,
                      WebFeature::kVRRequestPresentInsecureOrigin);
  }

  if (!did_log_requestPresent_ && GetDocument() &&
      GetDocument()->IsInMainFrame()) {
    did_log_requestPresent_ = true;
    ukm::builders::XR_WebXR(GetDocument()->UkmSourceID())
        .SetDidRequestPresentation(1)
        .Record(GetDocument()->UkmRecorder());
  }

  ReportPresentationResult(PresentationResult::kRequested);

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();

  // If the VRDisplay does not advertise the ability to present reject the
  // request.
  if (!capabilities_->canPresent()) {
    DOMException* exception = DOMException::Create(
        DOMExceptionCode::kInvalidStateError, "VRDisplay cannot present.");
    resolver->Reject(exception);
    ReportPresentationResult(PresentationResult::kVRDisplayCannotPresent);
    return promise;
  }

  bool first_present = !is_presenting_;
  Document* doc = GetDocument();

  // Initiating VR presentation is only allowed in response to a user gesture.
  // If the VRDisplay is already presenting, however, repeated calls are
  // allowed outside a user gesture so that the presented content may be
  // updated.
  if (first_present) {
    if (!LocalFrame::HasTransientUserActivation(doc ? doc->GetFrame()
                                                    : nullptr)) {
      DOMException* exception =
          DOMException::Create(DOMExceptionCode::kInvalidStateError,
                               "API can only be initiated by a user gesture.");
      resolver->Reject(exception);
      ReportPresentationResult(PresentationResult::kNotInitiatedByUserGesture);
      return promise;
    }

    // When we are requesting to start presentation with a user action or the
    // display has activated, record the user action.
    Platform::Current()->RecordAction(
        UserMetricsAction("VR.WebVR.requestPresent"));
  }

  // A valid number of layers must be provided in order to present.
  if (layers.size() == 0 || layers.size() > capabilities_->maxLayers()) {
    ForceExitPresent();
    DOMException* exception = DOMException::Create(
        DOMExceptionCode::kInvalidStateError, "Invalid number of layers.");
    resolver->Reject(exception);
    ReportPresentationResult(PresentationResult::kInvalidNumberOfLayers);
    return promise;
  }

  // If what we were given has an invalid source, need to exit fullscreen with
  // previous, valid source, so delay m_layer reassignment
  if (layers[0].source().IsNull()) {
    ForceExitPresent();
    DOMException* exception = DOMException::Create(
        DOMExceptionCode::kInvalidStateError, "Invalid layer source.");
    resolver->Reject(exception);
    ReportPresentationResult(PresentationResult::kInvalidLayerSource);
    return promise;
  }
  layer_ = layers[0];

  CanvasRenderingContext* rendering_context;
  if (layer_.source().IsHTMLCanvasElement()) {
    rendering_context =
        layer_.source().GetAsHTMLCanvasElement()->RenderingContext();
  } else {
    DCHECK(layer_.source().IsOffscreenCanvas());
    rendering_context =
        layer_.source().GetAsOffscreenCanvas()->RenderingContext();
  }

  if (!rendering_context || !rendering_context->Is3d()) {
    ForceExitPresent();
    DOMException* exception =
        DOMException::Create(DOMExceptionCode::kInvalidStateError,
                             "Layer source must have a WebGLRenderingContext");
    resolver->Reject(exception);
    ReportPresentationResult(
        PresentationResult::kLayerSourceMissingWebGLContext);
    return promise;
  }

  // Save the WebGL script and underlying GL contexts for use by submitFrame().
  rendering_context_ = ToWebGLRenderingContextBase(rendering_context);
  context_gl_ = rendering_context_->ContextGL();

  if ((layer_.leftBounds().size() != 0 && layer_.leftBounds().size() != 4) ||
      (layer_.rightBounds().size() != 0 && layer_.rightBounds().size() != 4)) {
    ForceExitPresent();
    DOMException* exception = DOMException::Create(
        DOMExceptionCode::kInvalidStateError,
        "Layer bounds must either be an empty array or have 4 values");
    resolver->Reject(exception);
    ReportPresentationResult(PresentationResult::kInvalidLayerBounds);
    return promise;
  }

  for (float value : layer_.leftBounds()) {
    if (std::isnan(value)) {
      ForceExitPresent();
      DOMException* exception =
          DOMException::Create(DOMExceptionCode::kInvalidStateError,
                               "Layer bounds must not contain NAN values");
      resolver->Reject(exception);
      ReportPresentationResult(PresentationResult::kInvalidLayerBounds);
      return promise;
    }
  }

  for (float value : layer_.rightBounds()) {
    if (std::isnan(value)) {
      ForceExitPresent();
      DOMException* exception =
          DOMException::Create(DOMExceptionCode::kInvalidStateError,
                               "Layer bounds must not contain NAN values");
      resolver->Reject(exception);
      ReportPresentationResult(PresentationResult::kInvalidLayerBounds);
      return promise;
    }
  }

  if (!pending_present_resolvers_.IsEmpty()) {
    // If we are waiting on the results of a previous requestPresent call don't
    // fire a new request, just cache the resolver and resolve it when the
    // original request returns.
    pending_present_resolvers_.push_back(resolver);
  } else if (first_present) {
    if (!device_ptr_) {
      ForceExitPresent();
      DOMException* exception =
          DOMException::Create(DOMExceptionCode::kInvalidStateError,
                               "The service is no longer active.");
      resolver->Reject(exception);
      return promise;
    }

    pending_present_resolvers_.push_back(resolver);

    // Set up the VR backwards compatible XRSessionOptions based on canvas
    // properties.
    device::mojom::blink::XRSessionOptionsPtr options =
        device::mojom::blink::XRSessionOptions::New();
    options->immersive = true;
    options->use_legacy_webvr_render_path = true;

    device_ptr_->RequestSession(
        std::move(options), in_display_activate_,
        WTF::Bind(&VRDisplay::OnRequestImmersiveSessionReturned,
                  WrapPersistent(this)));
    pending_present_request_ = true;

    // The old vr_presentation_provider_ won't be delivering any vsyncs anymore,
    // so we aren't waiting on it anymore.
    pending_presenting_vsync_ = false;
  } else {
    UpdateLayerBounds();
    resolver->Resolve();
    ReportPresentationResult(PresentationResult::kSuccessAlreadyPresenting);
  }

  return promise;
}

void VRDisplay::OnRequestImmersiveSessionReturned(
    device::mojom::blink::XRSessionPtr session) {
  pending_present_request_ = false;
  if (session) {
    DCHECK(session->submit_frame_sink);
    vr_presentation_data_provider_.reset();
    vr_presentation_data_provider_.Bind(std::move(session->data_provider));
    // The presentation provider error handler can trigger if a device is
    // disconnected from the system. This can happen if, for example, an HMD is
    // unplugged.
    vr_presentation_data_provider_.set_connection_error_handler(
        WTF::Bind(&VRDisplay::OnPresentationProviderConnectionError,
                  WrapWeakPersistent(this)));
    vr_presentation_provider_.Bind(
        std::move(session->submit_frame_sink->provider));
    vr_presentation_provider_.set_connection_error_handler(
        WTF::Bind(&VRDisplay::OnPresentationProviderConnectionError,
                  WrapWeakPersistent(this)));

    frame_transport_ = new XRFrameTransport();
    frame_transport_->BindSubmitFrameClient(
        std::move(session->submit_frame_sink->client_request));
    frame_transport_->SetTransportOptions(
        std::move(session->submit_frame_sink->transport_options));

    if (immersive_client_binding_)
      immersive_client_binding_->Close();
    immersive_client_binding_ = new SessionClientBinding(
        this, SessionClientBinding::SessionBindingType::kImmersive,
        std::move(session->client_request));

    Update(std::move(session->display_info));

    this->BeginPresent();
  } else {
    this->ForceExitPresent();
    DOMException* exception = DOMException::Create(
        DOMExceptionCode::kNotAllowedError, "Presentation request was denied.");

    while (!pending_present_resolvers_.IsEmpty()) {
      ScriptPromiseResolver* resolver = pending_present_resolvers_.TakeFirst();
      resolver->Reject(exception);
    }
  }
}

void VRDisplay::OnNonImmersiveSessionRequestReturned(
    device::mojom::blink::XRSessionPtr session) {
  if (!session) {
    // System does not support any kind of session.
    return;
  }
  non_immersive_provider_.Bind(std::move(session->data_provider));
  non_immersive_client_binding_ = new SessionClientBinding(
      this, SessionClientBinding::SessionBindingType::kNonImmersive,
      std::move(session->client_request));
  RequestVSync();
}

ScriptPromise VRDisplay::exitPresent(ScriptState* script_state) {
  DVLOG(1) << __FUNCTION__;
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();

  if (!is_presenting_) {
    // Can't stop presenting if we're not presenting.
    DOMException* exception = DOMException::Create(
        DOMExceptionCode::kInvalidStateError, "VRDisplay is not presenting.");
    resolver->Reject(exception);
    return promise;
  }

  if (!device_ptr_) {
    DOMException* exception = DOMException::Create(
        DOMExceptionCode::kInvalidStateError, "VRService is not available.");
    resolver->Reject(exception);
    return promise;
  }
  device_ptr_->ExitPresent();

  resolver->Resolve();

  StopPresenting();

  return promise;
}

void VRDisplay::BeginPresent() {
  Document* doc = this->GetDocument();

  DOMException* exception = nullptr;
  if (!frame_transport_) {
    exception =
        DOMException::Create(DOMExceptionCode::kInvalidStateError,
                             "VRDisplay presentation path not configured.");
  }

  if (layer_.source().IsOffscreenCanvas()) {
    // TODO(junov, crbug.com/695497): Implement OffscreenCanvas presentation
    exception =
        DOMException::Create(DOMExceptionCode::kInvalidStateError,
                             "OffscreenCanvas presentation not implemented.");
  } else {
    // A canvas must be either Offscreen or plain HTMLCanvas.
    DCHECK(layer_.source().IsHTMLCanvasElement());
  }

  if (exception) {
    ForceExitPresent();
    while (!pending_present_resolvers_.IsEmpty()) {
      ScriptPromiseResolver* resolver = pending_present_resolvers_.TakeFirst();
      resolver->Reject(exception);
    }
    ReportPresentationResult(
        PresentationResult::kPresentationNotSupportedByDisplay);
    return;
  }

  // Presenting with external displays has to make a copy of the image
  // since the canvas may still be visible at the same time.
  present_image_needs_copy_ = capabilities_->hasExternalDisplay();

  if (doc) {
    Platform::Current()->RecordRapporURL("VR.WebVR.PresentSuccess",
                                         WebURL(doc->Url()));
  }
  if (!FocusedOrPresenting() && display_blurred_) {
    // Presentation doesn't care about focus, so if we're blurred because of
    // focus, then unblur.
    OnFocus(true);
  }
  is_presenting_ = true;
  // Call RequestVSync to switch from the (internal) document rAF to the
  // XRPresentationProvider GetFrameData rate.
  RequestVSync();
  ReportPresentationResult(PresentationResult::kSuccess);

  UpdateLayerBounds();

  while (!pending_present_resolvers_.IsEmpty()) {
    ScriptPromiseResolver* resolver = pending_present_resolvers_.TakeFirst();
    resolver->Resolve();
  }
  OnPresentChange();

  // For GVR, we shut down normal vsync processing during VR presentation.
  // Run window.rAF once manually so that applications get a chance to
  // schedule a VRDisplay.rAF in case they do so only while presenting.
  if (doc && !pending_vrdisplay_raf_ && !capabilities_->hasExternalDisplay()) {
    TimeTicks timestamp = WTF::CurrentTimeTicks();
    doc->GetTaskRunner(blink::TaskType::kInternalMedia)
        ->PostTask(FROM_HERE,
                   WTF::Bind(&VRDisplay::ProcessScheduledWindowAnimations,
                             WrapWeakPersistent(this), timestamp));
  }
}

// Need to close service if exists and then free rendering context.
void VRDisplay::ForceExitPresent() {
  if (device_ptr_) {
    device_ptr_->ExitPresent();
  }
  StopPresenting();
}

void VRDisplay::UpdateLayerBounds() {
  if (!device_ptr_)
    return;

  // Left eye defaults
  if (layer_.leftBounds().size() != 4)
    layer_.setLeftBounds({0.0f, 0.0f, 0.5f, 1.0f});
  // Right eye defaults
  if (layer_.rightBounds().size() != 4)
    layer_.setRightBounds({0.5f, 0.0f, 0.5f, 1.0f});

  const Vector<float>& left = layer_.leftBounds();
  const Vector<float>& right = layer_.rightBounds();

  vr_presentation_provider_->UpdateLayerBounds(
      vr_frame_id_, WebFloatRect(left[0], left[1], left[2], left[3]),
      WebFloatRect(right[0], right[1], right[2], right[3]),
      WebSize(source_width_, source_height_));
}

HeapVector<VRLayerInit> VRDisplay::getLayers() {
  HeapVector<VRLayerInit> layers;

  if (is_presenting_) {
    layers.push_back(layer_);
  }

  return layers;
}

scoped_refptr<Image> VRDisplay::GetFrameImage(
    std::unique_ptr<viz::SingleReleaseCallback>* out_release_callback) {
  TRACE_EVENT_BEGIN0("gpu", "VRDisplay:GetStaticBitmapImage");

  scoped_refptr<Image> image_ref =
      rendering_context_->GetStaticBitmapImage(out_release_callback);
  TRACE_EVENT_END0("gpu", "VRDisplay::GetStaticBitmapImage");

  // Hardware-accelerated rendering should always be texture backed,
  // as implemented by AcceleratedStaticBitmapImage. Ensure this is
  // the case, don't attempt to render if using an unexpected drawing
  // path.
  if (!image_ref.get() || !image_ref->IsTextureBacked()) {
    TRACE_EVENT0("gpu", "VRDisplay::GetImage_SlowFallback");
    // We get a non-texture-backed image when running layout tests
    // on desktop builds. Add a slow fallback so that these continue
    // working.
    image_ref = rendering_context_->GetImage(kPreferAcceleration);
    if (!image_ref.get() || !image_ref->IsTextureBacked()) {
      NOTREACHED()
          << "WebXR requires hardware-accelerated rendering to texture";
      return nullptr;
    }
  }
  return image_ref;
}

void VRDisplay::submitFrame() {
  DVLOG(2) << __FUNCTION__;

  if (!device_ptr_)
    return;
  TRACE_EVENT1("gpu", "submitFrame", "frame", vr_frame_id_);

  Document* doc = this->GetDocument();
  if (!doc)
    return;

  if (!is_presenting_) {
    doc->AddConsoleMessage(ConsoleMessage::Create(
        kRenderingMessageSource, kWarningMessageLevel,
        "submitFrame has no effect when the VRDisplay is not presenting."));
    return;
  }

  if (!in_animation_frame_) {
    doc->AddConsoleMessage(
        ConsoleMessage::Create(kRenderingMessageSource, kWarningMessageLevel,
                               "submitFrame must be called within a "
                               "VRDisplay.requestAnimationFrame callback."));
    return;
  }

  if (!context_gl_) {
    // Something got confused, we can't submit frames without a GL context.
    return;
  }

  // No frame Id to write before submitting the frame.
  if (vr_frame_id_ < 0) {
    // TODO(klausw): There used to be a submitFrame here, but we can't
    // submit without a frameId and associated pose data. Just drop it.
    return;
  }

  // Can't submit frames when the page isn't visible. This can happen  because
  // we don't use the unified BeginFrame rendering path for WebVR so visibility
  // updates aren't synchronized with WebVR VSync.
  if (!doc->GetPage()->IsPageVisible())
    return;

  // Check if the canvas got resized, if yes send a bounds update.
  int current_width = rendering_context_->drawingBufferWidth();
  int current_height = rendering_context_->drawingBufferHeight();
  if ((current_width != source_width_ || current_height != source_height_) &&
      current_width != 0 && current_height != 0) {
    source_width_ = current_width;
    source_height_ = current_height;
    UpdateLayerBounds();
  }

  frame_transport_->FramePreImage(context_gl_);

  // Shared buffer draw is not supposed to be enabled for WebVR 1.1 since
  // we don't currently have a way to override the canvas drawing buffer's
  // bindings. Sanity check that it's off.
  DCHECK(!frame_transport_->DrawingIntoSharedBuffer());

  std::unique_ptr<viz::SingleReleaseCallback> image_release_callback;

  scoped_refptr<Image> image_ref = GetFrameImage(&image_release_callback);
  if (!image_ref)
    return;

  DrawingBuffer::Client* drawing_buffer_client =
      static_cast<DrawingBuffer::Client*>(rendering_context_.Get());

  frame_transport_->FrameSubmit(vr_presentation_provider_.get(), context_gl_,
                                drawing_buffer_client, std::move(image_ref),
                                std::move(image_release_callback), vr_frame_id_,
                                present_image_needs_copy_);

  did_submit_this_frame_ = true;
  // Reset our frame id, since anything we'd want to do (resizing/etc) can
  // no-longer happen to this frame.
  vr_frame_id_ = -1;

  // If preserveDrawingBuffer is false, must clear now. Normally this
  // happens as part of compositing, but that's not active while
  // presenting, so run the responsible code directly.
  rendering_context_->MarkCompositedAndClearBackbufferIfNeeded();
}

Document* VRDisplay::GetDocument() {
  return navigator_vr_->GetDocument();
}

device::mojom::blink::VRDisplayClientPtr VRDisplay::GetDisplayClient() {
  display_client_binding_.Close();
  device::mojom::blink::VRDisplayClientPtr client;
  display_client_binding_.Bind(mojo::MakeRequest(&client));
  return client;
}

void VRDisplay::OnPresentChange() {
  if (frame_transport_)
    frame_transport_->PresentChange();

  DVLOG(1) << __FUNCTION__ << ": is_presenting_=" << is_presenting_;
  if (is_presenting_ && !is_valid_device_for_presenting_) {
    DVLOG(1) << __FUNCTION__ << ": device not valid, not sending event";
    return;
  }
  navigator_vr_->EnqueueVREvent(
      VRDisplayEvent::Create(EventTypeNames::vrdisplaypresentchange, this, ""));
}

void VRDisplay::OnChanged(device::mojom::blink::VRDisplayInfoPtr display,
                          bool is_immersive) {
  // VrDisplayInfo is only used for immersive sessions, so unless this is
  // from an immersive device, ignore it.
  // We expect that non-immersive sessions don't use display info, and immersive
  // sessions will not use display info until they are presenting, so it is fine
  // for us not to start listening until then.
  if (is_immersive) {
    Update(display);
  }
}

void VRDisplay::OnExitPresent(bool is_immersive) {
  if (is_immersive) {
    StopPresenting();
  }
}

void VRDisplay::OnConnected() {
  navigator_vr_->EnqueueVREvent(VRDisplayEvent::Create(
      EventTypeNames::vrdisplayconnect, this, "connect"));
}

void VRDisplay::OnDisconnected() {
  navigator_vr_->EnqueueVREvent(VRDisplayEvent::Create(
      EventTypeNames::vrdisplaydisconnect, this, "disconnect"));
}

void VRDisplay::StopPresenting() {
  if (is_presenting_) {
    if (!capabilities_->hasExternalDisplay()) {
      if (layer_.source().IsHTMLCanvasElement()) {
        // TODO(klausw,crbug.com/698923): If compositor updates are
        // suppressed, restore them here.
      } else {
        // TODO(junov, crbug.com/695497): Implement for OffscreenCanvas
      }
    } else {
      // Can't get into this presentation mode, so nothing to do here.
    }
    is_presenting_ = false;

    OnPresentChange();

    // Record user action for stop presenting.  Note that this could be
    // user-triggered or not.
    Platform::Current()->RecordAction(
        UserMetricsAction("VR.WebVR.StopPresenting"));
  }

  frame_transport_ = nullptr;
  rendering_context_ = nullptr;
  context_gl_ = nullptr;
  did_submit_this_frame_ = false;
  RequestVSync();
}

void VRDisplay::OnActivate(device::mojom::blink::VRDisplayEventReason reason,
                           OnActivateCallback on_handled) {
  Document* doc = GetDocument();
  if (!doc) {
    std::move(on_handled).Run(true /* will_not_present */);
    return;
  }

  std::unique_ptr<UserGestureIndicator> gesture_indicator;
  if (reason == device::mojom::blink::VRDisplayEventReason::MOUNTED)
    gesture_indicator = LocalFrame::NotifyUserActivation(doc->GetFrame());

  base::AutoReset<bool> in_activate(&in_display_activate_, true);

  navigator_vr_->DispatchVREvent(
      VRDisplayEvent::Create(EventTypeNames::vrdisplayactivate, this, reason));
  std::move(on_handled).Run(!pending_present_request_ && !is_presenting_);
}

void VRDisplay::OnDeactivate(
    device::mojom::blink::VRDisplayEventReason reason) {
  navigator_vr_->EnqueueVREvent(VRDisplayEvent::Create(
      EventTypeNames::vrdisplaydeactivate, this, reason));
}

void VRDisplay::ProcessScheduledWindowAnimations(TimeTicks timestamp) {
  TRACE_EVENT1("gpu", "VRDisplay::window.rAF", "frame", vr_frame_id_);
  auto* doc = navigator_vr_->GetDocument();
  if (!doc)
    return;
  auto* page = doc->GetPage();
  if (!page)
    return;

  bool had_pending_vrdisplay_raf = pending_vrdisplay_raf_;
  // TODO(klausw): update timestamp based on scheduling delay?
  page->Animator().ServiceScriptedAnimations(timestamp);

  if (had_pending_vrdisplay_raf != pending_vrdisplay_raf_) {
    DVLOG(1) << __FUNCTION__
             << ": window.rAF fallback successfully scheduled VRDisplay.rAF";
  }

  if (!pending_vrdisplay_raf_) {
    // There wasn't any call to vrDisplay.rAF, so we will not be getting new
    // frames from now on unless the application schedules one down the road in
    // reaction to a separate event or timeout. TODO(klausw,crbug.com/716087):
    // do something more useful here?
    DVLOG(1) << __FUNCTION__
             << ": no scheduled VRDisplay.requestAnimationFrame, presentation "
                "broken?";
  }
}

void VRDisplay::ProcessScheduledAnimations(TimeTicks timestamp) {
  DVLOG(2) << __FUNCTION__;
  // Check if we still have a valid context, the animation controller
  // or document may have disappeared since we scheduled this.
  Document* doc = this->GetDocument();
  if (!doc || display_blurred_) {
    DVLOG(2) << __FUNCTION__ << ": early exit, doc=" << doc
             << " display_blurred_=" << display_blurred_;
    return;
  }

  if (doc->IsContextPaused()) {
    // We are currently suspended - try ProcessScheduledAnimations again later
    // when we resume.
    return;
  }

  TRACE_EVENT1("gpu", "VRDisplay::OnVSync", "frame", vr_frame_id_);

  if (pending_vrdisplay_raf_ && scripted_animation_controller_) {
    // Run the callback, making sure that in_animation_frame_ is only
    // true for the vrDisplay rAF and not for a legacy window rAF
    // that may be called later.
    base::AutoReset<bool> animating(&in_animation_frame_, true);
    pending_vrdisplay_raf_ = false;
    did_submit_this_frame_ = false;
    scripted_animation_controller_->ServiceScriptedAnimations(timestamp);
    // If presenting and the script didn't call SubmitFrame, let the device
    // side know so that it can cleanly reuse resources and make appropriate
    // timing decisions. Note that is_presenting_ could become false during
    // an animation loop due to reentrant mojo processing in SubmitFrame,
    // so there's no guarantee that this is called for the last animating
    // frame. That's OK since the sync token placed by FrameSubmitMissing
    // is only intended to separate frames while presenting.
    if (is_presenting_ && !did_submit_this_frame_) {
      DCHECK(frame_transport_);
      DCHECK(context_gl_);
      frame_transport_->FrameSubmitMissing(vr_presentation_provider_.get(),
                                           context_gl_, vr_frame_id_);
    }
  }
  if (pending_pose_)
    frame_pose_ = std::move(pending_pose_);

  // Sanity check: If pending_vrdisplay_raf_ is true and the vsync provider
  // is connected, we must now have a pending vsync.
  DCHECK(!pending_vrdisplay_raf_ || pending_non_immersive_vsync_ ||
         pending_presenting_vsync_);
}

void VRDisplay::OnPresentingVSync(
    device::mojom::blink::XRFrameDataPtr frame_data) {
  TRACE_EVENT0("gpu", __FUNCTION__);
  if (!frame_data) {
    return;
  }

  if (!context_gl_) {
    DVLOG(1) << __FUNCTION__ << ": lost context";
    return;
  }

  // All early exits that want this VSync converted to a non-immersive
  // VSync must happen before this line. Once it's set to not pending,
  // an early exit woud break animation.
  pending_presenting_vsync_ = false;

  frame_pose_ = std::move(frame_data->pose);
  vr_frame_id_ = frame_data->frame_id;

  if (frame_transport_ && frame_transport_->DrawingIntoSharedBuffer()) {
    NOTIMPLEMENTED();
  }

  Document* doc = GetDocument();
  if (!doc)
    return;

  // Post a task to handle scheduled animations after the current
  // execution context finishes, so that we yield to non-mojo tasks in
  // between frames. Executing mojo tasks back to back within the same
  // execution context caused extreme input delay due to processing
  // multiple frames without yielding, see crbug.com/701444. I suspect
  // this is due to WaitForIncomingMethodCall receiving the OnVSync
  // but queueing it for immediate execution since it doesn't match
  // the interface being waited on.
  //
  // Used kInternalMedia since 1) this is not spec-ed and 2) this is media
  // related then tasks should not be throttled or frozen in background tabs.
  doc->GetTaskRunner(blink::TaskType::kInternalMedia)
      ->PostTask(FROM_HERE, WTF::Bind(&VRDisplay::ProcessScheduledAnimations,
                                      WrapWeakPersistent(this),
                                      TimeTicks() + frame_data->time_delta));
}

void VRDisplay::OnNonImmersiveVSync(TimeTicks timestamp) {
  DVLOG(2) << __FUNCTION__;
  pending_non_immersive_vsync_ = false;
  pending_non_immersive_vsync_id_ = -1;
  if (is_presenting_)
    return;
  vr_frame_id_ = -1;
  WTF::TimeDelta pose_age =
      WTF::CurrentTimeTicks() - non_immersive_pose_received_time_;
  if (pose_age >= kNonImmersivePoseAgeThreshold &&
      non_immersive_pose_request_time_ > non_immersive_pose_received_time_) {
    // The VSync got triggered before ever receiving a pose, or the pose is
    // stale. Defer the animation until a pose arrives to avoid passing null
    // poses to the application, but only do this if we have an outstanding
    // unresolved GetPose request. For example, the pose might be stale after
    // exiting VR Browser non-immersive mode due to a longish transition, but we
    // need to use it anyway if it's from the current frame's GetPose.
    non_immersive_vsync_waiting_for_pose_ =
        WTF::Bind(&VRDisplay::ProcessScheduledAnimations,
                  WrapWeakPersistent(this), timestamp);
  } else {
    ProcessScheduledAnimations(timestamp);
  }
}

void VRDisplay::OnNonImmersiveFrameData(
    device::mojom::blink::XRFrameDataPtr data) {
  non_immersive_pose_received_time_ = WTF::CurrentTimeTicks();
  if (data) {
    if (!in_animation_frame_) {
      frame_pose_ = std::move(data->pose);
    } else {
      pending_pose_ = std::move(data->pose);
    }
  }
  if (non_immersive_vsync_waiting_for_pose_) {
    // We have a vsync waiting for a pose, run it now.
    std::move(non_immersive_vsync_waiting_for_pose_).Run();
    non_immersive_vsync_waiting_for_pose_.Reset();
  }
}

void VRDisplay::OnPresentationProviderConnectionError() {
  DVLOG(1) << __FUNCTION__ << ";;; is_presenting_=" << is_presenting_
           << " pending_non_immersive_vsync_=" << pending_non_immersive_vsync_
           << " pending_presenting_vsync_=" << pending_presenting_vsync_;
  vr_presentation_provider_.reset();
  vr_presentation_data_provider_.reset();
  if (is_presenting_) {
    ForceExitPresent();
  }
  pending_presenting_vsync_ = false;
  RequestVSync();
}

ScriptedAnimationController& VRDisplay::EnsureScriptedAnimationController(
    Document* doc) {
  if (!scripted_animation_controller_)
    scripted_animation_controller_ = ScriptedAnimationController::Create(doc);

  return *scripted_animation_controller_;
}

void VRDisplay::Dispose() {
  if (non_immersive_client_binding_)
    non_immersive_client_binding_->Close();
  non_immersive_client_binding_ = nullptr;
  vr_presentation_provider_.reset();
}

ExecutionContext* VRDisplay::GetExecutionContext() const {
  return ContextLifecycleObserver::GetExecutionContext();
}

const AtomicString& VRDisplay::InterfaceName() const {
  return EventTargetNames::VRDisplay;
}

void VRDisplay::ContextDestroyed(ExecutionContext* context) {
  PausableObject::ContextDestroyed(context);
  ForceExitPresent();
  scripted_animation_controller_.Clear();
}

bool VRDisplay::HasPendingActivity() const {
  // Prevent V8 from garbage collecting the wrapper object if there are
  // event listeners and/or callbacks attached to it.
  return GetExecutionContext() &&
         (HasEventListeners() ||
          (scripted_animation_controller_ &&
           scripted_animation_controller_->HasCallback()));
}

void VRDisplay::FocusChanged() {
  DVLOG(1) << __FUNCTION__;
  if (navigator_vr_->IsFocused()) {
    if (is_presenting_) {
      OnFocus(true /* is_immmersive */);
    } else {
      OnFocus(false /* is_immmersive */);
    }
  } else if (!is_presenting_) {
    OnBlur(false);
  }
}

bool VRDisplay::FocusedOrPresenting() {
  // The browser can't track focus for frames, so we still need to check for
  // focus in the renderer, even if the browser is checking focus before
  // sending input.
  return navigator_vr_->IsFocused() || is_presenting_;
}

void VRDisplay::Trace(blink::Visitor* visitor) {
  visitor->Trace(navigator_vr_);
  visitor->Trace(capabilities_);
  visitor->Trace(stage_parameters_);
  visitor->Trace(eye_parameters_left_);
  visitor->Trace(eye_parameters_right_);
  visitor->Trace(layer_);
  visitor->Trace(rendering_context_);
  visitor->Trace(frame_transport_);
  visitor->Trace(scripted_animation_controller_);
  visitor->Trace(non_immersive_client_binding_);
  visitor->Trace(immersive_client_binding_);
  visitor->Trace(pending_present_resolvers_);
  EventTargetWithInlineData::Trace(visitor);
  ContextLifecycleObserver::Trace(visitor);
}

}  // namespace blink
