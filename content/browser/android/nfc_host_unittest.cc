// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/nfc_host.h"

#include "base/memory/raw_ptr.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/test/mock_permission_controller.h"
#include "content/public/test/test_browser_context.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "services/device/public/mojom/nfc.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"

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

    auto mock_permission_controller =
        std::make_unique<MockPermissionController>();
    mock_permission_controller_ = mock_permission_controller.get();
    static_cast<TestBrowserContext*>(browser_context())
        ->SetPermissionControllerForTesting(
            std::move(mock_permission_controller));
  }

  MockPermissionController& mock_permission_controller() {
    return *mock_permission_controller_;
  }

 private:
  raw_ptr<MockPermissionController> mock_permission_controller_;
};

TEST_F(NFCHostTest, GetNFCTwice) {
  constexpr MockPermissionController::SubscriptionId kSubscriptionId(42);

  NavigateAndCommit(GURL(kTestUrl));

  EXPECT_CALL(mock_permission_controller(),
              GetPermissionStatusForCurrentDocument(blink::PermissionType::NFC,
                                                    main_rfh()))
      .WillOnce(Return(blink::mojom::PermissionStatus::GRANTED))
      .WillOnce(Return(blink::mojom::PermissionStatus::GRANTED));
  EXPECT_CALL(mock_permission_controller(),
              SubscribeToPermissionStatusChange(
                  blink::PermissionType::NFC, /*render_process_host=*/nullptr,
                  main_rfh(),
                  main_rfh()->GetMainFrame()->GetLastCommittedOrigin().GetURL(),
                  /*should_include_device_status*/ false, _))
      .WillOnce(Return(kSubscriptionId));

  mojo::Remote<device::mojom::NFC> nfc1, nfc2;
  contents()->GetNFC(main_rfh(), nfc1.BindNewPipeAndPassReceiver());
  contents()->GetNFC(main_rfh(), nfc2.BindNewPipeAndPassReceiver());

  nfc1.FlushForTesting();
  nfc2.FlushForTesting();
  EXPECT_TRUE(nfc1.is_bound());
  EXPECT_TRUE(nfc2.is_bound());

  EXPECT_CALL(mock_permission_controller(),
              UnsubscribeFromPermissionStatusChange(kSubscriptionId));

  DeleteContents();
}

}  // namespace content
