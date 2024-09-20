// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/passthrough_oauth_token_getter.h"

#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"

namespace remoting {

PassthroughOAuthTokenGetter::PassthroughOAuthTokenGetter() = default;

PassthroughOAuthTokenGetter::PassthroughOAuthTokenGetter(
    const std::string& username,
    const std::string& access_token,
    const std::string& scopes)
    : username_(username), access_token_(access_token), scopes_(scopes) {}

PassthroughOAuthTokenGetter::~PassthroughOAuthTokenGetter() = default;

void PassthroughOAuthTokenGetter::CallWithToken(
    OAuthTokenGetter::TokenCallback on_access_token) {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(on_access_token),
                                OAuthTokenGetter::Status::SUCCESS, username_,
                                access_token_, scopes_));
}

void PassthroughOAuthTokenGetter::InvalidateCache() {
  NOTIMPLEMENTED();
}

}  // namespace remoting
