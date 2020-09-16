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

#include "base/macros.h"
#include "third_party/blink/renderer/bindings/core/v8/classic_evaluation_result.h"
#include "third_party/blink/renderer/bindings/core/v8/rejected_promises.h"
#include "third_party/blink/renderer/bindings/core/v8/sanitize_script_errors.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_cache_options.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/text/text_position.h"
#include "v8/include/v8.h"

namespace blink {

class ScriptSourceCode;
class WorkerOrWorkletGlobalScope;

class CORE_EXPORT WorkerOrWorkletScriptController final
    : public GarbageCollected<WorkerOrWorkletScriptController> {
 public:
  WorkerOrWorkletScriptController(WorkerOrWorkletGlobalScope*, v8::Isolate*);
  virtual ~WorkerOrWorkletScriptController();
  void Dispose();

  bool IsExecutionForbidden() const;

  // Rethrow errors flag in
  // https://html.spec.whatwg.org/C/#run-a-classic-script
  class RethrowErrorsOption final {
    STACK_ALLOCATED();

   public:
    RethrowErrorsOption(RethrowErrorsOption&&) = default;
    RethrowErrorsOption& operator=(RethrowErrorsOption&&) = default;

    RethrowErrorsOption(const RethrowErrorsOption&) = delete;
    RethrowErrorsOption& operator=(const RethrowErrorsOption&) = delete;

    // Rethrow errors flag is false.
    static RethrowErrorsOption DoNotRethrow() {
      return RethrowErrorsOption(base::nullopt);
    }

    // Rethrow errors flag is true. When rethrowing, a NetworkError with
    // `message` is thrown. This is used only for importScripts(), and
    // `message` is used to throw NetworkErrors with the same message text,
    // no matter whether the NetworkError is thrown inside or outside
    // EvaluateAndReturnValue().
    static RethrowErrorsOption Rethrow(const String& message) {
      return RethrowErrorsOption(message);
    }

    bool ShouldRethrow() const { return static_cast<bool>(message_); }
    String Message() const { return *message_; }

   private:
    explicit RethrowErrorsOption(base::Optional<String> message)
        : message_(std::move(message)) {}

    // `nullopt` <=> rethrow errors is false.
    base::Optional<String> message_;
  };

  // https://html.spec.whatwg.org/C/#run-a-classic-script
  // Callers should enter ScriptState::Scope before calling this.
  ClassicEvaluationResult EvaluateAndReturnValue(
      const ScriptSourceCode&,
      SanitizeScriptErrors sanitize_script_errors,
      V8CacheOptions = kV8CacheOptionsDefault,
      RethrowErrorsOption = RethrowErrorsOption::DoNotRethrow());

  // Prevents future JavaScript execution.
  void ForbidExecution();

  // For WorkerGlobalScope and threaded WorkletGlobalScope, |url_for_debugger|
  // is and should be used only for setting name/origin that appears in
  // DevTools.
  // For main thread WorkletGlobalScope, WorkerOrWorkletGlobalScope::Name() is
  // used for setting DOMWrapperWorld's human readable name.
  // This should be called only once.
  void Initialize(const KURL& url_for_debugger);

  // Prepares for script evaluation. This must be called after Initialize()
  // before Evaluate().
  void PrepareForEvaluation();

  // Disables `eval()` on JavaScript. This must be called before Evaluate().
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

  void Trace(Visitor*) const;

  bool IsContextInitialized() const {
    return script_state_ && !!script_state_->PerContextData();
  }

 private:
  void DisableEvalInternal(const String& error_message);

  void DisposeContextIfNeeded();

  Member<WorkerOrWorkletGlobalScope> global_scope_;

  // The v8 isolate associated to the (worker or worklet) global scope. For
  // workers this should be the worker thread's isolate, while for worklets
  // usually the main thread's isolate is used.
  v8::Isolate* isolate_;

  Member<ScriptState> script_state_;
  scoped_refptr<DOMWrapperWorld> world_;

  // Keeps the error message for `eval()` on JavaScript until Initialize().
  String disable_eval_pending_;

  bool is_ready_to_evaluate_ = false;
  bool execution_forbidden_ = false;

  scoped_refptr<RejectedPromises> rejected_promises_;

  DISALLOW_COPY_AND_ASSIGN(WorkerOrWorkletScriptController);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_WORKER_OR_WORKLET_SCRIPT_CONTROLLER_H_
