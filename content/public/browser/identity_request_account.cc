// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/identity_request_account.h"

#include "content/public/browser/identity_request_dialog_controller.h"

namespace content {

IdentityRequestAccount::IdentityRequestAccount(
    const std::string& id,
    const std::string& email,
    const std::string& name,
    const std::string& given_name,
    const GURL& picture,
    std::vector<std::string> login_hints,
    std::vector<std::string> domain_hints,
    std::vector<std::string> labels,
    std::optional<LoginState> login_state,
    LoginState browser_trusted_login_state,
    std::optional<base::Time> last_used_timestamp)
    : id{id},
      email{email},
      name{name},
      given_name{given_name},
      picture{picture},
      login_hints(std::move(login_hints)),
      domain_hints(std::move(domain_hints)),
      labels(std::move(labels)),
      login_state{login_state},
      browser_trusted_login_state{browser_trusted_login_state},
      last_used_timestamp{last_used_timestamp} {}

IdentityRequestAccount::~IdentityRequestAccount() = default;

}  // namespace content
