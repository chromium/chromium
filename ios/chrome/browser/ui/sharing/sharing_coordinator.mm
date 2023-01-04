// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/sharing/sharing_coordinator.h"

#import <MaterialComponents/MaterialSnackbar.h>

#import "base/files/file_util.h"
#import "base/ios/block_types.h"
#import "base/mac/foundation_util.h"
#import "base/metrics/histogram_macros.h"
#import "base/strings/sys_string_conversions.h"
#import "base/task/thread_pool.h"
#import "base/threading/scoped_blocking_call.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/open_in/open_in_tab_helper.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/commands/qr_generation_commands.h"
#import "ios/chrome/browser/ui/commands/share_download_overlay_commands.h"
#import "ios/chrome/browser/ui/commands/snackbar_commands.h"
#import "ios/chrome/browser/ui/open_in/features.h"
#import "ios/chrome/browser/ui/open_in/open_in_histograms.h"
#import "ios/chrome/browser/ui/sharing/activity_services/activity_params.h"
#import "ios/chrome/browser/ui/sharing/activity_services/activity_service_coordinator.h"
#import "ios/chrome/browser/ui/sharing/activity_services/requirements/activity_service_positioner.h"
#import "ios/chrome/browser/ui/sharing/activity_services/requirements/activity_service_presentation.h"
#import "ios/chrome/browser/ui/sharing/qr_generator/qr_generator_coordinator.h"
#import "ios/chrome/browser/ui/sharing/share_download_overlay_coordinator.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/web/public/browser_state.h"
#import "ios/web/public/download/crw_web_view_download.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "net/base/load_flags.h"
#import "net/base/mac/url_conversions.h"
#import "services/network/public/cpp/resource_request.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"
#import "services/network/public/cpp/simple_url_loader.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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

// Ensures the destination directory is created and any contained obsolete files
// are deleted. Returns YES if the directory is created successfully.
BOOL CreateDestinationDirectoryAndRemoveObsoleteFiles() {
  NSString* temporary_directory_path = GetTemporaryDocumentDirectory();
  base::File::Error error;
  if (!CreateDirectoryAndGetError(
          base::mac::NSStringToFilePath(temporary_directory_path), &error)) {
    DLOG(ERROR) << "Error creating destination dir: " << error;
    return NO;
  }
  // Remove all documents that might be still on temporary storage.
  RemoveAllStoredDocumentsAtPath(temporary_directory_path);
  return YES;
}

}  // namespace

@interface SharingCoordinator () <ActivityServicePositioner,
                                  ActivityServicePresentation,
                                  CRWWebViewDownloadDelegate,
                                  QRGenerationCommands>

@property(nonatomic, strong)
    ActivityServiceCoordinator* activityServiceCoordinator;

// Coordinator that manage the overlay view displayed while downloading the
// file.
@property(nonatomic, strong) ShareDownloadOverlayCoordinator* overlay;

@property(nonatomic, strong) QRGeneratorCoordinator* qrGeneratorCoordinator;

@property(nonatomic, strong) ActivityParams* params;

@property(nonatomic, weak) UIView* originView;

@property(nonatomic, assign) CGRect originRect;

@property(nonatomic, weak) UIBarButtonItem* anchor;

// Path where the downloaded file is saved.
@property(nonatomic, strong) NSURL* fileNSURL;

// String where the downloaded file is saved.
@property(nonatomic, strong) NSString* filePath;

// CRWWebViewDownload instance that handle download interactions.
@property(nonatomic, strong) id<CRWWebViewDownload> download;

// YES if the file download was canceled.
@property(nonatomic, assign) BOOL isDownloadCanceled;

// Command dispatcher.
@property(nonatomic, strong) CommandDispatcher* dispatcher;

@end

@implementation SharingCoordinator {
  // Loader used to redownload the document and save it in the sandbox.
  // TODO(crbug.com/1357553): Remove when Open In download experiment is
  // finished.
  std::unique_ptr<network::SimpleURLLoader> _urlLoader;

  // URLLoaderFactory instance needed for URLLoader.
  // TODO(crbug.com/1357553): Remove when Open In download experiment is
  // finished.
  scoped_refptr<network::SharedURLLoaderFactory> _urlLoaderFactory;
}

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                    params:(ActivityParams*)params
                                originView:(UIView*)originView {
  DCHECK(originView);
  self = [self initWithBaseViewController:viewController
                                  browser:browser
                                   params:params
                               originView:originView
                               originRect:originView.bounds
                                   anchor:nil];
  return self;
}

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                    params:(ActivityParams*)params
                                    anchor:(UIBarButtonItem*)anchor {
  DCHECK(anchor);
  self = [self initWithBaseViewController:viewController
                                  browser:browser
                                   params:params
                               originView:nil
                               originRect:CGRectZero
                                   anchor:anchor];
  return self;
}

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                    params:(ActivityParams*)params
                                originView:(UIView*)originView
                                originRect:(CGRect)originRect
                                    anchor:(UIBarButtonItem*)anchor {
  DCHECK(params);
  if (self = [super initWithBaseViewController:viewController
                                       browser:browser]) {
    _params = params;
    _originView = originView;
    _originRect = originRect;
    _anchor = anchor;
  }
  return self;
}

#pragma mark - ChromeCoordinator

- (void)start {
  web::WebState* currentWebState =
      self.browser->GetWebStateList()->GetActiveWebState();
  if (currentWebState && OpenInTabHelper::ShouldDownload(currentWebState) &&
      IsOpenInActivitiesInShareButtonEnabled()) {
    // Creating the directory can block the main thread, so perform it on a
    // background sequence, then on current sequence complete the workflow.
    __weak SharingCoordinator* weakSelf = self;
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::TaskPriority::USER_VISIBLE, base::MayBlock()},
        base::BindOnce(&CreateDestinationDirectoryAndRemoveObsoleteFiles),
        base::BindOnce(^(BOOL directoryCreated) {
          [weakSelf startDownloadWithExistingDirectory:directoryCreated
                                              webState:currentWebState];
        }));
  } else {
    [self startActivityService];
  }
}

- (void)stop {
  [self activityServiceDidEndPresenting];
  [self hideQRCode];
  self.originView = nil;
}

#pragma mark - ActivityServicePositioner

- (UIView*)sourceView {
  return self.originView;
}

- (CGRect)sourceRect {
  return self.originRect;
}

- (UIBarButtonItem*)barButtonItem {
  return self.anchor;
}

#pragma mark - ActivityServicePresentation

- (void)activityServiceDidEndPresenting {
  [self.activityServiceCoordinator stop];
  self.activityServiceCoordinator = nil;

  _urlLoader.reset();

  // If a new download with a file with the same name exist it will throw an
  // error in downloadDidFailWithError method.
  [self removeFile];
}

#pragma mark - QRGenerationCommands

- (void)generateQRCode:(GenerateQRCodeCommand*)command {
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

// Starts download only if the final directory is created, if not created, shows
// the share menu without file options.
- (void)startDownloadWithExistingDirectory:(BOOL)directoryCreated
                                  webState:(web::WebState*)webState {
  if (directoryCreated) {
    [self startDisplayDownloadOverlayOnWebView:webState];
    [self startDownloadFromWebState:webState];
  } else {
    [self startActivityService];
  }
}

// Starts the share menu feature.
- (void)startActivityService {
  self.activityServiceCoordinator = [[ActivityServiceCoordinator alloc]
      initWithBaseViewController:self.baseViewController
                         browser:self.browser
                          params:self.params];

  self.activityServiceCoordinator.positionProvider = self;
  self.activityServiceCoordinator.presentationProvider = self;
  self.activityServiceCoordinator.scopedHandler = self;

  [self.activityServiceCoordinator start];
}

// Returns YES if the file located at `URL` can be read.
- (BOOL)hasValidFileAtURL:(NSURL*)URL {
  if (!URL)
    return false;

  return [[NSFileManager defaultManager] isReadableFileAtPath:URL.path];
}

// Starts downloading the file currently displayed at path `self.filePath`.
- (void)startDownloadFromWebState:(web::WebState*)webState {
  self.isDownloadCanceled = NO;
  NSString* tempDirPath = GetTemporaryDocumentDirectory();
  OpenInTabHelper* helper = OpenInTabHelper::FromWebState(webState);
  self.filePath = [tempDirPath
      stringByAppendingPathComponent:base::SysUTF16ToNSString(
                                         helper->GetFileNameSuggestion())];
  self.fileNSURL = [NSURL fileURLWithPath:self.filePath];

  if (@available(iOS 14.5, *)) {
    if (IsOpenInNewDownloadEnabled()) {
      __weak SharingCoordinator* weakSelf = self;
      webState->DownloadCurrentPage(self.filePath, self,
                                    ^(id<CRWWebViewDownload> download) {
                                      weakSelf.download = download;
                                    });
      return;
    }
  }

  // Download the document and save it at `self.filePath`.
  // TODO(crbug.com/1357553): Remove when Open In download experiment is
  // finished.
  web::NavigationItem* item =
      webState->GetNavigationManager()->GetLastCommittedItem();
  const GURL& last_committed_url = item ? item->GetURL() : GURL::EmptyGURL();

  auto resourceRequest = std::make_unique<network::ResourceRequest>();
  resourceRequest->url = last_committed_url;
  resourceRequest->load_flags = net::LOAD_SKIP_CACHE_VALIDATION;

  _urlLoader = network::SimpleURLLoader::Create(std::move(resourceRequest),
                                                NO_TRAFFIC_ANNOTATION_YET);

  _urlLoaderFactory = webState->GetBrowserState()->GetSharedURLLoaderFactory();

  __weak SharingCoordinator* weakSelf = self;
  _urlLoader->DownloadToFile(
      std::move(_urlLoaderFactory).get(),
      base::BindOnce(^(base::FilePath filePath) {
        if (!weakSelf.isDownloadCanceled) {
          if ([weakSelf hasValidFileAtURL:weakSelf.fileNSURL]) {
            [weakSelf downloadDidFinish];
          } else {
            [weakSelf downloadDidFailWithError:nil];
          }
        }
      }),
      base::FilePath(base::SysNSStringToUTF8(self.filePath)));
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
                        webState:currentWebState];
  [self.overlay start];
}

// Removes `self.overlay` from the top view of the application.
- (void)stopDisplayDownloadOverlay {
  [self.overlay stop];
  self.overlay = nil;
  [self.dispatcher stopDispatchingToTarget:self];
}

// Removes downloaded file at `self.filePath`.
- (void)removeFile {
  if ([[NSFileManager defaultManager] fileExistsAtPath:self.filePath]) {
    __weak SharingCoordinator* weakSelf = self;
    base::ThreadPool::PostTask(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
        base::BindOnce(^{
          NSError* error = nil;
          if (![[NSFileManager defaultManager]
                  removeItemAtPath:weakSelf.filePath
                             error:&error]) {
            DLOG(ERROR) << "Failed to remove file: "
                        << base::SysNSStringToUTF8([error description]);
          }
        }));
  }
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
  self.isDownloadCanceled = YES;
  [self stopDisplayDownloadOverlay];
  if (@available(iOS 14.5, *)) {
    [self.download cancelDownload];
  }
  UMA_HISTOGRAM_ENUMERATION(kOpenInDownloadHistogram,
                            OpenInDownloadResult::kCanceled);
}

@end
