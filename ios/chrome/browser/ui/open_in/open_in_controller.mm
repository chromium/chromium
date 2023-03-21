// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/open_in/open_in_controller.h"

#import <QuickLook/QuickLook.h>

#import "base/files/file_path.h"
#import "base/functional/bind.h"
#import "base/location.h"
#import "base/logging.h"
#import "base/mac/scoped_cftyperef.h"
#import "base/metrics/histogram_macros.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/sequence_checker.h"
#import "base/strings/sys_string_conversions.h"
#import "base/task/sequenced_task_runner.h"
#import "base/task/thread_pool.h"
#import "base/threading/scoped_blocking_call.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/shared/ui/util/layout_guide_names.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/shared/ui/util/util_swift.h"
#import "ios/chrome/browser/ui/alert_coordinator/alert_coordinator.h"
#import "ios/chrome/browser/ui/main/layout_guide_util.h"
#import "ios/chrome/browser/ui/open_in/features.h"
#import "ios/chrome/browser/ui/open_in/open_in_activity_delegate.h"
#import "ios/chrome/browser/ui/open_in/open_in_activity_view_controller.h"
#import "ios/chrome/browser/ui/open_in/open_in_controller_testing.h"
#import "ios/chrome/browser/ui/open_in/open_in_histograms.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/download/crw_web_view_download.h"
#import "ios/web/public/ui/crw_web_view_proxy.h"
#import "ios/web/public/ui/crw_web_view_scroll_view_proxy.h"
#import "ios/web/public/web_state.h"
#import "net/base/load_flags.h"
#import "services/network/public/cpp/resource_request.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"
#import "services/network/public/cpp/simple_url_loader.h"
#import "ui/base/device_form_factor.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "ui/gfx/ios/NSString+CrStringDrawing.h"
#import "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// The path in the temp directory containing documents that are to be opened in
// other applications.
static NSString* const kDocumentsTemporaryPath = @"OpenIn";

// Duration of the show/hide animation for the `openInToolbar_`.
const NSTimeInterval kOpenInToolbarAnimationDuration = 0.2;

// Duration to show or hide the `overlayedView_`.
const NSTimeInterval kOverlayViewAnimationDuration = 0.3;

// Time interval after which the `openInToolbar_` is automatically hidden.
const NSTimeInterval kOpenInToolbarDisplayDuration = 2.0;

// Alpha value for the background view of `overlayedView_`.
const CGFloat kOverlayedViewBackgroundAlpha = 0.6;

// Width of the label displayed on the `overlayedView_` as a percentage of the
// `overlayedView_`'s width.
const CGFloat kOverlayedViewLabelWidthPercentage = 0.7;

// Bottom margin for the label displayed on the `overlayedView_`.
const CGFloat kOverlayedViewLabelBottomMargin = 60;

// Logs the result of the download process after the user taps "open in" button.
void LogOpenInDownloadResult(const OpenInDownloadResult result) {
  UMA_HISTOGRAM_ENUMERATION(kOpenInDownloadHistogram, result);
}

// Returns true if the file located at `url` can be previewed.
bool HasValidFileAtUrl(NSURL* url) {
  if (!url)
    return false;

  if (![[NSFileManager defaultManager] isReadableFileAtPath:url.path])
    return false;

  NSString* extension = [url.path pathExtension];
  if ([extension isEqualToString:@"pdf"]) {
    base::ScopedCFTypeRef<CGPDFDocumentRef> document(
        CGPDFDocumentCreateWithURL((__bridge CFURLRef)url));
    return document;
  }

  return [QLPreviewController canPreviewItem:url];
}

// Returns the temporary path where documents are stored.
NSString* GetTemporaryDocumentDirectory() {
  return [NSTemporaryDirectory()
      stringByAppendingPathComponent:kDocumentsTemporaryPath];
}

// Removes the file at `file_url`.
void RemoveDocumentAtPath(NSURL* file_url) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::WILL_BLOCK);

  if (!file_url.path)
    return;

  NSError* error = nil;
  if (![[NSFileManager defaultManager] removeItemAtPath:file_url.path
                                                  error:&error]) {
    DLOG(ERROR) << "Failed to remove file: "
                << base::SysNSStringToUTF8([error description]);
  }
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
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::WILL_BLOCK);

  NSString* temporary_directory_path = GetTemporaryDocumentDirectory();
  NSFileManager* file_manager = [NSFileManager defaultManager];

  NSError* error = nil;
  BOOL is_directory = NO;
  if (![file_manager fileExistsAtPath:temporary_directory_path
                          isDirectory:&is_directory]) {
    BOOL created = [file_manager createDirectoryAtPath:temporary_directory_path
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
    if (!is_directory) {
      DLOG(ERROR) << "Destination Directory already exists and is a file.";
      return NO;
    }
    // Remove all documents that might be still on temporary storage.
    RemoveAllStoredDocumentsAtPath(temporary_directory_path);
  }
  return YES;
}

}  // anonymous namespace

@interface OpenInController () <CRWWebViewScrollViewProxyObserver,
                                CRWWebViewDownloadDelegate,
                                OpenInActivityDelegate> {
  // AlertCoordinator for showing an alert if no applications were found to open
  // the current document.
  AlertCoordinator* _alertCoordinator;
}

// Property storing the Y content offset the scroll view the last time it was
// updated. Used to know in which direction the scroll view is scrolling.
@property(nonatomic, assign) CGFloat previousScrollViewOffset;

// The base view controller from which to present UI.
@property(nonatomic, weak) UIViewController* baseViewController;

// Task runner on which file operations should happen.
@property(nonatomic, assign) scoped_refptr<base::SequencedTaskRunner>
    sequencedTaskRunner;

// Path where the downloaded file is saved.
@property(nonatomic, strong) NSString* filePath;

// CRWWebViewDownload instance that handle download interactions.
@property(nonatomic, strong) id<CRWWebViewDownload> download;

// SimpleURLLoader completion callback, when `urlLoader_` completes a request.
- (void)urlLoadDidComplete:(const base::FilePath&)file_path;
// Starts downloading the file at path `kDocumentsTemporaryPath` with the name
// `suggestedFilename_`.
- (void)startDownload;
// Shows the overlayed toolbar `openInToolbar_`. If `withTimer` is YES, it would
// be hidden after a certain amount of time.
- (void)showOpenInToolbarWithTimer:(BOOL)withTimer;
// Hides the overlayed toolbar `openInToolbar_`.
- (void)hideOpenInToolbar;
// Called when there is a tap on the `webState_`'s view to display the
// overlayed toolbar `openInToolbar_` if necessary and (re)schedule the
// `openInTimer_`.
- (void)handleTapFrom:(UIGestureRecognizer*)gestureRecognizer;
// Downloads the file at `documentURL_` and presents the OpenIn menu for opening
// it in other applications.
- (void)exportFileWithOpenInMenuAnchoredAt:(id)sender;
// Called when there is a tap on the `overlayedView_` to cancel the file
// download.
- (void)handleTapOnOverlayedView:(UIGestureRecognizer*)gestureRecognizer;
// Removes `overlayedView_` from the top view of the application.
- (void)removeOverlayedView;
// Shows an alert with the given error message.
- (void)showErrorWithMessage:(NSString*)message;
// Presents the OpenIn menu for the file at `fileURL`.
- (void)presentOpenInMenuForFileAtURL:(NSURL*)fileURL;
// Shows an overlayed spinner on the top view to indicate that a file download
// is in progress.
- (void)showDownloadOverlayView;
// Returns a toolbar with an "Open in..." button to be overlayed on a document
// on tap.
- (OpenInToolbar*)openInToolbar;
@end

@implementation OpenInController {
  // To check that callbacks are executed on the correct sequence.
  SEQUENCE_CHECKER(_sequenceChecker);

  // URL of the document.
  GURL _documentURL;

  // Controller for opening documents in other applications.
  OpenInActivityViewController* _activityViewController;

  // Toolbar overlay to be displayed on tap.
  OpenInToolbar* _openInToolbar;

  // Timer used to automatically hide the `openInToolbar_` after a period.
  NSTimer* _openInTimer;

  // Gesture recognizer to catch taps on the document.
  UITapGestureRecognizer* _tapRecognizer;

  // Layout guide to position the toolbar.
  UILayoutGuide* _layoutGuide;

  // Suggested filename for the document.
  NSString* _suggestedFilename;

  // Loader used to redownload the document and save it in the sandbox.
  std::unique_ptr<network::SimpleURLLoader> _urlLoader;

  // WebState used to check if the tap is not on a link and the
  // `openInToolbar_` should be displayed.
  web::WebState* _webState;

  // Browser used to display errors.
  Browser* _browser;

  // URLLoaderFactory instance needed for URLLoader.
  scoped_refptr<network::SharedURLLoaderFactory> _urlLoaderFactory;

  // Spinner view displayed while the file is downloading.
  UIView* _overlayedView;

  // The location where the "Open in..." menu is anchored.
  CGRect _anchorLocation;

  // YES if the file download was canceled.
  BOOL _downloadCanceled;

  // YES if the toolbar is displayed.
  BOOL _isOpenInToolbarDisplayed;

  // YES if the workflow has been canceled.
  BOOL _disabled;
}

@synthesize baseView = _baseView;
@synthesize previousScrollViewOffset = _previousScrollViewOffset;

- (instancetype)initWithBaseViewController:(UIViewController*)baseViewController
                          URLLoaderFactory:
                              (scoped_refptr<network::SharedURLLoaderFactory>)
                                  urlLoaderFactory
                                  webState:(web::WebState*)webState
                                   browser:(Browser*)browser {
  self = [super init];
  if (self) {
    _baseViewController = baseViewController;
    _urlLoaderFactory = std::move(urlLoaderFactory);
    _webState = webState;
    _tapRecognizer = [[UITapGestureRecognizer alloc]
        initWithTarget:self
                action:@selector(handleTapFrom:)];
    [_tapRecognizer setDelegate:self];
    LayoutGuideCenter* layoutGuideCenter = LayoutGuideCenterForBrowser(browser);
    _layoutGuide =
        [layoutGuideCenter makeLayoutGuideNamed:kSecondaryToolbarGuide];

    _sequencedTaskRunner = base::ThreadPool::CreateSequencedTaskRunner(
        {base::MayBlock(), base::TaskPriority::BEST_EFFORT});
    _previousScrollViewOffset = 0;
    _browser = browser;
  }
  return self;
}

- (void)enableWithDocumentURL:(const GURL&)documentURL
            suggestedFilename:(NSString*)suggestedFilename {
  _disabled = NO;
  _documentURL = GURL(documentURL);
  _suggestedFilename = suggestedFilename;

  if (self.baseView) {
    [self.baseView addGestureRecognizer:_tapRecognizer];
    self.openInToolbar.alpha = 0.0f;
    self.openInToolbar.translatesAutoresizingMaskIntoConstraints = NO;
    [self.baseView addSubview:self.openInToolbar];
    [self.baseView addLayoutGuide:_layoutGuide];
    [NSLayoutConstraint activateConstraints:@[
      [self.openInToolbar.leadingAnchor
          constraintEqualToAnchor:self.baseView.leadingAnchor],
      [self.openInToolbar.trailingAnchor
          constraintEqualToAnchor:self.baseView.trailingAnchor],
      [self.openInToolbar.bottomAnchor
          constraintEqualToAnchor:_layoutGuide.topAnchor],
    ]];
  }

  if (_webState)
    [[_webState->GetWebViewProxy() scrollViewProxy] addObserver:self];

  [self showOpenInToolbarWithTimer:NO];
}

- (void)disable {
  _disabled = YES;
  [self removeOverlayedView];
  self.openInToolbar.alpha = 0.0f;
  [_openInTimer invalidate];
  [self.baseView removeGestureRecognizer:_tapRecognizer];
  [self.baseView removeLayoutGuide:_layoutGuide];
  if (_webState)
    [[_webState->GetWebViewProxy() scrollViewProxy] removeObserver:self];
  self.previousScrollViewOffset = 0;
  [self.openInToolbar removeFromSuperview];
  _documentURL = GURL();
  _suggestedFilename = nil;
  _urlLoader.reset();
}

- (void)dismissModalView {
  [_activityViewController dismissViewControllerAnimated:NO completion:nil];
}

- (void)detachFromWebState {
  [self disable];
  // Animation blocks may be keeping this object alive; don't keep a
  // potentially dangling pointer to WebState and Browser.
  _webState = nullptr;
  _browser = nullptr;
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

  OpenInToolbar* openInToolbar = self.openInToolbar;
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
  UIView* openInToolbar = self.openInToolbar;
  [UIView animateWithDuration:kOpenInToolbarAnimationDuration
                   animations:^{
                     [openInToolbar setAlpha:0.0];
                   }];
  _isOpenInToolbarDisplayed = NO;
}

- (void)exportFileWithOpenInMenuAnchoredAt:(UIView*)view {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  DCHECK([view isKindOfClass:[UIView class]]);

  base::RecordAction(base::UserMetricsAction("IOS.OpenIn.Tapped"));

  if (!_webState)
    return;

  _anchorLocation = [self.openInToolbar convertRect:view.frame
                                             toView:self.baseView];
  [_openInTimer invalidate];

  // Creating the directory can block the main thread, so perform it on a
  // background sequence, then on current sequence complete the workflow.
  __weak OpenInController* weakSelf = self;
  _sequencedTaskRunner->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&CreateDestinationDirectoryAndRemoveObsoleteFiles),
      base::BindOnce(^(BOOL directoryCreated) {
        [weakSelf onDestinationDirectoryCreated:directoryCreated];
      }));
}

- (void)onDestinationDirectoryCreated:(BOOL)directoryCreated {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if (_disabled)
    return;

  if (!directoryCreated) {
    [self hideOpenInToolbar];
  } else {
    [self startDownload];
  }
}

- (void)startDownload {
  NSString* tempDirPath = GetTemporaryDocumentDirectory();
  self.filePath =
      [tempDirPath stringByAppendingPathComponent:_suggestedFilename];

  // In iPad the toolbar has to be displayed to anchor the "Open in" menu.
  if (ui::GetDeviceFormFactor() != ui::DEVICE_FORM_FACTOR_TABLET)
    [self hideOpenInToolbar];

  // Show an overlayed view to indicate a download is in progress. On tap this
  // view can be dismissed and the download canceled.
  [self showDownloadOverlayView];
  _downloadCanceled = NO;

  if (@available(iOS 14.5, *)) {
    if (IsOpenInNewDownloadEnabled()) {
      __weak OpenInController* weakSelf = self;
      _webState->DownloadCurrentPage(self.filePath, self,
                                     ^(id<CRWWebViewDownload> download) {
                                       weakSelf.download = download;
                                     });
      return;
    }
  }

  // Download the document and save it at `self.filePath`.
  // TODO(crbug.com/1357553): Remove when Open In download experiment is
  // finished.
  auto resourceRequest = std::make_unique<network::ResourceRequest>();
  resourceRequest->url = _documentURL;
  resourceRequest->load_flags = net::LOAD_SKIP_CACHE_VALIDATION;

  _urlLoader = network::SimpleURLLoader::Create(std::move(resourceRequest),
                                                NO_TRAFFIC_ANNOTATION_YET);
  _urlLoader->DownloadToFile(
      _urlLoaderFactory.get(), base::BindOnce(^(base::FilePath filePath) {
        [self urlLoadDidComplete:filePath];
      }),
      base::FilePath(base::SysNSStringToUTF8(self.filePath)));
}

- (void)handleTapOnOverlayedView:(UIGestureRecognizer*)gestureRecognizer {
  if ([gestureRecognizer state] != UIGestureRecognizerStateEnded)
    return;

  if (@available(iOS 14.5, *)) {
    if (IsOpenInDownloadWithWKDownload()) {
      [self.download cancelDownload];
    }
  }

  [self removeOverlayedView];
  if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET)
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

  _activityViewController =
      [[OpenInActivityViewController alloc] initWithURL:fileURL];
  _activityViewController.delegate = self;

  // UIActivityViewController is presented in a popover on iPad.
  _activityViewController.popoverPresentationController.sourceView =
      self.baseView;
  _activityViewController.popoverPresentationController.sourceRect =
      _anchorLocation;

  [self removeOverlayedView];
  [self.baseViewController presentViewController:_activityViewController
                                        animated:YES
                                      completion:nil];
}

- (void)completedPresentOpenInMenuForFileAtURL:(NSURL*)fileURL {
  _sequencedTaskRunner->PostTask(FROM_HERE, base::BindOnce(^{
                                   RemoveDocumentAtPath(fileURL);
                                 }));

  if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET) {
    _openInTimer =
        [NSTimer scheduledTimerWithTimeInterval:kOpenInToolbarDisplayDuration
                                         target:self
                                       selector:@selector(hideOpenInToolbar)
                                       userInfo:nil
                                        repeats:NO];
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

  UIActivityIndicatorView* spinner = GetLargeUIActivityIndicatorView();
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

#pragma mark - OpenInActivityDelegate

- (void)openInActivityWillDisappearForFileAtURL:(NSURL*)fileURL {
  [self completedPresentOpenInMenuForFileAtURL:fileURL];
}

#pragma mark File management

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
                                   RemoveDocumentAtPath(fileURL);
                                 }));
  OpenInDownloadResult download_result = OpenInDownloadResult::kCanceled;
  if (!_downloadCanceled) {
    download_result = OpenInDownloadResult::kFailed;
    if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET)
      [self hideOpenInToolbar];
    [self removeOverlayedView];
    [self showErrorWithMessage:l10n_util::GetNSStringWithFixup(
                                   IDS_IOS_OPEN_IN_FILE_DOWNLOAD_FAILED)];
  }
  LogOpenInDownloadResult(download_result);
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

  CGPoint location = [gestureRecognizer locationInView:self.openInToolbar];
  return ![self.openInToolbar pointInside:location withEvent:nil];
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

#pragma mark - CRWWebViewDownloadDelegate

- (void)downloadDidFinish {
  [self urlLoadDidComplete:base::FilePath(
                               base::SysNSStringToUTF8(self.filePath))];
}

- (void)downloadDidFailWithError:(NSError*)error {
  [self urlLoadDidComplete:base::FilePath(
                               base::SysNSStringToUTF8(self.filePath))];
}

@end
