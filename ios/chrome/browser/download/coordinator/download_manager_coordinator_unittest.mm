// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/download/coordinator/download_manager_coordinator.h"

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
#import "ios/chrome/browser/download/model/document_download_tab_helper.h"
#import "ios/chrome/browser/download/model/download_directory_util.h"
#import "ios/chrome/browser/download/model/download_manager_metric_names.h"
#import "ios/chrome/browser/download/model/download_manager_tab_helper.h"
#import "ios/chrome/browser/download/model/external_app_util.h"
#import "ios/chrome/browser/download/model/installation_notifier.h"
#import "ios/chrome/browser/download/ui/download_manager_view_controller+Testing.h"
#import "ios/chrome/browser/download/ui/download_manager_view_controller.h"
#import "ios/chrome/browser/download/ui/download_manager_view_controller_delegate.h"
#import "ios/chrome/browser/overlays/model/public/overlay_request_queue.h"
#import "ios/chrome/browser/overlays/model/public/web_content_area/alert_overlay.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_opener.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/util/file_size_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/fakes/fake_contained_presenter.h"
#import "ios/chrome/test/scoped_key_window.h"
#import "ios/web/public/test/fakes/fake_download_task.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "net/base/net_errors.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "ui/base/l10n/l10n_util_mac.h"

using base::test::ios::kWaitForUIElementTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;

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
    profile_ = TestProfileIOS::Builder().Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());
    presenter_ = [[FakeContainedPresenter alloc] init];
    base_view_controller_ = [[UIViewController alloc] init];
    activity_view_controller_class_ =
        OCMClassMock([UIActivityViewController class]);
    web_state_ = std::make_unique<web::FakeWebState>();
    OverlayRequestQueue::CreateForWebState(web_state_.get());
    DownloadManagerTabHelper::CreateForWebState(web_state_.get());
    DocumentDownloadTabHelper::CreateForWebState(web_state_.get());
    web_state_->SetBrowserState(profile_.get());
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
    return DownloadManagerTabHelper::FromWebState(web_state_.get());
  }

  // Creates a fake download task for testing.
  std::unique_ptr<web::FakeDownloadTask> CreateTestTask() {
    auto task =
        std::make_unique<web::FakeDownloadTask>(GURL(kTestUrl), kTestMimeType);
    task->SetTotalBytes(kTestTotalBytes);
    task->SetReceivedBytes(kTestReceivedBytes);
    task->SetGeneratedFileName(base::FilePath(kTestSuggestedFileName));
    task->SetWebState(web_state_.get());
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
  std::unique_ptr<web::FakeWebState> web_state_;
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
// DownloadManagerViewController is propertly configured and presented.
TEST_F(DownloadManagerCoordinatorTest, Start) {
  auto task = CreateTestTask();
  coordinator_.downloadTask = task.get();
  [coordinator_ start];

  // By default coordinator presents without animation.
  EXPECT_FALSE(presenter_.lastPresentationWasAnimated);

  // Verify that presented view controller is
  // DownloadManagerViewController.
  EXPECT_EQ(1U, base_view_controller_.childViewControllers.count);
  DownloadManagerViewController* viewController =
      base_view_controller_.childViewControllers.firstObject;
  ASSERT_EQ([DownloadManagerViewController class], [viewController class]);

  // Verify that DownloadManagerViewController configuration matches
  // download task.
  EXPECT_FALSE(viewController.actionButton.hidden);
  NSString* file_size = GetSizeString(kTestTotalBytes);
  EXPECT_NSEQ(
      l10n_util::GetNSStringF(IDS_IOS_DOWNLOAD_MANAGER_FILENAME_WITH_SIZE,
                              task->GenerateFileName().LossyDisplayName(),
                              base::SysNSStringToUTF16(file_size)),
      viewController.statusLabel.text);
  EXPECT_NSEQ([l10n_util::GetNSString(IDS_IOS_DOWNLOAD_MANAGER_DOWNLOAD)
                  localizedUppercaseString],
              viewController.actionButton.configuration.title);
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
  web::FakeDownloadTask* task_ptr = task.get();
  tab_helper()->SetCurrentDownload(std::move(task));
  coordinator_.downloadTask = task_ptr;
  [coordinator_ start];

  EXPECT_EQ(1U, base_view_controller_.childViewControllers.count);
  DownloadManagerViewController* viewController =
      base_view_controller_.childViewControllers.firstObject;
  ASSERT_EQ([DownloadManagerViewController class], [viewController class]);

  // Start the download.
  base::FilePath path;
  ASSERT_TRUE(base::GetTempDir(&path));
  task_ptr->Start(path.Append(task_ptr->GenerateFileName()));

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
      static_cast<base::HistogramBase::Sample32>(DownloadFileResult::Other), 1);
}

// Tests downloadManagerTabHelper:didCreateDownload:webStateIsVisible: callback
// for visible web state. Verifies that coordinator's properties are set up and
// that DownloadManagerViewController is properly configured and presented
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
  // DownloadManagerViewController.
  EXPECT_EQ(1U, base_view_controller_.childViewControllers.count);
  DownloadManagerViewController* viewController =
      base_view_controller_.childViewControllers.firstObject;
  ASSERT_EQ([DownloadManagerViewController class], [viewController class]);

  // Verify that DownloadManagerViewController configuration matches
  // download task.
  EXPECT_FALSE(viewController.actionButton.hidden);
  NSString* file_size = GetSizeString(kTestTotalBytes);
  EXPECT_NSEQ(
      l10n_util::GetNSStringF(IDS_IOS_DOWNLOAD_MANAGER_FILENAME_WITH_SIZE,
                              task->GenerateFileName().LossyDisplayName(),
                              base::SysNSStringToUTF16(file_size)),
      viewController.statusLabel.text);
  EXPECT_NSEQ([l10n_util::GetNSString(IDS_IOS_DOWNLOAD_MANAGER_DOWNLOAD)
                  localizedUppercaseString],
              viewController.actionButton.configuration.title);

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
  // DownloadManagerViewController.
  EXPECT_EQ(1U, base_view_controller_.childViewControllers.count);
  DownloadManagerViewController* viewController =
      base_view_controller_.childViewControllers.firstObject;
  ASSERT_EQ([DownloadManagerViewController class], [viewController class]);

  // Verify that DownloadManagerViewController configuration matches
  // download task.
  EXPECT_NSEQ(
      l10n_util::GetNSString(IDS_IOS_DOWNLOAD_MANAGER_DOWNLOAD_COMPLETE),
      viewController.statusLabel.text);
  EXPECT_FALSE(viewController.actionButton.hidden);
  EXPECT_NSEQ(
      [l10n_util::GetNSString(IDS_IOS_OPEN_IN) localizedUppercaseString],
      viewController.actionButton.configuration.title);

  // Replace download task with a new one.
  auto new_task = CreateTestTask();
  [coordinator_ downloadManagerTabHelper:tab_helper()
                       didCreateDownload:new_task.get()
                       webStateIsVisible:YES];

  // Verify that DownloadManagerViewController configuration matches new
  // download task.
  EXPECT_FALSE(viewController.actionButton.hidden);
  NSString* file_size = GetSizeString(kTestTotalBytes);
  EXPECT_NSEQ(
      l10n_util::GetNSStringF(IDS_IOS_DOWNLOAD_MANAGER_FILENAME_WITH_SIZE,
                              task->GenerateFileName().LossyDisplayName(),
                              base::SysNSStringToUTF16(file_size)),
      viewController.statusLabel.text);
  EXPECT_NSEQ([l10n_util::GetNSString(IDS_IOS_DOWNLOAD_MANAGER_DOWNLOAD)
                  localizedUppercaseString],
              viewController.actionButton.configuration.title);
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
  // DownloadManagerViewController.
  EXPECT_EQ(1U, base_view_controller_.childViewControllers.count);
  DownloadManagerViewController* viewController =
      base_view_controller_.childViewControllers.firstObject;
  ASSERT_EQ([DownloadManagerViewController class], [viewController class]);

  // Verify that DownloadManagerViewController configuration matches
  // download task for shown web state.
  EXPECT_FALSE(viewController.actionButton.hidden);
  NSString* file_size = GetSizeString(kTestTotalBytes);
  EXPECT_NSEQ(
      l10n_util::GetNSStringF(IDS_IOS_DOWNLOAD_MANAGER_FILENAME_WITH_SIZE,
                              task->GenerateFileName().LossyDisplayName(),
                              base::SysNSStringToUTF16(file_size)),
      viewController.statusLabel.text);
}

// Tests closing view controller. Coordinator should be stopped and task
// cancelled.
TEST_F(DownloadManagerCoordinatorTest, Close) {
  auto task = CreateTestTask();
  coordinator_.downloadTask = task.get();
  [coordinator_ start];

  EXPECT_EQ(1U, base_view_controller_.childViewControllers.count);
  DownloadManagerViewController* viewController =
      base_view_controller_.childViewControllers.firstObject;
  ASSERT_EQ([DownloadManagerViewController class], [viewController class]);
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
  // and download task remains in its original state (cleanup doesn't cancel).
  EXPECT_EQ(0U, base_view_controller_.childViewControllers.count);
  EXPECT_FALSE(coordinator_.downloadTask);
  EXPECT_EQ(web::DownloadTask::State::kNotStarted, task->GetState());
  histogram_tester_.ExpectUniqueSample(
      "Download.IOSDownloadFileResult",
      static_cast<base::HistogramBase::Sample32>(
          DownloadFileResult::NotStarted),
      1);
  histogram_tester_.ExpectTotalCount("Download.IOSDownloadFileUIGoogleDrive",
                                     0);
  EXPECT_EQ(1, user_action_tester_.GetActionCount("IOSDownloadClose"));
}

// Tests presenting Open In... menu without actually opening the download.
TEST_F(DownloadManagerCoordinatorTest, OpenIn) {
  auto task = CreateTestTask();
  web::FakeDownloadTask* task_ptr = task.get();
  tab_helper()->SetCurrentDownload(std::move(task));
  coordinator_.downloadTask = task_ptr;
  [coordinator_ start];

  EXPECT_EQ(1U, base_view_controller_.childViewControllers.count);
  DownloadManagerViewController* view_controller =
      base_view_controller_.childViewControllers.firstObject;
  ASSERT_EQ([DownloadManagerViewController class], [view_controller class]);

  id download_view_controller_mock = OCMPartialMock(view_controller);
  id dispatcher_mock = OCMProtocolMock(@protocol(BrowserCoordinatorCommands));
  [browser_->GetCommandDispatcher()
      startDispatchingToTarget:dispatcher_mock
                   forProtocol:@protocol(BrowserCoordinatorCommands)];

  // Start the download.
  base::FilePath path;
  ASSERT_TRUE(base::GetTempDir(&path));
  task_ptr->Start(path.Append(task_ptr->GenerateFileName()));

  // Stub UIActivityViewController.
  OCMStub([download_view_controller_mock
      presentViewController:[OCMArg checkWithBlock:^(id object) {
        EXPECT_EQ([UIActivityViewController class], [object class]);
        UIActivityViewController* open_in_controller =
            base::apple::ObjCCastStrict<UIActivityViewController>(object);
        EXPECT_EQ(open_in_controller.excludedActivityTypes.count, 2.0);
        return YES;
      }]
                   animated:YES
                 completion:[OCMArg any]]);

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
    task_ptr->SetDone(true);

    ASSERT_TRUE(WaitUntilConditionOrTimeout(
        base::test::ios::kWaitForDownloadTimeout, true, ^{
          return !tab_helper()->GetDownloadTaskFinalFilePath().empty();
        }));

    [view_controller.delegate
        presentOpenInForDownloadManagerViewController:view_controller];
  }

  // Download task is destroyed without opening the file.
  tab_helper()->SetCurrentDownload(CreateTestTask());
  histogram_tester_.ExpectTotalCount("Download.IOSDownloadedFileNetError", 0);
  histogram_tester_.ExpectUniqueSample(
      "Download.IOSDownloadFileResult",
      static_cast<base::HistogramBase::Sample32>(DownloadFileResult::Completed),
      1);
  histogram_tester_.ExpectUniqueSample(
      "Download.IOSDownloadedFileAction",
      static_cast<base::HistogramBase::Sample32>(
          DownloadedFileAction::NoActionOrOpenedViaExtension),
      1);
  histogram_tester_.ExpectUniqueSample(
      "Download.IOSDownloadedFileAction",
      static_cast<base::HistogramBase::Sample32>(
          DownloadFileInBackground::SucceededWithoutBackgrounding),
      1);
  histogram_tester_.ExpectTotalCount("Download.IOSDownloadFileUIGoogleDrive",
                                     1);
  EXPECT_EQ(1, user_action_tester_.GetActionCount("IOSDownloadOpenIn"));
}

// Tests destroying download task for in progress download.
TEST_F(DownloadManagerCoordinatorTest, DestroyInProgressDownload) {
  auto task = CreateTestTask();
  web::FakeDownloadTask* task_ptr = task.get();
  tab_helper()->SetCurrentDownload(std::move(task));
  coordinator_.downloadTask = task_ptr;
  [coordinator_ start];

  EXPECT_EQ(1U, base_view_controller_.childViewControllers.count);
  DownloadManagerViewController* viewController =
      base_view_controller_.childViewControllers.firstObject;
  ASSERT_EQ([DownloadManagerViewController class], [viewController class]);

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
  ASSERT_TRUE(
      WaitUntilConditionOrTimeout(base::test::ios::kWaitForDownloadTimeout, ^{
        base::RunLoop().RunUntilIdle();
        return task_ptr->GetState() == web::DownloadTask::State::kInProgress;
      }));

  // Download task is destroyed before the download is complete. In practice,
  // this happens if the associated WebState is destroyed.
  web_state_ = nullptr,
  histogram_tester_.ExpectTotalCount("Download.IOSDownloadedFileNetError", 0);
  histogram_tester_.ExpectTotalCount("Download.IOSDownloadedFileAction", 0);
  histogram_tester_.ExpectTotalCount("Download.IOSDownloadFileInBackground", 0);
  histogram_tester_.ExpectUniqueSample(
      "Download.IOSDownloadFileResult",
      static_cast<base::HistogramBase::Sample32>(DownloadFileResult::Other), 1);
  histogram_tester_.ExpectTotalCount("Download.IOSDownloadFileUIGoogleDrive",
                                     0);
}

// Tests quitting the app during in-progress download.
TEST_F(DownloadManagerCoordinatorTest, QuitDuringInProgressDownload) {
  auto task = CreateTestTask();
  web::DownloadTask* task_ptr = task.get();
  tab_helper()->SetCurrentDownload(std::move(task));
  coordinator_.downloadTask = task_ptr;
  auto web_state = std::make_unique<web::FakeWebState>();
  browser_->GetWebStateList()->InsertWebState(std::move(web_state));
  [coordinator_ start];

  EXPECT_EQ(1U, base_view_controller_.childViewControllers.count);
  DownloadManagerViewController* viewController =
      base_view_controller_.childViewControllers.firstObject;
  ASSERT_EQ([DownloadManagerViewController class], [viewController class]);

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
  ASSERT_TRUE(
      WaitUntilConditionOrTimeout(base::test::ios::kWaitForDownloadTimeout, ^{
        base::RunLoop().RunUntilIdle();
        return task_ptr->GetState() == web::DownloadTask::State::kInProgress;
      }));

  // Web States are closed without user action only during app termination.
  CloseAllWebStates(*browser_->GetWebStateList(),
                    WebStateList::ClosingReason::kDefault);

  // Download task is destroyed before the download is complete.
  web_state_ = nullptr;
  histogram_tester_.ExpectTotalCount("Download.IOSDownloadedFileNetError", 0);
  histogram_tester_.ExpectTotalCount("Download.IOSDownloadedFileAction", 0);
  histogram_tester_.ExpectUniqueSample(
      "Download.IOSDownloadFileInBackground",
      static_cast<base::HistogramBase::Sample32>(
          DownloadFileInBackground::CanceledAfterAppQuit),
      1);
  histogram_tester_.ExpectUniqueSample(
      "Download.IOSDownloadFileResult",
      static_cast<base::HistogramBase::Sample32>(DownloadFileResult::Other), 1);
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
  DownloadManagerViewController* viewController =
      base_view_controller_.childViewControllers.firstObject;
  ASSERT_EQ([DownloadManagerViewController class], [viewController class]);
  ASSERT_EQ(0, user_action_tester_.GetActionCount(
                   "IOSDownloadTryCloseWhenInProgress"));

  OverlayRequestQueue* queue = OverlayRequestQueue::FromWebState(
      web_state_.get(), OverlayModality::kWebContentArea);
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
      web_state_.get(), OverlayModality::kWebContentArea);
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
      web_state_.get(), OverlayModality::kWebContentArea);
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
  web::FakeDownloadTask* task_ptr = task.get();
  tab_helper()->SetCurrentDownload(std::move(task));
  coordinator_.downloadTask = task_ptr;
  [coordinator_ start];

  DownloadManagerViewController* viewController =
      base_view_controller_.childViewControllers.firstObject;
  ASSERT_EQ([DownloadManagerViewController class], [viewController class]);
  @autoreleasepool {
    // Calling -downloadManagerViewControllerDidStartDownload: will retain and
    // autorelease coordinator_. task_environment_ has to outlive the
    // coordinator, so wrapping -downloadManagerViewControllerDidStartDownload:
    // call in @autorelease will ensure that coordinator_ is deallocated.
    [viewController.delegate
        downloadManagerViewControllerDidStartDownload:viewController];
  }

  // Starting download is async for model.
  ASSERT_TRUE(
      WaitUntilConditionOrTimeout(base::test::ios::kWaitForDownloadTimeout, ^{
        base::RunLoop().RunUntilIdle();
        return task_ptr->GetState() == web::DownloadTask::State::kInProgress;
      }));

  // Download file should be located in download directory.
  base::FilePath file = task_ptr->GetResponsePath();
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
  web::FakeDownloadTask* task_ptr = task.get();
  tab_helper()->SetCurrentDownload(std::move(task));
  coordinator_.downloadTask = task_ptr;
  [coordinator_ start];

  // First download is a failure.
  DownloadManagerViewController* viewController =
      base_view_controller_.childViewControllers.firstObject;
  ASSERT_EQ([DownloadManagerViewController class], [viewController class]);
  ASSERT_EQ(0, user_action_tester_.GetActionCount("IOSDownloadStartDownload"));
  @autoreleasepool {
    // Calling -downloadManagerViewControllerDidStartDownload: will retain and
    // autorelease coordinator_. task_environment_ has to outlive the
    // coordinator, so wrapping -downloadManagerViewControllerDidStartDownload:
    // call in @autorelease will ensure that coordinator_ is deallocated.
    [viewController.delegate
        downloadManagerViewControllerDidStartDownload:viewController];
  }
  task_ptr->SetErrorCode(net::ERR_INTERNET_DISCONNECTED);
  task_ptr->SetDone(true);
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
  ASSERT_TRUE(
      WaitUntilConditionOrTimeout(base::test::ios::kWaitForDownloadTimeout, ^{
        base::RunLoop().RunUntilIdle();
        return task_ptr->GetState() == web::DownloadTask::State::kInProgress;
      }));

  histogram_tester_.ExpectUniqueSample("Download.IOSDownloadedFileNetError",
                                       -net::ERR_INTERNET_DISCONNECTED, 1);
  histogram_tester_.ExpectUniqueSample(
      "Download.IOSDownloadFileResult",
      static_cast<base::HistogramBase::Sample32>(DownloadFileResult::Failure),
      1);
  histogram_tester_.ExpectUniqueSample(
      "Download.IOSDownloadFileInBackground",
      static_cast<base::HistogramBase::Sample32>(
          DownloadFileInBackground::FailedWithoutBackgrounding),
      1);
  EXPECT_EQ(1,
            user_action_tester_.GetActionCount("MobileDownloadRetryDownload"));
}

// Tests download failure in background.
TEST_F(DownloadManagerCoordinatorTest, FailingInBackground) {
  auto task = CreateTestTask();
  web::FakeDownloadTask* task_ptr = task.get();
  tab_helper()->SetCurrentDownload(std::move(task));
  coordinator_.downloadTask = task_ptr;
  [coordinator_ start];

  // Start and immediately fail the download.
  DownloadManagerViewController* viewController =
      base_view_controller_.childViewControllers.firstObject;
  ASSERT_EQ([DownloadManagerViewController class], [viewController class]);
  @autoreleasepool {
    // Calling -downloadManagerViewControllerDidStartDownload: will retain and
    // autorelease coordinator_. task_environment_ has to outlive the
    // coordinator, so wrapping -downloadManagerViewControllerDidStartDownload:
    // call in @autorelease will ensure that coordinator_ is deallocated.
    [viewController.delegate
        downloadManagerViewControllerDidStartDownload:viewController];
  }
  task_ptr->SetPerformedBackgroundDownload(true);
  task_ptr->SetErrorCode(net::ERR_INTERNET_DISCONNECTED);
  task_ptr->SetDone(true);

  histogram_tester_.ExpectUniqueSample(
      "Download.IOSDownloadFileResult",
      static_cast<base::HistogramBase::Sample32>(DownloadFileResult::Failure),
      1);
  histogram_tester_.ExpectUniqueSample(
      "Download.IOSDownloadFileInBackground",
      static_cast<base::HistogramBase::Sample32>(
          DownloadFileInBackground::FailedWithBackgrounding),
      1);
  histogram_tester_.ExpectTotalCount("Download.IOSDownloadFileUIGoogleDrive",
                                     0);
}

// Tests successful download in background.
TEST_F(DownloadManagerCoordinatorTest, SucceedingInBackground) {
  auto task = CreateTestTask();
  web::FakeDownloadTask* task_ptr = task.get();
  tab_helper()->SetCurrentDownload(std::move(task));
  coordinator_.downloadTask = task_ptr;
  [coordinator_ start];

  EXPECT_EQ(1U, base_view_controller_.childViewControllers.count);
  DownloadManagerViewController* viewController =
      base_view_controller_.childViewControllers.firstObject;
  ASSERT_EQ([DownloadManagerViewController class], [viewController class]);

  // Start the download.
  base::FilePath path;
  ASSERT_TRUE(base::GetTempDir(&path));
  task_ptr->Start(path.Append(task_ptr->GenerateFileName()));

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
  task_ptr->SetPerformedBackgroundDownload(true);
  task_ptr->SetDone(true);
  histogram_tester_.ExpectUniqueSample(
      "Download.IOSDownloadFileInBackground",
      static_cast<base::HistogramBase::Sample32>(
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
  // DownloadManagerViewController.
  EXPECT_EQ(1U, base_view_controller_.childViewControllers.count);
  DownloadManagerViewController* viewController =
      base_view_controller_.childViewControllers.firstObject;
  ASSERT_EQ([DownloadManagerViewController class], [viewController class]);

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
