// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/device_bound_sessions/session_json_utils.h"

#include "base/json/json_reader.h"
#include "base/types/expected_macros.h"

namespace net::device_bound_sessions {

namespace {

base::expected<SessionParams::Scope, SessionError> ParseScope(
    const base::Value::Dict& scope_dict) {
  SessionParams::Scope scope;

  std::optional<bool> include_site = scope_dict.FindBool("include_site");
  scope.include_site = include_site.value_or(false);
  const std::string* origin = scope_dict.FindString("origin");
  scope.origin = origin ? *origin : "";
  const base::Value::List* specifications_list =
      scope_dict.FindList("scope_specification");
  if (!specifications_list) {
    return scope;
  }

  for (const auto& specification : *specifications_list) {
    const base::Value::Dict* specification_dict = specification.GetIfDict();
    if (!specification_dict) {
      return base::unexpected(
          SessionError{SessionError::ErrorType::kInvalidScopeRule});
    }

    const std::string* type = specification_dict->FindString("type");
    const std::string* domain = specification_dict->FindString("domain");
    const std::string* path = specification_dict->FindString("path");
    if (!type || !domain || domain->empty() || !path || path->empty()) {
      return base::unexpected(
          SessionError{SessionError::ErrorType::kInvalidScopeRule});
    }
    SessionParams::Scope::Specification::Type rule_type =
        SessionParams::Scope::Specification::Type::kInclude;
    if (*type == "include") {
      rule_type = SessionParams::Scope::Specification::Type::kInclude;
    } else if (*type == "exclude") {
      rule_type = SessionParams::Scope::Specification::Type::kExclude;
    } else {
      return base::unexpected(
          SessionError{SessionError::ErrorType::kInvalidScopeRule});
    }

    scope.specifications.push_back(
        SessionParams::Scope::Specification{rule_type, *domain, *path});
  }

  return scope;
}

base::expected<std::vector<SessionParams::Credential>, SessionError>
ParseCredentials(const base::Value::List& credentials_list) {
  std::vector<SessionParams::Credential> cookie_credentials;
  for (const auto& json_credential : credentials_list) {
    SessionParams::Credential credential;
    const base::Value::Dict* credential_dict = json_credential.GetIfDict();
    if (!credential_dict) {
      return base::unexpected(
          SessionError{SessionError::ErrorType::kInvalidCredentials});
    }
    const std::string* type = credential_dict->FindString("type");
    if (!type || *type != "cookie") {
      return base::unexpected(
          SessionError{SessionError::ErrorType::kInvalidCredentials});
    }
    const std::string* name = credential_dict->FindString("name");
    const std::string* attributes = credential_dict->FindString("attributes");
    if (!name || !attributes) {
      return base::unexpected(
          SessionError{SessionError::ErrorType::kInvalidCredentials});
    }

    cookie_credentials.push_back(SessionParams::Credential{*name, *attributes});
  }

  return cookie_credentials;
}

}  // namespace

base::expected<SessionParams, SessionError> ParseSessionInstructionJson(
    GURL fetcher_url,
    unexportable_keys::UnexportableKeyId key_id,
    std::optional<std::string> expected_session_id,
    std::string_view response_json) {
  std::optional<base::Value::Dict> maybe_root = base::JSONReader::ReadDict(
      response_json, base::JSON_PARSE_RFC, /*max_depth=*/5u);
  if (!maybe_root) {
    return base::unexpected(
        SessionError{SessionError::ErrorType::kInvalidConfigJson});
  }

  std::string* session_id = maybe_root->FindString("session_identifier");
  if (!session_id || session_id->empty()) {
    return base::unexpected(
        SessionError{SessionError::ErrorType::kInvalidSessionId});
  }

  if (expected_session_id.has_value() && *expected_session_id != *session_id) {
    return base::unexpected(
        SessionError{SessionError::ErrorType::kMismatchedSessionId});
  }

  std::optional<bool> continue_value = maybe_root->FindBool("continue");
  if (continue_value.has_value() && *continue_value == false) {
    return base::unexpected(
        SessionError{SessionError::ErrorType::kServerRequestedTermination});
  }

  base::Value::Dict* scope_dict = maybe_root->FindDict("scope");
  if (!scope_dict) {
    return base::unexpected(
        SessionError{SessionError::ErrorType::kMissingScope});
  }
  ASSIGN_OR_RETURN(SessionParams::Scope scope, ParseScope(*scope_dict));

  std::string* refresh_url = maybe_root->FindString("refresh_url");

  std::vector<SessionParams::Credential> credentials;
  base::Value::List* credentials_list = maybe_root->FindList("credentials");

  if (credentials_list) {
    ASSIGN_OR_RETURN(credentials, ParseCredentials(*credentials_list));
  }

  if (credentials.empty()) {
    return base::unexpected(
        SessionError{SessionError::ErrorType::kNoCredentials});
  }

  std::vector<std::string> allowed_refresh_initiators;
  if (base::Value::List* initiator_list =
          maybe_root->FindList("allowed_refresh_initiators");
      initiator_list) {
    for (base::Value& initiator : *initiator_list) {
      if (!initiator.is_string()) {
        return base::unexpected(
            SessionError{SessionError::ErrorType::kInvalidRefreshInitiators});
      }

      allowed_refresh_initiators.emplace_back(std::move(initiator.GetString()));
    }
  }

  return SessionParams(*session_id, fetcher_url,
                       refresh_url ? *refresh_url : "", std::move(scope),
                       std::move(credentials), key_id,
                       std::move(allowed_refresh_initiators));
}

}  // namespace net::device_bound_sessions
