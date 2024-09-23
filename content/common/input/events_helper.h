// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_INPUT_EVENTS_HELPER_H_
#define CONTENT_COMMON_INPUT_EVENTS_HELPER_H_

#include "third_party/blink/public/mojom/input/input_event_result.mojom-shared.h"

namespace content {

// Utility to map the event ack state from the renderer, returns true if the
// event could be handled blocking.
bool InputEventResultStateIsSetBlocking(blink::mojom::InputEventResultState);

}  // namespace content

#endif  // CONTENT_COMMON_INPUT_EVENTS_HELPER_H_
