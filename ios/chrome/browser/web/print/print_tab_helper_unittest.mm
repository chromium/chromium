// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/print/print_tab_helper.h"

#include "base/test/task_environment.h"
#include "components/sync_preferences/pref_service_mock_factory.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/chrome/browser/pref_names.h"
#include "ios/chrome/browser/prefs/browser_prefs.h"
#import "ios/chrome/browser/web/print/web_state_printer.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/web_state.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface PrintTabHelperTestPrinter : NSObject <WebStatePrinter>
@property(nonatomic, readwrite) BOOL printInvoked;
@end

@implementation PrintTabHelperTestPrinter
- (void)printWebState:(web::WebState*)webState {
  self.printInvoked = YES;
}

- (void)printWebState:(web::WebState*)webState
    baseViewController:(UIViewController*)baseViewController {
  self.printInvoked = YES;
}
@end

class PrintTabHelperTest : public PlatformTest {
 protected:
  PrintTabHelperTest() {
    scoped_refptr<user_prefs::PrefRegistrySyncable> registry =
        base::MakeRefCounted<user_prefs::PrefRegistrySyncable>();
    RegisterBrowserStatePrefs(registry.get());
    sync_preferences::PrefServiceMockFactory factory;

    TestChromeBrowserState::Builder test_cbs_builder;
    test_cbs_builder.SetPrefService(factory.CreateSyncable(registry.get()));
    chrome_browser_state_ = test_cbs_builder.Build();
    web_state_.SetBrowserState(chrome_browser_state_.get());

    printer_ = [[PrintTabHelperTestPrinter alloc] init];
    PrintTabHelper::CreateForWebState(&web_state_);
    PrintTabHelper::FromWebState(&web_state_)->set_printer(printer_);
  }

  PrintTabHelperTestPrinter* printer_;
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
  web::FakeWebState web_state_;
};

// Tests printing when the pref controlling printing is enabled.
TEST_F(PrintTabHelperTest, PrintEnabled) {
  ASSERT_FALSE(printer_.printInvoked);

  PrintTabHelper::FromWebState(&web_state_)->Print();
  EXPECT_TRUE(printer_.printInvoked);
}

// Tests printing when the pref controlling printing is disabled.
TEST_F(PrintTabHelperTest, PrintDisabled) {
  ASSERT_FALSE(printer_.printInvoked);
  chrome_browser_state_->GetPrefs()->SetBoolean(prefs::kPrintingEnabled, false);

  PrintTabHelper::FromWebState(&web_state_)->Print();
  EXPECT_FALSE(printer_.printInvoked);
}
