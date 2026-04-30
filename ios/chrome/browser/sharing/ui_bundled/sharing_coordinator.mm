// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sharing/ui_bundled/sharing_coordinator.h"

#import "base/apple/foundation_util.h"
#import "base/files/file_util.h"
#import "base/ios/block_types.h"
#import "base/metrics/histogram_macros.h"
#import "base/strings/sys_string_conversions.h"
#import "base/task/thread_pool.h"
#import "base/threading/scoped_blocking_call.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/activity_service_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/qr_generation_commands.h"
#import "ios/chrome/browser/shared/public/commands/share_download_overlay_commands.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/sharing/model/share_file_download_tab_helper.h"
#import "ios/chrome/browser/sharing/ui_bundled/activity_services/activity_service_coordinator.h"
#import "ios/chrome/browser/sharing/ui_bundled/activity_services/activity_service_presentation.h"
#import "ios/chrome/browser/sharing/ui_bundled/qr_generator/qr_generator_coordinator.h"
#import "ios/chrome/browser/sharing/ui_bundled/share_download_overlay_coordinator.h"
#import "ios/chrome/browser/sharing/ui_bundled/share_file_download_metrics.h"
#import "ios/chrome/browser/sharing/ui_bundled/sharing_params.h"
#import "ios/web/public/download/crw_web_view_download.h"

// Exposes methods to allow calling the from helper free functions.
@interface SharingCoordinator (ForHelperFunction)

// Starts the download if `directoryCreated`. If not, show the share menu
// without file options.
- (void)startDownloadForWebState:(web::WebState*)webState
                directoryCreated:(BOOL)directoryCreated;

@end

namespace {

// The path in the temp directory containing documents that are to be opened in
// other applications.
static NSString* const kDocumentsTemporaryPath = @"OpenIn";

// Returns the temporary path where documents are stored.
NSString* GetTemporaryDocumentDirectory() {
  return [NSTemporaryDirectory()
      stringByAppendingPathComponent:kDocumentsTemporaryPath];
}

// Removes all the stored files at `path`.
void RemoveAllStoredDocumentsAtPath(NSString* path) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::WILL_BLOCK);
  NSFileManager* file_manager = [NSFileManager defaultManager];

  NSError* error = nil;
  NSArray<NSString*>* document_files =
      [file_manager contentsOfDirectoryAtPath:path error:&error];
  if (!document_files) {
    DLOG(ERROR) << "Failed to get content of directory at path: "
                << base::SysNSStringToUTF8([error description]);
    return;
  }

  for (NSString* filename in document_files) {
    NSString* file_path = [path stringByAppendingPathComponent:filename];
    if (![file_manager removeItemAtPath:file_path error:&error]) {
      DLOG(ERROR) << "Failed to remove file: "
                  << base::SysNSStringToUTF8([error description]);
    }
  }
}

// Remove a file stored at `path` if it exists.
void RemoveFileAtPath(NSString* path) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::WILL_BLOCK);
  NSFileManager* file_manager = [NSFileManager defaultManager];

  if ([file_manager fileExistsAtPath:path]) {
    NSError* error = nil;
    if (![file_manager removeItemAtPath:path error:&error]) {
      DLOG(ERROR) << "Failed to remove file: "
                  << base::SysNSStringToUTF8([error description]);
    }
  }
}

// Ensures the destination directory is created and any contained obsolete files
// are deleted. Returns YES if the directory is created successfully.
BOOL CreateDestinationDirectoryAndRemoveObsoleteFiles() {
  NSString* temporary_directory_path = GetTemporaryDocumentDirectory();
  base::File::Error error;
  if (!CreateDirectoryAndGetError(
          base::apple::NSStringToFilePath(temporary_directory_path), &error)) {
    DLOG(ERROR) << "Error creating destination dir: " << error;
    return NO;
  }
  // Remove all documents that might be still on temporary storage.
  RemoveAllStoredDocumentsAtPath(temporary_directory_path);
  return YES;
}

// Starts download for `weak_web_state` if `directory_created` using
// `coordinator`.
void StartDownloadForWebState(__weak SharingCoordinator* coordinator,
                              base::WeakPtr<web::WebState> weak_web_state,
                              BOOL directory_created) {
  if (web::WebState* web_state = weak_web_state.get()) {
    [coordinator startDownloadForWebState:web_state
                         directoryCreated:directory_created];
  }
}

}  // namespace

@interface SharingCoordinator () <ActivityServicePresentation,
                                  CRWWebViewDownloadDelegate,
                                  QRGenerationCommands,
                                  ShareDownloadOverlayCommands>

@property(nonatomic, strong)
    ActivityServiceCoordinator* activityServiceCoordinator;

// Coordinator that manage the overlay view displayed while downloading the
// file.
@property(nonatomic, strong) ShareDownloadOverlayCoordinator* overlay;

@property(nonatomic, strong) QRGeneratorCoordinator* qrGeneratorCoordinator;

@property(nonatomic, strong) SharingParams* params;

// Path where the downloaded file is saved.
@property(nonatomic, strong) NSURL* fileNSURL;

// String where the downloaded file is saved.
@property(nonatomic, copy) NSString* filePath;

// CRWWebViewDownload instance that handle download interactions.
@property(nonatomic, strong) id<CRWWebViewDownload> download;

// YES if the file download was canceled.
@property(nonatomic, assign) BOOL isDownloadCanceled;

// YES if the file download is in the process of cancelling.
@property(nonatomic, assign) BOOL isCancelling;

// YES if this coordinator should be restarted.
@property(nonatomic, assign) BOOL shouldRestartCoordinator;

// Command dispatcher.
@property(nonatomic, strong) CommandDispatcher* dispatcher;

@end

@implementation SharingCoordinator {
  scoped_refptr<base::SequencedTaskRunner> _taskRunner;
  // The source item for the presentation.
  id<UIPopoverPresentationControllerSourceItem> _sourceItem;
  // The source view for the presentation.
  UIView* _sourceView;
  // The source rect in the _sourceView for the presentation.
  CGRect _sourceRect;
}

- (instancetype)
    initWithBaseViewController:(UIViewController*)viewController
                       browser:(Browser*)browser
                        params:(SharingParams*)params
                    sourceItem:(id<UIPopoverPresentationControllerSourceItem>)
                                   sourceItem {
  DCHECK(params);
  if ((self = [super initWithBaseViewController:viewController
                                        browser:browser])) {
    _params = params;
    _sourceItem = sourceItem;
    _taskRunner = base::ThreadPool::CreateSequencedTaskRunner(
        {base::TaskPriority::USER_VISIBLE, base::MayBlock()});
  }
  return self;
}

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                    params:(SharingParams*)params
                                sourceView:(UIView*)sourceView
                                sourceRect:(CGRect)sourceRect {
  DCHECK(params);
  if ((self = [super initWithBaseViewController:viewController
                                        browser:browser])) {
    _params = params;
    _sourceView = sourceView;
    _sourceRect = sourceRect;
    _taskRunner = base::ThreadPool::CreateSequencedTaskRunner(
        {base::TaskPriority::USER_VISIBLE, base::MayBlock()});
  }
  return self;
}

// The behaviour is predictable: the coordinator will be stopped, either right
// now or delayed (in -cancelDownload method). If we are already in the process
// of cancelling a download, do not call this again.
- (void)cancelIfNecessaryAndCreateNewCoordinatorFromView:(UIView*)shareButton {
  // Download has been cancelled or currently not download (so no overlay).
  if (self.isDownloadCanceled || !self.overlay) {
    // Stop the coordinator now.
    [self stopAndStartNewCoordinatorFromView:shareButton];
  } else if (!self.isCancelling) {
    // Delay stopping the coordinator after the download has been cancelled.
    self.shouldRestartCoordinator = YES;
    [self cancelDownloadFromView:shareButton];
  }
}

// Stop this coordinator and start a new one.
- (void)stopAndStartNewCoordinatorFromView:(UIView*)shareButton {
  id<ActivityServiceCommands> activityServiceHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), ActivityServiceCommands);
  [activityServiceHandler stopAndStartSharingCoordinatorFromView:shareButton];
}

#pragma mark - ChromeCoordinator

- (void)start {
  web::WebState* activeWebState =
      self.browser->GetWebStateList()->GetActiveWebState();
  if (activeWebState &&
      ShareFileDownloadTabHelper::ShouldDownload(activeWebState)) {
    // Creating the directory can block the main thread, so perform it on a
    // background sequence, then on current sequence complete the workflow.
    __weak SharingCoordinator* weakSelf = self;
    _taskRunner->PostTaskAndReplyWithResult(
        FROM_HERE,
        base::BindOnce(&CreateDestinationDirectoryAndRemoveObsoleteFiles),
        base::BindOnce(&StartDownloadForWebState, weakSelf,
                       activeWebState->GetWeakPtr()));
  } else {
    [self startActivityService];
  }
}

- (void)stop {
  [self activityServiceDidEndPresenting];
  [self hideQRCode];
  _sourceItem = nil;
  _sourceView = nil;
  _sourceRect = CGRectZero;
}

#pragma mark - ActivityServicePresentation

- (void)activityServiceDidEndPresenting {
  [self.activityServiceCoordinator stop];
  self.activityServiceCoordinator = nil;

  // If a new download with a file with the same name exist it will throw an
  // error in downloadDidFailWithError method.
  _taskRunner->PostTask(FROM_HERE,
                        base::BindOnce(&RemoveFileAtPath, self.filePath));
}

#pragma mark - QRGenerationCommands

- (void)showQRCode:(GenerateQRCodeCommand*)command {
  self.qrGeneratorCoordinator = [[QRGeneratorCoordinator alloc]
      initWithBaseViewController:self.baseViewController
                         browser:self.browser
                           title:command.title
                             URL:command.URL
                         handler:self];
  [self.qrGeneratorCoordinator start];
}

- (void)hideQRCode {
  [self.qrGeneratorCoordinator stop];
  self.qrGeneratorCoordinator = nil;
}

#pragma mark - Private Methods

- (void)startDownloadForWebState:(web::WebState*)webState
                directoryCreated:(BOOL)directoryCreated {
  if (directoryCreated) {
    [self startDisplayDownloadOverlayOnWebView:webState];
    [self startDownloadFromWebState:webState];
  } else {
    [self startActivityService];
  }
}

// Starts the share menu feature.
- (void)startActivityService {
  if (_sourceItem) {
    self.activityServiceCoordinator = [[ActivityServiceCoordinator alloc]
        initWithBaseViewController:self.baseViewController
                           browser:self.browser
                            params:self.params
                        sourceItem:_sourceItem];
  } else {
    self.activityServiceCoordinator = [[ActivityServiceCoordinator alloc]
        initWithBaseViewController:self.baseViewController
                           browser:self.browser
                            params:self.params
                        sourceView:_sourceView
                        sourceRect:_sourceRect];
  }

  self.activityServiceCoordinator.presentationProvider = self;
  self.activityServiceCoordinator.scopedHandler = self;

  [self.activityServiceCoordinator start];
}

// Returns YES if the file located at `URL` can be read.
- (BOOL)hasValidFileAtURL:(NSURL*)URL {
  if (!URL) {
    return false;
  }

  return [[NSFileManager defaultManager] isReadableFileAtPath:URL.path];
}

// Starts downloading the file currently displayed at path `self.filePath`.
- (void)startDownloadFromWebState:(web::WebState*)webState {
  self.isDownloadCanceled = NO;
  ShareFileDownloadTabHelper* helper =
      ShareFileDownloadTabHelper::FromWebState(webState);
  self.filePath = [GetTemporaryDocumentDirectory()
      stringByAppendingPathComponent:base::SysUTF16ToNSString(
                                         helper->GetFileNameSuggestion())];
  self.fileNSURL = [NSURL fileURLWithPath:self.filePath];

  __weak SharingCoordinator* weakSelf = self;
  webState->DownloadCurrentPage(self.filePath, self,
                                ^(id<CRWWebViewDownload> download) {
                                  weakSelf.download = download;
                                });
}

// Shows an overlayed spinner on the top view to indicate that a file download
// is in progress.
- (void)startDisplayDownloadOverlayOnWebView:(web::WebState*)currentWebState {
  self.dispatcher = self.browser->GetCommandDispatcher();
  [self.dispatcher
      startDispatchingToTarget:self
                   forProtocol:@protocol(ShareDownloadOverlayCommands)];
  self.overlay = [[ShareDownloadOverlayCoordinator alloc]
      initWithBaseViewController:self.baseViewController
                         browser:self.browser
                         webView:currentWebState->GetView()];
  [self.overlay start];
}

// Removes `self.overlay` from the top view of the application.
- (void)stopDisplayDownloadOverlay {
  [self.overlay stop];
  self.overlay = nil;
  [self.dispatcher stopDispatchingToTarget:self];
}

#pragma mark - CRWWebViewDownloadDelegate

- (void)downloadDidFinish {
  if (self.isDownloadCanceled) {
    return;
  }
  [self stopDisplayDownloadOverlay];
  self.params.filePath = self.fileNSURL;
  [self startActivityService];
  UMA_HISTOGRAM_ENUMERATION(kOpenInDownloadHistogram,
                            OpenInDownloadResult::kSucceeded);
}

- (void)downloadDidFailWithError:(NSError*)error {
  if (self.isDownloadCanceled) {
    return;
  }
  [self stopDisplayDownloadOverlay];
  [self startActivityService];
  UMA_HISTOGRAM_ENUMERATION(kOpenInDownloadHistogram,
                            OpenInDownloadResult::kFailed);
}

#pragma mark - ShareDownloadOverlayCommands

- (void)cancelDownload {
  [self cancelDownloadFromView:nil];
}

#pragma mark - Private

// Cancels the current download. If `shareButton` is not nil and
// `self.shouldRestartCoordinator` is true, restart the coordinator as if
// `shareButton` was tapped.
- (void)cancelDownloadFromView:(UIView*)shareButton {
  [self stopDisplayDownloadOverlay];
  self.isCancelling = YES;
  __weak SharingCoordinator* weakSelf = self;
  [self.download cancelDownload:^{
    [weakSelf downloadWasCancelledFromView:shareButton];
  }];
  UMA_HISTOGRAM_ENUMERATION(kOpenInDownloadHistogram,
                            OpenInDownloadResult::kCanceled);
}

// Called when the download was cancelled to restart the coordinator if needed.
- (void)downloadWasCancelledFromView:(UIView*)shareButton {
  self.isDownloadCanceled = YES;
  self.isCancelling = NO;
  if (self.shouldRestartCoordinator) {
    // Self will be destroyed after this call so it should not be used
    // anymore.
    [self stopAndStartNewCoordinatorFromView:shareButton];
  }
}

@end
