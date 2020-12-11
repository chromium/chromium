// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <tuple>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "remoting/protocol/authenticator.h"
#include "remoting/protocol/protocol_mock_objects.h"
#include "remoting/protocol/validating_authenticator.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libjingle_xmpp/xmllite/xmlelement.h"

namespace remoting {
namespace protocol {

namespace {

using testing::_;
using testing::Return;

typedef ValidatingAuthenticator::Result ValidationResult;

constexpr char kRemoteTestJid[] = "ficticious_jid_for_testing";

// testing::InvokeArgument<N> does not work with base::Callback, fortunately
// gmock makes it simple to create action templates that do for the various
// possible numbers of arguments.
ACTION_TEMPLATE(InvokeCallbackArgument,
                HAS_1_TEMPLATE_PARAMS(int, k),
                AND_0_VALUE_PARAMS()) {
  std::move(const_cast<base::OnceClosure&>(std::get<k>(args))).Run();
}

}  // namespace

class ValidatingAuthenticatorTest : public testing::Test {
 public:
  ValidatingAuthenticatorTest();
  ~ValidatingAuthenticatorTest() override;

  void ValidateCallback(const std::string& remote_jid,
                        ValidatingAuthenticator::ResultCallback callback);

 protected:
  // testing::Test overrides.
  void SetUp() override;

  // Calls ProcessMessage() on |validating_authenticator_| and blocks until
  // the result callback is called.
  void SendMessageAndWaitForCallback();

  // Used to set up our mock behaviors on the MockAuthenticator object passed
  // to |validating_authenticator_|.  Lifetime of the object is controlled by
  // |validating_authenticator_| so this pointer is no longer valid once
  // the owner is destroyed.
  testing::NiceMock<MockAuthenticator>* mock_authenticator_ = nullptr;

  // This member is used to drive behavior in |validating_authenticator_| when
  // its validation complete callback is run.
  ValidationResult validation_result_ = ValidationResult::SUCCESS;

  // Tracks whether our validation callback has been called or not.
  bool validate_complete_called_ = false;

  // The object under test.
  std::unique_ptr<ValidatingAuthenticator> validating_authenticator_;

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;

  DISALLOW_COPY_AND_ASSIGN(ValidatingAuthenticatorTest);
};

ValidatingAuthenticatorTest::ValidatingAuthenticatorTest() = default;

ValidatingAuthenticatorTest::~ValidatingAuthenticatorTest() = default;

void ValidatingAuthenticatorTest::ValidateCallback(
    const std::string& remote_jid,
    ValidatingAuthenticator::ResultCallback callback) {
  validate_complete_called_ = true;
  std::move(callback).Run(validation_result_);
}

void ValidatingAuthenticatorTest::SetUp() {
  mock_authenticator_ = new testing::NiceMock<MockAuthenticator>();
  std::unique_ptr<Authenticator> authenticator(mock_authenticator_);

  validating_authenticator_.reset(new ValidatingAuthenticator(
      kRemoteTestJid,
      base::BindRepeating(&ValidatingAuthenticatorTest::ValidateCallback,
                          base::Unretained(this)),
      std::move(authenticator)));
}

void ValidatingAuthenticatorTest::SendMessageAndWaitForCallback() {
  base::RunLoop run_loop;
  std::unique_ptr<jingle_xmpp::XmlElement> first_message(
      Authenticator::CreateEmptyAuthenticatorMessage());
  validating_authenticator_->ProcessMessage(first_message.get(),
                                            run_loop.QuitClosure());
  run_loop.Run();
}

TEST_F(ValidatingAuthenticatorTest, ValidConnection_SingleMessage) {
  EXPECT_CALL(*mock_authenticator_, ProcessMessage(_, _))
      .Times(1)
      .WillOnce(InvokeCallbackArgument<1>());

  ON_CALL(*mock_authenticator_, state())
      .WillByDefault(Return(Authenticator::ACCEPTED));

  SendMessageAndWaitForCallback();
  ASSERT_TRUE(validate_complete_called_);
  ASSERT_EQ(Authenticator::ACCEPTED, validating_authenticator_->state());
}

TEST_F(ValidatingAuthenticatorTest, ValidConnection_TwoMessages) {
  // Send the first message to the authenticator, set the mock up to act
  // like it is waiting for a second message.
  EXPECT_CALL(*mock_authenticator_, ProcessMessage(_, _))
      .Times(2)
      .WillRepeatedly(InvokeCallbackArgument<1>());

  EXPECT_CALL(*mock_authenticator_, state())
      .WillRepeatedly(Return(Authenticator::MESSAGE_READY));

  SendMessageAndWaitForCallback();
  ASSERT_FALSE(validate_complete_called_);
  ASSERT_EQ(Authenticator::MESSAGE_READY, validating_authenticator_->state());

  // Now 'retrieve' the message for the client which resets the state.
  EXPECT_CALL(*mock_authenticator_, state())
      .WillRepeatedly(Return(Authenticator::WAITING_MESSAGE));

  // This dance is needed because GMock doesn't handle unique_ptrs very well.
  // The mock method receives a raw pointer which it wraps and returns when
  // GetNextMessage() is called.
  std::unique_ptr<jingle_xmpp::XmlElement> next_message(
      Authenticator::CreateEmptyAuthenticatorMessage());
  EXPECT_CALL(*mock_authenticator_, GetNextMessagePtr())
      .Times(1)
      .WillOnce(Return(next_message.release()));

  validating_authenticator_->GetNextMessage();
  ASSERT_EQ(Authenticator::WAITING_MESSAGE, validating_authenticator_->state());

  // Now send the second message for processing.
  EXPECT_CALL(*mock_authenticator_, state())
      .WillRepeatedly(Return(Authenticator::ACCEPTED));

  SendMessageAndWaitForCallback();
  ASSERT_TRUE(validate_complete_called_);
  ASSERT_EQ(Authenticator::ACCEPTED, validating_authenticator_->state());
}

TEST_F(ValidatingAuthenticatorTest, ValidConnection_SendBeforeAccept) {
  // This test simulates an authenticator which needs to send a message before
  // transitioning to the ACCEPTED state.
  EXPECT_CALL(*mock_authenticator_, ProcessMessage(_, _))
      .Times(1)
      .WillRepeatedly(InvokeCallbackArgument<1>());

  EXPECT_CALL(*mock_authenticator_, state())
      .WillOnce(Return(Authenticator::MESSAGE_READY))
      .WillOnce(Return(Authenticator::ACCEPTED));

  // This dance is needed because GMock doesn't handle unique_ptrs very well.
  // The mock method receives a raw pointer which it wraps and returns when
  // GetNextMessage() is called.
  std::unique_ptr<jingle_xmpp::XmlElement> next_message(
      Authenticator::CreateEmptyAuthenticatorMessage());
  EXPECT_CALL(*mock_authenticator_, GetNextMessagePtr())
      .Times(1)
      .WillOnce(Return(next_message.release()));

  SendMessageAndWaitForCallback();
  ASSERT_TRUE(validate_complete_called_);
  ASSERT_EQ(Authenticator::MESSAGE_READY, validating_authenticator_->state());

  // Now 'retrieve' the message for the client which resets the state.
  validating_authenticator_->GetNextMessage();
  ASSERT_EQ(Authenticator::ACCEPTED, validating_authenticator_->state());
}

TEST_F(ValidatingAuthenticatorTest, ValidConnection_ErrorInvalidCredentials) {
  EXPECT_CALL(*mock_authenticator_, ProcessMessage(_, _))
      .Times(1)
      .WillOnce(InvokeCallbackArgument<1>());

  ON_CALL(*mock_authenticator_, state())
      .WillByDefault(Return(Authenticator::ACCEPTED));

  validation_result_ = ValidationResult::ERROR_INVALID_CREDENTIALS;

  SendMessageAndWaitForCallback();
  ASSERT_TRUE(validate_complete_called_);
  ASSERT_EQ(Authenticator::REJECTED, validating_authenticator_->state());
  ASSERT_EQ(Authenticator::INVALID_CREDENTIALS,
            validating_authenticator_->rejection_reason());
}

TEST_F(ValidatingAuthenticatorTest, ValidConnection_ErrorRejectedByUser) {
  EXPECT_CALL(*mock_authenticator_, ProcessMessage(_, _))
      .Times(1)
      .WillOnce(InvokeCallbackArgument<1>());

  ON_CALL(*mock_authenticator_, state())
      .WillByDefault(Return(Authenticator::ACCEPTED));

  validation_result_ = ValidationResult::ERROR_REJECTED_BY_USER;

  SendMessageAndWaitForCallback();
  ASSERT_TRUE(validate_complete_called_);
  ASSERT_EQ(Authenticator::REJECTED, validating_authenticator_->state());
  ASSERT_EQ(Authenticator::REJECTED_BY_USER,
            validating_authenticator_->rejection_reason());
}

TEST_F(ValidatingAuthenticatorTest, ValidConnection_ErrorTooManyConnections) {
  EXPECT_CALL(*mock_authenticator_, ProcessMessage(_, _))
      .Times(1)
      .WillOnce(InvokeCallbackArgument<1>());

  ON_CALL(*mock_authenticator_, state())
      .WillByDefault(Return(Authenticator::ACCEPTED));

  validation_result_ = ValidationResult::ERROR_TOO_MANY_CONNECTIONS;

  SendMessageAndWaitForCallback();
  ASSERT_TRUE(validate_complete_called_);
  ASSERT_EQ(Authenticator::REJECTED, validating_authenticator_->state());
  ASSERT_EQ(Authenticator::TOO_MANY_CONNECTIONS,
            validating_authenticator_->rejection_reason());
}

TEST_F(ValidatingAuthenticatorTest, InvalidConnection_InvalidCredentials) {
  EXPECT_CALL(*mock_authenticator_, ProcessMessage(_, _))
      .Times(1)
      .WillOnce(InvokeCallbackArgument<1>());

  ON_CALL(*mock_authenticator_, state())
      .WillByDefault(Return(Authenticator::REJECTED));

  ON_CALL(*mock_authenticator_, rejection_reason())
      .WillByDefault(Return(Authenticator::INVALID_CREDENTIALS));

  // Verify validation callback is not called for invalid connections.
  SendMessageAndWaitForCallback();
  ASSERT_FALSE(validate_complete_called_);
  ASSERT_EQ(Authenticator::REJECTED, validating_authenticator_->state());
  ASSERT_EQ(Authenticator::INVALID_CREDENTIALS,
            validating_authenticator_->rejection_reason());
}

TEST_F(ValidatingAuthenticatorTest, InvalidConnection_InvalidAccount) {
  EXPECT_CALL(*mock_authenticator_, ProcessMessage(_, _))
      .Times(1)
      .WillOnce(InvokeCallbackArgument<1>());

  ON_CALL(*mock_authenticator_, state())
      .WillByDefault(Return(Authenticator::REJECTED));

  ON_CALL(*mock_authenticator_, rejection_reason())
      .WillByDefault(Return(Authenticator::INVALID_ACCOUNT));

  // Verify validation callback is not called for invalid connections.
  SendMessageAndWaitForCallback();
  ASSERT_FALSE(validate_complete_called_);
  ASSERT_EQ(Authenticator::REJECTED, validating_authenticator_->state());
  ASSERT_EQ(Authenticator::INVALID_ACCOUNT,
            validating_authenticator_->rejection_reason());
}

TEST_F(ValidatingAuthenticatorTest, InvalidConnection_ProtocolError) {
  EXPECT_CALL(*mock_authenticator_, ProcessMessage(_, _))
      .Times(1)
      .WillOnce(InvokeCallbackArgument<1>());

  ON_CALL(*mock_authenticator_, state())
      .WillByDefault(Return(Authenticator::REJECTED));

  ON_CALL(*mock_authenticator_, rejection_reason())
      .WillByDefault(Return(Authenticator::PROTOCOL_ERROR));

  // Verify validation callback is not called for invalid connections.
  SendMessageAndWaitForCallback();
  ASSERT_FALSE(validate_complete_called_);
  ASSERT_EQ(Authenticator::REJECTED, validating_authenticator_->state());
  ASSERT_EQ(Authenticator::PROTOCOL_ERROR,
            validating_authenticator_->rejection_reason());
}

}  // namespace protocol
}  // namespace remoting
