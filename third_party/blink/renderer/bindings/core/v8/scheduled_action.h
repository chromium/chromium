/*
 * Copyright (C) 2007-2009 Google Inc. All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_SCHEDULED_ACTION_H_
#define THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_SCHEDULED_ACTION_H_

#include "third_party/blink/renderer/platform/bindings/name_client.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "v8/include/v8.h"

namespace blink {

class ExecutionContext;
class LocalFrame;
class ScriptState;
class ScriptStateProtectingContext;
class ScriptValue;
class V8Function;
class WorkerGlobalScope;

class ScheduledAction final : public GarbageCollected<ScheduledAction>,
                              public NameClient {
  DISALLOW_COPY_AND_ASSIGN(ScheduledAction);

 public:
  ScheduledAction(ScriptState*,
                  ExecutionContext* target,
                  V8Function* handler,
                  const HeapVector<ScriptValue>& arguments);
  ScheduledAction(ScriptState*,
                  ExecutionContext* target,
                  const String& handler);

  ~ScheduledAction();

  void Dispose();

  void Execute(ExecutionContext*);

  void Trace(blink::Visitor*);

  const char* NameInHeapSnapshot() const override { return "ScheduledAction"; }

 private:
  void Execute(LocalFrame*);
  void Execute(WorkerGlobalScope*);

  Member<ScriptStateProtectingContext> script_state_;
  Member<V8Function> function_;
  HeapVector<ScriptValue> arguments_;
  String code_;
};

}  // namespace blink

#endif  // ScheduledAction
