/*
 * Copyright (C) 2009, 2012 Google Inc. All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_WORKER_OR_WORKLET_SCRIPT_CONTROLLER_H_
#define THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_WORKER_OR_WORKLET_SCRIPT_CONTROLLER_H_

#include "third_party/blink/renderer/bindings/core/v8/rejected_promises.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_cache_options.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/loader/fetch/access_control_status.h"
#include "third_party/blink/renderer/platform/wtf/allocator.h"
#include "third_party/blink/renderer/platform/wtf/text/text_position.h"
#include "v8/include/v8.h"

namespace blink {

class ErrorEvent;
class ExceptionState;
class ScriptSourceCode;
class WorkerOrWorkletGlobalScope;

class CORE_EXPORT WorkerOrWorkletScriptController final
    : public GarbageCollectedFinalized<WorkerOrWorkletScriptController> {
  WTF_MAKE_NONCOPYABLE(WorkerOrWorkletScriptController);

 public:
  static WorkerOrWorkletScriptController* Create(WorkerOrWorkletGlobalScope*,
                                                 v8::Isolate*);
  virtual ~WorkerOrWorkletScriptController();
  void Dispose();

  bool IsExecutionForbidden() const;

  // Returns true if the evaluation completed with no uncaught exception.
  bool Evaluate(const ScriptSourceCode&,
                AccessControlStatus access_control_status,
                ErrorEvent** = nullptr,
                V8CacheOptions = kV8CacheOptionsDefault);

  // Prevents future JavaScript execution.
  void ForbidExecution();

  // Used by WorkerThread. Returns true if the context is successfully
  // initialized or already initialized.
  // For WorkerGlobalScope and threaded WorkletGlobalScope, |url_for_debugger|
  // is and should be used only for setting name/origin that appears in
  // DevTools. For other global scopes, |human_readable_name| is used for
  // setting DOMWrapperWorld's human readable name.
  bool InitializeContextIfNeeded(const String& human_readable_name,
                                 const KURL& url_for_debugger);

  // Used by WorkerGlobalScope:
  void RethrowExceptionFromImportedScript(ErrorEvent*, ExceptionState&);
  void DisableEval(const String&);

  // Used by Inspector agents:
  ScriptState* GetScriptState() { return script_state_; }

  // Used by V8 bindings:
  v8::Local<v8::Context> GetContext() {
    return script_state_ ? script_state_->GetContext()
                         : v8::Local<v8::Context>();
  }

  RejectedPromises* GetRejectedPromises() const {
    return rejected_promises_.get();
  }

  void Trace(blink::Visitor*);

  bool IsContextInitialized() const {
    return script_state_ && !!script_state_->PerContextData();
  }

  ScriptValue EvaluateAndReturnValueForTest(const ScriptSourceCode&);

 private:
  WorkerOrWorkletScriptController(WorkerOrWorkletGlobalScope*, v8::Isolate*);
  class ExecutionState;

  // Evaluate a script file in the current execution environment.
  ScriptValue EvaluateInternal(const ScriptSourceCode&,
                               AccessControlStatus,
                               V8CacheOptions);
  void DisposeContextIfNeeded();

  Member<WorkerOrWorkletGlobalScope> global_scope_;

  // The v8 isolate associated to the (worker or worklet) global scope. For
  // workers this should be the worker thread's isolate, while for worklets
  // usually the main thread's isolate is used.
  v8::Isolate* isolate_;

  Member<ScriptState> script_state_;
  scoped_refptr<DOMWrapperWorld> world_;
  String disable_eval_pending_;
  bool execution_forbidden_;

  scoped_refptr<RejectedPromises> rejected_promises_;

  // |m_executionState| refers to a stack object that evaluate() allocates;
  // evaluate() ensuring that the pointer reference to it is removed upon
  // returning. Hence kept as a bare pointer here, and not a Persistent with
  // Oilpan enabled; stack scanning will visit the object and
  // trace its on-heap fields.
  GC_PLUGIN_IGNORE("394615")
  ExecutionState* execution_state_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_WORKER_OR_WORKLET_SCRIPT_CONTROLLER_H_
