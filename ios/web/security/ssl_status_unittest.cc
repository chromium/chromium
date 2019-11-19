// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/public/security/ssl_status.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace web {

using SSLStatusTest = PlatformTest;

// The TrivialUserData class stores an integer and copies that integer when
// its Clone() method is called.
class TrivialUserData : public SSLStatus::UserData {
 public:
  TrivialUserData() : value_(0) {}
  explicit TrivialUserData(int value) : value_(value) {}
  ~TrivialUserData() override {}

  int value() { return value_; }

  std::unique_ptr<SSLStatus::UserData> Clone() override {
    return std::make_unique<TrivialUserData>(value_);
  }

 private:
  int value_;
  DISALLOW_COPY_AND_ASSIGN(TrivialUserData);
};

// Tests the Equals() method of the SSLStatus class.
TEST_F(SSLStatusTest, SSLStatusEqualityTest) {
  SSLStatus status;
  EXPECT_EQ(SECURITY_STYLE_UNKNOWN, status.security_style);
  EXPECT_EQ(0u, status.cert_status);
  EXPECT_EQ(SSLStatus::NORMAL_CONTENT, status.content_status);

  // Verify that the Equals() method returns false if two SSLStatus objects
  // have different ContentStatusFlags.
  SSLStatus other_status;
  other_status.content_status =
      SSLStatus::ContentStatusFlags::DISPLAYED_INSECURE_CONTENT;
  EXPECT_FALSE(status.Equals(other_status));
  EXPECT_FALSE(other_status.Equals(status));

  // Verify a copied SSLStatus Equals() the original.
  SSLStatus copied_status = status;
  EXPECT_TRUE(status.Equals(copied_status));
  EXPECT_TRUE(copied_status.Equals(status));

  // Verify a copied SSLStatus still Equals() the original after a UserData is
  // assigned to it.
  copied_status.user_data = std::make_unique<TrivialUserData>();
  EXPECT_TRUE(status.Equals(copied_status));
  EXPECT_TRUE(copied_status.Equals(status));
}

// Tests that copying a SSLStatus class clones its UserData.
TEST_F(SSLStatusTest, SSLStatusCloningTest) {
  const int kMagic = 1234;
  SSLStatus status;
  status.user_data = std::make_unique<TrivialUserData>(kMagic);

  // Verify that copying a SSLStatus with a UserData assigned will Clone()
  // the UserData to the new copy.
  SSLStatus copied_status = status;
  EXPECT_TRUE(status.Equals(copied_status));
  EXPECT_TRUE(copied_status.Equals(status));
  TrivialUserData* data =
      static_cast<TrivialUserData*>(copied_status.user_data.get());
  EXPECT_EQ(kMagic, data->value());
}

}  // namespace web
