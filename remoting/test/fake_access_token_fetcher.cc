// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/test/fake_access_token_fetcher.h"

namespace remoting {
namespace test {

FakeAccessTokenFetcher::FakeAccessTokenFetcher()
    : fail_access_token_from_auth_code_(false),
      fail_access_token_from_refresh_token_(false) {
}

FakeAccessTokenFetcher::~FakeAccessTokenFetcher() = default;

void FakeAccessTokenFetcher::GetAccessTokenFromAuthCode(
    const std::string& auth_code,
    AccessTokenCallback callback) {
  if (fail_access_token_from_auth_code_) {
    // Empty strings are returned in failure cases.
    std::move(callback).Run(std::string(), std::string());
  } else {
    std::move(callback).Run(kFakeAccessTokenFetcherAccessTokenValue,
                            kFakeAccessTokenFetcherRefreshTokenValue);
  }
}

void FakeAccessTokenFetcher::GetAccessTokenFromRefreshToken(
    const std::string& refresh_token,
    AccessTokenCallback callback) {
  if (fail_access_token_from_refresh_token_) {
    // Empty strings are returned in failure cases.
    std::move(callback).Run(std::string(), std::string());
  } else {
    std::move(callback).Run(kFakeAccessTokenFetcherAccessTokenValue,
                            kFakeAccessTokenFetcherRefreshTokenValue);
  }
}

}  // namespace test
}  // namespace remoting
