// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/download/download_manager_coordinator.h"

#import <MobileCoreServices/MobileCoreServices.h>
#import <StoreKit/StoreKit.h>

#import <memory>
#import <set>
#import <utility>

#import "base/check_op.h"
#import "base/functional/bind.h"
#import "base/mac/scoped_cftyperef.h"
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
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/overlays/public/common/confirmation/confirmation_overlay_response.h"
#import "ios/chrome/browser/overlays/public/overlay_callback_manager.h"
#import "ios/chrome/browser/overlays/public/overlay_request_queue.h"
#import "ios/chrome/browser/shared/coordinator/layout_guide/layout_guide_util.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/store_kit/store_kit_coordinator.h"
#import "ios/chrome/browser/ui/download/activities/open_downloads_folder_activity.h"
#import "ios/chrome/browser/ui/download/download_manager_mediator.h"
#import "ios/chrome/browser/ui/download/download_manager_view_controller.h"
#import "ios/chrome/browser/ui/presenters/contained_presenter.h"
#import "ios/chrome/browser/ui/presenters/contained_presenter_delegate.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_list_observer.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/common/features.h"
#import "ios/web/public/download/download_task.h"
#import "net/base/net_errors.h"
#import "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Tracks download tasks which were not opened by the user yet. Reports various
// metrics in DownloadTaskObserver callbacks.
class UnopenedDownloadsTracker : public web::DownloadTaskObserver,
                                 public WebStateListObserver {
 public:
  // Starts tracking this download task.
  void Add(web::DownloadTask* task) {
    task->AddObserver(this);
    observed_tasks_.insert(task);
  }
  // Stops tracking this download task.
  void Remove(web::DownloadTask* task) {
    task->RemoveObserver(this);
    observed_tasks_.erase(task);
  }
  // DownloadTaskObserver overrides:
  void OnDownloadUpdated(web::DownloadTask* task) override {
    if (task->IsDone()) {
      base::UmaHistogramEnumeration("Download.IOSDownloadFileResult",
                                    task->GetErrorCode()
                                        ? DownloadFileResult::Failure
                                        : DownloadFileResult::Completed,
                                    DownloadFileResult::Count);
      if (task->GetErrorCode()) {
        base::UmaHistogramSparse("Download.IOSDownloadedFileNetError",
                                 -task->GetErrorCode());
      } else {
        bool GoogleDriveIsInstalled = IsGoogleDriveAppInstalled();
        if (GoogleDriveIsInstalled)
          base::UmaHistogramEnumeration(
              "Download.IOSDownloadFileUIGoogleDrive",
              DownloadFileUIGoogleDrive::GoogleDriveAlreadyInstalled,
              DownloadFileUIGoogleDrive::Count);
        else
          base::UmaHistogramEnumeration(
              "Download.IOSDownloadFileUIGoogleDrive",
              DownloadFileUIGoogleDrive::GoogleDriveNotInstalled,
              DownloadFileUIGoogleDrive::Count);
      }

      bool backgrounded = task->HasPerformedBackgroundDownload();
      DownloadFileInBackground histogram_value =
          task->GetErrorCode()
              ? (backgrounded
                     ? DownloadFileInBackground::FailedWithBackgrounding
                     : DownloadFileInBackground::FailedWithoutBackgrounding)
              : (backgrounded
                     ? DownloadFileInBackground::SucceededWithBackgrounding
                     : DownloadFileInBackground::SucceededWithoutBackgrounding);
      base::UmaHistogramEnumeration("Download.IOSDownloadFileInBackground",
                                    histogram_value,
                                    DownloadFileInBackground::Count);
    }
  }
  void OnDownloadDestroyed(web::DownloadTask* task) override {
    // This download task was never open by the user.
    task->RemoveObserver(this);
    observed_tasks_.erase(task);

    DownloadAborted(task);
  }

  // Logs histograms. Called when DownloadTask or this object was destroyed.
  void DownloadAborted(web::DownloadTask* task) {
    if (task->GetState() == web::DownloadTask::State::kInProgress) {
      base::UmaHistogramEnumeration("Download.IOSDownloadFileResult",
                                    DownloadFileResult::Other,
                                    DownloadFileResult::Count);

      if (did_close_web_state_without_user_action) {
        // web state can be closed without user action only during the app
        // shutdown.
        base::UmaHistogramEnumeration(
            "Download.IOSDownloadFileInBackground",
            DownloadFileInBackground::CanceledAfterAppQuit,
            DownloadFileInBackground::Count);
      }
    }

    if (task->IsDone() && task->GetErrorCode() == net::OK) {
      base::UmaHistogramEnumeration(
          "Download.IOSDownloadedFileAction",
          DownloadedFileAction::NoActionOrOpenedViaExtension,
          DownloadedFileAction::Count);
    }
  }
  // WebStateListObserver overrides:
  void WillCloseWebStateAt(WebStateList* web_state_list,
                           web::WebState* web_state,
                           int index,
                           bool user_action) override {
    if (!user_action) {
      did_close_web_state_without_user_action = true;
    }
  }

  ~UnopenedDownloadsTracker() override {
    for (web::DownloadTask* task : observed_tasks_) {
      task->RemoveObserver(this);
      DownloadAborted(task);
    }
  }

 private:
  // True if a web state was closed without user action.
  bool did_close_web_state_without_user_action = false;
  // Keeps track of observed tasks to remove observer when
  // UnopenedDownloadsTracker is destructed.
  std::set<web::DownloadTask*> observed_tasks_;
};
}  // namespace

@interface DownloadManagerCoordinator () <
    ContainedPresenterDelegate,
    DownloadManagerViewControllerDelegate> {
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

  _viewController = [[DownloadManagerViewController alloc] init];
  _viewController.delegate = self;
  _viewController.layoutGuideCenter = LayoutGuideCenterForBrowser(self.browser);
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

  [_storeKitCoordinator stop];
  _storeKitCoordinator = nil;

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

  if (web::features::IsFullscreenAPIEnabled()) {
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
  if (web::features::IsFullscreenAPIEnabled()) {
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

@end
