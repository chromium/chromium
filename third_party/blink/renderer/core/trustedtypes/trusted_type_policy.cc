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

TrustedHTML* TrustedTypePolicy::createHTML(v8::Isolate* isolate,
                                           const String& input,
                                           const HeapVector<ScriptValue>& args,
                                           ExceptionState& exception_state) {
  // https://w3c.github.io/trusted-types/dist/spec/#create-a-trusted-type-algorithm
  // with |type name| being TrustedHTML.
  TrustedHTML* html = createHTMLInternal(isolate, input, args, exception_state);
  if (exception_state.HadException()) {
    return nullptr;
  }
  // createHTMLInternal does not do step 4,
  // "If policyValue is null or undefined, set dataString to the empty string."
  if (html->toString().IsNull()) {
    return MakeGarbageCollected<TrustedHTML>(g_empty_string);
  }
  return html;
}

TrustedHTML* TrustedTypePolicy::createHTMLInternal(
    v8::Isolate* isolate,
    const String& input,
    const HeapVector<ScriptValue>& args,
    ExceptionState& exception_state) {
  // https://w3c.github.io/trusted-types/dist/spec/#get-trusted-type-policy-value-algorithm
  // Except that we'll pass through null-ish Strings to the caller, so that we
  // can use this for both createHTML and default policy handling.
  if (!policy_options_->hasCreateHTML()) {
    exception_state.ThrowTypeError(
        StrCat({"Policy ", name_,
                "'s TrustedTypePolicyOptions did not specify a 'createHTML' "
                "member."}));
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

TrustedScript* TrustedTypePolicy::createScript(
    v8::Isolate* isolate,
    const String& input,
    const HeapVector<ScriptValue>& args,
    ExceptionState& exception_state) {
  // https://w3c.github.io/trusted-types/dist/spec/#create-a-trusted-type-algorithm
  // with |type name| being TrustedScript.
  TrustedScript* script =
      createScriptInternal(isolate, input, args, exception_state);
  if (exception_state.HadException()) {
    return nullptr;
  }
  // createScriptInternal does not do step 4,
  // "If policyValue is null or undefined, set dataString to the empty string."
  if (script->toString().IsNull()) {
    return MakeGarbageCollected<TrustedScript>(g_empty_string);
  }
  return script;
}

TrustedScript* TrustedTypePolicy::createScriptInternal(
    v8::Isolate* isolate,
    const String& input,
    const HeapVector<ScriptValue>& args,
    ExceptionState& exception_state) {
  // https://w3c.github.io/trusted-types/dist/spec/#get-trusted-type-policy-value-algorithm
  // Except that we'll pass through null-ish Strings to the caller, so that we
  // can use this for both createScript and default policy handling.
  if (!policy_options_->hasCreateScript()) {
    exception_state.ThrowTypeError(
        StrCat({"Policy ", name_,
                "'s TrustedTypePolicyOptions did not "
                "specify a 'createScript' member."}));
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

TrustedScriptURL* TrustedTypePolicy::createScriptURL(
    v8::Isolate* isolate,
    const String& input,
    const HeapVector<ScriptValue>& args,
    ExceptionState& exception_state) {
  // https://w3c.github.io/trusted-types/dist/spec/#create-a-trusted-type-algorithm
  // with |type name| being TrustedScriptURL.
  TrustedScriptURL* script_url =
      createScriptURLInternal(isolate, input, args, exception_state);
  if (exception_state.HadException()) {
    return nullptr;
  }
  // createScriptURLInternal does not do step 4,
  // "If policyValue is null or undefined, set dataString to the empty string."
  if (script_url->toString().IsNull()) {
    return MakeGarbageCollected<TrustedScriptURL>(g_empty_string);
  }
  return script_url;
}

TrustedScriptURL* TrustedTypePolicy::createScriptURLInternal(
    v8::Isolate* isolate,
    const String& input,
    const HeapVector<ScriptValue>& args,
    ExceptionState& exception_state) {
  // https://w3c.github.io/trusted-types/dist/spec/#get-trusted-type-policy-value-algorithm
  // Except that we'll pass through null-ish Strings to the caller, so that we
  // can use this for both createScriptURL and default policy handling.
  if (!policy_options_->hasCreateScriptURL()) {
    exception_state.ThrowTypeError(
        StrCat({"Policy ", name_,
                "'s TrustedTypePolicyOptions did not specify a "
                "'createScriptURL' member."}));
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
