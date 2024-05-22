// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/input/events_helper.h"

#include "third_party/blink/public/common/features.h"

namespace content {

bool InputEventResultStateIsSetBlocking(
    blink::mojom::InputEventResultState ack_state) {
  // Default input events are marked as kNotConsumed and should not
  // be marked as blocking.
  switch (ack_state) {
    case blink::mojom::InputEventResultState::kConsumed:
      return true;
    case blink::mojom::InputEventResultState::kUnknown:
    case blink::mojom::InputEventResultState::kNotConsumed:
    case blink::mojom::InputEventResultState::kNoConsumerExists:
    case blink::mojom::InputEventResultState::kIgnored:
    case blink::mojom::InputEventResultState::kSetNonBlocking:
    case blink::mojom::InputEventResultState::kSetNonBlockingDueToFling:
      return false;
  }
}

}  // namespace content
