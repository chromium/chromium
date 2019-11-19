// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_TEST_MOCK_ACCESS_TOKEN_FETCHER_H_
#define REMOTING_TEST_MOCK_ACCESS_TOKEN_FETCHER_H_

#include "remoting/test/access_token_fetcher.h"

#include <string>

#include "base/macros.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {
namespace test {

// Used for testing classes which rely on the AccessTokenFetcher and want to
// simulate success and failure scenarios without using the actual class and
// network connection.
class MockAccessTokenFetcher : public AccessTokenFetcher {
 public:
  MockAccessTokenFetcher();
  ~MockAccessTokenFetcher() override;

  MOCK_METHOD2(GetAccessTokenFromAuthCode,
               void(const std::string& auth_code,
                    AccessTokenCallback callback));

  MOCK_METHOD2(GetAccessTokenFromRefreshToken,
               void(const std::string& refresh_token,
                    AccessTokenCallback callback));

  // Stores an access token fetcher object and wires up the mock methods to call
  // through to the appropriate method on it.  This method is typically used to
  // pass a FakeAccessTokenFetcher.
  void SetAccessTokenFetcher(std::unique_ptr<AccessTokenFetcher> fetcher);

 private:
  std::unique_ptr<AccessTokenFetcher> internal_access_token_fetcher_;

  DISALLOW_COPY_AND_ASSIGN(MockAccessTokenFetcher);
};

}  // namespace test
}  // namespace remoting

#endif  // REMOTING_TEST_MOCK_ACCESS_TOKEN_FETCHER_H_
