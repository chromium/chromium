// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/link_to_text/link_to_text_mediator.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#import "base/strings/sys_string_conversions.h"
#import "base/test/scoped_feature_list.h"
#import "ios/chrome/browser/link_to_text/link_to_text_tab_helper.h"
#import "ios/chrome/browser/ui/commands/activity_service_commands.h"
#import "ios/chrome/browser/ui/commands/share_highlight_command.h"
#import "ios/chrome/browser/ui/ui_feature_flags.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_list_delegate.h"
#import "ios/chrome/browser/web_state_list/web_state_opener.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

using web::TestWebState;

namespace {
class TestWebStateListDelegate : public WebStateListDelegate {
  void WillAddWebState(web::WebState* web_state) override {}
  void WebStateDetached(web::WebState* web_state) override {}
};
}  // namespace

class LinkToTextMediatorTest : public PlatformTest {
 protected:
  LinkToTextMediatorTest()
      : web_state_list_delegate_(), web_state_list_(&web_state_list_delegate_) {
    feature_list_.InitAndEnableFeature(kSharedHighlightingIOS);
    mocked_handler_ = OCMStrictProtocolMock(@protocol(ActivityServiceCommands));

    auto web_state = std::make_unique<TestWebState>();
    web_state_ = web_state.get();
    web_state_list_.InsertWebState(0, std::move(web_state),
                                   WebStateList::INSERT_ACTIVATE,
                                   WebStateOpener());

    LinkToTextTabHelper::CreateForWebState(web_state_);

    mediator_ =
        [[LinkToTextMediator alloc] initWithWebStateList:&web_state_list_
                                                 handler:mocked_handler_];
  }

  base::test::ScopedFeatureList feature_list_;
  TestWebStateListDelegate web_state_list_delegate_;
  WebStateList web_state_list_;
  TestWebState* web_state_;
  LinkToTextMediator* mediator_;
  id mocked_handler_;
};

TEST_F(LinkToTextMediatorTest, ShouldOfferLinkToText) {
  EXPECT_TRUE([mediator_ shouldOfferLinkToText]);
}

TEST_F(LinkToTextMediatorTest, HandleLinkToTextSelectionTriggersCommand) {
  [[mocked_handler_ expect] shareHighlight:[OCMArg any]];
  [mediator_ handleLinkToTextSelection];
  [mocked_handler_ verify];
}
