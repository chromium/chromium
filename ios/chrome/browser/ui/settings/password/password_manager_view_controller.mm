// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_manager_view_controller.h"

#import <UIKit/UIKit.h>

#import <optional>
#import <utility>
#import <vector>

#import "base/apple/foundation_util.h"
#import "base/ios/ios_util.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/user_metrics.h"
#import "base/ranges/algorithm.h"
#import "base/strings/sys_string_conversions.h"
#import "components/google/core/common/google_util.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/password_manager/core/browser/password_manager_constants.h"
#import "components/password_manager/core/browser/password_manager_metrics_util.h"
#import "components/password_manager/core/browser/password_ui_utils.h"
#import "components/password_manager/core/browser/ui/affiliated_group.h"
#import "components/password_manager/core/browser/ui/credential_ui_entry.h"
#import "components/password_manager/core/browser/ui/password_check_referrer.h"
#import "components/password_manager/core/common/password_manager_pref_names.h"
#import "components/prefs/pref_service.h"
#import "components/strings/grit/components_strings.h"
#import "components/sync/service/sync_service.h"
#import "components/sync/service/sync_service_utils.h"
#import "components/sync/service/sync_user_settings.h"
#import "ios/chrome/browser/net/model/crurl.h"
#import "ios/chrome/browser/passwords/model/password_checkup_metrics.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/ui/elements/branded_navigation_item_title_view.h"
#import "ios/chrome/browser/shared/ui/elements/home_waiting_view.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
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
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_observer_bridge.h"
#import "ios/chrome/browser/ui/settings/cells/inline_promo_cell.h"
#import "ios/chrome/browser/ui/settings/cells/inline_promo_item.h"
#import "ios/chrome/browser/ui/settings/cells/settings_check_cell.h"
#import "ios/chrome/browser/ui/settings/cells/settings_check_item.h"
#import "ios/chrome/browser/ui/settings/elements/enterprise_info_popover_view_controller.h"
#import "ios/chrome/browser/ui/settings/password/create_password_manager_title_view.h"
#import "ios/chrome/browser/ui/settings/password/password_manager_view_controller+Testing.h"
#import "ios/chrome/browser/ui/settings/password/password_manager_view_controller_delegate.h"
#import "ios/chrome/browser/ui/settings/password/password_manager_view_controller_items.h"
#import "ios/chrome/browser/ui/settings/password/password_manager_view_controller_presentation_delegate.h"
#import "ios/chrome/browser/ui/settings/password/passwords_consumer.h"
#import "ios/chrome/browser/ui/settings/password/passwords_settings_commands.h"
#import "ios/chrome/browser/ui/settings/password/passwords_table_view_constants.h"
#import "ios/chrome/browser/ui/settings/settings_root_table_view_controller+toolbar_add.h"
#import "ios/chrome/browser/ui/settings/settings_root_table_view_controller+toolbar_settings.h"
#import "ios/chrome/browser/ui/settings/settings_root_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/utils/password_utils.h"
#import "ios/chrome/common/string_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/elements/popover_label_view_controller.h"
#import "ios/chrome/common/ui/reauthentication/reauthentication_module.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "net/base/apple/url_conversions.h"
#import "ui/base/device_form_factor.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "url/gurl.h"

using base::UmaHistogramEnumeration;
using password_manager::metrics_util::PasswordCheckInteraction;

namespace {

// Height of empty footer below the manage account header.
// This ammount added to the internal padding of the manage account header (8pt)
// and the height of the empty header of the next section (10pt) achieves the
// desired vertical spacing (20pt) between the manager account header's text and
// the first item of the next section.
constexpr CGFloat kManageAccountHeaderSectionFooterHeight = 2;

// The maximum width the view can have for the widget promo cell to be
// configured with its narrow layout. When the view's width is above that, the
// cell's layout should be switched to the wide one.
constexpr CGFloat kWidgetPromoLayoutThreshold = 500;

typedef NS_ENUM(NSInteger, ItemType) {
  // Section: SectionIdentifierManageAccountHeader
  ItemTypeLinkHeader = kItemTypeEnumZero,
  // Section: SectionIdentifierWidgetPromo
  ItemTypeWidgetPromo,
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

// Helper method to determine whether the Password Check cell is tappable or
// not.
bool IsPasswordCheckTappable(PasswordCheckUIState passwordCheckState) {
  switch (passwordCheckState) {
    case PasswordCheckStateUnmutedCompromisedPasswords:
    case PasswordCheckStateReusedPasswords:
    case PasswordCheckStateWeakPasswords:
    case PasswordCheckStateDismissedWarnings:
    case PasswordCheckStateSafe:
      return true;
    case PasswordCheckStateDefault:
    case PasswordCheckStateRunning:
    case PasswordCheckStateDisabled:
    case PasswordCheckStateError:
    case PasswordCheckStateSignedOut:
      return false;
  }
}

// TODO(crbug.com/40261300): Remove when CredentialUIEntry operator== is fixed.
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
    TableViewIllustratedEmptyViewDelegate>

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

// Stores the item which has form attribute's username and site equivalent to
// that of `mostRecentlyUpdatedCred`.
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

// Indicates whether the check button should be shown or not.
@property(nonatomic, assign) BOOL shouldShowCheckButton;

// The PrefService passed to this instance.
@property(nonatomic, assign) PrefService* prefService;

// The header for save passwords switch section.
@property(nonatomic, readonly)
    TableViewLinkHeaderFooterItem* manageAccountLinkItem;

// The item related to the password check status.
@property(nonatomic, readonly) SettingsCheckItem* passwordProblemsItem;

// The button to start password check.
@property(nonatomic, readonly) TableViewTextItem* checkForProblemsItem;

// The button to add a password.
@property(nonatomic, readonly) TableViewTextItem* addPasswordItem;

// The item used to present the Password Manager widget promo.
@property(nonatomic, readonly) InlinePromoItem* widgetPromoItem;

// Deleting passwords updates the SavedPasswordsPresenter, resulting in an
// observer callback, which handles general data updates with a `reloadData`.
// Visually, it is better to handle user-initiated changes with more specific
// actions such as inserting or removing items/sections, instead of waiting on a
// data reload. This boolean is used to stop the observer callback from acting
// on user-initiated changes.
@property(nonatomic, readwrite, assign) BOOL deletionInProgress;

@end

@implementation PasswordManagerViewController {
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
  // Whether -viewDidAppear was called.
  BOOL _hasViewAppeared;
  // Whether the table view is in search mode. That is, it only has the search
  // bar potentially saved passwords and blocked sites.
  BOOL _tableIsInSearchMode;
  // Whether the favicon metric was already logged.
  BOOL _faviconMetricLogged;
  // Whether the search controller should be set as active when the view is
  // presented.
  BOOL _shouldOpenInSearchMode;
  // Whether or not a search user action was recorded for the current search
  // session.
  BOOL _searchPasswordsUserActionWasRecorded;
  // Whether or not the Password Manager widget promo should be shown.
  BOOL _shouldShowPasswordManagerWidgetPromo;
  // Stores the most recently created or updated password form.
  std::optional<password_manager::CredentialUIEntry> _mostRecentlyUpdatedCred;
}

@synthesize manageAccountLinkItem = _manageAccountLinkItem;
@synthesize widgetPromoItem = _widgetPromoItem;
@synthesize passwordProblemsItem = _passwordProblemsItem;
@synthesize checkForProblemsItem = _checkForProblemsItem;
@synthesize addPasswordItem = _addPasswordItem;

#pragma mark - Initialization

- (instancetype)initWithChromeAccountManagerService:
                    (ChromeAccountManagerService*)accountManagerService
                                        prefService:(PrefService*)prefService
                             shouldOpenInSearchMode:
                                 (BOOL)shouldOpenInSearchMode {
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
    _shouldOpenInSearchMode = shouldOpenInSearchMode;

    [self updateUIForEditState];
  }
  return self;
}

- (void)dealloc {
  // Not an invariant due to possible race conditions. DCHECKing for debugging
  // purposes. See crbug.com/40067451.
  DCHECK(!_accountManagerServiceObserver.get());
}

- (void)setReauthenticationModule:
    (ReauthenticationModule*)reauthenticationModule {
  _reauthenticationModule = reauthenticationModule;
}

- (void)setMostRecentlyUpdatedPasswordDetails:
    (const password_manager::CredentialUIEntry&)credential {
  _mostRecentlyUpdatedCred = credential;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  [self setUpTitle];

  self.tableView.allowsMultipleSelectionDuringEditing = YES;
  self.tableView.accessibilityIdentifier = kPasswordsTableViewID;

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
  searchBar.accessibilityIdentifier = kPasswordsSearchBarID;

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
  self.scrimView.accessibilityIdentifier = kPasswordsScrimViewID;
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

- (void)viewDidAppear:(BOOL)animated {
  [super viewDidAppear:animated];
  _hasViewAppeared = YES;
  [self maybeFocusSearchBar];
}

- (void)viewWillDisappear:(BOOL)animated {
  [super viewWillDisappear:animated];

  // viewWillDisappear is also called if you drag the sheet down then release
  // without actually closing.
  if (!_faviconMetricLogged) {
    [self logPercentageMetricForFavicons];
    _faviconMetricLogged = YES;
  }
}

- (void)viewDidDisappear:(BOOL)animated {
  [super viewDidDisappear:animated];

  // Dismiss the search bar if presented; otherwise UIKit may retain it and
  // cause a memory leak. If this dismissal happens before viewWillDisappear
  // (e.g., settingsWillBeDismissed) an internal UIKit crash occurs. See also:
  // crbug.com/947417, crbug.com/1350625. Dismissing in viewDidDisappear to make
  // sure that it happens when the view is well and truly gone.
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
  BOOL viewWasInEditMode = self.editing;
  [super setEditing:editing animated:animated];

  // The UI needs to be updated only when we are switching between editing
  // states (i.e. no-edit -> edit and edit -> no-edit). Updating the UI using
  // batchUpdate (or equivalent) when the view is already in edit mode, causes
  // the view to forcibly exit edit mode.
  if (viewWasInEditMode == editing) {
    return;
  }

  [self setSearchBarEnabled:self.shouldEnableSearchBar];
  [self setWidgetPromoItemEnabled:!editing];
  [self updatePasswordCheckButtonWithState:self.passwordCheckState];
  [self updatePasswordCheckStatusLabelWithState:self.passwordCheckState];
  [self setAddPasswordButtonEnabled:!editing];
  if ([self.navigationController.topViewController
          isKindOfClass:[PasswordManagerViewController class]]) {
    [self updateUIForEditState];
  }
  [self reconfigurePasswordCheckSectionCellsWithState:self.passwordCheckState];
}

- (BOOL)hasPasswords {
  return !_affiliatedGroups.empty();
}

- (void)viewWillLayoutSubviews {
  [super viewWillLayoutSubviews];
  [self updateWidgetPromoCellLayoutIfNeeded];
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

  // Don't show sections hidden when search controller is displayed.
  if (!_tableIsInSearchMode) {
    // Manage account header.
    [model addSectionWithIdentifier:SectionIdentifierManageAccountHeader];
    [model setHeader:self.manageAccountLinkItem
        forSectionWithIdentifier:SectionIdentifierManageAccountHeader];

    // Widget promo.
    if (_shouldShowPasswordManagerWidgetPromo) {
      [model addSectionWithIdentifier:SectionIdentifierWidgetPromo];
      [model addItem:self.widgetPromoItem
          toSectionWithIdentifier:SectionIdentifierWidgetPromo];
    }

    // Password check.
    [model addSectionWithIdentifier:SectionIdentifierPasswordCheck];

    [self updatePasswordCheckStatusLabelWithState:_passwordCheckState];
    [model addItem:self.passwordProblemsItem
        toSectionWithIdentifier:SectionIdentifierPasswordCheck];

    [self updatePasswordCheckButtonWithState:_passwordCheckState];

    // Only add check button if the current PasswordCheckUIState requires the
    // button to be shown.
    if (self.shouldShowCheckButton) {
      [model addItem:self.checkForProblemsItem
          toSectionWithIdentifier:SectionIdentifierPasswordCheck];
    }

    // Add Password button.
    if ([self allowsAddPassword]) {
      [model addSectionWithIdentifier:SectionIdentifierAddPasswordButton];
      [model addItem:self.addPasswordItem
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
  if ([self indexPathsContainsSavedPassword:indexPaths]) {
    // Show password delete dialog before deleting the passwords.
    NSMutableArray<NSString*>* origins = [[NSMutableArray alloc] init];
    for (NSIndexPath* indexPath : indexPaths) {
      NSInteger itemType = [self.tableViewModel itemTypeForIndexPath:indexPath];
      if (itemType == ItemTypeSavedPassword) {
        password_manager::AffiliatedGroup affiliatedGroup =
            base::apple::ObjCCastStrict<AffiliatedGroupTableViewItem>(
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
  CHECK(self.prefService);
  _accountManagerServiceObserver.reset();
  self.prefService = nullptr;
}

#pragma mark - Items
- (TableViewLinkHeaderFooterItem*)manageAccountLinkItem {
  if (_manageAccountLinkItem) {
    return _manageAccountLinkItem;
  }

  _manageAccountLinkItem =
      [[TableViewLinkHeaderFooterItem alloc] initWithType:ItemTypeLinkHeader];

  if (_savingPasswordsToAccount) {
    _manageAccountLinkItem.text =
        l10n_util::GetNSString(IDS_IOS_SAVE_PASSWORDS_MANAGE_ACCOUNT_HEADER);

    _manageAccountLinkItem.urls = @[ [[CrURL alloc]
        initWithGURL:
            google_util::AppendGoogleLocaleParam(
                GURL(password_manager::kPasswordManagerHelpCenteriOSURL),
                GetApplicationContext()->GetApplicationLocale())] ];
  } else {
    _manageAccountLinkItem.text =
        l10n_util::GetNSString(IDS_IOS_PASSWORD_MANAGER_HEADER_NOT_SYNCING);
    _manageAccountLinkItem.urls = @[];
  }

  return _manageAccountLinkItem;
}

- (InlinePromoItem*)widgetPromoItem {
  if (_widgetPromoItem) {
    return _widgetPromoItem;
  }

  _widgetPromoItem = [[InlinePromoItem alloc] initWithType:ItemTypeWidgetPromo];
  _widgetPromoItem.promoImage = [UIImage imageNamed:WidgetPromoImageName()];
  _widgetPromoItem.promoText =
      l10n_util::GetNSString(IDS_IOS_PASSWORD_MANAGER_WIDGET_PROMO_TEXT);
  _widgetPromoItem.moreInfoButtonTitle = l10n_util::GetNSString(
      IDS_IOS_PASSWORD_MANAGER_WIDGET_PROMO_BUTTON_TITLE);
  _widgetPromoItem.shouldHaveWideLayout =
      [self shouldWidgetPromoCellHaveWideLayout];
  _widgetPromoItem.accessibilityIdentifier = kWidgetPromoID;
  return _widgetPromoItem;
}

- (SettingsCheckItem*)passwordProblemsItem {
  if (_passwordProblemsItem) {
    return _passwordProblemsItem;
  }

  _passwordProblemsItem =
      [[SettingsCheckItem alloc] initWithType:ItemTypePasswordCheckStatus];
  _passwordProblemsItem.enabled = NO;
  _passwordProblemsItem.text = l10n_util::GetNSString(IDS_IOS_PASSWORD_CHECKUP);
  _passwordProblemsItem.detailText =
      l10n_util::GetNSString(IDS_IOS_PASSWORD_CHECKUP_DESCRIPTION);
  _passwordProblemsItem.accessibilityTraits = UIAccessibilityTraitHeader;
  return _passwordProblemsItem;
}

- (TableViewTextItem*)checkForProblemsItem {
  if (_checkForProblemsItem) {
    return _checkForProblemsItem;
  }

  _checkForProblemsItem =
      [[TableViewTextItem alloc] initWithType:ItemTypeCheckForProblemsButton];
  _checkForProblemsItem.text =
      l10n_util::GetNSString(IDS_IOS_CHECK_PASSWORDS_NOW_BUTTON);
  _checkForProblemsItem.textColor = [UIColor colorNamed:kTextSecondaryColor];
  _checkForProblemsItem.accessibilityTraits = UIAccessibilityTraitButton;
  return _checkForProblemsItem;
}

- (TableViewTextItem*)addPasswordItem {
  if (_addPasswordItem) {
    return _addPasswordItem;
  }

  _addPasswordItem =
      [[TableViewTextItem alloc] initWithType:ItemTypeAddPasswordButton];
  _addPasswordItem.text = l10n_util::GetNSString(IDS_IOS_ADD_PASSWORD);
  _addPasswordItem.accessibilityIdentifier = kAddPasswordButtonID;
  _addPasswordItem.accessibilityTraits = UIAccessibilityTraitButton;
  _addPasswordItem.textColor = [UIColor colorNamed:kBlueColor];
  return _addPasswordItem;
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

  if (_mostRecentlyUpdatedCred) {
    // Find the affiliated group item with a credential on the same sign-on
    // realm as the most recently updated credential.
    auto groupCreds = affiliatedGroup.GetCredentials();
    auto mostRecentlyUpdatedCred = *_mostRecentlyUpdatedCred;
    auto pred = [&mostRecentlyUpdatedCred](
                    const password_manager::CredentialUIEntry& entry) {
      return mostRecentlyUpdatedCred.GetFirstSignonRealm() ==
             entry.GetFirstSignonRealm();
    };
    if (auto it = std::find_if(groupCreds.begin(), groupCreds.end(), pred);
        it != groupCreds.end()) {
      self.mostRecentlyUpdatedItem = passwordItem;
      _mostRecentlyUpdatedCred = std::nullopt;
    }
  }
  return passwordItem;
}

- (BlockedSiteTableViewItem*)blockedSiteItem:
    (const password_manager::CredentialUIEntry&)credential {
  BlockedSiteTableViewItem* passwordItem =
      [[BlockedSiteTableViewItem alloc] initWithType:ItemTypeBlocked];
  passwordItem.credential = credential;
  passwordItem.accessibilityTraits |= UIAccessibilityTraitButton;
  passwordItem.accessoryType = UITableViewCellAccessoryDisclosureIndicator;
  return passwordItem;
}

#pragma mark - PopoverLabelViewControllerDelegate

- (void)didTapLinkURL:(NSURL*)URL {
  [self view:nil didTapLinkURL:[[CrURL alloc] initWithNSURL:URL]];
}

#pragma mark - Actions

// Called when user tapped on the information button of the password check
// item. Shows popover with detailed description of an error.
- (void)didTapPasswordCheckInfoButton:(UIButton*)buttonView {
  NSAttributedString* info = [self.delegate passwordCheckErrorInfo];
  // If no info returned by mediator handle this tap as tap on a cell.
  if (!info) {
    [self showPasswordCheckupPage];
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

- (void)didTapWidgetPromoCloseButton {
  UmaHistogramEnumeration(kPasswordManagerWidgetPromoActionHistogram,
                          PasswordManagerWidgetPromoAction::kClose);

  [self clearSectionWithIdentifier:SectionIdentifierWidgetPromo
                  withRowAnimation:UITableViewRowAnimationFade];
  [self.delegate notifyFETOfPasswordManagerWidgetPromoDismissal];
}

- (void)didTapWidgetPromoMoreInfoButton {
  UmaHistogramEnumeration(kPasswordManagerWidgetPromoActionHistogram,
                          PasswordManagerWidgetPromoAction::kOpenInstructions);

  [self.presentationDelegate showPasswordManagerWidgetPromoInstructions];
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

  [self reconfigurePasswordCheckSectionCellsWithState:state];

  _passwordCheckState = state;
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
  if (self.deletionInProgress) {
    return;
  }

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
    // Force UI update, as setting table view's editing state to disabled might
    // not update the UI.
    [self updateUIForEditState];
    [self setEditing:NO animated:YES];
    [self reloadData];
    return;
  }

  // Update the UI for the edit state to make sure it reflects the content in
  // the table as the content may have changed since the view controller was
  // created.
  if ([self.navigationController.topViewController
          isKindOfClass:[PasswordManagerViewController class]]) {
    [self updateUIForEditState];
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
      // This is very rare condition, in this case just reload all data and
      // update the toolbar UI.
      //  We want to update the toolbar only if the current view is the Password
      //  Manager.
      if ([self.navigationController.topViewController
              isKindOfClass:[PasswordManagerViewController class]]) {
        [self updateUIForEditState];
      }
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

- (void)setShouldShowPasswordManagerWidgetPromo:
    (BOOL)shouldShowPasswordManagerWidgetPromo {
  _shouldShowPasswordManagerWidgetPromo = shouldShowPasswordManagerWidgetPromo;

  // Reload data to display the promo. No to need to reload before the view is
  // loaded, as loading the view triggers a data reload.
  if (self.viewLoaded) {
    [self reloadData];
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

        [self clearSectionWithIdentifier:SectionIdentifierWidgetPromo
                        withRowAnimation:UITableViewRowAnimationTop];

        [self clearSectionWithIdentifier:SectionIdentifierManageAccountHeader
                        withRowAnimation:UITableViewRowAnimationTop];

        // Hide the toolbar when the search controller is presented.
        self.navigationController.toolbarHidden = YES;
      }
                        completion:nil];
}

- (void)willDismissSearchController:(UISearchController*)searchController {
  _searchPasswordsUserActionWasRecorded = false;

  // This is needed to restore the transparency of the navigation bar at
  // scroll edge in iOS 15+.
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
        [model setHeader:self.manageAccountLinkItem
            forSectionWithIdentifier:SectionIdentifierManageAccountHeader];
        [self.tableView
              insertSections:[NSIndexSet indexSetWithIndex:sectionIndex]
            withRowAnimation:UITableViewRowAnimationTop];

        sectionIndex++;
        // Add widget promo section.
        if (_shouldShowPasswordManagerWidgetPromo) {
          [model insertSectionWithIdentifier:SectionIdentifierWidgetPromo
                                     atIndex:sectionIndex];
          [self.tableView
                insertSections:[NSIndexSet indexSetWithIndex:sectionIndex]
              withRowAnimation:UITableViewRowAnimationTop];
          [model addItem:self.widgetPromoItem
              toSectionWithIdentifier:SectionIdentifierWidgetPromo];

          sectionIndex++;
        }

        // Add "Password check" section.
        [model insertSectionWithIdentifier:SectionIdentifierPasswordCheck
                                   atIndex:sectionIndex];
        NSInteger checkSection =
            [model sectionForSectionIdentifier:SectionIdentifierPasswordCheck];

        [self.tableView
              insertSections:[NSIndexSet indexSetWithIndex:sectionIndex]
            withRowAnimation:UITableViewRowAnimationTop];
        [model addItem:self.passwordProblemsItem
            toSectionWithIdentifier:SectionIdentifierPasswordCheck];
        [rowsIndexPaths addObject:[NSIndexPath indexPathForRow:0
                                                     inSection:checkSection]];
        // Only add check button if the current PasswordCheckUIState requires
        // the button to be shown.
        if (self.shouldShowCheckButton) {
          [model addItem:self.checkForProblemsItem
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
          [model addItem:self.addPasswordItem
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

  // Only record a search user action once per search session.
  if (!_searchPasswordsUserActionWasRecorded) {
    base::RecordAction(
        base::UserMetricsAction("MobilePasswordManagerSearchPasswords"));
    _searchPasswordsUserActionWasRecorded = YES;
  }
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

// Adds filtered list of saved passwords.
- (void)addPasswordsSectionWithSearchTerm:(NSString*)searchTerm {
  const std::string searchTermStr =
      searchTerm.length == 0
          ? std::string()
          : base::ToLowerASCII(base::SysNSStringToUTF8(searchTerm));
  for (const auto& affiliatedGroup : _affiliatedGroups) {
    if (searchTermStr.empty() || password_manager::MatchAffiliatedGroupsForTerm(
                                     affiliatedGroup, searchTermStr)) {
      AffiliatedGroupTableViewItem* item =
          [self savedFormItemForAffiliatedGroup:affiliatedGroup];
      [self.tableViewModel addItem:item
           toSectionWithIdentifier:SectionIdentifierSavedPasswords];
    }
  }
}

// Adds filtered list of blocked sites.
- (void)addBlockedSitesSectionWithSearchTerm:(NSString*)searchTerm {
  const std::string searchTermStr =
      searchTerm.length == 0
          ? std::string()
          : base::ToLowerASCII(base::SysNSStringToUTF8(searchTerm));
  for (const auto& credential : _blockedSites) {
    if (searchTermStr.empty() ||
        password_manager::MatchCredentialForTerm(credential, searchTermStr)) {
      BlockedSiteTableViewItem* item = [self blockedSiteItem:credential];
      [self.tableViewModel addItem:item
           toSectionWithIdentifier:SectionIdentifierBlocked];
    }
  }
}

// Rebuilds the filtered list of passwords/blocked based on given
// `searchTerm`.
- (void)filterItems:(NSString*)searchTerm {
  TableViewModel* model = self.tableViewModel;

  if ([self hasPasswords]) {
    [model deleteAllItemsFromSectionWithIdentifier:
               SectionIdentifierSavedPasswords];
    [self addPasswordsSectionWithSearchTerm:searchTerm];
  }

  if (!_blockedSites.empty()) {
    [model deleteAllItemsFromSectionWithIdentifier:SectionIdentifierBlocked];
    [self addBlockedSitesSectionWithSearchTerm:searchTerm];
  }
}

// Updates password check button according to provided state.
- (void)updatePasswordCheckButtonWithState:(PasswordCheckUIState)state {
  self.checkForProblemsItem.text =
      l10n_util::GetNSString(IDS_IOS_CHECK_PASSWORDS_NOW_BUTTON);

  if (self.editing) {
    [self setCheckForProblemsItemEnabled:NO];
    return;
  }

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
}

// Updates password check status label according to provided state.
- (void)updatePasswordCheckStatusLabelWithState:(PasswordCheckUIState)state {
  self.passwordProblemsItem.trailingImage = nil;
  self.passwordProblemsItem.trailingImageTintColor = nil;
  self.passwordProblemsItem.enabled = !self.editing;
  self.passwordProblemsItem.indicatorHidden = YES;
  self.passwordProblemsItem.infoButtonHidden = YES;
  self.passwordProblemsItem.accessoryType =
      IsPasswordCheckTappable(state)
          ? UITableViewCellAccessoryDisclosureIndicator
          : UITableViewCellAccessoryNone;
  self.passwordProblemsItem.text =
      l10n_util::GetNSString(IDS_IOS_PASSWORD_CHECKUP);
  self.passwordProblemsItem.detailText =
      l10n_util::GetNSString(IDS_IOS_PASSWORD_CHECKUP_DESCRIPTION);

  switch (state) {
    case PasswordCheckStateRunning: {
      self.passwordProblemsItem.text =
          l10n_util::GetNSString(IDS_IOS_PASSWORD_CHECKUP_ONGOING);
      self.passwordProblemsItem.detailText =
          base::SysUTF16ToNSString(l10n_util::GetPluralStringFUTF16(
              IDS_IOS_PASSWORD_CHECKUP_SITES_AND_APPS_COUNT,
              _affiliatedGroups.size()));
      self.passwordProblemsItem.indicatorHidden = NO;
      break;
    }
    case PasswordCheckStateDisabled: {
      self.passwordProblemsItem.enabled = NO;
      break;
    }
    case PasswordCheckStateUnmutedCompromisedPasswords: {
      self.passwordProblemsItem.detailText =
          base::SysUTF16ToNSString(l10n_util::GetPluralStringFUTF16(
              IDS_IOS_PASSWORD_CHECKUP_COMPROMISED_COUNT,
              self.insecurePasswordsCount));
      self.passwordProblemsItem.warningState = WarningState::kSevereWarning;
      break;
    }
    case PasswordCheckStateReusedPasswords: {
      self.passwordProblemsItem.detailText = l10n_util::GetNSStringF(
          IDS_IOS_PASSWORD_CHECKUP_REUSED_COUNT,
          base::NumberToString16(self.insecurePasswordsCount));
      self.passwordProblemsItem.warningState = WarningState::kWarning;
      break;
    }
    case PasswordCheckStateWeakPasswords: {
      self.passwordProblemsItem.detailText = base::SysUTF16ToNSString(
          l10n_util::GetPluralStringFUTF16(IDS_IOS_PASSWORD_CHECKUP_WEAK_COUNT,
                                           self.insecurePasswordsCount));
      self.passwordProblemsItem.warningState = WarningState::kWarning;
      break;
    }
    case PasswordCheckStateDismissedWarnings: {
      self.passwordProblemsItem.detailText =
          base::SysUTF16ToNSString(l10n_util::GetPluralStringFUTF16(
              IDS_IOS_PASSWORD_CHECKUP_DISMISSED_COUNT,
              self.insecurePasswordsCount));
      self.passwordProblemsItem.warningState = WarningState::kWarning;
      break;
    }
    case PasswordCheckStateSafe: {
      self.passwordProblemsItem.detailText =
          [self.delegate formattedElapsedTimeSinceLastCheck];
      self.passwordProblemsItem.warningState = WarningState::kSafe;
      break;
    }
    case PasswordCheckStateDefault:
      break;
    case PasswordCheckStateError:
    case PasswordCheckStateSignedOut: {
      self.passwordProblemsItem.detailText =
          l10n_util::GetNSString(IDS_IOS_PASSWORD_CHECKUP_ERROR);
      self.passwordProblemsItem.infoButtonHidden = NO;
      break;
    }
  }

  // Notify accessibility to focus on the Password Check Status cell if needed.
  if ([self shouldFocusAccessibilityOnPasswordCheckStatusForState:state]) {
    [self focusAccessibilityOnPasswordCheckStatus];
    self.checkWasTriggeredManually = NO;
  }

  // Apply the changes to the "Password Check" cell.
  [self reconfigureCellsForItems:@[ self.passwordProblemsItem ]];
}

// Enables or disables the `widgetPromoItem`.
- (void)setWidgetPromoItemEnabled:(BOOL)enabled {
  if (self.widgetPromoItem.enabled == enabled) {
    return;
  }

  self.widgetPromoItem.enabled = enabled;
  self.widgetPromoItem.promoImage =
      [UIImage imageNamed:enabled ? WidgetPromoImageName()
                                  : WidgetPromoDisabledImageName()];
  [self reconfigureCellsForItems:@[ self.widgetPromoItem ]];
}

// Enables or disables the `checkForProblemsItem` and sets it up accordingly.
- (void)setCheckForProblemsItemEnabled:(BOOL)enabled {
  self.checkForProblemsItem.enabled = enabled;
  if (enabled) {
    self.checkForProblemsItem.textColor = [UIColor colorNamed:kBlueColor];
    self.checkForProblemsItem.accessibilityTraits &=
        ~UIAccessibilityTraitNotEnabled;
  } else {
    self.checkForProblemsItem.textColor =
        [UIColor colorNamed:kTextSecondaryColor];
    self.checkForProblemsItem.accessibilityTraits |=
        UIAccessibilityTraitNotEnabled;
  }
}

- (void)setAddPasswordButtonEnabled:(BOOL)enabled {
  if (enabled) {
    self.addPasswordItem.textColor = [UIColor colorNamed:kBlueColor];
    self.addPasswordItem.accessibilityTraits &= ~UIAccessibilityTraitNotEnabled;
  } else {
    self.addPasswordItem.textColor = [UIColor colorNamed:kTextSecondaryColor];
    self.addPasswordItem.accessibilityTraits |= UIAccessibilityTraitNotEnabled;
  }
  [self reconfigureCellsForItems:@[ self.addPasswordItem ]];
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
  self.deletionInProgress = YES;

  std::vector<password_manager::CredentialUIEntry> credentialsToDelete;
  for (NSIndexPath* indexPath in indexPaths) {
    // Only form items are editable.
    NSInteger itemType = [self.tableViewModel itemTypeForIndexPath:indexPath];
    TableViewItem* item = [self.tableViewModel itemAtIndexPath:indexPath];

    // Remove affiliated group.
    if (itemType == ItemTypeSavedPassword) {
      password_manager::AffiliatedGroup affiliatedGroup =
          base::apple::ObjCCastStrict<AffiliatedGroupTableViewItem>(item)
              .affiliatedGroup;

      // Remove from local cache.
      auto iterator = base::ranges::find(_affiliatedGroups, affiliatedGroup);
      if (iterator != _affiliatedGroups.end())
        _affiliatedGroups.erase(iterator);

      // Add to the credentials to delete vector to remove from store.
      credentialsToDelete.insert(credentialsToDelete.end(),
                                 affiliatedGroup.GetCredentials().begin(),
                                 affiliatedGroup.GetCredentials().end());
    } else if (itemType == ItemTypeBlocked) {
      password_manager::CredentialUIEntry credential =
          base::apple::ObjCCastStrict<BlockedSiteTableViewItem>(item)
              .credential;

      auto removeCredential =
          [](std::vector<password_manager::CredentialUIEntry>& credentials,
             const password_manager::CredentialUIEntry& credential) {
            auto iterator = base::ranges::find(credentials, credential);
            if (iterator != credentials.end())
              credentials.erase(iterator);
          };
      removeCredential(_blockedSites, credential);
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
        strongSelf.deletionInProgress = NO;
      }];
  [self.delegate deleteCredentials:credentialsToDelete];
}

// Notifies the handler to show the Password Checkup homepage if the state of
// the Password Check cell allows it.
- (void)showPasswordCheckupPage {
  if (!IsPasswordCheckTappable(self.passwordCheckState)) {
    return;
  }
  base::RecordAction(
      base::UserMetricsAction("MobilePasswordManagerOpenPasswordCheckup"));
  [self.handler showPasswordCheckup];
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

  // Accessibility should focus on the Password Check Status cell when:
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

// Logs favicon percentage metric for the Password Manager.
- (void)logPercentageMetricForFavicons {
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

  if (n_images + n_monograms > 0) {
    base::UmaHistogramPercentage("IOS.PasswordManager.Favicons.Percentage",
                                 100.0f * n_images / (n_images + n_monograms));
  }
}

- (bool)allowsAddPassword {
  if (!self.prefService) {
    return NO;
  }
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

// Don't focus the searchBar before the view has loaded or if the empty state
// view is displayed. It's possible for the view to load before the model or
// vice versa.
- (void)maybeFocusSearchBar {
  if ([self shouldShowEmptyStateView]) {
    return;
  }

  if (!_hasViewAppeared) {
    return;
  }

  if (_shouldOpenInSearchMode) {
    // Queue search bar focus so the keyboard animation doesn't collide with
    // other animations.
    __weak __typeof(self.searchController.searchBar) weakSearchBar =
        self.searchController.searchBar;
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(^{
          [weakSearchBar becomeFirstResponder];
        }));

    _shouldOpenInSearchMode = NO;
  }
}

// Shows the empty state view when there is no content to display in the
// tableView, otherwise hides the empty state view if one is being displayed.
- (void)showOrHideEmptyView {
  if ([self shouldShowEmptyStateView]) {
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
    [self maybeFocusSearchBar];
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

// Reconfigures the cells of the Password Check section. Adds or removes the
// check button from the table view if needed.
- (void)reconfigurePasswordCheckSectionCellsWithState:
    (PasswordCheckUIState)state {
  if (![self.tableViewModel
          hasSectionForSectionIdentifier:SectionIdentifierPasswordCheck]) {
    return;
  }

  __weak __typeof(self) weakSelf = self;
  [self.tableView
      performBatchUpdates:^{
        if (weakSelf.passwordProblemsItem) {
          [weakSelf
              reconfigureCellsForItems:@[ weakSelf.passwordProblemsItem ]];
          // When in safe state, a custom accessibility label needs to be set
          // for the Password Checkup cell.
          if (state == PasswordCheckStateSafe) {
            [weakSelf setPasswordProblemsItemAccessibilityLabelForSafeState];
          }
        }
        if (weakSelf.checkForProblemsItem) {
          BOOL checkForProblemsItemIsInModel = [weakSelf.tableViewModel
              hasItemForItemType:ItemTypeCheckForProblemsButton
               sectionIdentifier:SectionIdentifierPasswordCheck];
          // Check if the check button should be removed from the table view.
          if (!weakSelf.shouldShowCheckButton &&
              checkForProblemsItemIsInModel) {
            NSIndexPath* checkButtonIndexPath = [weakSelf checkButtonIndexPath];
            [weakSelf
                removeFromModelItemAtIndexPaths:@[ checkButtonIndexPath ]];
            [weakSelf.tableView
                deleteRowsAtIndexPaths:@[ checkButtonIndexPath ]
                      withRowAnimation:UITableViewRowAnimationAutomatic];
          } else if (weakSelf.shouldShowCheckButton) {
            [weakSelf
                reconfigureCellsForItems:@[ weakSelf.checkForProblemsItem ]];
            // Check if the check button should be added to the table view.
            if (!checkForProblemsItemIsInModel) {
              [weakSelf.tableViewModel addItem:weakSelf.checkForProblemsItem
                       toSectionWithIdentifier:SectionIdentifierPasswordCheck];
              [weakSelf.tableView
                  insertRowsAtIndexPaths:@[ [weakSelf checkButtonIndexPath] ]
                        withRowAnimation:UITableViewRowAnimationAutomatic];
            }
          }
        }
        [weakSelf.tableView layoutIfNeeded];
      }
               completion:nil];
}

- (NSIndexPath*)checkButtonIndexPath {
  return
      [self.tableViewModel indexPathForItemType:ItemTypeCheckForProblemsButton
                              sectionIdentifier:SectionIdentifierPasswordCheck];
}

- (void)showDetailedViewPageForItem:(TableViewItem*)item {
  base::RecordAction(
      base::UserMetricsAction("MobilePasswordManagerOpenPasswordDetails"));
  [self.handler
      showDetailedViewForAffiliatedGroup:base::apple::ObjCCastStrict<
                                             AffiliatedGroupTableViewItem>(item)
                                             .affiliatedGroup];
}

// Returns whether or not the widget promo cell should be configured with its
// wide layout. Should return `YES` when the view's width is greater than the
// established threshold.
- (BOOL)shouldWidgetPromoCellHaveWideLayout {
  return self.view.frame.size.width > kWidgetPromoLayoutThreshold;
}

// Updates the layout of the widget promo cell when needed. Disables the
// animation while updating to prevent having a weird animation from
// `beginUpdates` and `endUpdates`. `beginUpdates` and `endUpdates` are needed
// to ensure that the cell will be correctly resized when switching from one
// layout to the other.
- (void)updateWidgetPromoCellLayoutIfNeeded {
  BOOL shouldHaveWideLayout = [self shouldWidgetPromoCellHaveWideLayout];

  if (_shouldShowPasswordManagerWidgetPromo &&
      shouldHaveWideLayout != self.widgetPromoItem.shouldHaveWideLayout) {
    [UIView setAnimationsEnabled:NO];
    [self.tableView beginUpdates];
    self.widgetPromoItem.shouldHaveWideLayout = shouldHaveWideLayout;
    [self reconfigureCellsForItems:@[ self.widgetPromoItem ]];
    [self.tableView endUpdates];
    [UIView setAnimationsEnabled:YES];
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
      [self showPasswordCheckupPage];
      break;
    case ItemTypeSavedPassword: {
      DCHECK_EQ(SectionIdentifierSavedPasswords,
                [model sectionIdentifierForSectionIndex:indexPath.section]);
      [self showDetailedViewPageForItem:[model itemAtIndexPath:indexPath]];
      break;
    }
    case ItemTypeBlocked: {
      DCHECK_EQ(SectionIdentifierBlocked,
                [model sectionIdentifierForSectionIndex:indexPath.section]);
      password_manager::CredentialUIEntry credential =
          base::apple::ObjCCastStrict<BlockedSiteTableViewItem>(
              [model itemAtIndexPath:indexPath])
              .credential;
      base::RecordAction(
          base::UserMetricsAction("MobilePasswordManagerOpenPasswordDetails"));
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
    case ItemTypeWidgetPromo:
      NOTREACHED_IN_MIGRATION();
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
      return self.checkForProblemsItem.isEnabled;
    case ItemTypeAddPasswordButton:
      return [self allowsAddPassword];
    case ItemTypeWidgetPromo:
      return NO;
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
        base::apple::ObjCCastStrict<TableViewLinkHeaderFooterView>(view);
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

// TODO(crbug.com/40282917): Stop downcasting cells to configure them.
- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  UITableViewCell* cell = [super tableView:tableView
                     cellForRowAtIndexPath:indexPath];
  switch ([self.tableViewModel itemTypeForIndexPath:indexPath]) {
    case ItemTypeWidgetPromo: {
      InlinePromoCell* widgetPromoCell =
          base::apple::ObjCCastStrict<InlinePromoCell>(cell);
      [widgetPromoCell.closeButton
                 addTarget:self
                    action:@selector(didTapWidgetPromoCloseButton)
          forControlEvents:UIControlEventTouchUpInside];
      [widgetPromoCell.moreInfoButton
                 addTarget:self
                    action:@selector(didTapWidgetPromoMoreInfoButton)
          forControlEvents:UIControlEventTouchUpInside];
      widgetPromoCell.closeButton.accessibilityIdentifier =
          kWidgetPromoCloseButtonID;
      widgetPromoCell.promoImageView.accessibilityIdentifier =
          kWidgetPromoImageID;
      break;
    }
    case ItemTypePasswordCheckStatus: {
      SettingsCheckCell* passwordCheckCell =
          base::apple::ObjCCastStrict<SettingsCheckCell>(cell);
      [passwordCheckCell.infoButton
                 addTarget:self
                    action:@selector(didTapPasswordCheckInfoButton:)
          forControlEvents:UIControlEventTouchUpInside];
      break;
    }
    case ItemTypeSavedPassword:
    case ItemTypeBlocked: {
      // Load the favicon from cache.
      [base::apple::ObjCCastStrict<PasswordFormContentCell>(cell)
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
