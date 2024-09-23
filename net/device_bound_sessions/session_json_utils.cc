// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/device_bound_sessions/session_json_utils.h"

#include "base/json/json_reader.h"

namespace net::device_bound_sessions {

namespace {

SessionParams::Scope ParseScope(const base::Value::Dict& scope_dict) {
  SessionParams::Scope scope;

  std::optional<bool> include_site = scope_dict.FindBool("include_site");
  scope.include_site = include_site.value_or(false);
  const base::Value::List* specifications_list =
      scope_dict.FindList("scope_specification");
  if (!specifications_list) {
    return scope;
  }

  for (const auto& specification : *specifications_list) {
    const base::Value::Dict* specification_dict = specification.GetIfDict();
    if (!specification_dict) {
      continue;
    }

    const std::string* type = specification_dict->FindString("type");
    const std::string* domain = specification_dict->FindString("domain");
    const std::string* path = specification_dict->FindString("path");
    if (type && !type->empty() && domain && !domain->empty() && path &&
        !path->empty()) {
      if (*type == "include") {
        scope.specifications.push_back(SessionParams::Scope::Specification{
            SessionParams::Scope::Specification::Type::kInclude, *domain,
            *path});
      } else if (*type == "exclude") {
        scope.specifications.push_back(SessionParams::Scope::Specification{
            SessionParams::Scope::Specification::Type::kExclude, *domain,
            *path});
      }
    }
  }

  return scope;
}

std::vector<SessionParams::Credential> ParseCredentials(
    const base::Value::List& credentials_list) {
  std::vector<SessionParams::Credential> cookie_credentials;
  for (const auto& json_credential : credentials_list) {
    SessionParams::Credential credential;
    const base::Value::Dict* credential_dict = json_credential.GetIfDict();
    if (!credential_dict) {
      continue;
    }
    const std::string* type = credential_dict->FindString("type");
    if (!type || *type != "cookie") {
      continue;
    }
    const std::string* name = credential_dict->FindString("name");
    const std::string* attributes = credential_dict->FindString("attributes");
    if (name && attributes) {
      cookie_credentials.push_back(
          SessionParams::Credential{*name, *attributes});
    }
  }

  return cookie_credentials;
}

}  // namespace

std::optional<SessionParams> ParseSessionInstructionJson(
    std::string_view response_json) {
  // TODO(kristianm): Skip XSSI-escapes, see for example:
  // https://hg.mozilla.org/mozilla-central/rev/4cee9ec9155e
  // Discuss with others if XSSI should be part of the standard.

  // TODO(kristianm): Decide if the standard should require parsing
  // to fail fully if any item is wrong, or if that item should be
  // ignored.

  std::optional<base::Value::Dict> maybe_root = base::JSONReader::ReadDict(
      response_json, base::JSON_PARSE_RFC, /*max_depth=*/5u);
  if (!maybe_root) {
    return std::nullopt;
  }

  base::Value::Dict* scope_dict = maybe_root->FindDict("scope");

  std::string* session_id = maybe_root->FindString("session_identifier");
  if (!session_id || session_id->empty()) {
    return std::nullopt;
  }

  std::string* refresh_url = maybe_root->FindString("refresh_url");

  std::vector<SessionParams::Credential> credentials;
  base::Value::List* credentials_list = maybe_root->FindList("credentials");
  if (credentials_list) {
    credentials = ParseCredentials(*credentials_list);
  }

  if (credentials.empty()) {
    return std::nullopt;
  }

  return SessionParams(
      *session_id, refresh_url ? *refresh_url : "",
      scope_dict ? ParseScope(*scope_dict) : SessionParams::Scope{},
      std::move(credentials));
}

}  // namespace net::device_bound_sessions
