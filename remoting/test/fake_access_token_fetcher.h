// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_TEST_FAKE_ACCESS_TOKEN_FETCHER_H_
#define REMOTING_TEST_FAKE_ACCESS_TOKEN_FETCHER_H_

#include <string>

#include "base/macros.h"
#include "remoting/test/access_token_fetcher.h"

namespace remoting {
namespace test {

const char kFakeAccessTokenFetcherRefreshTokenValue[] = "fake_refresh_token";
const char kFakeAccessTokenFetcherAccessTokenValue[] = "fake_access_token";

// Used for testing classes which rely on the AccessTokenFetcher and want to
// simulate success and failure scenarios without using the actual class and
// network connection.
class FakeAccessTokenFetcher : public AccessTokenFetcher {
 public:
  FakeAccessTokenFetcher();
  ~FakeAccessTokenFetcher() override;

  // AccessTokenFetcher interface.
  void GetAccessTokenFromAuthCode(const std::string& auth_code,
                                  AccessTokenCallback callback) override;
  void GetAccessTokenFromRefreshToken(const std::string& refresh_token,
                                      AccessTokenCallback callback) override;

  void set_fail_access_token_from_auth_code(bool fail) {
    fail_access_token_from_auth_code_ = fail;
  }

  void set_fail_access_token_from_refresh_token(bool fail) {
    fail_access_token_from_refresh_token_ = fail;
  }

 private:
  // True if GetAccessTokenFromAuthCode() should fail.
  bool fail_access_token_from_auth_code_;

  // True if GetAccessTokenFromRefreshToken() should fail.
  bool fail_access_token_from_refresh_token_;

  DISALLOW_COPY_AND_ASSIGN(FakeAccessTokenFetcher);
};

}  // namespace test
}  // namespace remoting

#endif  // REMOTING_TEST_FAKE_ACCESS_TOKEN_FETCHER_H_
