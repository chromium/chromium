// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>
#include <string>

#include "remoting/host/pin_hash.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

class PinHashTest : public testing::Test {};

TEST_F(PinHashTest, KnownHashValue) {
  std::string hash = MakeHostPinHash("Host ID", "1234");
  ASSERT_EQ("hmac:bk6RVRFLpLO89mr4QPHSg8CemUUtI90r2F0VfvTmWLI=", hash);
}

TEST_F(PinHashTest, VerifyHostPinHash) {
  std::string host_id1("Host ID 1");
  std::string host_id2("Host ID 2");
  std::string pin1("1234");
  std::string pin2("4321");
  ASSERT_TRUE(
      VerifyHostPinHash(MakeHostPinHash(host_id1, pin1), host_id1, pin1));
  ASSERT_FALSE(
      VerifyHostPinHash(MakeHostPinHash(host_id1, pin1), host_id2, pin1));
  ASSERT_FALSE(
      VerifyHostPinHash(MakeHostPinHash(host_id1, pin1), host_id1, pin2));
}

}  // namespace remoting
