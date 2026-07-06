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
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/test/test_render_frame_host.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

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
    NavigateAndCommit(GURL("https://example.com/"));
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

  FrameTreeNode* AppendChildFrame(const std::string& name) {
    FrameTreeNode* root_node =
        contents()->GetPrimaryMainFrame()->frame_tree_node();
    TestRenderFrameHost* child_rfh =
        static_cast<TestRenderFrameHost*>(root_node->current_frame_host())
            ->AppendChild(name);
    return child_rfh->frame_tree_node();
  }

  VirtualWallet* WalletForFrame(FrameTreeNode* node) {
    return DigitalCredentialEnvironment::GetInstance()->MaybeGetVirtualWallet(
        node);
  }

  std::unique_ptr<DigitalCredentialsHandler> handler_;
};

TEST_F(DigitalCredentialsHandlerTest, NoFrameHostReturnsInvalidParams) {
  auto response = handler_->SetVirtualWalletBehavior(
      DigitalCredentials::VirtualWalletActionEnum::Respond,
      /*in_protocol=*/std::nullopt, /*in_response=*/nullptr,
      /*in_frame_id=*/std::nullopt);
  EXPECT_EQ(crdtp::DispatchCode::INVALID_PARAMS, response.Code());
}

TEST_F(DigitalCredentialsHandlerTest, ClearResetsWalletState) {
  AttachToMainFrame();
  handler_->SetVirtualWalletBehavior(
      DigitalCredentials::VirtualWalletActionEnum::Respond,
      /*in_protocol=*/"openid4vp",
      /*in_response=*/CreateDictValue(R"({"presentation":"abc"})"),
      /*in_frame_id=*/std::nullopt);

  auto response = handler_->SetVirtualWalletBehavior(
      DigitalCredentials::VirtualWalletActionEnum::Clear,
      /*in_protocol=*/std::nullopt, /*in_response=*/nullptr,
      /*in_frame_id=*/std::nullopt);

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
      DigitalCredentials::VirtualWalletActionEnum::Respond,
      /*in_protocol=*/std::nullopt,
      /*in_response=*/CreateDictValue(R"({"foo":1})"),
      /*in_frame_id=*/std::nullopt);

  EXPECT_EQ(crdtp::DispatchCode::INVALID_PARAMS, response.Code());
}

TEST_F(DigitalCredentialsHandlerTest,
       RespondWithoutResponseReturnsInvalidParams) {
  AttachToMainFrame();

  auto response = handler_->SetVirtualWalletBehavior(
      DigitalCredentials::VirtualWalletActionEnum::Respond,
      /*in_protocol=*/"openid4vp", /*in_response=*/nullptr,
      /*in_frame_id=*/std::nullopt);

  EXPECT_EQ(crdtp::DispatchCode::INVALID_PARAMS, response.Code());
}

TEST_F(DigitalCredentialsHandlerTest, RespondStoresCredentialAndSetsBehavior) {
  AttachToMainFrame();

  auto response = handler_->SetVirtualWalletBehavior(
      DigitalCredentials::VirtualWalletActionEnum::Respond,
      /*in_protocol=*/"openid4vp",
      /*in_response=*/CreateDictValue(R"({"presentation":"abc"})"),
      /*in_frame_id=*/std::nullopt);

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
      DigitalCredentials::VirtualWalletActionEnum::Decline,
      /*in_protocol=*/std::nullopt, /*in_response=*/nullptr,
      /*in_frame_id=*/std::nullopt);

  EXPECT_EQ(crdtp::DispatchCode::SUCCESS, response.Code());
  VirtualWallet* wallet = WalletForMainFrame();
  ASSERT_NE(nullptr, wallet);
  ASSERT_TRUE(wallet->action().has_value());
  EXPECT_EQ(VirtualWallet::Action::kDecline, *wallet->action());
}

TEST_F(DigitalCredentialsHandlerTest, WaitSetsBehaviorKWait) {
  AttachToMainFrame();

  auto response = handler_->SetVirtualWalletBehavior(
      DigitalCredentials::VirtualWalletActionEnum::Wait,
      /*in_protocol=*/std::nullopt, /*in_response=*/nullptr,
      /*in_frame_id=*/std::nullopt);

  EXPECT_EQ(crdtp::DispatchCode::SUCCESS, response.Code());
  VirtualWallet* wallet = WalletForMainFrame();
  ASSERT_NE(nullptr, wallet);
  ASSERT_TRUE(wallet->action().has_value());
  EXPECT_EQ(VirtualWallet::Action::kWait, *wallet->action());
}

TEST_F(DigitalCredentialsHandlerTest, UnknownActionReturnsInvalidParams) {
  AttachToMainFrame();

  auto response = handler_->SetVirtualWalletBehavior(
      /*in_action=*/"test", /*in_protocol=*/std::nullopt,
      /*in_response=*/nullptr, /*in_frame_id=*/std::nullopt);

  EXPECT_EQ(crdtp::DispatchCode::INVALID_PARAMS, response.Code());
}

TEST_F(DigitalCredentialsHandlerTest,
       NonRespondActionWithExtraParamsReturnsInvalidParams) {
  AttachToMainFrame();

  auto response = handler_->SetVirtualWalletBehavior(
      DigitalCredentials::VirtualWalletActionEnum::Decline,
      /*in_protocol=*/"openid4vp", /*in_response=*/nullptr,
      /*in_frame_id=*/std::nullopt);

  EXPECT_EQ(crdtp::DispatchCode::INVALID_PARAMS, response.Code());
}

TEST_F(DigitalCredentialsHandlerTest, MainFrameDefault) {
  AttachToMainFrame();

  auto response = handler_->SetVirtualWalletBehavior(
      DigitalCredentials::VirtualWalletActionEnum::Decline,
      /*in_protocol=*/std::nullopt, /*in_response=*/nullptr,
      /*in_frame_id=*/std::nullopt);

  EXPECT_EQ(crdtp::DispatchCode::SUCCESS, response.Code());
  VirtualWallet* wallet = WalletForMainFrame();
  ASSERT_NE(nullptr, wallet);
  ASSERT_TRUE(wallet->action().has_value());
  EXPECT_EQ(VirtualWallet::Action::kDecline, *wallet->action());
}

TEST_F(DigitalCredentialsHandlerTest, TargetedIframe) {
  AttachToMainFrame();
  FrameTreeNode* iframe = AppendChildFrame("iframe");
  ASSERT_NE(nullptr, iframe);
  std::string frame_token =
      iframe->current_frame_host()->devtools_frame_token().ToString();

  auto response = handler_->SetVirtualWalletBehavior(
      DigitalCredentials::VirtualWalletActionEnum::Respond,
      /*in_protocol=*/"openid4vp",
      /*in_response=*/CreateDictValue(R"({"presentation":"iframe_token"})"),
      /*in_frame_id=*/frame_token);

  EXPECT_EQ(crdtp::DispatchCode::SUCCESS, response.Code());
  VirtualWallet* iframe_wallet = WalletForFrame(iframe);
  ASSERT_NE(nullptr, iframe_wallet);
  ASSERT_TRUE(iframe_wallet->action().has_value());
  EXPECT_EQ(VirtualWallet::Action::kRespond, *iframe_wallet->action());
}

TEST_F(DigitalCredentialsHandlerTest, ScopingIsolation) {
  AttachToMainFrame();
  FrameTreeNode* iframe1 = AppendChildFrame("iframe1");
  FrameTreeNode* iframe2 = AppendChildFrame("iframe2");
  ASSERT_NE(nullptr, iframe1);
  ASSERT_NE(nullptr, iframe2);
  std::string frame_token1 =
      iframe1->current_frame_host()->devtools_frame_token().ToString();

  auto response = handler_->SetVirtualWalletBehavior(
      DigitalCredentials::VirtualWalletActionEnum::Decline,
      /*in_protocol=*/std::nullopt, /*in_response=*/nullptr,
      /*in_frame_id=*/frame_token1);

  EXPECT_EQ(crdtp::DispatchCode::SUCCESS, response.Code());
  VirtualWallet* wallet1 = WalletForFrame(iframe1);
  ASSERT_NE(nullptr, wallet1);
  EXPECT_EQ(VirtualWallet::Action::kDecline, *wallet1->action());

  // Main frame and sibling iframe should not have virtual wallets created or
  // configured.
  EXPECT_EQ(nullptr, WalletForMainFrame());
  EXPECT_EQ(nullptr, WalletForFrame(iframe2));
}

TEST_F(DigitalCredentialsHandlerTest, InvalidFrameId) {
  AttachToMainFrame();

  auto response = handler_->SetVirtualWalletBehavior(
      DigitalCredentials::VirtualWalletActionEnum::Decline,
      /*in_protocol=*/std::nullopt, /*in_response=*/nullptr,
      /*in_frame_id=*/"invalid-frame-id");

  EXPECT_EQ(crdtp::DispatchCode::INVALID_PARAMS, response.Code());
}

}  // namespace content::protocol
