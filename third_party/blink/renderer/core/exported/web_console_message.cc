// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/web/web_console_message.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {

void WebConsoleMessage::LogWebConsoleMessage(v8::Local<v8::Context> context,
                                             const WebConsoleMessage& message) {
  auto* execution_context = ToExecutionContext(context);
  if (!execution_context)  // Can happen in unittests.
    return;

  LocalFrame* frame = nullptr;
  if (auto* document = DynamicTo<Document>(execution_context))
    frame = document->GetFrame();
  execution_context->AddConsoleMessage(
      ConsoleMessage::CreateFromWebConsoleMessage(message, frame));
}

}  // namespace blink
