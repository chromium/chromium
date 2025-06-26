// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "remoting/base/rsa_key_pair.h"
#include "remoting/base/test_rsa_key_pair.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

using RsaKeyPairTest = ::testing::Test;

TEST_F(RsaKeyPairTest, GenerateKey) {
  // Test that we can generate a valid key.
  scoped_refptr<RsaKeyPair> key_pair = RsaKeyPair::Generate();
  ASSERT_TRUE(key_pair.get());
  ASSERT_NE(key_pair->ToString(), "");
  ASSERT_NE(key_pair->GetPublicKey(), "");
}

}  // namespace remoting
