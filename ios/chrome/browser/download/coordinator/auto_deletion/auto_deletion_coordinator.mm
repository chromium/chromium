// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/download/coordinator/auto_deletion/auto_deletion_coordinator.h"

#import "base/memory/raw_ptr.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "components/feature_engagement/public/feature_constants.h"
#import "components/feature_engagement/public/tracker.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/download/coordinator/auto_deletion/auto_deletion_iph_coordinator.h"
#import "ios/chrome/browser/download/model/auto_deletion/auto_deletion_service.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/shared/coordinator/alert/action_sheet_coordinator.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/public/commands/auto_deletion_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/download/download_task.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {
// The number of bytes in a KB.
constexpr CGFloat kBytesInKiloBytes = 1000;
// The number of KB in a MB.
constexpr CGFloat kKiloBytesInMegaBytes = 1000;
// The number of bytes in MB.
constexpr CGFloat kBytesInMegaBytes = kBytesInKiloBytes * kKiloBytesInMegaBytes;
// The threshold where if the user has less than this percentage of storage
// remaining on their device then the Auto-deletion IPH should be shown. This
// value is a percentage.
constexpr CGFloat kAvailableStorageThreshold = 2.0;
// The threshold where if a file downloaded onto the device is greater than this
// value then the Auto-deletion IPH should be shown. This value is in units of
// MB.
constexpr CGFloat kLargeFileSizeThreshold = 20.0;
}  // namespace

typedef void (^UIAlertActionHandler)(UIAlertAction* action);

@implementation AutoDeletionCoordinator {
  // The task that is downloading the web content onto the device.
  raw_ptr<web::DownloadTask> _downloadTask;
  // The coordinator that manages the Auto-deletion action sheet.
  ActionSheetCoordinator* _actionSheetCoordinator;
  // The coordinator that manages the Auto-deletion IPH.
  AutoDeletionIPHCoordinator* _IPHCoordinator;
}

- (instancetype)initWithBaseViewController:(UIViewController*)baseViewController
                                   browser:(Browser*)browser
                              downloadTask:(web::DownloadTask*)task {
  self = [super initWithBaseViewController:baseViewController browser:browser];
  if (self) {
    _downloadTask = task;
  }
  return self;
}

- (void)start {
  PrefService* localState = GetApplicationContext()->GetLocalState();
  BOOL isAutoDeletionEnabled =
      localState->GetBoolean(prefs::kDownloadAutoDeletionEnabled);
  if (!isAutoDeletionEnabled) {
    BOOL hasIPHBeenShown =
        localState->GetBoolean(prefs::kDownloadAutoDeletionIPHShown);
    if (!hasIPHBeenShown && [self shouldIPHBeShown]) {
      [self presentIPH];
      return;
    }

    [self dismiss];
    return;
  }

  _actionSheetCoordinator = [self createActionCoordinator];
  [_actionSheetCoordinator start];
}

- (void)stop {
  [_actionSheetCoordinator stop];
  _actionSheetCoordinator = nullptr;

  [_IPHCoordinator stop];
  _IPHCoordinator = nullptr;
}

#pragma mark - Private

// Creates the action sheet coordinator that manages the display of an action
// sheet when a user downloads content from the web onto the device. If the user
// clicks the primary action button, the downloaded file is scheduled for
// automatic deletion. Otherwise, the file will not be automatically deleted.
- (ActionSheetCoordinator*)createActionCoordinator {
  ActionSheetCoordinator* coordinator = [[ActionSheetCoordinator alloc]
      initWithBaseViewController:self.baseViewController
                         browser:self.browser
                           title:l10n_util::GetNSString(
                                     IDS_IOS_AUTO_DELETION_ACTION_SHEET_TITLE)
                         message:
                             l10n_util::GetNSString(
                                 IDS_IOS_AUTO_DELETION_ACTION_SHEET_DESCRIPTION)
                            rect:self.baseViewController.view.bounds
                            view:self.baseViewController.view];
  __weak __typeof(self) weakSelf = self;
  ProceduralBlock primaryItemAction = ^{
    base::RecordAction(base::UserMetricsAction(
        "IOS.AutoDeletion.ActionSheet.AcceptDownloadEnrollment"));
    [weakSelf scheduleFileForDeletion];
    [weakSelf dismiss];
  };
  [coordinator
      addItemWithTitle:l10n_util::GetNSString(
                           IDS_IOS_AUTO_DELETION_ACTION_SHEET_PRIMARY_ACTION)
                action:primaryItemAction
                 style:UIAlertActionStyleDestructive];
  ProceduralBlock cancelAction = ^{
    base::RecordAction(base::UserMetricsAction(
        "IOS.AutoDeletion.ActionSheet.RejectDownloadEnrollment"));
    [weakSelf cancel];
  };
  [coordinator
      addItemWithTitle:l10n_util::GetNSString(
                           IDS_IOS_AUTO_DELETION_ACTION_SHEET_CANCEL_ACTION)
                action:cancelAction
                 style:UIAlertActionStyleCancel];

  return coordinator;
}

// Creates the coordinator that manages the Auto-deletion IPH and displays the
// IPH on the screen. This function also initializes the UIGestureRecognizer and
// attaches it to the window to handle dimssing the IPH properly when the user
// swipes-down on it.
- (void)presentIPH {
  _IPHCoordinator = [[AutoDeletionIPHCoordinator alloc]
      initWithBaseViewController:self.baseViewController
                         browser:self.browser
                    downloadTask:_downloadTask];
  [_IPHCoordinator start];

  // Store that the IPH has been displayed.
  PrefService* localState = GetApplicationContext()->GetLocalState();
  localState->SetBoolean(prefs::kDownloadAutoDeletionIPHShown, true);
}

// Schedules the downloaded file for automatic deletion when the user hits the
// action sheet's primary action button.
- (void)scheduleFileForDeletion {
  GetApplicationContext()->GetAutoDeletionService()->MarkTaskForDeletion(
      _downloadTask, auto_deletion::DeletionEnrollmentStatus::kEnrolled);
}

// Informs the AutoDeletionService that the user does not intend to enroll the
// file in Auto-deletion and then closes the action sheet.
- (void)cancel {
  GetApplicationContext()->GetAutoDeletionService()->MarkTaskForDeletion(
      _downloadTask, auto_deletion::DeletionEnrollmentStatus::kNotEnrolled);
  [self dismiss];
}

// Creates a handler that conforms to the AutoDeletionCommands protocol and
// invokes its `dismissAutoDeletionActionSheet` function.
- (void)dismiss {
  id<AutoDeletionCommands> handler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), AutoDeletionCommands);
  [handler dismissAutoDeletionActionSheet];
}

// Checks if the Auto-deletion IPH should be shown. The IPH is shown if one of
// these conditions is met:
// 1. The downloaded file is larger than 20MB OR
// 2. The device's storage has less than 2% of capacity available OR
// 3. The user has cleared browsing history in the last 60 days OR
// 4. The user has used incognito in the past 60 days OR
// 5. The user is actively downloading content in Incognito.
- (BOOL)shouldIPHBeShown {
  int64_t byteThreshold = kLargeFileSizeThreshold * kBytesInMegaBytes;
  BOOL downloadFileIsLarge = _downloadTask->GetTotalBytes() >= byteThreshold;
  BOOL deviceIsNearCapacity =
      [self percentOfStorageAvailable] < kAvailableStorageThreshold;
  BOOL downloadedWhileInIncognito =
      self.browser->type() == Browser::Type::kIncognito;
  feature_engagement::Tracker* tracker =
      feature_engagement::TrackerFactory::GetForProfile(self.profile);
  BOOL triggerCriterionMet = tracker->WouldTriggerHelpUI(
      feature_engagement::kIPHiOSDownloadAutoDeletionFeature);

  return downloadFileIsLarge || deviceIsNearCapacity || triggerCriterionMet ||
         downloadedWhileInIncognito;
}

// Calculates the percent of storage remaining on the user's device.
- (int)percentOfStorageAvailable {
  NSURL* fileURL = [[NSURL alloc] initFileURLWithPath:@"/"];
  NSError* error = nil;
  NSDictionary* results = [fileURL resourceValuesForKeys:@[
    NSURLVolumeAvailableCapacityKey, NSURLVolumeTotalCapacityKey
  ]
                                                   error:&error];
  if (!results) {
    // If the total storage & available capacity cannot be determined, we don't
    // want storage to be the factor that's used to decide whether to display
    // the IPH. Therefore, 100% will be returned to prevent this.
    return 100;
  }

  NSUInteger availableStorage =
      [results[NSURLVolumeAvailableCapacityKey] integerValue];
  NSUInteger totalStorage = [results[NSURLVolumeTotalCapacityKey] integerValue];

  return ((float)availableStorage / (float)totalStorage) * 100;
}

@end
