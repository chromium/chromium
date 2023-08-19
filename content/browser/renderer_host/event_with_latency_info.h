// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_EVENT_WITH_LATENCY_INFO_H_
#define CONTENT_BROWSER_RENDERER_HOST_EVENT_WITH_LATENCY_INFO_H_

#include "content/common/input/event_with_latency_info.h"
#include "content/public/common/input/native_web_keyboard_event.h"

namespace content {

typedef EventWithLatencyInfo<NativeWebKeyboardEvent>
    NativeWebKeyboardEventWithLatencyInfo;

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_EVENT_WITH_LATENCY_INFO_H_
