// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/declarative/declarative_api.h"

#include <stddef.h>

#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "base/task/single_thread_task_runner.h"
#include "base/values.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/api/declarative/rules_registry_service.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/api/events.h"
#include "extensions/common/extension_api.h"
#include "extensions/common/permissions/permissions_data.h"

#if BUILDFLAG(ENABLE_GUEST_VIEW)
#include "extensions/browser/guest_view/web_view/web_view_constants.h"
#include "extensions/browser/guest_view/web_view/web_view_guest.h"
#endif

using extensions::api::events::Rule;

namespace AddRules = extensions::api::events::Event::AddRules;
namespace GetRules = extensions::api::events::Event::GetRules;
namespace RemoveRules = extensions::api::events::Event::RemoveRules;

namespace extensions {

namespace {

#if BUILDFLAG(ENABLE_GUEST_VIEW)
constexpr char kDeclarativeEventPrefix[] = "declarative";
#endif

constexpr char kDeclarativeContentEventPrefix[] = "declarativeContent.";
constexpr char kDeclarativeWebRequestEventPrefix[] = "declarativeWebRequest.";
constexpr char kDeclarativeWebRequestWebViewEventPrefix[] =
    "webViewInternal.declarativeWebRequest.";

// The type of Declarative API. To collect more granular metrics, a distinction
// is made when the declarative web request API is used from a webview.
enum class DeclarativeAPIType {
  kContent,
  kWebRequest,
  kWebRequestWebview,
  kUnknown,
};

// Describes the possible types of declarative API function calls.
// These values are recorded as UMA. New enum values can be added, but existing
// enum values must never be renumbered or deleted and reused.
enum DeclarativeAPIFunctionType {
  kDeclarativeContentAddRules = 0,
  kDeclarativeContentRemoveRules = 1,
  kDeclarativeContentGetRules = 2,
  kDeclarativeWebRequestAddRules = 3,
  kDeclarativeWebRequestRemoveRules = 4,
  kDeclarativeWebRequestGetRules = 5,
  kDeclarativeWebRequestWebviewAddRules = 6,
  kDeclarativeWebRequestWebviewRemoveRules = 7,
  kDeclarativeWebRequestWebviewGetRules = 8,
  kDeclarativeApiFunctionCallTypeMax,
};

DeclarativeAPIType GetDeclarativeAPIType(const std::string& event_name) {
  if (base::StartsWith(event_name, kDeclarativeContentEventPrefix,
                       base::CompareCase::SENSITIVE))
    return DeclarativeAPIType::kContent;
  if (base::StartsWith(event_name, kDeclarativeWebRequestEventPrefix,
                       base::CompareCase::SENSITIVE))
    return DeclarativeAPIType::kWebRequest;
  if (base::StartsWith(event_name, kDeclarativeWebRequestWebViewEventPrefix,
                       base::CompareCase::SENSITIVE))
    return DeclarativeAPIType::kWebRequestWebview;
  return DeclarativeAPIType::kUnknown;
}

void RecordUMAHelper(DeclarativeAPIFunctionType type) {
  DCHECK_LT(type, kDeclarativeApiFunctionCallTypeMax);
  UMA_HISTOGRAM_ENUMERATION("Extensions.DeclarativeAPIFunctionCalls", type,
                            kDeclarativeApiFunctionCallTypeMax);
}

void ConvertBinaryDictValuesToBase64(base::Value::Dict& dict);

// Encodes |binary| as base64 and returns a new string value populated with the
// encoded string.
base::Value ConvertBinaryToBase64(const base::Value& binary) {
  return base::Value(base::Base64Encode(binary.GetBlob()));
}

// Parses through |args| replacing any binary values with base64 encoded
// string values. Recurses over any nested List values, and calls
// ConvertBinaryDictValuesToBase64 for any nested Dict values.
void ConvertBinaryListElementsToBase64(base::Value::List& args) {
  for (auto& value : args) {
    if (value.is_blob()) {
      value = ConvertBinaryToBase64(value);
    } else if (value.is_list() && !value.GetList().empty()) {
      ConvertBinaryListElementsToBase64(value.GetList());
    } else if (value.is_dict()) {
      ConvertBinaryDictValuesToBase64(value.GetDict());
    }
  }
}

// Parses through |dict| replacing any BinaryValues with base64 encoded
// string values. Recurses over any nested Dict values, and calls
// ConvertBinaryListElementsToBase64 for any nested List values.
void ConvertBinaryDictValuesToBase64(base::Value::Dict& dict) {
  for (auto it : dict) {
    auto& value = it.second;
    if (value.is_blob()) {
      value = ConvertBinaryToBase64(value);
    } else if (value.is_list() && !value.GetList().empty()) {
      ConvertBinaryListElementsToBase64(value.GetList());
    } else if (value.is_dict()) {
      ConvertBinaryDictValuesToBase64(value.GetDict());
    }
  }
}

}  // namespace

RulesFunction::RulesFunction() = default;

RulesFunction::~RulesFunction() = default;

ExtensionFunction::ResponseAction RulesFunction::Run() {
  EXTENSION_FUNCTION_VALIDATE(CreateParams());
  EXTENSION_FUNCTION_VALIDATE(args().size() >= 2);
  const auto& event_name_value = args()[0];
  const auto& web_view_instance_id_value = args()[1];
  EXTENSION_FUNCTION_VALIDATE(event_name_value.is_string());
  EXTENSION_FUNCTION_VALIDATE(web_view_instance_id_value.is_int());
  std::string event_name = event_name_value.GetString();
  int web_view_instance_id = web_view_instance_id_value.GetInt();

  EXTENSION_FUNCTION_VALIDATE(extension_);

  // <webview> embedders use the declarativeWebRequest API via
  // <webview>.onRequest.
  if (web_view_instance_id && !extension_->permissions_data()->HasAPIPermission(
                                  mojom::APIPermissionID::kWebView)) {
    return RespondNow(Error("Missing webview permission"));
  }

  RecordUMA(event_name);

  // If we are not operating on a particular <webview>, then the key is 0.
  int rules_registry_id = RulesRegistryService::kDefaultRulesRegistryID;

#if BUILDFLAG(ENABLE_GUEST_VIEW)
  bool from_web_view = web_view_instance_id != 0;
  if (from_web_view) {
    // Sample event names:
    // webViewInternal.declarativeWebRequest.onRequest.
    // webViewInternal.declarativeWebRequest.onMessage.
    // The "webViewInternal." prefix is removed from the event name.
    std::size_t found = event_name.find(kDeclarativeEventPrefix);
    EXTENSION_FUNCTION_VALIDATE(found != std::string::npos);
    event_name = event_name.substr(found);

    rules_registry_id = WebViewGuest::GetOrGenerateRulesRegistryID(
        source_process_id(), web_view_instance_id);
  }
#endif

  // The following call will return a NULL pointer for apps_shell, but should
  // never be called there anyways.
  rules_registry_ = RulesRegistryService::Get(browser_context())->
      GetRulesRegistry(rules_registry_id, event_name);
  DCHECK(rules_registry_.get());
  // Raw access to this function is not available to extensions, therefore
  // there should never be a request for a nonexisting rules registry.
  EXTENSION_FUNCTION_VALIDATE(rules_registry_.get());

  return RespondNow(RunInternal());
}

void RulesFunction::SendResponse(ResponseValue response) {
  Respond(std::move(response));
}

EventsEventAddRulesFunction::EventsEventAddRulesFunction() = default;

EventsEventAddRulesFunction::~EventsEventAddRulesFunction() = default;

bool EventsEventAddRulesFunction::CreateParams() {
  ConvertBinaryListElementsToBase64(mutable_args());
  params_ = AddRules::Params::Create(args());
  return params_.has_value();
}

ExtensionFunction::ResponseValue EventsEventAddRulesFunction::RunInternal() {
  std::vector<const api::events::Rule*> rules_out;
  std::string error = rules_registry_->AddRules(
      extension_id(), std::move(params_->rules), &rules_out);
  if (!error.empty()) {
    return Error(error);
  }

  base::Value::List rules_value;
  rules_value.reserve(rules_out.size());
  for (const auto* rule : rules_out)
    rules_value.Append(rule->ToValue());
  return WithArguments(std::move(rules_value));
}

void EventsEventAddRulesFunction::RecordUMA(
    const std::string& event_name) const {
  DeclarativeAPIFunctionType type = kDeclarativeApiFunctionCallTypeMax;
  switch (GetDeclarativeAPIType(event_name)) {
    case DeclarativeAPIType::kContent:
      type = kDeclarativeContentAddRules;
      break;
    case DeclarativeAPIType::kWebRequest:
      type = kDeclarativeWebRequestAddRules;
      break;
    case DeclarativeAPIType::kWebRequestWebview:
      type = kDeclarativeWebRequestWebviewAddRules;
      break;
    case DeclarativeAPIType::kUnknown:
      NOTREACHED_IN_MIGRATION();
      return;
  }
  RecordUMAHelper(type);
}

EventsEventRemoveRulesFunction::EventsEventRemoveRulesFunction() = default;

EventsEventRemoveRulesFunction::~EventsEventRemoveRulesFunction() = default;

bool EventsEventRemoveRulesFunction::CreateParams() {
  params_ = RemoveRules::Params::Create(args());
  return params_.has_value();
}

ExtensionFunction::ResponseValue EventsEventRemoveRulesFunction::RunInternal() {
  std::string error;
  if (params_->rule_identifiers) {
    error = rules_registry_->RemoveRules(extension_id(),
                                         *params_->rule_identifiers);
  } else {
    error = rules_registry_->RemoveAllRules(extension_id());
  }

  return error.empty() ? NoArguments() : Error(error);
}

void EventsEventRemoveRulesFunction::RecordUMA(
    const std::string& event_name) const {
  DeclarativeAPIFunctionType type = kDeclarativeApiFunctionCallTypeMax;
  switch (GetDeclarativeAPIType(event_name)) {
    case DeclarativeAPIType::kContent:
      type = kDeclarativeContentRemoveRules;
      break;
    case DeclarativeAPIType::kWebRequest:
      type = kDeclarativeWebRequestRemoveRules;
      break;
    case DeclarativeAPIType::kWebRequestWebview:
      type = kDeclarativeWebRequestWebviewRemoveRules;
      break;
    case DeclarativeAPIType::kUnknown:
      NOTREACHED_IN_MIGRATION();
      return;
  }
  RecordUMAHelper(type);
}

EventsEventGetRulesFunction::EventsEventGetRulesFunction() = default;

EventsEventGetRulesFunction::~EventsEventGetRulesFunction() = default;

bool EventsEventGetRulesFunction::CreateParams() {
  params_ = GetRules::Params::Create(args());
  return params_.has_value();
}

ExtensionFunction::ResponseValue EventsEventGetRulesFunction::RunInternal() {
  std::vector<const Rule*> rules;
  if (params_->rule_identifiers) {
    rules_registry_->GetRules(extension_id(), *params_->rule_identifiers,
                              &rules);
  } else {
    rules_registry_->GetAllRules(extension_id(), &rules);
  }

  base::Value::List rules_value;
  rules_value.reserve(rules.size());
  for (const auto* rule : rules)
    rules_value.Append(rule->ToValue());
  return WithArguments(std::move(rules_value));
}

void EventsEventGetRulesFunction::RecordUMA(
    const std::string& event_name) const {
  DeclarativeAPIFunctionType type = kDeclarativeApiFunctionCallTypeMax;
  switch (GetDeclarativeAPIType(event_name)) {
    case DeclarativeAPIType::kContent:
      type = kDeclarativeContentGetRules;
      break;
    case DeclarativeAPIType::kWebRequest:
      type = kDeclarativeWebRequestGetRules;
      break;
    case DeclarativeAPIType::kWebRequestWebview:
      type = kDeclarativeWebRequestWebviewGetRules;
      break;
    case DeclarativeAPIType::kUnknown:
      NOTREACHED_IN_MIGRATION();
      return;
  }
  RecordUMAHelper(type);
}

}  // namespace extensions
