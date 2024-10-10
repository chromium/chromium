// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/fake_oauth_token_getter.h"

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/task/single_thread_task_runner.h"

namespace remoting {

FakeOAuthTokenGetter::FakeOAuthTokenGetter(Status status,
                                           const OAuthTokenInfo& token_info)
    : status_(status), token_info_(token_info) {}

FakeOAuthTokenGetter::~FakeOAuthTokenGetter() = default;

void FakeOAuthTokenGetter::CallWithToken(TokenCallback on_access_token) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(on_access_token), status_,
                                OAuthTokenInfo(token_info_)));
}

void FakeOAuthTokenGetter::InvalidateCache() {
  NOTIMPLEMENTED();
}

}  // namespace remoting
