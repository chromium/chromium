// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safari_data_import/coordinator/safari_data_import_import_coordinator.h"

#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

#import "base/check_op.h"
#import "base/notreached.h"
#import "components/autofill/core/browser/data_manager/payments/payments_data_manager.h"
#import "components/autofill/core/browser/data_manager/personal_data_manager.h"
#import "components/bookmarks/browser/bookmark_model.h"
#import "components/history/core/browser/history_service.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/password_manager/core/browser/ui/saved_passwords_presenter.h"
#import "components/reading_list/core/reading_list_model.h"
#import "ios/chrome/browser/affiliations/model/ios_chrome_affiliation_service_factory.h"
#import "ios/chrome/browser/autofill/model/personal_data_manager_factory.h"
#import "ios/chrome/browser/bookmarks/model/bookmark_model_factory.h"
#import "ios/chrome/browser/history/model/history_service_factory.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_account_password_store_factory.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_profile_password_store_factory.h"
#import "ios/chrome/browser/reading_list/model/reading_list_model_factory.h"
#import "ios/chrome/browser/safari_data_import/coordinator/safari_data_import_child_coordinator_delegate.h"
#import "ios/chrome/browser/safari_data_import/coordinator/safari_data_import_import_mediator.h"
#import "ios/chrome/browser/safari_data_import/public/password_import_item.h"
#import "ios/chrome/browser/safari_data_import/public/safari_data_import_stage.h"
#import "ios/chrome/browser/safari_data_import/ui/safari_data_import_import_stage_transition_handler.h"
#import "ios/chrome/browser/safari_data_import/ui/safari_data_import_import_view_controller.h"
#import "ios/chrome/browser/safari_data_import/ui/safari_data_import_password_conflict_resolution_view_controller.h"
#import "ios/chrome/browser/safari_data_import/ui/safari_data_item_table_view.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/webauthn/model/ios_passkey_model_factory.h"
#import "ios/chrome/common/ui/promo_style/promo_style_view_controller_delegate.h"

@interface SafariDataImportImportCoordinator () <
    PromoStyleViewControllerDelegate,
    SafariDataImportImportStageTransitionHandler>

/// The mediator handling the interaction with the model. Lazily loaded with
/// `-mediator` method.
@property(nonatomic, readonly) SafariDataImportImportMediator* mediator;

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
  _tableView = [[SafariDataItemTableView alloc] init];
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
    /// Initialize mediator.
    _mediator = [[SafariDataImportImportMediator alloc]
        initWithSavedPasswordsPresenter:std::move(savedPasswordsPresenter)
                    paymentsDataManager:&personalDataManager
                                             ->payments_data_manager()
                         historyService:historyService
                          bookmarkModel:bookmarkModel
                       readingListModel:readingListModel];
    _mediator.importStageTransitionHandler = self;
    _mediator.itemConsumer = _tableView;
  }
  return _mediator;
}

#pragma mark - PromoStyleViewControllerDelegate

- (void)didTapPrimaryActionButton {
  /// TODO(crbug.com/420703283): Use real data from mediator.
  BOOL hasConflicts = YES;
  switch (_containerViewController.importStage) {
    case SafariDataImportStage::kNotStarted:
      [self transitionToNextImportStage];
      [self showFilePicker];
      break;
    case SafariDataImportStage::kFileLoading:
      NOTREACHED() << "button should be disabled";
    case SafariDataImportStage::kReadyForImport:
      if (hasConflicts) {
        [self showPasswordConflictResolutionModal];
      } else {
        /// TODO(crbug.com/420703283): call the mediator's import method.
      }
      break;
    case SafariDataImportStage::kImporting:
    case SafariDataImportStage::kImported:
    default:
      break;
  }
}

- (void)didTapDismissButton {
  [self.delegate safariDataImportCoordinatorWillDismissWorkflow:self];
}

#pragma mark - SafariDataImportImportStageTransitionHandler

- (void)transitionToNextImportStage {
  CHECK_NE(_containerViewController.importStage,
           SafariDataImportStage::kImported)
      << "No next import stage.";
  int nextImportStageInt =
      static_cast<int>(_containerViewController.importStage) + 1;
  _containerViewController.importStage =
      static_cast<SafariDataImportStage>(nextImportStageInt);
}

- (void)resetToInitialImportStage:(BOOL)userInitiated {
  SafariDataImportStage currentStage = _containerViewController.importStage;
  CHECK_EQ(currentStage, SafariDataImportStage::kFileLoading)
      << "Not supported for stage: " << static_cast<int>(currentStage);
  if (!userInitiated) {
    // TODO(crbug.com/420703283): Display an alert.
  }
  [self.mediator reset];
  _containerViewController.importStage = SafariDataImportStage::kNotStarted;
}

#pragma mark - Private

/// Displays the document picker so the user could select the Safari data file.
- (void)showFilePicker {
  if (_documentProvider && (_documentProvider.isBeingPresented ||
                            _documentProvider.presentingViewController)) {
    return;
  }
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
  [_containerViewController presentViewController:_documentProvider
                                         animated:YES
                                       completion:nil];
}

/// Presents the modal for the user to handle password conflicts.
- (void)showPasswordConflictResolutionModal {
  /// TODO(crbug.com/420703283): Use real data from mediator.
  NSArray<PasswordImportItem*>* passwordConflicts = @[
    [[PasswordImportItem alloc] initWithURL:@"test.org"
                                   username:@"tester"
                                   password:@"te$t"],
    [[PasswordImportItem alloc] initWithURL:@"ryanputn.am"
                                   username:@"ryanputnam"
                                   password:@"ry@npUtn@m"]
  ];
  /// Wraps the password conflict view in a navigation controller to display
  /// navigation bar and toolbar.
  UINavigationController* wrapper = [[UINavigationController alloc]
      initWithRootViewController:
          [[SafariDataImportPasswordConflictResolutionViewController alloc]
              initWithPasswordConflicts:passwordConflicts]];
  wrapper.toolbarHidden = NO;
  [_containerViewController presentViewController:wrapper
                                         animated:YES
                                       completion:nil];
}

@end
