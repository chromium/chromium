// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/device_single_window_event_controller.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

namespace blink {

DeviceSingleWindowEventController::DeviceSingleWindowEventController(
    Document& document)
    : PlatformEventController(&document),
      needs_checking_null_events_(true),
      document_(document) {
  document.domWindow()->RegisterEventListenerObserver(this);
}

DeviceSingleWindowEventController::~DeviceSingleWindowEventController() =
    default;

void DeviceSingleWindowEventController::DidUpdateData() {
  DispatchDeviceEvent(LastEvent());
}

void DeviceSingleWindowEventController::DispatchDeviceEvent(Event* event) {
  if (!GetDocument().domWindow() || GetDocument().IsContextPaused() ||
      GetDocument().IsContextDestroyed())
    return;

  GetDocument().domWindow()->DispatchEvent(*event);

  if (needs_checking_null_events_) {
    if (IsNullEvent(event))
      StopUpdating();
    else
      needs_checking_null_events_ = false;
  }
}

void DeviceSingleWindowEventController::DidAddEventListener(
    LocalDOMWindow* window,
    const AtomicString& event_type) {
  if (event_type != EventTypeName())
    return;

  if (GetPage() && GetPage()->IsPageVisible())
    StartUpdating();

  has_event_listener_ = true;
}

void DeviceSingleWindowEventController::DidRemoveEventListener(
    LocalDOMWindow* window,
    const AtomicString& event_type) {
  if (event_type != EventTypeName() ||
      window->HasEventListeners(EventTypeName()))
    return;

  StopUpdating();
  has_event_listener_ = false;
}

void DeviceSingleWindowEventController::DidRemoveAllEventListeners(
    LocalDOMWindow*) {
  StopUpdating();
  has_event_listener_ = false;
}

bool DeviceSingleWindowEventController::IsSameSecurityOriginAsMainFrame()
    const {
  if (!GetDocument().GetFrame() || !GetDocument().GetPage())
    return false;

  if (GetDocument().GetFrame()->IsMainFrame())
    return true;

  const SecurityOrigin* main_security_origin = GetDocument()
                                                   .GetPage()
                                                   ->MainFrame()
                                                   ->GetSecurityContext()
                                                   ->GetSecurityOrigin();

  if (main_security_origin &&
      GetDocument().GetSecurityOrigin()->CanAccess(main_security_origin))
    return true;

  return false;
}

bool DeviceSingleWindowEventController::CheckPolicyFeatures(
    const Vector<mojom::FeaturePolicyFeature>& features) const {
  const Document& document = GetDocument();
  return std::all_of(features.begin(), features.end(),
                     [&document](mojom::FeaturePolicyFeature feature) {
                       return document.IsFeatureEnabled(
                           feature, ReportOptions::kReportOnFailure);
                     });
}

void DeviceSingleWindowEventController::Trace(blink::Visitor* visitor) {
  visitor->Trace(document_);
  PlatformEventController::Trace(visitor);
}

}  // namespace blink
