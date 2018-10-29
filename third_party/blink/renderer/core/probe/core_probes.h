/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PROBE_CORE_PROBES_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PROBE_CORE_PROBES_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/platform/probe/platform_probes.h"

namespace network {
namespace mojom {
namespace blink {
class WebSocketHandshakeResponse;
class WebSocketHandshakeRequest;
}  // namespace blink
}  // namespace mojom
}  // namespace network

namespace blink {

class CoreProbeSink;
class ThreadDebugger;

namespace probe {

class CORE_EXPORT AsyncTask {
  STACK_ALLOCATED();

 public:
  AsyncTask(ExecutionContext*,
            void* task,
            const char* step = nullptr,
            bool enabled = true);
  ~AsyncTask();

 private:
  ThreadDebugger* debugger_;
  void* task_;
  bool recurring_;
};

// Called from generated instrumentation code.
inline CoreProbeSink* ToCoreProbeSink(LocalFrame* frame) {
  return frame ? frame->GetProbeSink() : nullptr;
}

inline CoreProbeSink* ToCoreProbeSink(Document& document) {
  return document.GetProbeSink();
}

inline CoreProbeSink* ToCoreProbeSink(Document* document) {
  return document ? ToCoreProbeSink(*document) : nullptr;
}

inline CoreProbeSink* ToCoreProbeSink(ExecutionContext* context) {
  return context ? context->GetProbeSink() : nullptr;
}

inline CoreProbeSink* ToCoreProbeSink(Node* node) {
  return node ? ToCoreProbeSink(node->GetDocument()) : nullptr;
}

inline CoreProbeSink* ToCoreProbeSink(EventTarget* event_target) {
  return event_target ? ToCoreProbeSink(event_target->GetExecutionContext())
                      : nullptr;
}

CORE_EXPORT void AsyncTaskScheduled(ExecutionContext*,
                                    const StringView& name,
                                    void*);
CORE_EXPORT void AsyncTaskScheduledBreakable(ExecutionContext*,
                                             const char* name,
                                             void*);
CORE_EXPORT void AsyncTaskCanceled(ExecutionContext*, void*);
CORE_EXPORT void AsyncTaskCanceled(v8::Isolate*, void*);
CORE_EXPORT void AsyncTaskCanceledBreakable(ExecutionContext*,
                                            const char* name,
                                            void*);
CORE_EXPORT void AllAsyncTasksCanceled(ExecutionContext*);

}  // namespace probe
}  // namespace blink

#include "third_party/blink/renderer/core/core_probes_inl.h"

#endif  // !defined(CoreProbes_h)
