// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/webui/net_export_tab_helper.h"

#import <Foundation/Foundation.h>

#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/webui/net_export_tab_helper_delegate.h"
#import "ios/chrome/browser/webui/show_mail_composer_context.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// A test object that conforms to the NetExportTabHelperDelegate protocol.
@interface TestNetExportTabHelperDelegate : NSObject<NetExportTabHelperDelegate>

// The last context passed to `netExportTabHelper:showMailComposerWithContext:`.
// `lastContext` is nil if `netExportTabHelper:showMailComposerWithContext:` has
// never been called.
@property(nonatomic, readonly, strong) ShowMailComposerContext* lastContext;

@end

@implementation TestNetExportTabHelperDelegate
@synthesize lastContext = _lastContext;

- (void)netExportTabHelper:(NetExportTabHelper*)tabHelper
    showMailComposerWithContext:(ShowMailComposerContext*)context {
  _lastContext = context;
}

@end

// Test fixture for testing NetExportTabHelper.
class NetExportTabHelperTest : public PlatformTest {
 public:
  NetExportTabHelperTest()
      : delegate_([[TestNetExportTabHelperDelegate alloc] init]) {
    browser_state_ = TestChromeBrowserState::Builder().Build();

    web::WebState::CreateParams params(browser_state_.get());
    web_state_ = web::WebState::Create(params);
  }

  NetExportTabHelperTest(const NetExportTabHelperTest&) = delete;
  NetExportTabHelperTest& operator=(const NetExportTabHelperTest&) = delete;

 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    NetExportTabHelper::CreateForWebState(web_state());
    NetExportTabHelper::FromWebState(web_state())->SetDelegate(delegate_);
  }

  web::WebState* web_state() { return web_state_.get(); }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  std::unique_ptr<web::WebState> web_state_;
  // A delegate that is given to the NetExportTabHelper for testing.
  __strong TestNetExportTabHelperDelegate* delegate_;
};

// Verifies the initial state of the NetExportTabHelper and its delegate.
TEST_F(NetExportTabHelperTest, TestInitialState) {
  NetExportTabHelper* helper = NetExportTabHelper::FromWebState(web_state());

  EXPECT_TRUE(helper);
  // `lastContext` should not exist yet, as
  // `netExportTabHelper:showMailComposerWithContext:` has not been called.
  EXPECT_FALSE(delegate_.lastContext);
}

// Verifies that the delegate is instructed to show the mail composer with the
// correct context object when the NetExportTabHelper is told to do so.
TEST_F(NetExportTabHelperTest, TestShowMailComposer) {
  NetExportTabHelper* helper = NetExportTabHelper::FromWebState(web_state());
  ShowMailComposerContext* context =
      [[ShowMailComposerContext alloc] initWithToRecipients:nil
                                                    subject:@"subject"
                                                       body:@"body"
                             emailNotConfiguredAlertTitleId:IDS_OK
                           emailNotConfiguredAlertMessageId:IDS_OK];

  helper->ShowMailComposer(context);

  EXPECT_TRUE(delegate_.lastContext);
  EXPECT_EQ(0U, [delegate_.lastContext.toRecipients count]);
  EXPECT_NSEQ(@"subject", delegate_.lastContext.subject);
  EXPECT_NSEQ(@"body", delegate_.lastContext.body);
  EXPECT_EQ(IDS_OK, delegate_.lastContext.emailNotConfiguredAlertTitleId);
}
