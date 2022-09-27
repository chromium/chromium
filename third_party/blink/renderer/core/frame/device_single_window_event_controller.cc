// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/device_single_window_event_controller.h"

#include "base/ranges/algorithm.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

namespace blink {

DeviceSingleWindowEventController::DeviceSingleWindowEventController(
    LocalDOMWindow& window)
    : PlatformEventController(window), needs_checking_null_events_(true) {
  window.RegisterEventListenerObserver(this);
}

DeviceSingleWindowEventController::~DeviceSingleWindowEventController() =
    default;

void DeviceSingleWindowEventController::DidUpdateData() {
  DispatchDeviceEvent(LastEvent());
}

void DeviceSingleWindowEventController::DispatchDeviceEvent(Event* event) {
  if (GetWindow().IsContextPaused() || GetWindow().IsContextDestroyed())
    return;

  GetWindow().DispatchEvent(*event);

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

bool DeviceSingleWindowEventController::CheckPolicyFeatures(
    const Vector<mojom::blink::PermissionsPolicyFeature>& features) const {
  LocalDOMWindow& window = GetWindow();
  return base::ranges::all_of(
      features, [&window](mojom::blink::PermissionsPolicyFeature feature) {
        return window.IsFeatureEnabled(feature,
                                       ReportOptions::kReportOnFailure);
      });
}

void DeviceSingleWindowEventController::Trace(Visitor* visitor) const {
  PlatformEventController::Trace(visitor);
}

}  // namespace blink
