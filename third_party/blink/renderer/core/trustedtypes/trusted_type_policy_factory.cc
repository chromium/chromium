// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/trustedtypes/trusted_type_policy_factory.h"

#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_trusted_html.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_trusted_script.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_trusted_script_url.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_trusted_url.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/trustedtypes/trusted_type_policy.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hash.h"

namespace blink {

TrustedTypePolicy* TrustedTypePolicyFactory::createPolicy(
    const String& policy_name,
    const TrustedTypePolicyOptions& policy_options,
    bool exposed,
    ExceptionState& exception_state) {
  if (!GetFrame()
           ->GetDocument()
           ->GetContentSecurityPolicy()
           ->AllowTrustedTypePolicy(policy_name)) {
    exception_state.ThrowTypeError("Policy " + policy_name + " disallowed.");
    return nullptr;
  }
  // TODO(orsibatiz): After policy naming rules are estabilished, check for the
  // policy_name to be according to them.
  if (policy_map_.Contains(policy_name)) {
    exception_state.ThrowTypeError("Policy with name" + policy_name +
                                   " already exists.");
    return nullptr;
  }
  TrustedTypePolicy* policy =
      TrustedTypePolicy::Create(policy_name, policy_options, exposed);
  policy_map_.insert(policy_name, policy);
  return policy;
}

TrustedTypePolicy* TrustedTypePolicyFactory::getExposedPolicy(
    const String& policy_name) {
  TrustedTypePolicy* p = policy_map_.at(policy_name);
  if (p && p->exposed()) {
    return p;
  }
  return nullptr;
}

TrustedTypePolicyFactory::TrustedTypePolicyFactory(LocalFrame* frame)
    : DOMWindowClient(frame) {}

Vector<String> TrustedTypePolicyFactory::getPolicyNames() const {
  Vector<String> policyNames;
  for (const String name : policy_map_.Keys()) {
    policyNames.push_back(name);
  }
  return policyNames;
}

const WrapperTypeInfo*
TrustedTypePolicyFactory::GetWrapperTypeInfoFromScriptValue(
    ScriptState* script_state,
    const ScriptValue& script_value) {
  v8::Local<v8::Value> value = script_value.V8Value();
  v8::Isolate* isolate = script_state->GetIsolate();
  if (value.IsEmpty() || !value->IsObject() ||
      !V8DOMWrapper::IsWrapper(isolate, value))
    return nullptr;
  v8::Local<v8::Object> object = script_value.V8Value()->ToObject(isolate);
  return ToWrapperTypeInfo(object);
}

bool TrustedTypePolicyFactory::isHTML(ScriptState* script_state,
                                      const ScriptValue& script_value) {
  const WrapperTypeInfo* wrapper_type_info =
      GetWrapperTypeInfoFromScriptValue(script_state, script_value);
  return wrapper_type_info &&
         wrapper_type_info->Equals(&V8TrustedHTML::wrapperTypeInfo);
}

bool TrustedTypePolicyFactory::isScript(ScriptState* script_state,
                                        const ScriptValue& script_value) {
  const WrapperTypeInfo* wrapper_type_info =
      GetWrapperTypeInfoFromScriptValue(script_state, script_value);
  return wrapper_type_info &&
         wrapper_type_info->Equals(&V8TrustedScript::wrapperTypeInfo);
}

bool TrustedTypePolicyFactory::isScriptURL(ScriptState* script_state,
                                           const ScriptValue& script_value) {
  const WrapperTypeInfo* wrapper_type_info =
      GetWrapperTypeInfoFromScriptValue(script_state, script_value);
  return wrapper_type_info &&
         wrapper_type_info->Equals(&V8TrustedScriptURL::wrapperTypeInfo);
}

bool TrustedTypePolicyFactory::isURL(ScriptState* script_state,
                                     const ScriptValue& script_value) {
  const WrapperTypeInfo* wrapper_type_info =
      GetWrapperTypeInfoFromScriptValue(script_state, script_value);
  return wrapper_type_info &&
         wrapper_type_info->Equals(&V8TrustedURL::wrapperTypeInfo);
}

void TrustedTypePolicyFactory::Trace(blink::Visitor* visitor) {
  ScriptWrappable::Trace(visitor);
  DOMWindowClient::Trace(visitor);
  visitor->Trace(policy_map_);
}

}  // namespace blink
