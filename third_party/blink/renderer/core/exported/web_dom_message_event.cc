/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/public/web/web_dom_message_event.h"

#include "third_party/blink/public/mojom/messaging/delegated_capability.mojom-blink.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_frame.h"
#include "third_party/blink/public/web/web_serialized_script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/serialized_script_value.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/core/events/message_event.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/user_activation.h"
#include "third_party/blink/renderer/core/messaging/blink_transferable_message.h"
#include "third_party/blink/renderer/core/messaging/message_port.h"

namespace blink {

WebDOMMessageEvent::WebDOMMessageEvent(
    const WebSerializedScriptValue& message_data,
    const WebString& origin,
    const WebFrame* source_frame,
    const WebDocument& target_document,
    WebVector<MessagePortChannel> channels)
    : WebDOMMessageEvent(MessageEvent::Create()) {
  DOMWindow* window = nullptr;
  if (source_frame)
    window = WebFrame::ToCoreFrame(*source_frame)->DomWindow();
  MessagePortArray* ports = nullptr;
  if (!target_document.IsNull()) {
    Document* core_document = target_document;
    ports = MessagePort::EntanglePorts(*core_document->GetExecutionContext(),
                                       std::move(channels));
  }
  // TODO(esprehn): Chromium always passes empty string for lastEventId, is that
  // right?
  Unwrap<MessageEvent>()->initMessageEvent(
      event_type_names::kMessage, false, false, message_data, origin,
      "" /*lastEventId*/, window, ports, nullptr /*user_activation*/,
      mojom::blink::DelegatedCapability::kNone);
}

WebString WebDOMMessageEvent::Origin() const {
  return WebString(ConstUnwrap<MessageEvent>()->origin());
}

}  // namespace blink
