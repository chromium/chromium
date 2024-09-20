// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_TEST_TEST_TOKEN_STORAGE_H_
#define REMOTING_TEST_TEST_TOKEN_STORAGE_H_

#include <memory>
#include <string>

#include "remoting/test/test_device_id_provider.h"

namespace base {
class FilePath;
}

namespace remoting {
namespace test {

// Used to store and retrieve tokens for test.  This interface is provided to
// allow for stubbing out the storage mechanism for testing.
class TestTokenStorage : public TestDeviceIdProvider::TokenStorage {
 public:
  TestTokenStorage() = default;
  ~TestTokenStorage() override = default;

  virtual std::string FetchRefreshToken() = 0;
  virtual bool StoreRefreshToken(const std::string& refresh_token) = 0;

  virtual std::string FetchUserEmail() = 0;
  virtual bool StoreUserEmail(const std::string& user_email) = 0;

  virtual std::string FetchAccessToken() = 0;
  virtual bool StoreAccessToken(const std::string& access_token) = 0;

  virtual std::string FetchScopes() = 0;
  virtual bool StoreScopes(const std::string& scopes) = 0;

  // Returns a TestTokenStorage which reads/writes to a user specific token
  // file on the local disk.
  static std::unique_ptr<TestTokenStorage> OnDisk(
      const std::string& user_name,
      const base::FilePath& tokens_file_path);
};

}  // namespace test
}  // namespace remoting

#endif  // REMOTING_TEST_TEST_TOKEN_STORAGE_H_
