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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_WORKER_INSPECTOR_CONTROLLER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_WORKER_INSPECTOR_CONTROLLER_H_

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/unguessable_token.h"
#include "third_party/blink/renderer/core/inspector/inspector_session.h"
#include "third_party/blink/renderer/core/inspector/inspector_task_runner.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/allocator.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"

namespace blink {

class CoreProbeSink;
class WorkerThread;
class WorkerThreadDebugger;

class WorkerInspectorController final
    : public GarbageCollectedFinalized<WorkerInspectorController>,
      public TraceEvent::EnabledStateObserver,
      public InspectorSession::Client,
      private Thread::TaskObserver {
 public:
  static WorkerInspectorController* Create(WorkerThread*);
  ~WorkerInspectorController() override;
  void Trace(blink::Visitor*);

  CoreProbeSink* GetProbeSink() const { return probe_sink_.Get(); }

  void ConnectFrontend(int session_id);
  void DisconnectFrontend(int session_id);
  void DispatchMessageFromFrontend(int session_id, const String& message);
  void Dispose();
  void FlushProtocolNotifications();

 private:
  WorkerInspectorController(WorkerThread*, WorkerThreadDebugger*);

  // InspectorSession::Client implementation.
  void SendProtocolResponse(
      int session_id,
      int call_id,
      const String& response,
      mojom::blink::DevToolsSessionStatePtr updates) override;
  void SendProtocolNotification(
      int session_id,
      const String& message,
      mojom::blink::DevToolsSessionStatePtr updates) override;

  // Thread::TaskObserver implementation.
  void WillProcessTask(const base::PendingTask&) override;
  void DidProcessTask(const base::PendingTask&) override;

  // blink::TraceEvent::EnabledStateObserver implementation:
  void OnTraceLogEnabled() override;
  void OnTraceLogDisabled() override;

  void EmitTraceEvent();

  WorkerThreadDebugger* debugger_;
  WorkerThread* thread_;
  Member<CoreProbeSink> probe_sink_;
  HeapHashMap<int, Member<InspectorSession>> sessions_;

  // These fields are set up in the constructor and then read
  // on a random thread from EmitTraceEvent().
  base::UnguessableToken worker_devtools_token_;
  base::UnguessableToken parent_devtools_token_;
  KURL url_;
  PlatformThreadId worker_thread_id_;

  DISALLOW_COPY_AND_ASSIGN(WorkerInspectorController);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_WORKER_INSPECTOR_CONTROLLER_H_
