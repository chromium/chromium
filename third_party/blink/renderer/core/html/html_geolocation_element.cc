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

namespace blink {

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

void HTMLGeolocationElement::UpdateText() {
  // TODO(crbug.com/435376388): There will be more strings related to location
  // data querying, for example: Sending location.
  uint16_t message_id = GetTranslatedMessageID(
      is_precise_location() ? IDS_PERMISSION_REQUEST_PRECISE_GEOLOCATION
                            : IDS_PERMISSION_REQUEST_GEOLOCATION,
      ComputeInheritedLanguage().LowerASCII());
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
  // The autolocate and the watch attributes are exclusive to the geolocation
  // element, other attributes will be handled by the HTMLPermissionElement.
  if (params.name == html_names::kAutolocateAttr) {
    GetCurrentPosition();
  }
  if (params.name == html_names::kWatchAttr) {
    if (params.new_value) {
      WatchPosition();
    } else {
      auto* geolocation = GetGeolocation();
      if (!geolocation) {
        return;
      }
      geolocation->clearWatch(watch_id_);
      watch_id_ = 0;
    }
  }
  HTMLPermissionElement::AttributeChanged(params);
}

void HTMLGeolocationElement::GetCurrentPosition() {
  auto* geolocation = GetGeolocation();
  if (!geolocation) {
    return;
  }
  geolocation->GetCurrentPosition(
      blink::BindRepeating(&HTMLGeolocationElement::CurrentPositionCallback,
                           WrapWeakPersistent(this)));
}

void HTMLGeolocationElement::WatchPosition() {
  auto* geolocation = GetGeolocation();
  if (!geolocation) {
    return;
  }
  watch_id_ = geolocation->WatchPosition(
      blink::BindRepeating(&HTMLGeolocationElement::CurrentPositionCallback,
                           WrapWeakPersistent(this)));
}
void HTMLGeolocationElement::CurrentPositionCallback(
    base::expected<Geoposition*, GeolocationPositionError*> position) {
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

}  // namespace blink
