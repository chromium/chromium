// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_AUTHENTICATOR_TEST_BASE_H_
#define REMOTING_PROTOCOL_AUTHENTICATOR_TEST_BASE_H_

#include <memory>
#include <string>

#include "base/memory/scoped_refptr.h"
#include "base/test/task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

class RsaKeyPair;

namespace protocol {

class Authenticator;

class AuthenticatorTestBase : public testing::Test {
 public:
  AuthenticatorTestBase();

  AuthenticatorTestBase(const AuthenticatorTestBase&) = delete;
  AuthenticatorTestBase& operator=(const AuthenticatorTestBase&) = delete;

  ~AuthenticatorTestBase() override;

 protected:
  static inline constexpr char kHostId[] = "alice@gmail.com/123";
  static inline constexpr char kClientId[] = "alice@gmail.com/abc";

  class MockChannelDoneCallback {
   public:
    MockChannelDoneCallback();
    ~MockChannelDoneCallback();
    MOCK_METHOD(void, OnDone, (int error));
  };

  static void ContinueAuthExchangeWith(Authenticator* sender,
                                       Authenticator* receiver,
                                       bool sender_started,
                                       bool receiver_srated);
  void SetUp() override;
  void RunAuthExchange();
  void RunHostInitiatedAuthExchange();

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  scoped_refptr<RsaKeyPair> key_pair_;
  std::string host_public_key_;
  std::string host_cert_;
  std::unique_ptr<Authenticator> host_;
  std::unique_ptr<Authenticator> client_;
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_AUTHENTICATOR_TEST_BASE_H_
