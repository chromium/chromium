// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/anchor_element_listener.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/events/pointer_event.h"
#include "third_party/blink/renderer/core/html/html_anchor_element.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

namespace {
constexpr const int16_t kMainEventButtonValue = 0;
constexpr const int16_t kAuxiliaryEventButtonValue = 1;
}  // namespace

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
  // TODO(crbug.com/1297312): Check if user changed the default mouse settings
  if (DynamicTo<PointerEvent>(event)->button() != kMainEventButtonValue &&
      DynamicTo<PointerEvent>(event)->button() != kAuxiliaryEventButtonValue) {
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
