// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/pam_authorization_factory_posix.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "remoting/base/username.h"
#include "remoting/host/pam_utils.h"
#include "remoting/protocol/authenticator.h"
#include "remoting/protocol/fake_authenticator.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

namespace {

class TestUnderlyingAuthenticator : public protocol::FakeAuthenticator {
 public:
  TestUnderlyingAuthenticator()
      : protocol::FakeAuthenticator(protocol::FakeAuthenticator::CLIENT,
                                    protocol::FakeAuthenticator::Config(
                                        1,
                                        protocol::FakeAuthenticator::ACCEPT,
                                        false),
                                    "local",
                                    "remote") {}

  void set_on_get_next_message(base::OnceClosure callback) {
    on_get_next_message_ = std::move(callback);
  }

  void set_on_process_message(base::OnceClosure callback) {
    on_process_message_ = std::move(callback);
  }

  // protocol::Authenticator interface.
  JingleAuthentication GetNextMessage() override {
    JingleAuthentication result = protocol::FakeAuthenticator::GetNextMessage();
    if (on_get_next_message_) {
      std::move(on_get_next_message_).Run();
    }
    return result;
  }

  void ProcessMessage(const JingleAuthentication& message,
                      base::OnceClosure resume_callback) override {
    protocol::FakeAuthenticator::ProcessMessage(message,
                                                std::move(resume_callback));
    if (on_process_message_) {
      std::move(on_process_message_).Run();
    }
  }

 private:
  base::OnceClosure on_get_next_message_;
  base::OnceClosure on_process_message_;
};

class TestUnderlyingFactory : public protocol::AuthenticatorFactory {
 public:
  TestUnderlyingFactory() = default;
  ~TestUnderlyingFactory() override = default;

  void set_authenticator(
      std::unique_ptr<protocol::Authenticator> authenticator) {
    authenticator_ = std::move(authenticator);
  }

  std::unique_ptr<protocol::Authenticator> CreateAuthenticator(
      const std::string& local_jid,
      const std::string& remote_jid) override {
    return std::move(authenticator_);
  }

  std::unique_ptr<protocol::AuthenticatorFactory> Clone() const override {
    return nullptr;
  }

 private:
  std::unique_ptr<protocol::Authenticator> authenticator_;
};

}  // namespace

class PamAuthorizerUafTest : public testing::Test {
 public:
  PamAuthorizerUafTest() = default;
  ~PamAuthorizerUafTest() override = default;

 protected:
  void SetUp() override {
    auto underlying_factory = std::make_unique<TestUnderlyingFactory>();
    underlying_factory_ptr_ = underlying_factory.get();
    pam_factory_ = std::make_unique<PamAuthorizationFactory>(
        std::move(underlying_factory));
  }

  void TearDown() override {
    underlying_factory_ptr_ = nullptr;
    pam_factory_.reset();
  }

  std::unique_ptr<PamAuthorizationFactory> pam_factory_;
  raw_ptr<TestUnderlyingFactory> underlying_factory_ptr_;
};

TEST_F(PamAuthorizerUafTest, GetNextMessage_FreesThisDuringUnderlyingCall) {
  auto underlying = std::make_unique<TestUnderlyingAuthenticator>();
  TestUnderlyingAuthenticator* underlying_ptr = underlying.get();
  underlying_factory_ptr_->set_authenticator(std::move(underlying));

  std::unique_ptr<protocol::Authenticator> pam_authorizer =
      pam_factory_->CreateAuthenticator("local", "remote");

  // Configure the underlying authenticator to synchronously destroy the
  // PamAuthorizer wrapper when GetNextMessage() is called. This simulates
  // the UAF condition where the session is torn down during an authenticator
  // call.
  underlying_ptr->set_on_get_next_message(base::BindOnce(
      [](std::unique_ptr<protocol::Authenticator>* authorizer) {
        authorizer->reset();
      },
      &pam_authorizer));

  // This should not crash if fixed. Without the fix, it triggers UAF.
  pam_authorizer->GetNextMessage();
}

TEST_F(PamAuthorizerUafTest, ProcessMessage_FreesThisDuringUnderlyingCall) {
  auto underlying = std::make_unique<TestUnderlyingAuthenticator>();
  TestUnderlyingAuthenticator* underlying_ptr = underlying.get();
  underlying_factory_ptr_->set_authenticator(std::move(underlying));

  std::unique_ptr<protocol::Authenticator> pam_authorizer =
      pam_factory_->CreateAuthenticator("local", "remote");

  // Advance to WAITING_MESSAGE state.
  pam_authorizer->GetNextMessage();

  // Configure the underlying authenticator to synchronously destroy the
  // PamAuthorizer wrapper when ProcessMessage() is called. This simulates
  // the UAF condition where the session is torn down during an authenticator
  // call.
  underlying_ptr->set_on_process_message(base::BindOnce(
      [](std::unique_ptr<protocol::Authenticator>* authorizer) {
        authorizer->reset();
      },
      &pam_authorizer));

  // This should not crash if fixed.
  JingleAuthentication message;
  message.id = "remote";
  message.test_id = "1";
  message.test_key = {'t', 'e', 's', 't', '_', 'k', 'e', 'y'};
  pam_authorizer->ProcessMessage(message, base::DoNothing());
}

}  // namespace remoting
