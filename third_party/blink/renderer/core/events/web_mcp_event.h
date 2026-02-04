// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_EVENTS_WEB_MCP_EVENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_EVENTS_WEB_MCP_EVENT_H_

#include "base/types/pass_key.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/event_interface_names.h"

namespace blink {

class WebMCPEventInit;

class WebMCPEvent : public Event {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static WebMCPEvent* Create(const AtomicString& type,
                             const WebMCPEventInit* initializer);

  static WebMCPEvent* Create(const AtomicString& type,
                             const String& tool_name) {
    return MakeGarbageCollected<WebMCPEvent>(type, tool_name,
                                             base::PassKey<WebMCPEvent>());
  }

  WebMCPEvent(const AtomicString& type,
              const WebMCPEventInit* initializer,
              base::PassKey<WebMCPEvent>);
  WebMCPEvent(const AtomicString& type,
              const String& tool_name,
              base::PassKey<WebMCPEvent>);
  ~WebMCPEvent() override;

  const String& toolName() const { return tool_name_; }

  const AtomicString& InterfaceName() const override;

 private:
  String tool_name_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_EVENTS_WEB_MCP_EVENT_H_
