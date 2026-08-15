// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/device_bound_sessions/registration_request_param.h"

#include "net/device_bound_sessions/registration_fetcher_param.h"
#include "net/device_bound_sessions/session.h"

namespace net::device_bound_sessions {

RegistrationRequestParam::RegistrationRequestParam(
    const RegistrationRequestParam& other) = default;
RegistrationRequestParam& RegistrationRequestParam::operator=(
    const RegistrationRequestParam& other) = default;

RegistrationRequestParam::RegistrationRequestParam(
    RegistrationRequestParam&&) noexcept = default;
RegistrationRequestParam& RegistrationRequestParam::operator=(
    RegistrationRequestParam&&) noexcept = default;

RegistrationRequestParam::~RegistrationRequestParam() = default;

// static
RegistrationRequestParam RegistrationRequestParam::CreateForRegistration(
    RegistrationFetcherParam&& fetcher_param) {
  return RegistrationRequestParam(
      fetcher_param.TakeRegistrationEndpoint(),
      fetcher_param.TakeReferringOrigin(),
      /*session_identifier=*/std::nullopt, fetcher_param.TakeChallenge(),
      fetcher_param.TakeAuthorization(), fetcher_param.attestation_mode());
}

// static
RegistrationRequestParam RegistrationRequestParam::CreateForRefresh(
    const Session& session) {
  return RegistrationRequestParam(
      session.refresh_url(), session.origin(), session.id().value(),
      session.cached_challenge(),
      /*authorization=*/std::nullopt, AttestationMode::kNone);
}

// static
RegistrationRequestParam RegistrationRequestParam::CreateForTesting(
    const GURL& registration_endpoint,
    std::optional<std::string> session_identifier,
    std::optional<std::string> challenge,
    std::optional<std::string> authorization,
    AttestationMode attestation_mode,
    std::optional<url::Origin> maybe_referring_origin) {
  url::Origin referring_origin =
      maybe_referring_origin ? std::move(*maybe_referring_origin)
                             : url::Origin::Create(registration_endpoint);
  return RegistrationRequestParam(
      registration_endpoint, std::move(referring_origin),
      std::move(session_identifier), std::move(challenge),
      std::move(authorization), attestation_mode);
}

RegistrationRequestParam::RegistrationRequestParam(
    const GURL& registration_endpoint,
    url::Origin referring_origin,
    std::optional<std::string> session_identifier,
    std::optional<std::string> challenge,
    std::optional<std::string> authorization,
    AttestationMode attestation_mode)
    : registration_endpoint_(registration_endpoint),
      referring_origin_(std::move(referring_origin)),
      session_identifier_(std::move(session_identifier)),
      challenge_(std::move(challenge)),
      authorization_(std::move(authorization)),
      attestation_mode_(attestation_mode) {}

}  // namespace net::device_bound_sessions
