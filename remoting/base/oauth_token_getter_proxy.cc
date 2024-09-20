// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/oauth_token_getter_proxy.h"

#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread_checker.h"

namespace remoting {

namespace {

void ResolveCallback(
    OAuthTokenGetter::TokenCallback on_access_token,
    scoped_refptr<base::SequencedTaskRunner> original_task_runner,
    OAuthTokenGetter::Status status,
    const std::string& user_email,
    const std::string& access_token,
    const std::string& scopes) {
  if (!original_task_runner->RunsTasksInCurrentSequence()) {
    original_task_runner->PostTask(
        FROM_HERE, base::BindOnce(std::move(on_access_token), status,
                                  user_email, access_token, scopes));
    return;
  }

  std::move(on_access_token).Run(status, user_email, access_token, scopes);
}

}  // namespace

OAuthTokenGetterProxy::OAuthTokenGetterProxy(
    base::WeakPtr<OAuthTokenGetter> token_getter,
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : token_getter_(token_getter), task_runner_(task_runner) {}

OAuthTokenGetterProxy::OAuthTokenGetterProxy(
    base::WeakPtr<OAuthTokenGetter> token_getter)
    : OAuthTokenGetterProxy(token_getter,
                            base::SequencedTaskRunner::GetCurrentDefault()) {}

OAuthTokenGetterProxy::~OAuthTokenGetterProxy() = default;

void OAuthTokenGetterProxy::CallWithToken(
    OAuthTokenGetter::TokenCallback on_access_token) {
  if (!task_runner_->RunsTasksInCurrentSequence()) {
    auto task_runner_to_reply = base::SequencedTaskRunner::GetCurrentDefault();

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
