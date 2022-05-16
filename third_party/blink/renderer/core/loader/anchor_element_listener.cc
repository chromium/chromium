// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/anchor_element_listener.h"
#include "third_party/blink/public/common/input/web_pointer_properties.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/events/pointer_event.h"
#include "third_party/blink/renderer/core/html/html_anchor_element.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

namespace blink {

AnchorElementListener::AnchorElementListener(
    base::RepeatingCallback<void(const KURL&)> callback)
    : tracker_callback_(std::move(callback)) {}

void AnchorElementListener::Invoke(ExecutionContext* execution_context,
                                   Event* event) {
  if (!event->target()) {
    return;
  }
  if (!event->target()->ToNode()) {
    return;
  }
  if (!event->target()->ToNode()->IsHTMLElement()) {
    return;
  }
  PointerEvent* pointer_event = DynamicTo<PointerEvent>(event);
  if (!pointer_event) {
    // Pages are allowed to dispatch any event to the 'pointerdown' type, which
    // may result in this code running with an |event| that is not of type
    // PointerEvent.
    return;
  }
  if (!pointer_event->isPrimary()) {
    return;
  }
  // TODO(crbug.com/1297312): Check if user changed the default mouse settings
  if (pointer_event->button() !=
          static_cast<int>(WebPointerProperties::Button::kLeft) &&
      pointer_event->button() !=
          static_cast<int>(WebPointerProperties::Button::kMiddle)) {
    return;
  }
  Node* node = event->srcElement()->ToNode();
  HTMLAnchorElement* html_anchor_element =
      FirstAnchorElementIncludingSelf(node);
  if (!html_anchor_element) {
    return;
  }
  KURL anchor_url = GetHrefEligibleForPreloading(*html_anchor_element);
  if (anchor_url.IsEmpty()) {
    return;
  }
  tracker_callback_.Run(anchor_url);
}

HTMLAnchorElement* AnchorElementListener::FirstAnchorElementIncludingSelf(
    Node* node) {
  HTMLAnchorElement* html_anchor_element = nullptr;
  while (node && !html_anchor_element) {
    html_anchor_element = DynamicTo<HTMLAnchorElement>(node);
    node = node->parentNode();
  }
  return html_anchor_element;
}

KURL AnchorElementListener::GetHrefEligibleForPreloading(
    const HTMLAnchorElement& html_anchor_element) {
  KURL url = html_anchor_element.Href();
  if (url.ProtocolIsInHTTPFamily()) {
    return url;
  }
  return KURL("");
}

}  // namespace blink
