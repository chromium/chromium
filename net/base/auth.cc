// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/auth.h"

namespace net {

AuthChallengeInfo::AuthChallengeInfo() = default;

AuthChallengeInfo::AuthChallengeInfo(const AuthChallengeInfo& other) = default;

bool AuthChallengeInfo::MatchesExceptPath(
    const AuthChallengeInfo& other) const {
  return (is_proxy == other.is_proxy && challenger == other.challenger &&
          scheme == other.scheme && realm == other.realm &&
          challenge == other.challenge);
}

AuthChallengeInfo::~AuthChallengeInfo() = default;

AuthCredentials::AuthCredentials() = default;

AuthCredentials::AuthCredentials(const std::u16string& username,
                                 const std::u16string& password)
    : username_(username), password_(password) {}

AuthCredentials::~AuthCredentials() = default;

void AuthCredentials::Set(const std::u16string& username,
                          const std::u16string& password) {
  username_ = username;
  password_ = password;
}

bool AuthCredentials::Equals(const AuthCredentials& other) const {
  return username_ == other.username_ && password_ == other.password_;
}

bool AuthCredentials::Empty() const {
  return username_.empty() && password_.empty();
}

}  // namespace net
