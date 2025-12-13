// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DEVICE_BOUND_SESSIONS_REGISTRATION_FETCHER_PARAM_H_
#define NET_DEVICE_BOUND_SESSIONS_REGISTRATION_FETCHER_PARAM_H_

#include <string>
#include <vector>

#include "base/containers/span.h"
#include "crypto/signature_verifier.h"
#include "net/base/net_export.h"
#include "net/device_bound_sessions/session.h"
#include "net/http/http_response_headers.h"
#include "net/http/structured_headers.h"
#include "url/gurl.h"

namespace net::device_bound_sessions {

// Class to parse Secure-Session-Registration header.
// See explainer for details:
// https://github.com/WICG/dbsc/blob/main/README.md#start-session
// The header format for the session registration is a list of
// algorithm tokens, the list have two parameters, one is a string
// representing the challenge, the other is a string representing
// the path. Example:
// (RS256 ES256);path="start";challenge="code"
class NET_EXPORT RegistrationFetcherParam {
 public:
  RegistrationFetcherParam(RegistrationFetcherParam&& other);
  RegistrationFetcherParam& operator=(
      RegistrationFetcherParam&& other) noexcept;

  // Disabled to make accidental copies compile errors.
  RegistrationFetcherParam(const RegistrationFetcherParam& other) = delete;
  RegistrationFetcherParam& operator=(const RegistrationFetcherParam&) = delete;
  ~RegistrationFetcherParam();

  // Checks `headers` for any Secure-Session-Registration headers. Parses any
  // valid ones that are found into `RegistrationFetcherParam` instances and
  // returns a vector of these. `request_url` corresponds to the request that
  // returned these headers; it is used to resolve any relative registration
  // endpoints in the response headers and to validate that the scheme is
  // appropriate.
  static std::vector<RegistrationFetcherParam> CreateIfValid(
      const GURL& request_url,
      const HttpResponseHeaders* headers);

  // Convenience constructor for testing.
  static RegistrationFetcherParam CreateInstanceForTesting(
      GURL registration_endpoint,
      std::vector<crypto::SignatureVerifier::SignatureAlgorithm>
          supported_algos,
      std::string challenge,
      std::optional<std::string> authorization,
      std::optional<std::string> provider_key = std::nullopt,
      std::optional<GURL> provider_url = std::nullopt,
      std::optional<Session::Id> provider_session_id = std::nullopt);

  const GURL& registration_endpoint() const { return registration_endpoint_; }

  base::span<const crypto::SignatureVerifier::SignatureAlgorithm>
  supported_algos() const {
    return supported_algos_;
  }

  const std::string& challenge() const { return challenge_; }

  const std::optional<std::string>& authorization() const {
    return authorization_;
  }

  const std::optional<std::string>& provider_key() const {
    return provider_key_;
  }

  const std::optional<GURL>& provider_url() const { return provider_url_; }

  const std::optional<Session::Id>& provider_session_id() const {
    return provider_session_id_;
  }

  GURL TakeRegistrationEndpoint() { return std::move(registration_endpoint_); }

  std::string TakeChallenge() { return std::move(challenge_); }

  std::optional<std::string> TakeAuthorization() {
    return std::move(authorization_);
  }

 private:
  RegistrationFetcherParam(
      GURL registration_endpoint,
      std::vector<crypto::SignatureVerifier::SignatureAlgorithm>
          supported_algos,
      std::string challenge,
      std::optional<std::string> authorization,
      std::optional<std::string> provider_key,
      std::optional<GURL> provider_url,
      std::optional<Session::Id> provider_session_id);

  static std::optional<RegistrationFetcherParam> ParseItem(
      const GURL& request_url,
      const structured_headers::ParameterizedMember& session_registration);

  GURL registration_endpoint_;
  std::vector<crypto::SignatureVerifier::SignatureAlgorithm> supported_algos_;
  std::string challenge_;
  std::optional<std::string> authorization_;
  std::optional<std::string> provider_key_;
  std::optional<GURL> provider_url_;
  std::optional<Session::Id> provider_session_id_;
};

}  // namespace net::device_bound_sessions

#endif  // NET_DEVICE_BOUND_SESSIONS_REGISTRATION_FETCHER_PARAM_H_
