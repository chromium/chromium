// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/vr/navigator_vr.h"

#include "services/metrics/public/cpp/ukm_builders.h"
#include "third_party/blink/public/mojom/feature_policy/feature_policy.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/public/web/web_frame.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/navigator.h"
#include "third_party/blink/renderer/core/frame/use_counter.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/modules/vr/vr_controller.h"
#include "third_party/blink/renderer/modules/vr/vr_display.h"
#include "third_party/blink/renderer/modules/vr/vr_get_devices_callback.h"
#include "third_party/blink/renderer/modules/vr/vr_pose.h"
#include "third_party/blink/renderer/modules/xr/xr.h"

namespace blink {

namespace {

const char kFeaturePolicyBlockedMessage[] =
    "Access to the feature \"vr\" is disallowed by feature policy.";

const char kNotAssociatedWithDocumentMessage[] =
    "The object is no longer associated with a document.";

const char kCannotUseBothNewAndOldAPIMessage[] =
    "Cannot use navigator.getVRDisplays if the XR API is already in "
    "use.";

}  // namespace

NavigatorVR* NavigatorVR::From(Document& document) {
  if (!document.GetFrame() || !document.GetFrame()->DomWindow())
    return nullptr;
  Navigator& navigator = *document.GetFrame()->DomWindow()->navigator();
  return &From(navigator);
}

NavigatorVR& NavigatorVR::From(Navigator& navigator) {
  NavigatorVR* supplement = Supplement<Navigator>::From<NavigatorVR>(navigator);
  if (!supplement) {
    supplement = new NavigatorVR(navigator);
    ProvideTo(navigator, supplement);
  }
  return *supplement;
}

XR* NavigatorVR::xr(Navigator& navigator) {
  // Always return null when the navigator is detached.
  if (!navigator.GetFrame())
    return nullptr;

  return NavigatorVR::From(navigator).xr();
}

XR* NavigatorVR::xr() {
  if (!did_log_NavigatorXR_) {
    ukm::builders::XR_WebXR(GetSourceId())
        .SetDidUseNavigatorXR(1)
        .Record(GetDocument()->UkmRecorder());

    did_log_NavigatorXR_ = true;
  }

  LocalFrame* frame = GetSupplementable()->GetFrame();
  // Always return null when the navigator is detached.
  if (!frame)
    return nullptr;

  if (!xr_) {
    // For the sake of simplicity we're going to block developers from using the
    // new API if they've already made calls to the legacy API.
    if (controller_) {
      if (frame->GetDocument()) {
        frame->GetDocument()->AddConsoleMessage(ConsoleMessage::Create(
            kOtherMessageSource, kErrorMessageLevel,
            "Cannot use navigator.xr if the legacy VR API is already in use."));
      }
      return nullptr;
    }

    xr_ = XR::Create(*frame, ukm_source_id_);
    MaybeLogDidUseGamepad();
  }
  return xr_;
}

void NavigatorVR::SetDidUseGamepad() {
  did_use_gamepad_ = true;
  MaybeLogDidUseGamepad();
}

void NavigatorVR::MaybeLogDidUseGamepad() {
  // If we have used WebXR and Gamepad, and haven't already logged the metric,
  // record that Gamepad is used.
  if (xr_ && did_use_gamepad_ && !did_log_did_use_gamepad_) {
    ukm::builders::XR_WebXR(ukm_source_id_)
        .SetDidGetGamepads(1)
        .Record(GetDocument()->UkmRecorder());
    did_log_did_use_gamepad_ = true;
  }
}

ScriptPromise NavigatorVR::getVRDisplays(ScriptState* script_state,
                                         Navigator& navigator) {
  if (!navigator.GetFrame()) {
    return ScriptPromise::RejectWithDOMException(
        script_state, DOMException::Create(DOMExceptionCode::kInvalidStateError,
                                           kNotAssociatedWithDocumentMessage));
  }
  return NavigatorVR::From(navigator).getVRDisplays(script_state);
}

ScriptPromise NavigatorVR::getVRDisplays(ScriptState* script_state) {
  if (!GetDocument()) {
    return ScriptPromise::RejectWithDOMException(
        script_state, DOMException::Create(DOMExceptionCode::kInvalidStateError,
                                           kNotAssociatedWithDocumentMessage));
  }

  if (!did_log_getVRDisplays_ && GetDocument()->IsInMainFrame()) {
    did_log_getVRDisplays_ = true;

    ukm::builders::XR_WebXR(GetDocument()->UkmSourceID())
        .SetDidRequestAvailableDevices(1)
        .Record(GetDocument()->UkmRecorder());
  }

  LocalFrame* frame = GetDocument()->GetFrame();
  if (!frame) {
    return ScriptPromise::RejectWithDOMException(
        script_state, DOMException::Create(DOMExceptionCode::kInvalidStateError,
                                           kNotAssociatedWithDocumentMessage));
  }
  if (!GetDocument()->IsFeatureEnabled(mojom::FeaturePolicyFeature::kWebVr,
                                       ReportOptions::kReportOnFailure)) {
    return ScriptPromise::RejectWithDOMException(
        script_state, DOMException::Create(DOMExceptionCode::kSecurityError,
                                           kFeaturePolicyBlockedMessage));
  }

  // Similar to the restriciton above, we're going to block developers from
  // using the legacy API if they've already made calls to the new API.
  if (xr_) {
    return ScriptPromise::RejectWithDOMException(
        script_state, DOMException::Create(DOMExceptionCode::kInvalidStateError,
                                           kCannotUseBothNewAndOldAPIMessage));
  }

  UseCounter::Count(*GetDocument(), WebFeature::kVRGetDisplays);
  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  if (!execution_context->IsSecureContext())
    UseCounter::Count(*GetDocument(), WebFeature::kVRGetDisplaysInsecureOrigin);

  Platform::Current()->RecordRapporURL("VR.WebVR.GetDisplays",
                                       GetDocument()->Url());

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();
  Controller()->GetDisplays(resolver);

  return promise;
}

VRController* NavigatorVR::Controller() {
  if (!GetSupplementable()->GetFrame())
    return nullptr;

  if (!controller_) {
    controller_ = new VRController(this);
    controller_->SetListeningForActivate(focused_ && listening_for_activate_);
    controller_->FocusChanged();
  }

  return controller_;
}

Document* NavigatorVR::GetDocument() {
  if (!GetSupplementable() || !GetSupplementable()->GetFrame())
    return nullptr;

  return GetSupplementable()->GetFrame()->GetDocument();
}

void NavigatorVR::Trace(blink::Visitor* visitor) {
  visitor->Trace(xr_);
  visitor->Trace(controller_);
  Supplement<Navigator>::Trace(visitor);
}

NavigatorVR::NavigatorVR(Navigator& navigator)
    : Supplement<Navigator>(navigator),
      FocusChangedObserver(navigator.GetFrame()->GetPage()),
      ukm_source_id_(ukm::UkmRecorder::GetNewSourceID()) {
  navigator.GetFrame()->DomWindow()->RegisterEventListenerObserver(this);
  FocusedFrameChanged();

  if (navigator.GetFrame() && WebFrame::FromFrame(navigator.GetFrame())) {
    WebFrame* main_frame = WebFrame::FromFrame(navigator.GetFrame())->Top();
    if (main_frame) {
      url::Origin main_frame_origin = main_frame->GetSecurityOrigin();
      GetDocument()->UkmRecorder()->UpdateSourceURL(ukm_source_id_,
                                                    main_frame_origin.GetURL());
    }
  }
}

int64_t NavigatorVR::GetSourceId() const {
  return ukm_source_id_;
}

NavigatorVR::~NavigatorVR() = default;

const char NavigatorVR::kSupplementName[] = "NavigatorVR";

void NavigatorVR::EnqueueVREvent(VRDisplayEvent* event) {
  if (!GetSupplementable()->GetFrame())
    return;

  GetSupplementable()->GetFrame()->DomWindow()->EnqueueWindowEvent(
      *event, TaskType::kMiscPlatformAPI);
}

void NavigatorVR::DispatchVREvent(VRDisplayEvent* event) {
  if (!(GetSupplementable()->GetFrame()))
    return;

  LocalDOMWindow* window = GetSupplementable()->GetFrame()->DomWindow();
  DCHECK(window);
  event->SetTarget(window);
  window->DispatchEvent(*event);
}

void NavigatorVR::FocusedFrameChanged() {
  bool focused = IsFrameFocused(GetSupplementable()->GetFrame());
  if (focused == focused_)
    return;
  focused_ = focused;
  if (controller_) {
    controller_->SetListeningForActivate(listening_for_activate_ && focused);
    controller_->FocusChanged();
  }
}

void NavigatorVR::DidAddEventListener(LocalDOMWindow* window,
                                      const AtomicString& event_type) {
  // Don't bother if we're using the newer API
  if (xr_)
    return;

  if (event_type == EventTypeNames::vrdisplayactivate) {
    listening_for_activate_ = true;
    Controller()->SetListeningForActivate(focused_);
  } else if (event_type == EventTypeNames::vrdisplayconnect) {
    // If the page is listening for connection events make sure we've created a
    // controller so that we'll be notified of new devices.
    Controller();
  }
}

void NavigatorVR::DidRemoveEventListener(LocalDOMWindow* window,
                                         const AtomicString& event_type) {
  // Don't bother if we're using the newer API
  if (xr_)
    return;

  if (event_type == EventTypeNames::vrdisplayactivate &&
      !window->HasEventListeners(EventTypeNames::vrdisplayactivate)) {
    listening_for_activate_ = false;
    Controller()->SetListeningForActivate(false);
  }
}

void NavigatorVR::DidRemoveAllEventListeners(LocalDOMWindow* window) {
  if (xr_ || !controller_)
    return;

  controller_->SetListeningForActivate(false);
  listening_for_activate_ = false;
}

}  // namespace blink
