// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/protocol/digital_credentials_handler.h"

#include <memory>

#include "base/test/gtest_util.h"
#include "base/test/values_test_util.h"
#include "base/values.h"
#include "content/browser/digital_credentials/digital_credential_environment.h"
#include "content/browser/digital_credentials/virtual_wallet.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content::protocol {

namespace {

// Parses a JSON object string into a dictionary value.
std::unique_ptr<base::DictValue> CreateDictValue(std::string_view json) {
  return std::make_unique<base::DictValue>(base::test::ParseJsonDict(json));
}

}  // namespace

class DigitalCredentialsHandlerTest : public RenderViewHostImplTestHarness {
 public:
  void SetUp() override {
    RenderViewHostImplTestHarness::SetUp();
    handler_ = std::make_unique<DigitalCredentialsHandler>();
  }

  void TearDown() override {
    handler_.reset();
    // VirtualWallets live in a process-wide singleton; reset between tests
    // to keep them independent.
    DigitalCredentialEnvironment::GetInstance()->Reset();
    RenderViewHostImplTestHarness::TearDown();
  }

 protected:
  void AttachToMainFrame() {
    handler_->SetRenderer(/*process_host_id=*/0,
                          contents()->GetPrimaryMainFrame());
  }

  VirtualWallet* WalletForMainFrame() {
    return DigitalCredentialEnvironment::GetInstance()->MaybeGetVirtualWallet(
        contents()->GetPrimaryMainFrame()->frame_tree_node());
  }

  std::unique_ptr<DigitalCredentialsHandler> handler_;
};

TEST_F(DigitalCredentialsHandlerTest, NoFrameHostReturnsInvalidParams) {
  auto response = handler_->SetVirtualWalletBehavior(
      DigitalCredentials::VirtualWalletActionEnum::Respond, std::nullopt,
      nullptr);
  EXPECT_EQ(crdtp::DispatchCode::INVALID_PARAMS, response.Code());
}

TEST_F(DigitalCredentialsHandlerTest, ClearResetsWalletState) {
  AttachToMainFrame();
  handler_->SetVirtualWalletBehavior(
      DigitalCredentials::VirtualWalletActionEnum::Respond, "openid4vp",
      CreateDictValue(R"({"presentation":"abc"})"));

  auto response = handler_->SetVirtualWalletBehavior(
      DigitalCredentials::VirtualWalletActionEnum::Clear, std::nullopt,
      nullptr);

  EXPECT_EQ(crdtp::DispatchCode::SUCCESS, response.Code());
  VirtualWallet* wallet = WalletForMainFrame();
  ASSERT_NE(nullptr, wallet);
  EXPECT_FALSE(wallet->action().has_value());
  EXPECT_FALSE(wallet->GetCredential().has_value());
}

TEST_F(DigitalCredentialsHandlerTest,
       RespondWithoutProtocolReturnsInvalidParams) {
  AttachToMainFrame();

  auto response = handler_->SetVirtualWalletBehavior(
      DigitalCredentials::VirtualWalletActionEnum::Respond, std::nullopt,
      CreateDictValue(R"({"foo":1})"));

  EXPECT_EQ(crdtp::DispatchCode::INVALID_PARAMS, response.Code());
}

TEST_F(DigitalCredentialsHandlerTest,
       RespondWithoutResponseReturnsInvalidParams) {
  AttachToMainFrame();

  auto response = handler_->SetVirtualWalletBehavior(
      DigitalCredentials::VirtualWalletActionEnum::Respond, "openid4vp",
      nullptr);

  EXPECT_EQ(crdtp::DispatchCode::INVALID_PARAMS, response.Code());
}

TEST_F(DigitalCredentialsHandlerTest, RespondStoresCredentialAndSetsBehavior) {
  AttachToMainFrame();

  auto response = handler_->SetVirtualWalletBehavior(
      DigitalCredentials::VirtualWalletActionEnum::Respond, "openid4vp",
      CreateDictValue(R"({"presentation":"abc"})"));

  EXPECT_EQ(crdtp::DispatchCode::SUCCESS, response.Code());
  VirtualWallet* wallet = WalletForMainFrame();
  ASSERT_NE(nullptr, wallet);
  ASSERT_TRUE(wallet->action().has_value());
  EXPECT_EQ(VirtualWallet::Action::kRespond, *wallet->action());

  auto stored = wallet->GetCredential();
  ASSERT_TRUE(stored.has_value());
  EXPECT_EQ("openid4vp", stored->protocol);
  const std::string* presentation =
      stored->data.GetDict().FindString("presentation");
  ASSERT_TRUE(presentation);
  EXPECT_EQ("abc", *presentation);
}

TEST_F(DigitalCredentialsHandlerTest, DeclineSetsBehaviorKDecline) {
  AttachToMainFrame();

  auto response = handler_->SetVirtualWalletBehavior(
      DigitalCredentials::VirtualWalletActionEnum::Decline, std::nullopt,
      nullptr);

  EXPECT_EQ(crdtp::DispatchCode::SUCCESS, response.Code());
  VirtualWallet* wallet = WalletForMainFrame();
  ASSERT_NE(nullptr, wallet);
  ASSERT_TRUE(wallet->action().has_value());
  EXPECT_EQ(VirtualWallet::Action::kDecline, *wallet->action());
}

TEST_F(DigitalCredentialsHandlerTest, WaitSetsBehaviorKWait) {
  AttachToMainFrame();

  auto response = handler_->SetVirtualWalletBehavior(
      DigitalCredentials::VirtualWalletActionEnum::Wait, std::nullopt, nullptr);

  EXPECT_EQ(crdtp::DispatchCode::SUCCESS, response.Code());
  VirtualWallet* wallet = WalletForMainFrame();
  ASSERT_NE(nullptr, wallet);
  ASSERT_TRUE(wallet->action().has_value());
  EXPECT_EQ(VirtualWallet::Action::kWait, *wallet->action());
}

TEST_F(DigitalCredentialsHandlerTest, UnknownActionReturnsInvalidParams) {
  AttachToMainFrame();

  auto response =
      handler_->SetVirtualWalletBehavior("test", std::nullopt, nullptr);

  EXPECT_EQ(crdtp::DispatchCode::INVALID_PARAMS, response.Code());
}

TEST_F(DigitalCredentialsHandlerTest,
       NonRespondActionWithExtraParamsReturnsInvalidParams) {
  AttachToMainFrame();

  auto response = handler_->SetVirtualWalletBehavior(
      DigitalCredentials::VirtualWalletActionEnum::Decline, "openid4vp",
      nullptr);

  EXPECT_EQ(crdtp::DispatchCode::INVALID_PARAMS, response.Code());
}

}  // namespace content::protocol
