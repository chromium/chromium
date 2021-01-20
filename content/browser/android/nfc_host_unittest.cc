// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/nfc_host.h"

#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/test/mock_permission_manager.h"
#include "content/public/test/test_browser_context.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "services/device/public/mojom/nfc.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Return;

namespace content {

namespace {

constexpr char kTestUrl[] = "https://www.google.com";

}

class NFCHostTest : public RenderViewHostImplTestHarness {
 protected:
  NFCHostTest() = default;
  ~NFCHostTest() override = default;

  NFCHostTest(const NFCHostTest& other) = delete;
  NFCHostTest& operator=(const NFCHostTest& other) = delete;

  void SetUp() override {
    RenderViewHostImplTestHarness::SetUp();
    auto mock_permission_manager = std::make_unique<MockPermissionManager>();
    mock_permission_manager_ = mock_permission_manager.get();
    static_cast<TestBrowserContext*>(browser_context())
        ->SetPermissionControllerDelegate(std::move(mock_permission_manager));
  }

  MockPermissionManager& mock_permission_manager() {
    return *mock_permission_manager_;
  }

 private:
  MockPermissionManager* mock_permission_manager_;
};

TEST_F(NFCHostTest, GetNFCTwice) {
  constexpr int kSubscriptionId = 42;

  NavigateAndCommit(GURL(kTestUrl));

  EXPECT_CALL(mock_permission_manager(),
              GetPermissionStatusForFrame(PermissionType::NFC, main_rfh(), _))
      .WillOnce(Return(blink::mojom::PermissionStatus::GRANTED))
      .WillOnce(Return(blink::mojom::PermissionStatus::GRANTED));
  EXPECT_CALL(mock_permission_manager(),
              SubscribePermissionStatusChange(PermissionType::NFC, main_rfh(),
                                              GURL(kTestUrl), _))
      .WillOnce(Return(kSubscriptionId));

  mojo::Remote<device::mojom::NFC> nfc1, nfc2;
  contents()->GetNFC(main_rfh(), nfc1.BindNewPipeAndPassReceiver());
  contents()->GetNFC(main_rfh(), nfc2.BindNewPipeAndPassReceiver());

  nfc1.FlushForTesting();
  nfc2.FlushForTesting();
  EXPECT_TRUE(nfc1.is_bound());
  EXPECT_TRUE(nfc2.is_bound());

  EXPECT_CALL(mock_permission_manager(),
              UnsubscribePermissionStatusChange(kSubscriptionId));

  DeleteContents();
}

}  // namespace content
