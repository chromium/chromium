// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DEVICE_BOUND_SESSIONS_REGISTRATION_FETCHER_PARAM_H_
#define NET_DEVICE_BOUND_SESSIONS_REGISTRATION_FETCHER_PARAM_H_

#include <string>
#include <vector>

#include "base/containers/span.h"
#include "crypto/sign.h"
#include "net/base/net_export.h"
#include "net/device_bound_sessions/session.h"
#include "net/device_bound_sessions/session_params.h"
#include "net/http/http_response_headers.h"
#include "net/http/structured_headers.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace net::device_bound_sessions {

// Encapsulates parameters specific to a session registration initiated by an
// Identity Provider (IdP). This can be used for:
// - Single Sign-On (SSO): where the IdP pre-generates an attested key for a
//   Relying Party (RP).
// - Federated sessions: where the RP reuses the same key and session from the
//   IdP.
struct NET_EXPORT ProviderRegistrationParams {
  std::string provider_key;
  GURL provider_url;
  // `provider_session_id` is populated only during federated session
  // registrations when an RP reuses the same key as the IdP. It should be
  // empty for DBSC-SSO flows when an IdP pre-generates an attested key for
  // an RP.
  std::optional<Session::Id> provider_session_id;

  bool operator==(const ProviderRegistrationParams&) const = default;
};

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
      const HttpResponseHeaders* headers,
      const std::vector<SchemefulSite>& restricted_sites);

  // Convenience constructor for testing.
  static RegistrationFetcherParam CreateInstanceForTesting(
      GURL registration_endpoint,
      std::vector<crypto::sign::SignatureKind> supported_algos,
      std::optional<std::string> challenge,
      std::optional<std::string> authorization,
      std::optional<ProviderRegistrationParams> provider_params = std::nullopt,
      AttestationMode attestation_mode = AttestationMode::kNone,
      std::optional<url::Origin> maybe_referring_origin = std::nullopt);

  const GURL& registration_endpoint() const { return registration_endpoint_; }

  // The origin of the response that supplied the registration header. May
  // differ from the origin of `registration_endpoint()` when the header
  // specified a different origin within the same site.
  const url::Origin& referring_origin() const { return referring_origin_; }

  base::span<const crypto::sign::SignatureKind> supported_algos() const {
    return supported_algos_;
  }

  const std::optional<std::string>& challenge() const { return challenge_; }

  const std::optional<std::string>& authorization() const {
    return authorization_;
  }

  const std::optional<ProviderRegistrationParams>& provider_params() const {
    return provider_params_;
  }

  AttestationMode attestation_mode() const { return attestation_mode_; }

  GURL TakeRegistrationEndpoint() { return std::move(registration_endpoint_); }
  url::Origin TakeReferringOrigin() { return std::move(referring_origin_); }

  std::optional<std::string> TakeChallenge() { return std::move(challenge_); }

  std::optional<std::string> TakeAuthorization() {
    return std::move(authorization_);
  }

 private:
  RegistrationFetcherParam(
      GURL registration_endpoint,
      url::Origin referring_origin,
      std::vector<crypto::sign::SignatureKind> supported_algos,
      std::optional<std::string> challenge,
      std::optional<std::string> authorization,
      std::optional<ProviderRegistrationParams> provider_params,
      AttestationMode attestation_mode);

  static std::optional<RegistrationFetcherParam> ParseItem(
      const GURL& request_url,
      const structured_headers::ParameterizedMember& session_registration);

  GURL registration_endpoint_;
  url::Origin referring_origin_;
  std::vector<crypto::sign::SignatureKind> supported_algos_;
  std::optional<std::string> challenge_;
  std::optional<std::string> authorization_;
  std::optional<ProviderRegistrationParams> provider_params_;
  AttestationMode attestation_mode_ = AttestationMode::kNone;
};

}  // namespace net::device_bound_sessions

#endif  // NET_DEVICE_BOUND_SESSIONS_REGISTRATION_FETCHER_PARAM_H_
