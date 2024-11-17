// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/passthrough_oauth_token_getter.h"

#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"

namespace remoting {

PassthroughOAuthTokenGetter::PassthroughOAuthTokenGetter() = default;

PassthroughOAuthTokenGetter::PassthroughOAuthTokenGetter(
    const OAuthTokenInfo& token_info)
    : token_info_(token_info) {}

PassthroughOAuthTokenGetter::~PassthroughOAuthTokenGetter() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void PassthroughOAuthTokenGetter::CallWithToken(
    OAuthTokenGetter::TokenCallback on_access_token) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(on_access_token),
                                OAuthTokenGetter::Status::SUCCESS,
                                OAuthTokenInfo(token_info_)));
}

void PassthroughOAuthTokenGetter::InvalidateCache() {
  NOTIMPLEMENTED();
}

base::WeakPtr<PassthroughOAuthTokenGetter>
PassthroughOAuthTokenGetter::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace remoting
