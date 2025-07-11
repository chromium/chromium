// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/webid/identity_request_account.h"

#include <optional>

#include "content/public/browser/webid/identity_request_dialog_controller.h"
#include "third_party/blink/public/common/webid/login_status_account.h"

namespace content {

IdentityRequestAccount::IdentityRequestAccount(
    const std::string& id,
    const std::string& display_identifier,
    const std::string& display_name,
    const std::string& email,
    const std::string& name,
    const std::string& given_name,
    const GURL& picture,
    const std::string& phone,
    const std::string& username,
    std::vector<std::string> login_hints,
    std::vector<std::string> domain_hints,
    std::vector<std::string> labels,
    std::optional<LoginState> idp_claimed_login_state,
    LoginState browser_trusted_login_state,
    std::optional<base::Time> last_used_timestamp)
    : id{id},
      display_identifier{display_identifier},
      display_name{display_name},
      email{email},
      name{name},
      given_name{given_name},
      picture{picture},
      phone{phone},
      username{username},
      login_hints(std::move(login_hints)),
      domain_hints(std::move(domain_hints)),
      labels(std::move(labels)),
      idp_claimed_login_state{idp_claimed_login_state},
      browser_trusted_login_state{browser_trusted_login_state},
      last_used_timestamp{last_used_timestamp} {}

// TODO(crbug.com/405194067) Fix this to properly handle alternative
// identifiers.
IdentityRequestAccount::IdentityRequestAccount(
    const blink::common::webid::LoginStatusAccount& account)
    : id{account.id},
      display_identifier{account.email},
      display_name(account.name),
      email{account.email},
      name{account.name},
      given_name{account.given_name.value_or("")},
      picture{account.picture.value_or(GURL())},
      login_hints{{}},
      domain_hints{{}},
      labels{{}},
      idp_claimed_login_state{std::nullopt},
      browser_trusted_login_state(LoginState::kSignUp),
      last_used_timestamp{std::nullopt} {}

IdentityRequestAccount::~IdentityRequestAccount() = default;

}  // namespace content
