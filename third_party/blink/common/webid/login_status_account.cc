// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/webid/login_status_account.h"

#include <optional>

#include "base/types/optional_ref.h"

namespace blink::common::webid {
LoginStatusAccount::LoginStatusAccount() = default;

LoginStatusAccount::LoginStatusAccount(
    const std::string& id,
    const std::string& email,
    const std::string& name,
    base::optional_ref<const std::string> given_name,
    base::optional_ref<const GURL> picture_url)
    : id(id),
      email(email),
      name(name),
      given_name(given_name.CopyAsOptional()),
      picture(picture_url.CopyAsOptional()) {}

bool LoginStatusAccount::operator==(const LoginStatusAccount& account) const =
    default;

LoginStatusAccount::~LoginStatusAccount() = default;
}  // namespace blink::common::webid
