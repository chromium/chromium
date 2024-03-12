// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/corp_auth_util.h"

#include "base/check.h"
#include "remoting/base/oauth_token_getter_impl.h"

namespace remoting {
namespace {

constexpr char kOAuthScope[] =
    "https://www.googleapis.com/auth/chromoting.me2me.host";

}  // namespace

std::unique_ptr<OAuthTokenGetterImpl> CreateCorpTokenGetter(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const std::string& service_account_email,
    const std::string& refresh_token) {
  DCHECK(url_loader_factory);
  DCHECK(!service_account_email.empty());
  DCHECK(!refresh_token.empty());

  return std::make_unique<OAuthTokenGetterImpl>(
      std::make_unique<OAuthTokenGetter::OAuthAuthorizationCredentials>(
          service_account_email, refresh_token,
          /* is_service_account= */ true,
          std::vector<std::string>{kOAuthScope}),
      url_loader_factory,
      /* auto_refresh= */ false);
}

}  // namespace remoting
