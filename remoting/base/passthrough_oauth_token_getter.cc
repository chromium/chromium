// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/passthrough_oauth_token_getter.h"

#include "base/bind.h"
#include "base/threading/sequenced_task_runner_handle.h"

namespace remoting {

PassthroughOAuthTokenGetter::PassthroughOAuthTokenGetter(
    const std::string& username,
    const std::string& access_token)
    : username_(username), access_token_(access_token) {}

PassthroughOAuthTokenGetter::~PassthroughOAuthTokenGetter() = default;

void PassthroughOAuthTokenGetter::CallWithToken(
    OAuthTokenGetter::TokenCallback on_access_token) {
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(on_access_token),
                                OAuthTokenGetter::Status::SUCCESS, username_,
                                access_token_));
}

void PassthroughOAuthTokenGetter::InvalidateCache() {
  NOTIMPLEMENTED();
}

}  // namespace remoting
