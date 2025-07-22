// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_GEOLOCATION_ELEMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_GEOLOCATION_ELEMENT_H_

#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/html/html_permission_element.h"
#include "third_party/blink/renderer/platform/heap/member.h"

namespace blink {

class CORE_EXPORT HTMLGeolocationElement final : public HTMLPermissionElement {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit HTMLGeolocationElement(Document&);

  DEFINE_ATTRIBUTE_EVENT_LISTENER(location, kLocation)

  void Trace(Visitor*) const override;
};

}  // namespace blink
#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_GEOLOCATION_ELEMENT_H_
