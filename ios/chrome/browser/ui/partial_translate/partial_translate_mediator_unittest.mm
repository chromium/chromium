// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/partial_translate/partial_translate_mediator.h"

#import <Foundation/Foundation.h>

#import "base/ios/ios_util.h"
#import "base/memory/raw_ptr.h"
#import "base/test/ios/wait_util.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_feature_list.h"
#import "components/sync_preferences/pref_service_syncable.h"
#import "components/sync_preferences/testing_pref_service_syncable.h"
#import "components/translate/core/browser/translate_pref_names.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/test/fake_web_state_list_delegate.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_opener.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/ui/browser_container/edit_menu_alert_delegate.h"
#import "ios/chrome/browser/web/model/chrome_web_client.h"
#import "ios/chrome/browser/web_selection/model/web_selection_tab_helper.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/chrome/test/providers/partial_translate/test_partial_translate.h"
#import "ios/web/public/test/scoped_testing_web_client.h"
#import "ios/web/public/test/web_state_test_util.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

namespace {

// A template for an HTML page containing a selection.
// "TEMPLATE_SELECTION" can be replaced by a custom text depending on the test.
NSString* kPageHTMLTemplate =
    @"<html>"
     "  <body>"
     "    This text contains a <span id='selectid'>TEMPLATE_SELECTION</span>."
     "  </body>"
     "</html>";
}  // namespace

// A fake alert controller that immediately calls one of the actions.
@interface FakeAlertController : NSObject <EditMenuAlertDelegate>
// The index of the action that will be called in `actions`.
@property(nonatomic, assign) NSUInteger selectedAction;

// Tracks if the controller was called.
@property(nonatomic, assign) BOOL called;

@end

@implementation FakeAlertController

// Present the alert with the given title, message and actions.
// Immediately calls `actions[selectedAction]`.
- (void)showAlertWithTitle:(NSString*)title
                   message:(NSString*)message
                   actions:(NSArray<EditMenuAlertDelegateAction*>*)actions {
  self.called = YES;
  actions[self.selectedAction].action();
}

@end

// A fake PartialTranslateController that keep track of its latest call.
@interface FakePartialTranslateController
    : NSObject <PartialTranslateController>
// The latest values passed on all parameters.
@property(nonatomic, copy) NSString* sourceText;
@property(nonatomic, assign) CGRect anchor;
@property(nonatomic, assign) BOOL inIncognito;
@property(nonatomic, assign) BOOL shouldSucceed;
@property(nonatomic, strong) UIViewController* baseViewController;
@end

@implementation FakePartialTranslateController

- (instancetype)initWithSourceText:(NSString*)sourceText
                        anchorRect:(const CGRect&)anchor
                       inIncognito:(BOOL)inIncognito
                     shouldSucceed:(BOOL)shouldSucceed {
  self = [super init];
  if (self) {
    _sourceText = [sourceText copy];
    _anchor = anchor;
    _inIncognito = inIncognito;
    _shouldSucceed = shouldSucceed;
  }
  return self;
}

- (void)presentOnViewController:(UIViewController*)viewController
          flowCompletionHandler:(void (^)(BOOL))flowCompletionHandler {
  _baseViewController = viewController;
  if (flowCompletionHandler) {
    flowCompletionHandler(_shouldSucceed);
  }
}

@end

// A fake partial translate provider.
@interface FakePartialTranslateControllerFactory
    : NSObject <PartialTranslateControllerFactory>
@property(nonatomic, assign) BOOL shouldSucceed;
@property(nonatomic, strong) FakePartialTranslateController* latestController;
@end

@implementation FakePartialTranslateControllerFactory
- (instancetype)initWithSuccess:(BOOL)success {
  self = [super init];
  if (self) {
    _shouldSucceed = success;
  }
  return self;
}

- (id<PartialTranslateController>)
    createTranslateControllerForSourceText:(NSString*)sourceText
                                anchorRect:(CGRect)anchor
                               inIncognito:(BOOL)inIncognito {
  self.latestController = [[FakePartialTranslateController alloc]
      initWithSourceText:sourceText
              anchorRect:anchor
             inIncognito:inIncognito
           shouldSucceed:_shouldSucceed];
  return self.latestController;
}

- (NSUInteger)maximumCharacterLimit {
  return 1100;
}

@end

class PartialTranslateMediatorTest : public PlatformTest {
 public:
  PartialTranslateMediatorTest()
      : web_client_(std::make_unique<ChromeWebClient>()),
        web_state_list_(&web_state_list_delegate_) {
    profile_ = TestProfileIOS::Builder().Build();

    web::WebState::CreateParams params(profile_.get());
    auto web_state = web::WebState::Create(params);
    WebSelectionTabHelper::CreateForWebState(web_state.get());
    web_state_list_.InsertWebState(
        std::move(web_state),
        WebStateList::InsertionParams::Automatic().Activate());
    web_state_ = web_state_list_.GetActiveWebState();
    base_view_controller_ = [[UIViewController alloc] init];
    fake_alert_controller_ = [[FakeAlertController alloc] init];
    mock_browser_coordinator_commands_handler_ =
        OCMStrictProtocolMock(@protocol(BrowserCoordinatorCommands));
    mediator_ = [[PartialTranslateMediator alloc]
          initWithWebStateList:&web_state_list_
        withBaseViewController:base_view_controller_
                   prefService:profile_->GetSyncablePrefs()
          fullscreenController:nullptr
                     incognito:NO];
    mediator_.alertDelegate = fake_alert_controller_;
    mediator_.browserHandler = mock_browser_coordinator_commands_handler_;
  }

  void TearDown() override {
    [mediator_ shutdown];
    // Reset the factory
    ios::provider::test::SetPartialTranslateControllerFactory(nil);
    PlatformTest::TearDown();
  }

  // Create a factory for the Partial translate provider.
  // `shouldSucceed` indicates whether the PartialTranslateController
  // created by the factory should succeed when presented.
  FakePartialTranslateControllerFactory* SetupTranslateControllerFactory(
      bool shouldSucceed) {
    FakePartialTranslateControllerFactory* factory =
        [[FakePartialTranslateControllerFactory alloc]
            initWithSuccess:shouldSucceed];
    ios::provider::test::SetPartialTranslateControllerFactory(factory);
    return factory;
  }

  // Loads an HTML page and selects a text containing `size` characters.
  void LoadPageAndSelectSize(int size, NSString* filler = @"A") {
    NSString* pageHTML = [kPageHTMLTemplate
        stringByReplacingOccurrencesOfString:@"TEMPLATE_SELECTION"
                                  withString:[@"" stringByPaddingToLength:size
                                                               withString:filler
                                                          startingAtIndex:0]];
    web::test::LoadHtml(pageHTML, web_state_);
    web::test::ExecuteJavaScript(@"window.getSelection().selectAllChildren("
                                  "document.getElementById('selectid'));",
                                 web_state_);
  }

  // Indicates to the mocks that we expect a Show translate command.
  void ExpectShowTranslate() {
    OCMExpect([mock_browser_coordinator_commands_handler_ showTranslate]);
  }

 protected:
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  web::WebTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  web::ScopedTestingWebClient web_client_;
  std::unique_ptr<TestProfileIOS> profile_;
  FakeWebStateListDelegate web_state_list_delegate_;
  WebStateList web_state_list_;
  raw_ptr<web::WebState> web_state_;
  UIViewController* base_view_controller_;
  FakeAlertController* fake_alert_controller_;
  id mock_browser_coordinator_commands_handler_;
  PartialTranslateMediator* mediator_;
};

// Tests the behavior if partial translate is not supported.
TEST_F(PartialTranslateMediatorTest, NotSupported) {
  if (!base::ios::IsRunningOnIOS16OrLater()) {
    // Partial translate not supported before iOS16.
    return;
  }
  EXPECT_FALSE([mediator_ shouldInstallPartialTranslate]);
  EXPECT_FALSE([mediator_ canHandlePartialTranslateSelection]);
}

// Tests the behavior if partial translate is disabled by policy.
TEST_F(PartialTranslateMediatorTest, EnterpriseDisabled) {
  if (!base::ios::IsRunningOnIOS16OrLater()) {
    // Partial translate not supported before iOS16.
    return;
  }
  LoadPageAndSelectSize(10);
  auto factory = SetupTranslateControllerFactory(true);

  base::Value managed_value(false);
  profile_->GetTestingPrefService()->SetManagedPref(
      translate::prefs::kOfferTranslateEnabled, managed_value.Clone());
  EXPECT_FALSE([mediator_ shouldInstallPartialTranslate]);
  EXPECT_NSEQ(nil, factory.latestController);
}

// Tests the behavior in incognito.
TEST_F(PartialTranslateMediatorTest, IncognitoSupportedSuccess) {
  PartialTranslateMediator* mediator = [[PartialTranslateMediator alloc]
        initWithWebStateList:&web_state_list_
      withBaseViewController:base_view_controller_
                 prefService:profile_->GetSyncablePrefs()
        fullscreenController:nullptr
                   incognito:YES];
  base::HistogramTester histogram_tester;
  LoadPageAndSelectSize(10);
  auto factory = SetupTranslateControllerFactory(true);
  EXPECT_TRUE([mediator shouldInstallPartialTranslate]);
  EXPECT_TRUE([mediator canHandlePartialTranslateSelection]);
  [mediator handlePartialTranslateSelection];
  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForJSCompletionTimeout, /*run_message_loop=*/true,
      ^{
        return factory.latestController != nil;
      }));
  EXPECT_NSEQ(@"AAAAAAAAAA", factory.latestController.sourceText);
  histogram_tester.ExpectBucketCount("IOS.PartialTranslate.Outcome",
                                     0 /* kSuccess */, 1);
}

// Tests the behavior in incognito if not supported.
TEST_F(PartialTranslateMediatorTest, IncognitoNotSupported) {
  if (!base::ios::IsRunningOnIOS16OrLater()) {
    // Partial translate not supported before iOS16.
    return;
  }
  PartialTranslateMediator* mediator = [[PartialTranslateMediator alloc]
        initWithWebStateList:&web_state_list_
      withBaseViewController:base_view_controller_
                 prefService:profile_->GetSyncablePrefs()
        fullscreenController:nullptr
                   incognito:YES];
  EXPECT_FALSE([mediator shouldInstallPartialTranslate]);
}

// Tests the behavior if partial translate is supported.
TEST_F(PartialTranslateMediatorTest, SupportedSuccess) {
  if (!base::ios::IsRunningOnIOS16OrLater()) {
    // Partial translate not supported before iOS16.
    return;
  }
  base::HistogramTester histogram_tester;
  LoadPageAndSelectSize(10);
  auto factory = SetupTranslateControllerFactory(true);
  EXPECT_TRUE([mediator_ shouldInstallPartialTranslate]);
  EXPECT_TRUE([mediator_ canHandlePartialTranslateSelection]);
  [mediator_ handlePartialTranslateSelection];

  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForJSCompletionTimeout, /*run_message_loop=*/true,
      ^{
        return factory.latestController != nil;
      }));
  EXPECT_NSEQ(@"AAAAAAAAAA", factory.latestController.sourceText);
  histogram_tester.ExpectBucketCount("IOS.PartialTranslate.Outcome",
                                     0 /* kSuccess */, 1);
}

// Tests the behavior if selection is too long.
TEST_F(PartialTranslateMediatorTest, StringTooLongCancel) {
  if (!base::ios::IsRunningOnIOS16OrLater()) {
    // Partial translate not supported before iOS16.
    return;
  }
  base::HistogramTester histogram_tester;
  LoadPageAndSelectSize(1001);
  auto factory = SetupTranslateControllerFactory(true);

  EXPECT_TRUE([mediator_ shouldInstallPartialTranslate]);
  EXPECT_TRUE([mediator_ canHandlePartialTranslateSelection]);
  [mediator_ handlePartialTranslateSelection];
  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForJSCompletionTimeout, /*run_message_loop=*/true,
      ^{
        return fake_alert_controller_.called;
      }));
  EXPECT_NSEQ(nil, factory.latestController.sourceText);
  histogram_tester.ExpectBucketCount("IOS.PartialTranslate.Outcome",
                                     1 /* kTooLongCancel */, 1);
}

// Tests the behavior if selection is too long.
TEST_F(PartialTranslateMediatorTest, StringTooLongFullTranslate) {
  if (!base::ios::IsRunningOnIOS16OrLater()) {
    // Partial translate not supported before iOS16.
    return;
  }
  base::HistogramTester histogram_tester;
  LoadPageAndSelectSize(1001);
  auto factory = SetupTranslateControllerFactory(true);
  fake_alert_controller_.selectedAction = 1;

  EXPECT_TRUE([mediator_ shouldInstallPartialTranslate]);
  EXPECT_TRUE([mediator_ canHandlePartialTranslateSelection]);
  ExpectShowTranslate();
  [mediator_ handlePartialTranslateSelection];
  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForJSCompletionTimeout, /*run_message_loop=*/true,
      ^{
        return fake_alert_controller_.called;
      }));
  EXPECT_NSEQ(nil, factory.latestController.sourceText);
  histogram_tester.ExpectBucketCount("IOS.PartialTranslate.Outcome",
                                     2 /* kTooLongFullTranslate */, 1);
}

// Tests the behavior if selection is empty.
TEST_F(PartialTranslateMediatorTest, StringEmptyCancel) {
  if (!base::ios::IsRunningOnIOS16OrLater()) {
    // Partial translate not supported before iOS16.
    return;
  }
  base::HistogramTester histogram_tester;
  LoadPageAndSelectSize(0);
  auto factory = SetupTranslateControllerFactory(true);

  EXPECT_TRUE([mediator_ shouldInstallPartialTranslate]);
  EXPECT_TRUE([mediator_ canHandlePartialTranslateSelection]);
  [mediator_ handlePartialTranslateSelection];
  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForJSCompletionTimeout, /*run_message_loop=*/true,
      ^{
        return fake_alert_controller_.called;
      }));
  EXPECT_NSEQ(nil, factory.latestController.sourceText);
  histogram_tester.ExpectBucketCount("IOS.PartialTranslate.Outcome",
                                     3 /* kEmptyCancel */, 1);
}

// Tests the behavior if selection is only spaces.
TEST_F(PartialTranslateMediatorTest, StringSpacesCancel) {
  if (!base::ios::IsRunningOnIOS16OrLater()) {
    // Partial translate not supported before iOS16.
    return;
  }
  base::HistogramTester histogram_tester;
  LoadPageAndSelectSize(5, @" ");
  auto factory = SetupTranslateControllerFactory(true);

  EXPECT_TRUE([mediator_ shouldInstallPartialTranslate]);
  EXPECT_TRUE([mediator_ canHandlePartialTranslateSelection]);
  [mediator_ handlePartialTranslateSelection];
  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForJSCompletionTimeout, /*run_message_loop=*/true,
      ^{
        return fake_alert_controller_.called;
      }));
  EXPECT_NSEQ(nil, factory.latestController.sourceText);
  histogram_tester.ExpectBucketCount("IOS.PartialTranslate.Outcome",
                                     3 /* kEmptyCancel */, 1);
}

// Tests the behavior if selection is empty.
TEST_F(PartialTranslateMediatorTest, StringEmptyFullTranslate) {
  if (!base::ios::IsRunningOnIOS16OrLater()) {
    // Partial translate not supported before iOS16.
    return;
  }
  base::HistogramTester histogram_tester;
  LoadPageAndSelectSize(0);
  auto factory = SetupTranslateControllerFactory(true);
  fake_alert_controller_.selectedAction = 1;

  EXPECT_TRUE([mediator_ shouldInstallPartialTranslate]);
  EXPECT_TRUE([mediator_ canHandlePartialTranslateSelection]);
  ExpectShowTranslate();
  [mediator_ handlePartialTranslateSelection];
  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForJSCompletionTimeout, /*run_message_loop=*/true,
      ^{
        return fake_alert_controller_.called;
      }));
  EXPECT_NSEQ(nil, factory.latestController.sourceText);
  histogram_tester.ExpectBucketCount("IOS.PartialTranslate.Outcome",
                                     4 /* kEmptyFullTranslate */, 1);
}

// Tests the behavior if an error occurs.
TEST_F(PartialTranslateMediatorTest, InternalErrorCancel) {
  if (!base::ios::IsRunningOnIOS16OrLater()) {
    // Partial translate not supported before iOS16.
    return;
  }
  base::HistogramTester histogram_tester;
  LoadPageAndSelectSize(10);
  auto factory = SetupTranslateControllerFactory(false);

  EXPECT_TRUE([mediator_ shouldInstallPartialTranslate]);
  EXPECT_TRUE([mediator_ canHandlePartialTranslateSelection]);
  [mediator_ handlePartialTranslateSelection];
  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForJSCompletionTimeout, /*run_message_loop=*/true,
      ^{
        return fake_alert_controller_.called;
      }));
  EXPECT_NSEQ(@"AAAAAAAAAA", factory.latestController.sourceText);
  histogram_tester.ExpectBucketCount("IOS.PartialTranslate.Outcome",
                                     5 /* kErrorCancel */, 1);
}

// Tests the behavior if an error occurs.
TEST_F(PartialTranslateMediatorTest, InternalErrorFullTranslate) {
  if (!base::ios::IsRunningOnIOS16OrLater()) {
    // Partial translate not supported before iOS16.
    return;
  }
  base::HistogramTester histogram_tester;
  LoadPageAndSelectSize(10);
  auto factory = SetupTranslateControllerFactory(false);
  fake_alert_controller_.selectedAction = 1;

  EXPECT_TRUE([mediator_ shouldInstallPartialTranslate]);
  EXPECT_TRUE([mediator_ canHandlePartialTranslateSelection]);
  ExpectShowTranslate();
  [mediator_ handlePartialTranslateSelection];
  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForJSCompletionTimeout, /*run_message_loop=*/true,
      ^{
        return fake_alert_controller_.called;
      }));
  EXPECT_NSEQ(@"AAAAAAAAAA", factory.latestController.sourceText);
  histogram_tester.ExpectBucketCount("IOS.PartialTranslate.Outcome",
                                     6 /* kErrorFullTranslate */, 1);
}
