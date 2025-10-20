// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safari_data_import/coordinator/safari_data_import_import_coordinator.h"

#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

#import "base/check_op.h"
#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"
#import "components/autofill/core/browser/data_manager/payments/payments_data_manager.h"
#import "components/autofill/core/browser/data_manager/personal_data_manager.h"
#import "components/bookmarks/browser/bookmark_model.h"
#import "components/history/core/browser/history_service.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/password_manager/core/browser/ui/saved_passwords_presenter.h"
#import "components/prefs/pref_service.h"
#import "components/reading_list/core/reading_list_model.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/affiliations/model/ios_chrome_affiliation_service_factory.h"
#import "ios/chrome/browser/autofill/model/personal_data_manager_factory.h"
#import "ios/chrome/browser/bookmarks/model/bookmark_model_factory.h"
#import "ios/chrome/browser/favicon/model/ios_chrome_favicon_loader_factory.h"
#import "ios/chrome/browser/history/model/history_service_factory.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_account_password_store_factory.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_profile_password_store_factory.h"
#import "ios/chrome/browser/reading_list/model/reading_list_model_factory.h"
#import "ios/chrome/browser/safari_data_import/coordinator/safari_data_import_child_coordinator_delegate.h"
#import "ios/chrome/browser/safari_data_import/coordinator/safari_data_import_import_mediator.h"
#import "ios/chrome/browser/safari_data_import/public/metrics.h"
#import "ios/chrome/browser/safari_data_import/public/password_import_item.h"
#import "ios/chrome/browser/safari_data_import/public/safari_data_import_stage.h"
#import "ios/chrome/browser/safari_data_import/ui/safari_data_import_import_stage_transition_handler.h"
#import "ios/chrome/browser/safari_data_import/ui/safari_data_import_import_view_controller.h"
#import "ios/chrome/browser/safari_data_import/ui/safari_data_import_password_conflict_resolution_view_controller.h"
#import "ios/chrome/browser/safari_data_import/ui/safari_data_invalid_passwords_view_controller.h"
#import "ios/chrome/browser/safari_data_import/ui/safari_data_item_table_view.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/webauthn/model/ios_passkey_model_factory.h"
#import "ios/chrome/common/ui/promo_style/promo_style_view_controller_delegate.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

/// Number of expected items in the table.
constexpr NSInteger kExpectedItemsCount = 4;

}  // namespace

@interface SafariDataImportImportCoordinator () <
    PromoStyleViewControllerDelegate,
    SafariDataImportImportStageTransitionHandler,
    UITableViewDelegate>

/// The mediator handling the interaction with the model. Lazily loaded with
/// `-mediator` method.
@property(nonatomic, readonly) SafariDataImportImportMediator* mediator;

/// Alert screen being displayed when the last selected file could not be
/// processed or contains no valid items.
@property(nonatomic, readonly) UIAlertController* errorAlert;

/// Alert screen being displayed when the last selected file could not be
/// processed or contains no valid items.
@property(nonatomic, readonly) UIAlertController* fileDeletionAlert;

@end

@implementation SafariDataImportImportCoordinator {
  /// The view controller pushed onto the base navigation controller; user
  /// interacts with it to present other views.
  SafariDataImportImportViewController* _containerViewController;
  /// File picker for the user to select Safari data.
  UIDocumentPickerViewController* _documentProvider;
  /// Table view  that displays the import status of Safari data.
  SafariDataItemTableView* _tableView;
}

@synthesize mediator = _mediator;
@synthesize errorAlert = _errorAlert;
@synthesize fileDeletionAlert = _fileDeletionAlert;
@synthesize baseNavigationController = _baseNavigationController;

- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                         browser:(Browser*)browser {
  self = [super initWithBaseViewController:navigationController
                                   browser:browser];
  if (self) {
    _baseNavigationController = navigationController;
  }
  return self;
}

- (void)start {
  _containerViewController =
      [[SafariDataImportImportViewController alloc] init];
  _containerViewController.delegate = self;
  _tableView =
      [[SafariDataItemTableView alloc] initWithItemCount:kExpectedItemsCount];
  _tableView.delegate = self;
  _tableView.importStageTransitionHandler = self;
  _containerViewController.itemTableView = _tableView;
  self.baseNavigationController.navigationBarHidden = NO;
  [self.baseNavigationController pushViewController:_containerViewController
                                           animated:YES];
}

- (void)stop {
  [self.mediator disconnect];
  self.delegate = nil;
  _containerViewController = nil;
}

#pragma mark - Accessors

- (SafariDataImportStage)importStage {
  return _containerViewController.importStage;
}

- (SafariDataImportImportMediator*)mediator {
  if (!_mediator) {
    /// Use original profile as the user has explicitly requested this operation
    /// to update their personal data.
    ProfileIOS* profile = self.profile->GetOriginalProfile();
    /// Retrieve mediator dependencies.
    std::unique_ptr<password_manager::SavedPasswordsPresenter>
        savedPasswordsPresenter =
            std::make_unique<password_manager::SavedPasswordsPresenter>(
                IOSChromeAffiliationServiceFactory::GetForProfile(profile),
                IOSChromeProfilePasswordStoreFactory::GetForProfile(
                    profile, ServiceAccessType::EXPLICIT_ACCESS),
                IOSChromeAccountPasswordStoreFactory::GetForProfile(
                    profile, ServiceAccessType::EXPLICIT_ACCESS),
                /*passkey_model=*/nullptr);
    autofill::PersonalDataManager* personalDataManager =
        autofill::PersonalDataManagerFactory::GetForProfile(profile);
    history::HistoryService* historyService =
        ios::HistoryServiceFactory::GetForProfile(
            profile, ServiceAccessType::EXPLICIT_ACCESS);
    bookmarks::BookmarkModel* bookmarkModel =
        ios::BookmarkModelFactory::GetForProfile(profile);
    ReadingListModel* readingListModel =
        ReadingListModelFactory::GetForProfile(profile);
    syncer::SyncService* syncService =
        SyncServiceFactory::GetForProfile(profile);
    PrefService* prefService = profile->GetPrefs();
    FaviconLoader* faviconLoader =
        IOSChromeFaviconLoaderFactory::GetForProfile(profile);
    /// Initialize mediator.
    _mediator = [[SafariDataImportImportMediator alloc]
        initWithSavedPasswordsPresenter:std::move(savedPasswordsPresenter)
                    paymentsDataManager:&personalDataManager
                                             ->payments_data_manager()
                         historyService:historyService
                          bookmarkModel:bookmarkModel
                       readingListModel:readingListModel
                            syncService:syncService
                            prefService:prefService
                          faviconLoader:faviconLoader];
    _mediator.importStageTransitionHandler = self;
    _mediator.itemConsumer = _tableView;
  }
  return _mediator;
}

- (UIAlertController*)errorAlert {
  if (!_errorAlert) {
    NSString* title = l10n_util::GetNSString(
        IDS_IOS_SAFARI_IMPORT_IMPORT_FAILURE_MESSAGE_TITLE);
    NSString* description = l10n_util::GetNSString(
        IDS_IOS_SAFARI_IMPORT_IMPORT_FAILURE_MESSAGE_DESCRIPTION);
    NSString* buttonText = l10n_util::GetNSString(IDS_OK);
    UIAlertAction* dismiss =
        [UIAlertAction actionWithTitle:buttonText
                                 style:UIAlertActionStyleDefault
                               handler:nil];
    _errorAlert = [UIAlertController
        alertControllerWithTitle:title
                         message:description
                  preferredStyle:UIAlertControllerStyleAlert];
    [_errorAlert addAction:dismiss];
  }
  return _errorAlert;
}

- (UIAlertController*)fileDeletionAlert {
  if (!_fileDeletionAlert) {
    NSString* title = l10n_util::GetNSStringF(
        IDS_IOS_SAFARI_IMPORT_IMPORT_FILE_DELETION_ALERT_TITLE,
        base::SysNSStringToUTF16(self.mediator.filename));
    NSString* description = l10n_util::GetNSString(
        IDS_IOS_SAFARI_IMPORT_IMPORT_FILE_DELETION_ALERT_DESCRIPTION);
    NSString* buttonTextDelete = l10n_util::GetNSString(
        IDS_IOS_SAFARI_IMPORT_IMPORT_FILE_DELETION_ALERT_ACTION_DELETE);
    NSString* buttonTextCancel = l10n_util::GetNSString(IDS_CANCEL);
    __weak __typeof(self) weakSelf = self;
    UIAlertAction* deleteAction =
        [UIAlertAction actionWithTitle:buttonTextDelete
                                 style:UIAlertActionStyleDestructive
                               handler:^(UIAlertAction* action) {
                                 [weakSelf didRespondToFileDeletionAlert:YES];
                               }];
    UIAlertAction* dismissAction =
        [UIAlertAction actionWithTitle:buttonTextCancel
                                 style:UIAlertActionStyleDefault
                               handler:^(UIAlertAction* action) {
                                 [weakSelf didRespondToFileDeletionAlert:NO];
                               }];
    _fileDeletionAlert = [UIAlertController
        alertControllerWithTitle:title
                         message:description
                  preferredStyle:UIAlertControllerStyleAlert];
    [_fileDeletionAlert addAction:deleteAction];
    [_fileDeletionAlert addAction:dismissAction];
  }
  return _fileDeletionAlert;
}

#pragma mark - PromoStyleViewControllerDelegate

- (void)didTapPrimaryActionButton {
  switch (self.importStage) {
    case SafariDataImportStage::kNotStarted:
      if ([self showFilePicker]) {
        [self transitionToNextImportStage];
      }
      break;
    case SafariDataImportStage::kFileLoading:
      NOTREACHED() << "button should be disabled";
    case SafariDataImportStage::kReadyForImport:
      [self initiateImport];
      break;
    case SafariDataImportStage::kImporting:
      NOTREACHED() << "button should be disabled";
    case SafariDataImportStage::kImported:
      [self presentViewController:self.fileDeletionAlert];
      break;
    default:
      break;
  }
}

- (void)didTapDismissButton {
  [self dismissWorkflow];
}

#pragma mark - SafariDataImportImportStageTransitionHandler

- (void)transitionToNextImportStage {
  CHECK_NE(self.importStage, SafariDataImportStage::kImported)
      << "No next import stage.";
  int nextImportStageInt = static_cast<int>(self.importStage) + 1;
  _containerViewController.email = self.mediator.email;
  _containerViewController.importStage =
      static_cast<SafariDataImportStage>(nextImportStageInt);
}

- (void)resetToInitialImportStage:(BOOL)userInitiated {
  SafariDataImportStage currentStage = self.importStage;
  CHECK_EQ(currentStage, SafariDataImportStage::kFileLoading)
      << "Not supported for stage: " << static_cast<int>(currentStage);
  /// If the user has not explicitly canceled the import, alert the user that
  /// they selected the wrong file.
  if (!userInitiated) {
    BOOL success = [self presentViewController:self.errorAlert];
    RecordSafariDataImportFailure(success);
  }
  [self.mediator reset];
  _containerViewController.importStage = SafariDataImportStage::kNotStarted;
}

#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView*)tableView
    accessoryButtonTappedForRowWithIndexPath:(NSIndexPath*)indexPath {
  CHECK_EQ(tableView, _tableView);
  RecordSafariDataImportInvalidPasswordDisplay();
  NSArray<PasswordImportItem*>* invalidPasswords =
      self.mediator.invalidPasswords;
  CHECK_GT(invalidPasswords.count, 0u);
  SafariDataInvalidPasswordsViewController* invalidPasswordsViewController =
      [[SafariDataInvalidPasswordsViewController alloc]
          initWithInvalidPasswords:invalidPasswords];
  [self presentViewController:
            [[UINavigationController alloc]
                initWithRootViewController:invalidPasswordsViewController]];
}

#pragma mark - Private

/// Displays the document picker so the user could select the Safari data file.
/// If the picker cannot be displayed currently, return NO.
- (BOOL)showFilePicker {
  UTType* zip = [UTType typeWithIdentifier:@"com.pkware.zip-archive"];
  _documentProvider = [[UIDocumentPickerViewController alloc]
      initForOpeningContentTypes:@[ zip ]];
  _documentProvider.directoryURL =
      [[NSFileManager defaultManager] URLForDirectory:NSDownloadsDirectory
                                             inDomain:NSUserDomainMask
                                    appropriateForURL:nil
                                               create:NO
                                                error:nil];
  _documentProvider.allowsMultipleSelection = NO;
  _documentProvider.delegate = self.mediator;
  _documentProvider.modalPresentationStyle =
      UIModalPresentationOverCurrentContext;
  return [self presentViewController:_documentProvider];
}

/// Initiates the import process. If there are conflicting passwords, show them
/// the modal to resolve it. Otherwise,
- (void)initiateImport {
  NSArray<PasswordImportItem*>* passwordConflicts =
      [self.mediator conflictingPasswords];
  CHECK(passwordConflicts);
  if (passwordConflicts.count == 0) {
    /// Continue to import passwords without conflict override.
    [self.mediator continueToImportPasswords:[NSArray array]];
    return;
  }
  /// Wraps the password conflict view in a navigation controller to display
  /// navigation bar and toolbar.
  SafariDataImportPasswordConflictResolutionViewController*
      conflictResolutionViewController =
          [[SafariDataImportPasswordConflictResolutionViewController alloc]
              initWithPasswordConflicts:passwordConflicts];
  conflictResolutionViewController.mutator = self.mediator;
  UINavigationController* wrapper = [[UINavigationController alloc]
      initWithRootViewController:conflictResolutionViewController];
  wrapper.toolbarHidden = NO;
  wrapper.modalInPresentation = YES;
  [self presentViewController:wrapper];
}

/// Handler for actions in `self.fileDeletionAlert`.
- (void)didRespondToFileDeletionAlert:(BOOL)willDelete {
  RecordSafariDataImportFileDeletionDecision(willDelete);
  NSError* error = nil;
  if (willDelete) {
    error = [_mediator deleteFile];
  }
  if (error) {
    __weak __typeof(self) weakSelf = self;
    NSString* buttonText = l10n_util::GetNSString(IDS_OK);
    UIAlertAction* dismiss =
        [UIAlertAction actionWithTitle:buttonText
                                 style:UIAlertActionStyleDefault
                               handler:^(UIAlertAction* action) {
                                 [weakSelf dismissWorkflow];
                               }];
    UIAlertController* failToDeleteAlert = [UIAlertController
        alertControllerWithTitle:error.localizedDescription
                         message:nil
                  preferredStyle:UIAlertControllerStyleAlert];
    [failToDeleteAlert addAction:dismiss];
    if ([self presentViewController:failToDeleteAlert]) {
      /// Dismiss the workflow when user responds to `failToDeleteAlert`.
      return;
    }
  }
  [self dismissWorkflow];
}

/// Presents `viewController` and returns `YES` if no other view controller is
/// being presented. Returns `NO` otherwise.
- (BOOL)presentViewController:(UIViewController*)viewController {
  if (_containerViewController.presentedViewController) {
    return NO;
  }
  [_containerViewController presentViewController:viewController
                                         animated:YES
                                       completion:nil];
  return YES;
}

/// Dismisses Safari import workflow.
- (void)dismissWorkflow {
  RecordSafariDataImportEndsAtImportStage(self.importStage);
  [self.delegate safariDataImportCoordinatorWillDismissWorkflow:self];
}

@end
