// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/events/web_mcp_event.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_web_mcp_event_init.h"

namespace blink {

WebMCPEvent* WebMCPEvent::Create(const AtomicString& type,
                                 const WebMCPEventInit* initializer) {
  return MakeGarbageCollected<WebMCPEvent>(type, initializer,
                                           base::PassKey<WebMCPEvent>());
}

WebMCPEvent::WebMCPEvent(const AtomicString& type,
                         const WebMCPEventInit* initializer,
                         base::PassKey<WebMCPEvent>)
    : Event(type, initializer) {
  if (initializer->hasToolName()) {
    tool_name_ = initializer->toolName();
  }
}

WebMCPEvent::WebMCPEvent(const AtomicString& type,
                         const String& tool_name,
                         base::PassKey<WebMCPEvent>)
    : Event(type, Bubbles::kNo, Cancelable::kNo), tool_name_(tool_name) {}

WebMCPEvent::~WebMCPEvent() = default;

const AtomicString& WebMCPEvent::InterfaceName() const {
  return event_interface_names::kWebMCPEvent;
}

}  // namespace blink
