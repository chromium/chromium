// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/navigation_api/navigate_event_dispatch_params.h"

#include "third_party/blink/renderer/bindings/core/v8/serialization/serialized_script_value.h"
#include "third_party/blink/renderer/core/html/forms/html_form_element.h"
#include "third_party/blink/renderer/core/loader/history_item.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

namespace blink {

NavigateEventDispatchParams::NavigateEventDispatchParams(
    const KURL& url_in,
    NavigateEventType event_type_in,
    WebFrameLoadType frame_load_type_in)
    : url(url_in),
      event_type(event_type_in),
      frame_load_type(frame_load_type_in) {}

NavigateEventDispatchParams::~NavigateEventDispatchParams() = default;

void NavigateEventDispatchParams::Trace(Visitor* visitor) const {
  visitor->Trace(source_element);
  visitor->Trace(destination_item);
}

}  // namespace blink
