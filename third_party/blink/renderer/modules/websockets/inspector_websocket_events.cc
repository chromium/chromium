// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/websockets/inspector_websocket_events.h"

#include <memory>
#include "base/trace_event/trace_event.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/inspector/identifiers_factory.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/core/workers/worker_thread.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

namespace blink {
namespace {

void AddCommonData(ExecutionContext* execution_context,
                   uint64_t identifier,
                   perfetto::TracedDictionary& dict) {
  DCHECK(execution_context->IsContextThread());
  dict.Add("identifier", identifier);
  if (auto* window = DynamicTo<LocalDOMWindow>(execution_context)) {
    dict.Add("frame", IdentifiersFactory::FrameId(window->GetFrame()));
  } else if (auto* scope = DynamicTo<WorkerGlobalScope>(execution_context)) {
    dict.Add("workerId", IdentifiersFactory::IdFromToken(
                             scope->GetThread()->GetDevToolsWorkerToken()));
  } else {
    NOTREACHED_IN_MIGRATION()
        << "WebSocket is available only in Window and WorkerGlobalScope";
  }
}

} // namespace

void InspectorWebSocketCreateEvent::Data(perfetto::TracedValue context,
                                         ExecutionContext* execution_context,
                                         uint64_t identifier,
                                         const KURL& url,
                                         const String& protocol) {
  auto dict = std::move(context).WriteDictionary();
  AddCommonData(execution_context, identifier, dict);
  dict.Add("url", url.GetString());
  if (!protocol.IsNull())
    dict.Add("webSocketProtocol", protocol);
  SetCallStack(execution_context->GetIsolate(), dict);
}

void InspectorWebSocketEvent::Data(perfetto::TracedValue context,
                                   ExecutionContext* execution_context,
                                   uint64_t identifier) {
  auto dict = std::move(context).WriteDictionary();
  AddCommonData(execution_context, identifier, dict);
  SetCallStack(execution_context->GetIsolate(), dict);
}

void InspectorWebSocketTransferEvent::Data(perfetto::TracedValue context,
                                           ExecutionContext* execution_context,
                                           uint64_t identifier,
                                           uint64_t data_length) {
  auto dict = std::move(context).WriteDictionary();
  AddCommonData(execution_context, identifier, dict);
  dict.Add("dataLength", data_length);
  SetCallStack(execution_context->GetIsolate(), dict);
}

}  // namespace blink
