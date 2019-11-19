// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_TEST_FAKE_TEST_TOKEN_STORAGE_H_
#define REMOTING_TEST_FAKE_TEST_TOKEN_STORAGE_H_

#include <string>

#include "base/macros.h"
#include "remoting/test/test_token_storage.h"

namespace remoting {
namespace test {

// Stubs out the file API and returns fake data so we can remove
// file system dependencies when testing the TestDriverEnvironment.
class FakeTestTokenStorage : public TestTokenStorage {
 public:
  FakeTestTokenStorage();
  ~FakeTestTokenStorage() override;

  // TestTokenStorage interface.
  std::string FetchRefreshToken() override;
  bool StoreRefreshToken(const std::string& refresh_token) override;
  std::string FetchUserEmail() override;
  bool StoreUserEmail(const std::string& user_email) override;
  std::string FetchAccessToken() override;
  bool StoreAccessToken(const std::string& access_token) override;
  std::string FetchDeviceId() override;
  bool StoreDeviceId(const std::string& device_id) override;

  bool refresh_token_write_attempted() const {
    return refresh_token_write_attempted_;
  }

  const std::string& stored_refresh_token_value() const {
    return stored_refresh_token_value_;
  }

  void set_refresh_token_value(const std::string& new_token_value) {
    refresh_token_value_ = new_token_value;
  }

  void set_refresh_token_write_succeeded(bool write_succeeded) {
    refresh_token_write_succeeded_ = write_succeeded;
  }

 private:
  // Control members used to return specific data to the caller.
  std::string refresh_token_value_;
  bool refresh_token_write_succeeded_;

  // Verification members to observe the value of the data being written.
  bool refresh_token_write_attempted_;
  std::string stored_refresh_token_value_;

  DISALLOW_COPY_AND_ASSIGN(FakeTestTokenStorage);
};

}  // namespace test
}  // namespace remoting

#endif  // REMOTING_TEST_FAKE_TEST_TOKEN_STORAGE_H_
