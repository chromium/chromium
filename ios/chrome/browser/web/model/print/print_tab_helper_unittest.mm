// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/model/print/print_tab_helper.h"

#import "base/test/task_environment.h"
#import "components/sync_preferences/pref_service_mock_factory.h"
#import "components/sync_preferences/pref_service_syncable.h"
#import "ios/chrome/browser/shared/model/prefs/browser_prefs.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/web/model/print/web_state_printer.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/web_state.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

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
    RegisterProfilePrefs(registry.get());
    sync_preferences::PrefServiceMockFactory factory;

    TestProfileIOS::Builder test_cbs_builder;
    test_cbs_builder.SetPrefService(factory.CreateSyncable(registry.get()));
    profile_ = std::move(test_cbs_builder).Build();
    web_state_.SetBrowserState(profile_.get());

    printer_ = [[PrintTabHelperTestPrinter alloc] init];
    PrintTabHelper::GetOrCreateForWebState(&web_state_)->set_printer(printer_);
  }

  PrintTabHelperTestPrinter* printer_;
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  web::FakeWebState web_state_;
};

// Tests printing when the pref controlling printing is enabled.
TEST_F(PrintTabHelperTest, PrintEnabled) {
  ASSERT_FALSE(printer_.printInvoked);

  PrintTabHelper::GetOrCreateForWebState(&web_state_)->Print();
  EXPECT_TRUE(printer_.printInvoked);
}

// Tests printing when the pref controlling printing is disabled.
TEST_F(PrintTabHelperTest, PrintDisabled) {
  ASSERT_FALSE(printer_.printInvoked);
  profile_->GetPrefs()->SetBoolean(prefs::kPrintingEnabled, false);

  PrintTabHelper::GetOrCreateForWebState(&web_state_)->Print();
  EXPECT_FALSE(printer_.printInvoked);
}
