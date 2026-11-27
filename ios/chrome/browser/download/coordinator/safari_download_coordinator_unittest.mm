// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/download/coordinator/safari_download_coordinator.h"

#import "base/base_paths.h"
#import "base/files/file_path.h"
#import "base/path_service.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_feature_list.h"
#import "ios/chrome/browser/download/model/download_test_util.h"
#import "ios/chrome/browser/download/model/safari_download_tab_helper.h"
#import "ios/chrome/browser/download/model/safari_download_tab_helper_delegate.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/utils/mime_type_util.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/test/scoped_key_window.h"
#import "ios/web/public/test/fakes/fake_download_task.h"
#import "ios/web/public/test/fakes/fake_navigation_context.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "net/base/apple/url_conversions.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "net/test/embedded_test_server/http_request.h"
#import "net/test/embedded_test_server/http_response.h"
#import "testing/platform_test.h"

using base::test::ios::kWaitForUIElementTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;

namespace {

// The path to the .mobileconfig file on the test server.
const char kMobileConfigPath[] = "/mobileconfig";
const char kCalendarDownloadUrl[] = "https://test.test/calendar.ics";

// Request handler for loading a .mobileconfig file.
std::unique_ptr<net::test_server::HttpResponse> GetMobileConfigResponse(
    const net::test_server::HttpRequest& request) {
  auto result = std::make_unique<net::test_server::BasicHttpResponse>();
  result->set_code(net::HTTP_OK);

  if (request.GetURL().GetPath() == kMobileConfigPath) {
    result->AddCustomHeader("Content-Type", kMobileConfigurationType);
    result->set_content(
        testing::GetTestFileContents(testing::kMobileConfigFilePath));
  }

  return result;
}

// Returns the absolute path for the .ics file in the test data directory.
base::FilePath GetCalendarFilePath() {
  base::FilePath file_path;
  base::PathService::Get(base::DIR_ASSETS, &file_path);
  file_path = file_path.Append(FILE_PATH_LITERAL(testing::kCalendarFilePath));
  return file_path;
}

// Returns the absolute path for the .order file in the test data directory.
base::FilePath GetAppleWalletOrderFilePath() {
  base::FilePath file_path;
  base::PathService::Get(base::DIR_ASSETS, &file_path);
  file_path =
      file_path.Append(FILE_PATH_LITERAL(testing::kAppleWalletOrderFilePath));
  return file_path;
}

class SafariDownloadCoordinatorTest : public PlatformTest {
 protected:
  SafariDownloadCoordinatorTest() {
    profile_ = TestProfileIOS::Builder().Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());
    base_view_controller_ = [[UIViewController alloc] init];
    coordinator_ = [[SafariDownloadCoordinator alloc]
        initWithBaseViewController:base_view_controller_
                           browser:browser_.get()];
    [scoped_key_window_.Get() setRootViewController:base_view_controller_];

    // The Coordinator should install itself as delegate for the existing
    // SafariDownloadTabHelper instances once started.
    InsertWebState(WebStateList::InsertionParams::Automatic().Activate());
    [coordinator_ start];
  }

  ~SafariDownloadCoordinatorTest() override { [coordinator_ stop]; }

  WebStateList* web_state_list() { return browser_->GetWebStateList(); }

  web::FakeWebState* web_state_at(int index) {
    return static_cast<web::FakeWebState*>(
        web_state_list()->GetWebStateAt(index));
  }

  SafariDownloadTabHelper* tab_helper() {
    return SafariDownloadTabHelper::FromWebState(web_state_at(0));
  }

  // Inserts a FakeWebState with SafariDownloadTabHelper attached.
  web::FakeWebState* InsertWebState(
      WebStateList::InsertionParams params =
          WebStateList::InsertionParams::Automatic(),
      bool visible = true) {
    auto web_state = std::make_unique<web::FakeWebState>();
    web::FakeWebState* web_state_ptr = web_state.get();
    if (visible) {
      web_state_ptr->WasShown();
    } else {
      web_state_ptr->WasHidden();
    }
    SafariDownloadTabHelper::CreateForWebState(web_state_ptr);
    web_state_list()->InsertWebState(std::move(web_state), params);
    return web_state_ptr;
  }

  // Starts a calendar download on `web_state`.
  void TriggerCalendarDownload(web::WebState* web_state) {
    auto task = std::make_unique<web::FakeDownloadTask>(
        GURL(kCalendarDownloadUrl), kCalendarMimeType);
    SafariDownloadTabHelper::FromWebState(web_state)->DownloadCalendar(
        std::move(task));
  }

  // Waits until a UIAlertController finishes presenting.
  bool WaitForAlertPresented() {
    return WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, ^{
      UIViewController* presented =
          base_view_controller_.presentedViewController;
      return [presented class] == [UIAlertController class] &&
             !presented.beingPresented;
    });
  }

  // Waits until no view controller is presented.
  bool WaitForAlertDismissed() {
    return WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, ^{
      return base_view_controller_.presentedViewController == nil;
    });
  }

  // Needed for test profile created by TestBrowser().
  web::WebTaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  UIViewController* base_view_controller_;
  SafariDownloadCoordinator* coordinator_;
  ScopedKeyWindow scoped_key_window_;
  base::HistogramTester histogram_tester_;
};

// Tests that the coordinator installs itself as a SafariDownloadTabHelper
// delegate when SafariDownloadTabHelper instances become available.
TEST_F(SafariDownloadCoordinatorTest, InstallDelegates) {
  // Coordinator should install itself as delegate for a new web state.
  auto web_state2 = std::make_unique<web::FakeWebState>();
  auto* web_state_ptr2 = web_state2.get();
  SafariDownloadTabHelper::CreateForWebState(web_state_ptr2);
  EXPECT_FALSE(
      SafariDownloadTabHelper::FromWebState(web_state_ptr2)->delegate());
  browser_->GetWebStateList()->InsertWebState(std::move(web_state2));
  EXPECT_TRUE(
      SafariDownloadTabHelper::FromWebState(web_state_ptr2)->delegate());

  // Coordinator should install itself as delegate for a web state replacing an
  // existing one.
  auto web_state3 = std::make_unique<web::FakeWebState>();
  auto* web_state_ptr3 = web_state3.get();
  SafariDownloadTabHelper::CreateForWebState(web_state_ptr3);
  EXPECT_FALSE(
      SafariDownloadTabHelper::FromWebState(web_state_ptr3)->delegate());
  browser_->GetWebStateList()->ReplaceWebStateAt(0, std::move(web_state3));
  EXPECT_TRUE(
      SafariDownloadTabHelper::FromWebState(web_state_ptr3)->delegate());
}

// Tests presenting an UI alert before downloading a valid .mobileconfig file.
TEST_F(SafariDownloadCoordinatorTest, ValidMobileConfigFile) {
  net::EmbeddedTestServer server;
  server.RegisterRequestHandler(base::BindRepeating(&GetMobileConfigResponse));
  ASSERT_TRUE(server.Start());
  GURL config_url = server.GetURL(kMobileConfigPath);

  [tab_helper()->delegate()
      presentMobileConfigAlertFromURL:net::NSURLWithGURL(config_url)];

  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, ^{
    return [base_view_controller_.presentedViewController class] ==
           [UIAlertController class];
  }));

  histogram_tester_.ExpectUniqueSample(
      kUmaDownloadMobileConfigFileUI,
      static_cast<base::HistogramBase::Sample32>(
          SafariDownloadFileUI::kWarningAlertIsPresented),
      1);
}

// Tests attempting to download an invalid .mobileconfig file.
TEST_F(SafariDownloadCoordinatorTest, InvalidMobileConfigFile) {
  [tab_helper()->delegate() presentMobileConfigAlertFromURL:nil];

  EXPECT_FALSE(WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, ^{
    return [base_view_controller_.presentedViewController class] ==
           [UIAlertController class];
  }));

  histogram_tester_.ExpectUniqueSample(
      kUmaDownloadMobileConfigFileUI,
      static_cast<base::HistogramBase::Sample32>(
          SafariDownloadFileUI::kWarningAlertIsPresented),
      0);
}

// Tests presenting an UI alert before downloading a valid .ics file.
TEST_F(SafariDownloadCoordinatorTest, ValidCalendarFile) {
  base::FilePath path = GetCalendarFilePath();
  NSURL* fileURL =
      [NSURL fileURLWithPath:base::SysUTF8ToNSString(path.value())];

  [tab_helper()->delegate() presentCalendarAlertFromURL:fileURL];

  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, ^{
    return [base_view_controller_.presentedViewController class] ==
           [UIAlertController class];
  }));

  histogram_tester_.ExpectUniqueSample(
      kUmaDownloadCalendarFileUI,
      static_cast<base::HistogramBase::Sample32>(
          SafariDownloadFileUI::kWarningAlertIsPresented),
      1);
}

// Tests attempting to download an invalid .ics file.
TEST_F(SafariDownloadCoordinatorTest, InvalidCalendarFile) {
  [tab_helper()->delegate() presentCalendarAlertFromURL:nil];

  EXPECT_FALSE(WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, ^{
    return [base_view_controller_.presentedViewController class] ==
           [UIAlertController class];
  }));

  histogram_tester_.ExpectUniqueSample(
      kUmaDownloadCalendarFileUI,
      static_cast<base::HistogramBase::Sample32>(
          SafariDownloadFileUI::kWarningAlertIsPresented),
      0);
}

// Tests presenting an UI alert before downloading a valid .order file.
TEST_F(SafariDownloadCoordinatorTest, ValidAppleWalletOrderFile) {
  base::FilePath path = GetAppleWalletOrderFilePath();
  NSURL* fileURL =
      [NSURL fileURLWithPath:base::SysUTF8ToNSString(path.value())];

  [tab_helper()->delegate() presentAppleWalletOrderAlertFromURL:fileURL];

  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, ^{
    return [base_view_controller_.presentedViewController class] ==
           [UIAlertController class];
  }));

  histogram_tester_.ExpectUniqueSample(
      kUmaDownloadAppleWalletOrderFileUI,
      static_cast<base::HistogramBase::Sample32>(
          SafariDownloadFileUI::kWarningAlertIsPresented),
      1);
}

// Tests attempting to download an invalid .order file.
TEST_F(SafariDownloadCoordinatorTest, InvalidAppleWalletOrderFile) {
  [tab_helper()->delegate() presentAppleWalletOrderAlertFromURL:nil];

  EXPECT_FALSE(WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, ^{
    return [base_view_controller_.presentedViewController class] ==
           [UIAlertController class];
  }));

  histogram_tester_.ExpectUniqueSample(
      kUmaDownloadAppleWalletOrderFileUI,
      static_cast<base::HistogramBase::Sample32>(
          SafariDownloadFileUI::kWarningAlertIsPresented),
      0);
}

// Tests that SafariDownloadTabHelper defers UI alert presentation when the
// WebState is hidden.
TEST_F(SafariDownloadCoordinatorTest,
       DeferSafariDownloadPresentationWhenHidden) {
  web::FakeWebState* fake_web_state = static_cast<web::FakeWebState*>(
      browser_->GetWebStateList()->GetWebStateAt(0));
  fake_web_state->WasHidden();

  auto task = std::make_unique<web::FakeDownloadTask>(
      GURL("https://test.test/mobileconfig"), kMobileConfigurationType);

  // Start the download in tab helper.
  tab_helper()->DownloadMobileConfig(std::move(task));

  // The warning alert should not be presented while hidden.
  EXPECT_FALSE(WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, ^{
    return [base_view_controller_.presentedViewController class] ==
           [UIAlertController class];
  }));

  // Now show the web state. The warning alert should be presented.
  fake_web_state->WasShown();

  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, ^{
    return [base_view_controller_.presentedViewController class] ==
           [UIAlertController class];
  }));
}

// Tests alert dismissal when the active WebState closes and the previous tab
// becomes active.
TEST_F(SafariDownloadCoordinatorTest, DismissAlertOnActiveWebStateClosed) {
  web::WebState* first_web_state = web_state_at(0);
  web::FakeWebState* second_web_state =
      InsertWebState(WebStateList::InsertionParams::AtIndex(1).Activate());

  TriggerCalendarDownload(second_web_state);
  ASSERT_TRUE(WaitForAlertPresented());

  web_state_list()->CloseWebStateAt(1,
                                    WebStateList::ClosingReason::kUserAction);
  EXPECT_EQ(first_web_state, web_state_list()->GetActiveWebState());
  EXPECT_TRUE(WaitForAlertDismissed());
}

// Tests alert dismissal when the active WebState closes mid-presentation.
TEST_F(SafariDownloadCoordinatorTest, DismissAlertWhenClosedMidPresentation) {
  TriggerCalendarDownload(web_state_at(0));
  ASSERT_NE(nil, base_view_controller_.presentedViewController);
  ASSERT_TRUE(base_view_controller_.presentedViewController.beingPresented);

  web_state_list()->CloseWebStateAt(0,
                                    WebStateList::ClosingReason::kUserAction);
  EXPECT_TRUE(WaitForAlertDismissed());
}

// Tests that a download alert is dismissed if the coordinator is stopped and
// deallocated mid-presentation.
TEST_F(SafariDownloadCoordinatorTest,
       DismissAlertWhenCoordinatorDeallocatedMidPresentation) {
  __weak SafariDownloadCoordinator* weak_coordinator = coordinator_;
  @autoreleasepool {
    TriggerCalendarDownload(web_state_at(0));
    ASSERT_NE(nil, base_view_controller_.presentedViewController);
    ASSERT_TRUE(base_view_controller_.presentedViewController.beingPresented);

    [coordinator_ stop];
    coordinator_ = nil;
  }
  ASSERT_EQ(nil, weak_coordinator);

  EXPECT_TRUE(WaitForAlertDismissed());
}

// Tests alert dismissal when switching to another WebState.
TEST_F(SafariDownloadCoordinatorTest, DismissAlertOnActiveWebStateSwitched) {
  InsertWebState(WebStateList::InsertionParams::AtIndex(1), /*visible=*/false);

  TriggerCalendarDownload(web_state_at(0));
  ASSERT_TRUE(WaitForAlertPresented());

  web_state_list()->ActivateWebStateAt(1);
  EXPECT_TRUE(WaitForAlertDismissed());
}

// Tests alert dismissal when the active WebState is hidden.
TEST_F(SafariDownloadCoordinatorTest, DismissAlertOnActiveWebStateHidden) {
  web::FakeWebState* active_web_state = web_state_at(0);

  TriggerCalendarDownload(active_web_state);
  ASSERT_TRUE(WaitForAlertPresented());

  active_web_state->WasHidden();
  EXPECT_TRUE(WaitForAlertDismissed());
}

// Tests that cross-document navigation dismisses the alert only for the active
// WebState.
TEST_F(SafariDownloadCoordinatorTest,
       DismissAlertOnlyOnActiveWebStateNavigation) {
  web::FakeWebState* active_web_state = web_state_at(0);
  web::FakeWebState* background_web_state = InsertWebState(
      WebStateList::InsertionParams::AtIndex(1), /*visible=*/false);

  TriggerCalendarDownload(active_web_state);
  ASSERT_TRUE(WaitForAlertPresented());
  UIViewController* presented_alert =
      base_view_controller_.presentedViewController;

  web::FakeNavigationContext context;
  context.SetIsSameDocument(false);
  context.SetHasCommitted(true);
  background_web_state->OnNavigationStarted(&context);
  background_web_state->OnNavigationFinished(&context);
  EXPECT_EQ(presented_alert, base_view_controller_.presentedViewController);
  EXPECT_FALSE(presented_alert.beingDismissed);

  active_web_state->OnNavigationStarted(&context);
  EXPECT_TRUE(WaitForAlertDismissed());
}

// Tests that a duplicate download while an alert is presented is ignored and
// does not prevent dismissing the original alert.
TEST_F(SafariDownloadCoordinatorTest,
       IgnoreDuplicateAlertPresentationAndStillDismiss) {
  web::WebState* active_web_state = web_state_at(0);

  TriggerCalendarDownload(active_web_state);
  ASSERT_TRUE(WaitForAlertPresented());
  UIViewController* first_alert = base_view_controller_.presentedViewController;

  TriggerCalendarDownload(active_web_state);
  EXPECT_EQ(first_alert, base_view_controller_.presentedViewController);
  histogram_tester_.ExpectUniqueSample(
      kUmaDownloadCalendarFileUI,
      static_cast<base::HistogramBase::Sample32>(
          SafariDownloadFileUI::kWarningAlertIsPresented),
      1);

  web_state_list()->CloseWebStateAt(0,
                                    WebStateList::ClosingReason::kUserAction);
  EXPECT_TRUE(WaitForAlertDismissed());
}

// Tests that a download alert is not presented over another modal.
TEST_F(SafariDownloadCoordinatorTest,
       DoNotPresentAlertWhenAnotherViewControllerIsPresented) {
  UIViewController* presented_view_controller = [[UIViewController alloc] init];
  [base_view_controller_ presentViewController:presented_view_controller
                                      animated:NO
                                    completion:nil];
  ASSERT_EQ(presented_view_controller,
            base_view_controller_.presentedViewController);

  TriggerCalendarDownload(web_state_at(0));
  EXPECT_EQ(presented_view_controller,
            base_view_controller_.presentedViewController);
  histogram_tester_.ExpectTotalCount(kUmaDownloadCalendarFileUI, 0);

  [base_view_controller_ dismissViewControllerAnimated:NO completion:nil];
}

}  // namespace
