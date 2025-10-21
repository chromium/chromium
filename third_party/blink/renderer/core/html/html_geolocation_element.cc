// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/html_geolocation_element.h"

#include "third_party/blink/public/mojom/permissions/permission.mojom-blink.h"
#include "third_party/blink/public/strings/grit/permission_element_strings.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/navigator.h"
#include "third_party/blink/renderer/core/geolocation/geolocation.h"
#include "third_party/blink/renderer/core/html/html_permission_element.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/text/platform_locale.h"
#include "third_party/blink/renderer/platform/web_test_support.h"

namespace blink {

namespace {
// The minimum time that the spinning icon should be displayed.
constexpr base::TimeDelta kMinimumSpinningIconTime = base::Seconds(2);
const char kAccuracyModePrecise[] = "precise";
}  // namespace

HTMLGeolocationElement::HTMLGeolocationElement(Document& document)
    : HTMLPermissionElement(document, html_names::kGeolocationTag),
      spinning_icon_timer_(document.GetTaskRunner(TaskType::kInternalDefault),
                           this,
                           &HTMLGeolocationElement::SpinningIconTimerFired) {
  CHECK(RuntimeEnabledFeatures::GeolocationElementEnabled(
      document.GetExecutionContext()));
  setType(AtomicString("geolocation"));
}

Geoposition* HTMLGeolocationElement::position() const {
  return position_.Get();
}

GeolocationPositionError* HTMLGeolocationElement::error() const {
  return error_.Get();
}

void HTMLGeolocationElement::Trace(Visitor* visitor) const {
  visitor->Trace(position_);
  visitor->Trace(error_);
  visitor->Trace(spinning_icon_timer_);
  HTMLPermissionElement::Trace(visitor);
}

bool HTMLGeolocationElement::ShouldShowSpinningIcon() {
  return is_geolocation_request_in_progress_ ||
         (base::TimeTicks::Now() - spinning_started_time_ <
          kMinimumSpinningIconTime);
}

void HTMLGeolocationElement::UpdateAppearance() {
  uint16_t message_id;
  if (ShouldShowSpinningIcon()) {
    UpdateIcon(mojom::blink::PermissionName::GEOLOCATION,
               HTMLPermissionIconElement::VisualState::kWaiting);
    message_id =
        GetTranslatedMessageID(IDS_PERMISSION_REQUEST_USING_LOCATION,
                               ComputeInheritedLanguage().LowerASCII());
  } else {
    UpdateIcon(mojom::blink::PermissionName::GEOLOCATION);
    message_id = GetTranslatedMessageID(
        is_precise_location() ? IDS_PERMISSION_REQUEST_PRECISE_GEOLOCATION
                              : IDS_PERMISSION_REQUEST_GEOLOCATION,
        ComputeInheritedLanguage().LowerASCII());
  }
  CHECK(message_id);
  permission_text_span()->setInnerText(GetLocale().QueryString(message_id));
}

void HTMLGeolocationElement::UpdatePermissionStatusAndAppearance() {
  // Appearance will not be updated based on permission statuses. It will only
  // be updated based on the status of location data querying.
  UpdatePermissionStatus();
  PseudoStateChanged(CSSSelector::kPseudoPermissionGranted);
}

mojom::blink::EmbeddedPermissionRequestDescriptorPtr
HTMLGeolocationElement::CreateEmbeddedPermissionRequestDescriptor() {
  auto descriptor = mojom::blink::EmbeddedPermissionRequestDescriptor::New();
  descriptor->element_position = BoundsInWidget();
  descriptor->geolocation =
      mojom::blink::GeolocationEmbeddedPermissionRequestDescriptor::New();
  descriptor->geolocation->autolocate = autolocate();
  return descriptor;
}

Geolocation* HTMLGeolocationElement::GetGeolocation() {
  auto* dom_window = GetDocument().domWindow();
  if (!dom_window) {
    return nullptr;
  }
  return Geolocation::geolocation(*dom_window->navigator());
}

void HTMLGeolocationElement::AttributeChanged(
    const AttributeModificationParams& params) {
  // The "preciselocation" attribute does not have a special meaning on the
  // geolocation element. It is handled by the generic HTMLElement attribute
  // changed function to avoid the special handling in HTMLPermissionElement.
  // TODO(crbug.com/450801233): Remove this when the "preciselocation"
  // attribute is removed entirely along with the "geolocation" permission
  // element type.
  if (params.name == html_names::kPreciselocationAttr) {
    HTMLElement::AttributeChanged(params);
    return;
  } else if (params.name == html_names::kAutolocateAttr) {
    if (!params.new_value) {
      did_autolocate_trigger_request = false;
    } else {
      MaybeTriggerAutolocate(ForceAutolocate::kNo);
    }
  } else if (params.name == html_names::kWatchAttr) {
    if (!params.new_value) {
      ClearWatch();
    }
  } else if (params.name == html_names::kAccuracymodeAttr &&
             EqualIgnoringASCIICase(params.new_value, kAccuracyModePrecise)) {
    SetPreciseLocation();
  }

  // If it's not a geolocation element specific attribute, the base class
  // permission element can handle attributes.
  HTMLPermissionElement::AttributeChanged(params);
}

void HTMLGeolocationElement::DidFinishLifecycleUpdate(
    const LocalFrameView& view) {
  HTMLPermissionElement::DidFinishLifecycleUpdate(view);
  if (FastHasAttribute(html_names::kAutolocateAttr)) {
    MaybeTriggerAutolocate(ForceAutolocate::kNo);
  }
}

void HTMLGeolocationElement::OnActivated() {
  if (FastHasAttribute(html_names::kAutolocateAttr)) {
    MaybeTriggerAutolocate(ForceAutolocate::kYes);
  } else {
    RequestGeolocation();
  }
}

void HTMLGeolocationElement::DefaultEventHandler(Event& event) {
  // We consume the click event here if the permission is already granted
  // and propagate any other events to the parent HTMLPermissionElement.
  if (event.type() == event_type_names::kDOMActivate && PermissionsGranted()) {
    HandleActivation(event,
                     blink::BindOnce(&HTMLGeolocationElement::OnActivated,
                                     WrapWeakPersistent(this)));
    return;
  }
  HTMLPermissionElement::DefaultEventHandler(event);
}

void HTMLGeolocationElement::OnPermissionStatusChange(
    mojom::blink::PermissionName permission_name,
    mojom::blink::PermissionStatus status) {
  HTMLPermissionElement::OnPermissionStatusChange(permission_name, status);
  if (status != mojom::blink::PermissionStatus::GRANTED) {
    did_autolocate_trigger_request = false;
    ClearWatch();
    return;
  }

  if (FastHasAttribute(html_names::kAutolocateAttr)) {
    MaybeTriggerAutolocate(HasPendingPermissionRequest()
                               ? ForceAutolocate::kYes
                               : ForceAutolocate::kNo);
  } else if (HasPendingPermissionRequest()) {
    RequestGeolocation();
  }
}

void HTMLGeolocationElement::RequestGeolocation() {
  if (FastHasAttribute(html_names::kWatchAttr)) {
    WatchPosition();
  } else {
    GetCurrentPosition();
  }
}

void HTMLGeolocationElement::ClearWatch() {
  if (!watch_id_) {
    return;
  }
  if (auto* geolocation = GetGeolocation()) {
    geolocation->clearWatch(watch_id_);
    watch_id_ = 0;
  }
}

void HTMLGeolocationElement::MaybeTriggerAutolocate(ForceAutolocate force) {
  CHECK(FastHasAttribute(html_names::kAutolocateAttr));
  if (force == ForceAutolocate::kYes ||
      (!did_autolocate_trigger_request && IsRenderered() &&
       PermissionsGranted())) {
    did_autolocate_trigger_request = true;
    RequestGeolocation();
  }
}

void HTMLGeolocationElement::GetCurrentPosition() {
  auto* geolocation = GetGeolocation();
  if (!geolocation && !WebTestSupport::IsRunningWebTest()) {
    return;
  }

  StartSpinning(RequestInProgress::kYes);
  auto* dom_window = GetDocument().domWindow();
  if (!dom_window) {
    return;
  }

  if (!WebTestSupport::IsRunningWebTest()) {
    geolocation->GetCurrentPosition(
        blink::BindRepeating(&HTMLGeolocationElement::CurrentPositionCallback,
                             WrapWeakPersistent(this)));
  }
}

void HTMLGeolocationElement::WatchPosition() {
  auto* geolocation = GetGeolocation();
  if (!geolocation && !WebTestSupport::IsRunningWebTest()) {
    return;
  }

  StartSpinning(RequestInProgress::kYes);

  if (!WebTestSupport::IsRunningWebTest()) {
    if (watch_id_) {
      geolocation->clearWatch(watch_id_);
    }
    watch_id_ = geolocation->WatchPosition(
        blink::BindRepeating(&HTMLGeolocationElement::CurrentPositionCallback,
                             WrapWeakPersistent(this)));
  } else {
    // In web tests, we don't have a real geolocation service.
    // Set a dummy watch_id to simulate success.
    watch_id_ = 1;
  }
}
void HTMLGeolocationElement::CurrentPositionCallback(
    base::expected<Geoposition*, GeolocationPositionError*> position) {
  is_geolocation_request_in_progress_ = false;
  MaybeStopSpinning();
  if (position.has_value()) {
    position_ = position.value();
    error_ = nullptr;
  } else {
    error_ = position.error();
    position_ = nullptr;
  }
  EnqueueEvent(*Event::CreateCancelableBubble(event_type_names::kLocation),
               TaskType::kUserInteraction);

  if (watch_id_ != 0) {
    StartSpinning(RequestInProgress::kNo);
  }
}

void HTMLGeolocationElement::SpinningIconTimerFired(TimerBase*) {
  MaybeStopSpinning();
}

void HTMLGeolocationElement::MaybeStopSpinning() {
  if (!ShouldShowSpinningIcon()) {
    spinning_icon_timer_.Stop();
    UpdateAppearance();
  }
}

void HTMLGeolocationElement::StartSpinning(
    RequestInProgress request_in_progress) {
  if (request_in_progress == RequestInProgress::kYes) {
    is_geolocation_request_in_progress_ = true;
  }
  spinning_started_time_ = base::TimeTicks::Now();
  spinning_icon_timer_.StartOneShot(kMinimumSpinningIconTime, FROM_HERE);
  UpdateAppearance();
}

}  // namespace blink
