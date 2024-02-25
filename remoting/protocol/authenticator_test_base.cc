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
#include "remoting/protocol/channel_authenticator.h"
#include "remoting/protocol/fake_stream_socket.h"
#include "remoting/protocol/p2p_stream_socket.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libjingle_xmpp/xmllite/xmlelement.h"

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

AuthenticatorTestBase::MockChannelDoneCallback::MockChannelDoneCallback() =
    default;

AuthenticatorTestBase::MockChannelDoneCallback::~MockChannelDoneCallback() =
    default;

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
  std::unique_ptr<jingle_xmpp::XmlElement> message;
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
  ASSERT_TRUE(message.get());
  ASSERT_NE(Authenticator::MESSAGE_READY, sender->state());

  ASSERT_EQ(Authenticator::WAITING_MESSAGE, receiver->state());
  receiver->ProcessMessage(
      message.get(),
      base::BindOnce(&AuthenticatorTestBase::ContinueAuthExchangeWith,
                     base::Unretained(receiver), base::Unretained(sender),
                     receiver->started(), sender->started()));
}

void AuthenticatorTestBase::RunChannelAuth(bool expected_fail) {
  client_fake_socket_ = std::make_unique<FakeStreamSocket>();
  host_fake_socket_ = std::make_unique<FakeStreamSocket>();
  client_fake_socket_->PairWith(host_fake_socket_.get());

  client_auth_->SecureAndAuthenticate(
      std::move(client_fake_socket_),
      base::BindOnce(&AuthenticatorTestBase::OnClientConnected,
                     base::Unretained(this)));

  host_auth_->SecureAndAuthenticate(
      std::move(host_fake_socket_),
      base::BindOnce(&AuthenticatorTestBase::OnHostConnected,
                     base::Unretained(this)));

  // Expect two callbacks to be called - the client callback and the host
  // callback.
  int callback_counter = 2;
  base::RunLoop loop;
  EXPECT_CALL(client_callback_, OnDone(net::OK))
      .WillOnce(
          QuitThreadOnCounter(loop.QuitWhenIdleClosure(), &callback_counter));
  if (expected_fail) {
    EXPECT_CALL(host_callback_, OnDone(net::ERR_FAILED))
        .WillOnce(
            QuitThreadOnCounter(loop.QuitWhenIdleClosure(), &callback_counter));
  } else {
    EXPECT_CALL(host_callback_, OnDone(net::OK))
        .WillOnce(
            QuitThreadOnCounter(loop.QuitWhenIdleClosure(), &callback_counter));
  }

  // Ensure that .Run() does not run unbounded if the callbacks are never
  // called.
  base::OneShotTimer shutdown_timer;
  shutdown_timer.Start(FROM_HERE, TestTimeouts::action_timeout(),
                       loop.QuitWhenIdleClosure());
  loop.Run();
  shutdown_timer.Stop();

  testing::Mock::VerifyAndClearExpectations(&client_callback_);
  testing::Mock::VerifyAndClearExpectations(&host_callback_);

  if (!expected_fail) {
    ASSERT_TRUE(client_socket_.get() != nullptr);
    ASSERT_TRUE(host_socket_.get() != nullptr);
  }
}

void AuthenticatorTestBase::OnHostConnected(
    int error,
    std::unique_ptr<P2PStreamSocket> socket) {
  host_callback_.OnDone(error);
  host_socket_ = std::move(socket);
}

void AuthenticatorTestBase::OnClientConnected(
    int error,
    std::unique_ptr<P2PStreamSocket> socket) {
  client_callback_.OnDone(error);
  client_socket_ = std::move(socket);
}

}  // namespace remoting::protocol
