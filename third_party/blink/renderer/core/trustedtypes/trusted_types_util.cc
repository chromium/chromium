// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/trustedtypes/trusted_types_util.h"

#include "base/unguessable_token.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom-blink-forward.h"
#include "third_party/blink/public/mojom/reporting/reporting.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_string_trustedscript.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_stringlegacynulltoemptystring_trustedscript.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_trustedhtml_trustedscript_trustedscripturl.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/inspector/exception_metadata.h"
#include "third_party/blink/renderer/core/inspector/identifiers_factory.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/script/script_element_base.h"
#include "third_party/blink/renderer/core/trustedtypes/trusted_html.h"
#include "third_party/blink/renderer/core/trustedtypes/trusted_script.h"
#include "third_party/blink/renderer/core/trustedtypes/trusted_script_url.h"
#include "third_party/blink/renderer/core/trustedtypes/trusted_type_policy.h"
#include "third_party/blink/renderer/core/trustedtypes/trusted_type_policy_factory.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

namespace {

enum TrustedTypeViolationKind {
  kTrustedHTMLAssignment,
  kTrustedScriptAssignment,
  kTrustedScriptURLAssignment,
  kTrustedHTMLAssignmentAndDefaultPolicyFailed,
  kTrustedHTMLAssignmentAndNoDefaultPolicyExisted,
  kTrustedScriptAssignmentAndDefaultPolicyFailed,
  kTrustedScriptAssignmentAndNoDefaultPolicyExisted,
  kTrustedScriptURLAssignmentAndDefaultPolicyFailed,
  kTrustedScriptURLAssignmentAndNoDefaultPolicyExisted,
  kNavigateToJavascriptURL,
  kNavigateToJavascriptURLAndDefaultPolicyFailed,
  kScriptExecution,
  kScriptExecutionAndDefaultPolicyFailed,
};

// String to determine whether an incoming eval-ish call is comig from
// an actual eval or a Function constructor. The value is derived from
// from how JS builds up a string in the Function constructor, which in
// turn is defined in the TC39 spec.
const char* kAnonymousPrefix = "(function anonymous";

const char kFunctionConstructorFailureConsoleMessage[] =
    "The JavaScript Function constructor does not accept TrustedString "
    "arguments. See https://github.com/w3c/webappsec-trusted-types/wiki/"
    "Trusted-Types-for-function-constructor for more information.";

const char kScriptExecutionTrustedTypeFailConsoleMessage[] =
    "This document requires 'TrustedScript' assignment. "
    "An HTMLScriptElement was directly modified and will not be executed.";

const char* GetMessage(TrustedTypeViolationKind kind) {
  switch (kind) {
    case kTrustedHTMLAssignment:
      return "This document requires 'TrustedHTML' assignment.";
    case kTrustedScriptAssignment:
      return "This document requires 'TrustedScript' assignment.";
    case kTrustedScriptURLAssignment:
      return "This document requires 'TrustedScriptURL' assignment.";
    case kTrustedHTMLAssignmentAndDefaultPolicyFailed:
      return "This document requires 'TrustedHTML' assignment and the "
             "'default' policy failed to execute.";
    case kTrustedHTMLAssignmentAndNoDefaultPolicyExisted:
      return "This document requires 'TrustedHTML' assignment and no "
             "'default' policy for 'TrustedHTML' has been defined.";
    case kTrustedScriptAssignmentAndDefaultPolicyFailed:
      return "This document requires 'TrustedScript' assignment and the "
             "'default' policy failed to execute.";
    case kTrustedScriptAssignmentAndNoDefaultPolicyExisted:
      return "This document requires 'TrustedScript' assignment and no "
             "'default' policy for 'TrustedScript' has been defined.";
    case kTrustedScriptURLAssignmentAndDefaultPolicyFailed:
      return "This document requires 'TrustedScriptURL' assignment and the "
             "'default' policy failed to execute.";
    case kTrustedScriptURLAssignmentAndNoDefaultPolicyExisted:
      return "This document requires 'TrustedScriptURL' assignment and no "
             "'default' policy for 'TrustedScriptURL' has been defined.";
    case kNavigateToJavascriptURL:
      return "This document requires 'TrustedScript' assignment. "
             "Navigating to a javascript:-URL is equivalent to a "
             "'TrustedScript' assignment.";
    case kNavigateToJavascriptURLAndDefaultPolicyFailed:
      return "This document requires 'TrustedScript' assignment. "
             "Navigating to a javascript:-URL is equivalent to a "
             "'TrustedScript' assignment and the 'default' policy failed to"
             "execute.";
    case kScriptExecution:
      return "This document requires 'TrustedScript' assignment. "
             "This script element was modified without use of TrustedScript "
             "assignment.";
    case kScriptExecutionAndDefaultPolicyFailed:
      return "This document requires 'TrustedScript' assignment. "
             "This script element was modified without use of TrustedScript "
             "assignment and the 'default' policy failed to execute.";
  }
  NOTREACHED_IN_MIGRATION();
  return "";
}

String GetSamplePrefix(const char* interface_name,
                       const char* property_name,
                       const String& value) {
  // We have two sample formats, one for eval and one for assignment.
  // If we don't have the required values being passed in, just leave the
  // sample empty.
  StringBuilder sample_prefix;
  if (!interface_name) {
    // No interface name? Then we have no prefix to use.
  } else if (strcmp("eval", interface_name) == 0) {
    // eval? Try to distinguish between eval and Function constructor.
    sample_prefix.Append(value.StartsWith(kAnonymousPrefix) ? "Function"
                                                            : "eval");
  } else if ((strcmp("Worker", interface_name) == 0 ||
              strcmp("SharedWorker", interface_name) == 0) &&
             property_name) {
    // Worker/SharedWorker constructor has nullptr as property_name.
    sample_prefix.Append(interface_name);
    sample_prefix.Append(" constructor");
  } else if (interface_name && property_name) {
    sample_prefix.Append(interface_name);
    sample_prefix.Append(" ");
    sample_prefix.Append(property_name);
  }
  return sample_prefix.ToString();
}

const char* GetElementName(const ScriptElementBase::Type type) {
  switch (type) {
    case ScriptElementBase::Type::kHTMLScriptElement:
      return "HTMLScriptElement";
    case ScriptElementBase::Type::kSVGScriptElement:
      return "SVGScriptElement";
  }
  NOTREACHED_IN_MIGRATION();
  return "";
}

HeapVector<ScriptValue> GetDefaultCallbackArgs(
    v8::Isolate* isolate,
    const char* type,
    const char* interface_name,
    const char* property_name,
    const String& value = g_empty_string) {
  HeapVector<ScriptValue> args;
  args.push_back(ScriptValue(isolate, V8String(isolate, type)));
  args.push_back(ScriptValue(
      isolate, V8String(isolate, GetSamplePrefix(interface_name, property_name,
                                                 value))));
  return args;
}

// Handle failure of a Trusted Type assignment.
//
// If trusted type assignment fails, we need to
// - report the violation via CSP
// - increment the appropriate counter,
// - raise a JavaScript exception (if enforced).
//
// Returns whether the failure should be enforced.
bool TrustedTypeFail(TrustedTypeViolationKind kind,
                     const ExecutionContext* execution_context,
                     const char* interface_name,
                     const char* property_name,
                     ExceptionState& exception_state,
                     const String& value) {
  if (!execution_context)
    return true;

  // Test case docs (Document::CreateForTest()) might not have a window
  // and hence no TrustedTypesPolicyFactory.
  if (execution_context->GetTrustedTypes())
    execution_context->GetTrustedTypes()->CountTrustedTypeAssignmentError();

  String prefix = GetSamplePrefix(interface_name, property_name, value);
  // This issue_id is used to generate a link in the DevTools front-end from
  // the JavaScript TypeError to the inspector issue which is reported by
  // ContentSecurityPolicy::ReportViolation via the call to
  // AllowTrustedTypeAssignmentFailure below.
  base::UnguessableToken issue_id = base::UnguessableToken::Create();
  bool allow =
      execution_context->GetContentSecurityPolicy()
          ->AllowTrustedTypeAssignmentFailure(
              GetMessage(kind),
              prefix == "Function" ? value.Substring(static_cast<wtf_size_t>(
                                         strlen(kAnonymousPrefix)))
                                   : value,
              prefix, issue_id);

  // TODO(1087743): Add a console message for Trusted Type-related Function
  // constructor failures, to warn the developer of the outstanding issues
  // with TT and Function  constructors. This should be removed once the
  // underlying issue has been fixed.
  if (prefix == "Function" && !allow &&
      !RuntimeEnabledFeatures::TrustedTypesUseCodeLikeEnabled()) {
    DCHECK(kind == kTrustedScriptAssignment ||
           kind == kTrustedScriptAssignmentAndDefaultPolicyFailed ||
           kind == kTrustedScriptAssignmentAndNoDefaultPolicyExisted);
    execution_context->GetContentSecurityPolicy()->LogToConsole(
        MakeGarbageCollected<ConsoleMessage>(
            mojom::blink::ConsoleMessageSource::kRecommendation,
            mojom::blink::ConsoleMessageLevel::kInfo,
            kFunctionConstructorFailureConsoleMessage));
  }
  probe::OnContentSecurityPolicyViolation(
      const_cast<ExecutionContext*>(execution_context),
      ContentSecurityPolicyViolationType::kTrustedTypesSinkViolation);

  if (!allow) {
    v8::Isolate* isolate = execution_context->GetIsolate();
    TryRethrowScope rethrow_scope(isolate, exception_state);
    auto exception =
        V8ThrowException::CreateTypeError(isolate, GetMessage(kind));
    MaybeAssociateExceptionMetaData(exception, "issueId",
                                    IdentifiersFactory::IdFromToken(issue_id));
    V8ThrowException::ThrowException(isolate, exception);
  }
  return !allow;
}

TrustedTypePolicy* GetDefaultPolicy(const ExecutionContext* execution_context) {
  DCHECK(execution_context);
  return execution_context->GetTrustedTypes()
             ? execution_context->GetTrustedTypes()->defaultPolicy()
             : nullptr;
}

// Functionally identical to TrustedTypesCheckForScript(const String&, ..), but
// to be called outside of regular script execution. This is required for both
// GetStringForScriptExecution & TrustedTypesCheckForJavascriptURLinNavigation,
// and has a number of additional parameters to enable proper error reporting
// for each case.
String GetStringFromScriptHelper(
    const String& script,
    ExecutionContext* context,
    // Parameters to customize error messages:
    const char* interface_name,
    const char* property_name,
    TrustedTypeViolationKind violation_kind,
    TrustedTypeViolationKind violation_kind_when_default_policy_failed) {
  if (!context)
    return script;
  if (!RequireTrustedTypesCheck(context))
    return script;

  // Set up JS context & friends.
  //
  // All other functions in here are expected to be called during JS execution,
  // where naturally everything is properly set up for more JS execution.
  // This one is called during navigation, and thus needs to do a bit more
  // work. We need two JavaScript-ish things:
  // - TrustedTypeFail expects an ExceptionState, which it will use to throw
  //   an exception. In our case, we will always clear the exception (as there
  //   is no user script to pass it to), and we only use this as a signalling
  //   mechanism.
  // - If the default policy applies, we need to execute the JS callback.
  //   Unlike the various ScriptController::Execute* and ..::Eval* methods,
  //   we are not executing a source String, but an already compiled callback
  //   function.
  v8::HandleScope handle_scope(context->GetIsolate());
  ScriptState::Scope script_state_scope(ToScriptStateForMainWorld(context));
  DummyExceptionStateForTesting exception_state;

  TrustedTypePolicy* default_policy = GetDefaultPolicy(context);
  if (!default_policy) {
    if (TrustedTypeFail(violation_kind, context, interface_name, property_name,
                        exception_state, script)) {
      return String();
    }
    return script;
  }

  TrustedScript* result = default_policy->CreateScript(
      context->GetIsolate(), script,
      GetDefaultCallbackArgs(context->GetIsolate(), "TrustedScript",
                             interface_name, property_name, script),
      exception_state);
  if (!result) {
    return String();
  }

  if (result->toString().IsNull()) {
    if (TrustedTypeFail(violation_kind_when_default_policy_failed, context,
                        interface_name, property_name, exception_state,
                        script)) {
      return String();
    }
    return script;
  }
  return result->toString();
}

}  // namespace

bool RequireTrustedTypesCheck(const ExecutionContext* execution_context) {
  return execution_context && execution_context->RequireTrustedTypes() &&
         !ContentSecurityPolicy::ShouldBypassMainWorldDeprecated(
             execution_context);
}

String TrustedTypesCheckForHTML(const String& html,
                                const ExecutionContext* execution_context,
                                const char* interface_name,
                                const char* property_name,
                                ExceptionState& exception_state) {
  bool require_trusted_type = RequireTrustedTypesCheck(execution_context);
  if (!require_trusted_type) {
    return html;
  }

  TrustedTypePolicy* default_policy = GetDefaultPolicy(execution_context);
  if (!default_policy) {
    if (TrustedTypeFail(kTrustedHTMLAssignment, execution_context,
                        interface_name, property_name, exception_state, html)) {
      return g_empty_string;
    }
    return html;
  }

  if (!default_policy->HasCreateHTML()) {
    if (TrustedTypeFail(kTrustedHTMLAssignmentAndNoDefaultPolicyExisted,
                        execution_context, interface_name, property_name,
                        exception_state, html)) {
      return g_empty_string;
    } else {
      return html;
    }
  }
  // TODO(ajwong): This can be optimized to avoid a AddRef in the
  // StringCache::CreateStringAndInsertIntoCache() also, but it's a hard mess.
  // Punt for now.
  TrustedHTML* result = default_policy->CreateHTML(
      execution_context->GetIsolate(), html,
      GetDefaultCallbackArgs(execution_context->GetIsolate(), "TrustedHTML",
                             interface_name, property_name),
      exception_state);
  if (exception_state.HadException()) {
    return g_empty_string;
  }

  if (result->toString().IsNull()) {
    if (TrustedTypeFail(kTrustedHTMLAssignmentAndDefaultPolicyFailed,
                        execution_context, interface_name, property_name,
                        exception_state, html)) {
      return g_empty_string;
    } else {
      return html;
    }
  }

  return result->toString();
}

String TrustedTypesCheckForScript(const String& script,
                                  const ExecutionContext* execution_context,
                                  const char* interface_name,
                                  const char* property_name,
                                  ExceptionState& exception_state) {
  bool require_trusted_type = RequireTrustedTypesCheck(execution_context);
  if (!require_trusted_type) {
    return script;
  }

  TrustedTypePolicy* default_policy = GetDefaultPolicy(execution_context);
  if (!default_policy) {
    if (TrustedTypeFail(kTrustedScriptAssignment, execution_context,
                        interface_name, property_name, exception_state,
                        script)) {
      return g_empty_string;
    }
    return script;
  }

  if (!default_policy->HasCreateScript()) {
    if (TrustedTypeFail(kTrustedScriptAssignmentAndNoDefaultPolicyExisted,
                        execution_context, interface_name, property_name,
                        exception_state, script)) {
      return g_empty_string;
    } else {
      return script;
    }
  }
  // TODO(ajwong): This can be optimized to avoid a AddRef in the
  // StringCache::CreateStringAndInsertIntoCache() also, but it's a hard mess.
  // Punt for now.
  TrustedScript* result = default_policy->CreateScript(
      execution_context->GetIsolate(), script,
      GetDefaultCallbackArgs(execution_context->GetIsolate(), "TrustedScript",
                             interface_name, property_name, script),
      exception_state);
  DCHECK_EQ(!result, exception_state.HadException());
  if (exception_state.HadException()) {
    return g_empty_string;
  }

  if (result->toString().IsNull()) {
    if (TrustedTypeFail(kTrustedScriptAssignmentAndDefaultPolicyFailed,
                        execution_context, interface_name, property_name,
                        exception_state, script)) {
      return g_empty_string;
    } else {
      return script;
    }
  }

  return result->toString();
}

String TrustedTypesCheckForScriptURL(const String& script_url,
                                     const ExecutionContext* execution_context,
                                     const char* interface_name,
                                     const char* property_name,
                                     ExceptionState& exception_state) {
  bool require_trusted_type = RequireTrustedTypesCheck(execution_context);
  if (!require_trusted_type) {
    return script_url;
  }

  TrustedTypePolicy* default_policy = GetDefaultPolicy(execution_context);
  if (!default_policy) {
    if (TrustedTypeFail(kTrustedScriptURLAssignment, execution_context,
                        interface_name, property_name, exception_state,
                        script_url)) {
      return g_empty_string;
    }
    return script_url;
  }

  if (!default_policy->HasCreateScriptURL()) {
    if (TrustedTypeFail(kTrustedScriptURLAssignmentAndNoDefaultPolicyExisted,
                        execution_context, interface_name, property_name,
                        exception_state, script_url)) {
      return g_empty_string;
    } else {
      return script_url;
    }
  }
  // TODO(ajwong): This can be optimized to avoid a AddRef in the
  // StringCache::CreateStringAndInsertIntoCache() also, but it's a hard mess.
  // Punt for now.
  TrustedScriptURL* result = default_policy->CreateScriptURL(
      execution_context->GetIsolate(), script_url,
      GetDefaultCallbackArgs(execution_context->GetIsolate(),
                             "TrustedScriptURL", interface_name, property_name),
      exception_state);

  if (exception_state.HadException()) {
    return g_empty_string;
  }

  if (result->toString().IsNull()) {
    if (TrustedTypeFail(kTrustedScriptURLAssignmentAndDefaultPolicyFailed,
                        execution_context, interface_name, property_name,
                        exception_state, script_url)) {
      return g_empty_string;
    } else {
      return script_url;
    }
  }

  return result->toString();
}

String TrustedTypesCheckFor(SpecificTrustedType type,
                            const V8TrustedType* trusted,
                            const ExecutionContext* execution_context,
                            const char* interface_name,
                            const char* property_name,
                            ExceptionState& exception_state) {
  DCHECK(trusted);

  // Whatever happens below, we will need the string value:
  String value;
  bool does_type_match = false;
  switch (trusted->GetContentType()) {
    case V8TrustedType::ContentType::kTrustedHTML:
      value = trusted->GetAsTrustedHTML()->toString();
      does_type_match = type == SpecificTrustedType::kHTML;
      break;
    case V8TrustedType::ContentType::kTrustedScript:
      value = trusted->GetAsTrustedScript()->toString();
      does_type_match = type == SpecificTrustedType::kScript;
      break;
    case V8TrustedType::ContentType::kTrustedScriptURL:
      value = trusted->GetAsTrustedScriptURL()->toString();
      does_type_match = type == SpecificTrustedType::kScriptURL;
      break;
  }

  if (type == SpecificTrustedType::kNone || does_type_match)
    return value;

  // In all other cases: run the full check against the string value.
  return TrustedTypesCheckFor(type, std::move(value), execution_context,
                              interface_name, property_name, exception_state);
}

String TrustedTypesCheckForScript(const V8UnionStringOrTrustedScript* value,
                                  const ExecutionContext* execution_context,
                                  const char* interface_name,
                                  const char* property_name,
                                  ExceptionState& exception_state) {
  // To remain compatible with legacy behaviour, HTMLElement uses extended IDL
  // attributes to allow for nullable union of (DOMString or TrustedScript).
  // Thus, this method is required to handle the case where |!value|, unlike
  // the various similar methods in this file.
  if (!value) {
    return TrustedTypesCheckForScript(g_empty_string, execution_context,
                                      interface_name, property_name,
                                      exception_state);
  }

  switch (value->GetContentType()) {
    case V8UnionStringOrTrustedScript::ContentType::kString:
      return TrustedTypesCheckForScript(value->GetAsString(), execution_context,
                                        interface_name, property_name,
                                        exception_state);
    case V8UnionStringOrTrustedScript::ContentType::kTrustedScript:
      return value->GetAsTrustedScript()->toString();
  }

  NOTREACHED_IN_MIGRATION();
  return String();
}

String TrustedTypesCheckForScript(
    const V8UnionStringLegacyNullToEmptyStringOrTrustedScript* value,
    const ExecutionContext* execution_context,
    const char* interface_name,
    const char* property_name,
    ExceptionState& exception_state) {
  // To remain compatible with legacy behaviour, HTMLElement uses extended IDL
  // attributes to allow for nullable union of (DOMString or TrustedScript).
  // Thus, this method is required to handle the case where |!value|, unlike
  // the various similar methods in this file.
  if (!value) {
    return TrustedTypesCheckForScript(g_empty_string, execution_context,
                                      interface_name, property_name,
                                      exception_state);
  }

  switch (value->GetContentType()) {
    case V8UnionStringLegacyNullToEmptyStringOrTrustedScript::ContentType::
        kStringLegacyNullToEmptyString:
      return TrustedTypesCheckForScript(
          value->GetAsStringLegacyNullToEmptyString(), execution_context,
          interface_name, property_name, exception_state);
    case V8UnionStringLegacyNullToEmptyStringOrTrustedScript::ContentType::
        kTrustedScript:
      return value->GetAsTrustedScript()->toString();
  }

  NOTREACHED_IN_MIGRATION();
  return String();
}

String TrustedTypesCheckFor(SpecificTrustedType type,
                            String trusted,
                            const ExecutionContext* execution_context,
                            const char* interface_name,
                            const char* property_name,
                            ExceptionState& exception_state) {
  switch (type) {
    case SpecificTrustedType::kHTML:
      return TrustedTypesCheckForHTML(std::move(trusted), execution_context,
                                      interface_name, property_name,
                                      exception_state);
    case SpecificTrustedType::kScript:
      return TrustedTypesCheckForScript(std::move(trusted), execution_context,
                                        interface_name, property_name,
                                        exception_state);
    case SpecificTrustedType::kScriptURL:
      return TrustedTypesCheckForScriptURL(std::move(trusted),
                                           execution_context, interface_name,
                                           property_name, exception_state);
    case SpecificTrustedType::kNone:
      return trusted;
  }
  NOTREACHED_IN_MIGRATION();
  return g_empty_string;
}

String CORE_EXPORT
GetStringForScriptExecution(const String& script,
                            const ScriptElementBase::Type type,
                            ExecutionContext* context) {
  String value = GetStringFromScriptHelper(
      script, context, GetElementName(type), "text", kScriptExecution,
      kScriptExecutionAndDefaultPolicyFailed);
  if (!script.IsNull() && value.IsNull()) {
    context->AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
        mojom::blink::ConsoleMessageSource::kSecurity,
        mojom::blink::ConsoleMessageLevel::kError,
        kScriptExecutionTrustedTypeFailConsoleMessage));
  }
  return value;
}

String TrustedTypesCheckForJavascriptURLinNavigation(
    const String& javascript_url,
    ExecutionContext* context) {
  return GetStringFromScriptHelper(
      std::move(javascript_url), context, "Location", "href",
      kNavigateToJavascriptURL, kNavigateToJavascriptURLAndDefaultPolicyFailed);
}

String TrustedTypesCheckForExecCommand(
    const String& html,
    const ExecutionContext* execution_context,
    ExceptionState& exception_state) {
  return TrustedTypesCheckForHTML(html, execution_context, "Document",
                                  "execCommand", exception_state);
}

bool IsTrustedTypesEventHandlerAttribute(const QualifiedName& q_name) {
  return q_name.NamespaceURI().IsNull() &&
         TrustedTypePolicyFactory::IsEventHandlerAttributeName(
             q_name.LocalName());
}

String GetTrustedTypesLiteral(const ScriptValue& script_value,
                              ScriptState* script_state) {
  DCHECK(script_state);
  // TrustedTypes fromLiteral requires several checks, which are steps 1-3
  // in the "create a trusted type from literal algorithm". Ref:
  // https://w3c.github.io/trusted-types/dist/spec/#create-a-trusted-type-from-literal-algorithm

  // The core functionality here are the checks that we, indeed, have a
  // literal object. The key work is done by
  // v8::Context::HasTemplateLiteralObject, but we will additionally check that
  // we have an object, with a real (non-inherited) property 0 (but not 1),
  // whose value is a string.
  v8::Local<v8::Context> context = script_state->GetContext();
  v8::Local<v8::Value> value = script_value.V8ValueFor(script_state);
  if (!context.IsEmpty() && !value.IsEmpty() &&
      context->HasTemplateLiteralObject(value) && value->IsObject()) {
    v8::Local<v8::Object> value_as_object = v8::Local<v8::Object>::Cast(value);
    v8::Local<v8::Value> first_value;
    if (value_as_object->HasRealIndexedProperty(context, 0).FromMaybe(false) &&
        !value_as_object->HasRealIndexedProperty(context, 1).FromMaybe(false) &&
        value_as_object->Get(context, 0).ToLocal(&first_value) &&
        first_value->IsString()) {
      v8::Local<v8::String> first_value_as_string =
          v8::Local<v8::String>::Cast(first_value);
      return ToCoreString(script_state->GetIsolate(), first_value_as_string);
    }
  }

  // Fall-through: Some of the required conditions didn't hold. Return a
  // null-string.
  return String();
}

}  // namespace blink
