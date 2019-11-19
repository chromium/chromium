// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/oauth_token_getter_proxy.h"

#include "base/bind.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/threading/thread_checker.h"

namespace remoting {

namespace {

void ResolveCallback(
    OAuthTokenGetter::TokenCallback on_access_token,
    scoped_refptr<base::SequencedTaskRunner> original_task_runner,
    OAuthTokenGetter::Status status,
    const std::string& user_email,
    const std::string& access_token) {
  if (!original_task_runner->RunsTasksInCurrentSequence()) {
    original_task_runner->PostTask(
        FROM_HERE, base::BindOnce(std::move(on_access_token), status,
                                  user_email, access_token));
    return;
  }

  std::move(on_access_token).Run(status, user_email, access_token);
}

}  // namespace

OAuthTokenGetterProxy::OAuthTokenGetterProxy(
    base::WeakPtr<OAuthTokenGetter> token_getter,
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : token_getter_(token_getter), task_runner_(task_runner) {}

OAuthTokenGetterProxy::OAuthTokenGetterProxy(
    base::WeakPtr<OAuthTokenGetter> token_getter)
    : OAuthTokenGetterProxy(token_getter,
                            base::SequencedTaskRunnerHandle::Get()) {}

OAuthTokenGetterProxy::~OAuthTokenGetterProxy() = default;

void OAuthTokenGetterProxy::CallWithToken(
    OAuthTokenGetter::TokenCallback on_access_token) {
  if (!task_runner_->RunsTasksInCurrentSequence()) {
    auto task_runner_to_reply = base::SequencedTaskRunnerHandle::Get();

    auto reply_callback = base::BindOnce(
        &ResolveCallback, std::move(on_access_token), task_runner_to_reply);
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&OAuthTokenGetter::CallWithToken,
                                  token_getter_, std::move(reply_callback)));
    return;
  }

  if (token_getter_) {
    token_getter_->CallWithToken(std::move(on_access_token));
  }
}

void OAuthTokenGetterProxy::InvalidateCache() {
  if (!task_runner_->RunsTasksInCurrentSequence()) {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&OAuthTokenGetter::InvalidateCache, token_getter_));
    return;
  }

  if (token_getter_) {
    token_getter_->InvalidateCache();
  }
}

}  // namespace remoting
