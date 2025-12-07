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
const char kAccuracyModePrecise[] = "precise";

// Timeout for querying location (in milliseconds).
constexpr uint16_t kDefaultQueryLocationTimeoutMs = 10000;

PositionOptions* CreateDefaultLocationOptions() {
  PositionOptions* options = PositionOptions::Create();
  options->setTimeout(kDefaultQueryLocationTimeoutMs);
  options->setMaximumAge(0);
  options->setEnableHighAccuracy(false);
  return options;
}

}  // namespace

HTMLGeolocationElement::HTMLGeolocationElement(Document& document)
    : HTMLPermissionElement(document, html_names::kGeolocationTag) {
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
  HTMLPermissionElement::Trace(visitor);
}

void HTMLGeolocationElement::UpdateAppearance() {
  UpdateIcon(mojom::blink::PermissionName::GEOLOCATION);
  // We need `PostTask` here because setInnerText hits a DCHECK.
  GetDocument()
      .GetTaskRunner(TaskType::kInternalDefault)
      ->PostTask(FROM_HERE, BindOnce(&HTMLGeolocationElement::UpdateText,
                                     WrapWeakPersistent(this)));
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
    if (params.new_value) {
      MaybeTriggerAutolocate(ForceAutolocate::kNo);
    }
  } else if (params.name == html_names::kWatchAttr) {
    if (!params.new_value) {
      ClearWatch();
    }
  } else if (params.name == html_names::kAccuracymodeAttr) {
    SetPreciseLocation(
        EqualIgnoringASCIICase(params.new_value, kAccuracyModePrecise));
  }

  // If it's not a geolocation element specific attribute, the base class
  // permission element can handle attributes.
  HTMLPermissionElement::AttributeChanged(params);
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
  // This function may be triggered when there is a delayed system permission
  // update. If this occurs, we will check whether the user has previously given
  // permission to determine if a geolocation search should be initiated.
  bool has_made_permission_decision_granted = PermissionsGranted();
  HTMLPermissionElement::OnPermissionStatusChange(permission_name, status);
  if (status != mojom::blink::PermissionStatus::GRANTED) {
    ClearWatch();
    return;
  }

  if (FastHasAttribute(html_names::kAutolocateAttr)) {
    MaybeTriggerAutolocate(HasPendingPermissionRequest()
                               ? ForceAutolocate::kYes
                               : ForceAutolocate::kNo);
  } else if (HasPendingPermissionRequest() ||
             has_made_permission_decision_granted) {
    RequestGeolocation();
  }
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

void HTMLGeolocationElement::GetCurrentPosition() {
  auto* geolocation = GetGeolocation();
  if (!geolocation && !WebTestSupport::IsRunningWebTest()) {
    return;
  }

  is_geolocation_request_in_progress_ = true;
  ShowInProgressAppearance();
  auto* dom_window = GetDocument().domWindow();
  if (!dom_window) {
    return;
  }

  if (!WebTestSupport::IsRunningWebTest()) {
    geolocation->GetCurrentPosition(
        blink::BindRepeating(&HTMLGeolocationElement::CurrentPositionCallback,
                             WrapWeakPersistent(this)),
        CreateDefaultLocationOptions());
  }
}

void HTMLGeolocationElement::WatchPosition() {
  auto* geolocation = GetGeolocation();
  if (!geolocation && !WebTestSupport::IsRunningWebTest()) {
    return;
  }

  is_geolocation_request_in_progress_ = true;
  ShowInProgressAppearance();

  if (!WebTestSupport::IsRunningWebTest()) {
    if (watch_id_) {
      geolocation->clearWatch(watch_id_);
    }
    watch_id_ = geolocation->WatchPosition(
        blink::BindRepeating(&HTMLGeolocationElement::CurrentPositionCallback,
                             WrapWeakPersistent(this)),
        CreateDefaultLocationOptions());
  } else {
    // In web tests, we don't have a real geolocation service.
    // Set a dummy watch_id to simulate success.
    watch_id_ = 1;
  }
}

void HTMLGeolocationElement::CurrentPositionCallback(
    base::expected<Geoposition*, GeolocationPositionError*> position) {
  is_geolocation_request_in_progress_ = false;
  MaybeHideInProgressAppearance();
  if (position.has_value()) {
    position_ = position.value();
    error_ = nullptr;
  } else {
    error_ = position.error();
    position_ = nullptr;
  }
  EnqueueEvent(*Event::CreateCancelableBubble(event_type_names::kLocation),
               TaskType::kUserInteraction);
}

Geolocation* HTMLGeolocationElement::GetGeolocation() {
  auto* dom_window = GetDocument().domWindow();
  if (!dom_window) {
    return nullptr;
  }
  return Geolocation::geolocation(*dom_window->navigator());
}

void HTMLGeolocationElement::MaybeHideInProgressAppearance() {
  if (!ShouldShowInProgressAppearance()) {
    in_progress_appearance_started_time_ = base::TimeTicks();
    // TODO(crbug.com/461538228): We drop the in-progress UI until we have a
    // well-considered approach for devs to better control that UI.
  }
}

void HTMLGeolocationElement::ShowInProgressAppearance() {
  in_progress_appearance_started_time_ = base::TimeTicks::Now();
  // TODO(crbug.com/461538228): We drop the in-progress UI until we have a
  // well-considered approach for devs to better control that UI.
}

bool HTMLGeolocationElement::ShouldShowInProgressAppearance() {
  // TODO(crbug.com/461538228): We drop the in-progress UI until we have a
  // well-considered approach for devs to better control that UI.
  return false;
}

void HTMLGeolocationElement::RequestGeolocation() {
  if (in_progress_appearance_started_time_ != base::TimeTicks()) {
    return;
  }
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

void HTMLGeolocationElement::UpdateText() {
  uint16_t message_id = GetTranslatedMessageID(
      is_precise_location() ? IDS_PERMISSION_REQUEST_PRECISE_GEOLOCATION
                            : IDS_PERMISSION_REQUEST_GEOLOCATION,
      ComputeInheritedLanguage().LowerASCII());
  CHECK(message_id);
  permission_text_span()->setInnerText(GetLocale().QueryString(message_id));
}

}  // namespace blink
