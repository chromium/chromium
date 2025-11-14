// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_GAIA_REGISTER_BOUND_SESSION_PAYLOAD_H_
#define GOOGLE_APIS_GAIA_REGISTER_BOUND_SESSION_PAYLOAD_H_

#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/types/expected.h"
#include "base/values.h"

// Represents the payload of the bound session registration response.
//
// `RegisterBoundSessionPayload::ParseFromJson` can be used to parse it from the
// JSON response.
struct COMPONENT_EXPORT(GOOGLE_APIS) RegisterBoundSessionPayload {
  // Represents the scope (specification) that should be applied to either a
  // credential (prototype) or an entire session (standard).
  struct Scope {
    enum class Type {
      kExclude,
      kInclude,
    };

    friend bool operator==(const Scope&, const Scope&) = default;

    std::string domain;
    std::string path;
    Type type = Type::kExclude;
  };

  struct SessionScope {
    SessionScope();
    ~SessionScope();

    SessionScope(const SessionScope& other);
    SessionScope& operator=(const SessionScope& other);

    SessionScope(SessionScope&& other);
    SessionScope& operator=(SessionScope&& other);

    friend bool operator==(const SessionScope&, const SessionScope&) = default;

    std::string origin;
    bool include_site = false;
    std::vector<Scope> specifications;
  };

  struct COMPONENT_EXPORT(GOOGLE_APIS) Credential {
    Credential();
    ~Credential();

    Credential(const Credential& other);
    Credential& operator=(const Credential& other);

    Credential(Credential&& other);
    Credential& operator=(Credential&& other);

    friend bool operator==(const Credential&, const Credential&) = default;

    std::string name;
    std::string type;
    Scope scope;
    std::string attributes;
  };

  enum class ParserError {
    kRequiredFieldMissing,
    kRequiredCredentialFieldMissing,
    kRequiredScopeFieldMissing,
    kMalformedRefreshInitiator,
    kMalformedSessionScopeSpecification,
    kInvalidScopeType,
    kInvalidCredentialType,
  };

  // Parses the payload from the JSON response. It returns an error if any of
  // the required fields are missing.
  // `parse_for_dbsc_standard` indicates whether the response is expected to
  // contain the DBSC standard session(s) format. If set to `false`, the DBSC
  // prototype session(s) format will be used to parse the response.
  //
  // TODO(crbug.com/457814683): Unify the payload parsing logic with `//net`.
  static base::expected<RegisterBoundSessionPayload, ParserError> ParseFromJson(
      const base::Value::Dict& dict,
      bool parse_for_dbsc_standard);

  RegisterBoundSessionPayload();
  ~RegisterBoundSessionPayload();

  RegisterBoundSessionPayload(const RegisterBoundSessionPayload& other) =
      delete;
  RegisterBoundSessionPayload& operator=(
      const RegisterBoundSessionPayload& other) = delete;

  RegisterBoundSessionPayload(RegisterBoundSessionPayload&& other);
  RegisterBoundSessionPayload& operator=(RegisterBoundSessionPayload&& other);

  friend bool operator==(const RegisterBoundSessionPayload&,
                         const RegisterBoundSessionPayload&) = default;

  std::string session_id;
  std::string refresh_url;
  SessionScope scope;
  std::vector<Credential> credentials;
  std::vector<std::string> allowed_refresh_initiators;
  // Indicates whether the payload was parsed for DBSC standard format. If set
  // to `false`, the payload was parsed for DBSC prototype format.
  bool parsed_for_dbsc_standard = false;
};

#endif  // GOOGLE_APIS_GAIA_REGISTER_BOUND_SESSION_PAYLOAD_H_
