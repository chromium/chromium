// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_manager_view_controller.h"

#import <UIKit/UIKit.h>

#import <utility>
#import <vector>

#import "base/ios/ios_util.h"
#import "base/mac/foundation_util.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/user_metrics.h"
#import "base/ranges/algorithm.h"
#import "base/strings/sys_string_conversions.h"
#import "components/google/core/common/google_util.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/password_manager/core/browser/password_list_sorter.h"
#import "components/password_manager/core/browser/password_manager_constants.h"
#import "components/password_manager/core/browser/password_manager_metrics_util.h"
#import "components/password_manager/core/browser/password_ui_utils.h"
#import "components/password_manager/core/browser/ui/affiliated_group.h"
#import "components/password_manager/core/browser/ui/credential_ui_entry.h"
#import "components/password_manager/core/browser/ui/password_check_referrer.h"
#import "components/password_manager/core/common/password_manager_features.h"
#import "components/password_manager/core/common/password_manager_pref_names.h"
#import "components/prefs/pref_service.h"
#import "components/strings/grit/components_strings.h"
#import "components/sync/base/features.h"
#import "components/sync/service/sync_service.h"
#import "components/sync/service/sync_service_utils.h"
#import "components/sync/service/sync_user_settings.h"
#import "ios/chrome/browser/net/crurl.h"
#import "ios/chrome/browser/passwords/password_checkup_metrics.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/public/features/system_flags.h"
#import "ios/chrome/browser/shared/ui/elements/home_waiting_view.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_detail_icon_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_detail_text_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_image_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_info_button_cell.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_info_button_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_link_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_switch_cell.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_switch_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_url_item.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_favicon_data_source.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_illustrated_empty_view.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_navigation_controller_constants.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service_observer_bridge.h"
#import "ios/chrome/browser/ui/settings/cells/settings_check_cell.h"
#import "ios/chrome/browser/ui/settings/cells/settings_check_item.h"
#import "ios/chrome/browser/ui/settings/elements/enterprise_info_popover_view_controller.h"
#import "ios/chrome/browser/ui/settings/password/branded_navigation_item_title_view.h"
#import "ios/chrome/browser/ui/settings/password/create_password_manager_title_view.h"
#import "ios/chrome/browser/ui/settings/password/password_manager_view_controller+private.h"
#import "ios/chrome/browser/ui/settings/password/password_manager_view_controller_delegate.h"
#import "ios/chrome/browser/ui/settings/password/password_manager_view_controller_items.h"
#import "ios/chrome/browser/ui/settings/password/password_manager_view_controller_presentation_delegate.h"
#import "ios/chrome/browser/ui/settings/password/passwords_consumer.h"
#import "ios/chrome/browser/ui/settings/password/passwords_settings_commands.h"
#import "ios/chrome/browser/ui/settings/password/passwords_table_view_constants.h"
#import "ios/chrome/browser/ui/settings/settings_root_table_view_controller+toolbar_add.h"
#import "ios/chrome/browser/ui/settings/settings_root_table_view_controller+toolbar_settings.h"
#import "ios/chrome/browser/ui/settings/settings_root_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/utils/settings_utils.h"
#import "ios/chrome/common/string_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/elements/popover_label_view_controller.h"
#import "ios/chrome/common/ui/reauthentication/reauthentication_module.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_chromium_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "net/base/mac/url_conversions.h"
#import "third_party/abseil-cpp/absl/types/optional.h"
#import "ui/base/device_form_factor.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::UmaHistogramEnumeration;
using password_manager::features::IsPasswordCheckupEnabled;
using password_manager::metrics_util::PasswordCheckInteraction;

namespace {

// Height of empty footer below the manage account header.
// This ammount added to the internal padding of the manage account header (8pt)
// and the height of the empty header of the next section (10pt) achieves the
// desired vertical spacing (20pt) between the manager account header's text and
// the first item of the next section.
constexpr CGFloat kManageAccountHeaderSectionFooterHeight = 2;

typedef NS_ENUM(NSInteger, ItemType) {
  // Section: SectionIdentifierManageAccountHeader
  ItemTypeLinkHeader = kItemTypeEnumZero,
  // Section: SectionIdentifierPasswordCheck
  ItemTypePasswordCheckStatus,
  ItemTypeCheckForProblemsButton,
  ItemTypeLastCheckTimestampFooter,
  // Section: SectionIdentifierSavedPasswords
  ItemTypeHeader,
  ItemTypeSavedPassword,  // This is a repeated item type.
  // Section: SectionIdentifierBlocked
  ItemTypeBlocked,  // This is a repeated item type.
  // Section: SectionIdentifierAddPasswordButton
  ItemTypeAddPasswordButton,
};

// Return if the feature flag for the password grouping is enabled.
// TODO(crbug.com/1359392): Remove this when kPasswordsGrouping flag is removed.
bool IsPasswordGroupingEnabled() {
  return base::FeatureList::IsEnabled(
      password_manager::features::kPasswordsGrouping);
}

bool IsPasswordNotesWithBackupEnabled() {
  return base::FeatureList::IsEnabled(syncer::kPasswordNotesWithBackup);
}

// Helper method to determine whether the Password Check cell is tappable or
// not.
bool IsPasswordCheckTappable(PasswordCheckUIState passwordCheckState) {
  switch (passwordCheckState) {
    case PasswordCheckStateUnmutedCompromisedPasswords:
      return true;
    case PasswordCheckStateReusedPasswords:
    case PasswordCheckStateWeakPasswords:
    case PasswordCheckStateDismissedWarnings:
    case PasswordCheckStateSafe:
      return IsPasswordCheckupEnabled();
    case PasswordCheckStateDefault:
    case PasswordCheckStateRunning:
    case PasswordCheckStateDisabled:
    case PasswordCheckStateError:
    case PasswordCheckStateSignedOut:
      return false;
  }
}

// TODO(crbug.com/1426463): Remove when CredentialUIEntry operator== is fixed.
template <typename T>
bool AreNotesEqual(const T& lhs, const T& rhs) {
  return base::ranges::equal(lhs, rhs, {},
                             &password_manager::CredentialUIEntry::note,
                             &password_manager::CredentialUIEntry::note);
}

bool AreNotesEqual(const std::vector<password_manager::AffiliatedGroup>& lhs,
                   const std::vector<password_manager::AffiliatedGroup>& rhs) {
  return base::ranges::equal(
      lhs, rhs,
      AreNotesEqual<base::span<const password_manager::CredentialUIEntry>>,
      &password_manager::AffiliatedGroup::GetCredentials,
      &password_manager::AffiliatedGroup::GetCredentials);
}

template <typename T>
bool AreStoresEqual(const T& lhs, const T& rhs) {
  return base::ranges::equal(lhs, rhs, {},
                             &password_manager::CredentialUIEntry::stored_in,
                             &password_manager::CredentialUIEntry::stored_in);
}

bool AreStoresEqual(const std::vector<password_manager::AffiliatedGroup>& lhs,
                    const std::vector<password_manager::AffiliatedGroup>& rhs) {
  return base::ranges::equal(
      lhs, rhs,
      AreStoresEqual<base::span<const password_manager::CredentialUIEntry>>,
      &password_manager::AffiliatedGroup::GetCredentials,
      &password_manager::AffiliatedGroup::GetCredentials);
}

template <typename T>
bool AreIssuesEqual(const T& lhs, const T& rhs) {
  return base::ranges::equal(
      lhs, rhs, {}, &password_manager::CredentialUIEntry::password_issues,
      &password_manager::CredentialUIEntry::password_issues);
}

bool AreIssuesEqual(const std::vector<password_manager::AffiliatedGroup>& lhs,
                    const std::vector<password_manager::AffiliatedGroup>& rhs) {
  return base::ranges::equal(
      lhs, rhs,
      AreIssuesEqual<base::span<const password_manager::CredentialUIEntry>>,
      &password_manager::AffiliatedGroup::GetCredentials,
      &password_manager::AffiliatedGroup::GetCredentials);
}

}  // namespace

@interface PasswordManagerViewController () <
    ChromeAccountManagerServiceObserver,
    PopoverLabelViewControllerDelegate,
    TableViewIllustratedEmptyViewDelegate> {
  // The header for save passwords switch section.
  TableViewLinkHeaderFooterItem* _manageAccountLinkItem;
  // The item related to the password check status.
  SettingsCheckItem* _passwordProblemsItem;
  // The button to start password check.
  TableViewTextItem* _checkForProblemsItem;
  // The button to add a password.
  TableViewTextItem* _addPasswordItem;
  // The list of the user's saved passwords.
  std::vector<password_manager::CredentialUIEntry> _passwords;
  // Boolean indicating that passwords are being saved in an account if YES,
  // and locally if NO.
  BOOL _savingPasswordsToAccount;
  // The list of the user's blocked sites.
  std::vector<password_manager::CredentialUIEntry> _blockedSites;
  // The list of the user's saved grouped passwords.
  std::vector<password_manager::AffiliatedGroup> _affiliatedGroups;
  // AcountManagerService Observer.
  std::unique_ptr<ChromeAccountManagerServiceObserverBridge>
      _accountManagerServiceObserver;
  // Boolean indicating if password forms have been received for the first time.
  // Used to show a loading indicator while waiting for the store response.
  BOOL _didReceivePasswords;
  // Whether the table view is in search mode. That is, it only has the search
  // bar potentially saved passwords and blocked sites.
  BOOL _tableIsInSearchMode;
  // Whether the favicon metric was already logged.
  BOOL _faviconMetricLogged;
}

// Current passwords search term.
@property(nonatomic, copy) NSString* searchTerm;

// The scrim view that covers the table view when search bar is focused with
// empty search term. Tapping on the scrim view will dismiss the search bar.
@property(nonatomic, strong) UIControl* scrimView;

// The loading spinner background which appears when loading passwords.
@property(nonatomic, strong) HomeWaitingView* spinnerView;

// Current state of the Password Check.
@property(nonatomic, assign) PasswordCheckUIState passwordCheckState;

// Number of insecure passwords.
@property(assign) NSInteger insecurePasswordsCount;

// Stores the most recently created or updated Affiliated Group.
@property(nonatomic, assign) absl::optional<password_manager::AffiliatedGroup>
    mostRecentlyUpdatedAffiliatedGroup;

// Stores the most recently created or updated password form.
@property(nonatomic, assign) absl::optional<password_manager::CredentialUIEntry>
    mostRecentlyUpdatedPassword;

// Stores the item which has form attribute's username and site equivalent to
// that of `mostRecentlyUpdatedPassword`.
@property(nonatomic, weak) TableViewItem* mostRecentlyUpdatedItem;

// YES, if the user triggered a password check by tapping on the "Check Now"
// button.
@property(nonatomic, assign) BOOL checkWasTriggeredManually;

// Return YES if the search bar should be enabled.
@property(nonatomic, assign) BOOL shouldEnableSearchBar;

// The search controller used in this view. This may be added/removed from the
// navigation controller, but the instance will persist here.
@property(nonatomic, strong) UISearchController* searchController;

// Settings button for the toolbar.
@property(nonatomic, strong) UIBarButtonItem* settingsButtonInToolbar;

// Add button for the toolbar.
@property(nonatomic, strong) UIBarButtonItem* addButtonInToolbar;

// Indicates whether the check button should be shown or not. Used when
// kIOSPasswordCheckup feature is enabled.
@property(nonatomic, assign) BOOL shouldShowCheckButton;

// The PrefService passed to this instance.
@property(nonatomic, assign) PrefService* prefService;

@end

@implementation PasswordManagerViewController

#pragma mark - Initialization

- (instancetype)initWithChromeAccountManagerService:
                    (ChromeAccountManagerService*)accountManagerService
                                        prefService:(PrefService*)prefService {
  self = [super initWithStyle:ChromeTableViewStyle()];
  if (self) {
    _prefService = prefService;
    _accountManagerServiceObserver =
        std::make_unique<ChromeAccountManagerServiceObserverBridge>(
            self, accountManagerService);

    self.shouldDisableDoneButtonOnEdit = YES;
    self.searchTerm = @"";

    // Default behavior: search bar is enabled.
    self.shouldEnableSearchBar = YES;

    [self updateUIForEditState];
  }
  return self;
}

- (void)dealloc {
  DCHECK(!_accountManagerServiceObserver.get());
}

- (void)setReauthenticationModule:
    (ReauthenticationModule*)reauthenticationModule {
  _reauthenticationModule = reauthenticationModule;
}

// TODO(crbug.com/1358978): Receive AffiliatedGroup object instead of a
// CredentialUIEntry. Store into mostRecentlyUpdatedAffiliatedGroup.
- (void)setMostRecentlyUpdatedPasswordDetails:
    (const password_manager::CredentialUIEntry&)credential {
  self.mostRecentlyUpdatedPassword = credential;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  [self setUpTitle];

  self.tableView.allowsMultipleSelectionDuringEditing = YES;
  self.tableView.accessibilityIdentifier = kPasswordsTableViewId;

  // With no header on first appearance, UITableView adds a 35 points space at
  // the beginning of the table view. This space remains after this table view
  // reloads with headers. Setting a small tableHeaderView avoids this.
  self.tableView.tableHeaderView =
      [[UIView alloc] initWithFrame:CGRectMake(0, 0, 0, CGFLOAT_MIN)];

  // SearchController Configuration.
  // Init the searchController with nil so the results are displayed on the same
  // TableView.
  UISearchController* searchController =
      [[UISearchController alloc] initWithSearchResultsController:nil];
  self.searchController = searchController;

  searchController.obscuresBackgroundDuringPresentation = NO;
  searchController.delegate = self;

  UISearchBar* searchBar = searchController.searchBar;
  searchBar.delegate = self;
  searchBar.backgroundColor = UIColor.clearColor;
  searchBar.accessibilityIdentifier = kPasswordsSearchBarId;

  // TODO(crbug.com/1268684): Explicitly set the background color for the search
  // bar to match with the color of navigation bar in iOS 13/14 to work around
  // an iOS issue.

  // UIKit needs to know which controller will be presenting the
  // searchController. If we don't add this trying to dismiss while
  // SearchController is active will fail.
  self.definesPresentationContext = YES;

  // Place the search bar in the navigation bar.
  self.navigationItem.searchController = searchController;
  self.navigationItem.hidesSearchBarWhenScrolling = NO;

  self.scrimView = [[UIControl alloc] init];
  self.scrimView.alpha = 0.0f;
  self.scrimView.backgroundColor = [UIColor colorNamed:kScrimBackgroundColor];
  self.scrimView.translatesAutoresizingMaskIntoConstraints = NO;
  self.scrimView.accessibilityIdentifier = kPasswordsScrimViewId;
  [self.scrimView addTarget:self
                     action:@selector(dismissSearchController:)
           forControlEvents:UIControlEventTouchUpInside];

  [self loadModel];

  if (!_didReceivePasswords) {
    [self showLoadingSpinnerBackground];
  }
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];

  self.navigationController.toolbarHidden = NO;
}

- (void)viewWillDisappear:(BOOL)animated {
  [super viewWillDisappear:animated];

  // viewWillDisappear is also called if you drag the sheet down then release
  // without actually closing.
  if (!_faviconMetricLogged) {
    [self logMetricsForFavicons];
    _faviconMetricLogged = YES;
  }

  // Dismiss the search bar if presented; otherwise UIKit may retain it and
  // cause a memory leak. If this dismissal happens before viewWillDisappear
  // (e.g., settingsWillBeDismissed) an internal UIKit crash occurs. See also:
  // crbug.com/947417, crbug.com/1350625.
  if (self.navigationItem.searchController.active == YES) {
    self.navigationItem.searchController.active = NO;
  }
}

- (void)didMoveToParentViewController:(UIViewController*)parent {
  [super didMoveToParentViewController:parent];
  if (!parent) {
    [self.presentationDelegate PasswordManagerViewControllerDismissed];
  }
}

- (void)setEditing:(BOOL)editing animated:(BOOL)animated {
  [super setEditing:editing animated:animated];
  [self setAddPasswordButtonEnabled:!editing];
  [self setSearchBarEnabled:self.shouldEnableSearchBar];
  [self updatePasswordCheckButtonWithState:self.passwordCheckState];
  [self updatePasswordCheckStatusLabelWithState:self.passwordCheckState];
  [self updatePasswordCheckSectionWithState:self.passwordCheckState];
  [self updateUIForEditState];
}

- (BOOL)hasPasswords {
  if (IsPasswordGroupingEnabled()) {
    return !_affiliatedGroups.empty();
  }
  return !_passwords.empty();
}

#pragma mark - SettingsRootTableViewController

- (void)loadModel {
  [super loadModel];

  if (!_didReceivePasswords) {
    return;
  }

  [self showOrHideEmptyView];

  // If we don't have data or settings to show, add an empty state, then
  // stop so that we don't add anything that overlaps the illustrated
  // background.
  if ([self shouldShowEmptyStateView]) {
    return;
  }

  TableViewModel* model = self.tableViewModel;

  if (!_manageAccountLinkItem) {
    _manageAccountLinkItem = [self manageAccountLinkItem];
  }

  // Don't show sections hidden when search controller is displayed.
  if (!_tableIsInSearchMode) {
    // Manage account header.
    [model addSectionWithIdentifier:SectionIdentifierManageAccountHeader];
    [model setHeader:_manageAccountLinkItem
        forSectionWithIdentifier:SectionIdentifierManageAccountHeader];

    // Password check.
    [model addSectionWithIdentifier:SectionIdentifierPasswordCheck];
    if (!_passwordProblemsItem) {
      _passwordProblemsItem = [self passwordProblemsItem];
    }

    [self updatePasswordCheckStatusLabelWithState:_passwordCheckState];
    [model addItem:_passwordProblemsItem
        toSectionWithIdentifier:SectionIdentifierPasswordCheck];

    if (!_checkForProblemsItem) {
      _checkForProblemsItem = [self checkForProblemsItem];
    }

    [self updatePasswordCheckButtonWithState:_passwordCheckState];

    // Only add check button if kIOSPasswordCheckup is disabled, or if it is
    // enabled and the current PasswordCheckUIState requires the button to be
    // shown.
    if (!IsPasswordCheckupEnabled() || self.shouldShowCheckButton) {
      [model addItem:_checkForProblemsItem
          toSectionWithIdentifier:SectionIdentifierPasswordCheck];
    }

    // When the Password Checkup feature is enabled, this timestamp only appears
    // in the detail text of the Password Checkup status cell. It is therefore
    // managed in `updatePasswordCheckStatusLabelWithState`.
    if (!IsPasswordCheckupEnabled()) {
      [self updateLastCheckTimestampWithState:_passwordCheckState
                                    fromState:_passwordCheckState
                                       update:NO];
    }

    // Add Password button.
    if ([self allowsAddPassword]) {
      [model addSectionWithIdentifier:SectionIdentifierAddPasswordButton];
      _addPasswordItem = [self addPasswordItem];
      [model addItem:_addPasswordItem
          toSectionWithIdentifier:SectionIdentifierAddPasswordButton];
    }
  }

  // Saved passwords.
  if ([self hasPasswords]) {
    [model addSectionWithIdentifier:SectionIdentifierSavedPasswords];
    TableViewTextHeaderFooterItem* headerItem =
        [[TableViewTextHeaderFooterItem alloc] initWithType:ItemTypeHeader];
    headerItem.text =
        l10n_util::GetNSString(IDS_IOS_SETTINGS_PASSWORDS_SAVED_HEADING);
    [model setHeader:headerItem
        forSectionWithIdentifier:SectionIdentifierSavedPasswords];
  }

  // Blocked passwords.
  if (!_blockedSites.empty()) {
    [model addSectionWithIdentifier:SectionIdentifierBlocked];
    TableViewTextHeaderFooterItem* headerItem =
        [[TableViewTextHeaderFooterItem alloc] initWithType:ItemTypeHeader];
    headerItem.text =
        l10n_util::GetNSString(IDS_IOS_SETTINGS_PASSWORDS_EXCEPTIONS_HEADING);
    [model setHeader:headerItem
        forSectionWithIdentifier:SectionIdentifierBlocked];
  }

  [self filterItems:self.searchTerm];
}

// Returns YES if the array of index path contains a saved password. This is to
// determine if we need to show the user the alert dialog.
- (BOOL)indexPathsContainsSavedPassword:(NSArray<NSIndexPath*>*)indexPaths {
  for (NSIndexPath* indexPath : indexPaths) {
    if ([self.tableViewModel itemTypeForIndexPath:indexPath] ==
        ItemTypeSavedPassword) {
      return YES;
    }
  }
  return NO;
}

- (void)deleteItems:(NSArray<NSIndexPath*>*)indexPaths {
  // Only show the user the alert dialog if the index path array contain at
  // least one saved password.
  if (IsPasswordGroupingEnabled() &&
      [self indexPathsContainsSavedPassword:indexPaths]) {
    // Show password delete dialog before deleting the passwords.
    NSMutableArray<NSString*>* origins = [[NSMutableArray alloc] init];
    for (NSIndexPath* indexPath : indexPaths) {
      NSInteger itemType = [self.tableViewModel itemTypeForIndexPath:indexPath];
      if (itemType == ItemTypeSavedPassword) {
        password_manager::AffiliatedGroup affiliatedGroup =
            base::mac::ObjCCastStrict<AffiliatedGroupTableViewItem>(
                [self.tableViewModel itemAtIndexPath:indexPath])
                .affiliatedGroup;
        [origins addObject:base::SysUTF8ToNSString(
                               affiliatedGroup.GetDisplayName())];
      }
    }
    [self.handler
        showPasswordDeleteDialogWithOrigins:origins
                                 completion:^{
                                   [self deleteItemAtIndexPaths:indexPaths];
                                 }];
  } else {
    // Do not call super as this also deletes the section if it is empty.
    [self deleteItemAtIndexPaths:indexPaths];
  }
}

- (BOOL)editButtonEnabled {
  return [self hasPasswords] || !_blockedSites.empty();
}

- (BOOL)shouldHideToolbar {
  return NO;
}

- (BOOL)shouldShowEditDoneButton {
  // The "Done" button in the navigation bar closes the sheet.
  return NO;
}

- (void)updateUIForEditState {
  [super updateUIForEditState];
  [self updatedToolbarForEditState];
}

- (void)editButtonPressed {
  // Disable search bar if the user is bulk editing (edit mode). (Reverse logic
  // because parent method -editButtonPressed is calling setEditing to change
  // the state).
  self.shouldEnableSearchBar = self.tableView.editing;
  [super editButtonPressed];
}

- (UIBarButtonItem*)customLeftToolbarButton {
  return self.tableView.isEditing ? nil : self.settingsButtonInToolbar;
}

- (UIBarButtonItem*)customRightToolbarButton {
  if (!self.tableView.isEditing) {
    // Display Add button on the right side of the toolbar when the empty state
    // is displayed. The Settings button will be on the left. When the tableView
    // is not empty, the Add button is displayed in a row.
    if ([self shouldShowEmptyStateView]) {
      return self.addButtonInToolbar;
    }
  }
  return nil;
}

#pragma mark - SettingsControllerProtocol

- (void)reportDismissalUserAction {
  base::RecordAction(base::UserMetricsAction("MobilePasswordsSettingsClose"));
  _accountManagerServiceObserver.reset();
}

- (void)reportBackUserAction {
  base::RecordAction(base::UserMetricsAction("MobilePasswordsSettingsBack"));
  _accountManagerServiceObserver.reset();
}

- (void)settingsWillBeDismissed {
  _accountManagerServiceObserver.reset();
  self.prefService = nullptr;
}

#pragma mark - Items
- (TableViewLinkHeaderFooterItem*)manageAccountLinkItem {
  TableViewLinkHeaderFooterItem* header =
      [[TableViewLinkHeaderFooterItem alloc] initWithType:ItemTypeLinkHeader];

  if (_savingPasswordsToAccount) {
    header.text =
        l10n_util::GetNSString(IDS_IOS_SAVE_PASSWORDS_MANAGE_ACCOUNT_HEADER);

    header.urls = @[ [[CrURL alloc]
        initWithGURL:
            google_util::AppendGoogleLocaleParam(
                GURL(password_manager::kPasswordManagerHelpCenteriOSURL),
                GetApplicationContext()->GetApplicationLocale())] ];
  } else {
    header.text =
        l10n_util::GetNSString(IDS_IOS_PASSWORD_MANAGER_HEADER_NOT_SYNCING);
    header.urls = @[];
  }

  return header;
}

- (SettingsCheckItem*)passwordProblemsItem {
  SettingsCheckItem* passwordProblemsItem =
      [[SettingsCheckItem alloc] initWithType:ItemTypePasswordCheckStatus];
  passwordProblemsItem.enabled = NO;
  passwordProblemsItem.text =
      IsPasswordCheckupEnabled()
          ? l10n_util::GetNSString(IDS_IOS_PASSWORD_CHECKUP)
          : l10n_util::GetNSString(IDS_IOS_CHECK_PASSWORDS);
  passwordProblemsItem.detailText =
      IsPasswordCheckupEnabled()
          ? l10n_util::GetNSString(IDS_IOS_PASSWORD_CHECKUP_DESCRIPTION)
          : l10n_util::GetNSString(IDS_IOS_CHECK_PASSWORDS_DESCRIPTION);
  passwordProblemsItem.accessibilityTraits = UIAccessibilityTraitHeader;
  return passwordProblemsItem;
}

- (TableViewTextItem*)checkForProblemsItem {
  TableViewTextItem* checkForProblemsItem =
      [[TableViewTextItem alloc] initWithType:ItemTypeCheckForProblemsButton];
  checkForProblemsItem.text =
      l10n_util::GetNSString(IDS_IOS_CHECK_PASSWORDS_NOW_BUTTON);
  checkForProblemsItem.textColor = [UIColor colorNamed:kTextSecondaryColor];
  checkForProblemsItem.accessibilityTraits = UIAccessibilityTraitButton;
  return checkForProblemsItem;
}

- (TableViewLinkHeaderFooterItem*)lastCompletedCheckTime {
  TableViewLinkHeaderFooterItem* footerItem =
      [[TableViewLinkHeaderFooterItem alloc]
          initWithType:ItemTypeLastCheckTimestampFooter];
  footerItem.text = [self.delegate formattedElapsedTimeSinceLastCheck];
  return footerItem;
}

- (TableViewTextItem*)addPasswordItem {
  TableViewTextItem* addPasswordItem =
      [[TableViewTextItem alloc] initWithType:ItemTypeAddPasswordButton];
  addPasswordItem.text = l10n_util::GetNSString(IDS_IOS_ADD_PASSWORD);
  addPasswordItem.accessibilityIdentifier = kAddPasswordButtonId;
  addPasswordItem.accessibilityTraits = UIAccessibilityTraitButton;
  addPasswordItem.textColor = [UIColor colorNamed:kBlueColor];
  return addPasswordItem;
}

- (CredentialTableViewItem*)savedFormItemForCredential:
    (const password_manager::CredentialUIEntry&)credential {
  CredentialTableViewItem* passwordItem =
      [[CredentialTableViewItem alloc] initWithType:ItemTypeSavedPassword];
  passwordItem.credential = credential;
  passwordItem.showLocalOnlyIcon =
      [self.delegate shouldShowLocalOnlyIconForCredential:credential];
  passwordItem.accessibilityTraits |= UIAccessibilityTraitButton;
  passwordItem.accessoryType = UITableViewCellAccessoryDisclosureIndicator;
  if (self.mostRecentlyUpdatedPassword) {
    if (self.mostRecentlyUpdatedPassword->username == credential.username &&
        self.mostRecentlyUpdatedPassword->GetFirstSignonRealm() ==
            credential.GetFirstSignonRealm()) {
      self.mostRecentlyUpdatedItem = passwordItem;
      self.mostRecentlyUpdatedPassword = absl::nullopt;
    }
  }
  return passwordItem;
}

- (AffiliatedGroupTableViewItem*)savedFormItemForAffiliatedGroup:
    (const password_manager::AffiliatedGroup&)affiliatedGroup {
  AffiliatedGroupTableViewItem* passwordItem =
      [[AffiliatedGroupTableViewItem alloc] initWithType:ItemTypeSavedPassword];
  passwordItem.affiliatedGroup = affiliatedGroup;
  passwordItem.showLocalOnlyIcon =
      [self.delegate shouldShowLocalOnlyIconForGroup:affiliatedGroup];
  passwordItem.accessibilityTraits |= UIAccessibilityTraitButton;
  passwordItem.accessoryType = UITableViewCellAccessoryDisclosureIndicator;

  if (self.mostRecentlyUpdatedAffiliatedGroup) {
    if (self.mostRecentlyUpdatedAffiliatedGroup->GetDisplayName() ==
        affiliatedGroup.GetDisplayName()) {
      self.mostRecentlyUpdatedItem = passwordItem;
      self.mostRecentlyUpdatedAffiliatedGroup = absl::nullopt;
    }
  }
  return passwordItem;
}

- (CredentialTableViewItem*)blockedSiteItem:
    (const password_manager::CredentialUIEntry&)credential {
  CredentialTableViewItem* passwordItem =
      [[CredentialTableViewItem alloc] initWithType:ItemTypeBlocked];
  passwordItem.credential = credential;
  passwordItem.showLocalOnlyIcon =
      [self.delegate shouldShowLocalOnlyIconForCredential:credential];
  passwordItem.accessibilityTraits |= UIAccessibilityTraitButton;
  passwordItem.accessoryType = UITableViewCellAccessoryDisclosureIndicator;
  return passwordItem;
}

#pragma mark - PopoverLabelViewControllerDelegate

- (void)didTapLinkURL:(NSURL*)URL {
  [self view:nil didTapLinkURL:[[CrURL alloc] initWithNSURL:URL]];
}

#pragma mark - Actions

// Called when the user clicks on the information button of the managed
// setting's UI. Shows a textual bubble with the information of the enterprise.
- (void)didTapManagedUIInfoButton:(UIButton*)buttonView {
  EnterpriseInfoPopoverViewController* bubbleViewController =
      [[EnterpriseInfoPopoverViewController alloc] initWithEnterpriseName:nil];
  bubbleViewController.delegate = self;
  [self presentViewController:bubbleViewController animated:YES completion:nil];

  // Disable the button when showing the bubble.
  buttonView.enabled = NO;

  // Set the anchor and arrow direction of the bubble.
  bubbleViewController.popoverPresentationController.sourceView = buttonView;
  bubbleViewController.popoverPresentationController.sourceRect =
      buttonView.bounds;
  bubbleViewController.popoverPresentationController.permittedArrowDirections =
      UIPopoverArrowDirectionAny;
}

// Called when user tapped on the information button of the password check
// item. Shows popover with detailed description of an error.
- (void)didTapPasswordCheckInfoButton:(UIButton*)buttonView {
  NSAttributedString* info = [self.delegate passwordCheckErrorInfo];
  // If no info returned by mediator handle this tap as tap on a cell.
  if (!info) {
    IsPasswordCheckupEnabled() ? [self showPasswordCheckupPage]
                               : [self showPasswordIssuesPage];
    return;
  }

  PopoverLabelViewController* errorInfoPopover =
      [[PopoverLabelViewController alloc] initWithPrimaryAttributedString:info
                                                secondaryAttributedString:nil];
  errorInfoPopover.delegate = self;

  errorInfoPopover.popoverPresentationController.sourceView = buttonView;
  errorInfoPopover.popoverPresentationController.sourceRect = buttonView.bounds;
  errorInfoPopover.popoverPresentationController.permittedArrowDirections =
      UIPopoverArrowDirectionAny;
  [self presentViewController:errorInfoPopover animated:YES completion:nil];
}

#pragma mark - PasswordsConsumer

- (void)setPasswordCheckUIState:(PasswordCheckUIState)state
         insecurePasswordsCount:(NSInteger)insecureCount {
  self.insecurePasswordsCount = insecureCount;
  // Update password check status and check button with new state.
  [self updatePasswordCheckButtonWithState:state];
  [self updatePasswordCheckStatusLabelWithState:state];

  // During searching Password Check section is hidden so cells should not be
  // reconfigured.
  if (_tableIsInSearchMode) {
    _passwordCheckState = state;
    return;
  }

  [self updatePasswordCheckSectionWithState:state];

  // When the Password Checkup feature is enabled, this timestamp only appears
  // in the detail text of the Password Checkup status cell. It is therefore
  // managed in `updatePasswordCheckStatusLabelWithState`.
  if (!IsPasswordCheckupEnabled()) {
    // Before updating cached state value update timestamp as for proper
    // animation it requires both new and old values.
    [self updateLastCheckTimestampWithState:state
                                  fromState:_passwordCheckState
                                     update:YES];
  }

  _passwordCheckState = state;
}

// TODO(crbug.com/1359392): Remove this.
- (void)setPasswords:(std::vector<password_manager::CredentialUIEntry>)passwords
        blockedSites:
            (std::vector<password_manager::CredentialUIEntry>)blockedSites {
  if (!_didReceivePasswords) {
    _blockedSites = std::move(blockedSites);
    _passwords = std::move(passwords);
    [self hideLoadingSpinnerBackground];
  } else {
    // The CredentialUIEntry equality operator ignores the password stores, but
    // this UI cares, c.f. password_manager::ShouldShowLocalOnlyIcon().
    // The CredentialUIEntry equality operator ignores password notes, but the
    // UI should be updated so that any changes to just notes are visible.
    if (_passwords == passwords && _blockedSites == blockedSites &&
        AreStoresEqual(_passwords, passwords) &&
        AreIssuesEqual(_passwords, passwords) &&
        AreNotesEqual(_passwords, passwords)) {
      return;
    }

    _blockedSites = std::move(blockedSites);
    _passwords = std::move(passwords);

    [self updatePasswordManagerUI];
  }
}

- (void)setSavingPasswordsToAccount:(BOOL)savingPasswordsToAccount {
  if (_savingPasswordsToAccount == savingPasswordsToAccount) {
    return;
  }
  _savingPasswordsToAccount = savingPasswordsToAccount;
  [self reloadData];
}

- (void)setAffiliatedGroups:
            (const std::vector<password_manager::AffiliatedGroup>&)
                affiliatedGroups
               blockedSites:
                   (const std::vector<password_manager::CredentialUIEntry>&)
                       blockedSites {
  DCHECK(IsPasswordGroupingEnabled());
  if (!_didReceivePasswords) {
    _blockedSites = blockedSites;
    _affiliatedGroups = affiliatedGroups;
    [self hideLoadingSpinnerBackground];
  } else {
    // The AffiliatedGroup equality operator ignores the password stores, but
    // this UI cares, see password_manager::ShouldShowLocalOnlyIcon().
    // The AffiliatedGroup equality operator ignores password notes, but the UI
    // should be updated so that any changes to just notes are visible.
    if (_affiliatedGroups == affiliatedGroups &&
        _blockedSites == blockedSites &&
        AreStoresEqual(_affiliatedGroups, affiliatedGroups) &&
        AreIssuesEqual(_affiliatedGroups, affiliatedGroups) &&
        AreNotesEqual(_affiliatedGroups, affiliatedGroups)) {
      return;
    }

    _blockedSites = blockedSites;
    _affiliatedGroups = affiliatedGroups;

    [self updatePasswordManagerUI];
  }
}

- (void)updatePasswordManagerUI {
  if ([self shouldShowEmptyStateView]) {
    [self setEditing:NO animated:YES];
    [self reloadData];
    return;
  }

  TableViewModel* model = self.tableViewModel;
  NSMutableIndexSet* sectionIdentifiersToUpdate = [NSMutableIndexSet indexSet];

  // Hold in reverse order of section indexes (bottom up of section
  // displayed). If we don't we'll cause a crash.
  std::vector<PasswordSectionIdentifier> sections = {
      SectionIdentifierBlocked, SectionIdentifierSavedPasswords};

  for (const auto& section : sections) {
    bool hasSection = [model hasSectionForSectionIdentifier:section];
    bool needsSection = section == SectionIdentifierBlocked
                            ? !_blockedSites.empty()
                            : [self hasPasswords];

    // If section exists but it shouldn't - gracefully remove it with
    // animation.
    if (!needsSection && hasSection) {
      [self clearSectionWithIdentifier:section
                      withRowAnimation:UITableViewRowAnimationAutomatic];
    }
    // If section exists and it should - reload it.
    else if (needsSection && hasSection) {
      [sectionIdentifiersToUpdate addIndex:section];
    }
    // If section doesn't exist but it should - add it.
    else if (needsSection && !hasSection) {
      // This is very rare condition, in this case just reload all data.
      [self updateUIForEditState];
      [self reloadData];
      return;
    }
  }

  // After deleting any sections, calculate the indices of sections to be
  // updated. Doing this before deleting sections will lead to incorrect indices
  // and possible crashes.
  NSMutableIndexSet* sectionsToUpdate = [NSMutableIndexSet indexSet];
  [sectionIdentifiersToUpdate
      enumerateIndexesUsingBlock:^(NSUInteger sectionIdentifier, BOOL* stop) {
        [sectionsToUpdate
            addIndex:[model sectionForSectionIdentifier:sectionIdentifier]];
      }];

  // Reload items in sections.
  if (sectionsToUpdate.count > 0) {
    [self filterItems:self.searchTerm];
    [self.tableView reloadSections:sectionsToUpdate
                  withRowAnimation:UITableViewRowAnimationAutomatic];
    [self scrollToLastUpdatedItem];
  } else if (_affiliatedGroups.empty() && _blockedSites.empty()) {
    [self setEditing:NO animated:YES];
  }
}

#pragma mark - UISearchControllerDelegate

- (void)willPresentSearchController:(UISearchController*)searchController {
  // This is needed to remove the transparency of the navigation bar at scroll
  // edge in iOS 15+ to prevent the following UITableViewRowAnimationTop
  // animations from being visible through the navigation bar.
  self.navigationController.navigationBar.backgroundColor =
      [UIColor colorNamed:kGroupedPrimaryBackgroundColor];

  [self showScrim];
  // Remove save passwords switch section, password check section and
  // on device encryption.

  _tableIsInSearchMode = YES;
  [self
      performBatchTableViewUpdates:^{
        // Sections must be removed from bottom to top, otherwise it crashes
        [self clearSectionWithIdentifier:SectionIdentifierAddPasswordButton
                        withRowAnimation:UITableViewRowAnimationTop];

        [self clearSectionWithIdentifier:SectionIdentifierPasswordCheck
                        withRowAnimation:UITableViewRowAnimationTop];

        [self clearSectionWithIdentifier:SectionIdentifierManageAccountHeader
                        withRowAnimation:UITableViewRowAnimationTop];

        // Hide the toolbar when the search controller is presented.
        self.navigationController.toolbarHidden = YES;
      }
                        completion:nil];
}

- (void)willDismissSearchController:(UISearchController*)searchController {
  // This is needed to restore the transparency of the navigation bar at scroll
  // edge in iOS 15+.
  self.navigationController.navigationBar.backgroundColor = nil;

  // No need to restore UI if the Password Manager is being dismissed or if a
  // previous call to `willDismissSearchController` already restored the UI.
  if (self.navigationController.isBeingDismissed || !_tableIsInSearchMode) {
    return;
  }

  [self hideScrim];
  [self searchForTerm:@""];
  // Recover save passwords switch section.
  TableViewModel* model = self.tableViewModel;
  [self.tableView
      performBatchUpdates:^{
        int sectionIndex = 0;
        NSMutableArray<NSIndexPath*>* rowsIndexPaths =
            [[NSMutableArray alloc] init];

        // Add manage account header.
        [model insertSectionWithIdentifier:SectionIdentifierManageAccountHeader
                                   atIndex:sectionIndex];
        [model setHeader:_manageAccountLinkItem
            forSectionWithIdentifier:SectionIdentifierManageAccountHeader];
        [self.tableView
              insertSections:[NSIndexSet indexSetWithIndex:sectionIndex]
            withRowAnimation:UITableViewRowAnimationTop];

        sectionIndex++;

        // Add "Password check" section.
        [model insertSectionWithIdentifier:SectionIdentifierPasswordCheck
                                   atIndex:sectionIndex];
        NSInteger checkSection =
            [model sectionForSectionIdentifier:SectionIdentifierPasswordCheck];

        [self.tableView
              insertSections:[NSIndexSet indexSetWithIndex:sectionIndex]
            withRowAnimation:UITableViewRowAnimationTop];
        [model addItem:_passwordProblemsItem
            toSectionWithIdentifier:SectionIdentifierPasswordCheck];
        [rowsIndexPaths addObject:[NSIndexPath indexPathForRow:0
                                                     inSection:checkSection]];
        // Only add check button if kIOSPasswordCheckup is disabled, or if it is
        // enabled and the current PasswordCheckUIState requires the button to
        // be shown.
        if (!IsPasswordCheckupEnabled() || self.shouldShowCheckButton) {
          [model addItem:_checkForProblemsItem
              toSectionWithIdentifier:SectionIdentifierPasswordCheck];

          [rowsIndexPaths addObject:[NSIndexPath indexPathForRow:1
                                                       inSection:checkSection]];
        }
        sectionIndex++;

        // Add "Add Password" button.
        if ([self allowsAddPassword]) {
          [model insertSectionWithIdentifier:SectionIdentifierAddPasswordButton
                                     atIndex:sectionIndex];
          [self.tableView
                insertSections:[NSIndexSet indexSetWithIndex:sectionIndex]
              withRowAnimation:UITableViewRowAnimationTop];
          [model addItem:_addPasswordItem
              toSectionWithIdentifier:SectionIdentifierAddPasswordButton];
          [rowsIndexPaths
              addObject:
                  [NSIndexPath
                      indexPathForRow:0
                            inSection:
                                [model
                                    sectionForSectionIdentifier:
                                        SectionIdentifierAddPasswordButton]]];
          sectionIndex++;
        }

        [self.tableView insertRowsAtIndexPaths:rowsIndexPaths
                              withRowAnimation:UITableViewRowAnimationTop];

        //  We want to restart the toolbar (display it) when the search bar is
        //  dismissed only if the current view is the Password Manager.
        if ([self.navigationController.topViewController
                isKindOfClass:[PasswordManagerViewController class]]) {
          self.navigationController.toolbarHidden = NO;
        }

        _tableIsInSearchMode = NO;
      }
               completion:nil];
}

#pragma mark - UISearchBarDelegate

- (void)searchBar:(UISearchBar*)searchBar textDidChange:(NSString*)searchText {
  if (searchText.length == 0 && self.navigationItem.searchController.active) {
    [self showScrim];
  } else {
    [self hideScrim];
  }

  [self searchForTerm:searchText];
}

#pragma mark - Private methods

// Shows loading spinner background view.
- (void)showLoadingSpinnerBackground {
  if (!self.spinnerView) {
    self.spinnerView =
        [[HomeWaitingView alloc] initWithFrame:self.tableView.bounds
                               backgroundColor:UIColor.clearColor];
    [self.spinnerView startWaiting];
  }
  self.navigationItem.searchController.searchBar.userInteractionEnabled = NO;
  self.tableView.backgroundView = self.spinnerView;
}

// Hide the loading spinner if it is showing.
- (void)hideLoadingSpinnerBackground {
  DCHECK(self.spinnerView);
  __weak __typeof(self) weakSelf = self;
  [self.spinnerView stopWaitingWithCompletion:^{
    [UIView animateWithDuration:0.2
        animations:^{
          self.spinnerView.alpha = 0.0;
        }
        completion:^(BOOL finished) {
          [weakSelf didHideSpinner];
        }];
  }];
}

// Called after the loading spinner hiding animation finished. Updates
// `tableViewModel` and then the view hierarchy.
- (void)didHideSpinner {
  // Remove spinner view after animation finished.
  self.navigationItem.searchController.searchBar.userInteractionEnabled = YES;
  self.tableView.backgroundView = nil;
  self.spinnerView = nil;
  // Update model and view hierarchy.
  _didReceivePasswords = YES;
  [self updateUIForEditState];
  [self reloadData];
}

// Dismisses the search controller when there's a touch event on the scrim.
- (void)dismissSearchController:(UIControl*)sender {
  if (self.navigationItem.searchController.active) {
    self.navigationItem.searchController.active = NO;
  }
}

// Shows scrim overlay and hide toolbar.
- (void)showScrim {
  if (self.scrimView.alpha < 1.0f) {
    self.scrimView.alpha = 0.0f;
    [self.tableView addSubview:self.scrimView];
    // We attach our constraints to the superview because the tableView is
    // a scrollView and it seems that we get an empty frame when attaching to
    // it.
    AddSameConstraints(self.scrimView, self.view.superview);
    self.tableView.accessibilityElementsHidden = YES;
    self.tableView.scrollEnabled = NO;
    [UIView animateWithDuration:kTableViewNavigationScrimFadeDuration
                     animations:^{
                       self.scrimView.alpha = 1.0f;
                       [self.view layoutIfNeeded];
                     }];
  }
}

// Hides scrim and restore toolbar.
- (void)hideScrim {
  if (self.scrimView.alpha > 0.0f) {
    [UIView animateWithDuration:kTableViewNavigationScrimFadeDuration
        animations:^{
          self.scrimView.alpha = 0.0f;
        }
        completion:^(BOOL finished) {
          [self.scrimView removeFromSuperview];
          self.tableView.accessibilityElementsHidden = NO;
          self.tableView.scrollEnabled = YES;
        }];
  }
}

- (void)searchForTerm:(NSString*)searchTerm {
  self.searchTerm = searchTerm;
  [self filterItems:searchTerm];

  TableViewModel* model = self.tableViewModel;
  NSMutableIndexSet* indexSet = [NSMutableIndexSet indexSet];
  if ([model hasSectionForSectionIdentifier:SectionIdentifierSavedPasswords]) {
    NSInteger passwordSection =
        [model sectionForSectionIdentifier:SectionIdentifierSavedPasswords];
    [indexSet addIndex:passwordSection];
  }
  if ([model hasSectionForSectionIdentifier:SectionIdentifierBlocked]) {
    NSInteger blockedSection =
        [model sectionForSectionIdentifier:SectionIdentifierBlocked];
    [indexSet addIndex:blockedSection];
  }
  if (indexSet.count > 0) {
    BOOL animationsWereEnabled = [UIView areAnimationsEnabled];
    [UIView setAnimationsEnabled:NO];
    [self.tableView reloadSections:indexSet
                  withRowAnimation:UITableViewRowAnimationAutomatic];
    [UIView setAnimationsEnabled:animationsWereEnabled];
  }
}

- (void)updatePasswordsSectionWithSearchTerm:(NSString*)searchTerm {
  if (IsPasswordGroupingEnabled()) {
    for (const auto& affiliatedGroup : _affiliatedGroups) {
      AffiliatedGroupTableViewItem* item =
          [self savedFormItemForAffiliatedGroup:affiliatedGroup];
      bool hidden =
          searchTerm.length > 0 &&
          ![item.title localizedCaseInsensitiveContainsString:searchTerm];
      if (hidden)
        continue;
      [self.tableViewModel addItem:item
           toSectionWithIdentifier:SectionIdentifierSavedPasswords];
    }
  } else {
    for (const auto& credential : _passwords) {
      CredentialTableViewItem* item =
          [self savedFormItemForCredential:credential];
      bool hidden =
          searchTerm.length > 0 &&
          ![item.title localizedCaseInsensitiveContainsString:searchTerm] &&
          ![item.detailText localizedCaseInsensitiveContainsString:searchTerm];
      if (hidden)
        continue;
      [self.tableViewModel addItem:item
           toSectionWithIdentifier:SectionIdentifierSavedPasswords];
    }
  }
}

// Builds the filtered list of passwords/blocked based on given
// `searchTerm`.
- (void)filterItems:(NSString*)searchTerm {
  TableViewModel* model = self.tableViewModel;

  if ([self hasPasswords]) {
    [model deleteAllItemsFromSectionWithIdentifier:
               SectionIdentifierSavedPasswords];
    [self updatePasswordsSectionWithSearchTerm:searchTerm];
  }

  if (!_blockedSites.empty()) {
    [model deleteAllItemsFromSectionWithIdentifier:SectionIdentifierBlocked];
    for (const auto& credential : _blockedSites) {
      CredentialTableViewItem* item = [self blockedSiteItem:credential];
      bool hidden =
          searchTerm.length > 0 &&
          ![item.title localizedCaseInsensitiveContainsString:searchTerm];
      if (hidden)
        continue;
      [model addItem:item toSectionWithIdentifier:SectionIdentifierBlocked];
    }
  }
}

// Update timestamp of the last check. Both old and new password check state
// should be provided in order to animate footer in a proper way.
- (void)updateLastCheckTimestampWithState:(PasswordCheckUIState)state
                                fromState:(PasswordCheckUIState)oldState
                                   update:(BOOL)update {
  if (!_didReceivePasswords || ![self hasPasswords]) {
    return;
  }

  NSInteger checkSection = [self.tableViewModel
      sectionForSectionIdentifier:SectionIdentifierPasswordCheck];

  switch (state) {
    case PasswordCheckStateUnmutedCompromisedPasswords:
      [self.tableViewModel setFooter:[self lastCompletedCheckTime]
            forSectionWithIdentifier:SectionIdentifierPasswordCheck];
      // Transition from disabled to unsafe state is possible only on page load.
      // In this case we want to avoid animation.
      if (oldState == PasswordCheckStateDisabled) {
        [UIView performWithoutAnimation:^{
          [self.tableView
                reloadSections:[NSIndexSet indexSetWithIndex:checkSection]
              withRowAnimation:UITableViewRowAnimationNone];
        }];
        return;
      }
      break;
    case PasswordCheckStateSafe:
    case PasswordCheckStateDefault:
    case PasswordCheckStateError:
    case PasswordCheckStateSignedOut:
    case PasswordCheckStateRunning:
    case PasswordCheckStateDisabled:
      if (oldState != PasswordCheckStateUnmutedCompromisedPasswords) {
        return;
      }

      [self.tableViewModel setFooter:nil
            forSectionWithIdentifier:SectionIdentifierPasswordCheck];
      break;
    // These states only occur when the kIOSPasswordCheckup feature is enabled
    // and the last check timestamp footer item is only shown when
    // kIOSPasswordCheckup feature is disabled. These should never be reached.
    case PasswordCheckStateReusedPasswords:
    case PasswordCheckStateWeakPasswords:
    case PasswordCheckStateDismissedWarnings:
      NOTREACHED_NORETURN();
  }
  if (update) {
    [self.tableView
        performBatchUpdates:^{
          if (!self.tableView)
            return;
          // Deleting and inserting section results in pleasant animation of
          // footer being added/removed.
          [self.tableView
                deleteSections:[NSIndexSet indexSetWithIndex:checkSection]
              withRowAnimation:UITableViewRowAnimationNone];
          [self.tableView
                insertSections:[NSIndexSet indexSetWithIndex:checkSection]
              withRowAnimation:UITableViewRowAnimationNone];
        }
                 completion:nil];
  }
}

// Updates password check button according to provided state.
- (void)updatePasswordCheckButtonWithState:(PasswordCheckUIState)state {
  if (!_checkForProblemsItem) {
    return;
  }

  _checkForProblemsItem.text =
      l10n_util::GetNSString(IDS_IOS_CHECK_PASSWORDS_NOW_BUTTON);

  if (self.editing) {
    _checkForProblemsItem.textColor = [UIColor colorNamed:kTextSecondaryColor];
    _checkForProblemsItem.accessibilityTraits |= UIAccessibilityTraitNotEnabled;
    return;
  }

  if (IsPasswordCheckupEnabled()) {
    switch (state) {
      case PasswordCheckStateSafe:
      case PasswordCheckStateUnmutedCompromisedPasswords:
      case PasswordCheckStateReusedPasswords:
      case PasswordCheckStateWeakPasswords:
      case PasswordCheckStateDismissedWarnings:
      case PasswordCheckStateRunning:
        self.shouldShowCheckButton = NO;
        break;
      case PasswordCheckStateDefault:
      case PasswordCheckStateError:
        self.shouldShowCheckButton = YES;
        [self setCheckForProblemsItemEnabled:YES];
        break;
      case PasswordCheckStateSignedOut:
        self.shouldShowCheckButton = YES;
        [self setCheckForProblemsItemEnabled:NO];
        break;
      // Fall through.
      case PasswordCheckStateDisabled:
        self.shouldShowCheckButton = YES;
        [self setCheckForProblemsItemEnabled:NO];
        break;
    }
  } else {
    switch (state) {
      case PasswordCheckStateSafe:
      case PasswordCheckStateUnmutedCompromisedPasswords:
      case PasswordCheckStateReusedPasswords:
      case PasswordCheckStateWeakPasswords:
      case PasswordCheckStateDismissedWarnings:
      case PasswordCheckStateDefault:
      case PasswordCheckStateError:
        [self setCheckForProblemsItemEnabled:YES];
        break;
      case PasswordCheckStateSignedOut:
        [self setCheckForProblemsItemEnabled:NO];
        break;
      case PasswordCheckStateRunning:
      // Fall through.
      case PasswordCheckStateDisabled:
        [self setCheckForProblemsItemEnabled:NO];
        break;
    }
  }
}

// Updates password check status label according to provided state.
- (void)updatePasswordCheckStatusLabelWithState:(PasswordCheckUIState)state {
  if (!_passwordProblemsItem)
    return;

  _passwordProblemsItem.trailingImage = nil;
  _passwordProblemsItem.trailingImageTintColor = nil;
  _passwordProblemsItem.enabled = !self.editing;
  _passwordProblemsItem.indicatorHidden = YES;
  _passwordProblemsItem.infoButtonHidden = YES;
  _passwordProblemsItem.accessoryType =
      IsPasswordCheckTappable(state)
          ? UITableViewCellAccessoryDisclosureIndicator
          : UITableViewCellAccessoryNone;
  _passwordProblemsItem.text =
      IsPasswordCheckupEnabled()
          ? l10n_util::GetNSString(IDS_IOS_PASSWORD_CHECKUP)
          : l10n_util::GetNSString(IDS_IOS_CHECK_PASSWORDS);
  _passwordProblemsItem.detailText =
      IsPasswordCheckupEnabled()
          ? l10n_util::GetNSString(IDS_IOS_PASSWORD_CHECKUP_DESCRIPTION)
          : l10n_util::GetNSString(IDS_IOS_CHECK_PASSWORDS_DESCRIPTION);

  switch (state) {
    case PasswordCheckStateRunning: {
      if (IsPasswordCheckupEnabled()) {
        _passwordProblemsItem.text =
            l10n_util::GetNSString(IDS_IOS_PASSWORD_CHECKUP_ONGOING);
        _passwordProblemsItem.detailText =
            base::SysUTF16ToNSString(l10n_util::GetPluralStringFUTF16(
                IDS_IOS_PASSWORD_CHECKUP_SITES_AND_APPS_COUNT,
                _affiliatedGroups.size()));
      }
      _passwordProblemsItem.indicatorHidden = NO;
      break;
    }
    case PasswordCheckStateDisabled: {
      _passwordProblemsItem.enabled = NO;
      break;
    }
    case PasswordCheckStateUnmutedCompromisedPasswords: {
      _passwordProblemsItem.detailText =
          base::SysUTF16ToNSString(l10n_util::GetPluralStringFUTF16(
              IsPasswordCheckupEnabled()
                  ? IDS_IOS_PASSWORD_CHECKUP_COMPROMISED_COUNT
                  : IDS_IOS_CHECK_PASSWORDS_COMPROMISED_COUNT,
              self.insecurePasswordsCount));
      _passwordProblemsItem.warningState = WarningState::kSevereWarning;

      // The red tint color for the compromised password warning here depends on
      // the Password Grouping feature (which will be enabled before Password
      // Checkup). Overriding the tint color set by setting the item's warning
      // state to make sure it is the correct one for the Password Grouping
      // feature. TODO(crbug.com/1406871): Remove line when kIOSPasswordCheckup
      // is enabled by default.
      _passwordProblemsItem.trailingImageTintColor = [UIColor
          colorNamed:IsPasswordGroupingEnabled() ? kRed500Color : kRedColor];
      break;
    }
    case PasswordCheckStateReusedPasswords: {
      _passwordProblemsItem.detailText = l10n_util::GetNSStringF(
          IDS_IOS_PASSWORD_CHECKUP_REUSED_COUNT,
          base::NumberToString16(self.insecurePasswordsCount));
      _passwordProblemsItem.warningState = WarningState::kWarning;
      break;
    }
    case PasswordCheckStateWeakPasswords: {
      _passwordProblemsItem.detailText = base::SysUTF16ToNSString(
          l10n_util::GetPluralStringFUTF16(IDS_IOS_PASSWORD_CHECKUP_WEAK_COUNT,
                                           self.insecurePasswordsCount));
      _passwordProblemsItem.warningState = WarningState::kWarning;
      break;
    }
    case PasswordCheckStateDismissedWarnings: {
      _passwordProblemsItem.detailText =
          base::SysUTF16ToNSString(l10n_util::GetPluralStringFUTF16(
              IDS_IOS_PASSWORD_CHECKUP_DISMISSED_COUNT,
              self.insecurePasswordsCount));
      _passwordProblemsItem.warningState = WarningState::kWarning;
      break;
    }
    case PasswordCheckStateSafe: {
      _passwordProblemsItem.detailText =
          IsPasswordCheckupEnabled()
              ? [self.delegate formattedElapsedTimeSinceLastCheck]
              : base::SysUTF16ToNSString(l10n_util::GetPluralStringFUTF16(
                    IDS_IOS_PASSWORD_CHECKUP_COMPROMISED_COUNT, 0));
      _passwordProblemsItem.warningState = WarningState::kSafe;
      break;
    }
    case PasswordCheckStateDefault:
      break;
    case PasswordCheckStateError:
    case PasswordCheckStateSignedOut: {
      _passwordProblemsItem.detailText =
          IsPasswordCheckupEnabled()
              ? l10n_util::GetNSString(IDS_IOS_PASSWORD_CHECKUP_ERROR)
              : l10n_util::GetNSString(IDS_IOS_PASSWORD_CHECK_ERROR);
      _passwordProblemsItem.infoButtonHidden = NO;
      break;
    }
  }

  // Notify accessibility to focus on the Password Check Status cell if needed.
  if ([self shouldFocusAccessibilityOnPasswordCheckStatusForState:state]) {
    [self focusAccessibilityOnPasswordCheckStatus];
    self.checkWasTriggeredManually = NO;
  }
}

// Enables or disables the `_checkForProblemsItem` and sets it up accordingly.
- (void)setCheckForProblemsItemEnabled:(BOOL)enabled {
  if (!_checkForProblemsItem) {
    return;
  }

  _checkForProblemsItem.enabled = enabled;
  if (enabled) {
    _checkForProblemsItem.textColor = [UIColor colorNamed:kBlueColor];
    _checkForProblemsItem.accessibilityTraits &=
        ~UIAccessibilityTraitNotEnabled;
  } else {
    _checkForProblemsItem.textColor = [UIColor colorNamed:kTextSecondaryColor];
    _checkForProblemsItem.accessibilityTraits |= UIAccessibilityTraitNotEnabled;
  }
}

- (void)setAddPasswordButtonEnabled:(BOOL)enabled {
  if (!_addPasswordItem) {
    return;
  }
  if (enabled) {
    _addPasswordItem.textColor = [UIColor colorNamed:kBlueColor];
    _addPasswordItem.accessibilityTraits &= ~UIAccessibilityTraitNotEnabled;
  } else {
    _addPasswordItem.textColor = [UIColor colorNamed:kTextSecondaryColor];
    _addPasswordItem.accessibilityTraits |= UIAccessibilityTraitNotEnabled;
  }
  [self reconfigureCellsForItems:@[ _addPasswordItem ]];
}

// Removes the given section if it exists.
- (void)clearSectionWithIdentifier:(NSInteger)sectionIdentifier
                  withRowAnimation:(UITableViewRowAnimation)animation {
  TableViewModel* model = self.tableViewModel;
  if ([model hasSectionForSectionIdentifier:sectionIdentifier]) {
    NSInteger section = [model sectionForSectionIdentifier:sectionIdentifier];
    [model removeSectionWithIdentifier:sectionIdentifier];
    [[self tableView] deleteSections:[NSIndexSet indexSetWithIndex:section]
                    withRowAnimation:animation];
  }
}

- (void)deleteItemAtIndexPaths:(NSArray<NSIndexPath*>*)indexPaths {
  std::vector<password_manager::CredentialUIEntry> credentialsToDelete;

  for (NSIndexPath* indexPath in indexPaths) {
    // Only form items are editable.
    NSInteger itemType = [self.tableViewModel itemTypeForIndexPath:indexPath];
    TableViewItem* item = [self.tableViewModel itemAtIndexPath:indexPath];

    // Remove affiliated group.
    if (IsPasswordGroupingEnabled() && itemType == ItemTypeSavedPassword) {
      password_manager::AffiliatedGroup affiliatedGroup =
          base::mac::ObjCCastStrict<AffiliatedGroupTableViewItem>(item)
              .affiliatedGroup;

      // Remove from local cache.
      auto iterator = base::ranges::find(_affiliatedGroups, affiliatedGroup);
      if (iterator != _affiliatedGroups.end())
        _affiliatedGroups.erase(iterator);

      // Add to the credentials to delete vector to remove from store.
      credentialsToDelete.insert(credentialsToDelete.end(),
                                 affiliatedGroup.GetCredentials().begin(),
                                 affiliatedGroup.GetCredentials().end());
    } else {
      password_manager::CredentialUIEntry credential =
          base::mac::ObjCCastStrict<CredentialTableViewItem>(item).credential;

      auto removeCredential =
          [](std::vector<password_manager::CredentialUIEntry>& credentials,
             const password_manager::CredentialUIEntry& credential) {
            auto iterator = base::ranges::find(credentials, credential);
            if (iterator != credentials.end())
              credentials.erase(iterator);
          };

      if (itemType == ItemTypeBlocked) {
        removeCredential(_blockedSites, credential);
      } else {
        removeCredential(_passwords, credential);
      }

      credentialsToDelete.push_back(std::move(credential));
    }
  }

  // Remove empty sections.
  __weak PasswordManagerViewController* weakSelf = self;
  [self.tableView
      performBatchUpdates:^{
        PasswordManagerViewController* strongSelf = weakSelf;
        if (!strongSelf)
          return;

        [strongSelf removeFromModelItemAtIndexPaths:indexPaths];
        [strongSelf.tableView
            deleteRowsAtIndexPaths:indexPaths
                  withRowAnimation:UITableViewRowAnimationAutomatic];

        // Delete in reverse order of section indexes (bottom up of section
        // displayed), so that indexes in model matches those in the view.  if
        // we don't we'll cause a crash.
        if (strongSelf->_blockedSites.empty()) {
          [strongSelf
              clearSectionWithIdentifier:SectionIdentifierBlocked
                        withRowAnimation:UITableViewRowAnimationAutomatic];
        }
        if (![strongSelf hasPasswords]) {
          [strongSelf
              clearSectionWithIdentifier:SectionIdentifierSavedPasswords
                        withRowAnimation:UITableViewRowAnimationAutomatic];
        }
      }
      completion:^(BOOL finished) {
        PasswordManagerViewController* strongSelf = weakSelf;
        if (!strongSelf)
          return;
        // If both lists are empty, exit editing mode.
        if (![strongSelf hasPasswords] && strongSelf->_blockedSites.empty()) {
          [strongSelf setEditing:NO animated:YES];
          // An illustrated empty state is required, so reload the whole model.
          [strongSelf reloadData];
        }
        [strongSelf updateUIForEditState];
      }];

  [self.delegate deleteCredentials:credentialsToDelete];
}

// Notifies the handler to show the Password Checkup homepage if the state of
// the Password Check cell allows it.
- (void)showPasswordCheckupPage {
  if (!IsPasswordCheckTappable(self.passwordCheckState)) {
    return;
  }
  [self.handler showPasswordCheckup];
}

// Notifies the handler to show the password issues page if the state of the
// Password Check cell allows it.
// TODO(crbug.com/1406871): Remove when kIOSPasswordCheckup is enabled by
// default.
- (void)showPasswordIssuesPage {
  if (!IsPasswordCheckTappable(self.passwordCheckState)) {
    return;
  }
  [self.handler showPasswordIssues];
  password_manager::LogPasswordCheckReferrer(
      password_manager::PasswordCheckReferrer::kPasswordSettings);
}

// Scrolls the password lists such that most recently updated
// SavedFormContentItem is in the top of the screen.
- (void)scrollToLastUpdatedItem {
  if (self.mostRecentlyUpdatedItem) {
    NSIndexPath* indexPath =
        [self.tableViewModel indexPathForItem:self.mostRecentlyUpdatedItem];
    [self.tableView scrollToRowAtIndexPath:indexPath
                          atScrollPosition:UITableViewScrollPositionTop
                                  animated:NO];
    self.mostRecentlyUpdatedItem = nil;
  }
}

// Returns YES if accessibility should focus on the Password Check Status cell.
- (BOOL)shouldFocusAccessibilityOnPasswordCheckStatusForState:
    (PasswordCheckUIState)state {
  if (!UIAccessibilityIsVoiceOverRunning()) {
    return false;
  }

  BOOL passwordCheckStateIsValid = state != PasswordCheckStateDefault &&
                                   state != PasswordCheckStateRunning &&
                                   state != PasswordCheckStateDisabled;

  // When kIOSPasswordCheckup is disabled, accessibility should focus on the
  // Password Check Status cell when:
  // 1. The password check was triggered manually.
  // AND
  // 2. The password check state changed to insecure (compromised, reused, weak
  // or dismissed warnings), safe or error (i.e., any state other than default,
  // running and disabled).
  if (!IsPasswordCheckupEnabled()) {
    return self.checkWasTriggeredManually && passwordCheckStateIsValid;
  }

  // When kIOSPasswordCheckup is enabled, accessibility should focus on the
  // Password Check Status cell when:
  // 1. The password check was triggered manually (because the "Check Now"
  // button dissapears afterwards, so the focus should move to the status cell).
  // OR
  // 2. The focus was already on the Password Check Status cell. AND
  // 3. The password check state changed to insecure (compromised, reused, weak
  // or dismissed warnings), safe or error (i.e., any state other than default,
  // running and disabled).
  return self.checkWasTriggeredManually ||
         ([self isPasswordCheckStatusFocusedByVoiceOver] &&
          passwordCheckStateIsValid);
}

// Returns YES if the Password Check Staus cell is currently focused by
// accessibility.
- (BOOL)isPasswordCheckStatusFocusedByVoiceOver {
  if (![self.tableViewModel
          hasItemForItemType:ItemTypePasswordCheckStatus
           sectionIdentifier:SectionIdentifierPasswordCheck]) {
    return false;
  }

  // Get the Password Check Status cell.
  NSIndexPath* indexPath =
      [self.tableViewModel indexPathForItemType:ItemTypePasswordCheckStatus
                              sectionIdentifier:SectionIdentifierPasswordCheck];
  UITableViewCell* passwordCheckStatusCell =
      [self.tableView cellForRowAtIndexPath:indexPath];

  // Get the view element that is currently focused.
  UIAccessibilityElement* focusedElement = UIAccessibilityFocusedElement(
      UIAccessibilityNotificationVoiceOverIdentifier);

  return [passwordCheckStatusCell.accessibilityLabel
      isEqualToString:focusedElement.accessibilityLabel];
}

// Notifies accessibility to focus on the Password Check Status cell when its
// layout changed.
- (void)focusAccessibilityOnPasswordCheckStatus {
  if ([self.tableViewModel hasItemForItemType:ItemTypePasswordCheckStatus
                            sectionIdentifier:SectionIdentifierPasswordCheck]) {
    NSIndexPath* indexPath = [self.tableViewModel
        indexPathForItemType:ItemTypePasswordCheckStatus
           sectionIdentifier:SectionIdentifierPasswordCheck];
    UITableViewCell* cell = [self.tableView cellForRowAtIndexPath:indexPath];
    UIAccessibilityPostNotification(UIAccessibilityLayoutChangedNotification,
                                    cell);
  }
}

- (void)setPasswordProblemsItemAccessibilityLabelForSafeState {
  NSIndexPath* indexPath =
      [self.tableViewModel indexPathForItemType:ItemTypePasswordCheckStatus
                              sectionIdentifier:SectionIdentifierPasswordCheck];
  UITableViewCell* passwordProblemsCell =
      [self.tableView cellForRowAtIndexPath:indexPath];
  passwordProblemsCell.accessibilityLabel = [NSString
      stringWithFormat:
          @"%@. %@", passwordProblemsCell.accessibilityLabel,
          l10n_util::GetNSString(
              IDS_IOS_PASSWORD_CHECKUP_SAFE_STATE_ACCESSIBILITY_LABEL)];
}

// Logs metrics related to favicons for the Password Manager.
- (void)logMetricsForFavicons {
  DCHECK(!_faviconMetricLogged);

  int n_monograms = 0;
  int n_images = 0;
  std::vector sections_and_types = {
      std::pair{SectionIdentifierSavedPasswords, ItemTypeSavedPassword},
      std::pair{SectionIdentifierBlocked, ItemTypeBlocked}};
  for (auto [section, type] : sections_and_types) {
    if (![self.tableViewModel hasSectionForSectionIdentifier:section]) {
      continue;
    }

    NSArray<NSIndexPath*>* indexPaths =
        [self.tableViewModel indexPathsForItemType:type
                                 sectionIdentifier:section];
    for (NSIndexPath* indexPath : indexPaths) {
      PasswordFormContentCell* cell =
          [self.tableView cellForRowAtIndexPath:indexPath];
      if (!cell) {
        // Cell not queued for displaying yet.
        continue;
      }

      switch (cell.faviconTypeForMetrics) {
        case FaviconTypeNotLoaded:
          continue;
        case FaviconTypeMonogram:
          n_monograms++;
          break;
        case FaviconTypeImage:
          n_images++;
          break;
      }
    }
  }

  base::UmaHistogramCounts10000(
      "IOS.PasswordManager.PasswordsWithFavicons.Count",
      n_images + n_monograms);
  if (n_images + n_monograms > 0) {
    base::UmaHistogramCounts10000("IOS.PasswordManager.Favicons.Count",
                                  n_images);
    base::UmaHistogramPercentage("IOS.PasswordManager.Favicons.Percentage",
                                 100.0f * n_images / (n_images + n_monograms));
  }
}

- (bool)allowsAddPassword {
  // If the settings are managed by enterprise policy and the password manager
  // is not enabled, there won't be any add functionality.
  const char* prefName = password_manager::prefs::kCredentialsEnableService;
  return !self.prefService->IsManagedPreference(prefName) ||
         self.prefService->GetBoolean(prefName);
}

// Configures the title of this ViewController.
- (void)setUpTitle {
  self.title = l10n_util::GetNSString(IDS_IOS_PASSWORD_MANAGER);

  self.navigationItem.titleView =
      password_manager::CreatePasswordManagerTitleView(/*title=*/self.title);
}

// Shows the empty state view when there is no content to display in the
// tableView, otherwise hides the empty state view if one is being displayed.
- (void)showOrHideEmptyView {
  if (![self hasPasswords] && _blockedSites.empty()) {
    NSString* title =
        l10n_util::GetNSString(IDS_IOS_SETTINGS_PASSWORD_EMPTY_TITLE);

    NSDictionary* textAttributes =
        [TableViewIllustratedEmptyView defaultTextAttributesForSubtitle];
    NSURL* linkURL = net::NSURLWithGURL(google_util::AppendGoogleLocaleParam(
        GURL(password_manager::kPasswordManagerHelpCenteriOSURL),
        GetApplicationContext()->GetApplicationLocale()));
    NSDictionary* linkAttributes = @{
      NSForegroundColorAttributeName : [UIColor colorNamed:kBlueColor],
      NSLinkAttributeName : linkURL,
    };
    NSAttributedString* subtitle = AttributedStringFromStringWithLink(
        l10n_util::GetNSString(IDS_IOS_SAVE_PASSWORDS_MANAGE_ACCOUNT_HEADER),
        textAttributes, linkAttributes);

    [self addEmptyTableViewWithImage:[UIImage imageNamed:@"passwords_empty"]
                               title:title
                  attributedSubtitle:subtitle
                            delegate:self];
    self.navigationItem.searchController = nil;
    self.tableView.alwaysBounceVertical = NO;
  } else {
    [self removeEmptyTableView];
    self.navigationItem.searchController = self.searchController;
    self.tableView.alwaysBounceVertical = YES;
  }
}

// Private accessor to `_didReceivePasswords` only exposed to unit tests.
- (BOOL)didReceivePasswords {
  return _didReceivePasswords;
}

- (void)settingsButtonCallback {
  [self.presentationDelegate showPasswordSettingsSubmenu];
}

- (void)addButtonCallback {
  [self.handler showAddPasswordSheet];
}

- (UIBarButtonItem*)settingsButtonInToolbar {
  if (!_settingsButtonInToolbar) {
    _settingsButtonInToolbar =
        [self settingsButtonWithAction:@selector(settingsButtonCallback)];
  }

  return _settingsButtonInToolbar;
}

- (UIBarButtonItem*)addButtonInToolbar {
  if (!_addButtonInToolbar) {
    _addButtonInToolbar =
        [self addButtonWithAction:@selector(addButtonCallback)];
  }
  return _addButtonInToolbar;
}

// Helper method determining if the empty state view should be displayed.
- (BOOL)shouldShowEmptyStateView {
  return ![self hasPasswords] && _blockedSites.empty();
}

- (void)deleteItemAtIndexPathsForTesting:(NSArray<NSIndexPath*>*)indexPaths {
  [self deleteItemAtIndexPaths:indexPaths];
}

- (void)updatePasswordCheckSectionWithState:(PasswordCheckUIState)state {
  if (![self.tableViewModel
          hasSectionForSectionIdentifier:SectionIdentifierPasswordCheck]) {
    return;
  }

  [self.tableView
      performBatchUpdates:^{
        if (_passwordProblemsItem) {
          [self reconfigureCellsForItems:@[ _passwordProblemsItem ]];
          // When in safe state, a custom accessibility label needs to be set
          // for the Password Checkup cell.
          if (state == PasswordCheckStateSafe) {
            [self setPasswordProblemsItemAccessibilityLabelForSafeState];
          }
        }
        if (_checkForProblemsItem) {
          // If kIOSPasswordCheckup feature is disabled, only reconfigure the
          // check button cell.
          if (!IsPasswordCheckupEnabled()) {
            [self reconfigureCellsForItems:@[ _checkForProblemsItem ]];
          } else {
            BOOL checkForProblemsItemIsInModel = [self.tableViewModel
                hasItemForItemType:ItemTypeCheckForProblemsButton
                 sectionIdentifier:SectionIdentifierPasswordCheck];
            // Check if the check button should be removed from the table view.
            if (!self.shouldShowCheckButton && checkForProblemsItemIsInModel) {
              [self.tableView
                  deleteRowsAtIndexPaths:@[ [self checkButtonIndexPath] ]
                        withRowAnimation:UITableViewRowAnimationAutomatic];
              [self.tableViewModel
                         removeItemWithType:ItemTypeCheckForProblemsButton
                  fromSectionWithIdentifier:SectionIdentifierPasswordCheck];
            } else if (self.shouldShowCheckButton) {
              [self reconfigureCellsForItems:@[ _checkForProblemsItem ]];
              // Check if the check button should be added to the table view.
              if (!checkForProblemsItemIsInModel) {
                [self.tableViewModel addItem:_checkForProblemsItem
                     toSectionWithIdentifier:SectionIdentifierPasswordCheck];
                [self.tableView
                    insertRowsAtIndexPaths:@[ [self checkButtonIndexPath] ]
                          withRowAnimation:UITableViewRowAnimationAutomatic];
              }
            }
          }
        }
        [self.tableView layoutIfNeeded];
      }
               completion:nil];
}

- (NSIndexPath*)checkButtonIndexPath {
  return
      [self.tableViewModel indexPathForItemType:ItemTypeCheckForProblemsButton
                              sectionIdentifier:SectionIdentifierPasswordCheck];
}

- (void)showDetailedViewPageForItem:(TableViewItem*)item {
  if (IsPasswordGroupingEnabled()) {
    [self.handler
        showDetailedViewForAffiliatedGroup:base::mac::ObjCCastStrict<
                                               AffiliatedGroupTableViewItem>(
                                               item)
                                               .affiliatedGroup];
  } else {
    [self.handler
        showDetailedViewForCredential:base::mac::ObjCCastStrict<
                                          CredentialTableViewItem>(item)
                                          .credential];
  }
}

#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  [super tableView:tableView didSelectRowAtIndexPath:indexPath];

  // Actions should only take effect when not in editing mode.
  if (self.editing) {
    self.deleteButton.enabled = YES;
    return;
  }

  TableViewModel* model = self.tableViewModel;
  ItemType itemType =
      static_cast<ItemType>([model itemTypeForIndexPath:indexPath]);
  switch (itemType) {
    case ItemTypePasswordCheckStatus:
      IsPasswordCheckupEnabled() ? [self showPasswordCheckupPage]
                                 : [self showPasswordIssuesPage];
      break;
    case ItemTypeSavedPassword: {
      DCHECK_EQ(SectionIdentifierSavedPasswords,
                [model sectionIdentifierForSectionIndex:indexPath.section]);
      TableViewItem* item = [model itemAtIndexPath:indexPath];

      if (!IsPasswordNotesWithBackupEnabled()) {
        [self showDetailedViewPageForItem:item];
      } else if ([self.reauthenticationModule canAttemptReauth]) {
        void (^showPasswordDetailsHandler)(ReauthenticationResult) =
            ^(ReauthenticationResult result) {
              if (result == ReauthenticationResult::kFailure) {
                return;
              }

              [self showDetailedViewPageForItem:item];
            };

        [self.reauthenticationModule
            attemptReauthWithLocalizedReason:
                l10n_util::GetNSString(
                    IDS_IOS_SETTINGS_PASSWORD_REAUTH_REASON_SHOW)
                        canReusePreviousAuth:YES
                                     handler:showPasswordDetailsHandler];
      } else {
        DCHECK(self.handler);
        [self.handler showSetupPasscodeDialog];
      }

      break;
    }
    case ItemTypeBlocked: {
      DCHECK_EQ(SectionIdentifierBlocked,
                [model sectionIdentifierForSectionIndex:indexPath.section]);
      password_manager::CredentialUIEntry credential =
          base::mac::ObjCCastStrict<CredentialTableViewItem>(
              [model itemAtIndexPath:indexPath])
              .credential;
      [self.handler showDetailedViewForCredential:credential];
      break;
    }
    case ItemTypeCheckForProblemsButton:
      if (self.passwordCheckState != PasswordCheckStateRunning) {
        [self.delegate startPasswordCheck];
        password_manager::LogStartPasswordCheckManually();
        self.checkWasTriggeredManually = YES;
      }
      break;
    case ItemTypeAddPasswordButton: {
      [self.handler showAddPasswordSheet];
      break;
    }
    case ItemTypeLastCheckTimestampFooter:
    case ItemTypeLinkHeader:
    case ItemTypeHeader:
      NOTREACHED();
  }
  [tableView deselectRowAtIndexPath:indexPath animated:YES];
}

- (void)tableView:(UITableView*)tableView
    didDeselectRowAtIndexPath:(NSIndexPath*)indexPath {
  [super tableView:tableView didDeselectRowAtIndexPath:indexPath];
  if (!self.editing) {
    return;
  }

  if (self.tableView.indexPathsForSelectedRows.count == 0) {
    self.deleteButton.enabled = NO;
  }
}

- (BOOL)tableView:(UITableView*)tableView
    shouldHighlightRowAtIndexPath:(NSIndexPath*)indexPath {
  NSInteger itemType = [self.tableViewModel itemTypeForIndexPath:indexPath];
  switch (itemType) {
    case ItemTypePasswordCheckStatus:
      return IsPasswordCheckTappable(self.passwordCheckState);
    case ItemTypeCheckForProblemsButton:
      return _checkForProblemsItem.isEnabled;
    case ItemTypeAddPasswordButton:
      return [self allowsAddPassword];
  }
  return YES;
}

- (UIView*)tableView:(UITableView*)tableView
    viewForHeaderInSection:(NSInteger)section {
  UIView* view = [super tableView:tableView viewForHeaderInSection:section];

  if ([self.tableViewModel sectionIdentifierForSectionIndex:section] ==
      SectionIdentifierManageAccountHeader) {
    // This is the text at the top of the page with a link. Attach as a delegate
    // to ensure clicks on the link are handled.
    TableViewLinkHeaderFooterView* linkView =
        base::mac::ObjCCastStrict<TableViewLinkHeaderFooterView>(view);
    linkView.delegate = self;
  }

  return view;
}

- (CGFloat)tableView:(UITableView*)tableView
    heightForFooterInSection:(NSInteger)section {
  // Customize height of emtpy footer for manage account header section to
  // achieve desired vertical spacing to next item.
  if ([self.tableViewModel sectionIdentifierForSectionIndex:section] ==
      SectionIdentifierManageAccountHeader) {
    return kManageAccountHeaderSectionFooterHeight;
  }

  return [super tableView:tableView heightForFooterInSection:section];
}

#pragma mark - UITableViewDataSource

- (BOOL)tableView:(UITableView*)tableView
    canEditRowAtIndexPath:(NSIndexPath*)indexPath {
  // Only password cells are editable.
  NSInteger itemType = [self.tableViewModel itemTypeForIndexPath:indexPath];
  return itemType == ItemTypeSavedPassword || itemType == ItemTypeBlocked;
}

- (void)tableView:(UITableView*)tableView
    commitEditingStyle:(UITableViewCellEditingStyle)editingStyle
     forRowAtIndexPath:(NSIndexPath*)indexPath {
  if (editingStyle != UITableViewCellEditingStyleDelete)
    return;
  [self deleteItemAtIndexPaths:@[ indexPath ]];
}

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  UITableViewCell* cell = [super tableView:tableView
                     cellForRowAtIndexPath:indexPath];
  switch ([self.tableViewModel itemTypeForIndexPath:indexPath]) {
    case ItemTypePasswordCheckStatus: {
      SettingsCheckCell* passwordCheckCell =
          base::mac::ObjCCastStrict<SettingsCheckCell>(cell);
      [passwordCheckCell.infoButton
                 addTarget:self
                    action:@selector(didTapPasswordCheckInfoButton:)
          forControlEvents:UIControlEventTouchUpInside];
      break;
    }
    case ItemTypeSavedPassword:
    case ItemTypeBlocked: {
      // Load the favicon from cache.
      [base::mac::ObjCCastStrict<PasswordFormContentCell>(cell)
          loadFavicon:self.imageDataSource];
      break;
    }
  }
  return cell;
}

#pragma mark Helper methods

// Enables/disables search bar.
- (void)setSearchBarEnabled:(BOOL)enabled {
  if (enabled) {
    self.navigationItem.searchController.searchBar.userInteractionEnabled = YES;
    self.navigationItem.searchController.searchBar.alpha = 1.0;
  } else {
    self.navigationItem.searchController.searchBar.userInteractionEnabled = NO;
    self.navigationItem.searchController.searchBar.alpha =
        kTableViewNavigationAlphaForDisabledSearchBar;
  }
}

#pragma mark - ChromeAccountManagerServiceObserver

- (void)identityListChanged {
  [self reloadData];
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  base::RecordAction(
      base::UserMetricsAction("IOSPasswordsSettingsCloseWithSwipe"));
  _accountManagerServiceObserver.reset();
}

#pragma mark - TableViewIllustratedEmptyViewDelegate

- (void)tableViewIllustratedEmptyView:(TableViewIllustratedEmptyView*)view
                   didTapSubtitleLink:(NSURL*)URL {
  [self didTapLinkURL:URL];
}

@end
