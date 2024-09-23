// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/fake_oauth_token_getter.h"

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/task/single_thread_task_runner.h"

namespace remoting {

FakeOAuthTokenGetter::FakeOAuthTokenGetter(Status status,
                                           const std::string& user_email,
                                           const std::string& access_token,
                                           const std::string& scopes)
    : status_(status),
      user_email_(user_email),
      access_token_(access_token),
      scopes_(scopes) {}

FakeOAuthTokenGetter::~FakeOAuthTokenGetter() = default;

void FakeOAuthTokenGetter::CallWithToken(TokenCallback on_access_token) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(on_access_token), status_,
                                user_email_, access_token_, scopes_));
}

void FakeOAuthTokenGetter::InvalidateCache() {
  NOTIMPLEMENTED();
}

}  // namespace remoting
