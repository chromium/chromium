// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_INPUT_EVENT_ACK_STATE_H_
#define CONTENT_PUBLIC_COMMON_INPUT_EVENT_ACK_STATE_H_

namespace content {

const char* InputEventResultStateToString(
    blink::mojom::InputEventResultState ack_state);

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_INPUT_EVENT_ACK_STATE_H_
