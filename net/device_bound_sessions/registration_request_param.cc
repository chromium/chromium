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
RegistrationRequestParam RegistrationRequestParam::Create(
    RegistrationFetcherParam&& fetcher_param) {
  return RegistrationRequestParam(fetcher_param.TakeRegistrationEndpoint(),
                                  std::nullopt, fetcher_param.TakeChallenge(),
                                  fetcher_param.TakeAuthorization());
}

// static
RegistrationRequestParam RegistrationRequestParam::Create(
    const Session& session) {
  return RegistrationRequestParam(session.refresh_url(), session.id().value(),
                                  session.cached_challenge(), std::nullopt);
}

// static
RegistrationRequestParam RegistrationRequestParam::CreateForTesting(
    const GURL& registration_endpoint,
    std::string session_identifier,
    std::optional<std::string> challenge) {
  return RegistrationRequestParam(registration_endpoint,
                                  std::move(session_identifier),
                                  std::move(challenge), std::nullopt);
}

RegistrationRequestParam::RegistrationRequestParam(
    const GURL& registration_endpoint,
    std::optional<std::string> session_identifier,
    std::optional<std::string> challenge,
    std::optional<std::string> authorization)
    : registration_endpoint_(registration_endpoint),
      session_identifier_(std::move(session_identifier)),
      challenge_(std::move(challenge)),
      authorization_(std::move(authorization)) {}

}  // namespace net::device_bound_sessions
