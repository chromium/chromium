// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/html_geolocation_element.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/html/html_permission_element.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

HTMLGeolocationElement::HTMLGeolocationElement(Document& document)
    : HTMLPermissionElement(document) {
  CHECK(RuntimeEnabledFeatures::GeolocationElementEnabled(
      document.GetExecutionContext()));
  setType(AtomicString("geolocation"));
}

void HTMLGeolocationElement::Trace(Visitor* visitor) const {
  HTMLPermissionElement::Trace(visitor);
}
}  // namespace blink
