/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/testing/v8/web_core_test_support.h"

#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/testing/internal_settings.h"
#include "third_party/blink/renderer/core/testing/internals.h"
#include "third_party/blink/renderer/core/testing/worker_internals.h"
#include "third_party/blink/renderer/platform/bindings/dom_wrapper_world.h"
#include "third_party/blink/renderer/platform/bindings/v8_per_context_data.h"

namespace blink {

namespace web_core_test_support {

namespace {

v8::Local<v8::Value> CreateInternalsObject(v8::Local<v8::Context> context) {
  v8::Isolate* isolate = context->GetIsolate();
  ScriptState* script_state = ScriptState::From(isolate, context);
  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  if (execution_context->IsWindow()) {
    return ToV8Traits<Internals>::ToV8(
        script_state, MakeGarbageCollected<Internals>(execution_context));
  }
  if (execution_context->IsWorkerGlobalScope()) {
    return ToV8Traits<WorkerInternals>::ToV8(
        script_state, MakeGarbageCollected<WorkerInternals>());
  }
  return v8::Local<v8::Value>();
}

}  // namespace

void InjectInternalsObject(v8::Local<v8::Context> context) {
  v8::Isolate* isolate = context->GetIsolate();
  ScriptState* script_state = ScriptState::From(isolate, context);
  ScriptState::Scope scope(script_state);
  v8::Local<v8::Value> internals = CreateInternalsObject(context);
  if (internals.IsEmpty())
    return;

  v8::Local<v8::Object> global = context->Global();
  global
      ->CreateDataProperty(
          context, V8AtomicString(script_state->GetIsolate(), "internals"),
          internals)
      .ToChecked();
}

void ResetInternalsObject(v8::Local<v8::Context> context) {
  // This can happen if JavaScript is disabled in the main frame.
  if (context.IsEmpty())
    return;

  v8::Isolate* isolate = context->GetIsolate();
  ScriptState* script_state = ScriptState::From(isolate, context);
  ScriptState::Scope scope(script_state);
  LocalFrame* frame = LocalDOMWindow::From(script_state)->GetFrame();
  // Should the frame have been detached, the page is assumed being destroyed
  // (=> no reset required.)
  if (!frame)
    return;
  Page* page = frame->GetPage();
  DCHECK(page);
  Internals::ResetToConsistentState(page);
  InternalSettings::From(*page)->ResetToConsistentState();
}

}  // namespace web_core_test_support

}  // namespace blink
