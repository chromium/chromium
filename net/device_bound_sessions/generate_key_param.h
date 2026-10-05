// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DEVICE_BOUND_SESSIONS_GENERATE_KEY_PARAM_H_
#define NET_DEVICE_BOUND_SESSIONS_GENERATE_KEY_PARAM_H_

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "crypto/sign.h"
#include "net/base/net_export.h"
#include "net/device_bound_sessions/session.h"
#include "url/origin.h"

namespace net::device_bound_sessions {

// Represents the parsed Secure-Session-GenerateKey header value.
// See https://github.com/WICG/dbsc-sso#dbsc-key-generation-header.
struct NET_EXPORT GenerateKeyParam {
  // Parses `header_value` as a structured header list. Returns std::nullopt if
  // it is malformed, if none of the listed algorithms is supported, if a
  // required parameter is missing, empty, or not a string, or if
  // `target_origin` is not a secure origin.
  //
  // Unlike Secure-Session-Registration, the list must have exactly one member.
  // Multiple members are rejected, including those produced by joining
  // repeated header lines.
  static std::optional<GenerateKeyParam> Parse(std::string_view header_value);

  friend bool operator==(const GenerateKeyParam&,
                         const GenerateKeyParam&) = default;

  // In the server's order of preference.
  std::vector<crypto::sign::SignatureKind> supported_algos;
  // The RP origin that the generated key is restricted to.
  url::Origin target_origin;
  // The IdP session whose attestation key must certify the generated key.
  // Unlike `ProviderRegistrationParams::provider_session_id`, this is required.
  Session::Id provider_session_id;
  // The challenge to include in the attestation of the generated key.
  std::string challenge;
};

}  // namespace net::device_bound_sessions

#endif  // NET_DEVICE_BOUND_SESSIONS_GENERATE_KEY_PARAM_H_
