// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/autofill/suggestions_from_gemini/ui/suggestions_from_gemini_table_view_controller.h"

#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_controller_test.h"
#import "ios/chrome/grit/ios_strings.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

class SuggestionsFromGeminiTableViewControllerTest
    : public LegacyChromeTableViewControllerTest {
 protected:
  LegacyChromeTableViewController* InstantiateController() override {
    return [[SuggestionsFromGeminiTableViewController alloc] init];
  }
};

// Tests that the SuggestionsFromGemini subpage is correctly initialized.
TEST_F(SuggestionsFromGeminiTableViewControllerTest, TestInitialization) {
  CreateController();
  CheckController();

  EXPECT_NSEQ(
      l10n_util::GetNSString(IDS_IOS_PERSONAL_CONTEXT_AUTOFILL_SETTINGS_TITLE),
      controller().title);
}

}  // namespace
