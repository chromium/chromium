// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gaia/register_bound_session_payload.h"

#include "base/types/expected_macros.h"

namespace {

using enum ::RegisterBoundSessionPayload::ParserError;

base::expected<std::vector<RegisterBoundSessionPayload::Credential>,
               RegisterBoundSessionPayload::ParserError>
ParseCredentials(const base::Value::List& credentials_list) {
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
    const base::Value::Dict* scope = credential_dict->FindDict("scope");
    if (!scope) {
      return base::unexpected(kRequiredCredentialFieldMissing);
    }
    const std::string* domain = scope->FindString("domain");
    if (!domain || domain->empty()) {
      return base::unexpected(kRequiredCredentialFieldMissing);
    }
    credential.scope.domain = *domain;
    const std::string* path = scope->FindString("path");
    if (!path || path->empty()) {
      return base::unexpected(kRequiredCredentialFieldMissing);
    }
    credential.scope.path = *path;
    const std::string* type = credential_dict->FindString("type");
    if (type) {
      credential.type = *type;
    }
    credentials.push_back(std::move(credential));
  }
  return std::move(credentials);
}

}  // namespace

RegisterBoundSessionPayload::RegisterBoundSessionPayload() = default;
RegisterBoundSessionPayload::~RegisterBoundSessionPayload() = default;

RegisterBoundSessionPayload::RegisterBoundSessionPayload(
    RegisterBoundSessionPayload&& other) = default;
RegisterBoundSessionPayload& RegisterBoundSessionPayload::operator=(
    RegisterBoundSessionPayload&& other) = default;

base::expected<RegisterBoundSessionPayload,
               RegisterBoundSessionPayload::ParserError>
RegisterBoundSessionPayload::ParseFromJson(const base::Value::Dict& dict) {
  RegisterBoundSessionPayload payload;
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

  ASSIGN_OR_RETURN(payload.credentials, ParseCredentials(*credentials_list));
  return payload;
}
