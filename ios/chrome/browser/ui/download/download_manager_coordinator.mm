// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/download/download_manager_coordinator.h"

#import <MobileCoreServices/MobileCoreServices.h>
#import <StoreKit/StoreKit.h>

#import <memory>
#import <set>
#import <utility>

#import "base/apple/scoped_cftyperef.h"
#import "base/check_op.h"
#import "base/functional/bind.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/download/confirm_download_closing_overlay.h"
#import "ios/chrome/browser/download/confirm_download_replacing_overlay.h"
#import "ios/chrome/browser/download/download_directory_util.h"
#import "ios/chrome/browser/download/download_manager_metric_names.h"
#import "ios/chrome/browser/download/download_manager_tab_helper.h"
#import "ios/chrome/browser/download/external_app_util.h"
#import "ios/chrome/browser/download/installation_notifier.h"
#import "ios/chrome/browser/overlays/public/common/confirmation/confirmation_overlay_response.h"
#import "ios/chrome/browser/overlays/public/overlay_callback_manager.h"
#import "ios/chrome/browser/overlays/public/overlay_request_queue.h"
#import "ios/chrome/browser/shared/coordinator/layout_guide/layout_guide_util.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/store_kit/model/store_kit_coordinator.h"
#import "ios/chrome/browser/store_kit/model/store_kit_coordinator_delegate.h"
#import "ios/chrome/browser/ui/download/activities/open_downloads_folder_activity.h"
#import "ios/chrome/browser/ui/download/download_manager_mediator.h"
#import "ios/chrome/browser/ui/download/download_manager_view_controller.h"
#import "ios/chrome/browser/ui/download/unopened_downloads_tracker.h"
#import "ios/chrome/browser/ui/presenters/contained_presenter.h"
#import "ios/chrome/browser/ui/presenters/contained_presenter_delegate.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/common/features.h"
#import "ios/web/public/download/download_task.h"
#import "ios/web/public/web_client.h"
#import "net/base/net_errors.h"
#import "ui/base/l10n/l10n_util_mac.h"

@interface DownloadManagerCoordinator () <ContainedPresenterDelegate,
                                          DownloadManagerViewControllerDelegate,
                                          StoreKitCoordinatorDelegate> {
  // View controller for presenting Download Manager UI.
  DownloadManagerViewController* _viewController;
  // View controller for presenting "Open In.." dialog.
  UIActivityViewController* _openInController;
  DownloadManagerMediator _mediator;
  StoreKitCoordinator* _storeKitCoordinator;
  UnopenedDownloadsTracker _unopenedDownloads;
  // YES after _stop has been called.
  BOOL _stopped;
}
@end

@implementation DownloadManagerCoordinator

- (void)dealloc {
  DCHECK(_stopped);
}

- (void)start {
  DCHECK(self.presenter);
  DCHECK(self.browser);

  NSNotificationCenter* defaultCenter = [NSNotificationCenter defaultCenter];
  [defaultCenter addObserver:self
                    selector:@selector(applicationDidEnterBackground:)
                        name:UIApplicationDidEnterBackgroundNotification
                      object:nil];

  BOOL isIncognito = self.browser->GetBrowserState()->IsOffTheRecord();
  _viewController = [[DownloadManagerViewController alloc] init];
  _viewController.delegate = self;
  _viewController.layoutGuideCenter = LayoutGuideCenterForBrowser(self.browser);
  _viewController.incognito = isIncognito;
  _mediator.SetDownloadTask(_downloadTask);
  _mediator.SetConsumer(_viewController);

  self.presenter.baseViewController = self.baseViewController;
  self.presenter.presentedViewController = _viewController;
  self.presenter.delegate = self;

  self.browser->GetWebStateList()->AddObserver(&_unopenedDownloads);

  [self.presenter prepareForPresentation];

  [self.presenter presentAnimated:self.animatesPresentation];
}

- (void)stop {
  if (_viewController) {
    [self.presenter dismissAnimated:self.animatesPresentation];
    // Prevent delegate callbacks for stopped coordinator.
    _viewController.delegate = nil;
    _viewController = nil;
  }

  _downloadTask = nullptr;

  if (self.browser)
    (self.browser->GetWebStateList())->RemoveObserver(&_unopenedDownloads);

  [self stopStoreKitCoordinator];

  [[InstallationNotifier sharedInstance] unregisterForNotifications:self];
  _stopped = YES;
}

- (UIViewController*)viewController {
  return _viewController;
}

#pragma mark - DownloadManagerTabHelperDelegate

- (void)downloadManagerTabHelper:(nonnull DownloadManagerTabHelper*)tabHelper
               didCreateDownload:(nonnull web::DownloadTask*)download
               webStateIsVisible:(BOOL)webStateIsVisible {
  base::UmaHistogramEnumeration("Download.IOSDownloadFileUI",
                                DownloadFileUI::DownloadFileStarted,
                                DownloadFileUI::Count);

  if (!webStateIsVisible) {
    // Do nothing if a background Tab requested download UI presentation.
    return;
  }

  BOOL replacingExistingDownload = _downloadTask ? YES : NO;
  _downloadTask = download;

  if (web::GetWebClient()->EnableFullscreenAPI()) {
    // Exit fullscreen since download UI will be behind fullscreen mode.
    web::WebState* webState = download->GetWebState();
    webState->CloseMediaPresentations();
  }

  if (replacingExistingDownload) {
    _mediator.SetDownloadTask(_downloadTask);
  } else {
    self.animatesPresentation = YES;
    [self start];
  }
}

- (void)downloadManagerTabHelper:(nonnull DownloadManagerTabHelper*)tabHelper
         decidePolicyForDownload:(nonnull web::DownloadTask*)download
               completionHandler:(nonnull void (^)(NewDownloadPolicy))handler {
  std::unique_ptr<OverlayRequest> request =
      OverlayRequest::CreateWithConfig<ConfirmDownloadReplacingRequest>();

  request->GetCallbackManager()->AddCompletionCallback(
      base::BindOnce(^(OverlayResponse* response) {
        // `response` is null if WebState was destroyed. Don't call completion
        // handler if no buttons were tapped.
        if (response) {
          bool confirmed =
              response->GetInfo<ConfirmationOverlayResponse>()->confirmed();
          base::UmaHistogramBoolean("Download.IOSDownloadReplaced", confirmed);
          handler(confirmed ? kNewDownloadPolicyReplace
                            : kNewDownloadPolicyDiscard);
        }
      }));

  web::WebState* webState = download->GetWebState();
  if (web::GetWebClient()->EnableFullscreenAPI()) {
    // Close fullscreen mode in the event that a download is attempting to
    // replace a pending download request.
    webState->CloseMediaPresentations();
  }

  OverlayRequestQueue::FromWebState(webState, OverlayModality::kWebContentArea)
      ->AddRequest(std::move(request));
}

- (void)downloadManagerTabHelper:(nonnull DownloadManagerTabHelper*)tabHelper
                 didHideDownload:(nonnull web::DownloadTask*)download {
  DCHECK_EQ(_downloadTask, download);
  self.animatesPresentation = NO;
  [self stop];
  self.animatesPresentation = YES;
}

- (void)downloadManagerTabHelper:(nonnull DownloadManagerTabHelper*)tabHelper
                 didShowDownload:(nonnull web::DownloadTask*)download {
  DCHECK_NE(_downloadTask, download);
  _downloadTask = download;
  self.animatesPresentation = NO;
  [self start];
  self.animatesPresentation = YES;
}

#pragma mark - ContainedPresenterDelegate

- (void)containedPresenterDidPresent:(id<ContainedPresenter>)presenter {
  DCHECK(presenter == self.presenter);
}

- (void)containedPresenterDidDismiss:(id<ContainedPresenter>)presenter {
  DCHECK(presenter == self.presenter);
}

#pragma mark - DownloadManagerViewControllerDelegate

- (void)downloadManagerViewControllerDidClose:
    (DownloadManagerViewController*)controller {
  if (_downloadTask->GetState() != web::DownloadTask::State::kInProgress) {
    base::UmaHistogramEnumeration("Download.IOSDownloadFileResult",
                                  DownloadFileResult::NotStarted,
                                  DownloadFileResult::Count);
    base::RecordAction(base::UserMetricsAction("IOSDownloadClose"));
    [self cancelDownload];
    return;
  }
  base::RecordAction(
      base::UserMetricsAction("IOSDownloadTryCloseWhenInProgress"));

  std::unique_ptr<OverlayRequest> request =
      OverlayRequest::CreateWithConfig<ConfirmDownloadClosingRequest>();

  __weak DownloadManagerCoordinator* weakSelf = self;
  request->GetCallbackManager()->AddCompletionCallback(
      base::BindOnce(^(OverlayResponse* response) {
        if (response &&
            response->GetInfo<ConfirmationOverlayResponse>()->confirmed()) {
          base::UmaHistogramEnumeration("Download.IOSDownloadFileResult",
                                        DownloadFileResult::Cancelled,
                                        DownloadFileResult::Count);
          [weakSelf cancelDownload];
        }
      }));

  web::WebState* webState = self.downloadTask->GetWebState();
  OverlayRequestQueue::FromWebState(webState, OverlayModality::kWebContentArea)
      ->AddRequest(std::move(request));
}

- (void)installDriveForDownloadManagerViewController:
    (DownloadManagerViewController*)controller {
  base::RecordAction(base::UserMetricsAction("IOSDownloadInstallGoogleDrive"));
  [self presentStoreKitForGoogleDriveApp];
}

- (void)downloadManagerViewControllerDidStartDownload:
    (DownloadManagerViewController*)controller {
  if (_downloadTask->GetErrorCode() != net::OK) {
    base::RecordAction(base::UserMetricsAction("MobileDownloadRetryDownload"));
  } else {
    base::RecordAction(base::UserMetricsAction("IOSDownloadStartDownload"));
    _unopenedDownloads.Add(_downloadTask);
  }
  _mediator.StartDowloading();
}

- (void)presentOpenInForDownloadManagerViewController:
    (DownloadManagerViewController*)controller {
  base::RecordAction(base::UserMetricsAction("IOSDownloadOpenIn"));
  base::FilePath path = _mediator.GetDownloadPath();
  NSURL* URL = [NSURL fileURLWithPath:base::SysUTF8ToNSString(path.value())];

  NSArray* customActions = @[ URL ];
  NSArray* activities = nil;

  OpenDownloadsFolderActivity* customActivity =
      [[OpenDownloadsFolderActivity alloc] init];
  customActivity.browserHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), BrowserCoordinatorCommands);
  activities = @[ customActivity ];

  _openInController =
      [[UIActivityViewController alloc] initWithActivityItems:customActions
                                        applicationActivities:activities];

  _openInController.excludedActivityTypes =
      @[ UIActivityTypeCopyToPasteboard, UIActivityTypeSaveToCameraRoll ];

  // UIActivityViewController is presented in a popover on iPad.
  _openInController.popoverPresentationController.sourceView =
      _viewController.actionButton;
  _openInController.popoverPresentationController.sourceRect =
      _viewController.actionButton.bounds;
  [_viewController presentViewController:_openInController
                                animated:YES
                              completion:nil];
}

#pragma mark - Private

- (void)stopStoreKitCoordinator {
  [_storeKitCoordinator stop];
  _storeKitCoordinator.delegate = nil;
  _storeKitCoordinator = nil;
}

// Cancels the download task and stops the coordinator.
- (void)cancelDownload {
  // `stop` nulls-our _downloadTask and `Cancel` destroys the task. Call `stop`
  // first to perform all coordinator cleanups, but copy `_downloadTask`
  // pointer to destroy the task.
  web::DownloadTask* downloadTask = _downloadTask;
  [self stop];

  // The pointer may be null if -stop was called before -cancelDownload.
  // This can happen during shutdown because -stop is called when the UI
  // is destroyed, but whether or not -cancelDownload is called depends
  // on whether the object is deallocated or not when the block created
  // in -downloadManagerViewControllerDidClose: is executed. Due to the
  // autorelease pool, it is not possible to control how the order of
  // those two events. Thus, this code needs to support a null value at
  // this point.
  if (downloadTask) {
    downloadTask->Cancel();
  }
}

// Called when Google Drive app is installed after starting StoreKitCoordinator.
- (void)didInstallGoogleDriveApp {
  base::UmaHistogramEnumeration(
      "Download.IOSDownloadFileUIGoogleDrive",
      DownloadFileUIGoogleDrive::GoogleDriveInstalledAfterDisplay,
      DownloadFileUIGoogleDrive::Count);
}

// Presents StoreKit dialog for Google Drive application.
- (void)presentStoreKitForGoogleDriveApp {
  if (!_storeKitCoordinator) {
    _storeKitCoordinator = [[StoreKitCoordinator alloc]
        initWithBaseViewController:self.baseViewController
                           browser:self.browser];
    _storeKitCoordinator.delegate = self;
    _storeKitCoordinator.iTunesProductParameters = @{
      SKStoreProductParameterITunesItemIdentifier :
          kGoogleDriveITunesItemIdentifier
    };
  }
  [_storeKitCoordinator start];
  [_viewController setInstallDriveButtonVisible:NO animated:YES];

  [[InstallationNotifier sharedInstance]
      registerForInstallationNotifications:self
                              withSelector:@selector(didInstallGoogleDriveApp)
                                 forScheme:kGoogleDriveAppURLScheme];
}

#pragma mark - Notification callback

- (void)applicationDidEnterBackground:(NSNotification*)note {
  [_openInController.presentingViewController
      dismissViewControllerAnimated:YES
                         completion:nil];
}

#pragma mark - StoreKitCoordinatorDelegate

- (void)storeKitCoordinatorWantsToStop:(StoreKitCoordinator*)coordinator {
  CHECK_EQ(coordinator, _storeKitCoordinator);
  [self stopStoreKitCoordinator];
}

@end
