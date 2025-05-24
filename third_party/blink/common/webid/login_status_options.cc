// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/webid/login_status_options.h"

#include <optional>
#include <vector>

#include "third_party/blink/public/common/webid/login_status_account.h"

namespace blink::common::webid {

LoginStatusOptions::LoginStatusOptions() = default;

LoginStatusOptions::LoginStatusOptions(
    std::vector<LoginStatusAccount> accounts,
    const std::optional<base::TimeDelta>& expiration)
    : accounts(std::move(accounts)), expiration(expiration) {}

bool LoginStatusOptions::operator==(const LoginStatusOptions& account) const =
    default;

LoginStatusOptions::~LoginStatusOptions() = default;

}  // namespace blink::common::webid
