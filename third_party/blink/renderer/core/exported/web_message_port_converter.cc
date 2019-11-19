// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/web/web_message_port_converter.h"

#include "third_party/blink/public/common/messaging/message_port_channel.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_message_port.h"
#include "third_party/blink/renderer/core/messaging/message_port.h"

namespace blink {

base::Optional<MessagePortChannel>
WebMessagePortConverter::DisentangleAndExtractMessagePortChannel(
    v8::Isolate* isolate,
    v8::Local<v8::Value> value) {
  MessagePort* port = V8MessagePort::ToImplWithTypeCheck(isolate, value);
  if (!port || port->IsNeutered())
    return base::nullopt;
  return port->Disentangle();
}

}  // namespace blink
