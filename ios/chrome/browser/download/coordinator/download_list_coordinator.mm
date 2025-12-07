// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/download/coordinator/download_list_coordinator.h"

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#import "base/files/file_path.h"
#import "base/files/file_util.h"
#import "base/functional/bind.h"
#import "base/strings/stringprintf.h"
#import "base/strings/sys_string_conversions.h"
#import "base/task/thread_pool.h"
#import "ios/chrome/browser/download/coordinator/download_file_preview_coordinator.h"
#import "ios/chrome/browser/download/coordinator/download_list_mediator.h"
#import "ios/chrome/browser/download/model/download_directory_util.h"
#import "ios/chrome/browser/download/model/download_record.h"
#import "ios/chrome/browser/download/model/download_record_service_factory.h"
#import "ios/chrome/browser/download/ui/download_list/download_list_action_delegate.h"
#import "ios/chrome/browser/download/ui/download_list/download_list_item.h"
#import "ios/chrome/browser/download/ui/download_list/download_list_table_view_controller.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/model/utils/mime_type_util.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/download_record_commands.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/components/webui/web_ui_url_constants.h"
#import "ios/web/public/navigation/referrer.h"
#import "url/gurl.h"

@interface DownloadListCoordinator () <DownloadListActionDelegate,
                                       DownloadRecordCommands> {
  // Mediator for handling download list logic.
  DownloadListMediator* _mediator;

  // View controller for presenting Download List UI.
  DownloadListTableViewController* _downloadListViewController;

  // Navigation controller for the download list.
  UINavigationController* _navigationController;

  // Coordinator for file preview functionality.
  DownloadFilePreviewCoordinator* _filePreviewCoordinator;

  // Activity view controller for sharing downloads.
  UIActivityViewController* _shareActivityController;

  // YES after start has been called.
  BOOL _started;
}
@end

@implementation DownloadListCoordinator

- (void)start {
  if (_started) {
    return;
  }
  [super start];

  _started = YES;

  ProfileIOS* profile = self.browser->GetProfile();
  DownloadRecordService* downloadRecordService =
      DownloadRecordServiceFactory::GetForProfile(profile);
  BOOL isIncognito = profile->IsOffTheRecord();
  _mediator = [[DownloadListMediator alloc]
      initWithDownloadRecordService:downloadRecordService
                        isIncognito:isIncognito];

  [_mediator connect];

  // Register coordinator as DownloadRecordCommands handler.
  [self.browser->GetCommandDispatcher()
      startDispatchingToTarget:self
                   forProtocol:@protocol(DownloadRecordCommands)];

  [self setupAndPresentDownloadListUI];
}

- (void)stop {
  if (!_started) {
    return;
  }
  [super stop];
  _started = NO;

  // Dismiss any currently presented download list UI if we're dismissing it
  // programmatically.
  if (_navigationController) {
    [_navigationController.presentingViewController
        dismissViewControllerAnimated:YES
                           completion:nil];
  }

  _downloadListViewController = nil;
  _navigationController = nil;
  _shareActivityController = nil;

  // Stop and clean up file preview coordinator
  if (_filePreviewCoordinator) {
    [_filePreviewCoordinator stop];
    _filePreviewCoordinator = nil;
  }

  [_mediator disconnect];
  _mediator = nil;

  [self.browser->GetCommandDispatcher() stopDispatchingToTarget:self];
}

#pragma mark - Private methods

// Creates, configures and presents the download list view controller.
- (void)setupAndPresentDownloadListUI {
  // Create view controller based on UI type.
  DownloadListUIType uiType = CurrentDownloadListUIType();
  switch (uiType) {
    case DownloadListUIType::kDefaultUI:
      _downloadListViewController = [[DownloadListTableViewController alloc]
          initWithStyle:UITableViewStyleInsetGrouped];
      break;
    case DownloadListUIType::kCustomUI:
      // Custom UI can be implemented here if needed.
      // For now, we will use the default implementation.
      _downloadListViewController = [[DownloadListTableViewController alloc]
          initWithStyle:UITableViewStyleInsetGrouped];
      break;
  }
  _downloadListViewController.mutator = _mediator;
  _downloadListViewController.actionDelegate = self;
  [_mediator setConsumer:_downloadListViewController];

  CommandDispatcher* commandDispatcher = self.browser->GetCommandDispatcher();
  id<DownloadListCommands> downloadListHandler =
      HandlerForProtocol(commandDispatcher, DownloadListCommands);
  id<DownloadRecordCommands> downloadRecordHandler =
      HandlerForProtocol(commandDispatcher, DownloadRecordCommands);
  _downloadListViewController.downloadListHandler = downloadListHandler;
  _downloadListViewController.downloadRecordHandler = downloadRecordHandler;

  _navigationController = [[UINavigationController alloc]
      initWithRootViewController:_downloadListViewController];
  _navigationController.presentationController.delegate =
      _downloadListViewController;

  [self.baseViewController presentViewController:_navigationController
                                        animated:YES
                                      completion:nil];
}

#pragma mark - DownloadRecordCommands

- (void)openFileWithPath:(const base::FilePath&)filePath
                mimeType:(const std::string&)mimeType {
  base::FilePath filePathCopy = filePath;
  __weak __typeof(self) weakSelf = self;
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&base::PathExists, filePathCopy),
      base::BindOnce(^(bool fileExists) {
        [weakSelf handleOpenFilePath:filePathCopy
                            mimeType:mimeType
                          fileExists:fileExists];
      }));
}

- (void)shareDownloadedFile:(const DownloadRecord&)record
                 sourceView:(UIView*)sourceView {
  base::FilePath filePath = ConvertToAbsoluteDownloadPath(record.file_path);
  NSURL* fileURL =
      [NSURL fileURLWithPath:base::SysUTF8ToNSString(filePath.value())];
  if (!fileURL) {
    return;
  }

  _shareActivityController =
      [[UIActivityViewController alloc] initWithActivityItems:@[ fileURL ]
                                        applicationActivities:nil];

  // Configure popover presentation for iPad.
  _shareActivityController.popoverPresentationController.sourceView =
      sourceView;
  _shareActivityController.popoverPresentationController.sourceRect =
      sourceView.bounds;

  [_downloadListViewController presentViewController:_shareActivityController
                                            animated:YES
                                          completion:nil];
}

#pragma mark - Private Helper Methods

- (void)handleOpenFilePath:(const base::FilePath&)filePath
                  mimeType:(const std::string&)mimeType
                fileExists:(bool)fileExists {
  // This method now guaranteed to run on main thread.
  if (!fileExists) {
    // File not found, possibly moved or deleted.
    // Show user-friendly error message in future iteration.
    return;
  }

  // Check if this is a PDF file that should open in a new tab.
  if (mimeType == kAdobePortableDocumentFormatMimeType) {
    [self openPDFInNewTab:filePath];
  } else {
    [self openFileWithSystemPreview:filePath];
  }
}

// Opens a PDF file in a new tab.
- (void)openPDFInNewTab:(const base::FilePath&)filePath {
  GURL filePathURL =
      GURL(base::StringPrintf("%s://%s", "file", filePath.value().c_str()));

  GURL virtualFilePathURL = GURL(
      base::StringPrintf("%s://%s/%s", kChromeUIScheme, kChromeUIDownloadsHost,
                         filePathURL.ExtractFileName().c_str()));

  OpenNewTabCommand* command = [[OpenNewTabCommand alloc]
       initWithURL:filePathURL
        virtualURL:virtualFilePathURL
          referrer:web::Referrer()
       inIncognito:self.browser->GetProfile()->IsOffTheRecord()
      inBackground:NO
          appendTo:OpenPosition::kCurrentTab];

  id<ApplicationCommands> applicationCommands = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), ApplicationCommands);
  [applicationCommands openURLInNewTab:command];
}

// Opens a file with the system's preview.
- (void)openFileWithSystemPreview:(const base::FilePath&)filePath {
  if (!_filePreviewCoordinator) {
    _filePreviewCoordinator = [[DownloadFilePreviewCoordinator alloc]
        initWithBaseViewController:_downloadListViewController
                           browser:self.browser];
    [_filePreviewCoordinator start];
  }
  NSURL* fileURL =
      [NSURL fileURLWithPath:base::SysUTF8ToNSString(filePath.value().c_str())];
  [_filePreviewCoordinator presentFilePreviewWithURL:fileURL];
}

#pragma mark - DownloadListActionDelegate

- (void)openDownloadInFiles:(DownloadListItem*)item {
  base::FilePath filePath = item.filePath;

  NSString* pathString = base::SysUTF8ToNSString(filePath.value());
  NSString* filesURLString =
      [NSString stringWithFormat:@"shareddocuments://%@", pathString];
  NSURL* filesURL = [NSURL URLWithString:filesURLString];

  [[UIApplication sharedApplication] openURL:filesURL
                                     options:@{}
                           completionHandler:nil];
}

@end
