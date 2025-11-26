// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/device_bound_sessions/session_json_utils.h"

#include "base/feature_list.h"
#include "base/json/json_reader.h"
#include "base/types/expected_macros.h"
#include "net/base/features.h"

namespace net::device_bound_sessions {

namespace {

std::string FindStringWithDefault(const base::Value::Dict& dict,
                                  std::string_view key,
                                  std::string_view default_value) {
  const std::string* value = dict.FindString(key);
  if (value) {
    return *value;
  }

  return std::string(default_value);
}

base::expected<SessionParams::Scope, SessionError> ParseScope(
    const base::Value::Dict& scope_dict) {
  SessionParams::Scope scope;

  std::optional<bool> include_site = scope_dict.FindBool("include_site");
  if (!include_site.has_value()) {
    return base::unexpected{
        SessionError{SessionError::kMissingScopeIncludeSite}};
  }
  scope.include_site = *include_site;
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
          SessionError{SessionError::kInvalidScopeSpecification});
    }

    const std::string* type = specification_dict->FindString("type");
    if (!type) {
      return base::unexpected(
          SessionError{SessionError::kMissingScopeSpecificationType});
    }
    std::string domain =
        FindStringWithDefault(*specification_dict, "domain", "*");
    if (domain.empty()) {
      return base::unexpected(
          SessionError{SessionError::kEmptyScopeSpecificationDomain});
    }
    std::string path = FindStringWithDefault(*specification_dict, "path", "/");
    if (path.empty()) {
      return base::unexpected(
          SessionError{SessionError::kEmptyScopeSpecificationPath});
    }
    SessionParams::Scope::Specification::Type rule_type =
        SessionParams::Scope::Specification::Type::kInclude;
    if (*type == "include") {
      rule_type = SessionParams::Scope::Specification::Type::kInclude;
    } else if (*type == "exclude") {
      rule_type = SessionParams::Scope::Specification::Type::kExclude;
    } else {
      return base::unexpected(
          SessionError{SessionError::kInvalidScopeSpecificationType});
    }

    scope.specifications.push_back(SessionParams::Scope::Specification{
        rule_type, std::move(domain), std::move(path)});
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
          SessionError{SessionError::kInvalidCredentialsConfig});
    }
    const std::string* type = credential_dict->FindString("type");
    if (!type || *type != "cookie") {
      return base::unexpected(
          SessionError{SessionError::kInvalidCredentialsType});
    }
    const std::string* name = credential_dict->FindString("name");
    if (!name || name->empty()) {
      return base::unexpected(
          SessionError{SessionError::kInvalidCredentialsEmptyName});
    }
    std::string attributes =
        FindStringWithDefault(*credential_dict, "attributes", "");

    cookie_credentials.push_back(
        SessionParams::Credential{*name, std::move(attributes)});
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
    return base::unexpected(SessionError{SessionError::kInvalidConfigJson});
  }

  std::string* session_id = maybe_root->FindString("session_identifier");
  if (!session_id || session_id->empty()) {
    return base::unexpected(SessionError{SessionError::kInvalidSessionId});
  }

  if (expected_session_id.has_value() && *expected_session_id != *session_id) {
    return base::unexpected(SessionError{SessionError::kMismatchedSessionId});
  }

  std::optional<bool> continue_value = maybe_root->FindBool("continue");
  if (continue_value.has_value() && *continue_value == false) {
    return base::unexpected(
        SessionError{SessionError::kServerRequestedTermination});
  }

  base::Value::Dict* scope_dict = maybe_root->FindDict("scope");
  if (!scope_dict) {
    return base::unexpected(SessionError{SessionError::kMissingScope});
  }
  ASSIGN_OR_RETURN(SessionParams::Scope scope, ParseScope(*scope_dict));

  std::string* refresh_url = maybe_root->FindString("refresh_url");

  std::vector<SessionParams::Credential> credentials;
  base::Value::List* credentials_list = maybe_root->FindList("credentials");

  if (credentials_list) {
    ASSIGN_OR_RETURN(credentials, ParseCredentials(*credentials_list));
  }

  if (credentials.empty()) {
    return base::unexpected(SessionError{SessionError::kNoCredentials});
  }

  std::vector<std::string> allowed_refresh_initiators;
  if (base::Value::List* initiator_list =
          maybe_root->FindList("allowed_refresh_initiators");
      initiator_list) {
    for (base::Value& initiator : *initiator_list) {
      if (!initiator.is_string()) {
        return base::unexpected(
            SessionError{SessionError::kRefreshInitiatorNotString});
      }

      allowed_refresh_initiators.emplace_back(std::move(initiator.GetString()));
    }
  }

  return SessionParams(*session_id, fetcher_url,
                       refresh_url ? *refresh_url : "", std::move(scope),
                       std::move(credentials), key_id,
                       std::move(allowed_refresh_initiators));
}

std::optional<WellKnownParams> ParseWellKnownJson(
    std::string_view response_json) {
  std::optional<base::Value::Dict> maybe_root = base::JSONReader::ReadDict(
      response_json, base::JSON_PARSE_RFC, /*max_depth=*/5u);
  if (!maybe_root) {
    return std::nullopt;
  }

  WellKnownParams params;
  const base::Value* registering_origins =
      maybe_root->Find("registering_origins");
  if (registering_origins) {
    const base::Value::List* registering_origins_list =
        registering_origins->GetIfList();
    if (!registering_origins_list) {
      return std::nullopt;
    }
    std::vector<std::string> registering_origin_strings;
    registering_origin_strings.reserve(registering_origins_list->size());
    for (const auto& registering_origin : *registering_origins_list) {
      const std::string* registering_origin_string =
          registering_origin.GetIfString();
      if (!registering_origin_string) {
        return std::nullopt;
      }

      registering_origin_strings.push_back(*registering_origin_string);
    }
    params.registering_origins = std::move(registering_origin_strings);
  }

  const base::Value* relying_origins = maybe_root->Find("relying_origins");
  if (relying_origins) {
    const base::Value::List* relying_origins_list =
        relying_origins->GetIfList();
    if (!relying_origins_list) {
      return std::nullopt;
    }
    std::vector<std::string> relying_origin_strings;
    relying_origin_strings.reserve(relying_origins_list->size());
    for (const auto& relying_origin : *relying_origins_list) {
      const std::string* relying_origin_string = relying_origin.GetIfString();
      if (!relying_origin_string) {
        return std::nullopt;
      }

      relying_origin_strings.push_back(*relying_origin_string);
    }
    params.relying_origins = std::move(relying_origin_strings);
  }

  const base::Value* provider_origin = maybe_root->Find("provider_origin");
  if (provider_origin) {
    const std::string* provider_origin_string = provider_origin->GetIfString();
    if (!provider_origin_string) {
      return std::nullopt;
    }
    params.provider_origin = *provider_origin_string;
  }

  return params;
}

}  // namespace net::device_bound_sessions
