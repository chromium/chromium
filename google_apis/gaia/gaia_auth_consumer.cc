// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gaia/gaia_auth_consumer.h"

GaiaAuthConsumer::ClientLoginResult::ClientLoginResult() = default;

GaiaAuthConsumer::ClientLoginResult::ClientLoginResult(
    const std::string& new_sid,
    const std::string& new_lsid,
    const std::string& new_token,
    const std::string& new_data)
    : sid(new_sid), lsid(new_lsid), token(new_token), data(new_data) {}

GaiaAuthConsumer::ClientLoginResult::ClientLoginResult(
    const ClientLoginResult& other) = default;

GaiaAuthConsumer::ClientLoginResult::~ClientLoginResult() {}

bool GaiaAuthConsumer::ClientLoginResult::operator==(
    const ClientLoginResult &b) const {
  return sid == b.sid && lsid == b.lsid && token == b.token && data == b.data;
}

GaiaAuthConsumer::ClientOAuthResult::ClientOAuthResult(
    const std::string& new_refresh_token,
    const std::string& new_access_token,
    int new_expires_in_secs,
    bool new_is_child_account,
    bool new_is_under_advanced_protection)
    : refresh_token(new_refresh_token),
      access_token(new_access_token),
      expires_in_secs(new_expires_in_secs),
      is_child_account(new_is_child_account),
      is_under_advanced_protection(new_is_under_advanced_protection) {}

GaiaAuthConsumer::ClientOAuthResult::ClientOAuthResult(
    const ClientOAuthResult& other) = default;

GaiaAuthConsumer::ClientOAuthResult::~ClientOAuthResult() = default;

bool GaiaAuthConsumer::ClientOAuthResult::operator==(
    const ClientOAuthResult &b) const {
  return refresh_token == b.refresh_token &&
      access_token == b.access_token &&
      expires_in_secs == b.expires_in_secs;
}
