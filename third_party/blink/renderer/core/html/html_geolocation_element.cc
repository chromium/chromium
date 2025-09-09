// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/html_geolocation_element.h"

#include "third_party/blink/public/strings/grit/permission_element_strings.h"
#include "third_party/blink/renderer/core/dom/document.h"
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

}  // namespace blink
