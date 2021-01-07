// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/open_in/open_in_controller.h"

#import <QuickLook/QuickLook.h>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/sys_string_conversions.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/ui/alert_coordinator/alert_coordinator.h"
#import "ios/chrome/browser/ui/open_in/open_in_controller_testing.h"
#include "ios/chrome/browser/ui/util/ui_util.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ios/web/public/thread/web_thread.h"
#import "ios/web/public/ui/crw_web_view_proxy.h"
#import "ios/web/public/ui/crw_web_view_scroll_view_proxy.h"
#import "ios/web/public/web_state.h"
#include "net/base/load_flags.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "ui/base/l10n/l10n_util_mac.h"
#import "ui/gfx/ios/NSString+CrStringDrawing.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// The path in the temp directory containing documents that are to be opened in
// other applications.
static NSString* const kDocumentsTempPath = @"OpenIn";

// Duration of the show/hide animation for the |openInToolbar_|.
const NSTimeInterval kOpenInToolbarAnimationDuration = 0.2;

// Duration to show or hide the |overlayedView_|.
const NSTimeInterval kOverlayViewAnimationDuration = 0.3;

// Time interval after which the |openInToolbar_| is automatically hidden.
const NSTimeInterval kOpenInToolbarDisplayDuration = 2.0;

// Alpha value for the background view of |overlayedView_|.
const CGFloat kOverlayedViewBackgroundAlpha = 0.6;

// Width of the label displayed on the |overlayedView_| as a percentage of the
// |overlayedView_|'s width.
const CGFloat kOverlayedViewLabelWidthPercentage = 0.7;

// Bottom margin for the label displayed on the |overlayedView_|.
const CGFloat kOverlayedViewLabelBottomMargin = 60;

// Logs the result of the download process after the user taps "open in" button.
void LogOpenInDownloadResult(const OpenInDownloadResult result) {
  UMA_HISTOGRAM_ENUMERATION("IOS.OpenIn.DownloadResult", result);
}

// Returns true if the file located at |url| is file.
bool HasValidFileAtUrl(NSURL* _Nullable url) {
  if (!url)
    return false;

  NSString* extension = [[url path] pathExtension];
  if ([extension isEqualToString:@"pdf"]) {
    base::ScopedCFTypeRef<CGPDFDocumentRef> document(
        CGPDFDocumentCreateWithURL((__bridge CFURLRef)url));
    return document;
  }

  return [QLPreviewController canPreviewItem:url];
}

}  // anonymous namespace

@interface OpenInController () <CRWWebViewScrollViewProxyObserver> {
  // AlertCoordinator for showing an alert if no applications were found to open
  // the current document.
  AlertCoordinator* _alertCoordinator;
}

// Property storing the Y content offset the scroll view the last time it was
// updated. Used to know in which direction the scroll view is scrolling.
@property(nonatomic, assign) CGFloat previousScrollViewOffset;

// SimpleURLLoader completion callback, when |urlLoader_| completes a request.
- (void)urlLoadDidComplete:(const base::FilePath&)file_path;
// Ensures the destination directory is created and any contained obsolete files
// are deleted. Returns YES if the directory is created successfully.
+ (BOOL)createDestinationDirectoryAndRemoveObsoleteFiles;
// Starts downloading the file at path |kDocumentsTempPath| with the name
// |suggestedFilename_|.
- (void)startDownload;
// Shows the overlayed toolbar |openInToolbar_|. If |withTimer| is YES, it would
// be hidden after a certain amount of time.
- (void)showOpenInToolbarWithTimer:(BOOL)withTimer;
// Hides the overlayed toolbar |openInToolbar_|.
- (void)hideOpenInToolbar;
// Called when there is a tap on the |webState_|'s view to display the
// overlayed toolbar |openInToolbar_| if necessary and (re)schedule the
// |openInTimer_|.
- (void)handleTapFrom:(UIGestureRecognizer*)gestureRecognizer;
// Downloads the file at |documentURL_| and presents the OpenIn menu for opening
// it in other applications.
- (void)exportFileWithOpenInMenuAnchoredAt:(id)sender;
// Called when there is a tap on the |overlayedView_| to cancel the file
// download.
- (void)handleTapOnOverlayedView:(UIGestureRecognizer*)gestureRecognizer;
// Removes |overlayedView_| from the top view of the application.
- (void)removeOverlayedView;
// Shows an alert with the given error message.
- (void)showErrorWithMessage:(NSString*)message;
// Presents the OpenIn menu for the file at |fileURL|.
- (void)presentOpenInMenuForFileAtURL:(NSURL*)fileURL;
// Removes the file at path |path|.
- (void)removeDocumentAtPath:(NSString*)path;
// Removes all the stored files at path |path|.
+ (void)removeAllStoredDocumentsAtPath:(NSString*)path;
// Shows an overlayed spinner on the top view to indicate that a file download
// is in progress.
- (void)showDownloadOverlayView;
// Returns a toolbar with an "Open in..." button to be overlayed on a document
// on tap.
- (OpenInToolbar*)openInToolbar;
@end

// Bridge to deliver method calls from C++ to the |OpenInController| class.
class OpenInControllerBridge
    : public base::RefCountedThreadSafe<OpenInControllerBridge> {
 public:
  explicit OpenInControllerBridge(OpenInController* owner) : owner_(owner) {}

  BOOL CreateDestinationDirectoryAndRemoveObsoleteFiles(void) {
    return [OpenInController createDestinationDirectoryAndRemoveObsoleteFiles];
  }

  void OnDestinationDirectoryCreated(BOOL success) {
    DCHECK_CURRENTLY_ON(web::WebThread::UI);
    if (!success)
      [owner_ hideOpenInToolbar];
    else
      [owner_ startDownload];
  }

  void OnOwnerDisabled() {
    // When the owner is disabled:
    // - if there is a task in flight posted via |PostTaskAndReplyWithResult|
    // then dereferencing |bridge_| will not release it as |bridge_| is also
    // referenced by the task posting; setting |owner_| to nil makes sure that
    // no methods are called on it, and it works since |owner_| is only used on
    // the main thread.
    // - if there is a task in flight posted by the URLFetcher then
    // |OpenInController| destroys the fetcher and cancels the callback. This is
    // why |OnURLFetchComplete| will neved be called after |owner_| is disabled.
    owner_ = nil;
  }

 protected:
  friend base::RefCountedThreadSafe<OpenInControllerBridge>;
  virtual ~OpenInControllerBridge() {}

 private:
  __weak OpenInController* owner_;
};

@implementation OpenInController {
  // Bridge from C++ to Obj-C class.
  scoped_refptr<OpenInControllerBridge> _bridge;

  // URL of the document.
  GURL _documentURL;

  // Controller for opening documents in other applications.
  UIDocumentInteractionController* _documentController;

  // Toolbar overlay to be displayed on tap.
  OpenInToolbar* _openInToolbar;

  // Timer used to automatically hide the |openInToolbar_| after a period.
  NSTimer* _openInTimer;

  // Gesture recognizer to catch taps on the document.
  UITapGestureRecognizer* _tapRecognizer;

  // Suggested filename for the document.
  NSString* _suggestedFilename;

  // Loader used to redownload the document and save it in the sandbox.
  std::unique_ptr<network::SimpleURLLoader> _urlLoader;

  // WebState used to check if the tap is not on a link and the
  // |openInToolbar_| should be displayed.
  web::WebState* _webState;

  // URLLoaderFactory instance needed for URLLoader.
  scoped_refptr<network::SharedURLLoaderFactory> _urlLoaderFactory;

  // Spinner view displayed while the file is downloading.
  UIView* _overlayedView;

  // The location where the "Open in..." menu is anchored.
  CGRect _anchorLocation;

  // YES if the file download was canceled.
  BOOL _downloadCanceled;

  // YES if the OpenIn menu is displayed.
  BOOL _isOpenInMenuDisplayed;

  // YES if the toolbar is displayed.
  BOOL _isOpenInToolbarDisplayed;

  // Task runner on which file operations should happen.
  scoped_refptr<base::SequencedTaskRunner> _sequencedTaskRunner;
}

@synthesize baseView = _baseView;
@synthesize browser = _browser;
@synthesize previousScrollViewOffset = _previousScrollViewOffset;

- (id)initWithURLLoaderFactory:
          (scoped_refptr<network::SharedURLLoaderFactory>)urlLoaderFactory
                      webState:(web::WebState*)webState {
  self = [super init];
  if (self) {
    _urlLoaderFactory = std::move(urlLoaderFactory);
    _webState = webState;
    _tapRecognizer = [[UITapGestureRecognizer alloc]
        initWithTarget:self
                action:@selector(handleTapFrom:)];
    [_tapRecognizer setDelegate:self];
    _sequencedTaskRunner = base::ThreadPool::CreateSequencedTaskRunner(
        {base::MayBlock(), base::TaskPriority::BEST_EFFORT});
    _isOpenInMenuDisplayed = NO;
    _previousScrollViewOffset = 0;
  }
  return self;
}

- (void)enableWithDocumentURL:(const GURL&)documentURL
            suggestedFilename:(NSString*)suggestedFilename {
  _documentURL = GURL(documentURL);
  _suggestedFilename = suggestedFilename;
  [self.baseView addGestureRecognizer:_tapRecognizer];
  [self openInToolbar].alpha = 0.0f;
  [self.baseView addSubview:[self openInToolbar]];
  if (_webState)
    [[_webState->GetWebViewProxy() scrollViewProxy] addObserver:self];

  [self showOpenInToolbarWithTimer:NO];
}

- (void)disable {
  [self openInToolbar].alpha = 0.0f;
  [_openInTimer invalidate];
  if (_bridge.get())
    _bridge->OnOwnerDisabled();
  _bridge = nil;
  [self.baseView removeGestureRecognizer:_tapRecognizer];
  if (_webState)
    [[_webState->GetWebViewProxy() scrollViewProxy] removeObserver:self];
  self.previousScrollViewOffset = 0;
  [[self openInToolbar] removeFromSuperview];
  [_documentController dismissMenuAnimated:NO];
  [_documentController setDelegate:nil];
  _documentURL = GURL();
  _suggestedFilename = nil;
  _urlLoader.reset();
}

- (void)detachFromWebState {
  [self disable];
  // Animation blocks may be keeping this object alive; don't extend the
  // lifetime of WebState.
  _webState = nullptr;
}

- (void)dealloc {
  [self disable];
}

- (void)handleTapFrom:(UIGestureRecognizer*)gestureRecognizer {
  if ([gestureRecognizer state] == UIGestureRecognizerStateEnded) {
    if (_isOpenInToolbarDisplayed) {
      [self hideOpenInToolbar];
    } else {
      [self showOpenInToolbarWithTimer:YES];
    }
  }
}

- (void)showOpenInToolbarWithTimer:(BOOL)withTimer {
  if (withTimer) {
    if ([_openInTimer isValid]) {
      [_openInTimer setFireDate:([NSDate dateWithTimeIntervalSinceNow:
                                             kOpenInToolbarDisplayDuration])];
    } else {
      _openInTimer =
          [NSTimer scheduledTimerWithTimeInterval:kOpenInToolbarDisplayDuration
                                           target:self
                                         selector:@selector(hideOpenInToolbar)
                                         userInfo:nil
                                          repeats:NO];
    }
  } else {
    [_openInTimer invalidate];
  }

  OpenInToolbar* openInToolbar = [self openInToolbar];
  if (!_isOpenInToolbarDisplayed) {
    [UIView animateWithDuration:kOpenInToolbarAnimationDuration
                     animations:^{
                       [openInToolbar setAlpha:1.0];
                     }];
  }
  _isOpenInToolbarDisplayed = YES;
}

- (void)hideOpenInToolbar {
  if (!_openInToolbar)
    return;
  [_openInTimer invalidate];
  UIView* openInToolbar = [self openInToolbar];
  [UIView animateWithDuration:kOpenInToolbarAnimationDuration
                   animations:^{
                     [openInToolbar setAlpha:0.0];
                   }];
  _isOpenInToolbarDisplayed = NO;
}

- (void)exportFileWithOpenInMenuAnchoredAt:(UIView*)view {
  DCHECK([view isKindOfClass:[UIView class]]);
  DCHECK_CURRENTLY_ON(web::WebThread::UI);

  base::RecordAction(base::UserMetricsAction("IOS.OpenIn.Tapped"));

  if (!_webState)
    return;

  _anchorLocation = [[self openInToolbar] convertRect:view.frame
                                               toView:self.baseView];
  [_openInTimer invalidate];
  if (!_bridge.get())
    _bridge = new OpenInControllerBridge(self);

  // This needs to be done in two steps, on two separate threads. The
  // first task needs to be done on the worker pool and returns a BOOL which is
  // then used in the second function, |OnDestinationDirectoryCreated|, which
  // runs on the UI thread.
  base::OnceCallback<BOOL(void)> task = base::BindOnce(
      &OpenInControllerBridge::CreateDestinationDirectoryAndRemoveObsoleteFiles,
      _bridge);
  base::OnceCallback<void(BOOL)> reply = base::BindOnce(
      &OpenInControllerBridge::OnDestinationDirectoryCreated, _bridge);
  base::PostTaskAndReplyWithResult(_sequencedTaskRunner.get(), FROM_HERE,
                                   std::move(task), std::move(reply));
}

- (void)startDownload {
  NSString* tempDirPath = [NSTemporaryDirectory()
      stringByAppendingPathComponent:kDocumentsTempPath];
  NSString* filePath =
      [tempDirPath stringByAppendingPathComponent:_suggestedFilename];

  // In iPad the toolbar has to be displayed to anchor the "Open in" menu.
  if (!IsIPadIdiom())
    [self hideOpenInToolbar];

  // Show an overlayed view to indicate a download is in progress. On tap this
  // view can be dismissed and the download canceled.
  [self showDownloadOverlayView];
  _downloadCanceled = NO;

  // Ensure |bridge_| is set in case this function is called from a unittest.
  if (!_bridge.get())
    _bridge = new OpenInControllerBridge(self);

  // Download the document and save it at |filePath|.
  auto resourceRequest = std::make_unique<network::ResourceRequest>();
  resourceRequest->url = _documentURL;
  resourceRequest->load_flags = net::LOAD_SKIP_CACHE_VALIDATION;

  _urlLoader = network::SimpleURLLoader::Create(std::move(resourceRequest),
                                                NO_TRAFFIC_ANNOTATION_YET);
  _urlLoader->DownloadToFile(_urlLoaderFactory.get(),
                             base::BindOnce(^(base::FilePath filePath) {
                               [self urlLoadDidComplete:filePath];
                             }),
                             base::FilePath(base::SysNSStringToUTF8(filePath)));
}

- (void)handleTapOnOverlayedView:(UIGestureRecognizer*)gestureRecognizer {
  if ([gestureRecognizer state] != UIGestureRecognizerStateEnded)
    return;

  [self removeOverlayedView];
  if (IsIPadIdiom())
    [self hideOpenInToolbar];
  _downloadCanceled = YES;
}

- (void)removeOverlayedView {
  if (!_overlayedView)
    return;

  UIView* overlayedView = _overlayedView;
  [UIView animateWithDuration:kOverlayViewAnimationDuration
      animations:^{
        [overlayedView setAlpha:0.0];
      }
      completion:^(BOOL finished) {
        [overlayedView removeFromSuperview];
      }];
  _overlayedView = nil;
}

- (void)showErrorWithMessage:(NSString*)message {
  UIViewController* topViewController = [GetAnyKeyWindow() rootViewController];

  _alertCoordinator =
      [[AlertCoordinator alloc] initWithBaseViewController:topViewController
                                                   browser:_browser
                                                     title:nil
                                                   message:message];

  [_alertCoordinator addItemWithTitle:l10n_util::GetNSString(IDS_OK)
                               action:nil
                                style:UIAlertActionStyleDefault];

  [_alertCoordinator start];
}

- (void)presentOpenInMenuForFileAtURL:(NSURL*)fileURL {
  if (!_webState)
    return;

  _documentController =
      [UIDocumentInteractionController interactionControllerWithURL:fileURL];

  // TODO(cgrigoruta): The UTI is hardcoded for now, change this when we add
  // support for other file types as well.
  [_documentController setUTI:@"com.adobe.pdf"];
  [_documentController setDelegate:self];
  BOOL success = [_documentController presentOpenInMenuFromRect:_anchorLocation
                                                         inView:self.baseView
                                                       animated:YES];
  [self removeOverlayedView];
  if (!success) {
    if (IsIPadIdiom())
      [self hideOpenInToolbar];
    NSString* errorMessage =
        l10n_util::GetNSStringWithFixup(IDS_IOS_OPEN_IN_NO_APPS_REGISTERED);
    [self showErrorWithMessage:errorMessage];
  } else {
    _isOpenInMenuDisplayed = YES;
  }
}

- (void)showDownloadOverlayView {
  _overlayedView = [[UIView alloc] initWithFrame:[self.baseView bounds]];
  [_overlayedView setAutoresizingMask:(UIViewAutoresizingFlexibleWidth |
                                       UIViewAutoresizingFlexibleHeight)];
  UIView* grayBackgroundView =
      [[UIView alloc] initWithFrame:[_overlayedView frame]];
  [grayBackgroundView setBackgroundColor:[UIColor darkGrayColor]];
  [grayBackgroundView setAlpha:kOverlayedViewBackgroundAlpha];
  [grayBackgroundView setAutoresizingMask:(UIViewAutoresizingFlexibleWidth |
                                           UIViewAutoresizingFlexibleHeight)];
  [_overlayedView addSubview:grayBackgroundView];

  UIActivityIndicatorView* spinner = [[UIActivityIndicatorView alloc]
      initWithActivityIndicatorStyle:UIActivityIndicatorViewStyleWhiteLarge];
  [spinner setFrame:[_overlayedView frame]];
  [spinner setHidesWhenStopped:YES];
  [spinner setUserInteractionEnabled:NO];
  [spinner startAnimating];
  [spinner setAutoresizingMask:(UIViewAutoresizingFlexibleWidth |
                                UIViewAutoresizingFlexibleHeight)];
  [_overlayedView addSubview:spinner];

  UILabel* label = [[UILabel alloc] init];
  [label setTextColor:[UIColor whiteColor]];
  [label setFont:[UIFont preferredFontForTextStyle:UIFontTextStyleHeadline]];
  [label setNumberOfLines:0];
  [label setShadowColor:[UIColor blackColor]];
  [label setShadowOffset:CGSizeMake(0.0, 1.0)];
  [label setBackgroundColor:[UIColor clearColor]];
  [label setText:l10n_util::GetNSString(IDS_IOS_OPEN_IN_FILE_DOWNLOAD_CANCEL)];
  [label setLineBreakMode:NSLineBreakByWordWrapping];
  [label setTextAlignment:NSTextAlignmentCenter];
  CGFloat labelWidth =
      [_overlayedView frame].size.width * kOverlayedViewLabelWidthPercentage;
  CGFloat originX = ([_overlayedView frame].size.width - labelWidth) / 2;

  CGFloat labelHeight =
      [[label text] cr_boundingSizeWithSize:CGSizeMake(labelWidth, CGFLOAT_MAX)
                                       font:[label font]]
          .height;
  CGFloat originY =
      [_overlayedView center].y - labelHeight - kOverlayedViewLabelBottomMargin;
  [label setFrame:CGRectMake(originX, originY, labelWidth, labelHeight)];
  [_overlayedView addSubview:label];

  UITapGestureRecognizer* tapRecognizer = [[UITapGestureRecognizer alloc]
      initWithTarget:self
              action:@selector(handleTapOnOverlayedView:)];
  [tapRecognizer setDelegate:self];
  [_overlayedView addGestureRecognizer:tapRecognizer];
  [_overlayedView setAlpha:0.0];
  [self.baseView addSubview:_overlayedView];
  UIView* overlayedView = _overlayedView;
  [UIView animateWithDuration:kOverlayViewAnimationDuration
                   animations:^{
                     [overlayedView setAlpha:1.0];
                   }];
}

- (OpenInToolbar*)openInToolbar {
  if (!_openInToolbar) {
    _openInToolbar = [[OpenInToolbar alloc]
        initWithTarget:self
                action:@selector(exportFileWithOpenInMenuAnchoredAt:)];
  }
  return _openInToolbar;
}

#pragma mark -
#pragma mark File management

- (void)removeDocumentAtPath:(nullable NSString*)path {
  if (!path)
    return;
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::WILL_BLOCK);
  NSFileManager* fileManager = [NSFileManager defaultManager];
  NSError* error = nil;
  if (![fileManager removeItemAtPath:path error:&error]) {
    DLOG(ERROR) << "Failed to remove file: "
                << base::SysNSStringToUTF8([error description]);
  }
}

+ (void)removeAllStoredDocumentsAtPath:(NSString*)tempDirPath {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::WILL_BLOCK);
  NSFileManager* fileManager = [NSFileManager defaultManager];
  NSError* error = nil;
  NSArray* documentFiles = [fileManager contentsOfDirectoryAtPath:tempDirPath
                                                            error:&error];
  if (!documentFiles) {
    DLOG(ERROR) << "Failed to get content of directory at path: "
                << base::SysNSStringToUTF8([error description]);
    return;
  }

  for (NSString* filename in documentFiles) {
    NSString* filePath = [tempDirPath stringByAppendingPathComponent:filename];
    if (![fileManager removeItemAtPath:filePath error:&error]) {
      DLOG(ERROR) << "Failed to remove file: "
                  << base::SysNSStringToUTF8([error description]);
    }
  }
}

+ (BOOL)createDestinationDirectoryAndRemoveObsoleteFiles {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::WILL_BLOCK);
  NSString* tempDirPath = [NSTemporaryDirectory()
      stringByAppendingPathComponent:kDocumentsTempPath];
  NSFileManager* fileManager = [NSFileManager defaultManager];
  BOOL isDirectory;
  NSError* error = nil;
  if (![fileManager fileExistsAtPath:tempDirPath isDirectory:&isDirectory]) {
    BOOL created = [fileManager createDirectoryAtPath:tempDirPath
                          withIntermediateDirectories:YES
                                           attributes:nil
                                                error:&error];
    DCHECK(created);
    if (!created) {
      DLOG(ERROR) << "Error creating destination dir: "
                  << base::SysNSStringToUTF8([error description]);
      return NO;
    }
  } else {
    DCHECK(isDirectory);
    if (!isDirectory) {
      DLOG(ERROR) << "Destination Directory already exists and is a file.";
      return NO;
    }
    // Remove all documents that might be still on temporary storage.
    [self removeAllStoredDocumentsAtPath:(NSString*)tempDirPath];
  }
  return YES;
}

- (void)urlLoadDidComplete:(const base::FilePath&)filePath {
  NSURL* fileURL = nil;
  if (!filePath.empty())
    fileURL = [NSURL fileURLWithPath:base::SysUTF8ToNSString(filePath.value())];
  if (!_downloadCanceled && HasValidFileAtUrl(fileURL)) {
    LogOpenInDownloadResult(OpenInDownloadResult::kSucceeded);
    [self presentOpenInMenuForFileAtURL:fileURL];
    return;
  }
  _sequencedTaskRunner->PostTask(FROM_HERE, base::BindOnce(^{
                                   [self removeDocumentAtPath:fileURL.path];
                                 }));
  OpenInDownloadResult download_result = OpenInDownloadResult::kCanceled;
  if (!_downloadCanceled) {
    download_result = OpenInDownloadResult::kFailed;
    if (IsIPadIdiom())
      [self hideOpenInToolbar];
    [self removeOverlayedView];
    [self showErrorWithMessage:l10n_util::GetNSStringWithFixup(
                                   IDS_IOS_OPEN_IN_FILE_DOWNLOAD_FAILED)];
  }
  LogOpenInDownloadResult(download_result);
}

#pragma mark -
#pragma mark UIDocumentInteractionControllerDelegate Methods

- (void)documentInteractionController:(UIDocumentInteractionController*)contr
           didEndSendingToApplication:(NSString*)application {
  _sequencedTaskRunner->PostTask(FROM_HERE, base::BindOnce(^{
                                   [self
                                       removeDocumentAtPath:[[contr URL] path]];
                                 }));
  if (IsIPadIdiom()) {
    // Call the |documentInteractionControllerDidDismissOpenInMenu:| method
    // as this is not called on the iPad after the document has been opened
    // in another application.
    [self documentInteractionControllerDidDismissOpenInMenu:contr];
  }
}

- (void)documentInteractionControllerDidDismissOpenInMenu:
    (UIDocumentInteractionController*)controller {
  if (!IsIPadIdiom()) {
    _isOpenInMenuDisplayed = NO;
    // On the iPhone the |openInToolber_| is hidden already.
    return;
  }

  // On iPad this method is called whenever the device changes orientation,
  // even thought the OpenIn menu is not displayed. To distinguish the cases
  // when this method is called after the OpenIn menu is dismissed, we
  // check the BOOL |isOpenInMenuDisplayed|.
  if (_isOpenInMenuDisplayed) {
    _openInTimer =
        [NSTimer scheduledTimerWithTimeInterval:kOpenInToolbarDisplayDuration
                                         target:self
                                       selector:@selector(hideOpenInToolbar)
                                       userInfo:nil
                                        repeats:NO];
  }
  _isOpenInMenuDisplayed = NO;
}

#pragma mark - UIGestureRecognizerDelegate Methods

- (BOOL)gestureRecognizer:(UIGestureRecognizer*)gestureRecognizer
    shouldRecognizeSimultaneouslyWithGestureRecognizer:
        (UIGestureRecognizer*)otherGestureRecognizer {
  return YES;
}

- (BOOL)gestureRecognizerShouldBegin:(UIGestureRecognizer*)gestureRecognizer {
  if ([gestureRecognizer.view isEqual:_overlayedView])
    return YES;

  CGPoint location = [gestureRecognizer locationInView:[self openInToolbar]];
  return ![[self openInToolbar] pointInside:location withEvent:nil];
}

#pragma mark - CRWWebViewScrollViewProxyObserver

- (void)webViewScrollViewDidScroll:
    (CRWWebViewScrollViewProxy*)webViewScrollViewProxy {
  // Store the values.
  CGFloat previousScrollOffset = self.previousScrollViewOffset;
  CGFloat currentScrollOffset = webViewScrollViewProxy.contentOffset.y;
  self.previousScrollViewOffset = currentScrollOffset;

  if (previousScrollOffset - currentScrollOffset > 0) {
    if (!_isOpenInToolbarDisplayed ||
        (_isOpenInToolbarDisplayed && [_openInTimer isValid])) {
      // Shows the OpenInToolbar only if it isn't displayed, or if it is
      // displayed with a timer to have the timer reset.
      [self showOpenInToolbarWithTimer:YES];
    }
  } else if (webViewScrollViewProxy.dragging) {
    [self hideOpenInToolbar];
  }
}

#pragma mark - TestingAditions

- (NSString*)suggestedFilename {
  return _suggestedFilename;
}

@end
