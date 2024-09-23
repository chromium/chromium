// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTO_SESSION_AUTHZ_SERVICE_H_
#define REMOTING_PROTO_SESSION_AUTHZ_SERVICE_H_

#include <optional>
#include <string>

#include "base/time/time.h"
#include "remoting/base/session_policies.h"

// This file defines structs for the SessionAuthzService. For official builds,
// these structs are populated by code in //remoting/internal. For unofficial
// builds, they are populated by code in internal_stubs.h.
namespace remoting::internal {

struct GenerateHostTokenRequestStruct {};

struct GenerateHostTokenResponseStruct {
  GenerateHostTokenResponseStruct();
  ~GenerateHostTokenResponseStruct();

  std::string host_token;
  std::string session_id;
};

struct VerifySessionTokenRequestStruct {
  VerifySessionTokenRequestStruct();
  ~VerifySessionTokenRequestStruct();
  VerifySessionTokenRequestStruct(const VerifySessionTokenRequestStruct&);
  bool operator==(const VerifySessionTokenRequestStruct&) const;

  std::string session_token;
};

struct VerifySessionTokenResponseStruct {
  VerifySessionTokenResponseStruct();
  ~VerifySessionTokenResponseStruct();

  std::string session_id;
  std::string shared_secret;
  std::string session_reauth_token;
  base::TimeDelta session_reauth_token_lifetime;
  std::optional<SessionPolicies> session_policies;
};

struct ReauthorizeHostRequestStruct {
  ReauthorizeHostRequestStruct();
  ~ReauthorizeHostRequestStruct();
  bool operator==(const ReauthorizeHostRequestStruct&) const;

  std::string session_reauth_token;
  std::string session_id;
};

struct ReauthorizeHostResponseStruct {
  ReauthorizeHostResponseStruct();
  ~ReauthorizeHostResponseStruct();

  std::string session_reauth_token;
  base::TimeDelta session_reauth_token_lifetime;
};

}  // namespace remoting::internal

#endif  // REMOTING_PROTO_SESSION_AUTHZ_SERVICE_H_
