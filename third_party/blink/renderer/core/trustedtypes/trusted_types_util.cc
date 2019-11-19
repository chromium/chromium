// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/trustedtypes/trusted_types_util.h"

#include "third_party/blink/public/mojom/reporting/reporting.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/bindings/core/v8/string_or_trusted_html.h"
#include "third_party/blink/renderer/bindings/core/v8/string_or_trusted_html_or_trusted_script_or_trusted_script_url.h"
#include "third_party/blink/renderer/bindings/core/v8/string_or_trusted_script.h"
#include "third_party/blink/renderer/bindings/core/v8/string_or_trusted_script_url.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/window_proxy_manager.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/trustedtypes/trusted_html.h"
#include "third_party/blink/renderer/core/trustedtypes/trusted_script.h"
#include "third_party/blink/renderer/core/trustedtypes/trusted_script_url.h"
#include "third_party/blink/renderer/core/trustedtypes/trusted_type_policy.h"
#include "third_party/blink/renderer/core/trustedtypes/trusted_type_policy_factory.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

namespace {

// This value is derived from the Trusted Types spec (draft), and determines the
// maximum length of the sample value in the violation reports.
const unsigned kReportedValueMaximumLength = 40;

enum TrustedTypeViolationKind {
  kAnyTrustedTypeAssignment,
  kTrustedHTMLAssignment,
  kTrustedScriptAssignment,
  kTrustedScriptURLAssignment,
  kTrustedHTMLAssignmentAndDefaultPolicyFailed,
  kTrustedScriptAssignmentAndDefaultPolicyFailed,
  kTrustedScriptURLAssignmentAndDefaultPolicyFailed,
  kTextNodeScriptAssignment,
  kTextNodeScriptAssignmentAndDefaultPolicyFailed,
  kNavigateToJavascriptURL,
  kNavigateToJavascriptURLAndDefaultPolicyFailed,
};

const char* GetMessage(TrustedTypeViolationKind kind) {
  switch (kind) {
    case kAnyTrustedTypeAssignment:
      return "This document requires any trusted type assignment.";
    case kTrustedHTMLAssignment:
      return "This document requires 'TrustedHTML' assignment.";
    case kTrustedScriptAssignment:
      return "This document requires 'TrustedScript' assignment.";
    case kTrustedScriptURLAssignment:
      return "This document requires 'TrustedScriptURL' assignment.";
    case kTrustedHTMLAssignmentAndDefaultPolicyFailed:
      return "This document requires 'TrustedHTML' assignment and the "
             "'default' policy failed to execute.";
    case kTrustedScriptAssignmentAndDefaultPolicyFailed:
      return "This document requires 'TrustedScript' assignment and the "
             "'default' policy failed to execute.";
    case kTrustedScriptURLAssignmentAndDefaultPolicyFailed:
      return "This document requires 'TrustedScriptURL' assignment and the "
             "'default' policy failed to execute.";
    case kTextNodeScriptAssignment:
      return "This document requires 'TrustedScript' assignment, "
             "and inserting a text node into a script element is equivalent to "
             "a 'TrustedScript' assignment.";
    case kTextNodeScriptAssignmentAndDefaultPolicyFailed:
      return "This document requires 'TrustedScript' assignment. "
             "Inserting a text node into a script element is equivalent to "
             "a 'TrustedScript' assignment and the default policy failed to "
             "execute.";
    case kNavigateToJavascriptURL:
      return "This document requires 'TrustedScript' assignment. "
             "Navigating to a javascript:-URL is equivalent to a "
             "'TrustedScript' assignment.";
    case kNavigateToJavascriptURLAndDefaultPolicyFailed:
      return "This document requires 'TrustedScript' assignment. "
             "Navigating to a javascript:-URL is equivalent to a "
             "'TrustedScript' assignment and the default policy failed to"
             "execute.";
  }
  NOTREACHED();
  return "";
}

std::pair<String, String> GetMessageAndSample(
    TrustedTypeViolationKind kind,
    const ExceptionState& exception_state,
    const String& value) {
  const char* interface_name = exception_state.InterfaceName();
  const char* property_name = exception_state.PropertyName();

  // We have two sample formats, one for eval and one for assignment.
  // If we don't have the required values being passed in, just leave the
  // sample empty.
  StringBuilder sample;
  if (interface_name && strcmp("eval", interface_name) == 0) {
    sample.Append("eval");
  } else if (interface_name && property_name) {
    sample.Append(interface_name);
    sample.Append(".");
    sample.Append(property_name);
  }
  if (!sample.IsEmpty()) {
    sample.Append(" ");
    sample.Append(value.Left(kReportedValueMaximumLength));
  }
  return std::make_pair<String, String>(GetMessage(kind), sample.ToString());
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
                     ExceptionState& exception_state,
                     const String& value) {
  if (!execution_context)
    return true;

  // Test case docs (MakeGarbageCollected<Document>()) might not have a window
  // and hence no TrustedTypesPolicyFactory.
  if (execution_context->GetTrustedTypes())
    execution_context->GetTrustedTypes()->CountTrustedTypeAssignmentError();

  String message;
  String sample;
  std::tie(message, sample) = GetMessageAndSample(kind, exception_state, value);
  bool allow = execution_context->GetSecurityContext()
                   .GetContentSecurityPolicy()
                   ->AllowTrustedTypeAssignmentFailure(message, sample);
  if (!allow) {
    exception_state.ThrowTypeError(message);
  }
  return !allow;
}

TrustedTypePolicy* GetDefaultPolicy(const ExecutionContext* execution_context) {
  DCHECK(execution_context);
  return execution_context->GetTrustedTypes()
             ? execution_context->GetTrustedTypes()->defaultPolicy()
             : nullptr;
}

}  // namespace

bool RequireTrustedTypesCheck(const ExecutionContext* execution_context) {
  return execution_context && execution_context->RequireTrustedTypes() &&
         !ContentSecurityPolicy::ShouldBypassMainWorld(execution_context);
}

String GetStringFromTrustedType(
    const StringOrTrustedHTMLOrTrustedScriptOrTrustedScriptURL&
        string_or_trusted_type,
    const ExecutionContext* execution_context,
    ExceptionState& exception_state) {
  DCHECK(!string_or_trusted_type.IsNull());

  if (string_or_trusted_type.IsString() &&
      RequireTrustedTypesCheck(execution_context)) {
    TrustedTypeFail(
        kAnyTrustedTypeAssignment, execution_context, exception_state,
        GetStringFromTrustedTypeWithoutCheck(string_or_trusted_type));
    return g_empty_string;
  }

  if (string_or_trusted_type.IsTrustedHTML())
    return string_or_trusted_type.GetAsTrustedHTML()->toString();
  if (string_or_trusted_type.IsTrustedScript())
    return string_or_trusted_type.GetAsTrustedScript()->toString();
  if (string_or_trusted_type.IsTrustedScriptURL())
    return string_or_trusted_type.GetAsTrustedScriptURL()->toString();

  return string_or_trusted_type.GetAsString();
}

String GetStringFromTrustedTypeWithoutCheck(
    const StringOrTrustedHTMLOrTrustedScriptOrTrustedScriptURL&
        string_or_trusted_type) {
  if (string_or_trusted_type.IsTrustedHTML())
    return string_or_trusted_type.GetAsTrustedHTML()->toString();
  if (string_or_trusted_type.IsTrustedScript())
    return string_or_trusted_type.GetAsTrustedScript()->toString();
  if (string_or_trusted_type.IsTrustedScriptURL())
    return string_or_trusted_type.GetAsTrustedScriptURL()->toString();
  if (string_or_trusted_type.IsString())
    return string_or_trusted_type.GetAsString();

  return g_empty_string;
}

String GetStringFromSpecificTrustedType(
    const StringOrTrustedHTMLOrTrustedScriptOrTrustedScriptURL&
        string_or_trusted_type,
    SpecificTrustedType specific_trusted_type,
    const ExecutionContext* execution_context,
    ExceptionState& exception_state) {
  switch (specific_trusted_type) {
    case SpecificTrustedType::kNone:
      return GetStringFromTrustedTypeWithoutCheck(string_or_trusted_type);
    case SpecificTrustedType::kTrustedHTML: {
      StringOrTrustedHTML string_or_trusted_html =
          string_or_trusted_type.IsTrustedHTML()
              ? StringOrTrustedHTML::FromTrustedHTML(
                    string_or_trusted_type.GetAsTrustedHTML())
              : StringOrTrustedHTML::FromString(
                    GetStringFromTrustedTypeWithoutCheck(
                        string_or_trusted_type));
      return GetStringFromTrustedHTML(string_or_trusted_html, execution_context,
                                      exception_state);
    }
    case SpecificTrustedType::kTrustedScript: {
      StringOrTrustedScript string_or_trusted_script =
          string_or_trusted_type.IsTrustedScript()
              ? StringOrTrustedScript::FromTrustedScript(
                    string_or_trusted_type.GetAsTrustedScript())
              : StringOrTrustedScript::FromString(
                    GetStringFromTrustedTypeWithoutCheck(
                        string_or_trusted_type));
      return GetStringFromTrustedScript(string_or_trusted_script,
                                        execution_context, exception_state);
    }
    case SpecificTrustedType::kTrustedScriptURL: {
      StringOrTrustedScriptURL string_or_trusted_script_url =
          string_or_trusted_type.IsTrustedScriptURL()
              ? StringOrTrustedScriptURL::FromTrustedScriptURL(
                    string_or_trusted_type.GetAsTrustedScriptURL())
              : StringOrTrustedScriptURL::FromString(
                    GetStringFromTrustedTypeWithoutCheck(
                        string_or_trusted_type));
      return GetStringFromTrustedScriptURL(string_or_trusted_script_url,
                                           execution_context, exception_state);
    }
  }
}

String GetStringFromSpecificTrustedType(
    const String& string,
    SpecificTrustedType specific_trusted_type,
    const ExecutionContext* execution_context,
    ExceptionState& exception_state) {
  if (specific_trusted_type == SpecificTrustedType::kNone)
    return string;
  return GetStringFromSpecificTrustedType(
      StringOrTrustedHTMLOrTrustedScriptOrTrustedScriptURL::FromString(string),
      specific_trusted_type, execution_context, exception_state);
}

String GetStringFromTrustedHTML(StringOrTrustedHTML string_or_trusted_html,
                                const ExecutionContext* execution_context,
                                ExceptionState& exception_state) {
  DCHECK(!string_or_trusted_html.IsNull());

  if (string_or_trusted_html.IsTrustedHTML()) {
    return string_or_trusted_html.GetAsTrustedHTML()->toString();
  }

  return GetStringFromTrustedHTML(string_or_trusted_html.GetAsString(),
                                  execution_context, exception_state);
}

String GetStringFromTrustedHTML(const String& string,
                                const ExecutionContext* execution_context,
                                ExceptionState& exception_state) {
  bool require_trusted_type = RequireTrustedTypesCheck(execution_context);
  if (!require_trusted_type) {
    return string;
  }

  TrustedTypePolicy* default_policy = GetDefaultPolicy(execution_context);
  if (!default_policy) {
    if (TrustedTypeFail(kTrustedHTMLAssignment, execution_context,
                        exception_state, string)) {
      return g_empty_string;
    }
    return string;
  }

  TrustedHTML* result = default_policy->CreateHTML(
      execution_context->GetIsolate(), string, exception_state);
  if (exception_state.HadException()) {
    return g_empty_string;
  }

  if (result->toString().IsNull()) {
    if (TrustedTypeFail(kTrustedHTMLAssignmentAndDefaultPolicyFailed,
                        execution_context, exception_state, string)) {
      return g_empty_string;
    } else {
      return string;
    }
  }

  return result->toString();
}

String GetStringFromTrustedScript(
    StringOrTrustedScript string_or_trusted_script,
    const ExecutionContext* execution_context,
    ExceptionState& exception_state) {
  // To remain compatible with legacy behaviour, HTMLElement uses extended IDL
  // attributes to allow for nullable union of (DOMString or TrustedScript).
  // Thus, this method is required to handle the case where
  // string_or_trusted_script.IsNull(), unlike the various similar methods in
  // this file.

  if (string_or_trusted_script.IsTrustedScript()) {
    return string_or_trusted_script.GetAsTrustedScript()->toString();
  }

  if (string_or_trusted_script.IsNull()) {
    string_or_trusted_script =
        StringOrTrustedScript::FromString(g_empty_string);
  }
  return GetStringFromTrustedScript(string_or_trusted_script.GetAsString(),
                                    execution_context, exception_state);
}

String GetStringFromTrustedScript(const String& potential_script,
                                  const ExecutionContext* execution_context,
                                  ExceptionState& exception_state) {
  bool require_trusted_type = RequireTrustedTypesCheck(execution_context);
  if (!require_trusted_type) {
    return potential_script;
  }

  TrustedTypePolicy* default_policy = GetDefaultPolicy(execution_context);
  if (!default_policy) {
    if (TrustedTypeFail(kTrustedScriptAssignment, execution_context,
                        exception_state, potential_script)) {
      return g_empty_string;
    }
    return potential_script;
  }

  TrustedScript* result = default_policy->CreateScript(
      execution_context->GetIsolate(), potential_script, exception_state);
  DCHECK_EQ(!result, exception_state.HadException());
  if (exception_state.HadException()) {
    return g_empty_string;
  }

  if (result->toString().IsNull()) {
    if (TrustedTypeFail(kTrustedScriptAssignmentAndDefaultPolicyFailed,
                        execution_context, exception_state, potential_script)) {
      return g_empty_string;
    } else {
      return potential_script;
    }
  }

  return result->toString();
}

String GetStringFromTrustedScriptURL(
    StringOrTrustedScriptURL string_or_trusted_script_url,
    const ExecutionContext* execution_context,
    ExceptionState& exception_state) {
  DCHECK(!string_or_trusted_script_url.IsNull());
  if (string_or_trusted_script_url.IsTrustedScriptURL()) {
    return string_or_trusted_script_url.GetAsTrustedScriptURL()->toString();
  }

  DCHECK(string_or_trusted_script_url.IsString());
  String string = string_or_trusted_script_url.GetAsString();

  bool require_trusted_type =
      RequireTrustedTypesCheck(execution_context) &&
      RuntimeEnabledFeatures::TrustedDOMTypesEnabled(execution_context);
  if (!require_trusted_type) {
    return string;
  }

  TrustedTypePolicy* default_policy = GetDefaultPolicy(execution_context);
  if (!default_policy) {
    if (TrustedTypeFail(kTrustedScriptURLAssignment, execution_context,
                        exception_state, string)) {
      return g_empty_string;
    }
    return string;
  }

  TrustedScriptURL* result = default_policy->CreateScriptURL(
      execution_context->GetIsolate(), string, exception_state);

  if (exception_state.HadException()) {
    return g_empty_string;
  }

  if (result->toString().IsNull()) {
    if (TrustedTypeFail(kTrustedScriptURLAssignmentAndDefaultPolicyFailed,
                        execution_context, exception_state, string)) {
      return g_empty_string;
    } else {
      return string;
    }
  }

  return result->toString();
}

Node* TrustedTypesCheckForHTMLScriptElement(Node* child,
                                            Document* doc,
                                            ExceptionState& exception_state) {
  bool require_trusted_type = RequireTrustedTypesCheck(doc);
  if (!require_trusted_type)
    return child;

  TrustedTypePolicy* default_policy = GetDefaultPolicy(doc);
  if (!default_policy) {
    return TrustedTypeFail(kTextNodeScriptAssignment, doc, exception_state,
                           child->textContent())
               ? nullptr
               : child;
  }

  TrustedScript* result = default_policy->CreateScript(
      doc->GetIsolate(), child->textContent(), exception_state);
  if (exception_state.HadException()) {
    return nullptr;
  }

  if (result->toString().IsNull()) {
    return TrustedTypeFail(kTextNodeScriptAssignmentAndDefaultPolicyFailed, doc,
                           exception_state, child->textContent())
               ? nullptr
               : child;
  }

  return Text::Create(*doc, result->toString());
}

String TrustedTypesCheckForJavascriptURLinNavigation(
    const String& javascript_url,
    Document* doc) {
  bool require_trusted_type = RequireTrustedTypesCheck(doc);
  if (!require_trusted_type)
    return javascript_url;

  // Set up JS context & friends.
  //
  // All other functions in here are expected to be called during JS execution,
  // where naturally everything is propertly set up for more JS execution.
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
  v8::HandleScope handle_scope(doc->GetIsolate());
  ScriptState::Scope script_state_scope(
      ScriptState::From(static_cast<LocalWindowProxyManager*>(
                            doc->GetFrame()->GetWindowProxyManager())
                            ->MainWorldProxy()
                            ->ContextIfInitialized()));
  ExceptionState exception_state(
      doc->GetIsolate(), ExceptionState::kUnknownContext, "Location", "href");

  TrustedTypePolicy* default_policy = GetDefaultPolicy(doc);
  if (!default_policy) {
    if (TrustedTypeFail(kNavigateToJavascriptURL, doc, exception_state,
                        javascript_url)) {
      exception_state.ClearException();
      return String();
    }
    return javascript_url;
  }

  TrustedScript* result = default_policy->CreateScript(
      doc->GetIsolate(), javascript_url, exception_state);
  if (exception_state.HadException()) {
    exception_state.ClearException();
    return String();
  }

  if (result->toString().IsNull()) {
    if (TrustedTypeFail(kNavigateToJavascriptURLAndDefaultPolicyFailed, doc,
                        exception_state, javascript_url)) {
      exception_state.ClearException();
      return String();
    }
    return javascript_url;
  }
  // TODO(vogelheim): Figure out whether we need to check whether this string
  // parses as a URL, and whether we need to do this here.
  return result->toString();
}

}  // namespace blink
