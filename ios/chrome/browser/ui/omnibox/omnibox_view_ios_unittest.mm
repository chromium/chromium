// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/omnibox_view_ios.h"

#import "base/test/task_environment.h"
#import "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_text_field_legacy.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

class OmniboxViewIOSTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    TestChromeBrowserState::Builder test_cbs_builder;
    browser_state_ = test_cbs_builder.Build();
    mockOmniboxTextfield_ = OCMClassMock([OmniboxTextFieldLegacy class]);
    view_ = std::make_unique<OmniboxViewIOS>(
        mockOmniboxTextfield_, /* WebOmniboxEditModelDelegate*/ nullptr,
        browser_state_.get(),
        /*id<OmniboxCommands>*/ nil);
  }

  // Test broser state.
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  // The tested object.
  std::unique_ptr<OmniboxViewIOS> view_;
  // Mock for the OmniboxTextFieldLegacy.
  id mockOmniboxTextfield_;
  // Message loop for the main test thread.
  base::test::TaskEnvironment environment_;
};

TEST_F(OmniboxViewIOSTest, copyAddsTextToPasteboard) {
  [[UIPasteboard generalPasteboard] setString:@""];

  OCMExpect([mockOmniboxTextfield_ isPreEditing]).andReturn(YES);
  OCMExpect([mockOmniboxTextfield_ preEditText]).andReturn(@"foobar");

  view_->OnCopy();

  EXPECT_TRUE(
      [[[UIPasteboard generalPasteboard] string] isEqualToString:@"foobar"]);
  [mockOmniboxTextfield_ verify];

  // Clear the pasteboard state.
  [[UIPasteboard generalPasteboard] setString:@""];
}

}  // namespace
