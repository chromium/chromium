// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBSOCKETS_INSPECTOR_WEBSOCKET_EVENTS_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBSOCKETS_INSPECTOR_WEBSOCKET_EVENTS_H_

#include <memory>
#include "third_party/blink/renderer/core/inspector/inspector_trace_events.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/traced_value.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

class ExecutionContext;
class KURL;

class InspectorWebSocketCreateEvent {
  STATIC_ONLY(InspectorWebSocketCreateEvent);

 public:
  static std::unique_ptr<TracedValue> Data(ExecutionContext*,
                                           uint64_t identifier,
                                           const KURL&,
                                           const String& protocol);
};

class InspectorWebSocketEvent {
  STATIC_ONLY(InspectorWebSocketEvent);

 public:
  static std::unique_ptr<TracedValue> Data(ExecutionContext*,
                                           uint64_t identifier);
};

}  // namespace blink

#endif  // !defined(InspectorWebSocketEvents_h)
