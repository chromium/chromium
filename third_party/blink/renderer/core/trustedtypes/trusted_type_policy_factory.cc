// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/trustedtypes/trusted_type_policy_factory.h"

#include "third_party/blink/public/mojom/web_feature/web_feature.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_trusted_html.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_trusted_script.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_trusted_script_url.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/events/before_create_policy_event.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/trustedtypes/trusted_type_policy.h"
#include "third_party/blink/renderer/core/trustedtypes/trusted_types_util.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_dom_wrapper.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hash.h"

namespace blink {

TrustedTypePolicy* TrustedTypePolicyFactory::createPolicy(
    const String& policy_name,
    const TrustedTypePolicyOptions* policy_options,
    ExceptionState& exception_state) {
  if (RuntimeEnabledFeatures::TrustedTypeBeforePolicyCreationEventEnabled()) {
    DispatchEventResult result =
        DispatchEvent(*BeforeCreatePolicyEvent::Create(policy_name));
    if (result != DispatchEventResult::kNotCanceled) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kNotAllowedError,
          "The policy creation has been canceled.");
      return nullptr;
    }
  }
  if (!GetExecutionContext()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "The document is detached.");
    return nullptr;
  }
  UseCounter::Count(GetExecutionContext(),
                    WebFeature::kTrustedTypesCreatePolicy);

  if (RuntimeEnabledFeatures::TrustedDOMTypesEnabled(GetExecutionContext()) &&
      GetExecutionContext()->GetContentSecurityPolicy()) {
    ContentSecurityPolicy::AllowTrustedTypePolicyDetails violation_details =
        ContentSecurityPolicy::AllowTrustedTypePolicyDetails::kAllowed;
    bool disallowed = !GetExecutionContext()
                           ->GetContentSecurityPolicy()
                           ->AllowTrustedTypePolicy(
                               policy_name, policy_map_.Contains(policy_name),
                               violation_details);
    if (disallowed) {
      // For a better error message, we'd like to disambiguate between
      // "disallowed" and "disallowed because of a duplicate name".
      bool disallowed_because_of_duplicate_name =
          violation_details ==
          ContentSecurityPolicy::AllowTrustedTypePolicyDetails::
              kDisallowedDuplicateName;
      const String message =
          disallowed_because_of_duplicate_name
              ? "Policy with name \"" + policy_name + "\" already exists."
              : "Policy \"" + policy_name + "\" disallowed.";
      exception_state.ThrowTypeError(message);
      return nullptr;
    }
  }
  UseCounter::Count(GetExecutionContext(),
                    WebFeature::kTrustedTypesPolicyCreated);
  if (policy_name == "default") {
    DCHECK(!policy_map_.Contains("default"));
    UseCounter::Count(GetExecutionContext(),
                      WebFeature::kTrustedTypesDefaultPolicyCreated);
  }

  auto* policy = MakeGarbageCollected<TrustedTypePolicy>(
      policy_name, const_cast<TrustedTypePolicyOptions*>(policy_options));
  policy_map_.insert(policy_name, policy);
  return policy;
}

TrustedTypePolicy* TrustedTypePolicyFactory::defaultPolicy() const {
  return policy_map_.at("default");
}

TrustedTypePolicyFactory::TrustedTypePolicyFactory(ExecutionContext* context)
    : ExecutionContextClient(context),
      empty_html_(MakeGarbageCollected<TrustedHTML>("")),
      empty_script_(MakeGarbageCollected<TrustedScript>("")) {}

const WrapperTypeInfo*
TrustedTypePolicyFactory::GetWrapperTypeInfoFromScriptValue(
    ScriptState* script_state,
    const ScriptValue& script_value) {
  v8::Local<v8::Value> value = script_value.V8Value();
  if (value.IsEmpty() || !value->IsObject() ||
      !V8DOMWrapper::IsWrapper(script_state->GetIsolate(), value))
    return nullptr;
  v8::Local<v8::Object> object = value.As<v8::Object>();
  return ToWrapperTypeInfo(object);
}

bool TrustedTypePolicyFactory::isHTML(ScriptState* script_state,
                                      const ScriptValue& script_value) {
  const WrapperTypeInfo* wrapper_type_info =
      GetWrapperTypeInfoFromScriptValue(script_state, script_value);
  return wrapper_type_info &&
         wrapper_type_info->Equals(V8TrustedHTML::GetWrapperTypeInfo());
}

bool TrustedTypePolicyFactory::isScript(ScriptState* script_state,
                                        const ScriptValue& script_value) {
  const WrapperTypeInfo* wrapper_type_info =
      GetWrapperTypeInfoFromScriptValue(script_state, script_value);
  return wrapper_type_info &&
         wrapper_type_info->Equals(V8TrustedScript::GetWrapperTypeInfo());
}

bool TrustedTypePolicyFactory::isScriptURL(ScriptState* script_state,
                                           const ScriptValue& script_value) {
  const WrapperTypeInfo* wrapper_type_info =
      GetWrapperTypeInfoFromScriptValue(script_state, script_value);
  return wrapper_type_info &&
         wrapper_type_info->Equals(V8TrustedScriptURL::GetWrapperTypeInfo());
}

TrustedHTML* TrustedTypePolicyFactory::emptyHTML() const {
  return empty_html_.Get();
}

TrustedScript* TrustedTypePolicyFactory::emptyScript() const {
  return empty_script_.Get();
}

const struct {
  const char* element;
  const char* property;
  const char* element_namespace;
  SpecificTrustedType type;
  // We use this table for both attributes and properties. Most are both,
  // because DOM "reflects" the attributes onto their specified poperties.
  // We use is_not_* so that the default value initialization (false) will
  // match the common case and we only need to explicitly provide the uncommon
  // values.
  bool is_not_property : 1;
  bool is_not_attribute : 1;
} kTypeTable[] = {
    {"embed", "src", nullptr, SpecificTrustedType::kScriptURL},
    {"iframe", "srcdoc", nullptr, SpecificTrustedType::kHTML},
    {"object", "codeBase", nullptr, SpecificTrustedType::kScriptURL},
    {"object", "data", nullptr, SpecificTrustedType::kScriptURL},
    {"script", "innerText", nullptr, SpecificTrustedType::kScript, false, true},
    {"script", "src", nullptr, SpecificTrustedType::kScriptURL},
    {"script", "text", nullptr, SpecificTrustedType::kScript, false, true},
    {"script", "textContent", nullptr, SpecificTrustedType::kScript, false,
     true},
    {"*", "innerHTML", nullptr, SpecificTrustedType::kHTML, false, true},
    {"*", "outerHTML", nullptr, SpecificTrustedType::kHTML, false, true},
    {"*", "on*", nullptr, SpecificTrustedType::kScript, true, false},
};

// Does a type table entry match a property?
// (Properties are evaluated by JavaScript and are case-sensitive.)
bool EqualsProperty(decltype(*kTypeTable)& left,
                    const String& tag,
                    const String& attr,
                    const String& ns) {
  DCHECK_EQ(tag.LowerASCII(), tag);
  return (left.element == tag || !strcmp(left.element, "*")) &&
         (left.property == attr ||
          (!strcmp(left.property, "on*") && attr.StartsWith("on"))) &&
         left.element_namespace == ns && !left.is_not_property;
}

// Does a type table entry match an attribute?
// (Attributes get queried by calling acecssor methods on the DOM. These are
//  case-insensitivem, because DOM.)
bool EqualsAttribute(decltype(*kTypeTable)& left,
                     const String& tag,
                     const String& attr,
                     const String& ns) {
  DCHECK_EQ(tag.LowerASCII(), tag);
  return (left.element == tag || !strcmp(left.element, "*")) &&
         (String(left.property).LowerASCII() == attr.LowerASCII() ||
          (!strcmp(left.property, "on*") && attr.StartsWith("on"))) &&
         left.element_namespace == ns && !left.is_not_attribute;
}

String getTrustedTypeName(SpecificTrustedType type) {
  switch (type) {
    case SpecificTrustedType::kHTML:
      return "TrustedHTML";
    case SpecificTrustedType::kScript:
      return "TrustedScript";
    case SpecificTrustedType::kScriptURL:
      return "TrustedScriptURL";
    case SpecificTrustedType::kNone:
      return String();
  }
}

typedef bool (*PropertyEqualsFn)(decltype(*kTypeTable)&,
                                 const String&,
                                 const String&,
                                 const String&);

String FindTypeInTypeTable(const String& tagName,
                           const String& propertyName,
                           const String& elementNS,
                           PropertyEqualsFn equals) {
  SpecificTrustedType type = SpecificTrustedType::kNone;
  for (auto* it = std::cbegin(kTypeTable); it != std::cend(kTypeTable); it++) {
    if ((*equals)(*it, tagName, propertyName, elementNS)) {
      type = it->type;
      break;
    }
  }
  return getTrustedTypeName(type);
}

String TrustedTypePolicyFactory::getPropertyType(
    const String& tagName,
    const String& propertyName,
    const String& elementNS) const {
  return FindTypeInTypeTable(tagName.LowerASCII(), propertyName, elementNS,
                             &EqualsProperty);
}

String TrustedTypePolicyFactory::getAttributeType(
    const String& tagName,
    const String& attributeName,
    const String& tagNS,
    const String& attributeNS) const {
  return FindTypeInTypeTable(tagName.LowerASCII(), attributeName, tagNS,
                             &EqualsAttribute);
}

String TrustedTypePolicyFactory::getPropertyType(
    const String& tagName,
    const String& propertyName) const {
  return getPropertyType(tagName, propertyName, String());
}

String TrustedTypePolicyFactory::getAttributeType(
    const String& tagName,
    const String& attributeName) const {
  return getAttributeType(tagName, attributeName, String(), String());
}

String TrustedTypePolicyFactory::getAttributeType(const String& tagName,
                                                  const String& attributeName,
                                                  const String& tagNS) const {
  return getAttributeType(tagName, attributeName, tagNS, String());
}

ScriptValue TrustedTypePolicyFactory::getTypeMapping(
    ScriptState* script_state) const {
  return getTypeMapping(script_state, String());
}

ScriptValue TrustedTypePolicyFactory::getTypeMapping(ScriptState* script_state,
                                                     const String& ns) const {
  // Create three-deep dictionary of properties, like so:
  // {tagname: { ["attributes"|"properties"]: { attribute: type }}}

  if (!ns.IsEmpty())
    return ScriptValue();

  v8::HandleScope handle_scope(script_state->GetIsolate());
  v8::Local<v8::Object> top = v8::Object::New(script_state->GetIsolate());
  v8::Local<v8::Object> properties;
  v8::Local<v8::Object> attributes;
  const char* element = nullptr;
  for (const auto& iter : kTypeTable) {
    if (properties.IsEmpty() || !element || strcmp(iter.element, element)) {
      element = iter.element;
      v8::Local<v8::Object> middle =
          v8::Object::New(script_state->GetIsolate());
      top->Set(script_state->GetContext(),
               V8String(script_state->GetIsolate(), iter.element), middle)
          .Check();
      properties = v8::Object::New(script_state->GetIsolate());
      middle
          ->Set(script_state->GetContext(),
                V8String(script_state->GetIsolate(), "properties"), properties)
          .Check();
      attributes = v8::Object::New(script_state->GetIsolate());
      middle
          ->Set(script_state->GetContext(),
                V8String(script_state->GetIsolate(), "attributes"), attributes)
          .Check();
    }
    if (!iter.is_not_property) {
      properties
          ->Set(script_state->GetContext(),
                V8String(script_state->GetIsolate(), iter.property),
                V8String(script_state->GetIsolate(),
                         getTrustedTypeName(iter.type)))
          .Check();
    }
    if (!iter.is_not_attribute) {
      attributes
          ->Set(script_state->GetContext(),
                V8String(script_state->GetIsolate(), iter.property),
                V8String(script_state->GetIsolate(),
                         getTrustedTypeName(iter.type)))
          .Check();
    }
  }

  return ScriptValue(script_state->GetIsolate(), top);
}

void TrustedTypePolicyFactory::CountTrustedTypeAssignmentError() {
  if (!hadAssignmentError) {
    UseCounter::Count(GetExecutionContext(),
                      WebFeature::kTrustedTypesAssignmentError);
    hadAssignmentError = true;
  }
}

const AtomicString& TrustedTypePolicyFactory::InterfaceName() const {
  return event_target_names::kTrustedTypePolicyFactory;
}

ExecutionContext* TrustedTypePolicyFactory::GetExecutionContext() const {
  return ExecutionContextClient::GetExecutionContext();
}

void TrustedTypePolicyFactory::Trace(Visitor* visitor) const {
  EventTargetWithInlineData::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
  visitor->Trace(empty_html_);
  visitor->Trace(empty_script_);
  visitor->Trace(policy_map_);
}

}  // namespace blink
