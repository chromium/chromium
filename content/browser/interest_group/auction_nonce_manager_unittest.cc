// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/auction_nonce_manager.h"
#include <memory>
#include <string>

#include "base/strings/string_util.h"
#include "base/test/bind.h"
#include "base/uuid.h"
#include "content/public/test/browser_test_utils.h"
#include "content/test/test_render_frame_host.h"
#include "content/test/test_render_view_host.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace content {

class AuctionNonceManagerTest : public RenderViewHostImplTestHarness {
 public:
  void SetUp() override {
    RenderViewHostImplTestHarness::SetUp();
    NavigateAndCommit(GURL("https://test.org"));
    nonce_manager_ = std::make_unique<AuctionNonceManager>(main_test_rfh());
  }

  void TearDown() override {
    nonce_manager_.reset();
    RenderViewHostImplTestHarness::TearDown();
  }

  // Simpler version of what NavigatorAuction::createAuctionNonce does, which
  // is to take an AuctionNonce and change the last few digits to make it
  // unique.
  AuctionNonce CreateAuctionNonce(uint8_t suffix) {
    base::Uuid base_auction_nonce = main_test_rfh()->GetBaseAuctionNonce();
    std::string nonce = base::StringPrintf(
        "%s%02x", base_auction_nonce.AsLowercaseString().substr(0, 34).c_str(),
        suffix);
    return AuctionNonce(base::Uuid::ParseLowercase(nonce));
  }

  AuctionNonceManager& nonce_manager() { return *nonce_manager_; }

 private:
  std::unique_ptr<AuctionNonceManager> nonce_manager_;
};

TEST_F(AuctionNonceManagerTest, AuctionNonceIsAvailable) {
  AuctionNonce nonce1 = CreateAuctionNonce(1);
  EXPECT_TRUE(nonce_manager().ClaimAuctionNonceIfAvailable(nonce1));

  AuctionNonce nonce2 = CreateAuctionNonce(2);
  EXPECT_TRUE(nonce_manager().ClaimAuctionNonceIfAvailable(nonce2));
}

TEST_F(AuctionNonceManagerTest, AuctionNonceCanOnlyBeClaimedOnce) {
  AuctionNonce nonce = CreateAuctionNonce(1);
  nonce_manager().ClaimAuctionNonceIfAvailable(nonce);
  EXPECT_FALSE(nonce_manager().ClaimAuctionNonceIfAvailable(nonce));

  // Monitor the console errors.
  WebContentsConsoleObserver console_observer(web_contents());
  console_observer.SetFilter(base::BindLambdaForTesting(
      [](const content::WebContentsConsoleObserver::Message& msg) {
        return msg.log_level == blink::mojom::ConsoleMessageLevel::kError;
      }));
  console_observer.SetPattern(
      "Invalid AuctionConfig. The config provided an auctionNonce value "
      "that was _not_ created by a previous call to createAuctionNonce.");

  EXPECT_FALSE(nonce_manager().ClaimAuctionNonceIfAvailable(
      static_cast<AuctionNonce>(base::Uuid::GenerateRandomV4())));

  // Verify the expected error is logged to the console.
  ASSERT_TRUE(console_observer.Wait());
  EXPECT_EQ(console_observer.messages().size(), 1u);
}

TEST_F(AuctionNonceManagerTest, AuctionNonceIsNotAvailable) {
  AuctionNonce nonce(
      base::Uuid::ParseLowercase("a4245d17-4287-47c2-9c1a-1809cf150f76"));

  // Monitor the console errors.
  WebContentsConsoleObserver console_observer(web_contents());
  console_observer.SetFilter(base::BindLambdaForTesting(
      [](const content::WebContentsConsoleObserver::Message& msg) {
        return msg.log_level == blink::mojom::ConsoleMessageLevel::kError;
      }));
  console_observer.SetPattern(
      "Invalid AuctionConfig. The config provided an auctionNonce value "
      "that was _not_ created by a previous call to createAuctionNonce.");

  EXPECT_FALSE(nonce_manager().ClaimAuctionNonceIfAvailable(
      static_cast<AuctionNonce>(base::Uuid::GenerateRandomV4())));

  // Verify the expected error is logged to the console.
  ASSERT_TRUE(console_observer.Wait());
  EXPECT_EQ(console_observer.messages().size(), 1u);
}

}  // namespace content
