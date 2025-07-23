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
  struct Credential {
    struct Scope {
      friend bool operator==(const Scope&, const Scope&) = default;

      std::string domain;
      std::string path;
    };

    friend bool operator==(const Credential&, const Credential&) = default;

    std::string name;
    std::string type;
    Scope scope;
  };

  enum class ParserError {
    kRequiredFieldMissing,
    kRequiredCredentialFieldMissing,
  };

  // Parses the payload from the JSON response. It returns an error if any of
  // the required fields are missing.
  static base::expected<RegisterBoundSessionPayload, ParserError> ParseFromJson(
      const base::Value::Dict& dict);

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
  std::vector<Credential> credentials;
};

#endif  // GOOGLE_APIS_GAIA_REGISTER_BOUND_SESSION_PAYLOAD_H_
