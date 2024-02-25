// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/model/public/web_content_area/insecure_form_overlay.h"

#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/overlays/model/public/overlay_request.h"
#import "ios/chrome/browser/overlays/model/public/overlay_response.h"
#import "ios/chrome/browser/overlays/model/public/web_content_area/alert_overlay.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "ui/base/l10n/l10n_util.h"

using alert_overlays::AlertRequest;
using alert_overlays::AlertResponse;
using alert_overlays::ButtonConfig;

// Test fixture for app launcher overlays.
using InsecureFormOverlayTest = PlatformTest;

// Tests that the alert overlay request is set correctly for the first app
// launch request.
TEST_F(InsecureFormOverlayTest, FirstRequestAlertSetup) {
  std::unique_ptr<OverlayRequest> request =
      OverlayRequest::CreateWithConfig<InsecureFormOverlayRequestConfig>();
  AlertRequest* config = request->GetConfig<AlertRequest>();
  ASSERT_TRUE(config);

  NSString* alert_title =
      l10n_util::GetNSStringWithFixup(IDS_INSECURE_FORM_HEADING);
  EXPECT_NSEQ(alert_title, config->title());

  ASSERT_EQ(1U, config->button_configs().size());
  const std::vector<ButtonConfig>& button_configs = config->button_configs()[0];
  ASSERT_EQ(2U, button_configs.size());

  // Check that the first button is cancel.
  ButtonConfig cancel_button = button_configs[0];
  EXPECT_NSEQ(l10n_util::GetNSString(IDS_CANCEL), cancel_button.title);
  EXPECT_EQ(UIAlertActionStyleCancel, cancel_button.style);

  // Check the send button.
  ButtonConfig send_button = button_configs[1];
  EXPECT_NSEQ(l10n_util::GetNSString(IDS_INSECURE_FORM_SUBMIT_BUTTON),
              send_button.title);
}
