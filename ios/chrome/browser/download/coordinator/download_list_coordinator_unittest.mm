// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/download/coordinator/download_list_coordinator.h"

#import <UIKit/UIKit.h>

#import <memory>

#import "base/files/file_util.h"
#import "base/files/scoped_temp_dir.h"
#import "base/memory/raw_ptr.h"
#import "base/run_loop.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/scoped_feature_list.h"
#import "ios/chrome/browser/download/model/download_directory_util.h"
#import "ios/chrome/browser/download/model/download_record.h"
#import "ios/chrome/browser/download/model/download_record_service_factory.h"
#import "ios/chrome/browser/download/ui/download_list/download_list_table_view_controller.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/utils/mime_type_util.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/download_list_commands.h"
#import "ios/chrome/browser/shared/public/commands/download_record_commands.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/test/scoped_key_window.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

// Mock handler for DownloadListCommands that delegates to the coordinator.
@interface MockDownloadListCommandsHandler : NSObject <DownloadListCommands>

@property(nonatomic, strong) DownloadListCoordinator* downloadListCoordinator;

@end

@implementation MockDownloadListCommandsHandler

- (void)showDownloadList {
  [self.downloadListCoordinator start];
}

- (void)hideDownloadList {
  [self.downloadListCoordinator stop];
}

@end

// Test fixture for DownloadListCoordinator functionality.
class DownloadListCoordinatorTest : public PlatformTest {
 protected:
  DownloadListCoordinatorTest() {
    feature_list_.InitAndEnableFeature(kDownloadList);
    DownloadRecordServiceFactory::GetInstance();
    profile_ = TestProfileIOS::Builder().Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());
    base_view_controller_ = [[UIViewController alloc] init];

    coordinator_ = [[DownloadListCoordinator alloc]
        initWithBaseViewController:base_view_controller_
                           browser:browser_.get()];
    CommandDispatcher* dispatcher = browser_.get()->GetCommandDispatcher();
    MockDownloadListCommandsHandler* handler =
        [[MockDownloadListCommandsHandler alloc] init];
    handler.downloadListCoordinator = coordinator_;
    [dispatcher startDispatchingToTarget:handler
                             forProtocol:@protocol(DownloadListCommands)];
    [scoped_key_window_.Get() setRootViewController:base_view_controller_];
  }

  void SetUp() override {
    PlatformTest::SetUp();

    // Set up test downloads directory.
    ASSERT_TRUE(test_downloads_dir_.CreateUniqueTempDir());
    downloads_path_ = test_downloads_dir_.GetPath().AppendASCII("downloads");
    ASSERT_TRUE(base::CreateDirectory(downloads_path_));
    test::SetDownloadsDirectoryForTesting(&downloads_path_);
  }

  void TearDown() override {
    // Reset downloads directory override.
    test::SetDownloadsDirectoryForTesting(nullptr);
    PlatformTest::TearDown();
  }

  ~DownloadListCoordinatorTest() override {
    CommandDispatcher* dispatcher = browser_.get()->GetCommandDispatcher();
    [dispatcher stopDispatchingForProtocol:@protocol(DownloadListCommands)];
    [dispatcher stopDispatchingForProtocol:@protocol(DownloadRecordCommands)];
    [coordinator_ stop];
    task_environment_.RunUntilIdle();
    coordinator_ = nil;
    browser_.reset();
    profile_.reset();
    task_environment_.RunUntilIdle();
  }

  base::test::ScopedFeatureList feature_list_;
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  UIViewController* base_view_controller_;
  ScopedKeyWindow scoped_key_window_;
  DownloadListCoordinator* coordinator_;
  base::ScopedTempDir test_downloads_dir_;
  base::FilePath downloads_path_;
};

// Tests that the coordinator initializes with the correct base view controller
// and browser instance.
TEST_F(DownloadListCoordinatorTest, Initialization) {
  EXPECT_TRUE(coordinator_);
  EXPECT_EQ(coordinator_.baseViewController, base_view_controller_);
  EXPECT_EQ(coordinator_.browser, browser_.get());
}

// Tests starting the coordinator and verifies command dispatcher registration.
TEST_F(DownloadListCoordinatorTest, Start) {
  [coordinator_ start];

  // Verify command dispatcher registration.
  CommandDispatcher* dispatcher = browser_->GetCommandDispatcher();
  id<DownloadRecordCommands> handler =
      HandlerForProtocol(dispatcher, DownloadRecordCommands);
  EXPECT_TRUE(handler);
  EXPECT_TRUE(
      [coordinator_ conformsToProtocol:@protocol(DownloadRecordCommands)]);
  // Wait for async actions to complete.
  task_environment_.RunUntilIdle();
}

// Tests that the coordinator creates and presents the download list UI with the
// correct navigation structure when started.
TEST_F(DownloadListCoordinatorTest, StartCreatesAndPresentsDownloadListUI) {
  [coordinator_ start];

  // Verify that the base view controller has a presented view controller
  // after starting the coordinator.
  EXPECT_TRUE(base_view_controller_.presentedViewController);
  EXPECT_TRUE([base_view_controller_.presentedViewController
      isKindOfClass:[UINavigationController class]]);

  UINavigationController* navigationController =
      (UINavigationController*)base_view_controller_.presentedViewController;

  // Verify the navigation controller has the download list table view
  // controller as its root view controller.
  EXPECT_EQ(navigationController.viewControllers.count, 1u);
  EXPECT_TRUE([navigationController.topViewController
      isKindOfClass:[DownloadListTableViewController class]]);
  // Wait for async actions to complete.
  task_environment_.RunUntilIdle();
}

// Tests opening a PDF file with download record and verifies that the correct
// command is dispatched with proper URL and virtual URL properties.
TEST_F(DownloadListCoordinatorTest, OpenFileWithDownloadRecordPDF) {
  [coordinator_ start];

  // Create a mock ApplicationCommands handler
  id mockApplicationCommands = OCMProtocolMock(@protocol(ApplicationCommands));
  CommandDispatcher* dispatcher = browser_->GetCommandDispatcher();
  [dispatcher startDispatchingToTarget:mockApplicationCommands
                           forProtocol:@protocol(ApplicationCommands)];

  // Create a PDF file in the downloads directory for testing.
  base::FilePath downloads_directory;
  GetDownloadsDirectory(&downloads_directory);
  base::FilePath pdf_file_path = downloads_directory.Append("document.pdf");
  ASSERT_TRUE(base::WriteFile(pdf_file_path, "PDF content"));

  // Create a PDF download record.
  DownloadRecord pdf_record;
  pdf_record.download_id = "test_pdf_id";
  pdf_record.file_path = base::FilePath("document.pdf");
  pdf_record.mime_type = kAdobePortableDocumentFormatMimeType;
  pdf_record.original_url = "https://example.com/document.pdf";

  // Set up detailed expectation for openURLInNewTab call.
  OCMExpect([mockApplicationCommands
      openURLInNewTab:[OCMArg checkWithBlock:^BOOL(OpenNewTabCommand* command) {
        // Verify the command object properties match expected values.
        EXPECT_TRUE(command != nil);
        EXPECT_FALSE(command.inIncognito);
        EXPECT_FALSE(command.inBackground);
        EXPECT_EQ(command.appendTo, OpenPosition::kCurrentTab);

        // Verify URL contains the file name.
        NSString* urlString = base::SysUTF8ToNSString(command.URL.spec());
        EXPECT_TRUE([urlString containsString:@"document.pdf"]);

        // Verify virtual URL is set for downloads.
        EXPECT_TRUE(command.virtualURL.is_valid());
        NSString* virtualUrlString =
            base::SysUTF8ToNSString(command.virtualURL.spec());
        EXPECT_TRUE([virtualUrlString containsString:@"chrome://downloads/"]);

        return YES;
      }]]);

  // Call the method under test through command dispatcher.
  id<DownloadRecordCommands> downloadRecordHandler =
      HandlerForProtocol(dispatcher, DownloadRecordCommands);
  [downloadRecordHandler
      openFileWithPath:ConvertToAbsoluteDownloadPath(pdf_record.file_path)
              mimeType:pdf_record.mime_type];

  // Wait for the openURLInNewTab call to complete.
  task_environment_.RunUntilIdle();

  // Verify all mock expectations were met.
  [mockApplicationCommands verify];

  // Clean up the test file.
  base::DeleteFile(pdf_file_path);
}

// Tests opening a non-PDF file with download record and verifies that no
// command is dispatched since non-PDF files are not supported for opening.
TEST_F(DownloadListCoordinatorTest, OpenFileWithDownloadRecordNonPDF) {
  [coordinator_ start];

  // Create an image file in the downloads directory for testing.
  base::FilePath downloads_directory;
  GetDownloadsDirectory(&downloads_directory);
  base::FilePath image_file_path = downloads_directory.Append("test.jpg");
  ASSERT_TRUE(base::WriteFile(image_file_path, "JPEG content"));

  // Create an image download record.
  DownloadRecord image_record;
  image_record.download_id = "test_image_id";
  image_record.file_path = base::FilePath("test.jpg");
  image_record.mime_type = "image/jpeg";
  image_record.original_url = "https://example.com/test.jpg";

  // Create a mock ApplicationCommands handler.
  id mockApplicationCommands = OCMProtocolMock(@protocol(ApplicationCommands));
  CommandDispatcher* dispatcher = browser_->GetCommandDispatcher();
  [dispatcher startDispatchingToTarget:mockApplicationCommands
                           forProtocol:@protocol(ApplicationCommands)];

  // For non-PDF files, openURLInNewTab should NOT be called.
  [[mockApplicationCommands reject] openURLInNewTab:[OCMArg any]];

  // Call the method under test through command dispatcher.
  dispatcher = browser_->GetCommandDispatcher();
  id<DownloadRecordCommands> downloadRecordHandler2 =
      HandlerForProtocol(dispatcher, DownloadRecordCommands);
  [downloadRecordHandler2
      openFileWithPath:ConvertToAbsoluteDownloadPath(image_record.file_path)
              mimeType:image_record.mime_type];

  // Wait for async file existence check to complete.
  task_environment_.RunUntilIdle();

  // Verify all mock expectations were met.
  [mockApplicationCommands verify];

  // Clean up the test file.
  base::DeleteFile(image_file_path);
}

// Tests opening a file that doesn't exist and verifies that no command is
// dispatched when the file is not found on disk.
TEST_F(DownloadListCoordinatorTest, OpenFileWithDownloadRecordFileNotExists) {
  [coordinator_ start];

  // Create a download record for a non-existent file.
  DownloadRecord missing_record;
  missing_record.download_id = "missing_file_id";
  missing_record.file_path = base::FilePath("nonexistent.pdf");
  missing_record.mime_type = kAdobePortableDocumentFormatMimeType;
  missing_record.original_url = "https://example.com/nonexistent.pdf";

  // Create a mock ApplicationCommands handler.
  id mockApplicationCommands = OCMProtocolMock(@protocol(ApplicationCommands));
  CommandDispatcher* dispatcher = browser_->GetCommandDispatcher();
  [dispatcher startDispatchingToTarget:mockApplicationCommands
                           forProtocol:@protocol(ApplicationCommands)];

  // For non-existent files, openURLInNewTab should NOT be called.
  [[mockApplicationCommands reject] openURLInNewTab:[OCMArg any]];

  // Call the method under test through command dispatcher.
  dispatcher = browser_->GetCommandDispatcher();
  id<DownloadRecordCommands> downloadRecordHandler3 =
      HandlerForProtocol(dispatcher, DownloadRecordCommands);
  [downloadRecordHandler3
      openFileWithPath:ConvertToAbsoluteDownloadPath(missing_record.file_path)
              mimeType:missing_record.mime_type];

  // Wait for async file existence check to complete.
  task_environment_.RunUntilIdle();

  // Verify all mock expectations were met.
  [mockApplicationCommands verify];
}
