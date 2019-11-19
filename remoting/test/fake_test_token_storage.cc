// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/test/fake_test_token_storage.h"

#include "base/logging.h"

namespace {
const char kRefreshTokenValue[] = "1/lkjalseLKJlsiJgr45jbv";
}

namespace remoting {
namespace test {

FakeTestTokenStorage::FakeTestTokenStorage()
    : refresh_token_value_(kRefreshTokenValue),
      refresh_token_write_succeeded_(true),
      refresh_token_write_attempted_(false) {}

FakeTestTokenStorage::~FakeTestTokenStorage() = default;

std::string FakeTestTokenStorage::FetchRefreshToken() {
  return refresh_token_value_;
}

bool FakeTestTokenStorage::StoreRefreshToken(const std::string& refresh_token) {
  // Record the information passed to us to write.
  refresh_token_write_attempted_ = true;
  stored_refresh_token_value_ = refresh_token;

  return refresh_token_write_succeeded_;
}

std::string FakeTestTokenStorage::FetchUserEmail() {
  NOTIMPLEMENTED();
  return "";
}

bool FakeTestTokenStorage::StoreUserEmail(const std::string& user_email) {
  NOTIMPLEMENTED();
  return false;
}

std::string FakeTestTokenStorage::FetchAccessToken() {
  NOTIMPLEMENTED();
  return "";
}

bool FakeTestTokenStorage::StoreAccessToken(const std::string& access_token) {
  NOTIMPLEMENTED();
  return false;
}

std::string FakeTestTokenStorage::FetchDeviceId() {
  NOTIMPLEMENTED();
  return "";
}

bool FakeTestTokenStorage::StoreDeviceId(const std::string& device_id) {
  NOTIMPLEMENTED();
  return false;
}

}  // namespace test
}  // namespace remoting
