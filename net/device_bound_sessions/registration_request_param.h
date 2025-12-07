// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DEVICE_BOUND_SESSIONS_REGISTRATION_REQUEST_PARAM_H_
#define NET_DEVICE_BOUND_SESSIONS_REGISTRATION_REQUEST_PARAM_H_

#include <optional>
#include <string>
#include <utility>

#include "net/base/net_export.h"
#include "url/gurl.h"

namespace net::device_bound_sessions {

class RegistrationFetcherParam;
class Session;

class NET_EXPORT RegistrationRequestParam {
 public:
  RegistrationRequestParam(const RegistrationRequestParam& other);
  RegistrationRequestParam& operator=(const RegistrationRequestParam& other);

  RegistrationRequestParam(RegistrationRequestParam&& other) noexcept;
  RegistrationRequestParam& operator=(
      RegistrationRequestParam&& other) noexcept;

  ~RegistrationRequestParam();

  static RegistrationRequestParam CreateForRegistration(
      RegistrationFetcherParam&& fetcher_param);
  static RegistrationRequestParam CreateForRefresh(const Session& session);

  const std::optional<std::string>& challenge() const { return challenge_; }
  const std::optional<std::string>& authorization() const {
    return authorization_;
  }

  GURL TakeRegistrationEndpoint() { return std::move(registration_endpoint_); }
  std::optional<std::string> TakeSessionIdentifier() {
    return std::move(session_identifier_);
  }
  std::optional<std::string> TakeChallenge() { return std::move(challenge_); }
  std::optional<std::string> TakeAuthorization() {
    return std::move(authorization_);
  }

  static RegistrationRequestParam CreateForTesting(
      const GURL& registration_endpoint,
      std::optional<std::string> session_identifier,
      std::optional<std::string> challenge);

 private:
  RegistrationRequestParam(const GURL& registration_endpoint,
                           std::optional<std::string> session_identifier,
                           std::optional<std::string> challenge,
                           std::optional<std::string> authorization);

  GURL registration_endpoint_;
  std::optional<std::string> session_identifier_;
  std::optional<std::string> challenge_;
  std::optional<std::string> authorization_;
};

}  // namespace net::device_bound_sessions

#endif  // NET_DEVICE_BOUND_SESSIONS_REGISTRATION_REQUEST_PARAM_H_
