// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/main_thread/pending_user_input.h"

#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {
namespace scheduler {

void PendingUserInput::Monitor::OnEnqueue(
    WebInputEvent::Type type,
    const WebInputEventAttribution& attribution) {
  DCHECK_NE(type, WebInputEvent::Type::kUndefined);
  DCHECK_LE(type, WebInputEvent::Type::kTypeLast);

  // Ignore events without attribution information.
  if (attribution.type() == WebInputEventAttribution::kUnknown)
    return;

  auto result =
      pending_events_.insert(AttributionGroup(attribution), EventCounter());
  auto& value = result.stored_value->value;
  if (IsContinuousEventType(type)) {
    value.num_continuous++;
  } else {
    value.num_discrete++;
  }
}

void PendingUserInput::Monitor::OnDequeue(
    WebInputEvent::Type type,
    const WebInputEventAttribution& attribution) {
  DCHECK_NE(type, WebInputEvent::Type::kUndefined);
  DCHECK_LE(type, WebInputEvent::Type::kTypeLast);

  if (attribution.type() == WebInputEventAttribution::kUnknown)
    return;

  auto it = pending_events_.find(AttributionGroup(attribution));
  DCHECK_NE(it, pending_events_.end());

  auto& value = it->value;
  if (IsContinuousEventType(type)) {
    DCHECK_GT(value.num_continuous, 0U);
    value.num_continuous--;
  } else {
    DCHECK_GT(value.num_discrete, 0U);
    value.num_discrete--;
  }

  if (value.num_continuous == 0 && value.num_discrete == 0) {
    pending_events_.erase(it->key);
  }
}

Vector<WebInputEventAttribution> PendingUserInput::Monitor::Info(
    bool include_continuous) const {
  Vector<WebInputEventAttribution> attributions;
  for (const auto& entry : pending_events_) {
    if (entry.value.num_discrete > 0 ||
        (entry.value.num_continuous > 0 && include_continuous)) {
      attributions.push_back(entry.key.attribution);
    }
  }
  return attributions;
}

bool PendingUserInput::IsContinuousEventType(WebInputEvent::Type type) {
  switch (type) {
    case WebInputEvent::Type::kMouseMove:
    case WebInputEvent::Type::kMouseWheel:
    case WebInputEvent::Type::kTouchMove:
    case WebInputEvent::Type::kPointerMove:
    case WebInputEvent::Type::kPointerRawUpdate:
      return true;
    default:
      return false;
  }
}

}  // namespace scheduler
}  // namespace blink
