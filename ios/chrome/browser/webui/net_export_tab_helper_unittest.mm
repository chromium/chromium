// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/webui/net_export_tab_helper.h"

#import <Foundation/Foundation.h>

#include "base/macros.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/webui/net_export_tab_helper_delegate.h"
#import "ios/chrome/browser/webui/show_mail_composer_context.h"
#import "ios/web/public/test/web_test_with_web_state.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// A test object that conforms to the NetExportTabHelperDelegate protocol.
@interface TestNetExportTabHelperDelegate : NSObject<NetExportTabHelperDelegate>

// The last context passed to |netExportTabHelper:showMailComposerWithContext:|.
// |lastContext| is nil if |netExportTabHelper:showMailComposerWithContext:| has
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
class NetExportTabHelperTest : public web::WebTestWithWebState {
 public:
  NetExportTabHelperTest()
      : delegate_([[TestNetExportTabHelperDelegate alloc] init]) {}

 protected:
  void SetUp() override {
    web::WebTestWithWebState::SetUp();
    NetExportTabHelper::CreateForWebState(web_state(), delegate_);
  }

  // A delegate that is given to the NetExportTabHelper for testing.
  __strong TestNetExportTabHelperDelegate* delegate_;

 private:
  DISALLOW_COPY_AND_ASSIGN(NetExportTabHelperTest);
};

// Verifies the initial state of the NetExportTabHelper and its delegate.
TEST_F(NetExportTabHelperTest, TestInitialState) {
  NetExportTabHelper* helper = NetExportTabHelper::FromWebState(web_state());

  EXPECT_TRUE(helper);
  // |lastContext| should not exist yet, as
  // |netExportTabHelper:showMailComposerWithContext:| has not been called.
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
