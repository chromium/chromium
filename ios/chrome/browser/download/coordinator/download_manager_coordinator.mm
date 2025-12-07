// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/download/coordinator/download_manager_coordinator.h"

#import <MobileCoreServices/MobileCoreServices.h>
#import <StoreKit/StoreKit.h>

#import <memory>
#import <set>
#import <utility>

#import "base/apple/foundation_util.h"
#import "base/apple/scoped_cftyperef.h"
#import "base/check_op.h"
#import "base/feature_list.h"
#import "base/functional/bind.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/strings/stringprintf.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "components/prefs/pref_service.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/download/coordinator/activities/open_downloads_folder_activity.h"
#import "ios/chrome/browser/download/coordinator/download_manager_mediator.h"
#import "ios/chrome/browser/download/model/confirm_download_closing_overlay.h"
#import "ios/chrome/browser/download/model/confirm_download_replacing_overlay.h"
#import "ios/chrome/browser/download/model/download_directory_util.h"
#import "ios/chrome/browser/download/model/download_manager_metric_names.h"
#import "ios/chrome/browser/download/model/download_manager_tab_helper.h"
#import "ios/chrome/browser/download/model/download_record_service_factory.h"
#import "ios/chrome/browser/download/model/external_app_util.h"
#import "ios/chrome/browser/download/model/installation_notifier.h"
#import "ios/chrome/browser/download/ui/download_manager_view_controller.h"
#import "ios/chrome/browser/download/ui/download_manager_view_controller_delegate.h"
#import "ios/chrome/browser/download/ui/download_manager_view_controller_protocol.h"
#import "ios/chrome/browser/download/ui/unopened_downloads_tracker.h"
#import "ios/chrome/browser/drive/model/drive_service_factory.h"
#import "ios/chrome/browser/drive/model/upload_task.h"
#import "ios/chrome/browser/fullscreen/ui_bundled/fullscreen_controller.h"
#import "ios/chrome/browser/overlays/model/public/common/confirmation/confirmation_overlay_response.h"
#import "ios/chrome/browser/overlays/model/public/overlay_callback_manager.h"
#import "ios/chrome/browser/overlays/model/public/overlay_request_queue.h"
#import "ios/chrome/browser/presenters/ui_bundled/contained_presenter.h"
#import "ios/chrome/browser/presenters/ui_bundled/contained_presenter_delegate.h"
#import "ios/chrome/browser/shared/coordinator/layout_guide/layout_guide_util.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/auto_deletion_commands.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/download_list_commands.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/shared/public/commands/save_to_drive_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/store_kit/model/store_kit_coordinator.h"
#import "ios/chrome/browser/store_kit/model/store_kit_coordinator_delegate.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/components/webui/web_ui_url_constants.h"
#import "ios/web/common/features.h"
#import "ios/web/public/download/download_task.h"
#import "ios/web/public/web_client.h"
#import "net/base/apple/url_conversions.h"
#import "net/base/net_errors.h"
#import "ui/base/l10n/l10n_util_mac.h"

@interface DownloadManagerCoordinator () <ContainedPresenterDelegate,
                                          DownloadManagerViewControllerDelegate,
                                          StoreKitCoordinatorDelegate> {
  // View controller for presenting Download Manager UI.
  UIViewController<DownloadManagerConsumer,
                   DownloadManagerViewControllerProtocol>* _viewController;
  // View controller for presenting "Open In.." dialog.
  UIActivityViewController* _openInController;
  DownloadManagerMediator _mediator;
  StoreKitCoordinator* _storeKitCoordinator;
  UnopenedDownloadsTracker _unopenedDownloads;
  // YES after _stop has been called.
  BOOL _stopped;
  // YES if the UI presented by this coordinator should adapt to the fullscreen.
  BOOL _shouldObserveFullscreen;
  // `start` was called when the coordinator stopping animation was still
  // in progress.
  // Restart when the animation ends.
  BOOL _restartPending;
}
@end

@implementation DownloadManagerCoordinator

- (void)dealloc {
  DCHECK(_stopped);
}

- (void)start {
  [self restart];
}

// Similar to start but can be called after pause.
- (void)restart {
  DCHECK(self.presenter);
  DCHECK(self.browser);

  if (_stopped && self.presenter.presentedViewController) {
    // Stopping animation is still in progress. Wait until it is done to
    // restart.
    _restartPending = YES;
    return;
  }
  _stopped = NO;
  NSNotificationCenter* defaultCenter = [NSNotificationCenter defaultCenter];
  [defaultCenter addObserver:self
                    selector:@selector(applicationDidEnterBackground:)
                        name:UIApplicationDidEnterBackgroundNotification
                      object:nil];

  BOOL isIncognito = self.isOffTheRecord;
  _viewController = [[DownloadManagerViewController alloc] init];
  _viewController.delegate = self;
  _viewController.layoutGuideCenter = LayoutGuideCenterForBrowser(self.browser);
  _viewController.incognito = isIncognito;

  if (_shouldObserveFullscreen) {
    FullscreenController* fullscreenController =
        FullscreenController::FromBrowser(self.browser);
    [_viewController setFullscreenController:fullscreenController];
  }

  _mediator.SetIsIncognito(isIncognito);
  ProfileIOS* profile = self.profile;
  _mediator.SetIdentityManager(IdentityManagerFactory::GetForProfile(profile));
  _mediator.SetDriveService(drive::DriveServiceFactory::GetForProfile(profile));
  _mediator.SetPrefService(profile->GetPrefs());
  if (IsDownloadListEnabled()) {
    _mediator.SetDownloadRecordService(
        DownloadRecordServiceFactory::GetForProfile(profile));
  }

  _mediator.SetDownloadTask(_downloadTask);
  _mediator.SetConsumer(_viewController);
  if (base::FeatureList::IsEnabled(kIOSDownloadNoUIUpdateInBackground)) {
    _mediator.StartObservingNotifications();
  }

  self.presenter.baseViewController = self.baseViewController;
  self.presenter.presentedViewController = _viewController;
  self.presenter.delegate = self;

  self.browser->GetWebStateList()->AddObserver(&_unopenedDownloads);

  [self.presenter prepareForPresentation];

  [self.presenter presentAnimated:self.animatesPresentation];
}

- (void)stop {
  [self pause];
}

// Similar to stop, but the coordinator can be restarted later.
- (void)pause {
  if (_stopped) {
    return;
  }

  _mediator.SetDriveService(nullptr);
  _mediator.SetPrefService(nullptr);
  _mediator.SetIdentityManager(nullptr);
  if (IsDownloadListEnabled()) {
    _mediator.SetDownloadRecordService(nullptr);
  }
  if (base::FeatureList::IsEnabled(kIOSDownloadNoUIUpdateInBackground)) {
    _mediator.StopObservingNotifications();
  }

  if (_viewController) {
    [self.presenter dismissAnimated:self.animatesPresentation];
    // Prevent delegate callbacks for stopped coordinator.
    _viewController.delegate = nil;
    [_viewController setFullscreenController:nullptr];
    _viewController = nil;
  }

  _shouldObserveFullscreen = NO;
  _downloadTask = nullptr;

  self.browser->GetWebStateList()->RemoveObserver(&_unopenedDownloads);

  [self stopStoreKitCoordinator];

  [[InstallationNotifier sharedInstance] unregisterForNotifications:self];
  _stopped = YES;
}

- (UIViewController*)viewController {
  return _viewController;
}

#pragma mark - DownloadManagerTabHelperDelegate

- (void)downloadManagerTabHelper:(DownloadManagerTabHelper*)tabHelper
               didCreateDownload:(web::DownloadTask*)download
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
    [self restart];
  }
}

- (void)downloadManagerTabHelper:(DownloadManagerTabHelper*)tabHelper
         decidePolicyForDownload:(web::DownloadTask*)download
               completionHandler:(void (^)(NewDownloadPolicy))handler {
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

- (void)downloadManagerTabHelper:(DownloadManagerTabHelper*)tabHelper
                 didHideDownload:(web::DownloadTask*)download
                        animated:(BOOL)animated {
  DCHECK_EQ(_downloadTask, download);
  self.animatesPresentation = animated;
  [self stop];
  self.animatesPresentation = YES;
}

- (void)downloadManagerTabHelper:(DownloadManagerTabHelper*)tabHelper
                 didShowDownload:(web::DownloadTask*)download
                        animated:(BOOL)animated {
  DCHECK_NE(_downloadTask, download);
  _downloadTask = download;
  self.animatesPresentation = animated;
  [self start];
  self.animatesPresentation = YES;
}

- (void)downloadManagerTabHelper:(DownloadManagerTabHelper*)tabHelper
              didCleanupDownload:(web::DownloadTask*)download {
  if (!_downloadTask) {
    // If the task was initially cancelled from this coordinator, it may already
    // be stopped. Test if the `_downloadTask` was already cleaned before this
    // observer is called.
    return;
  }
  DCHECK_EQ(_downloadTask, download);
  self.animatesPresentation = NO;
  [self pause];
  self.animatesPresentation = YES;
}

- (void)downloadManagerTabHelper:(DownloadManagerTabHelper*)tabHelper
               adaptToFullscreen:(bool)adaptToFullscreen {
  _shouldObserveFullscreen = adaptToFullscreen;
  if (adaptToFullscreen) {
    FullscreenController* fullscreenController =
        FullscreenController::FromBrowser(self.browser);
    [_viewController setFullscreenController:fullscreenController];
  } else {
    [_viewController setFullscreenController:nullptr];
  }
}

- (void)downloadManagerTabHelper:(DownloadManagerTabHelper*)tabHelper
            wantsToStartDownload:(web::DownloadTask*)download {
  DCHECK_EQ(_downloadTask, download);
  [self tryDownload];
}

#pragma mark - ContainedPresenterDelegate

- (void)containedPresenterDidPresent:(id<ContainedPresenter>)presenter {
  DCHECK(presenter == self.presenter);
}

- (void)containedPresenterDidDismiss:(id<ContainedPresenter>)presenter {
  DCHECK(presenter == self.presenter);
  // The view controller may not be dealloced immediately.
  presenter.presentedViewController = nil;
  if (_restartPending) {
    _restartPending = NO;
    [self start];
  }
}

#pragma mark - DownloadManagerViewControllerDelegate

- (void)downloadManagerViewControllerDidClose:(UIViewController*)controller {
  if (_mediator.GetDownloadManagerState() !=
      DownloadManagerState::kInProgress) {
    base::UmaHistogramEnumeration("Download.IOSDownloadFileResult",
                                  DownloadFileResult::NotStarted,
                                  DownloadFileResult::Count);
    base::RecordAction(base::UserMetricsAction("IOSDownloadClose"));
    [self cleanupCurrentDownload];
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
    (UIViewController*)controller {
  base::RecordAction(base::UserMetricsAction("IOSDownloadInstallGoogleDrive"));
  [self presentStoreKitForGoogleDriveApp];
}

- (void)downloadManagerViewControllerDidStartDownload:
    (UIViewController*)controller {
  if (!_mediator.IsSaveToDriveAvailable()) {
    [self tryDownload];
    return;
  }
  id<SaveToDriveCommands> saveToDriveHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), SaveToDriveCommands);
  [saveToDriveHandler showSaveToDriveForDownload:_downloadTask];
}

- (void)downloadManagerViewControllerDidRetry:(UIViewController*)controller {
  UploadTask* uploadTask = _mediator.GetUploadTask();
  if (uploadTask && uploadTask->GetError() != nil) {
    // If there is an upload task which failed, retry the upload.
    base::RecordAction(base::UserMetricsAction("MobileDownloadRetryUpload"));
    uploadTask->Start();
    return;
  }
  // Otherwise retry download.
  [self tryDownload];
}

- (void)downloadManagerViewControllerDidOpenInDriveApp:
    (UIViewController*)controller {
  UploadTask* uploadTask = _mediator.GetUploadTask();
  if (!uploadTask) {
    // While it should not be possible that uploadTask is nil at this point,
    // there has been reports that show it is possible.
    // TODO(crbug.com/324897399): investigate and remove early return.
    return;
  }
  base::RecordAction(base::UserMetricsAction("IOSDownloadOpenInDriveApp"));
  std::optional<GURL> openFileInDriveURL =
      uploadTask->GetResponseLink(/* add_user_identifier= */ true);
  CHECK(openFileInDriveURL);
  [UIApplication.sharedApplication
                openURL:net::NSURLWithGURL(*openFileInDriveURL)
                options:@{UIApplicationOpenURLOptionUniversalLinksOnly : @YES}
      completionHandler:nil];
}

- (void)presentOpenInForDownloadManagerViewController:
    (UIViewController*)controller {
  if (IsDownloadListEnabled()) {
    id<DownloadListCommands> downloadListHandler = HandlerForProtocol(
        self.browser->GetCommandDispatcher(), DownloadListCommands);
    [downloadListHandler showDownloadList];
    return;
  }
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
      _viewController.openInSourceView;
  _openInController.popoverPresentationController.sourceRect =
      _viewController.openInSourceView.bounds;
  [_viewController presentViewController:_openInController
                                animated:YES
                              completion:nil];
}

- (void)openDownloadedFileForDownloadManagerViewController:
    (UIViewController*)controller {
  base::RecordAction(base::UserMetricsAction("IOSDownloadOpen"));
  base::FilePath path = _mediator.GetDownloadPath();
  GURL filePathURL =
      GURL(base::StringPrintf("%s://%s", "file", path.value().c_str()));
  GURL virtualFilePathURL = GURL(
      base::StringPrintf("%s://%s/%s", kChromeUIScheme, kChromeUIDownloadsHost,
                         filePathURL.ExtractFileName().c_str()));
  OpenNewTabCommand* command =
      [[OpenNewTabCommand alloc] initWithURL:filePathURL
                                  virtualURL:virtualFilePathURL
                                    referrer:web::Referrer()
                                 inIncognito:self.isOffTheRecord
                                inBackground:NO
                                    appendTo:OpenPosition::kCurrentTab];
  id<ApplicationCommands> applicationHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), ApplicationCommands);
  [applicationHandler openURLInNewTab:command];
}

#pragma mark - Private

// Attempts to start the current download task, either for the first time or
// after one or several previously failed attempts.
- (void)tryDownload {
  DownloadManagerTabHelper* tabHelper =
      DownloadManagerTabHelper::FromWebState(_downloadTask->GetWebState());
  if (_downloadTask->GetErrorCode() != net::OK) {
    base::RecordAction(base::UserMetricsAction("MobileDownloadRetryDownload"));
  } else if (tabHelper->WillDownloadTaskBeSavedToDrive()) {
    base::RecordAction(
        base::UserMetricsAction("IOSDownloadStartDownloadToDrive"));
  } else {
    base::RecordAction(base::UserMetricsAction("IOSDownloadStartDownload"));
    _unopenedDownloads.Add(_downloadTask);
    [self maybePresentAutoDeletionActionSheet];
  }
  _mediator.StartDownloading();
}

- (void)stopStoreKitCoordinator {
  [_storeKitCoordinator stop];
  _storeKitCoordinator.delegate = nil;
  _storeKitCoordinator = nil;
}

- (void)cleanupCurrentDownload {
  if (!_downloadTask) {
    return;
  }
  // Copy the task pointer before pause nullifies _downloadTask.
  web::DownloadTask* downloadTask = _downloadTask;
  [self pause];

  DownloadManagerTabHelper* tabHelper =
      DownloadManagerTabHelper::FromWebState(downloadTask->GetWebState());
  tabHelper->CleanupCurrentDownload();
}

// Cancels the download task and stops the coordinator.
- (void)cancelDownload {
  // `pause` nulls-our _downloadTask and `Cancel` destroys the task. Call `stop`
  // first to perform all coordinator cleanups, but copy `_downloadTask`
  // pointer to destroy the task.
  web::DownloadTask* downloadTask = _downloadTask;
  [self pause];

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
  _mediator.SetGoogleDriveAppInstalled(true);
  _mediator.UpdateConsumer();
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
  [[InstallationNotifier sharedInstance]
      registerForInstallationNotifications:self
                              withSelector:@selector(didInstallGoogleDriveApp)
                                 forScheme:kGoogleDriveAppURLScheme];
}

// Presents the Download auto-deletion action sheet if the feature flag is
// enabled and the user has the enabled in settings.
- (void)maybePresentAutoDeletionActionSheet {
  if (!IsDownloadAutoDeletionFeatureEnabled()) {
    return;
  }

  id<AutoDeletionCommands> autoDeletionHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), AutoDeletionCommands);
  [autoDeletionHandler
      presentAutoDeletionActionSheetWithDownloadTask:_downloadTask];
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
