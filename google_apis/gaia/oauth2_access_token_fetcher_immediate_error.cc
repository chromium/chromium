// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gaia/oauth2_access_token_fetcher_immediate_error.h"

#include "base/bind.h"
#include "base/threading/thread_task_runner_handle.h"
#include "google_apis/gaia/google_service_auth_error.h"


OAuth2AccessTokenFetcherImmediateError::FailCaller::FailCaller(
    OAuth2AccessTokenFetcherImmediateError* fetcher)
    : fetcher_(fetcher) {
}

OAuth2AccessTokenFetcherImmediateError::FailCaller::~FailCaller() {
}

void OAuth2AccessTokenFetcherImmediateError::FailCaller::run() {
  if (fetcher_) {
    fetcher_->Fail();
    fetcher_ = nullptr;
  }
}

void OAuth2AccessTokenFetcherImmediateError::FailCaller::detach() {
  fetcher_ = nullptr;
}


OAuth2AccessTokenFetcherImmediateError::OAuth2AccessTokenFetcherImmediateError(
    OAuth2AccessTokenConsumer* consumer,
    const GoogleServiceAuthError& error)
    : OAuth2AccessTokenFetcher(consumer),
      immediate_error_(error) {
  DCHECK(immediate_error_ != GoogleServiceAuthError::AuthErrorNone());
}

OAuth2AccessTokenFetcherImmediateError::
    ~OAuth2AccessTokenFetcherImmediateError() {
  CancelRequest();
}

void OAuth2AccessTokenFetcherImmediateError::CancelRequest() {
  if (failer_) {
    failer_->detach();
    failer_ = nullptr;
  }
}

void OAuth2AccessTokenFetcherImmediateError::Start(
    const std::string& client_id,
    const std::string& client_secret,
    const std::vector<std::string>& scopes) {
  failer_ = new FailCaller(this);
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&OAuth2AccessTokenFetcherImmediateError::FailCaller::run,
                     failer_));
}

void OAuth2AccessTokenFetcherImmediateError::Fail() {
  // The call below will likely destruct this object.  We have to make a copy
  // of the error into a local variable because the class member thus will
  // be destroyed after which the copy-passed-by-reference will cause a
  // memory violation when accessed.
  GoogleServiceAuthError error_copy = immediate_error_;
  FireOnGetTokenFailure(error_copy);
}
