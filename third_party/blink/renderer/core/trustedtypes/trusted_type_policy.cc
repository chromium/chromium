// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/trustedtypes/trusted_type_policy.h"

#include "third_party/blink/renderer/core/trustedtypes/trusted_html.h"
#include "third_party/blink/renderer/core/trustedtypes/trusted_script.h"
#include "third_party/blink/renderer/core/trustedtypes/trusted_script_url.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/to_v8.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

TrustedTypePolicy::TrustedTypePolicy(const String& policy_name,
                                     TrustedTypePolicyOptions* policy_options)
    : name_(policy_name), policy_options_(policy_options) {}

TrustedHTML* TrustedTypePolicy::createHTML(ScriptState* script_state,
                                           const String& input,
                                           ExceptionState& exception_state) {
  return CreateHTML(script_state->GetIsolate(), input, exception_state);
}

TrustedScript* TrustedTypePolicy::createScript(
    ScriptState* script_state,
    const String& input,
    ExceptionState& exception_state) {
  return CreateScript(script_state->GetIsolate(), input, exception_state);
}

TrustedScriptURL* TrustedTypePolicy::createScriptURL(
    ScriptState* script_state,
    const String& input,
    ExceptionState& exception_state) {
  return CreateScriptURL(script_state->GetIsolate(), input, exception_state);
}

TrustedHTML* TrustedTypePolicy::CreateHTML(v8::Isolate* isolate,
                                           const String& input,
                                           ExceptionState& exception_state) {
  if (!policy_options_->createHTML()) {
    exception_state.ThrowTypeError(
        "Policy " + name_ +
        "'s TrustedTypePolicyOptions did not specify a 'createHTML' member.");
    return nullptr;
  }
  v8::TryCatch try_catch(isolate);
  String html;
  if (!policy_options_->createHTML()->Invoke(nullptr, input).To(&html)) {
    DCHECK(try_catch.HasCaught());
    exception_state.RethrowV8Exception(try_catch.Exception());
    return nullptr;
  }
  return MakeGarbageCollected<TrustedHTML>(html);
}

TrustedScript* TrustedTypePolicy::CreateScript(
    v8::Isolate* isolate,
    const String& input,
    ExceptionState& exception_state) {
  if (!policy_options_->createScript()) {
    exception_state.ThrowTypeError(
        "Policy " + name_ +
        "'s TrustedTypePolicyOptions did not specify a 'createScript' member.");
    return nullptr;
  }
  v8::TryCatch try_catch(isolate);
  String script;
  if (!policy_options_->createScript()->Invoke(nullptr, input).To(&script)) {
    DCHECK(try_catch.HasCaught());
    exception_state.RethrowV8Exception(try_catch.Exception());
    return nullptr;
  }
  return MakeGarbageCollected<TrustedScript>(script);
}

TrustedScriptURL* TrustedTypePolicy::CreateScriptURL(
    v8::Isolate* isolate,
    const String& input,
    ExceptionState& exception_state) {
  if (!policy_options_->createScriptURL()) {
    exception_state.ThrowTypeError("Policy " + name_ +
                                   "'s TrustedTypePolicyOptions did not "
                                   "specify a 'createScriptURL' member.");
    return nullptr;
  }
  v8::TryCatch try_catch(isolate);
  String script_url;
  if (!policy_options_->createScriptURL()
           ->Invoke(nullptr, input)
           .To(&script_url)) {
    DCHECK(try_catch.HasCaught());
    exception_state.RethrowV8Exception(try_catch.Exception());
    return nullptr;
  }
  return MakeGarbageCollected<TrustedScriptURL>(script_url);
}

String TrustedTypePolicy::name() const {
  return name_;
}

void TrustedTypePolicy::Trace(blink::Visitor* visitor) {
  visitor->Trace(policy_options_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
