// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/oauth_token_info.h"

namespace remoting {

OAuthTokenInfo::OAuthTokenInfo(const std::string& token)
    : access_token_(token), user_email_("") {}

OAuthTokenInfo::OAuthTokenInfo(const std::string& token,
                               const std::string& email)
    : access_token_(token), user_email_(email) {}

}  // namespace remoting
