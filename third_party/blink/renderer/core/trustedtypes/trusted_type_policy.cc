// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/trustedtypes/trusted_type_policy.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_create_html_callback.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_create_script_callback.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_create_url_callback.h"
#include "third_party/blink/renderer/core/trustedtypes/trusted_html.h"
#include "third_party/blink/renderer/core/trustedtypes/trusted_script.h"
#include "third_party/blink/renderer/core/trustedtypes/trusted_script_url.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

TrustedTypePolicy::TrustedTypePolicy(const String& policy_name,
                                     TrustedTypePolicyOptions* policy_options)
    : name_(policy_name), policy_options_(policy_options) {
  DCHECK(policy_options_);
}

TrustedHTML* TrustedTypePolicy::createHTML(ScriptState* script_state,
                                           const String& input,
                                           const HeapVector<ScriptValue>& args,
                                           ExceptionState& exception_state) {
  return CreateHTML(script_state->GetIsolate(), input, args, exception_state);
}

TrustedScript* TrustedTypePolicy::createScript(
    ScriptState* script_state,
    const String& input,
    const HeapVector<ScriptValue>& args,
    ExceptionState& exception_state) {
  return CreateScript(script_state->GetIsolate(), input, args, exception_state);
}

TrustedScriptURL* TrustedTypePolicy::createScriptURL(
    ScriptState* script_state,
    const String& input,
    const HeapVector<ScriptValue>& args,
    ExceptionState& exception_state) {
  return CreateScriptURL(script_state->GetIsolate(), input, args,
                         exception_state);
}

TrustedHTML* TrustedTypePolicy::CreateHTML(v8::Isolate* isolate,
                                           const String& input,
                                           const HeapVector<ScriptValue>& args,
                                           ExceptionState& exception_state) {
  if (!policy_options_->hasCreateHTML()) {
    exception_state.ThrowTypeError(
        "Policy " + name_ +
        "'s TrustedTypePolicyOptions did not specify a 'createHTML' member.");
    return nullptr;
  }
  TryRethrowScope rethrow_scope(isolate, exception_state);
  String html;
  if (!policy_options_->createHTML()->Invoke(nullptr, input, args).To(&html)) {
    DCHECK(rethrow_scope.HasCaught());
    return nullptr;
  }
  return MakeGarbageCollected<TrustedHTML>(html);
}

TrustedScript* TrustedTypePolicy::CreateScript(
    v8::Isolate* isolate,
    const String& input,
    const HeapVector<ScriptValue>& args,
    ExceptionState& exception_state) {
  if (!policy_options_->hasCreateScript()) {
    exception_state.ThrowTypeError(
        "Policy " + name_ +
        "'s TrustedTypePolicyOptions did not specify a 'createScript' member.");
    return nullptr;
  }
  TryRethrowScope rethrow_scope(isolate, exception_state);
  String script;
  if (!policy_options_->createScript()
           ->Invoke(nullptr, input, args)
           .To(&script)) {
    DCHECK(rethrow_scope.HasCaught());
    return nullptr;
  }
  return MakeGarbageCollected<TrustedScript>(script);
}

TrustedScriptURL* TrustedTypePolicy::CreateScriptURL(
    v8::Isolate* isolate,
    const String& input,
    const HeapVector<ScriptValue>& args,
    ExceptionState& exception_state) {
  if (!policy_options_->hasCreateScriptURL()) {
    exception_state.ThrowTypeError("Policy " + name_ +
                                   "'s TrustedTypePolicyOptions did not "
                                   "specify a 'createScriptURL' member.");
    return nullptr;
  }
  TryRethrowScope rethrow_scope(isolate, exception_state);
  String script_url;
  if (!policy_options_->createScriptURL()
           ->Invoke(nullptr, input, args)
           .To(&script_url)) {
    DCHECK(rethrow_scope.HasCaught());
    return nullptr;
  }
  return MakeGarbageCollected<TrustedScriptURL>(script_url);
}

bool TrustedTypePolicy::HasCreateHTML() {
  return policy_options_->hasCreateHTML();
}

bool TrustedTypePolicy::HasCreateScript() {
  return policy_options_->hasCreateScript();
}

bool TrustedTypePolicy::HasCreateScriptURL() {
  return policy_options_->hasCreateScriptURL();
}

String TrustedTypePolicy::name() const {
  return name_;
}

void TrustedTypePolicy::Trace(Visitor* visitor) const {
  visitor->Trace(policy_options_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
