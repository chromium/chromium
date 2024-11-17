// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_AUTHENTICATION_METHOD_H_
#define REMOTING_BASE_AUTHENTICATION_METHOD_H_

#include <string>
#include <string_view>

namespace remoting {

// Method represents an authentication algorithm.
enum class AuthenticationMethod {
  INVALID,

  // SPAKE2 PIN or access code hashed with host_id using HMAC-SHA256.
  SHARED_SECRET_SPAKE2_CURVE25519,

  // SPAKE2 using shared pairing secret.
  PAIRED_SPAKE2_CURVE25519,

  // Authentication using the SessionAuthz service, which generates the
  // shared secret for SPAKE2 key exchange. This authz mode is used for Cloud
  // machines and is incompatible with other forms of SessionAuthz.
  CLOUD_SESSION_AUTHZ_SPAKE2_CURVE25519,

  // Authentication using the SessionAuthz service, which generates the
  // shared secret for SPAKE2 key exchange. This authz mode is used for Corp
  // machines and is incompatible with other forms of SessionAuthz.
  CORP_SESSION_AUTHZ_SPAKE2_CURVE25519,
};

// Parses a string that defines an authentication method. Returns
// Method::INVALID if the string is invalid.
extern AuthenticationMethod ParseAuthenticationMethodString(
    std::string_view value);

// Returns string representation of |method|.
extern std::string AuthenticationMethodToString(AuthenticationMethod method);

}  // namespace remoting

#endif  // REMOTING_BASE_AUTHENTICATION_METHOD_H_
