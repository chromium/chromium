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
#include "base/metrics/histogram_functions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/values.h"
#include "components/guest_view/buildflags/buildflags.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/api/declarative/rules_registry_service.h"
#include "extensions/browser/api/declarative_webrequest/webrequest_constants.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/rules_registry_ids.h"
#include "extensions/common/api/declarative/declarative_constants.h"
#include "extensions/common/api/events.h"
#include "extensions/common/extension_api.h"
#include "extensions/common/mojom/context_type.mojom.h"
#include "extensions/common/permissions/permissions_data.h"

#if BUILDFLAG(ENABLE_GUEST_VIEW)
#include "extensions/browser/guest_view/web_view/web_view_guest.h"
#endif

using extensions::api::events::Rule;

namespace AddRules = extensions::api::events::Event::AddRules;
namespace GetRules = extensions::api::events::Event::GetRules;
namespace RemoveRules = extensions::api::events::Event::RemoveRules;

namespace extensions {

namespace {

constexpr char kDeclarativeWebRequestWebViewOnRequestEvent[] =
    "webViewInternal.declarativeWebRequest.onRequest";
constexpr char kRulesRegistryNotAvailableError[] =
    "Internal error: rules registry is not available.";

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
enum class DeclarativeAPIFunctionType {
  kDeclarativeContentAddRules = 0,
  kDeclarativeContentRemoveRules = 1,
  kDeclarativeContentGetRules = 2,
  kDeclarativeWebRequestAddRules = 3,
  kDeclarativeWebRequestRemoveRules = 4,
  kDeclarativeWebRequestGetRules = 5,
  kDeclarativeWebRequestWebviewAddRules = 6,
  kDeclarativeWebRequestWebviewRemoveRules = 7,
  kDeclarativeWebRequestWebviewGetRules = 8,

  kMaxValue = kDeclarativeWebRequestWebviewGetRules,
};

DeclarativeAPIType GetDeclarativeAPIType(const std::string& event_name) {
  if (event_name == declarative_content_constants::kOnPageChanged) {
    return DeclarativeAPIType::kContent;
  }
  if (event_name == declarative_webrequest_constants::kOnRequest) {
    return DeclarativeAPIType::kWebRequest;
  }
  if (event_name == kDeclarativeWebRequestWebViewOnRequestEvent) {
    return DeclarativeAPIType::kWebRequestWebview;
  }
  return DeclarativeAPIType::kUnknown;
}

void RecordUMAHelper(DeclarativeAPIFunctionType type) {
  DCHECK_LE(type, DeclarativeAPIFunctionType::kMaxValue);
  base::UmaHistogramEnumeration("Extensions.DeclarativeAPIFunctionCalls", type);
}

void ConvertBinaryDictValuesToBase64(base::DictValue& dict);

// Encodes |binary| as base64 and returns a new string value populated with the
// encoded string.
base::Value ConvertBinaryToBase64(const base::Value& binary) {
  return base::Value(base::Base64Encode(binary.GetBlob()));
}

// Parses through |args| replacing any binary values with base64 encoded
// string values. Recurses over any nested List values, and calls
// ConvertBinaryDictValuesToBase64 for any nested Dict values.
void ConvertBinaryListElementsToBase64(base::ListValue& args) {
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
void ConvertBinaryDictValuesToBase64(base::DictValue& dict) {
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
  // Declarative rule functions are restricted to privileged extension contexts.
  if (!extension_ ||
      source_context_type() != mojom::ContextType::kPrivilegedExtension) {
    return RespondNow(BadMessage());
  }

  // Validate IPC arguments against the generated schema (which also guarantees
  // `args()[0]` is a string `eventName` and `args()[1]` is an int
  // `webViewInstanceId`).
  if (!CreateParams()) {
    return RespondNow(BadMessage());
  }
  const std::string event_name = args()[0].GetString();
  const int web_view_instance_id = args()[1].GetInt();
  // `0` indicates a regular extension event; `> 0` is a `<webview>` instance
  // ID.
  if (web_view_instance_id < 0) {
    return RespondNow(BadMessage());
  }

  // Only allow the three declarative events that actually support rules.
  const DeclarativeAPIType api_type = GetDeclarativeAPIType(event_name);
  if (api_type == DeclarativeAPIType::kUnknown) {
    return RespondNow(BadMessage());
  }

  // `<webview>` instance IDs must be paired exclusively with the `<webview>`
  // declarativeWebRequest event.
  const bool from_web_view = web_view_instance_id > 0;
  if (from_web_view != (api_type == DeclarativeAPIType::kWebRequestWebview)) {
    return RespondNow(BadMessage());
  }

  // <webview> embedders use the declarativeWebRequest API via
  // <webview>.request.onRequest from an embedder frame (never a service
  // worker). Unlike `declarativeContent` and `declarativeWebRequest`,
  // `kWebView` sets `kFlagCannotBeOptional` and `<webview>` bindings are never
  // instantiated without it, so an uncompromised renderer cannot send
  // `web_view_instance_id > 0` without `kWebView`.
  if (from_web_view) {
#if BUILDFLAG(ENABLE_GUEST_VIEW)
    if (is_from_service_worker() || render_frame_host() == nullptr ||
        !extension_->permissions_data()->HasAPIPermission(
            mojom::APIPermissionID::kWebView)) {
      return RespondNow(BadMessage());
    }
#else
    return RespondNow(BadMessage());
#endif
  }

  // Verify the target declarative API (`declarativeContent`,
  // `declarativeWebRequest`, or `webViewInternal`) is available to this
  // extension and context. Return `Error` rather than `BadMessage` because
  // `declarativeContent` and `declarativeWebRequest` can be optional
  // permissions and `DeclarativeEvent` wrappers do not re-check access on call.
  Feature::Availability availability =
      ExtensionAPI::GetSharedInstance()->IsAvailable(
          event_name, extension_.get(), source_context_type(), source_url(),
          CheckAliasStatus::ALLOWED, context_id(), *GetContextData());
  if (!availability.is_available()) {
    return RespondNow(Error(availability.message()));
  }

  // If we are not operating on a particular <webview>, then the key is 0.
  int rules_registry_id = rules_registry_ids::kDefaultRulesRegistryID;
  std::string registry_event_name = event_name;

#if BUILDFLAG(ENABLE_GUEST_VIEW)
  if (from_web_view) {
    registry_event_name = declarative_webrequest_constants::kOnRequest;
    rules_registry_id = WebViewGuest::GetOrGenerateRulesRegistryID(
        source_process_id(), web_view_instance_id);
  }
#endif

  RulesRegistryService* rules_registry_service =
      RulesRegistryService::Get(browser_context());
  rules_registry_ = rules_registry_service
                        ? rules_registry_service->GetRulesRegistry(
                              rules_registry_id, registry_event_name)
                        : nullptr;
  // `rules_registry_` is only null on an invalid `source_process_id()`, during
  // `RulesRegistryService` shutdown, or in embedders without a
  // `ContentRulesRegistry`. None of these indicate a misbehaving renderer, so
  // return a normal error rather than `BadMessage()`.
  if (!rules_registry_) {
    return RespondNow(Error(kRulesRegistryNotAvailableError));
  }

  RecordUMA(event_name);

  return RespondNow(RunInternal());
}

void RulesFunction::SendResponse(ResponseValue response) {
  Respond(std::move(response));
}

EventsEventAddRulesFunction::EventsEventAddRulesFunction() = default;

EventsEventAddRulesFunction::~EventsEventAddRulesFunction() = default;

bool EventsEventAddRulesFunction::CreateParams() {
  ConvertBinaryListElementsToBase64(GetMutableArgs());
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

  base::ListValue rules_value;
  rules_value.reserve(rules_out.size());
  for (const auto* rule : rules_out)
    rules_value.Append(rule->ToValue());
  return WithArguments(std::move(rules_value));
}

void EventsEventAddRulesFunction::RecordUMA(
    const std::string& event_name) const {
  DeclarativeAPIFunctionType type;
  switch (GetDeclarativeAPIType(event_name)) {
    case DeclarativeAPIType::kContent:
      type = DeclarativeAPIFunctionType::kDeclarativeContentAddRules;
      break;
    case DeclarativeAPIType::kWebRequest:
      type = DeclarativeAPIFunctionType::kDeclarativeWebRequestAddRules;
      break;
    case DeclarativeAPIType::kWebRequestWebview:
      type = DeclarativeAPIFunctionType::kDeclarativeWebRequestWebviewAddRules;
      break;
    case DeclarativeAPIType::kUnknown:
      NOTREACHED();
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
  DeclarativeAPIFunctionType type;
  switch (GetDeclarativeAPIType(event_name)) {
    case DeclarativeAPIType::kContent:
      type = DeclarativeAPIFunctionType::kDeclarativeContentRemoveRules;
      break;
    case DeclarativeAPIType::kWebRequest:
      type = DeclarativeAPIFunctionType::kDeclarativeWebRequestRemoveRules;
      break;
    case DeclarativeAPIType::kWebRequestWebview:
      type =
          DeclarativeAPIFunctionType::kDeclarativeWebRequestWebviewRemoveRules;
      break;
    case DeclarativeAPIType::kUnknown:
      NOTREACHED();
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

  base::ListValue rules_value;
  rules_value.reserve(rules.size());
  for (const auto* rule : rules)
    rules_value.Append(rule->ToValue());
  return WithArguments(std::move(rules_value));
}

void EventsEventGetRulesFunction::RecordUMA(
    const std::string& event_name) const {
  DeclarativeAPIFunctionType type;
  switch (GetDeclarativeAPIType(event_name)) {
    case DeclarativeAPIType::kContent:
      type = DeclarativeAPIFunctionType::kDeclarativeContentGetRules;
      break;
    case DeclarativeAPIType::kWebRequest:
      type = DeclarativeAPIFunctionType::kDeclarativeWebRequestGetRules;
      break;
    case DeclarativeAPIType::kWebRequestWebview:
      type = DeclarativeAPIFunctionType::kDeclarativeWebRequestWebviewGetRules;
      break;
    case DeclarativeAPIType::kUnknown:
      NOTREACHED();
  }
  RecordUMAHelper(type);
}

}  // namespace extensions
