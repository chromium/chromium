// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/omnibox/model/omnibox_controller_ios.h"

#import <stddef.h>

#import <memory>

#import "base/test/task_environment.h"
#import "components/omnibox/browser/autocomplete_classifier.h"
#import "components/omnibox/browser/autocomplete_controller.h"
#import "components/omnibox/browser/autocomplete_provider.h"
#import "components/omnibox/browser/test_omnibox_client.h"
#import "components/open_from_clipboard/fake_clipboard_recent_content.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

class OmniboxControllerIOSTest : public PlatformTest {
 protected:
  OmniboxControllerIOSTest() {
    // AutocompleteController needs an instance of ClipboardRecentContent to
    // create the ClipboardProvider. The instance can be nullptr in iOS tests.
    ClipboardRecentContent::SetInstance(
        std::make_unique<FakeClipboardRecentContent>());

    omnibox_controller_ = std::make_unique<OmniboxControllerIOS>(
        /*view=*/nullptr, std::make_unique<TestOmniboxClient>());
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<OmniboxControllerIOS> omnibox_controller_;
};

// Tests that the list of autocomplete providers created by the
// OmniboxController matches the expectations.
TEST_F(OmniboxControllerIOSTest, CheckDefaultAutocompleteProviders) {
  int expected_providers = AutocompleteClassifier::DefaultOmniboxProviders();

  for (const auto& provider :
       omnibox_controller_->autocomplete_controller()->providers()) {
    // Ensure this is a provider we wanted.
    int type = provider->type();
    ASSERT_TRUE(expected_providers & type);
    // Remove it from expectations so we fail if it's there twice.
    expected_providers &= ~type;
  }

  // Ensure we saw all the providers we expected.
  ASSERT_EQ(0, expected_providers);
}
