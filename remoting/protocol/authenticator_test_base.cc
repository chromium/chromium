// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/authenticator_test_base.h"

#include <memory>
#include <utility>

#include "base/base64.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/test_timeouts.h"
#include "base/timer/timer.h"
#include "net/base/net_errors.h"
#include "net/test/test_data_directory.h"
#include "remoting/base/rsa_key_pair.h"
#include "remoting/protocol/authenticator.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::SaveArg;

namespace remoting::protocol {

namespace {

ACTION_P2(QuitThreadOnCounter, quit_closure, counter) {
  --(*counter);
  EXPECT_GE(*counter, 0);
  if (*counter == 0) {
    std::move(quit_closure).Run();
  }
}

}  // namespace

AuthenticatorTestBase::AuthenticatorTestBase() = default;

AuthenticatorTestBase::~AuthenticatorTestBase() = default;

void AuthenticatorTestBase::SetUp() {
  base::FilePath certs_dir(net::GetTestCertsDirectory());

  base::FilePath cert_path = certs_dir.AppendASCII("unittest.selfsigned.der");
  ASSERT_TRUE(base::ReadFileToString(cert_path, &host_cert_));

  base::FilePath key_path = certs_dir.AppendASCII("unittest.key.bin");
  std::string key_string;
  ASSERT_TRUE(base::ReadFileToString(key_path, &key_string));
  std::string key_base64 = base::Base64Encode(key_string);
  key_pair_ = RsaKeyPair::FromString(key_base64);
  ASSERT_TRUE(key_pair_.get());
  host_public_key_ = key_pair_->GetPublicKey();
}

void AuthenticatorTestBase::RunAuthExchange() {
  ContinueAuthExchangeWith(client_.get(), host_.get(), client_->started(),
                           host_->started());
}

void AuthenticatorTestBase::RunHostInitiatedAuthExchange() {
  ContinueAuthExchangeWith(host_.get(), client_.get(), host_->started(),
                           client_->started());
}

// static
// This function sends a message from the sender and receiver and recursively
// calls itself to the send the next message from the receiver to the sender
// untils the authentication completes.
void AuthenticatorTestBase::ContinueAuthExchangeWith(Authenticator* sender,
                                                     Authenticator* receiver,
                                                     bool sender_started,
                                                     bool receiver_started) {
  JingleAuthentication message;
  ASSERT_NE(Authenticator::WAITING_MESSAGE, sender->state());
  if (sender->state() == Authenticator::ACCEPTED ||
      sender->state() == Authenticator::REJECTED) {
    return;
  }

  // Verify that once the started flag for either party is set to true,
  // it should always stay true.
  if (receiver_started) {
    ASSERT_TRUE(receiver->started());
  }

  if (sender_started) {
    ASSERT_TRUE(sender->started());
  }

  ASSERT_EQ(Authenticator::MESSAGE_READY, sender->state());
  message = sender->GetNextMessage();
  ASSERT_FALSE(message.is_empty());
  ASSERT_NE(Authenticator::MESSAGE_READY, sender->state());

  ASSERT_EQ(Authenticator::WAITING_MESSAGE, receiver->state());
  receiver->ProcessMessage(
      message,
      base::BindOnce(&AuthenticatorTestBase::ContinueAuthExchangeWith,
                     base::Unretained(receiver), base::Unretained(sender),
                     receiver->started(), sender->started()));
}

}  // namespace remoting::protocol
