// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/declarative_webrequest/webrequest_action.h"

#include <limits>
#include <utility>

#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "content/public/common/url_constants.h"
#include "extensions/browser/api/declarative/deduping_factory.h"
#include "extensions/browser/api/declarative_webrequest/request_stage.h"
#include "extensions/browser/api/declarative_webrequest/webrequest_condition.h"
#include "extensions/browser/api/declarative_webrequest/webrequest_constants.h"
#include "extensions/browser/api/web_request/web_request_api_constants.h"
#include "extensions/browser/api/web_request/web_request_api_helpers.h"
#include "extensions/browser/api/web_request/web_request_info.h"
#include "extensions/browser/api/web_request/web_request_permissions.h"
#include "extensions/browser/extension_navigation_ui_data.h"
#include "extensions/browser/guest_view/web_view/web_view_renderer_state.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/http/http_util.h"
#include "third_party/re2/src/re2/re2.h"

using extension_web_request_api_helpers::EventResponseDelta;

namespace extensions {

namespace helpers = extension_web_request_api_helpers;
namespace keys = declarative_webrequest_constants;

namespace {
// Error messages.
const char kIgnoreRulesRequiresParameterError[] =
    "IgnoreRules requires at least one parameter.";

const char kTransparentImageUrl[] = "data:image/png;base64,iVBORw0KGgoAAAANSUh"
    "EUgAAAAEAAAABCAYAAAAfFcSJAAAACklEQVR4nGMAAQAABQABDQottAAAAABJRU5ErkJggg==";
const char kEmptyDocumentUrl[] = "data:text/html,";

#define INPUT_FORMAT_VALIDATE(test)                          \
  do {                                                       \
    if (!(test)) {                                           \
      *bad_message = true;                                   \
      return scoped_refptr<const WebRequestAction>(nullptr); \
    }                                                        \
  } while (0)

helpers::RequestCookie ParseRequestCookie(const base::DictionaryValue* dict) {
  helpers::RequestCookie result;
  std::string tmp;
  if (dict->GetString(keys::kNameKey, &tmp))
    result.name = tmp;
  if (dict->GetString(keys::kValueKey, &tmp))
    result.value = tmp;
  return result;
}

void ParseResponseCookieImpl(const base::DictionaryValue* dict,
                             helpers::ResponseCookie* cookie) {
  std::string string_tmp;
  int int_tmp = 0;
  bool bool_tmp = false;
  if (dict->GetString(keys::kNameKey, &string_tmp))
    cookie->name = string_tmp;
  if (dict->GetString(keys::kValueKey, &string_tmp))
    cookie->value = string_tmp;
  if (dict->GetString(keys::kExpiresKey, &string_tmp))
    cookie->expires = string_tmp;
  if (dict->GetInteger(keys::kMaxAgeKey, &int_tmp))
    cookie->max_age = int_tmp;
  if (dict->GetString(keys::kDomainKey, &string_tmp))
    cookie->domain = string_tmp;
  if (dict->GetString(keys::kPathKey, &string_tmp))
    cookie->path = string_tmp;
  if (dict->GetBoolean(keys::kSecureKey, &bool_tmp))
    cookie->secure = bool_tmp;
  if (dict->GetBoolean(keys::kHttpOnlyKey, &bool_tmp))
    cookie->http_only = bool_tmp;
}

helpers::ResponseCookie ParseResponseCookie(const base::DictionaryValue* dict) {
  helpers::ResponseCookie result;
  ParseResponseCookieImpl(dict, &result);
  return result;
}

helpers::FilterResponseCookie ParseFilterResponseCookie(
    const base::DictionaryValue* dict) {
  helpers::FilterResponseCookie result;
  ParseResponseCookieImpl(dict, &result);

  int int_tmp = 0;
  bool bool_tmp = false;
  if (dict->GetInteger(keys::kAgeUpperBoundKey, &int_tmp))
    result.age_upper_bound = int_tmp;
  if (dict->GetInteger(keys::kAgeLowerBoundKey, &int_tmp))
    result.age_lower_bound = int_tmp;
  if (dict->GetBoolean(keys::kSessionCookieKey, &bool_tmp))
    result.session_cookie = bool_tmp;
  return result;
}

// Helper function for WebRequestActions that can be instantiated by just
// calling the constructor.
template <class T>
scoped_refptr<const WebRequestAction> CallConstructorFactoryMethod(
    const std::string& instance_type,
    const base::Value* value,
    std::string* error,
    bool* bad_message) {
  return base::MakeRefCounted<T>();
}

scoped_refptr<const WebRequestAction> CreateRedirectRequestAction(
    const std::string& instance_type,
    const base::Value* value,
    std::string* error,
    bool* bad_message) {
  const base::DictionaryValue* dict = NULL;
  CHECK(value->GetAsDictionary(&dict));
  std::string redirect_url_string;
  INPUT_FORMAT_VALIDATE(
      dict->GetString(keys::kRedirectUrlKey, &redirect_url_string));
  GURL redirect_url(redirect_url_string);
  return base::MakeRefCounted<WebRequestRedirectAction>(redirect_url);
}

scoped_refptr<const WebRequestAction> CreateRedirectRequestByRegExAction(
    const std::string& instance_type,
    const base::Value* value,
    std::string* error,
    bool* bad_message) {
  const base::DictionaryValue* dict = NULL;
  CHECK(value->GetAsDictionary(&dict));
  std::string from;
  std::string to;
  INPUT_FORMAT_VALIDATE(dict->GetString(keys::kFromKey, &from));
  INPUT_FORMAT_VALIDATE(dict->GetString(keys::kToKey, &to));

  to = WebRequestRedirectByRegExAction::PerlToRe2Style(to);

  RE2::Options options;
  options.set_case_sensitive(false);
  std::unique_ptr<RE2> from_pattern = std::make_unique<RE2>(from, options);

  if (!from_pattern->ok()) {
    *error = "Invalid pattern '" + from + "' -> '" + to + "'";
    return scoped_refptr<const WebRequestAction>(nullptr);
  }
  return base::MakeRefCounted<WebRequestRedirectByRegExAction>(
      std::move(from_pattern), to);
}

scoped_refptr<const WebRequestAction> CreateSetRequestHeaderAction(
    const std::string& instance_type,
    const base::Value* json_value,
    std::string* error,
    bool* bad_message) {
  const base::DictionaryValue* dict = NULL;
  CHECK(json_value->GetAsDictionary(&dict));
  std::string name;
  std::string value;
  INPUT_FORMAT_VALIDATE(dict->GetString(keys::kNameKey, &name));
  INPUT_FORMAT_VALIDATE(dict->GetString(keys::kValueKey, &value));
  if (!net::HttpUtil::IsValidHeaderName(name)) {
    *error = extension_web_request_api_constants::kInvalidHeaderName;
    return scoped_refptr<const WebRequestAction>(nullptr);
  }
  if (!net::HttpUtil::IsValidHeaderValue(value)) {
    *error = ErrorUtils::FormatErrorMessage(
        extension_web_request_api_constants::kInvalidHeaderValue, name);
    return scoped_refptr<const WebRequestAction>(nullptr);
  }
  return base::MakeRefCounted<WebRequestSetRequestHeaderAction>(name, value);
}

scoped_refptr<const WebRequestAction> CreateRemoveRequestHeaderAction(
    const std::string& instance_type,
    const base::Value* value,
    std::string* error,
    bool* bad_message) {
  const base::DictionaryValue* dict = NULL;
  CHECK(value->GetAsDictionary(&dict));
  std::string name;
  INPUT_FORMAT_VALIDATE(dict->GetString(keys::kNameKey, &name));
  if (!net::HttpUtil::IsValidHeaderName(name)) {
    *error = extension_web_request_api_constants::kInvalidHeaderName;
    return scoped_refptr<const WebRequestAction>(nullptr);
  }
  return base::MakeRefCounted<WebRequestRemoveRequestHeaderAction>(name);
}

scoped_refptr<const WebRequestAction> CreateAddResponseHeaderAction(
    const std::string& instance_type,
    const base::Value* json_value,
    std::string* error,
    bool* bad_message) {
  const base::DictionaryValue* dict = NULL;
  CHECK(json_value->GetAsDictionary(&dict));
  std::string name;
  std::string value;
  INPUT_FORMAT_VALIDATE(dict->GetString(keys::kNameKey, &name));
  INPUT_FORMAT_VALIDATE(dict->GetString(keys::kValueKey, &value));
  if (!net::HttpUtil::IsValidHeaderName(name)) {
    *error = extension_web_request_api_constants::kInvalidHeaderName;
    return scoped_refptr<const WebRequestAction>(nullptr);
  }
  if (!net::HttpUtil::IsValidHeaderValue(value)) {
    *error = ErrorUtils::FormatErrorMessage(
        extension_web_request_api_constants::kInvalidHeaderValue, name);
    return scoped_refptr<const WebRequestAction>(nullptr);
  }
  return base::MakeRefCounted<WebRequestAddResponseHeaderAction>(name, value);
}

scoped_refptr<const WebRequestAction> CreateRemoveResponseHeaderAction(
    const std::string& instance_type,
    const base::Value* json_value,
    std::string* error,
    bool* bad_message) {
  const base::DictionaryValue* dict = NULL;
  CHECK(json_value->GetAsDictionary(&dict));
  std::string name;
  std::string value;
  INPUT_FORMAT_VALIDATE(dict->GetString(keys::kNameKey, &name));
  bool has_value = dict->GetString(keys::kValueKey, &value);
  if (!net::HttpUtil::IsValidHeaderName(name)) {
    *error = extension_web_request_api_constants::kInvalidHeaderName;
    return scoped_refptr<const WebRequestAction>(nullptr);
  }
  if (has_value && !net::HttpUtil::IsValidHeaderValue(value)) {
    *error = ErrorUtils::FormatErrorMessage(
        extension_web_request_api_constants::kInvalidHeaderValue, name);
    return scoped_refptr<const WebRequestAction>(nullptr);
  }
  return base::MakeRefCounted<WebRequestRemoveResponseHeaderAction>(name, value,
                                                                    has_value);
}

scoped_refptr<const WebRequestAction> CreateIgnoreRulesAction(
    const std::string& instance_type,
    const base::Value* value,
    std::string* error,
    bool* bad_message) {
  const base::DictionaryValue* dict = NULL;
  CHECK(value->GetAsDictionary(&dict));
  bool has_parameter = false;
  int minimum_priority = std::numeric_limits<int>::min();
  std::string ignore_tag;
  if (dict->HasKey(keys::kLowerPriorityThanKey)) {
    INPUT_FORMAT_VALIDATE(
        dict->GetInteger(keys::kLowerPriorityThanKey, &minimum_priority));
    has_parameter = true;
  }
  if (dict->HasKey(keys::kHasTagKey)) {
    INPUT_FORMAT_VALIDATE(dict->GetString(keys::kHasTagKey, &ignore_tag));
    has_parameter = true;
  }
  if (!has_parameter) {
    *error = kIgnoreRulesRequiresParameterError;
    return scoped_refptr<const WebRequestAction>(nullptr);
  }
  return base::MakeRefCounted<WebRequestIgnoreRulesAction>(minimum_priority,
                                                           ignore_tag);
}

scoped_refptr<const WebRequestAction> CreateRequestCookieAction(
    const std::string& instance_type,
    const base::Value* value,
    std::string* error,
    bool* bad_message) {
  using extension_web_request_api_helpers::RequestCookieModification;

  const base::DictionaryValue* dict = NULL;
  CHECK(value->GetAsDictionary(&dict));

  RequestCookieModification modification;

  // Get modification type.
  if (instance_type == keys::kAddRequestCookieType)
    modification.type = helpers::ADD;
  else if (instance_type == keys::kEditRequestCookieType)
    modification.type = helpers::EDIT;
  else if (instance_type == keys::kRemoveRequestCookieType)
    modification.type = helpers::REMOVE;
  else
    INPUT_FORMAT_VALIDATE(false);

  // Get filter.
  if (modification.type == helpers::EDIT ||
      modification.type == helpers::REMOVE) {
    const base::DictionaryValue* filter = NULL;
    INPUT_FORMAT_VALIDATE(dict->GetDictionary(keys::kFilterKey, &filter));
    modification.filter = ParseRequestCookie(filter);
  }

  // Get new value.
  if (modification.type == helpers::ADD) {
    const base::DictionaryValue* value = NULL;
    INPUT_FORMAT_VALIDATE(dict->GetDictionary(keys::kCookieKey, &value));
    modification.modification = ParseRequestCookie(value);
  } else if (modification.type == helpers::EDIT) {
    const base::DictionaryValue* value = NULL;
    INPUT_FORMAT_VALIDATE(dict->GetDictionary(keys::kModificationKey, &value));
    modification.modification = ParseRequestCookie(value);
  }

  return base::MakeRefCounted<WebRequestRequestCookieAction>(
      std::move(modification));
}

scoped_refptr<const WebRequestAction> CreateResponseCookieAction(
    const std::string& instance_type,
    const base::Value* value,
    std::string* error,
    bool* bad_message) {
  using extension_web_request_api_helpers::ResponseCookieModification;

  const base::DictionaryValue* dict = NULL;
  CHECK(value->GetAsDictionary(&dict));

  ResponseCookieModification modification;

  // Get modification type.
  if (instance_type == keys::kAddResponseCookieType)
    modification.type = helpers::ADD;
  else if (instance_type == keys::kEditResponseCookieType)
    modification.type = helpers::EDIT;
  else if (instance_type == keys::kRemoveResponseCookieType)
    modification.type = helpers::REMOVE;
  else
    INPUT_FORMAT_VALIDATE(false);

  // Get filter.
  if (modification.type == helpers::EDIT ||
      modification.type == helpers::REMOVE) {
    const base::DictionaryValue* filter = NULL;
    INPUT_FORMAT_VALIDATE(dict->GetDictionary(keys::kFilterKey, &filter));
    modification.filter = ParseFilterResponseCookie(filter);
  }

  // Get new value.
  if (modification.type == helpers::ADD) {
    const base::DictionaryValue* value = NULL;
    INPUT_FORMAT_VALIDATE(dict->GetDictionary(keys::kCookieKey, &value));
    modification.modification = ParseResponseCookie(value);
  } else if (modification.type == helpers::EDIT) {
    const base::DictionaryValue* value = NULL;
    INPUT_FORMAT_VALIDATE(dict->GetDictionary(keys::kModificationKey, &value));
    modification.modification = ParseResponseCookie(value);
  }

  return base::MakeRefCounted<WebRequestResponseCookieAction>(
      std::move(modification));
}

scoped_refptr<const WebRequestAction> CreateSendMessageToExtensionAction(
    const std::string& name,
    const base::Value* value,
    std::string* error,
    bool* bad_message) {
  const base::DictionaryValue* dict = NULL;
  CHECK(value->GetAsDictionary(&dict));
  std::string message;
  INPUT_FORMAT_VALIDATE(dict->GetString(keys::kMessageKey, &message));
  return base::MakeRefCounted<WebRequestSendMessageToExtensionAction>(message);
}

struct WebRequestActionFactory {
  DedupingFactory<WebRequestAction> factory;

  WebRequestActionFactory() : factory(5) {
    factory.RegisterFactoryMethod(
        keys::kAddRequestCookieType,
        DedupingFactory<WebRequestAction>::IS_PARAMETERIZED,
        &CreateRequestCookieAction);
    factory.RegisterFactoryMethod(
        keys::kAddResponseCookieType,
        DedupingFactory<WebRequestAction>::IS_PARAMETERIZED,
        &CreateResponseCookieAction);
    factory.RegisterFactoryMethod(
        keys::kAddResponseHeaderType,
        DedupingFactory<WebRequestAction>::IS_PARAMETERIZED,
        &CreateAddResponseHeaderAction);
    factory.RegisterFactoryMethod(
        keys::kCancelRequestType,
        DedupingFactory<WebRequestAction>::IS_NOT_PARAMETERIZED,
        &CallConstructorFactoryMethod<WebRequestCancelAction>);
    factory.RegisterFactoryMethod(
        keys::kEditRequestCookieType,
        DedupingFactory<WebRequestAction>::IS_PARAMETERIZED,
        &CreateRequestCookieAction);
    factory.RegisterFactoryMethod(
        keys::kEditResponseCookieType,
        DedupingFactory<WebRequestAction>::IS_PARAMETERIZED,
        &CreateResponseCookieAction);
    factory.RegisterFactoryMethod(
        keys::kRedirectByRegExType,
        DedupingFactory<WebRequestAction>::IS_PARAMETERIZED,
        &CreateRedirectRequestByRegExAction);
    factory.RegisterFactoryMethod(
        keys::kRedirectRequestType,
        DedupingFactory<WebRequestAction>::IS_PARAMETERIZED,
        &CreateRedirectRequestAction);
    factory.RegisterFactoryMethod(
        keys::kRedirectToTransparentImageType,
        DedupingFactory<WebRequestAction>::IS_NOT_PARAMETERIZED,
        &CallConstructorFactoryMethod<
            WebRequestRedirectToTransparentImageAction>);
    factory.RegisterFactoryMethod(
        keys::kRedirectToEmptyDocumentType,
        DedupingFactory<WebRequestAction>::IS_NOT_PARAMETERIZED,
        &CallConstructorFactoryMethod<WebRequestRedirectToEmptyDocumentAction>);
    factory.RegisterFactoryMethod(
        keys::kRemoveRequestCookieType,
        DedupingFactory<WebRequestAction>::IS_PARAMETERIZED,
        &CreateRequestCookieAction);
    factory.RegisterFactoryMethod(
        keys::kRemoveResponseCookieType,
        DedupingFactory<WebRequestAction>::IS_PARAMETERIZED,
        &CreateResponseCookieAction);
    factory.RegisterFactoryMethod(
        keys::kSetRequestHeaderType,
        DedupingFactory<WebRequestAction>::IS_PARAMETERIZED,
        &CreateSetRequestHeaderAction);
    factory.RegisterFactoryMethod(
        keys::kRemoveRequestHeaderType,
        DedupingFactory<WebRequestAction>::IS_PARAMETERIZED,
        &CreateRemoveRequestHeaderAction);
    factory.RegisterFactoryMethod(
        keys::kRemoveResponseHeaderType,
        DedupingFactory<WebRequestAction>::IS_PARAMETERIZED,
        &CreateRemoveResponseHeaderAction);
    factory.RegisterFactoryMethod(
        keys::kIgnoreRulesType,
        DedupingFactory<WebRequestAction>::IS_PARAMETERIZED,
        &CreateIgnoreRulesAction);
    factory.RegisterFactoryMethod(
        keys::kSendMessageToExtensionType,
        DedupingFactory<WebRequestAction>::IS_PARAMETERIZED,
        &CreateSendMessageToExtensionAction);
  }
};

base::LazyInstance<WebRequestActionFactory>::Leaky
    g_web_request_action_factory = LAZY_INSTANCE_INITIALIZER;

}  // namespace

//
// WebRequestAction
//

WebRequestAction::~WebRequestAction() {}

bool WebRequestAction::Equals(const WebRequestAction* other) const {
  return type() == other->type();
}

bool WebRequestAction::HasPermission(ApplyInfo* apply_info,
                                     const std::string& extension_id) const {
  PermissionHelper* permission_helper = apply_info->permission_helper;
  const WebRequestInfo* request = apply_info->request_data.request;
  if (WebRequestPermissions::HideRequest(permission_helper, *request))
    return false;

  // In unit tests we don't have a permission_helper object here and skip host
  // permission checks.
  if (!permission_helper)
    return true;

  // The embedder can always access all hosts from within a <webview>.
  // The same is not true of extensions.
  if (request->is_web_view)
    return true;

  WebRequestPermissions::HostPermissionsCheck permission_check =
      WebRequestPermissions::REQUIRE_ALL_URLS;
  switch (host_permissions_strategy()) {
    case STRATEGY_DEFAULT:  // Default value is already set.
      break;
    case STRATEGY_NONE:
      permission_check = WebRequestPermissions::DO_NOT_CHECK_HOST;
      break;
    case STRATEGY_HOST:
      permission_check = WebRequestPermissions::REQUIRE_HOST_PERMISSION_FOR_URL;
      break;
  }
  // TODO(devlin): Pass in the real tab id here.
  return WebRequestPermissions::CanExtensionAccessURL(
             permission_helper, extension_id, request->url, -1,
             apply_info->crosses_incognito, permission_check,
             request->initiator,
             request->type) == PermissionsData::PageAccess::kAllowed;
}

// static
scoped_refptr<const WebRequestAction> WebRequestAction::Create(
    content::BrowserContext* browser_context,
    const Extension* extension,
    const base::Value& json_action,
    std::string* error,
    bool* bad_message) {
  *error = "";
  *bad_message = false;

  const base::DictionaryValue* action_dict = NULL;
  INPUT_FORMAT_VALIDATE(json_action.GetAsDictionary(&action_dict));

  std::string instance_type;
  INPUT_FORMAT_VALIDATE(
      action_dict->GetString(keys::kInstanceTypeKey, &instance_type));

  WebRequestActionFactory& factory = g_web_request_action_factory.Get();
  return factory.factory.Instantiate(
      instance_type, action_dict, error, bad_message);
}

void WebRequestAction::Apply(const std::string& extension_id,
                             base::Time extension_install_time,
                             ApplyInfo* apply_info) const {
  if (!HasPermission(apply_info, extension_id))
    return;
  if (stages() & apply_info->request_data.stage) {
    base::Optional<EventResponseDelta> delta = CreateDelta(
        apply_info->request_data, extension_id, extension_install_time);
    if (delta.has_value())
      apply_info->deltas->push_back(std::move(delta.value()));
    if (type() == WebRequestAction::ACTION_IGNORE_RULES) {
      const WebRequestIgnoreRulesAction* ignore_action =
          static_cast<const WebRequestIgnoreRulesAction*>(this);
      if (!ignore_action->ignore_tag().empty())
        apply_info->ignored_tags->insert(ignore_action->ignore_tag());
    }
  }
}

WebRequestAction::WebRequestAction(int stages,
                                   Type type,
                                   int minimum_priority,
                                   HostPermissionsStrategy strategy)
    : stages_(stages),
      type_(type),
      minimum_priority_(minimum_priority),
      host_permissions_strategy_(strategy) {}

//
// WebRequestCancelAction
//

WebRequestCancelAction::WebRequestCancelAction()
    : WebRequestAction(ON_BEFORE_REQUEST | ON_BEFORE_SEND_HEADERS |
                           ON_HEADERS_RECEIVED | ON_AUTH_REQUIRED,
                       ACTION_CANCEL_REQUEST,
                       std::numeric_limits<int>::min(),
                       STRATEGY_NONE) {}

WebRequestCancelAction::~WebRequestCancelAction() {}

std::string WebRequestCancelAction::GetName() const {
  return keys::kCancelRequestType;
}

base::Optional<EventResponseDelta> WebRequestCancelAction::CreateDelta(
    const WebRequestData& request_data,
    const std::string& extension_id,
    const base::Time& extension_install_time) const {
  CHECK(request_data.stage & stages());
  EventResponseDelta result(extension_id, extension_install_time);
  result.cancel = true;
  return result;
}

//
// WebRequestRedirectAction
//

WebRequestRedirectAction::WebRequestRedirectAction(const GURL& redirect_url)
    : WebRequestAction(ON_BEFORE_REQUEST | ON_HEADERS_RECEIVED,
                       ACTION_REDIRECT_REQUEST,
                       std::numeric_limits<int>::min(),
                       STRATEGY_DEFAULT),
      redirect_url_(redirect_url) {}

WebRequestRedirectAction::~WebRequestRedirectAction() {}

bool WebRequestRedirectAction::Equals(const WebRequestAction* other) const {
  return WebRequestAction::Equals(other) &&
         redirect_url_ ==
             static_cast<const WebRequestRedirectAction*>(other)->redirect_url_;
}

std::string WebRequestRedirectAction::GetName() const {
  return keys::kRedirectRequestType;
}

base::Optional<EventResponseDelta> WebRequestRedirectAction::CreateDelta(
    const WebRequestData& request_data,
    const std::string& extension_id,
    const base::Time& extension_install_time) const {
  CHECK(request_data.stage & stages());
  if (request_data.request->url == redirect_url_)
    return base::nullopt;
  EventResponseDelta result(extension_id, extension_install_time);
  result.new_url = redirect_url_;
  return result;
}

//
// WebRequestRedirectToTransparentImageAction
//

WebRequestRedirectToTransparentImageAction::
    WebRequestRedirectToTransparentImageAction()
    : WebRequestAction(ON_BEFORE_REQUEST | ON_HEADERS_RECEIVED,
                       ACTION_REDIRECT_TO_TRANSPARENT_IMAGE,
                       std::numeric_limits<int>::min(),
                       STRATEGY_NONE) {}

WebRequestRedirectToTransparentImageAction::
~WebRequestRedirectToTransparentImageAction() {}

std::string WebRequestRedirectToTransparentImageAction::GetName() const {
  return keys::kRedirectToTransparentImageType;
}

base::Optional<EventResponseDelta>
WebRequestRedirectToTransparentImageAction::CreateDelta(
    const WebRequestData& request_data,
    const std::string& extension_id,
    const base::Time& extension_install_time) const {
  CHECK(request_data.stage & stages());
  EventResponseDelta result(extension_id, extension_install_time);
  result.new_url = GURL(kTransparentImageUrl);
  return result;
}

//
// WebRequestRedirectToEmptyDocumentAction
//

WebRequestRedirectToEmptyDocumentAction::
    WebRequestRedirectToEmptyDocumentAction()
    : WebRequestAction(ON_BEFORE_REQUEST | ON_HEADERS_RECEIVED,
                       ACTION_REDIRECT_TO_EMPTY_DOCUMENT,
                       std::numeric_limits<int>::min(),
                       STRATEGY_NONE) {}

WebRequestRedirectToEmptyDocumentAction::
~WebRequestRedirectToEmptyDocumentAction() {}

std::string WebRequestRedirectToEmptyDocumentAction::GetName() const {
  return keys::kRedirectToEmptyDocumentType;
}

base::Optional<EventResponseDelta>
WebRequestRedirectToEmptyDocumentAction::CreateDelta(
    const WebRequestData& request_data,
    const std::string& extension_id,
    const base::Time& extension_install_time) const {
  CHECK(request_data.stage & stages());
  EventResponseDelta result(extension_id, extension_install_time);
  result.new_url = GURL(kEmptyDocumentUrl);
  return result;
}

//
// WebRequestRedirectByRegExAction
//

WebRequestRedirectByRegExAction::WebRequestRedirectByRegExAction(
    std::unique_ptr<RE2> from_pattern,
    const std::string& to_pattern)
    : WebRequestAction(ON_BEFORE_REQUEST | ON_HEADERS_RECEIVED,
                       ACTION_REDIRECT_BY_REGEX_DOCUMENT,
                       std::numeric_limits<int>::min(),
                       STRATEGY_DEFAULT),
      from_pattern_(std::move(from_pattern)),
      to_pattern_(to_pattern.data(), to_pattern.size()) {}

WebRequestRedirectByRegExAction::~WebRequestRedirectByRegExAction() {}

// About the syntax of the two languages:
//
// ICU (Perl) states:
// $n The text of capture group n will be substituted for $n. n must be >= 0
//    and not greater than the number of capture groups. A $ not followed by a
//    digit has no special meaning, and will appear in the substitution text
//    as itself, a $.
// \  Treat the following character as a literal, suppressing any special
//    meaning. Backslash escaping in substitution text is only required for
//    '$' and '\', but may be used on any other character without bad effects.
//
// RE2, derived from RE2::Rewrite()
// \  May only be followed by a digit or another \. If followed by a single
//    digit, both characters represent the respective capture group. If followed
//    by another \, it is used as an escape sequence.

// static
std::string WebRequestRedirectByRegExAction::PerlToRe2Style(
    const std::string& perl) {
  std::string::const_iterator i = perl.begin();
  std::string result;
  while (i != perl.end()) {
    if (*i == '$') {
      ++i;
      if (i == perl.end()) {
        result += '$';
        return result;
      } else if (isdigit(*i)) {
        result += '\\';
        result += *i;
      } else {
        result += '$';
        result += *i;
      }
    } else if (*i == '\\') {
      ++i;
      if (i == perl.end()) {
        result += '\\';
      } else if (*i == '$') {
        result += '$';
      } else if (*i == '\\') {
        result += "\\\\";
      } else {
        result += *i;
      }
    } else {
      result += *i;
    }
    ++i;
  }
  return result;
}

bool WebRequestRedirectByRegExAction::Equals(
    const WebRequestAction* other) const {
  if (!WebRequestAction::Equals(other))
    return false;
  const WebRequestRedirectByRegExAction* casted_other =
      static_cast<const WebRequestRedirectByRegExAction*>(other);
  return from_pattern_->pattern() == casted_other->from_pattern_->pattern() &&
         to_pattern_ == casted_other->to_pattern_;
}

std::string WebRequestRedirectByRegExAction::GetName() const {
  return keys::kRedirectByRegExType;
}

base::Optional<EventResponseDelta> WebRequestRedirectByRegExAction::CreateDelta(
    const WebRequestData& request_data,
    const std::string& extension_id,
    const base::Time& extension_install_time) const {
  CHECK(request_data.stage & stages());
  CHECK(from_pattern_.get());

  const std::string& old_url = request_data.request->url.spec();
  std::string new_url = old_url;
  if (!RE2::Replace(&new_url, *from_pattern_, to_pattern_) ||
      new_url == old_url) {
    return base::nullopt;
  }

  EventResponseDelta result(extension_id, extension_install_time);
  result.new_url = GURL(new_url);
  return result;
}

//
// WebRequestSetRequestHeaderAction
//

WebRequestSetRequestHeaderAction::WebRequestSetRequestHeaderAction(
    const std::string& name,
    const std::string& value)
    : WebRequestAction(ON_BEFORE_SEND_HEADERS,
                       ACTION_SET_REQUEST_HEADER,
                       std::numeric_limits<int>::min(),
                       STRATEGY_DEFAULT),
      name_(name),
      value_(value) {}

WebRequestSetRequestHeaderAction::~WebRequestSetRequestHeaderAction() {}

bool WebRequestSetRequestHeaderAction::Equals(
    const WebRequestAction* other) const {
  if (!WebRequestAction::Equals(other))
    return false;
  const WebRequestSetRequestHeaderAction* casted_other =
      static_cast<const WebRequestSetRequestHeaderAction*>(other);
  return name_ == casted_other->name_ && value_ == casted_other->value_;
}

std::string WebRequestSetRequestHeaderAction::GetName() const {
  return keys::kSetRequestHeaderType;
}

base::Optional<EventResponseDelta>
WebRequestSetRequestHeaderAction::CreateDelta(
    const WebRequestData& request_data,
    const std::string& extension_id,
    const base::Time& extension_install_time) const {
  CHECK(request_data.stage & stages());
  EventResponseDelta result(extension_id, extension_install_time);
  result.modified_request_headers.SetHeader(name_, value_);
  return result;
}

//
// WebRequestRemoveRequestHeaderAction
//

WebRequestRemoveRequestHeaderAction::WebRequestRemoveRequestHeaderAction(
    const std::string& name)
    : WebRequestAction(ON_BEFORE_SEND_HEADERS,
                       ACTION_REMOVE_REQUEST_HEADER,
                       std::numeric_limits<int>::min(),
                       STRATEGY_DEFAULT),
      name_(name) {}

WebRequestRemoveRequestHeaderAction::~WebRequestRemoveRequestHeaderAction() {}

bool WebRequestRemoveRequestHeaderAction::Equals(
    const WebRequestAction* other) const {
  if (!WebRequestAction::Equals(other))
    return false;
  const WebRequestRemoveRequestHeaderAction* casted_other =
      static_cast<const WebRequestRemoveRequestHeaderAction*>(other);
  return name_ == casted_other->name_;
}

std::string WebRequestRemoveRequestHeaderAction::GetName() const {
  return keys::kRemoveRequestHeaderType;
}

base::Optional<EventResponseDelta>
WebRequestRemoveRequestHeaderAction::CreateDelta(
    const WebRequestData& request_data,
    const std::string& extension_id,
    const base::Time& extension_install_time) const {
  CHECK(request_data.stage & stages());
  EventResponseDelta result(extension_id, extension_install_time);
  result.deleted_request_headers.push_back(name_);
  return result;
}

//
// WebRequestAddResponseHeaderAction
//

WebRequestAddResponseHeaderAction::WebRequestAddResponseHeaderAction(
    const std::string& name,
    const std::string& value)
    : WebRequestAction(ON_HEADERS_RECEIVED,
                       ACTION_ADD_RESPONSE_HEADER,
                       std::numeric_limits<int>::min(),
                       STRATEGY_DEFAULT),
      name_(name),
      value_(value) {}

WebRequestAddResponseHeaderAction::~WebRequestAddResponseHeaderAction() {}

bool WebRequestAddResponseHeaderAction::Equals(
    const WebRequestAction* other) const {
  if (!WebRequestAction::Equals(other))
    return false;
  const WebRequestAddResponseHeaderAction* casted_other =
      static_cast<const WebRequestAddResponseHeaderAction*>(other);
  return name_ == casted_other->name_ && value_ == casted_other->value_;
}

std::string WebRequestAddResponseHeaderAction::GetName() const {
  return keys::kAddResponseHeaderType;
}

base::Optional<EventResponseDelta>
WebRequestAddResponseHeaderAction::CreateDelta(
    const WebRequestData& request_data,
    const std::string& extension_id,
    const base::Time& extension_install_time) const {
  CHECK(request_data.stage & stages());
  const net::HttpResponseHeaders* headers =
      request_data.original_response_headers;
  if (!headers)
    return base::nullopt;

  // Don't generate the header if it exists already.
  if (headers->HasHeaderValue(name_, value_))
    return base::nullopt;

  EventResponseDelta result(extension_id, extension_install_time);
  result.added_response_headers.push_back(make_pair(name_, value_));
  return result;
}

//
// WebRequestRemoveResponseHeaderAction
//

WebRequestRemoveResponseHeaderAction::WebRequestRemoveResponseHeaderAction(
    const std::string& name,
    const std::string& value,
    bool has_value)
    : WebRequestAction(ON_HEADERS_RECEIVED,
                       ACTION_REMOVE_RESPONSE_HEADER,
                       std::numeric_limits<int>::min(),
                       STRATEGY_DEFAULT),
      name_(name),
      value_(value),
      has_value_(has_value) {}

WebRequestRemoveResponseHeaderAction::~WebRequestRemoveResponseHeaderAction() {}

bool WebRequestRemoveResponseHeaderAction::Equals(
    const WebRequestAction* other) const {
  if (!WebRequestAction::Equals(other))
    return false;
  const WebRequestRemoveResponseHeaderAction* casted_other =
      static_cast<const WebRequestRemoveResponseHeaderAction*>(other);
  return name_ == casted_other->name_ && value_ == casted_other->value_ &&
         has_value_ == casted_other->has_value_;
}

std::string WebRequestRemoveResponseHeaderAction::GetName() const {
  return keys::kRemoveResponseHeaderType;
}

base::Optional<EventResponseDelta>
WebRequestRemoveResponseHeaderAction::CreateDelta(
    const WebRequestData& request_data,
    const std::string& extension_id,
    const base::Time& extension_install_time) const {
  CHECK(request_data.stage & stages());
  const net::HttpResponseHeaders* headers =
      request_data.original_response_headers;
  if (!headers)
    return base::nullopt;

  EventResponseDelta result(extension_id, extension_install_time);
  size_t iter = 0;
  std::string current_value;
  while (headers->EnumerateHeader(&iter, name_, &current_value)) {
    if (has_value_ && !base::EqualsCaseInsensitiveASCII(current_value, value_))
      continue;
    result.deleted_response_headers.push_back(make_pair(name_, current_value));
  }
  return result;
}

//
// WebRequestIgnoreRulesAction
//

WebRequestIgnoreRulesAction::WebRequestIgnoreRulesAction(
    int minimum_priority,
    const std::string& ignore_tag)
    : WebRequestAction(ON_BEFORE_REQUEST | ON_BEFORE_SEND_HEADERS |
                           ON_HEADERS_RECEIVED | ON_AUTH_REQUIRED,
                       ACTION_IGNORE_RULES,
                       minimum_priority,
                       STRATEGY_NONE),
      ignore_tag_(ignore_tag) {}

WebRequestIgnoreRulesAction::~WebRequestIgnoreRulesAction() {}

bool WebRequestIgnoreRulesAction::Equals(const WebRequestAction* other) const {
  if (!WebRequestAction::Equals(other))
    return false;
  const WebRequestIgnoreRulesAction* casted_other =
      static_cast<const WebRequestIgnoreRulesAction*>(other);
  return minimum_priority() == casted_other->minimum_priority() &&
         ignore_tag_ == casted_other->ignore_tag_;
}

std::string WebRequestIgnoreRulesAction::GetName() const {
  return keys::kIgnoreRulesType;
}

base::Optional<EventResponseDelta> WebRequestIgnoreRulesAction::CreateDelta(
    const WebRequestData& request_data,
    const std::string& extension_id,
    const base::Time& extension_install_time) const {
  CHECK(request_data.stage & stages());
  return base::nullopt;
}

//
// WebRequestRequestCookieAction
//

WebRequestRequestCookieAction::WebRequestRequestCookieAction(
    RequestCookieModification request_cookie_modification)
    : WebRequestAction(ON_BEFORE_SEND_HEADERS,
                       ACTION_MODIFY_REQUEST_COOKIE,
                       std::numeric_limits<int>::min(),
                       STRATEGY_DEFAULT),
      request_cookie_modification_(std::move(request_cookie_modification)) {}

WebRequestRequestCookieAction::~WebRequestRequestCookieAction() {}

bool WebRequestRequestCookieAction::Equals(
    const WebRequestAction* other) const {
  if (!WebRequestAction::Equals(other))
    return false;
  const WebRequestRequestCookieAction* casted_other =
      static_cast<const WebRequestRequestCookieAction*>(other);
  return request_cookie_modification_ ==
         casted_other->request_cookie_modification_;
}

std::string WebRequestRequestCookieAction::GetName() const {
  switch (request_cookie_modification_.type) {
    case helpers::ADD:
      return keys::kAddRequestCookieType;
    case helpers::EDIT:
      return keys::kEditRequestCookieType;
    case helpers::REMOVE:
      return keys::kRemoveRequestCookieType;
  }
  NOTREACHED();
  return "";
}

base::Optional<EventResponseDelta> WebRequestRequestCookieAction::CreateDelta(
    const WebRequestData& request_data,
    const std::string& extension_id,
    const base::Time& extension_install_time) const {
  CHECK(request_data.stage & stages());
  EventResponseDelta result(extension_id, extension_install_time);
  result.request_cookie_modifications.push_back(
      request_cookie_modification_.Clone());
  return result;
}

//
// WebRequestResponseCookieAction
//

WebRequestResponseCookieAction::WebRequestResponseCookieAction(
    ResponseCookieModification response_cookie_modification)
    : WebRequestAction(ON_HEADERS_RECEIVED,
                       ACTION_MODIFY_RESPONSE_COOKIE,
                       std::numeric_limits<int>::min(),
                       STRATEGY_DEFAULT),
      response_cookie_modification_(std::move(response_cookie_modification)) {}

WebRequestResponseCookieAction::~WebRequestResponseCookieAction() {}

bool WebRequestResponseCookieAction::Equals(
    const WebRequestAction* other) const {
  if (!WebRequestAction::Equals(other))
    return false;
  const WebRequestResponseCookieAction* casted_other =
      static_cast<const WebRequestResponseCookieAction*>(other);
  return response_cookie_modification_ ==
         casted_other->response_cookie_modification_;
}

std::string WebRequestResponseCookieAction::GetName() const {
  switch (response_cookie_modification_.type) {
    case helpers::ADD:
      return keys::kAddResponseCookieType;
    case helpers::EDIT:
      return keys::kEditResponseCookieType;
    case helpers::REMOVE:
      return keys::kRemoveResponseCookieType;
  }
  NOTREACHED();
  return "";
}

base::Optional<EventResponseDelta> WebRequestResponseCookieAction::CreateDelta(
    const WebRequestData& request_data,
    const std::string& extension_id,
    const base::Time& extension_install_time) const {
  CHECK(request_data.stage & stages());
  EventResponseDelta result(extension_id, extension_install_time);
  result.response_cookie_modifications.push_back(
      response_cookie_modification_.Clone());
  return result;
}

//
// WebRequestSendMessageToExtensionAction
//

WebRequestSendMessageToExtensionAction::WebRequestSendMessageToExtensionAction(
    const std::string& message)
    : WebRequestAction(ON_BEFORE_REQUEST | ON_BEFORE_SEND_HEADERS |
                           ON_HEADERS_RECEIVED | ON_AUTH_REQUIRED,
                       ACTION_SEND_MESSAGE_TO_EXTENSION,
                       std::numeric_limits<int>::min(),
                       STRATEGY_HOST),
      message_(message) {}

WebRequestSendMessageToExtensionAction::
~WebRequestSendMessageToExtensionAction() {}

bool WebRequestSendMessageToExtensionAction::Equals(
    const WebRequestAction* other) const {
  if (!WebRequestAction::Equals(other))
    return false;
  const WebRequestSendMessageToExtensionAction* casted_other =
      static_cast<const WebRequestSendMessageToExtensionAction*>(other);
  return message_ == casted_other->message_;
}

std::string WebRequestSendMessageToExtensionAction::GetName() const {
  return keys::kSendMessageToExtensionType;
}

base::Optional<EventResponseDelta>
WebRequestSendMessageToExtensionAction::CreateDelta(
    const WebRequestData& request_data,
    const std::string& extension_id,
    const base::Time& extension_install_time) const {
  CHECK(request_data.stage & stages());
  EventResponseDelta result(extension_id, extension_install_time);
  result.messages_to_extension.insert(message_);
  return result;
}

}  // namespace extensions
