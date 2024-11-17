// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/download/ui_bundled/download_manager_coordinator.h"

#import <MobileCoreServices/MobileCoreServices.h>
#import <StoreKit/StoreKit.h>
#import <UIKit/UIKit.h>

#import "base/apple/foundation_util.h"
#import "base/files/file_util.h"
#import "base/run_loop.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/metrics/user_action_tester.h"
#import "base/test/scoped_feature_list.h"
#import "ios/chrome/browser/download/model/confirm_download_closing_overlay.h"
#import "ios/chrome/browser/download/model/confirm_download_replacing_overlay.h"
#import "ios/chrome/browser/download/model/download_directory_util.h"
#import "ios/chrome/browser/download/model/download_manager_metric_names.h"
#import "ios/chrome/browser/download/model/download_manager_tab_helper.h"
#import "ios/chrome/browser/download/model/external_app_util.h"
#import "ios/chrome/browser/download/model/installation_notifier.h"
#import "ios/chrome/browser/download/ui_bundled/download_manager_view_controller_delegate.h"
#import "ios/chrome/browser/download/ui_bundled/legacy_download_manager_view_controller.h"
#import "ios/chrome/browser/overlays/model/public/overlay_request_queue.h"
#import "ios/chrome/browser/overlays/model/public/web_content_area/alert_overlay.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_opener.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/test/fakes/fake_contained_presenter.h"
#import "ios/chrome/test/scoped_key_window.h"
#import "ios/web/public/test/fakes/fake_download_task.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "net/base/net_errors.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

using base::test::ios::WaitUntilConditionOrTimeout;
using base::test::ios::kWaitForUIElementTimeout;

namespace {

// Constants for configuring a fake download task.
const char kTestUrl[] = "https://chromium.test/download.txt";
const char kTestMimeType[] = "text/html";
const int64_t kTestTotalBytes = 10;
const int64_t kTestReceivedBytes = 0;
const base::FilePath::CharType kTestSuggestedFileName[] =
    FILE_PATH_LITERAL("file.zip");

}  // namespace

// Test fixture for testing DownloadManagerCoordinator class.
class DownloadManagerCoordinatorTest : public PlatformTest {
 protected:
  DownloadManagerCoordinatorTest() {
    feature_list_.InitAndDisableFeature(kIOSSaveToDrive);
    profile_ = TestProfileIOS::Builder().Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());
    presenter_ = [[FakeContainedPresenter alloc] init];
    base_view_controller_ = [[UIViewController alloc] init];
    activity_view_controller_class_ =
        OCMClassMock([UIActivityViewController class]);
    OverlayRequestQueue::CreateForWebState(&web_state_);
    DownloadManagerTabHelper::CreateForWebState(&web_state_);
    coordinator_ = [[DownloadManagerCoordinator alloc]
        initWithBaseViewController:base_view_controller_
                           browser:browser_.get()];
    [scoped_key_window_.Get() setRootViewController:base_view_controller_];
    coordinator_.presenter = presenter_;
  }
  ~DownloadManagerCoordinatorTest() override {
    // Stop to avoid holding a dangling pointer to destroyed task.
    @autoreleasepool {
      // Calling -stop will retain and autorelease coordinator_.
      // task_environment_ has to outlive the coordinator, so wrapping -stop
      // call in @autorelease will ensure that coordinator_ is deallocated.
      [coordinator_ stop];
    }

    [activity_view_controller_class_ stopMocking];
    [application_ stopMocking];
    [[InstallationNotifier sharedInstance] stopPolling];
  }

  DownloadManagerTabHelper* tab_helper() {
    return DownloadManagerTabHelper::FromWebState(&web_state_);
  }

  // Creates a fake download task for testing.
  std::unique_ptr<web::FakeDownloadTask> CreateTestTask() {
    auto task =
        std::make_unique<web::FakeDownloadTask>(GURL(kTestUrl), kTestMimeType);
    task->SetTotalBytes(kTestTotalBytes);
    task->SetReceivedBytes(kTestReceivedBytes);
    task->SetGeneratedFileName(base::FilePath(kTestSuggestedFileName));
    task->SetWebState(&web_state_);
    return task;
  }

  base::test::ScopedFeatureList feature_list_;
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  FakeContainedPresenter* presenter_;
  UIViewController* base_view_controller_;
  ScopedKeyWindow scoped_key_window_;
  id activity_view_controller_class_;
  web::FakeWebState web_state_;
  // Application can be lazily created by tests, but it has to be OCMock.
  // Destructor will call -stopMocking on this object to make sure that
  // UIApplication is not mocked after these test finish running.
  id application_;
  DownloadManagerCoordinator* coordinator_;
  base::UserActionTester user_action_tester_;
  base::HistogramTester histogram_tester_;
};

// Tests starting the coordinator. Verifies that view controller is presented
// without animation (default configuration) and that
// LegacyDownloadManagerViewController is propertly configured and presented.
TEST_F(DownloadManagerCoordinatorTest, Start) {
  auto task = CreateTestTask();
  coordinator_.downloadTask = task.get();
  [coordinator_ start];

  // By default coordinator presents without animation.
  EXPECT_FALSE(presenter_.lastPresentationWasAnimated);

  // Verify that presented view controller is
  // LegacyDownloadManagerViewController.
  EXPECT_EQ(1U, base_view_controller_.childViewControllers.count);
  LegacyDownloadManagerViewController* viewController =
      base_view_controller_.childViewControllers.firstObject;
  ASSERT_EQ([LegacyDownloadManagerViewController class],
            [viewController class]);

  // Verify that LegacyDownloadManagerViewController configuration matches
  // download task.
  EXPECT_FALSE(viewController.actionButton.hidden);
  EXPECT_NSEQ(@"file.zip - 10 bytes", viewController.statusLabel.text);
  EXPECT_NSEQ(@"Download",
              [viewController.actionButton titleForState:UIControlStateNormal]);
}

// Tests stopping coordinator. Verifies that hiding web states dismisses the
// presented view controller and download task is reset to null (to prevent a
// stale raw pointer).
TEST_F(DownloadManagerCoordinatorTest, Stop) {
  auto task = CreateTestTask();
  coordinator_.downloadTask = task.get();
  [coordinator_ start];
  @autoreleasepool {
    // Calling -stop will retain and autorelease coordinator_. task_environment_
    // has to outlive the coordinator, so wrapping -stop call in @autorelease
    // will ensure that coordinator_ is deallocated.
    [coordinator_ stop];
  }

  // Verify that child view controller is removed and download task is set to
  // null.
  EXPECT_EQ(0U, base_view_controller_.childViewControllers.count);
  EXPECT_FALSE(coordinator_.downloadTask);
}

// Tests destroying coordinator during the download.
TEST_F(DownloadManagerCoordinatorTest, DestructionDuringDownload) {
  auto task = CreateTestTask();
  coordinator_.downloadTask = task.get();
  [coordinator_ start];

  EXPECT_EQ(1U, base_view_controller_.childViewControllers.count);
  LegacyDownloadManagerViewController* viewController =
      base_view_controller_.childViewControllers.firstObject;
  ASSERT_EQ([LegacyDownloadManagerViewController class],
            [viewController class]);

  // Start the download.
  base::FilePath path;
  ASSERT_TRUE(base::GetTempDir(&path));
  task->Start(path.Append(task->GenerateFileName()));

  @autoreleasepool {
    // Calling -downloadManagerViewControllerDidStartDownload will retain and
    // autorelease coordinator_. task_environment_ has to outlive the
    // coordinator, so wrapping -downloadManagerViewControllerDidStartDownload
    // call in @autorelease will ensure that coordinator_ is deallocated.
    [viewController.delegate
        downloadManagerViewControllerDidStartDownload:viewController];

    [coordinator_ stop];
    // Destroy coordinator before destroying the download task.
    coordinator_ = nil;
  }

  // Verify that child view controller is removed.
  EXPECT_EQ(0U, base_view_controller_.childViewControllers.count);
  histogram_tester_.ExpectUniqueSample(
      "Download.IOSDownloadFileResult",
      static_cast<base::HistogramBase::Sample>(DownloadFileResult::Other), 1);
}

// Tests downloadManagerTabHelper:didCreateDownload:webStateIsVisible: callback
// for visible web state. Verifies that coordinator's properties are set up and
// that LegacyDownloadManagerViewController is properly configured and presented
// with animation.
TEST_F(DownloadManagerCoordinatorTest, DelegateCreatedDownload) {
  auto task = CreateTestTask();
  histogram_tester_.ExpectTotalCount("Download.IOSDownloadFileUI", 0);

  [coordinator_ downloadManagerTabHelper:tab_helper()
                       didCreateDownload:task.get()
                       webStateIsVisible:YES];

  // Verify that coordinator's properties are set up.
  EXPECT_EQ(task.get(), coordinator_.downloadTask);
  EXPECT_TRUE(coordinator_.animatesPresentation);

  // First presentation of Download Manager UI should be animated.
  EXPECT_TRUE(presenter_.lastPresentationWasAnimated);

  // Verify that presented view controller is
  // LegacyDownloadManagerViewController.
  EXPECT_EQ(1U, base_view_controller_.childViewControllers.count);
  LegacyDownloadManagerViewController* viewController =
      base_view_controller_.childViewControllers.firstObject;
  ASSERT_EQ([LegacyDownloadManagerViewController class],
            [viewController class]);

  // Verify that LegacyDownloadManagerViewController configuration matches
  // download task.
  EXPECT_NSEQ(@"file.zip - 10 bytes", viewController.statusLabel.text);
  EXPECT_FALSE(viewController.actionButton.hidden);
  EXPECT_NSEQ(@"Download",
              [viewController.actionButton titleForState:UIControlStateNormal]);

  // Verify that UMA action was logged.
  histogram_tester_.ExpectTotalCount("Download.IOSDownloadFileUI", 1);
}

// Tests calling downloadManagerTabHelper:didCreateDownload:webStateIsVisible:
// callback twice. Second call should replace the old download task with the new
// one.
TEST_F(DownloadManagerCoordinatorTest, DelegateReplacedDownload) {
  auto task = CreateTestTask();
  task->Start(base::FilePath());
  task->SetDone(true);

  [coordinator_ downloadManagerTabHelper:tab_helper()
                       didCreateDownload:task.get()
                       webStateIsVisible:YES];

  // Verify that presented view controller is
  // LegacyDownloadManagerViewController.
  EXPECT_EQ(1U, base_view_controller_.childViewControllers.count);
  LegacyDownloadManagerViewController* viewController =
      base_view_controller_.childViewControllers.firstObject;
  ASSERT_EQ([LegacyDownloadManagerViewController class],
            [viewController class]);

  // Verify that LegacyDownloadManagerViewController configuration matches
  // download task.
  EXPECT_NSEQ(@"file.zip", viewController.statusLabel.text);
  EXPECT_FALSE(viewController.actionButton.hidden);
  EXPECT_NSEQ(@"Open in…",
              [viewController.actionButton titleForState:UIControlStateNormal]);

  // Replace download task with a new one.
  auto new_task = CreateTestTask();
  [coordinator_ downloadManagerTabHelper:tab_helper()
                       didCreateDownload:new_task.get()
                       webStateIsVisible:YES];

  // Verify that LegacyDownloadManagerViewController configuration matches new
  // download task.
  EXPECT_NSEQ(@"file.zip - 10 bytes", viewController.statusLabel.text);
  EXPECT_FALSE(viewController.actionButton.hidden);
  EXPECT_NSEQ(@"Download",
              [viewController.actionButton titleForState:UIControlStateNormal]);
}

// Tests downloadManagerTabHelper:didCreateDownload:webStateIsVisible: callback
// for hidden web state. Verifies that coordinator ignores callback from
// a background tab.
TEST_F(DownloadManagerCoordinatorTest,
       DelegateCreatedDownloadForHiddenWebState) {
  auto task = CreateTestTask();
  [coordinator_ downloadManagerTabHelper:tab_helper()
                       didCreateDownload:task.get()
                       webStateIsVisible:NO];

  // Background tab should not present Download Manager UI.
  EXPECT_EQ(0U, base_view_controller_.childViewControllers.count);
}

// Tests downloadManagerTabHelper:didHideDownload:animated: callback. Verifies
// that hiding web states dismisses the presented view controller and download
// task is reset to null (to prevent a stale raw pointer).
TEST_F(DownloadManagerCoordinatorTest, DelegateHideDownload) {
  auto task = CreateTestTask();
  [coordinator_ downloadManagerTabHelper:tab_helper()
                       didCreateDownload:task.get()
                       webStateIsVisible:YES];
  @autoreleasepool {
    // Calling -downloadManagerTabHelper:didHideDownload:animated: will retain
    // and autorelease coordinator_. task_environment_ has to outlive the
    // coordinator, so wrapping
    // -downloadManagerTabHelper:didHideDownload:animated: call in @autorelease
    // will ensure that coordinator_ is deallocated.
    [coordinator_ downloadManagerTabHelper:tab_helper()
                           didHideDownload:task.get()
                                  animated:NO];
  }

  // Verify that child view controller is removed and download task is set to
  // null.
  EXPECT_EQ(0U, base_view_controller_.childViewControllers.count);
  EXPECT_FALSE(coordinator_.downloadTask);
}

// Tests downloadManagerTabHelper:didShowDownload:animated: callback. Verifies
// that showing web state presents download manager UI for that web state.
TEST_F(DownloadManagerCoordinatorTest, DelegateShowDownload) {
  auto task = CreateTestTask();
  [coordinator_ downloadManagerTabHelper:tab_helper()
                         didShowDownload:task.get()
                                animated:NO];

  // Only first presentation is animated. Switching between tab should create
  // the impression that UI was never dismissed.
  EXPECT_FALSE(presenter_.lastPresentationWasAnimated);

  // Verify that presented view controller is
  // LegacyDownloadManagerViewController.
  EXPECT_EQ(1U, base_view_controller_.childViewControllers.count);
  LegacyDownloadManagerViewController* viewController =
      base_view_controller_.childViewControllers.firstObject;
  ASSERT_EQ([LegacyDownloadManagerViewController class],
            [viewController class]);

  // Verify that LegacyDownloadManagerViewController configuration matches
  // download task for shown web state.
  EXPECT_NSEQ(@"file.zip - 10 bytes", viewController.statusLabel.text);
  EXPECT_FALSE(viewController.actionButton.hidden);
}

// Tests closing view controller. Coordinator should be stopped and task
// cancelled.
TEST_F(DownloadManagerCoordinatorTest, Close) {
  auto task = CreateTestTask();
  coordinator_.downloadTask = task.get();
  [coordinator_ start];

  EXPECT_EQ(1U, base_view_controller_.childViewControllers.count);
  LegacyDownloadManagerViewController* viewController =
      base_view_controller_.childViewControllers.firstObject;
  ASSERT_EQ([LegacyDownloadManagerViewController class],
            [viewController class]);
  ASSERT_EQ(0, user_action_tester_.GetActionCount("IOSDownloadClose"));
  @autoreleasepool {
    // Calling -downloadManagerViewControllerDidClose: will retain and
    // autorelease coordinator_. task_environment_ has to outlive the
    // coordinator, so wrapping -downloadManagerViewControllerDidClose:
    // call in @autorelease will ensure that coordinator_ is deallocated.
    [viewController.delegate
        downloadManagerViewControllerDidClose:viewController];
  }

  // Verify that child view controller is removed, download task is set to null
  // and download task is cancelled.
  EXPECT_EQ(0U, base_view_controller_.childViewControllers.count);
  EXPECT_FALSE(coordinator_.downloadTask);
  EXPECT_EQ(web::DownloadTask::State::kCancelled, task->GetState());
  histogram_tester_.ExpectUniqueSample(
      "Download.IOSDownloadFileResult",
      static_cast<base::HistogramBase::Sample>(DownloadFileResult::NotStarted),
      1);
  histogram_tester_.ExpectTotalCount("Download.IOSDownloadFileUIGoogleDrive",
                                     0);
  EXPECT_EQ(1, user_action_tester_.GetActionCount("IOSDownloadClose"));
}

// Tests presenting Install Google Drive dialog. Coordinator presents StoreKit
// dialog and hides Install Google Drive button.
TEST_F(DownloadManagerCoordinatorTest, InstallDrive) {
  auto task = CreateTestTask();
  coordinator_.downloadTask = task.get();
  [coordinator_ start];

  EXPECT_EQ(1U, base_view_controller_.childViewControllers.count);
  LegacyDownloadManagerViewController* viewController =
      base_view_controller_.childViewControllers.firstObject;
  ASSERT_EQ([LegacyDownloadManagerViewController class],
            [viewController class]);

  // Make Install Google Drive UI visible.
  [viewController setInstallDriveButtonVisible:YES animated:NO];
  // The button itself is never hidden, but the superview which contains the
  // button changes it's alpha.
  ASSERT_EQ(1.0f, viewController.installDriveButton.superview.alpha);

  ASSERT_EQ(
      0, user_action_tester_.GetActionCount("IOSDownloadInstallGoogleDrive"));
  @autoreleasepool {
    // Calling -installDriveForDownloadManagerViewController: will retain and
    // autorelease coordinator_. task_environment_ has to outlive the
    // coordinator, so wrapping -installDriveForDownloadManagerViewController:
    // call in @autorelease will ensure that coordinator_ is deallocated.
    [viewController.delegate
        installDriveForDownloadManagerViewController:viewController];
  }
  // Verify that Store Kit dialog was presented.
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, ^{
    return [base_view_controller_.presentedViewController class] ==
           [SKStoreProductViewController class];
  }));

  // Verify that Install Google Drive UI is hidden.
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, ^{
    return viewController.installDriveButton.superview.alpha == 0.0f;
  }));

  // Simulate Google Drive app installation and verify that expected histograms
  // have been recorded.
  EXPECT_EQ(
      1, user_action_tester_.GetActionCount("IOSDownloadInstallGoogleDrive"));
  histogram_tester_.ExpectTotalCount("Download.IOSDownloadFileUIGoogleDrive",
                                     0);
  // SKStoreProductViewController uses UIApplication, so it's not possible to
  // install the mock before the test run.
  application_ = OCMClassMock([UIApplication class]);
  OCMStub([application_ sharedApplication]).andReturn(application_);
  OCMStub([application_ canOpenURL:GetGoogleDriveAppUrl()]).andReturn(YES);
}

// Tests presenting Open In... menu without actually opening the download.
TEST_F(DownloadManagerCoordinatorTest, OpenIn) {
  auto task = CreateTestTask();
  coordinator_.downloadTask = task.get();
  [coordinator_ start];

  EXPECT_EQ(1U, base_view_controller_.childViewControllers.count);
  LegacyDownloadManagerViewController* view_controller =
      base_view_controller_.childViewControllers.firstObject;
  ASSERT_EQ([LegacyDownloadManagerViewController class],
            [view_controller class]);

  id download_view_controller_mock = OCMPartialMock(view_controller);
  id dispatcher_mock = OCMProtocolMock(@protocol(BrowserCoordinatorCommands));
  [browser_->GetCommandDispatcher()
      startDispatchingToTarget:dispatcher_mock
                   forProtocol:@protocol(BrowserCoordinatorCommands)];

  // Start the download.
  base::FilePath path;
  ASSERT_TRUE(base::GetTempDir(&path));
  task->Start(path.Append(task->GenerateFileName()));

  // Stub UIActivityViewController.
  OCMStub([download_view_controller_mock presentViewController:[OCMArg any]
                                                      animated:YES
                                                    completion:[OCMArg any]])
      .andDo(^(NSInvocation* invocation) {
        __weak id object;
        [invocation getArgument:&object atIndex:2];
        EXPECT_EQ([UIActivityViewController class], [object class]);
        UIActivityViewController* open_in_controller =
            base::apple::ObjCCastStrict<UIActivityViewController>(object);
        EXPECT_EQ(open_in_controller.excludedActivityTypes.count, 2.0);
      });

  ASSERT_EQ(0, user_action_tester_.GetActionCount("IOSDownloadOpenIn"));

  // Present Open In... menu.
  @autoreleasepool {
    // Calling -installDriveForDownloadManagerViewController: and
    // presentOpenInForDownloadManagerViewController will retain and
    // autorelease coordinator_. task_environment_ has to outlive the
    // coordinator, so wrapping calls in @autorelease will ensure that
    // coordinator_ is deallocated.
    [view_controller.delegate
        downloadManagerViewControllerDidStartDownload:view_controller];

    // Complete the download before presenting Open In... menu.
    task->SetDone(true);

    [view_controller.delegate
        presentOpenInForDownloadManagerViewController:view_controller];
  }

  // Download task is destroyed without opening the file.
  task = nullptr;
  histogram_tester_.ExpectTotalCount("Download.IOSDownloadedFileNetError", 0);
  histogram_tester_.ExpectUniqueSample(
      "Download.IOSDownloadFileResult",
      static_cast<base::HistogramBase::Sample>(DownloadFileResult::Completed),
      1);
  histogram_tester_.ExpectUniqueSample(
      "Download.IOSDownloadedFileAction",
      static_cast<base::HistogramBase::Sample>(
          DownloadedFileAction::NoActionOrOpenedViaExtension),
      1);
  histogram_tester_.ExpectUniqueSample(
      "Download.IOSDownloadedFileAction",
      static_cast<base::HistogramBase::Sample>(
          DownloadFileInBackground::SucceededWithoutBackgrounding),
      1);
  histogram_tester_.ExpectTotalCount("Download.IOSDownloadFileUIGoogleDrive",
                                     1);
  EXPECT_EQ(1, user_action_tester_.GetActionCount("IOSDownloadOpenIn"));
}

// Tests destroying download task for in progress download.
TEST_F(DownloadManagerCoordinatorTest, DestroyInProgressDownload) {
  auto task = CreateTestTask();
  coordinator_.downloadTask = task.get();
  [coordinator_ start];

  EXPECT_EQ(1U, base_view_controller_.childViewControllers.count);
  LegacyDownloadManagerViewController* viewController =
      base_view_controller_.childViewControllers.firstObject;
  ASSERT_EQ([LegacyDownloadManagerViewController class],
            [viewController class]);

  // Start and the download.
  @autoreleasepool {
    // Calling -downloadManagerViewControllerDidStartDownload: will retain and
    // autorelease coordinator_. task_environment_ has to outlive the
    // coordinator, so wrapping -downloadManagerViewControllerDidStartDownload:
    // call in @autorelease will ensure that coordinator_ is deallocated.
    [viewController.delegate
        downloadManagerViewControllerDidStartDownload:viewController];
  }

  // Starting download is async for model.
  web::DownloadTask* task_ptr = task.get();
  ASSERT_TRUE(
      WaitUntilConditionOrTimeout(base::test::ios::kWaitForDownloadTimeout, ^{
        base::RunLoop().RunUntilIdle();
        return task_ptr->GetState() == web::DownloadTask::State::kInProgress;
      }));

  // Download task is destroyed before the download is complete.
  task = nullptr;
  histogram_tester_.ExpectTotalCount("Download.IOSDownloadedFileNetError", 0);
  histogram_tester_.ExpectTotalCount("Download.IOSDownloadedFileAction", 0);
  histogram_tester_.ExpectTotalCount("Download.IOSDownloadFileInBackground", 0);
  histogram_tester_.ExpectUniqueSample(
      "Download.IOSDownloadFileResult",
      static_cast<base::HistogramBase::Sample>(DownloadFileResult::Other), 1);
  histogram_tester_.ExpectTotalCount("Download.IOSDownloadFileUIGoogleDrive",
                                     0);
}

// Tests quitting the app during in-progress download.
TEST_F(DownloadManagerCoordinatorTest, QuitDuringInProgressDownload) {
  auto task = CreateTestTask();
  coordinator_.downloadTask = task.get();
  auto web_state = std::make_unique<web::FakeWebState>();
  browser_->GetWebStateList()->InsertWebState(std::move(web_state));
  [coordinator_ start];

  EXPECT_EQ(1U, base_view_controller_.childViewControllers.count);
  LegacyDownloadManagerViewController* viewController =
      base_view_controller_.childViewControllers.firstObject;
  ASSERT_EQ([LegacyDownloadManagerViewController class],
            [viewController class]);

  // Start and the download.
  @autoreleasepool {
    // Calling -downloadManagerViewControllerDidStartDownload: will retain and
    // autorelease coordinator_. task_environment_ has to outlive the
    // coordinator, so wrapping -downloadManagerViewControllerDidStartDownload:
    // call in @autorelease will ensure that coordinator_ is deallocated.
    [viewController.delegate
        downloadManagerViewControllerDidStartDownload:viewController];
  }

  // Starting download is async for model.
  web::DownloadTask* task_ptr = task.get();
  ASSERT_TRUE(
      WaitUntilConditionOrTimeout(base::test::ios::kWaitForDownloadTimeout, ^{
        base::RunLoop().RunUntilIdle();
        return task_ptr->GetState() == web::DownloadTask::State::kInProgress;
      }));

  // Web States are closed without user action only during app termination.
  CloseAllWebStates(*browser_->GetWebStateList(), WebStateList::CLOSE_NO_FLAGS);

  // Download task is destroyed before the download is complete.
  task = nullptr;
  histogram_tester_.ExpectTotalCount("Download.IOSDownloadedFileNetError", 0);
  histogram_tester_.ExpectTotalCount("Download.IOSDownloadedFileAction", 0);
  histogram_tester_.ExpectUniqueSample(
      "Download.IOSDownloadFileInBackground",
      static_cast<base::HistogramBase::Sample>(
          DownloadFileInBackground::CanceledAfterAppQuit),
      1);
  histogram_tester_.ExpectUniqueSample(
      "Download.IOSDownloadFileResult",
      static_cast<base::HistogramBase::Sample>(DownloadFileResult::Other), 1);
  histogram_tester_.ExpectTotalCount("Download.IOSDownloadFileUIGoogleDrive",
                                     0);
}

// Tests closing view controller while the download is in progress. Coordinator
// should present the confirmation dialog.
TEST_F(DownloadManagerCoordinatorTest, CloseInProgressDownload) {
  auto task = CreateTestTask();
  task->Start(base::FilePath());
  coordinator_.downloadTask = task.get();
  [coordinator_ start];

  EXPECT_EQ(1U, base_view_controller_.childViewControllers.count);
  LegacyDownloadManagerViewController* viewController =
      base_view_controller_.childViewControllers.firstObject;
  ASSERT_EQ([LegacyDownloadManagerViewController class],
            [viewController class]);
  ASSERT_EQ(0, user_action_tester_.GetActionCount(
                   "IOSDownloadTryCloseWhenInProgress"));

  OverlayRequestQueue* queue = OverlayRequestQueue::FromWebState(
      &web_state_, OverlayModality::kWebContentArea);
  ASSERT_EQ(0U, queue->size());
  @autoreleasepool {
    // Calling -downloadManagerViewControllerDidClose: will retain and
    // autorelease coordinator_. task_environment_ has to outlive the
    // coordinator, so wrapping -downloadManagerViewControllerDidClose:
    // call in @autorelease will ensure that coordinator_ is deallocated.
    [viewController.delegate
        downloadManagerViewControllerDidClose:viewController];
  }
  // Verify that confirm request was sent.
  ASSERT_EQ(1U, queue->size());

  alert_overlays::AlertRequest* config =
      queue->front_request()->GetConfig<alert_overlays::AlertRequest>();
  ASSERT_TRUE(config);
  EXPECT_NSEQ(@"Stop download?", config->title());
  EXPECT_FALSE(config->message());
  ASSERT_EQ(2U, config->button_configs().size());
  alert_overlays::ButtonConfig stop_button = config->button_configs()[0][0];
  EXPECT_NSEQ(@"Stop", stop_button.title);
  EXPECT_EQ(kDownloadCloseActionName, stop_button.user_action_name);
  alert_overlays::ButtonConfig continue_button = config->button_configs()[1][0];
  EXPECT_NSEQ(@"Continue", continue_button.title);
  EXPECT_EQ(kDownloadDoNotCloseActionName, continue_button.user_action_name);

  // Stop to avoid holding a dangling pointer to destroyed task.
  queue->CancelAllRequests();
  @autoreleasepool {
    // Calling -stop will retain and autorelease coordinator_. task_environment_
    // has to outlive the coordinator, so wrapping -stop call in @autorelease
    // will ensure that coordinator_ is deallocated.
    [coordinator_ stop];
  }

  EXPECT_EQ(0U, queue->size());
  EXPECT_EQ(1, user_action_tester_.GetActionCount(
                   "IOSDownloadTryCloseWhenInProgress"));
  EXPECT_EQ(0, user_action_tester_.GetActionCount(kDownloadCloseActionName));
  EXPECT_EQ(0,
            user_action_tester_.GetActionCount(kDownloadDoNotCloseActionName));
}

// Tests downloadManagerTabHelper:decidePolicyForDownload:completionHandler:.
// Coordinator should present the confirmation dialog.
TEST_F(DownloadManagerCoordinatorTest, DecidePolicyForDownload) {
  auto task = CreateTestTask();
  coordinator_.downloadTask = task.get();

  OverlayRequestQueue* queue = OverlayRequestQueue::FromWebState(
      &web_state_, OverlayModality::kWebContentArea);
  ASSERT_EQ(0U, queue->size());
  [coordinator_ downloadManagerTabHelper:tab_helper()
                 decidePolicyForDownload:task.get()
                       completionHandler:^(NewDownloadPolicy){
                       }];

  // Verify that confirm request was sent.
  ASSERT_EQ(1U, queue->size());

  alert_overlays::AlertRequest* config =
      queue->front_request()->GetConfig<alert_overlays::AlertRequest>();
  ASSERT_TRUE(config);
  EXPECT_NSEQ(@"Start new download?", config->title());
  EXPECT_NSEQ(@"This will stop all progress for your current download.",
              config->message());
  ASSERT_EQ(2U, config->button_configs().size());
  alert_overlays::ButtonConfig ok_button = config->button_configs()[0][0];
  EXPECT_NSEQ(@"OK", ok_button.title);
  EXPECT_EQ(kDownloadReplaceActionName, ok_button.user_action_name);
  alert_overlays::ButtonConfig cancel_button = config->button_configs()[1][0];
  EXPECT_NSEQ(@"Cancel", cancel_button.title);
  EXPECT_EQ(kDownloadDoNotReplaceActionName, cancel_button.user_action_name);

  queue->CancelAllRequests();
  @autoreleasepool {
    // Calling -stop will retain and autorelease coordinator_. task_environment_
    // has to outlive the coordinator, so wrapping -stop call in @autorelease
    // will ensure that coordinator_ is deallocated.
    [coordinator_ stop];
  }

  EXPECT_EQ(0U, queue->size());
}

// Tests downloadManagerTabHelper:decidePolicyForDownload:completionHandler:.
// Coordinator should present the confirmation dialog.
TEST_F(DownloadManagerCoordinatorTest,
       DecidePolicyForDownloadFromBackgroundTab) {
  auto task = CreateTestTask();
  coordinator_.downloadTask = nullptr;  // Current Tab does not have task.

  OverlayRequestQueue* queue = OverlayRequestQueue::FromWebState(
      &web_state_, OverlayModality::kWebContentArea);
  ASSERT_EQ(0U, queue->size());
  [coordinator_ downloadManagerTabHelper:tab_helper()
                 decidePolicyForDownload:task.get()
                       completionHandler:^(NewDownloadPolicy){
                       }];

  // Verify that confirm request was sent.
  ASSERT_EQ(1U, queue->size());

  alert_overlays::AlertRequest* config =
      queue->front_request()->GetConfig<alert_overlays::AlertRequest>();
  ASSERT_TRUE(config);
  EXPECT_NSEQ(@"Start new download?", config->title());
  EXPECT_NSEQ(@"This will stop all progress for your current download.",
              config->message());
  ASSERT_EQ(2U, config->button_configs().size());
  alert_overlays::ButtonConfig ok_button = config->button_configs()[0][0];
  EXPECT_NSEQ(@"OK", ok_button.title);
  EXPECT_EQ(kDownloadReplaceActionName, ok_button.user_action_name);
  alert_overlays::ButtonConfig cancel_button = config->button_configs()[1][0];
  EXPECT_NSEQ(@"Cancel", cancel_button.title);
  EXPECT_EQ(kDownloadDoNotReplaceActionName, cancel_button.user_action_name);

  queue->CancelAllRequests();
  @autoreleasepool {
    // Calling -stop will retain and autorelease coordinator_. task_environment_
    // has to outlive the coordinator, so wrapping -stop call in @autorelease
    // will ensure that coordinator_ is deallocated.
    [coordinator_ stop];
  }

  EXPECT_EQ(0U, queue->size());
}

// Tests starting the download. Verifies that download task is started and its
// file writer is configured to write into download directory.
TEST_F(DownloadManagerCoordinatorTest, StartDownload) {
  auto task = CreateTestTask();
  coordinator_.downloadTask = task.get();
  [coordinator_ start];

  LegacyDownloadManagerViewController* viewController =
      base_view_controller_.childViewControllers.firstObject;
  ASSERT_EQ([LegacyDownloadManagerViewController class],
            [viewController class]);
  @autoreleasepool {
    // Calling -downloadManagerViewControllerDidStartDownload: will retain and
    // autorelease coordinator_. task_environment_ has to outlive the
    // coordinator, so wrapping -downloadManagerViewControllerDidStartDownload:
    // call in @autorelease will ensure that coordinator_ is deallocated.
    [viewController.delegate
        downloadManagerViewControllerDidStartDownload:viewController];
  }

  // Starting download is async for model.
  web::DownloadTask* task_ptr = task.get();
  ASSERT_TRUE(
      WaitUntilConditionOrTimeout(base::test::ios::kWaitForDownloadTimeout, ^{
        base::RunLoop().RunUntilIdle();
        return task_ptr->GetState() == web::DownloadTask::State::kInProgress;
      }));

  // Download file should be located in download directory.
  base::FilePath file = task->GetResponsePath();
  base::FilePath download_dir;
  ASSERT_TRUE(GetTempDownloadsDirectory(&download_dir));
  EXPECT_TRUE(download_dir.IsParent(file));

  histogram_tester_.ExpectTotalCount("Download.IOSDownloadFileInBackground", 0);
  EXPECT_EQ(0,
            user_action_tester_.GetActionCount("MobileDownloadRetryDownload"));
  EXPECT_EQ(1, user_action_tester_.GetActionCount("IOSDownloadStartDownload"));
}

// Tests retrying the download. Verifies that kDownloadManagerRetryDownload UMA
// metric is logged.
TEST_F(DownloadManagerCoordinatorTest, RetryingDownload) {
  auto task = CreateTestTask();
  coordinator_.downloadTask = task.get();
  [coordinator_ start];

  // First download is a failure.
  LegacyDownloadManagerViewController* viewController =
      base_view_controller_.childViewControllers.firstObject;
  ASSERT_EQ([LegacyDownloadManagerViewController class],
            [viewController class]);
  ASSERT_EQ(0, user_action_tester_.GetActionCount("IOSDownloadStartDownload"));
  @autoreleasepool {
    // Calling -downloadManagerViewControllerDidStartDownload: will retain and
    // autorelease coordinator_. task_environment_ has to outlive the
    // coordinator, so wrapping -downloadManagerViewControllerDidStartDownload:
    // call in @autorelease will ensure that coordinator_ is deallocated.
    [viewController.delegate
        downloadManagerViewControllerDidStartDownload:viewController];
  }
  task->SetErrorCode(net::ERR_INTERNET_DISCONNECTED);
  task->SetDone(true);
  ASSERT_EQ(1, user_action_tester_.GetActionCount("IOSDownloadStartDownload"));

  @autoreleasepool {
    // Calling -downloadManagerViewControllerDidStartDownload: will retain and
    // autorelease coordinator_. task_environment_ has to outlive the
    // coordinator, so wrapping -downloadManagerViewControllerDidStartDownload:
    // call in @autorelease will ensure that coordinator_ is deallocated.
    [viewController.delegate
        downloadManagerViewControllerDidStartDownload:viewController];
  }

  // Starting download is async for model.
  web::DownloadTask* task_ptr = task.get();
  ASSERT_TRUE(
      WaitUntilConditionOrTimeout(base::test::ios::kWaitForDownloadTimeout, ^{
        base::RunLoop().RunUntilIdle();
        return task_ptr->GetState() == web::DownloadTask::State::kInProgress;
      }));

  histogram_tester_.ExpectUniqueSample("Download.IOSDownloadedFileNetError",
                                       -net::ERR_INTERNET_DISCONNECTED, 1);
  histogram_tester_.ExpectUniqueSample(
      "Download.IOSDownloadFileResult",
      static_cast<base::HistogramBase::Sample>(DownloadFileResult::Failure), 1);
  histogram_tester_.ExpectUniqueSample(
      "Download.IOSDownloadFileInBackground",
      static_cast<base::HistogramBase::Sample>(
          DownloadFileInBackground::FailedWithoutBackgrounding),
      1);
  EXPECT_EQ(1,
            user_action_tester_.GetActionCount("MobileDownloadRetryDownload"));
}

// Tests download failure in background.
TEST_F(DownloadManagerCoordinatorTest, FailingInBackground) {
  auto task = CreateTestTask();
  coordinator_.downloadTask = task.get();
  [coordinator_ start];

  // Start and immediately fail the download.
  LegacyDownloadManagerViewController* viewController =
      base_view_controller_.childViewControllers.firstObject;
  ASSERT_EQ([LegacyDownloadManagerViewController class],
            [viewController class]);
  @autoreleasepool {
    // Calling -downloadManagerViewControllerDidStartDownload: will retain and
    // autorelease coordinator_. task_environment_ has to outlive the
    // coordinator, so wrapping -downloadManagerViewControllerDidStartDownload:
    // call in @autorelease will ensure that coordinator_ is deallocated.
    [viewController.delegate
        downloadManagerViewControllerDidStartDownload:viewController];
  }
  task->SetPerformedBackgroundDownload(true);
  task->SetErrorCode(net::ERR_INTERNET_DISCONNECTED);
  task->SetDone(true);

  histogram_tester_.ExpectUniqueSample(
      "Download.IOSDownloadFileResult",
      static_cast<base::HistogramBase::Sample>(DownloadFileResult::Failure), 1);
  histogram_tester_.ExpectUniqueSample(
      "Download.IOSDownloadFileInBackground",
      static_cast<base::HistogramBase::Sample>(
          DownloadFileInBackground::FailedWithBackgrounding),
      1);
  histogram_tester_.ExpectTotalCount("Download.IOSDownloadFileUIGoogleDrive",
                                     0);
}

// Tests successful download in background.
TEST_F(DownloadManagerCoordinatorTest, SucceedingInBackground) {
  auto task = CreateTestTask();
  coordinator_.downloadTask = task.get();
  [coordinator_ start];

  EXPECT_EQ(1U, base_view_controller_.childViewControllers.count);
  LegacyDownloadManagerViewController* viewController =
      base_view_controller_.childViewControllers.firstObject;
  ASSERT_EQ([LegacyDownloadManagerViewController class],
            [viewController class]);

  // Start the download.
  base::FilePath path;
  ASSERT_TRUE(base::GetTempDir(&path));
  task->Start(path.Append(task->GenerateFileName()));

  // Start the download.
  @autoreleasepool {
    // Calling -downloadManagerViewControllerDidStartDownload: will retain and
    // autorelease coordinator_. task_environment_ has to outlive the
    // coordinator, so wrapping -downloadManagerViewControllerDidStartDownload:
    // call in @autorelease will ensure that coordinator_ is deallocated.
    [viewController.delegate
        downloadManagerViewControllerDidStartDownload:viewController];
  }

  // Complete the download to log UMA.
  task->SetPerformedBackgroundDownload(true);
  task->SetDone(true);
  histogram_tester_.ExpectUniqueSample(
      "Download.IOSDownloadFileInBackground",
      static_cast<base::HistogramBase::Sample>(
          DownloadFileInBackground::SucceededWithBackgrounding),
      1);
}

// Tests that viewController returns correct view controller if coordinator is
// started and nil when stopped.
TEST_F(DownloadManagerCoordinatorTest, ViewController) {
  auto task = CreateTestTask();
  coordinator_.downloadTask = task.get();
  ASSERT_FALSE(coordinator_.viewController);
  [coordinator_ start];

  // Verify that presented view controller is
  // LegacyDownloadManagerViewController.
  EXPECT_EQ(1U, base_view_controller_.childViewControllers.count);
  LegacyDownloadManagerViewController* viewController =
      base_view_controller_.childViewControllers.firstObject;
  ASSERT_EQ([LegacyDownloadManagerViewController class],
            [viewController class]);

  // Verify view controller property.
  EXPECT_NSEQ(viewController, coordinator_.viewController);

  @autoreleasepool {
    // Calling -stop will retain and autorelease coordinator_. task_environment_
    // has to outlive the coordinator, so wrapping -stop call in @autorelease
    // will ensure that coordinator_ is deallocated.
    [coordinator_ stop];
  }
  EXPECT_FALSE(coordinator_.viewController);
}
