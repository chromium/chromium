// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/authentication_method.h"

#include "remoting/base/name_value_map.h"

namespace remoting {

namespace {

const NameMapElement<AuthenticationMethod> kAuthenticationMethodStrings[] = {
    {AuthenticationMethod::SHARED_SECRET_SPAKE2_CURVE25519,
     "spake2_curve25519"},
    {AuthenticationMethod::PAIRED_SPAKE2_CURVE25519, "pair_spake2_curve25519"},
    {AuthenticationMethod::CLOUD_SESSION_AUTHZ_SPAKE2_CURVE25519,
     "cloud_session_authz_spake2_curve25519"},
    {AuthenticationMethod::CORP_SESSION_AUTHZ_SPAKE2_CURVE25519,
     "corp_session_authz_spake2_curve25519"},
};

}  // namespace

AuthenticationMethod ParseAuthenticationMethodString(std::string_view value) {
  AuthenticationMethod result;
  if (!NameToValue(kAuthenticationMethodStrings, value, &result)) {
    return AuthenticationMethod::INVALID;
  }
  return result;
}

std::string AuthenticationMethodToString(AuthenticationMethod method) {
  return ValueToName(kAuthenticationMethodStrings, method);
}

}  // namespace remoting
