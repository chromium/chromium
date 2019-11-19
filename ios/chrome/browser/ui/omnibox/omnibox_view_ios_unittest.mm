// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/omnibox_view_ios.h"

#include "base/test/task_environment.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"
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
    mockOmniboxTextfield_ = OCMClassMock([OmniboxTextFieldIOS class]);
    view_ = std::make_unique<OmniboxViewIOS>(
        mockOmniboxTextfield_, /* WebOmniboxEditController*/ nullptr,
        /*id<OmniboxLeftImageConsumer> */ nil, browser_state_.get(),
        /*id<OmniboxFocuser>*/ nil);
  }

  // Test broser state.
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  // The tested object.
  std::unique_ptr<OmniboxViewIOS> view_;
  // Mock for the OmniboxTextFieldIOS.
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
