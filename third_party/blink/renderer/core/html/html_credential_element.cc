// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/html_credential_element.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/platform/json/json_parser.h"
#include "third_party/blink/renderer/platform/json/json_values.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/wtf/text/string_concatenate.h"

namespace blink {

HTMLCredentialElement::HTMLCredentialElement(Document& document)
    : HTMLElement(html_names::kCredentialTag, document) {}

bool HTMLCredentialElement::IsURLAttribute(const Attribute& attribute) const {
  return attribute.GetName() == html_names::kConfigurlAttr ||
         HTMLElement::IsURLAttribute(attribute);
}

void HTMLCredentialElement::ParseAttribute(
    const AttributeModificationParams& params) {
  if (params.name == html_names::kParamsAttr && !params.new_value.IsNull()) {
    JSONParseError parse_error;
    if (!ParseJSON(params.new_value, &parse_error)) {
      GetExecutionContext()->AddConsoleMessage(
          MakeGarbageCollected<ConsoleMessage>(
              mojom::blink::ConsoleMessageSource::kOther,
              mojom::blink::ConsoleMessageLevel::kError,
              StrCat({"credential params attribute was invalid JSON: ",
                      parse_error.message, " (line ",
                      String::Number(parse_error.line), ", col ",
                      String::Number(parse_error.column), ")"})));
    }
  } else if (params.name == html_names::kConfigurlAttr &&
             !params.new_value.IsNull()) {
    KURL config_url = GetDocument().CompleteURL(params.new_value);
    if (params.new_value.empty() || !config_url.IsValid()) {
      GetExecutionContext()->AddConsoleMessage(
          MakeGarbageCollected<ConsoleMessage>(
              mojom::blink::ConsoleMessageSource::kJavaScript,
              mojom::blink::ConsoleMessageLevel::kError,
              StrCat({"credential configurl attribute was an invalid URL: ",
                      params.new_value})));
    }
  }
  HTMLElement::ParseAttribute(params);
}

mojom::blink::IdentityProviderRequestOptionsPtr
HTMLCredentialElement::GetFederatedRequestOptions() const {
  String type = FastGetAttribute(html_names::kTypeAttr);
  if (type != "federated") {
    return nullptr;
  }

  KURL config_url = GetNonEmptyURLAttribute(html_names::kConfigurlAttr);
  if (config_url.IsEmpty() || !config_url.IsValid()) {
    return nullptr;
  }

  if (!GetExecutionContext()->GetContentSecurityPolicy()->AllowConnectToSource(
          config_url, config_url, ResourceRequest::RedirectStatus::kNoRedirect,
          ReportingDisposition::kSuppressReporting,
          ContentSecurityPolicy::CheckHeaderType::kCheckAll)) {
    GetExecutionContext()->AddConsoleMessage(
        MakeGarbageCollected<ConsoleMessage>(
            mojom::blink::ConsoleMessageSource::kJavaScript,
            mojom::blink::ConsoleMessageLevel::kError,
            StrCat({"Refused to connect to '", config_url.ElidedString(),
                    "' because it violates the document's Content Security "
                    "Policy."})));
    return nullptr;
  }

  auto options = mojom::blink::IdentityProviderRequestOptions::New();
  options->config = mojom::blink::IdentityProviderConfig::New();
  options->config->config_url = std::move(config_url);
  options->config->client_id = FastGetAttribute(html_names::kClientidAttr);
  if (options->config->client_id.IsNull()) {
    options->config->client_id = g_empty_string;
  }

  // Initialize non-nullable fields to satisfy mojom validation.
  options->nonce = g_empty_string;
  options->login_hint = FastGetAttribute(html_names::kLoginhintAttr);
  options->domain_hint = FastGetAttribute(html_names::kDomainhintAttr);
  if (options->login_hint.IsNull()) {
    options->login_hint = g_empty_string;
  }
  if (options->domain_hint.IsNull()) {
    options->domain_hint = g_empty_string;
  }

  String fields_attr = FastGetAttribute(html_names::kFieldsAttr);
  if (!fields_attr.IsNull()) {
    options->fields = fields_attr.Split(',');
  }

  String params_attr = FastGetAttribute(html_names::kParamsAttr);
  if (!params_attr.IsNull() && ParseJSON(params_attr, nullptr)) {
    options->params_json = params_attr;
  }

  return options;
}

}  // namespace blink
