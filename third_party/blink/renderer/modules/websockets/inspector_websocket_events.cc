// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/websockets/inspector_websocket_events.h"

#include <memory>
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/inspector/identifiers_factory.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/core/workers/worker_thread.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

namespace blink {

std::unique_ptr<TracedValue> InspectorWebSocketCreateEvent::Data(
    ExecutionContext* execution_context,
    uint64_t identifier,
    const KURL& url,
    const String& protocol) {
  DCHECK(execution_context->IsContextThread());
  auto value = std::make_unique<TracedValue>();
  value->SetInteger("identifier", static_cast<int>(identifier));
  value->SetString("url", url.GetString());
  if (auto* document = DynamicTo<Document>(execution_context)) {
    value->SetString("frame",
                     IdentifiersFactory::FrameId(document->GetFrame()));
  } else if (auto* scope = DynamicTo<WorkerGlobalScope>(execution_context)) {
    value->SetString("workerId",
                     IdentifiersFactory::IdFromToken(
                         scope->GetThread()->GetDevToolsWorkerToken()));
  } else {
    NOTREACHED()
        << "WebSocket is available only in Document and WorkerGlobalScope";
  }
  if (!protocol.IsNull())
    value->SetString("webSocketProtocol", protocol);
  SetCallStack(value.get());
  return value;
}

std::unique_ptr<TracedValue> InspectorWebSocketEvent::Data(
    ExecutionContext* execution_context,
    uint64_t identifier) {
  DCHECK(execution_context->IsContextThread());
  auto value = std::make_unique<TracedValue>();
  value->SetInteger("identifier", static_cast<int>(identifier));
  if (auto* document = DynamicTo<Document>(execution_context)) {
    value->SetString("frame",
                     IdentifiersFactory::FrameId(document->GetFrame()));
  } else if (auto* scope = DynamicTo<WorkerGlobalScope>(execution_context)) {
    value->SetString("workerId",
                     IdentifiersFactory::IdFromToken(
                         scope->GetThread()->GetDevToolsWorkerToken()));
  } else {
    NOTREACHED()
        << "WebSocket is available only in Document and WorkerGlobalScope";
  }
  SetCallStack(value.get());
  return value;
}

}  // namespace blink
