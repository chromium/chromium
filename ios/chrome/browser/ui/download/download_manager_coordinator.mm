// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/download/download_manager_coordinator.h"

#import <MobileCoreServices/MobileCoreServices.h>
#import <StoreKit/StoreKit.h>

#include <memory>

#include "base/logging.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/sys_string_conversions.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/download/download_manager_metric_names.h"
#import "ios/chrome/browser/download/download_manager_tab_helper.h"
#import "ios/chrome/browser/download/google_drive_app_util.h"
#import "ios/chrome/browser/installation_notifier.h"
#import "ios/chrome/browser/store_kit/store_kit_coordinator.h"
#import "ios/chrome/browser/ui/alert_coordinator/alert_coordinator.h"
#import "ios/chrome/browser/ui/download/download_manager_mediator.h"
#import "ios/chrome/browser/ui/download/download_manager_view_controller.h"
#import "ios/chrome/browser/ui/presenters/contained_presenter.h"
#import "ios/chrome/browser/ui/presenters/contained_presenter_delegate.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_list_observer.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/download/download_task.h"
#include "net/base/net_errors.h"
#include "net/url_request/url_fetcher_response_writer.h"
#include "ui/base/l10n/l10n_util_mac.h"

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
      UMA_HISTOGRAM_ENUMERATION("Download.IOSDownloadFileResult",
                                task->GetErrorCode()
                                    ? DownloadFileResult::Failure
                                    : DownloadFileResult::Completed,
                                DownloadFileResult::Count);
      if (task->GetErrorCode()) {
        base::UmaHistogramSparse("Download.IOSDownloadedFileNetError",
                                 -task->GetErrorCode());
      } else {
        UMA_HISTOGRAM_BOOLEAN("Download.IOSDownloadInstallDrivePromoShown",
                              !IsGoogleDriveAppInstalled());
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
      UMA_HISTOGRAM_ENUMERATION("Download.IOSDownloadFileInBackground",
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
      UMA_HISTOGRAM_ENUMERATION("Download.IOSDownloadFileResult",
                                DownloadFileResult::Other,
                                DownloadFileResult::Count);

      if (did_close_web_state_without_user_action) {
        // web state can be closed without user action only during the app
        // shutdown.
        UMA_HISTOGRAM_ENUMERATION(
            "Download.IOSDownloadFileInBackground",
            DownloadFileInBackground::CanceledAfterAppQuit,
            DownloadFileInBackground::Count);
      }
    }

    if (task->IsDone() && task->GetErrorCode() == net::OK) {
      UMA_HISTOGRAM_ENUMERATION(
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

@interface DownloadManagerCoordinator ()<
    ContainedPresenterDelegate,
    DownloadManagerViewControllerDelegate,
    UIDocumentInteractionControllerDelegate> {
  // View controller for presenting Download Manager UI.
  DownloadManagerViewController* _viewController;
  // A dialog which requests a confirmation from the user.
  UIAlertController* _confirmationDialog;
  // View controller for presenting "Open In.." dialog.
  UIDocumentInteractionController* _openInController;
  DownloadManagerMediator _mediator;
  StoreKitCoordinator* _storeKitCoordinator;
  // Coordinator for displaying the alert informing the user that no application
  // on the device can open the file. The alert offers the user to install
  // Google Drive app.
  AlertCoordinator* _installDriveAlertCoordinator;
  UnopenedDownloadsTracker _unopenedDownloads;
}
@end

@implementation DownloadManagerCoordinator

@synthesize presenter = _presenter;
@synthesize animatesPresentation = _animatesPresentation;
@synthesize downloadTask = _downloadTask;
@synthesize webStateList = _webStateList;
@synthesize bottomMarginHeightAnchor = _bottomMarginHeightAnchor;

- (void)dealloc {
  [[InstallationNotifier sharedInstance] unregisterForNotifications:self];
}

- (void)start {
  DCHECK(self.presenter);

  _viewController = [[DownloadManagerViewController alloc] init];
  _viewController.delegate = self;
  _viewController.bottomMarginHeightAnchor = self.bottomMarginHeightAnchor;
  _mediator.SetDownloadTask(_downloadTask);
  _mediator.SetConsumer(_viewController);

  self.presenter.baseViewController = self.baseViewController;
  self.presenter.presentedViewController = _viewController;
  self.presenter.delegate = self;

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
  [_confirmationDialog dismissViewControllerAnimated:self.animatesPresentation
                                          completion:nil];
  _confirmationDialog = nil;
  _downloadTask = nullptr;
  self.webStateList = nullptr;

  [_storeKitCoordinator stop];
  _storeKitCoordinator = nil;
  [_installDriveAlertCoordinator stop];
  _installDriveAlertCoordinator = nil;
}

- (void)setWebStateList:(WebStateList*)webStateList {
  if (_webStateList == webStateList)
    return;

  if (_webStateList)
    _webStateList->RemoveObserver(&_unopenedDownloads);

  _webStateList = webStateList;

  if (_webStateList)
    _webStateList->AddObserver(&_unopenedDownloads);
}

- (UIViewController*)viewController {
  return _viewController;
}

#pragma mark - DownloadManagerTabHelperDelegate

- (void)downloadManagerTabHelper:(nonnull DownloadManagerTabHelper*)tabHelper
               didCreateDownload:(nonnull web::DownloadTask*)download
               webStateIsVisible:(BOOL)webStateIsVisible {
  base::RecordAction(base::UserMetricsAction("MobileDownloadFileUIShown"));
  if (!webStateIsVisible) {
    // Do nothing if a background Tab requested download UI presentation.
    return;
  }

  BOOL replacingExistingDownload = _downloadTask ? YES : NO;
  _downloadTask = download;

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
  const int title = IDS_IOS_DOWNLOAD_MANAGER_REPLACE_CONFIRMATION;
  const int message = IDS_IOS_DOWNLOAD_MANAGER_REPLACE_CONFIRMATION_MESSAGE;
  [self runConfirmationDialogWithTitle:title
                               message:message
                          confirmTitle:IDS_OK
                           cancelTitle:IDS_CANCEL
                     completionHandler:^(BOOL confirmed) {
                       UMA_HISTOGRAM_BOOLEAN("Download.IOSDownloadReplaced",
                                             confirmed);
                       handler(confirmed ? kNewDownloadPolicyReplace
                                         : kNewDownloadPolicyDiscard);
                     }];
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

#pragma mark - UIDocumentInteractionControllerDelegate

- (void)documentInteractionController:
            (UIDocumentInteractionController*)controller
        willBeginSendingToApplication:(NSString*)applicationID {
  DownloadedFileAction action = [applicationID isEqual:kGoogleDriveAppBundleID]
                                    ? DownloadedFileAction::OpenedInDrive
                                    : DownloadedFileAction::OpenedInOtherApp;
  UMA_HISTOGRAM_ENUMERATION("Download.IOSDownloadedFileAction", action,
                            DownloadedFileAction::Count);
  if (_downloadTask) {  // _downloadTask can be null if coordinator was stopped.
    _unopenedDownloads.Remove(_downloadTask);
  }
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
    UMA_HISTOGRAM_ENUMERATION("Download.IOSDownloadFileResult",
                              DownloadFileResult::NotStarted,
                              DownloadFileResult::Count);
    [self cancelDownload];
    return;
  }

  __weak DownloadManagerCoordinator* weakSelf = self;
  int title = IDS_IOS_DOWNLOAD_MANAGER_CANCEL_CONFIRMATION;
  [self runConfirmationDialogWithTitle:title
                               message:-1
                          confirmTitle:IDS_IOS_DOWNLOAD_MANAGER_STOP
                           cancelTitle:IDS_IOS_DOWNLOAD_MANAGER_CONTINUE
                     completionHandler:^(BOOL confirmed) {
                       if (confirmed) {
                         UMA_HISTOGRAM_ENUMERATION(
                             "Download.IOSDownloadFileResult",
                             DownloadFileResult::Cancelled,
                             DownloadFileResult::Count);

                         [weakSelf cancelDownload];
                       }
                     }];
}

- (void)installDriveForDownloadManagerViewController:
    (DownloadManagerViewController*)controller {
  [self presentStoreKitForGoogleDriveApp];
}

- (void)downloadManagerViewControllerDidStartDownload:
    (DownloadManagerViewController*)controller {
  if (_downloadTask->GetErrorCode() != net::OK) {
    base::RecordAction(base::UserMetricsAction("MobileDownloadRetryDownload"));
  } else {
    _unopenedDownloads.Add(_downloadTask);
  }
  _mediator.StartDowloading();
}

- (void)downloadManagerViewController:(DownloadManagerViewController*)controller
     presentOpenInMenuWithLayoutGuide:(UILayoutGuide*)layoutGuide {
  base::FilePath path =
      _downloadTask->GetResponseWriter()->AsFileWriter()->file_path();
  NSURL* URL = [NSURL fileURLWithPath:base::SysUTF8ToNSString(path.value())];
  _openInController =
      [UIDocumentInteractionController interactionControllerWithURL:URL];

  base::ScopedCFTypeRef<CFStringRef> MIMEType(
      base::SysUTF8ToCFStringRef(_downloadTask->GetMimeType()));
  CFStringRef UTI = UTTypeCreatePreferredIdentifierForTag(
      kUTTagClassMIMEType, MIMEType.get(), nullptr);
  _openInController.UTI = CFBridgingRelease(UTI);
  _openInController.delegate = self;

  BOOL menuShown =
      [_openInController presentOpenInMenuFromRect:layoutGuide.layoutFrame
                                            inView:layoutGuide.owningView
                                          animated:YES];

  // No application on this device can open the file. Typically happens on
  // iOS 10, where Files app does not exist.
  if (!menuShown) {
    [self didFailOpenInMenuPresentation];
  }
}

#pragma mark - Private

// Cancels the download task and stops the coordinator.
- (void)cancelDownload {
  // |stop| nulls-our _downloadTask and |Cancel| destroys the task. Call |stop|
  // first to perform all coordinator cleanups, but retain |_downloadTask|
  // pointer to destroy the task.
  web::DownloadTask* downloadTask = _downloadTask;
  [self stop];
  downloadTask->Cancel();
}

// Presents UIAlertController with |titleID|, |messageID| and two buttons
// (confirmTitleID and cancelTitleID). |handler| is called with YES if confirm
// button was tapped and with NO  if Cancel button was tapped. |messageID| is
// optional and can be -1.
- (void)runConfirmationDialogWithTitle:(int)titleID
                               message:(int)messageID
                          confirmTitle:(int)confirmTitleID
                           cancelTitle:(int)cancelTitleID
                     completionHandler:(void (^)(BOOL confirmed))handler {
  NSString* message = messageID != -1 ? l10n_util::GetNSString(messageID) : nil;
  NSString* title = l10n_util::GetNSString(titleID);
  _confirmationDialog =
      [UIAlertController alertControllerWithTitle:title
                                          message:message
                                   preferredStyle:UIAlertControllerStyleAlert];
  UIAlertAction* OKAction =
      [UIAlertAction actionWithTitle:l10n_util::GetNSString(confirmTitleID)
                               style:UIAlertActionStyleDefault
                             handler:^(UIAlertAction*) {
                               handler(YES);
                             }];
  [_confirmationDialog addAction:OKAction];

  UIAlertAction* cancelAction =
      [UIAlertAction actionWithTitle:l10n_util::GetNSString(cancelTitleID)
                               style:UIAlertActionStyleCancel
                             handler:^(UIAlertAction*) {
                               handler(NO);
                             }];
  [_confirmationDialog addAction:cancelAction];

  [self.baseViewController presentViewController:_confirmationDialog
                                        animated:YES
                                      completion:nil];
}

// Called when Google Drive app is installed after starting StoreKitCoordinator.
- (void)didInstallGoogleDriveApp {
  base::RecordAction(
      base::UserMetricsAction("MobileDownloadFileUIInstallGoogleDrive"));
}

// Called when Open In... menu was not presented. This method shows the alert
// which offers the user to install Google Drive app.
- (void)didFailOpenInMenuPresentation {
  NSString* title =
      l10n_util::GetNSString(IDS_IOS_DOWNLOAD_MANAGER_UNABLE_TO_OPEN_FILE);
  NSString* message =
      l10n_util::GetNSString(IDS_IOS_DOWNLOAD_MANAGER_NO_APP_MESSAGE);

  _installDriveAlertCoordinator = [[AlertCoordinator alloc]
      initWithBaseViewController:self.baseViewController
                           title:title
                         message:message];

  NSString* googleDriveButtonTitle =
      l10n_util::GetNSString(IDS_IOS_DOWNLOAD_MANAGER_UPLOAD_TO_GOOGLE_DRIVE);
  __weak DownloadManagerCoordinator* weakSelf = self;
  [_installDriveAlertCoordinator
      addItemWithTitle:googleDriveButtonTitle
                action:^{
                  [weakSelf presentStoreKitForGoogleDriveApp];
                }
                 style:UIAlertActionStyleDefault];

  [_installDriveAlertCoordinator
      addItemWithTitle:l10n_util::GetNSString(IDS_CANCEL)
                action:nil
                 style:UIAlertActionStyleCancel];

  [_installDriveAlertCoordinator start];
}

// Presents StoreKit dialog for Google Drive application.
- (void)presentStoreKitForGoogleDriveApp {
  if (!_storeKitCoordinator) {
    _storeKitCoordinator = [[StoreKitCoordinator alloc]
        initWithBaseViewController:self.baseViewController];
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
