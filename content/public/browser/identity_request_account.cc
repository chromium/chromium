// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/identity_request_account.h"

namespace content {

IdentityRequestAccount::IdentityRequestAccount(
    const std::string& id,
    const std::string& email,
    const std::string& name,
    const std::string& given_name,
    const GURL& picture,
    std::vector<std::string> login_hints,
    absl::optional<LoginState> login_state)
    : id{id},
      email{email},
      name{name},
      given_name{given_name},
      picture{picture},
      login_hints(std::move(login_hints)),
      login_state{login_state} {}

IdentityRequestAccount::IdentityRequestAccount(const IdentityRequestAccount&) =
    default;
IdentityRequestAccount::~IdentityRequestAccount() = default;

}  // namespace content
