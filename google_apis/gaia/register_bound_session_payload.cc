// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gaia/register_bound_session_payload.h"

#include "base/types/expected_macros.h"

namespace {

using enum ::RegisterBoundSessionPayload::ParserError;

base::expected<RegisterBoundSessionPayload::Scope::Type,
               RegisterBoundSessionPayload::ParserError>
ParseScopeType(std::string_view type) {
  if (type == "exclude") {
    return RegisterBoundSessionPayload::Scope::Type::kExclude;
  }
  if (type == "include") {
    return RegisterBoundSessionPayload::Scope::Type::kInclude;
  }
  return base::unexpected(kInvalidScopeType);
}

base::expected<RegisterBoundSessionPayload::Scope,
               RegisterBoundSessionPayload::ParserError>
ParseSessionScopeSpecification(const base::Value::Dict& dict) {
  RegisterBoundSessionPayload::Scope scope;

  const std::string* domain = dict.FindString("domain");
  scope.domain = domain ? *domain : "*";

  const std::string* path = dict.FindString("path");
  scope.path = path ? *path : "/";

  const std::string* type = dict.FindString("type");
  if (!type) {
    return base::unexpected(kRequiredScopeFieldMissing);
  }
  ASSIGN_OR_RETURN(scope.type, ParseScopeType(*type));

  return scope;
}

base::expected<RegisterBoundSessionPayload::Scope,
               RegisterBoundSessionPayload::ParserError>
ParseCredentialScope(const base::Value::Dict& dict) {
  RegisterBoundSessionPayload::Scope scope;

  const std::string* domain = dict.FindString("domain");
  if (!domain || domain->empty()) {
    return base::unexpected(kRequiredScopeFieldMissing);
  }
  scope.domain = *domain;
  const std::string* path = dict.FindString("path");
  if (!path || path->empty()) {
    return base::unexpected(kRequiredScopeFieldMissing);
  }
  scope.path = *path;

  return scope;
}

base::expected<std::vector<RegisterBoundSessionPayload::Credential>,
               RegisterBoundSessionPayload::ParserError>
ParseCredentials(const base::Value::List& credentials_list,
                 bool parse_for_dbsc_standard) {
  std::vector<RegisterBoundSessionPayload::Credential> credentials;
  for (const base::Value& credential_val : credentials_list) {
    const base::Value::Dict* credential_dict = credential_val.GetIfDict();
    if (!credential_dict) {
      continue;
    }

    RegisterBoundSessionPayload::Credential credential;

    const std::string* name = credential_dict->FindString("name");
    if (!name || name->empty()) {
      return base::unexpected(kRequiredCredentialFieldMissing);
    }
    credential.name = *name;

    if (parse_for_dbsc_standard) {
      const std::string* attributes = credential_dict->FindString("attributes");
      if (attributes) {
        credential.attributes = *attributes;
      }

      const std::string* type = credential_dict->FindString("type");
      if (!type) {
        return base::unexpected(kRequiredCredentialFieldMissing);
      }
      if (*type != "cookie") {
        return base::unexpected(kInvalidCredentialType);
      }
      credential.type = *type;
    } else {
      const base::Value::Dict* scope = credential_dict->FindDict("scope");
      if (!scope) {
        return base::unexpected(kRequiredCredentialFieldMissing);
      }
      ASSIGN_OR_RETURN(credential.scope, ParseCredentialScope(*scope));
    }

    credentials.push_back(std::move(credential));
  }

  return std::move(credentials);
}

base::expected<std::vector<std::string>,
               RegisterBoundSessionPayload::ParserError>
ParseAllowedRefreshInitiators(const base::Value::List& list) {
  std::vector<std::string> allowed_refresh_initiators;
  for (const base::Value& allowed_refresh_initiator_value : list) {
    const std::string* allowed_refresh_initiator =
        allowed_refresh_initiator_value.GetIfString();
    if (!allowed_refresh_initiator) {
      return base::unexpected(kMalformedRefreshInitiator);
    }
    allowed_refresh_initiators.push_back(*allowed_refresh_initiator);
  }
  return std::move(allowed_refresh_initiators);
}

base::expected<RegisterBoundSessionPayload::SessionScope,
               RegisterBoundSessionPayload::ParserError>
ParseSessionScope(const base::Value::Dict& dict) {
  RegisterBoundSessionPayload::SessionScope scope;

  const std::string* origin = dict.FindString("origin");
  if (origin) {
    scope.origin = *origin;
  }

  const std::optional<bool> include_site = dict.FindBool("include_site");
  if (include_site.has_value()) {
    scope.include_site = *include_site;
  }

  const base::Value::List* specifications =
      dict.FindList("scope_specification");
  if (!specifications) {
    return scope;
  }
  for (const base::Value& specification_val : *specifications) {
    const base::Value::Dict* specification_dict = specification_val.GetIfDict();
    if (!specification_dict) {
      return base::unexpected(kMalformedSessionScopeSpecification);
    }
    ASSIGN_OR_RETURN(scope.specifications.emplace_back(),
                     ParseSessionScopeSpecification(*specification_dict));
  }

  return scope;
}

}  // namespace

RegisterBoundSessionPayload::SessionScope::SessionScope() = default;
RegisterBoundSessionPayload::SessionScope::~SessionScope() = default;

RegisterBoundSessionPayload::SessionScope::SessionScope(
    const SessionScope& other) = default;
RegisterBoundSessionPayload::SessionScope&
RegisterBoundSessionPayload::SessionScope::operator=(
    const SessionScope& other) = default;

RegisterBoundSessionPayload::SessionScope::SessionScope(SessionScope&& other) =
    default;
RegisterBoundSessionPayload::SessionScope&
RegisterBoundSessionPayload::SessionScope::operator=(SessionScope&& other) =
    default;

RegisterBoundSessionPayload::Credential::Credential() = default;
RegisterBoundSessionPayload::Credential::~Credential() = default;

RegisterBoundSessionPayload::Credential::Credential(const Credential& other) =
    default;
RegisterBoundSessionPayload::Credential&
RegisterBoundSessionPayload::Credential::operator=(const Credential& other) =
    default;

RegisterBoundSessionPayload::Credential::Credential(Credential&& other) =
    default;
RegisterBoundSessionPayload::Credential&
RegisterBoundSessionPayload::Credential::operator=(Credential&& other) =
    default;

RegisterBoundSessionPayload::RegisterBoundSessionPayload() = default;
RegisterBoundSessionPayload::~RegisterBoundSessionPayload() = default;

RegisterBoundSessionPayload::RegisterBoundSessionPayload(
    RegisterBoundSessionPayload&& other) = default;
RegisterBoundSessionPayload& RegisterBoundSessionPayload::operator=(
    RegisterBoundSessionPayload&& other) = default;

base::expected<RegisterBoundSessionPayload,
               RegisterBoundSessionPayload::ParserError>
RegisterBoundSessionPayload::ParseFromJson(const base::Value::Dict& dict,
                                           bool parse_for_dbsc_standard) {
  RegisterBoundSessionPayload payload;
  payload.parsed_for_dbsc_standard = parse_for_dbsc_standard;

  const std::string* session_id = dict.FindString("session_identifier");
  if (!session_id || session_id->empty()) {
    return base::unexpected(kRequiredFieldMissing);
  }

  payload.session_id = *session_id;
  const std::string* refresh_url = dict.FindString("refresh_url");
  if (!refresh_url || refresh_url->empty()) {
    return base::unexpected(kRequiredFieldMissing);
  }

  payload.refresh_url = *refresh_url;
  const base::Value::List* credentials_list = dict.FindList("credentials");
  if (!credentials_list || credentials_list->empty()) {
    return base::unexpected(kRequiredFieldMissing);
  }

  ASSIGN_OR_RETURN(
      payload.credentials,
      ParseCredentials(*credentials_list, parse_for_dbsc_standard));

  if (parse_for_dbsc_standard) {
    const base::Value::Dict* scope = dict.FindDict("scope");
    if (!scope) {
      return base::unexpected(kRequiredFieldMissing);
    }
    ASSIGN_OR_RETURN(payload.scope, ParseSessionScope(*scope));

    const base::Value::List* allowed_refresh_initiators_list =
        dict.FindList("allowed_refresh_initiators");
    if (allowed_refresh_initiators_list) {
      ASSIGN_OR_RETURN(
          payload.allowed_refresh_initiators,
          ParseAllowedRefreshInitiators(*allowed_refresh_initiators_list));
    }
  }

  return payload;
}
