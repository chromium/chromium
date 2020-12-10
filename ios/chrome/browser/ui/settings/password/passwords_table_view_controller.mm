// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/passwords_table_view_controller.h"

#import <UIKit/UIKit.h>

#include "base/ios/ios_util.h"
#include "base/mac/foundation_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/sys_string_conversions.h"
#include "components/google/core/common/google_util.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_list_sorter.h"
#include "components/password_manager/core/browser/password_manager_constants.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/password_manager/core/browser/password_ui_utils.h"
#include "components/password_manager/core/browser/ui/password_check_referrer.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/main/browser.h"
#include "ios/chrome/browser/passwords/ios_chrome_password_store_factory.h"
#import "ios/chrome/browser/signin/chrome_identity_service_observer_bridge.h"
#include "ios/chrome/browser/system_flags.h"
#import "ios/chrome/browser/ui/elements/home_waiting_view.h"
#import "ios/chrome/browser/ui/settings/cells/settings_check_cell.h"
#import "ios/chrome/browser/ui/settings/cells/settings_check_item.h"
#import "ios/chrome/browser/ui/settings/cells/settings_switch_cell.h"
#import "ios/chrome/browser/ui/settings/cells/settings_switch_item.h"
#import "ios/chrome/browser/ui/settings/elements/enterprise_info_popover_view_controller.h"
#import "ios/chrome/browser/ui/settings/password/password_exporter.h"
#import "ios/chrome/browser/ui/settings/password/passwords_consumer.h"
#import "ios/chrome/browser/ui/settings/password/passwords_settings_commands.h"
#import "ios/chrome/browser/ui/settings/password/passwords_table_view_constants.h"
#import "ios/chrome/browser/ui/settings/password/passwords_table_view_controller_delegate.h"
#import "ios/chrome/browser/ui/settings/password/passwords_table_view_controller_presentation_delegate.h"
#import "ios/chrome/browser/ui/settings/utils/pref_backed_boolean.h"
#import "ios/chrome/browser/ui/settings/utils/settings_utils.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_detail_text_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_info_button_cell.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_info_button_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_link_header_footer_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_text_header_footer_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_text_item.h"
#import "ios/chrome/browser/ui/table_view/table_view_navigation_controller_constants.h"
#import "ios/chrome/browser/ui/table_view/table_view_utils.h"
#include "ios/chrome/browser/ui/ui_feature_flags.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/UIColor+cr_semantic_colors.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/elements/popover_label_view_controller.h"
#import "ios/chrome/common/ui/reauthentication/reauthentication_module.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#include "ios/chrome/grit/ios_chromium_strings.h"
#include "ios/chrome/grit/ios_strings.h"
#import "net/base/mac/url_conversions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

using base::UmaHistogramEnumeration;
using password_manager::metrics_util::PasswordCheckInteraction;

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierSavePasswordsSwitch = kSectionIdentifierEnumZero,
  SectionIdentifierSavedPasswords,
  SectionIdentifierBlocked,
  SectionIdentifierExportPasswordsButton,
  SectionIdentifierPasswordCheck,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeLinkHeader = kItemTypeEnumZero,
  ItemTypeHeader,
  ItemTypeSavePasswordsSwitch,
  ItemTypeManagedSavePasswords,
  ItemTypePasswordCheckStatus,
  ItemTypeCheckForProblemsButton,
  ItemTypeLastCheckTimestampFooter,
  ItemTypeSavedPassword,  // This is a repeated item type.
  ItemTypeBlocked,        // This is a repeated item type.
  ItemTypeExportPasswordsButton,
};

std::vector<std::unique_ptr<password_manager::PasswordForm>> CopyOf(
    const std::vector<std::unique_ptr<password_manager::PasswordForm>>&
        password_list) {
  std::vector<std::unique_ptr<password_manager::PasswordForm>>
      password_list_copy;
  for (const auto& form : password_list) {
    password_list_copy.push_back(
        std::make_unique<password_manager::PasswordForm>(*form));
  }
  return password_list_copy;
}

}  // namespace

@interface PasswordFormContentItem : TableViewDetailTextItem
@property(nonatomic) password_manager::PasswordForm* form;
@end
@implementation PasswordFormContentItem
@end

// Use the type of the items to convey the Saved/Blocked status.
@interface SavedFormContentItem : PasswordFormContentItem
@end
@implementation SavedFormContentItem
@end
@interface BlockedFormContentItem : PasswordFormContentItem
@end
@implementation BlockedFormContentItem
@end

@protocol PasswordExportActivityViewControllerDelegate <NSObject>

// Used to reset the export state when the activity view disappears.
- (void)resetExport;

@end

@interface PasswordExportActivityViewController : UIActivityViewController

- (PasswordExportActivityViewController*)
    initWithActivityItems:(NSArray*)activityItems
                 delegate:
                     (id<PasswordExportActivityViewControllerDelegate>)delegate;

@end

@implementation PasswordExportActivityViewController {
  __weak id<PasswordExportActivityViewControllerDelegate> _weakDelegate;
}

- (PasswordExportActivityViewController*)
    initWithActivityItems:(NSArray*)activityItems
                 delegate:(id<PasswordExportActivityViewControllerDelegate>)
                              delegate {
  self = [super initWithActivityItems:activityItems applicationActivities:nil];
  if (self) {
    _weakDelegate = delegate;
  }

  return self;
}

- (void)viewDidDisappear:(BOOL)animated {
  [_weakDelegate resetExport];
  [super viewDidDisappear:animated];
}

@end

@interface PasswordsTableViewController () <
    BooleanObserver,
    ChromeIdentityServiceObserver,
    PasswordExporterDelegate,
    PasswordExportActivityViewControllerDelegate,
    PasswordsConsumer,
    PopoverLabelViewControllerDelegate,
    UISearchBarDelegate,
    UISearchControllerDelegate> {
  // The observable boolean that binds to the password manager setting state.
  // Saved passwords are only on if the password manager is enabled.
  PrefBackedBoolean* _passwordManagerEnabled;
  // The header for save passwords switch section.
  TableViewLinkHeaderFooterItem* _manageAccountLinkItem;
  // The item related to the switch for the password manager setting.
  SettingsSwitchItem* _savePasswordsItem;
  // The item related to the enterprise managed save password setting.
  TableViewInfoButtonItem* _managedSavePasswordItem;
  // The item related to the password check status.
  SettingsCheckItem* _passwordProblemsItem;
  // The button to start password check.
  TableViewTextItem* _checkForProblemsItem;
  // The item related to the button for exporting passwords.
  TableViewTextItem* _exportPasswordsItem;
  // The interface for getting and manipulating a user's saved passwords.
  scoped_refptr<password_manager::PasswordStore> _passwordStore;
  // The list of the user's saved passwords.
  std::vector<std::unique_ptr<password_manager::PasswordForm>> _savedForms;
  // The list of the user's blocked sites.
  std::vector<std::unique_ptr<password_manager::PasswordForm>> _blockedForms;
  // Map containing duplicates of saved passwords.
  password_manager::DuplicatesMap _savedPasswordDuplicates;
  // Map containing duplicates of blocked passwords.
  password_manager::DuplicatesMap _blockedPasswordDuplicates;
  // The browser where the screen is being displayed.
  Browser* _browser;
  // The current Chrome browser state.
  ChromeBrowserState* _browserState;
  // Authentication Service Observer.
  std::unique_ptr<ChromeIdentityServiceObserverBridge> _identityServiceObserver;
  // Boolean containing whether the export operation is ready. This implies that
  // the exporter is idle and there is at least one saved passwords to export.
  BOOL _exportReady;
  // Boolean indicating if password forms have been received for the first time.
  // Used to show a loading indicator while waiting for the store response.
  BOOL _didReceiveSavedForms;
  // Alert informing the user that passwords are being prepared for
  // export.
  UIAlertController* _preparingPasswordsAlert;
}

// Object handling passwords export operations.
@property(nonatomic, strong) PasswordExporter* passwordExporter;

// Current passwords search term.
@property(nonatomic, copy) NSString* searchTerm;

// The scrim view that covers the table view when search bar is focused with
// empty search term. Tapping on the scrim view will dismiss the search bar.
@property(nonatomic, strong) UIControl* scrimView;

// Example headers for calculating headers' heights.
@property(nonatomic, strong)
    NSMutableDictionary<Class, UITableViewHeaderFooterView*>* exampleHeaders;

// The loading spinner background which appears when loading passwords.
@property(nonatomic, strong) HomeWaitingView* spinnerView;

// Current state of the Password Check.
@property(nonatomic, assign) PasswordCheckUIState passwordCheckState;

// Number of compromised passwords.
@property(assign) NSInteger compromisedPasswordsCount;

@end

@implementation PasswordsTableViewController

#pragma mark - Initialization

- (instancetype)initWithBrowser:(Browser*)browser {
  DCHECK(browser);

  self = [super initWithStyle:ChromeTableViewStyle()];
  if (self) {
    _browser = browser;
    _browserState = browser->GetBrowserState();
    self.exampleHeaders = [[NSMutableDictionary alloc] init];
    self.title = l10n_util::GetNSString(IDS_IOS_PASSWORDS);
    self.shouldHideDoneButton = YES;
    self.searchTerm = @"";
    _passwordStore = IOSChromePasswordStoreFactory::GetForBrowserState(
        _browserState, ServiceAccessType::EXPLICIT_ACCESS);
    DCHECK(_passwordStore);
    _passwordManagerEnabled = [[PrefBackedBoolean alloc]
        initWithPrefService:_browserState->GetPrefs()
                   prefName:password_manager::prefs::kCredentialsEnableService];
    [_passwordManagerEnabled setObserver:self];
    [self updateUIForEditState];
    [self updateExportPasswordsButton];
  }
  return self;
}

- (void)setReauthenticationModule:
    (ReauthenticationModule*)reauthenticationModule {
  _reauthenticationModule = reauthenticationModule;
  _passwordExporter = [[PasswordExporter alloc]
      initWithReauthenticationModule:_reauthenticationModule
                            delegate:self];
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
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
  searchController.obscuresBackgroundDuringPresentation = NO;
  searchController.delegate = self;
  searchController.searchBar.delegate = self;
  searchController.searchBar.backgroundColor = UIColor.clearColor;
  searchController.searchBar.accessibilityIdentifier = kPasswordsSearchBarId;
  // Center search bar and cancel button vertically so it looks centered
  // in the header when searching.
  UIOffset offset =
      UIOffsetMake(0.0f, kTableViewNavigationVerticalOffsetForSearchHeader);
  searchController.searchBar.searchFieldBackgroundPositionAdjustment = offset;

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

  if (!_didReceiveSavedForms) {
    [self showLoadingSpinnerBackground];
  }
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];

  // Center search bar's cancel button vertically so it looks centered.
  // We change the cancel button proxy styles, so we will return it to
  // default in viewDidDisappear.
  UIOffset offset =
      UIOffsetMake(0.0f, kTableViewNavigationVerticalOffsetForSearchHeader);
  UIBarButtonItem* cancelButton = [UIBarButtonItem
      appearanceWhenContainedInInstancesOfClasses:@ [[UISearchBar class]]];
  [cancelButton setTitlePositionAdjustment:offset
                             forBarMetrics:UIBarMetricsDefault];
}

- (void)viewWillDisappear:(BOOL)animated {
  [super viewWillDisappear:animated];

  // Restore to default origin offset for cancel button proxy style.
  UIBarButtonItem* cancelButton = [UIBarButtonItem
      appearanceWhenContainedInInstancesOfClasses:@ [[UISearchBar class]]];
  [cancelButton setTitlePositionAdjustment:UIOffsetZero
                             forBarMetrics:UIBarMetricsDefault];
}

- (void)didMoveToParentViewController:(UIViewController*)parent {
  [super didMoveToParentViewController:parent];
  if (!parent) {
    [self.presentationDelegate passwordsTableViewControllerDismissed];
  }
}

- (void)setEditing:(BOOL)editing animated:(BOOL)animated {
  [super setEditing:editing animated:animated];
  if (editing) {
    [self setSavePasswordsSwitchItemEnabled:NO];
    [self setExportPasswordsButtonEnabled:NO];
    [self setSearchBarEnabled:NO];
  } else {
    [self setSavePasswordsSwitchItemEnabled:YES];
    if (_exportReady) {
      [self setExportPasswordsButtonEnabled:YES];
    }
    [self setSearchBarEnabled:YES];
  }
  [self updateUIForEditState];
}

#pragma mark - SettingsRootTableViewController

- (void)loadModel {
  [super loadModel];

  if (!_didReceiveSavedForms) {
    return;
  }

  TableViewModel* model = self.tableViewModel;

  // Save passwords switch and manage account message. Only show this section
  // when the searchController is not active.
  if (!self.navigationItem.searchController.active) {
    [model addSectionWithIdentifier:SectionIdentifierSavePasswordsSwitch];

    if (base::FeatureList::IsEnabled(kEnableIOSManagedSettingsUI) &&
        _browserState->GetPrefs()->IsManagedPreference(
            password_manager::prefs::kCredentialsEnableService)) {
      // TODO(crbug.com/1082827): observe the managing status of the pref.
      // Show managed settings UI when the pref is managed by the policy.
      _managedSavePasswordItem = [self managedSavePasswordItem];
      [model addItem:_managedSavePasswordItem
          toSectionWithIdentifier:SectionIdentifierSavePasswordsSwitch];
    } else {
      _savePasswordsItem = [self savePasswordsItem];
      [model addItem:_savePasswordsItem
          toSectionWithIdentifier:SectionIdentifierSavePasswordsSwitch];
    }

    _manageAccountLinkItem = [self manageAccountLinkItem];
    [model setHeader:_manageAccountLinkItem
        forSectionWithIdentifier:SectionIdentifierSavePasswordsSwitch];
  }

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
  [model addItem:_checkForProblemsItem
      toSectionWithIdentifier:SectionIdentifierPasswordCheck];

  [self updateLastCheckTimestampWithState:_passwordCheckState
                                fromState:_passwordCheckState
                                   update:NO];

  // Saved passwords.
  if (!_savedForms.empty()) {
    [model addSectionWithIdentifier:SectionIdentifierSavedPasswords];
    TableViewTextHeaderFooterItem* headerItem =
        [[TableViewTextHeaderFooterItem alloc] initWithType:ItemTypeHeader];
    headerItem.text =
        l10n_util::GetNSString(IDS_IOS_SETTINGS_PASSWORDS_SAVED_HEADING);
    [model setHeader:headerItem
        forSectionWithIdentifier:SectionIdentifierSavedPasswords];
  }

  // Blocked passwords.
  if (!_blockedForms.empty()) {
    [model addSectionWithIdentifier:SectionIdentifierBlocked];
    TableViewTextHeaderFooterItem* headerItem =
        [[TableViewTextHeaderFooterItem alloc] initWithType:ItemTypeHeader];
    headerItem.text =
        l10n_util::GetNSString(IDS_IOS_SETTINGS_PASSWORDS_EXCEPTIONS_HEADING);
    [model setHeader:headerItem
        forSectionWithIdentifier:SectionIdentifierBlocked];
  }

  // Export passwords button.
  [model addSectionWithIdentifier:SectionIdentifierExportPasswordsButton];
  _exportPasswordsItem = [self exportPasswordsItem];
  [model addItem:_exportPasswordsItem
      toSectionWithIdentifier:SectionIdentifierExportPasswordsButton];

  [self filterItems:self.searchTerm];
}

- (void)deleteItems:(NSArray<NSIndexPath*>*)indexPaths {
  // Do not call super as this also deletes the section if it is empty.
  [self deleteItemAtIndexPaths:indexPaths];
}

- (void)reloadData {
  [super reloadData];
  [self updateExportPasswordsButton];
}

- (BOOL)shouldShowEditButton {
  return YES;
}

- (BOOL)editButtonEnabled {
  DCHECK([self shouldShowEditButton]);
  return !_savedForms.empty() || !_blockedForms.empty();
}

#pragma mark - SettingsControllerProtocol

- (void)reportDismissalUserAction {
  base::RecordAction(base::UserMetricsAction("MobilePasswordsSettingsClose"));
}

- (void)reportBackUserAction {
  base::RecordAction(base::UserMetricsAction("MobilePasswordsSettingsBack"));
}

- (void)settingsWillBeDismissed {
  // Dismiss the search bar if presented, otherwise the VC will be retained by
  // UIKit thus cause a memory leak.
  // TODO(crbug.com/947417): Remove this once the memory leak issue is fixed.
  if (self.navigationItem.searchController.active == YES) {
    self.navigationItem.searchController.active = NO;
  }
}

#pragma mark - Items

- (TableViewLinkHeaderFooterItem*)manageAccountLinkItem {
  TableViewLinkHeaderFooterItem* footerItem =
      [[TableViewLinkHeaderFooterItem alloc] initWithType:ItemTypeLinkHeader];
  footerItem.text =
      l10n_util::GetNSString(IDS_IOS_SAVE_PASSWORDS_MANAGE_ACCOUNT);
  footerItem.linkURL = google_util::AppendGoogleLocaleParam(
      GURL(password_manager::kPasswordManagerAccountDashboardURL),
      GetApplicationContext()->GetApplicationLocale());
  return footerItem;
}

- (SettingsSwitchItem*)savePasswordsItem {
  SettingsSwitchItem* savePasswordsItem =
      [[SettingsSwitchItem alloc] initWithType:ItemTypeSavePasswordsSwitch];
  savePasswordsItem.text = l10n_util::GetNSString(IDS_IOS_SAVE_PASSWORDS);
  savePasswordsItem.on = [_passwordManagerEnabled value];
  savePasswordsItem.accessibilityIdentifier = kSavePasswordSwitchTableViewId;
  return savePasswordsItem;
}

- (TableViewInfoButtonItem*)managedSavePasswordItem {
  TableViewInfoButtonItem* managedSavePasswordItem =
      [[TableViewInfoButtonItem alloc]
          initWithType:ItemTypeManagedSavePasswords];
  managedSavePasswordItem.text = l10n_util::GetNSString(IDS_IOS_SAVE_PASSWORDS);
  managedSavePasswordItem.statusText =
      [_passwordManagerEnabled value]
          ? l10n_util::GetNSString(IDS_IOS_SETTING_ON)
          : l10n_util::GetNSString(IDS_IOS_SETTING_OFF);
  managedSavePasswordItem.accessibilityHint =
      l10n_util::GetNSString(IDS_IOS_TOGGLE_SETTING_MANAGED_ACCESSIBILITY_HINT);
  managedSavePasswordItem.accessibilityIdentifier =
      kSavePasswordManagedTableViewId;
  return managedSavePasswordItem;
}

- (SettingsCheckItem*)passwordProblemsItem {
  SettingsCheckItem* passwordProblemsItem =
      [[SettingsCheckItem alloc] initWithType:ItemTypePasswordCheckStatus];
  passwordProblemsItem.enabled = NO;
  passwordProblemsItem.text = l10n_util::GetNSString(IDS_IOS_CHECK_PASSWORDS);
  passwordProblemsItem.detailText =
      l10n_util::GetNSString(IDS_IOS_CHECK_PASSWORDS_DESCRIPTION);
  return passwordProblemsItem;
}

- (TableViewTextItem*)checkForProblemsItem {
  TableViewTextItem* checkForProblemsItem =
      [[TableViewTextItem alloc] initWithType:ItemTypeCheckForProblemsButton];
  checkForProblemsItem.text =
      l10n_util::GetNSString(IDS_IOS_CHECK_PASSWORDS_NOW_BUTTON);
  checkForProblemsItem.textColor = UIColor.cr_secondaryLabelColor;
  checkForProblemsItem.accessibilityTraits = UIAccessibilityTraitButton;
  return checkForProblemsItem;
}

- (TableViewLinkHeaderFooterItem*)lastCompletedCheckTime {
  TableViewLinkHeaderFooterItem* footerItem =
      [[TableViewLinkHeaderFooterItem alloc]
          initWithType:ItemTypeLastCheckTimestampFooter];
  footerItem.text = [self.delegate formatElapsedTimeSinceLastCheck];
  return footerItem;
}

- (TableViewTextItem*)exportPasswordsItem {
  TableViewTextItem* exportPasswordsItem =
      [[TableViewTextItem alloc] initWithType:ItemTypeExportPasswordsButton];
  exportPasswordsItem.text = l10n_util::GetNSString(IDS_IOS_EXPORT_PASSWORDS);
  exportPasswordsItem.textColor = [UIColor colorNamed:kBlueColor];
  exportPasswordsItem.accessibilityIdentifier = @"exportPasswordsItem_button";
  exportPasswordsItem.accessibilityTraits = UIAccessibilityTraitButton;
  return exportPasswordsItem;
}

- (SavedFormContentItem*)savedFormItemWithText:(NSString*)text
                                 andDetailText:(NSString*)detailText
                                       forForm:(password_manager::PasswordForm*)
                                                   form {
  SavedFormContentItem* passwordItem =
      [[SavedFormContentItem alloc] initWithType:ItemTypeSavedPassword];
  passwordItem.text = text;
  passwordItem.form = form;
  passwordItem.detailText = detailText;
  passwordItem.accessibilityTraits |= UIAccessibilityTraitButton;
  passwordItem.accessoryType = UITableViewCellAccessoryDisclosureIndicator;
  return passwordItem;
}

- (BlockedFormContentItem*)
    blockedFormItemWithText:(NSString*)text
                    forForm:(password_manager::PasswordForm*)form {
  BlockedFormContentItem* passwordItem =
      [[BlockedFormContentItem alloc] initWithType:ItemTypeBlocked];
  passwordItem.text = text;
  passwordItem.form = form;
  passwordItem.accessibilityTraits |= UIAccessibilityTraitButton;
  passwordItem.accessoryType = UITableViewCellAccessoryDisclosureIndicator;
  return passwordItem;
}

#pragma mark - PopoverLabelViewControllerDelegate

- (void)didTapLinkURL:(NSURL*)URL {
  GURL convertedURL = net::GURLWithNSURL(URL);
  [self view:nil didTapLinkURL:convertedURL];
}

#pragma mark - BooleanObserver

- (void)booleanDidChange:(id<ObservableBoolean>)observableBoolean {
  if (observableBoolean == _passwordManagerEnabled) {
    if (_savePasswordsItem) {
      // Update the item.
      _savePasswordsItem.on = [_passwordManagerEnabled value];

      // Update the cell if it's not removed by presenting search controller.
      if ([self.tableViewModel
              hasItemForItemType:ItemTypeSavePasswordsSwitch
               sectionIdentifier:SectionIdentifierSavePasswordsSwitch]) {
        [self reconfigureCellsForItems:@[ _savePasswordsItem ]];
      }
    } else {
      _managedSavePasswordItem.detailText =
          [_passwordManagerEnabled value]
              ? l10n_util::GetNSString(IDS_IOS_SETTING_ON)
              : l10n_util::GetNSString(IDS_IOS_SETTING_OFF);
    }
  } else {
    NOTREACHED();
  }
}

#pragma mark - Actions

- (void)savePasswordsSwitchChanged:(UISwitch*)switchView {
  // Update the setting.
  [_passwordManagerEnabled setValue:switchView.on];

  // Update the item.
  _savePasswordsItem.on = [_passwordManagerEnabled value];
}

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
    [self showPasswordIssuesPage];
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
      compromisedPasswordsCount:(NSInteger)count {
  self.compromisedPasswordsCount = count;
  // Update password check status and check button with new state.
  [self updatePasswordCheckButtonWithState:state];
  [self updatePasswordCheckStatusLabelWithState:state];

  // During searching Password Check section is hidden so cells should not be
  // reconfigured.
  if (self.navigationItem.searchController.active) {
    _passwordCheckState = state;
    return;
  }

  if (_checkForProblemsItem)
    [self reconfigureCellsForItems:@[ _checkForProblemsItem ]];
  if (_passwordProblemsItem) {
    [self reconfigureCellsForItems:@[ _passwordProblemsItem ]];
  }
  // Before updating cached state value update timestamp as for proper animation
  // it requires both new and old values.
  [self updateLastCheckTimestampWithState:state
                                fromState:_passwordCheckState
                                   update:YES];
  _passwordCheckState = state;
}

- (void)setPasswordsForms:
    (std::vector<std::unique_ptr<password_manager::PasswordForm>>)results {
  _blockedForms.clear();
  _savedForms.clear();
  _savedPasswordDuplicates.clear();
  _blockedPasswordDuplicates.clear();

  _didReceiveSavedForms = YES;
  [self hideLoadingSpinnerBackground];
  if (results.empty()) {
    [self reloadData];
    return;
  }
  for (auto& form : results) {
    if (form->blocked_by_user)
      _blockedForms.push_back(std::move(form));
    else
      _savedForms.push_back(std::move(form));
  }

  password_manager::SortEntriesAndHideDuplicates(&_savedForms,
                                                 &_savedPasswordDuplicates);
  password_manager::SortEntriesAndHideDuplicates(&_blockedForms,
                                                 &_blockedPasswordDuplicates);

  [self updateUIForEditState];
  [self reloadData];
}

#pragma mark - UITableViewDelegate

// Uses a group of example headers to calculate the heights. Returning
// UITableViewAutomaticDimension here will cause UITableView to cache the
// heights and reuse them when sections are inserted or removed, which will
// break the UI. For example:
//   1. UITableView is inited with 4 headers;
//   2. "tableView:heightForHeaderInSection" is called and
//      UITableViewAutomaticDimension is returned;
//   3. UITableView calculates headers' heights and get [10, 20, 10, 20];
//   4. UITableView caches these heights;
//   5. The first header is removed from UITableView;
//   6. "tableView:heightForHeaderInSection" is called and
//      UITableViewAutomaticDimension is returned;
//   7. UITableView decides to use cached results as [10, 20, 10], while
//      expected heights are [20, 10, 20].
- (CGFloat)tableView:(UITableView*)tableView
    heightForHeaderInSection:(NSInteger)section {
  if ([self.tableViewModel headerForSection:section]) {
    TableViewHeaderFooterItem* item =
        [self.tableViewModel headerForSection:section];
    Class headerClass = item.cellClass;
    if (!self.exampleHeaders[headerClass]) {
      UITableViewHeaderFooterView* view =
          [[headerClass alloc] initWithReuseIdentifier:@""];
      [item configureHeaderFooterView:view withStyler:self.styler];
      [self.exampleHeaders setObject:view forKey:headerClass];
    }
    UITableViewHeaderFooterView* view = self.exampleHeaders[headerClass];
    CGSize size =
        [view systemLayoutSizeFittingSize:self.tableView.safeAreaLayoutGuide
                                              .layoutFrame.size
            withHorizontalFittingPriority:UILayoutPriorityRequired
                  verticalFittingPriority:1];
    return size.height;
  }
  return [super tableView:tableView heightForHeaderInSection:section];
}

#pragma mark - UISearchControllerDelegate

- (void)willPresentSearchController:(UISearchController*)searchController {
  [self showScrim];
  // Remove save passwords switch section and password check section.
  [self
      performBatchTableViewUpdates:^{
        [self clearSectionWithIdentifier:SectionIdentifierPasswordCheck
                        withRowAnimation:UITableViewRowAnimationTop];

        [self clearSectionWithIdentifier:SectionIdentifierSavePasswordsSwitch
                        withRowAnimation:UITableViewRowAnimationTop];
      }
                        completion:nil];
}

- (void)willDismissSearchController:(UISearchController*)searchController {
  [self hideScrim];
  [self searchForTerm:@""];
  // Recover save passwords switch section.
  TableViewModel* model = self.tableViewModel;
  [self.tableView
      performBatchUpdates:^{
        [model insertSectionWithIdentifier:SectionIdentifierSavePasswordsSwitch
                                   atIndex:0];
        [model setHeader:_manageAccountLinkItem
            forSectionWithIdentifier:SectionIdentifierSavePasswordsSwitch];
        [self.tableView insertSections:[NSIndexSet indexSetWithIndex:0]
                      withRowAnimation:UITableViewRowAnimationTop];
        if (_savePasswordsItem) {
          [model addItem:_savePasswordsItem
              toSectionWithIdentifier:SectionIdentifierSavePasswordsSwitch];
        } else {
          [model addItem:_managedSavePasswordItem
              toSectionWithIdentifier:SectionIdentifierSavePasswordsSwitch];
        }
        NSInteger switchSection = [model
            sectionForSectionIdentifier:SectionIdentifierSavePasswordsSwitch];
        NSMutableArray<NSIndexPath*>* rowsIndexPaths = [NSMutableArray
            arrayWithObjects:[NSIndexPath indexPathForRow:0
                                                inSection:switchSection],
                             nil];
        [model insertSectionWithIdentifier:SectionIdentifierPasswordCheck
                                   atIndex:1];
        NSInteger checkSection =
            [model sectionForSectionIdentifier:SectionIdentifierPasswordCheck];

        [self.tableView insertSections:[NSIndexSet indexSetWithIndex:1]
                      withRowAnimation:UITableViewRowAnimationTop];
        [model addItem:_passwordProblemsItem
            toSectionWithIdentifier:SectionIdentifierPasswordCheck];
        [model addItem:_checkForProblemsItem
            toSectionWithIdentifier:SectionIdentifierPasswordCheck];
        [rowsIndexPaths addObject:[NSIndexPath indexPathForRow:0
                                                     inSection:checkSection]];
        [rowsIndexPaths addObject:[NSIndexPath indexPathForRow:1
                                                     inSection:checkSection]];

        [self.tableView insertRowsAtIndexPaths:rowsIndexPaths
                              withRowAnimation:UITableViewRowAnimationTop];
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
  if (self.spinnerView) {
    [self.spinnerView stopWaitingWithCompletion:^{
      [UIView animateWithDuration:0.2
          animations:^{
            self.spinnerView.alpha = 0.0;
          }
          completion:^(BOOL finished) {
            self.navigationItem.searchController.searchBar
                .userInteractionEnabled = YES;
            self.tableView.backgroundView = nil;
            self.spinnerView = nil;
          }];
    }];
  }
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

// Builds the filtered list of passwords/blocked based on given
// |searchTerm|.
- (void)filterItems:(NSString*)searchTerm {
  TableViewModel* model = self.tableViewModel;

  if (!_savedForms.empty()) {
    [model deleteAllItemsFromSectionWithIdentifier:
               SectionIdentifierSavedPasswords];
    for (const auto& form : _savedForms) {
      NSString* text = base::SysUTF8ToNSString(
          password_manager::GetShownOriginAndLinkUrl(*form).first);
      NSString* detailText = base::SysUTF16ToNSString(form->username_value);
      bool hidden =
          searchTerm.length > 0 &&
          ![text localizedCaseInsensitiveContainsString:searchTerm] &&
          ![detailText localizedCaseInsensitiveContainsString:searchTerm];
      if (hidden)
        continue;
      [model addItem:[self savedFormItemWithText:text
                                   andDetailText:detailText
                                         forForm:form.get()]
          toSectionWithIdentifier:SectionIdentifierSavedPasswords];
    }
  }

  if (!_blockedForms.empty()) {
    [model deleteAllItemsFromSectionWithIdentifier:SectionIdentifierBlocked];
    for (const auto& form : _blockedForms) {
      NSString* text = base::SysUTF8ToNSString(
          password_manager::GetShownOriginAndLinkUrl(*form).first);
      bool hidden = searchTerm.length > 0 &&
                    ![text localizedCaseInsensitiveContainsString:searchTerm];
      if (hidden)
        continue;
      [model addItem:[self blockedFormItemWithText:text forForm:form.get()]
          toSectionWithIdentifier:SectionIdentifierBlocked];
    }
  }
}

// Update timestamp of the last check. Both old and new password check state
// should be provided in order to animate footer in a proper way.
- (void)updateLastCheckTimestampWithState:(PasswordCheckUIState)state
                                fromState:(PasswordCheckUIState)oldState
                                   update:(BOOL)update {
  if (!_didReceiveSavedForms) {
    return;
  }

  NSInteger checkSection = [self.tableViewModel
      sectionForSectionIdentifier:SectionIdentifierPasswordCheck];

  switch (state) {
    case PasswordCheckStateUnSafe:
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
    case PasswordCheckStateRunning:
    case PasswordCheckStateDisabled:
      if (oldState != PasswordCheckStateUnSafe)
        return;

      [self.tableViewModel setFooter:nil
            forSectionWithIdentifier:SectionIdentifierPasswordCheck];
      break;
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
  if (!_checkForProblemsItem)
    return;

  switch (state) {
    case PasswordCheckStateSafe:
    case PasswordCheckStateUnSafe:
    case PasswordCheckStateDefault:
    case PasswordCheckStateError:
      _checkForProblemsItem.textColor = [UIColor colorNamed:kBlueColor];
      _checkForProblemsItem.accessibilityTraits &=
          ~UIAccessibilityTraitNotEnabled;
      _checkForProblemsItem.text =
          l10n_util::GetNSString(IDS_IOS_CHECK_PASSWORDS_NOW_BUTTON);
      break;
    case PasswordCheckStateRunning:
    // Fall through.
    case PasswordCheckStateDisabled:
      _checkForProblemsItem.text =
          l10n_util::GetNSString(IDS_IOS_CHECK_PASSWORDS_NOW_BUTTON);
      _checkForProblemsItem.textColor = UIColor.cr_secondaryLabelColor;
      _checkForProblemsItem.accessibilityTraits |=
          UIAccessibilityTraitNotEnabled;
      break;
  }
}

// Updates password check status label according to provided state.
- (void)updatePasswordCheckStatusLabelWithState:(PasswordCheckUIState)state {
  if (!_passwordProblemsItem)
    return;

  _passwordProblemsItem.trailingImage = nil;
  _passwordProblemsItem.enabled = YES;
  _passwordProblemsItem.indicatorHidden = YES;
  _passwordProblemsItem.infoButtonHidden = YES;
  _passwordProblemsItem.accessoryType = UITableViewCellAccessoryNone;
  _passwordProblemsItem.detailText =
      l10n_util::GetNSString(IDS_IOS_CHECK_PASSWORDS_DESCRIPTION);

  switch (state) {
    case PasswordCheckStateRunning: {
      _passwordProblemsItem.trailingImage = nil;
      _passwordProblemsItem.indicatorHidden = NO;
      break;
    }
    case PasswordCheckStateDisabled: {
      _passwordProblemsItem.enabled = NO;
      break;
    }
    case PasswordCheckStateUnSafe: {
      _passwordProblemsItem.detailText =
          base::SysUTF16ToNSString(l10n_util::GetPluralStringFUTF16(
              IDS_IOS_CHECK_PASSWORDS_COMPROMISED_COUNT,
              self.compromisedPasswordsCount));
      UIImage* unSafeIconImage = [[UIImage imageNamed:@"settings_unsafe_state"]
          imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
      _passwordProblemsItem.trailingImage = unSafeIconImage;
      _passwordProblemsItem.trailingImageTintColor =
          [UIColor colorNamed:kRedColor];
      _passwordProblemsItem.accessoryType =
          UITableViewCellAccessoryDisclosureIndicator;
      break;
    }
    case PasswordCheckStateSafe: {
      DCHECK(!self.compromisedPasswordsCount);
      UIImage* safeIconImage = [[UIImage imageNamed:@"settings_safe_state"]
          imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
      _passwordProblemsItem.detailText =
          base::SysUTF16ToNSString(l10n_util::GetPluralStringFUTF16(
              IDS_IOS_CHECK_PASSWORDS_COMPROMISED_COUNT, 0));
      _passwordProblemsItem.trailingImage = safeIconImage;
      _passwordProblemsItem.trailingImageTintColor =
          [UIColor colorNamed:kGreenColor];
      break;
    }
    case PasswordCheckStateDefault:
      break;
    case PasswordCheckStateError: {
      _passwordProblemsItem.detailText =
          l10n_util::GetNSString(IDS_IOS_PASSWORD_CHECK_ERROR);
      _passwordProblemsItem.infoButtonHidden = NO;
      break;
    }
  }
}

- (void)updateExportPasswordsButton {
  if (!_exportPasswordsItem)
    return;
  if (!_savedForms.empty() &&
      self.passwordExporter.exportState == ExportState::IDLE) {
    _exportReady = YES;
    if (!self.editing) {
      [self setExportPasswordsButtonEnabled:YES];
    }
  } else {
    _exportReady = NO;
    [self setExportPasswordsButtonEnabled:NO];
  }
}

- (void)setExportPasswordsButtonEnabled:(BOOL)enabled {
  if (enabled) {
    DCHECK(_exportReady && !self.editing);
    _exportPasswordsItem.textColor = [UIColor colorNamed:kBlueColor];
    _exportPasswordsItem.accessibilityTraits &= ~UIAccessibilityTraitNotEnabled;
  } else {
    _exportPasswordsItem.textColor = UIColor.cr_labelColor;
    _exportPasswordsItem.accessibilityTraits |= UIAccessibilityTraitNotEnabled;
  }
  [self reconfigureCellsForItems:@[ _exportPasswordsItem ]];
}

- (void)startPasswordsExportFlow {
  UIAlertController* exportConfirmation = [UIAlertController
      alertControllerWithTitle:nil
                       message:l10n_util::GetNSString(
                                   IDS_IOS_EXPORT_PASSWORDS_ALERT_MESSAGE)
                preferredStyle:UIAlertControllerStyleActionSheet];
  exportConfirmation.view.accessibilityIdentifier =
      kPasswordsExportConfirmViewId;

  UIAlertAction* cancelAction =
      [UIAlertAction actionWithTitle:l10n_util::GetNSString(
                                         IDS_IOS_EXPORT_PASSWORDS_CANCEL_BUTTON)
                               style:UIAlertActionStyleCancel
                             handler:^(UIAlertAction* action){
                             }];
  [exportConfirmation addAction:cancelAction];

  __weak PasswordsTableViewController* weakSelf = self;
  UIAlertAction* exportAction = [UIAlertAction
      actionWithTitle:l10n_util::GetNSString(IDS_IOS_EXPORT_PASSWORDS)
                style:UIAlertActionStyleDefault
              handler:^(UIAlertAction* action) {
                PasswordsTableViewController* strongSelf = weakSelf;
                if (!strongSelf) {
                  return;
                }
                [strongSelf.passwordExporter
                    startExportFlow:CopyOf(strongSelf->_savedForms)];
              }];

  [exportConfirmation addAction:exportAction];

  // Starting with iOS13, alerts of style UIAlertControllerStyleActionSheet
  // need a sourceView or sourceRect, or this crashes.
  if (base::ios::IsRunningOnIOS13OrLater() && IsIPadIdiom()) {
    exportConfirmation.popoverPresentationController.sourceView =
        self.tableView;
  }

  [self presentViewController:exportConfirmation animated:YES completion:nil];
}

// Removes the given section if it exists and if isEmpty is true.
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
  // Ensure indexPaths are sorted to maintain delete logic, and keep track of
  // number of items deleted to adjust index for accessing elements in the
  // forms vectors.
  NSArray* sortedIndexPaths =
      [indexPaths sortedArrayUsingSelector:@selector(compare:)];
  auto passwordIterator = _savedForms.begin();
  auto passwordEndIterator = _savedForms.end();
  auto blockedIterator = _blockedForms.begin();
  auto blockedEndIterator = _blockedForms.end();
  for (NSIndexPath* indexPath in sortedIndexPaths) {
    // Only form items are editable.
    PasswordFormContentItem* item =
        base::mac::ObjCCastStrict<PasswordFormContentItem>(
            [self.tableViewModel itemAtIndexPath:indexPath]);
    BOOL blocked = [item isKindOfClass:[BlockedFormContentItem class]];
    auto& forms = blocked ? _blockedForms : _savedForms;
    auto& duplicates =
        blocked ? _blockedPasswordDuplicates : _savedPasswordDuplicates;

    const password_manager::PasswordForm& deletedForm = *item.form;
    auto begin = blocked ? blockedIterator : passwordIterator;
    auto end = blocked ? blockedEndIterator : passwordEndIterator;

    auto formIterator = std::find_if(
        begin, end,
        [&deletedForm](
            const std::unique_ptr<password_manager::PasswordForm>& value) {
          return password_manager::ArePasswordFormUniqueKeysEqual(*value,
                                                                  deletedForm);
        });
    DCHECK(formIterator != end);

    std::unique_ptr<password_manager::PasswordForm> form =
        std::move(*formIterator);
    std::string key = password_manager::CreateSortKey(*form);
    auto duplicatesRange = duplicates.equal_range(key);
    for (auto iterator = duplicatesRange.first;
         iterator != duplicatesRange.second; ++iterator) {
      _passwordStore->RemoveLogin(*(iterator->second));
    }
    duplicates.erase(key);

    formIterator = forms.erase(formIterator);
    _passwordStore->RemoveLogin(*form);

    // Keep track of where we are in the current list.
    if (blocked) {
      blockedIterator = formIterator;
    } else {
      passwordIterator = formIterator;
    }
  }

  // Remove empty sections.
  __weak PasswordsTableViewController* weakSelf = self;
  [self.tableView
      performBatchUpdates:^{
        PasswordsTableViewController* strongSelf = weakSelf;
        if (!strongSelf)
          return;

        [strongSelf removeFromModelItemAtIndexPaths:indexPaths];
        [strongSelf.tableView
            deleteRowsAtIndexPaths:indexPaths
                  withRowAnimation:UITableViewRowAnimationAutomatic];

        // Delete in reverse order of section indexes (bottom up of section
        // displayed), so that indexes in model matches those in the view.  if
        // we don't we'll cause a crash.
        if (strongSelf->_blockedForms.empty()) {
          [self clearSectionWithIdentifier:SectionIdentifierBlocked
                          withRowAnimation:UITableViewRowAnimationAutomatic];
        }
        if (strongSelf->_savedForms.empty()) {
          [strongSelf
              clearSectionWithIdentifier:SectionIdentifierSavedPasswords
                        withRowAnimation:UITableViewRowAnimationAutomatic];
        }
      }
      completion:^(BOOL finished) {
        PasswordsTableViewController* strongSelf = weakSelf;
        if (!strongSelf)
          return;
        // If both lists are empty, exit editing mode.
        if (strongSelf->_savedForms.empty() &&
            strongSelf->_blockedForms.empty())
          [strongSelf setEditing:NO animated:YES];
        [strongSelf updateUIForEditState];
        [strongSelf updateExportPasswordsButton];
      }];
}

- (void)showPasswordIssuesPage {
  if (!self.compromisedPasswordsCount ||
      self.passwordCheckState == PasswordCheckStateRunning)
    return;
  [self.handler showCompromisedPasswords];
  password_manager::LogPasswordCheckReferrer(
      password_manager::PasswordCheckReferrer::kPasswordSettings);
}

#pragma mark UITableViewDelegate

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  [super tableView:tableView didSelectRowAtIndexPath:indexPath];

  // Actions should only take effect when not in editing mode.
  if (self.editing) {
    return;
  }

  TableViewModel* model = self.tableViewModel;
  NSInteger itemType = [model itemTypeForIndexPath:indexPath];
  switch (itemType) {
    case ItemTypeLinkHeader:
    case ItemTypeHeader:
    case ItemTypeSavePasswordsSwitch:
    case ItemTypeManagedSavePasswords:
      break;
    case ItemTypePasswordCheckStatus:
      [self showPasswordIssuesPage];
      break;
    case ItemTypeSavedPassword: {
      DCHECK_EQ(SectionIdentifierSavedPasswords,
                [model sectionIdentifierForSection:indexPath.section]);
      SavedFormContentItem* saveFormItem =
          base::mac::ObjCCastStrict<SavedFormContentItem>(
              [model itemAtIndexPath:indexPath]);
      [self.handler showDetailedViewForForm:*saveFormItem.form];
      break;
    }
    case ItemTypeBlocked: {
      DCHECK_EQ(SectionIdentifierBlocked,
                [model sectionIdentifierForSection:indexPath.section]);
      BlockedFormContentItem* blockedItem =
          base::mac::ObjCCastStrict<BlockedFormContentItem>(
              [model itemAtIndexPath:indexPath]);
      [self.handler showDetailedViewForForm:*blockedItem.form];
      break;
    }
    case ItemTypeExportPasswordsButton:
      DCHECK_EQ(SectionIdentifierExportPasswordsButton,
                [model sectionIdentifierForSection:indexPath.section]);
      if (_exportReady) {
        [self startPasswordsExportFlow];
      }
      break;
    case ItemTypeCheckForProblemsButton:
      if (self.passwordCheckState != PasswordCheckStateRunning) {
        [self.delegate startPasswordCheck];
        UmaHistogramEnumeration("PasswordManager.BulkCheck.UserAction",
                                PasswordCheckInteraction::kManualPasswordCheck);
      }
      break;
    default:
      NOTREACHED();
  }
  [tableView deselectRowAtIndexPath:indexPath animated:YES];
}

- (BOOL)tableView:(UITableView*)tableView
    shouldHighlightRowAtIndexPath:(NSIndexPath*)indexPath {
  NSInteger itemType = [self.tableViewModel itemTypeForIndexPath:indexPath];
  switch (itemType) {
    case ItemTypeSavePasswordsSwitch:
      return NO;
    case ItemTypePasswordCheckStatus:
      return self.passwordCheckState == PasswordCheckStateUnSafe;
    case ItemTypeCheckForProblemsButton:
      return self.passwordCheckState != PasswordCheckStateRunning &&
             self.passwordCheckState != PasswordCheckStateDisabled;
  }
  return YES;
}

- (UIView*)tableView:(UITableView*)tableView
    viewForHeaderInSection:(NSInteger)section {
  UIView* view = [super tableView:tableView viewForHeaderInSection:section];
  switch ([self.tableViewModel sectionIdentifierForSection:section]) {
    case SectionIdentifierSavePasswordsSwitch: {
      TableViewLinkHeaderFooterView* linkView =
          base::mac::ObjCCastStrict<TableViewLinkHeaderFooterView>(view);
      linkView.delegate = self;
      break;
    }
    default:
      break;
  }
  return view;
}

#pragma mark - UITableViewDataSource

- (BOOL)tableView:(UITableView*)tableView
    canEditRowAtIndexPath:(NSIndexPath*)indexPath {
  // Only password cells are editable.
  TableViewItem* item = [self.tableViewModel itemAtIndexPath:indexPath];
  return [item isKindOfClass:[SavedFormContentItem class]] ||
         [item isKindOfClass:[BlockedFormContentItem class]];
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
    case ItemTypeSavePasswordsSwitch: {
      SettingsSwitchCell* switchCell =
          base::mac::ObjCCastStrict<SettingsSwitchCell>(cell);
      [switchCell.switchView addTarget:self
                                action:@selector(savePasswordsSwitchChanged:)
                      forControlEvents:UIControlEventValueChanged];
      break;
    }
    case ItemTypeManagedSavePasswords: {
      TableViewInfoButtonCell* managedCell =
          base::mac::ObjCCastStrict<TableViewInfoButtonCell>(cell);
      [managedCell.trailingButton
                 addTarget:self
                    action:@selector(didTapManagedUIInfoButton:)
          forControlEvents:UIControlEventTouchUpInside];
      break;
    }
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
      TableViewDetailTextCell* textCell =
          base::mac::ObjCCastStrict<TableViewDetailTextCell>(cell);
      textCell.textLabel.lineBreakMode = NSLineBreakByTruncatingHead;
      break;
    }
  }
  return cell;
}

#pragma mark PasswordExporterDelegate

- (void)showSetPasscodeDialog {
  UIAlertController* alertController = [UIAlertController
      alertControllerWithTitle:l10n_util::GetNSString(
                                   IDS_IOS_SETTINGS_SET_UP_SCREENLOCK_TITLE)
                       message:
                           l10n_util::GetNSString(
                               IDS_IOS_SETTINGS_EXPORT_PASSWORDS_SET_UP_SCREENLOCK_CONTENT)
                preferredStyle:UIAlertControllerStyleAlert];

  ProceduralBlockWithURL blockOpenURL = BlockToOpenURL(self, self.dispatcher);
  UIAlertAction* learnAction = [UIAlertAction
      actionWithTitle:l10n_util::GetNSString(
                          IDS_IOS_SETTINGS_SET_UP_SCREENLOCK_LEARN_HOW)
                style:UIAlertActionStyleDefault
              handler:^(UIAlertAction*) {
                blockOpenURL(GURL(kPasscodeArticleURL));
              }];
  [alertController addAction:learnAction];
  UIAlertAction* okAction =
      [UIAlertAction actionWithTitle:l10n_util::GetNSString(IDS_OK)
                               style:UIAlertActionStyleDefault
                             handler:nil];
  [alertController addAction:okAction];
  alertController.preferredAction = okAction;
  [self presentViewController:alertController animated:YES completion:nil];
}

- (void)showPreparingPasswordsAlert {
  _preparingPasswordsAlert = [UIAlertController
      alertControllerWithTitle:
          l10n_util::GetNSString(IDS_IOS_EXPORT_PASSWORDS_PREPARING_ALERT_TITLE)
                       message:nil
                preferredStyle:UIAlertControllerStyleAlert];
  __weak PasswordsTableViewController* weakSelf = self;
  UIAlertAction* cancelAction =
      [UIAlertAction actionWithTitle:l10n_util::GetNSString(
                                         IDS_IOS_EXPORT_PASSWORDS_CANCEL_BUTTON)
                               style:UIAlertActionStyleCancel
                             handler:^(UIAlertAction*) {
                               [weakSelf.passwordExporter cancelExport];
                             }];
  [_preparingPasswordsAlert addAction:cancelAction];
  [self presentViewController:_preparingPasswordsAlert
                     animated:YES
                   completion:nil];
}

- (void)showExportErrorAlertWithLocalizedReason:(NSString*)localizedReason {
  UIAlertController* alertController = [UIAlertController
      alertControllerWithTitle:l10n_util::GetNSString(
                                   IDS_IOS_EXPORT_PASSWORDS_FAILED_ALERT_TITLE)
                       message:localizedReason
                preferredStyle:UIAlertControllerStyleAlert];
  UIAlertAction* okAction =
      [UIAlertAction actionWithTitle:l10n_util::GetNSString(IDS_OK)
                               style:UIAlertActionStyleDefault
                             handler:nil];
  [alertController addAction:okAction];
  [self presentViewController:alertController];
}

- (void)showActivityViewWithActivityItems:(NSArray*)activityItems
                        completionHandler:(void (^)(NSString* activityType,
                                                    BOOL completed,
                                                    NSArray* returnedItems,
                                                    NSError* activityError))
                                              completionHandler {
  PasswordExportActivityViewController* activityViewController =
      [[PasswordExportActivityViewController alloc]
          initWithActivityItems:activityItems
                       delegate:self];
  NSArray* excludedActivityTypes = @[
    UIActivityTypeAddToReadingList, UIActivityTypeAirDrop,
    UIActivityTypeCopyToPasteboard, UIActivityTypeOpenInIBooks,
    UIActivityTypePostToFacebook, UIActivityTypePostToFlickr,
    UIActivityTypePostToTencentWeibo, UIActivityTypePostToTwitter,
    UIActivityTypePostToVimeo, UIActivityTypePostToWeibo, UIActivityTypePrint
  ];
  [activityViewController setExcludedActivityTypes:excludedActivityTypes];

  [activityViewController setCompletionWithItemsHandler:completionHandler];

  UIView* sourceView = nil;
  CGRect sourceRect = CGRectZero;
  if (IsIPadIdiom() && !IsCompactWidth(self.view.window)) {
    NSIndexPath* indexPath = [self.tableViewModel
        indexPathForItemType:ItemTypeExportPasswordsButton
           sectionIdentifier:SectionIdentifierExportPasswordsButton];
    UITableViewCell* cell = [self.tableView cellForRowAtIndexPath:indexPath];
    sourceView = self.tableView;
    sourceRect = cell.frame;
  }
  activityViewController.modalPresentationStyle = UIModalPresentationPopover;
  activityViewController.popoverPresentationController.sourceView = sourceView;
  activityViewController.popoverPresentationController.sourceRect = sourceRect;
  activityViewController.popoverPresentationController
      .permittedArrowDirections =
      UIPopoverArrowDirectionDown | UIPopoverArrowDirectionDown;

  [self presentViewController:activityViewController];
}

#pragma mark - PasswordExportActivityViewControllerDelegate

- (void)resetExport {
  [self.passwordExporter resetExportState];
}

#pragma mark Helper methods

- (void)presentViewController:(UIViewController*)viewController {
  if (self.presentedViewController == _preparingPasswordsAlert &&
      !_preparingPasswordsAlert.beingDismissed) {
    __weak PasswordsTableViewController* weakSelf = self;
    [self dismissViewControllerAnimated:YES
                             completion:^{
                               [weakSelf presentViewController:viewController
                                                      animated:YES
                                                    completion:nil];
                             }];
  } else {
    [self presentViewController:viewController animated:YES completion:nil];
  }
}

// Sets the save passwords switch item's enabled status to |enabled| and
// reconfigures the corresponding cell.
- (void)setSavePasswordsSwitchItemEnabled:(BOOL)enabled {
  if (_savePasswordsItem) {
    [_savePasswordsItem setEnabled:enabled];
    [self reconfigureCellsForItems:@[ _savePasswordsItem ]];
  }
}

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

// Deletes passed password form and updates list accordingly.
- (void)deletePasswordForm:(const password_manager::PasswordForm&)form {
  _passwordStore->RemoveLogin(form);

  std::vector<std::unique_ptr<password_manager::PasswordForm>>& forms =
      form.blocked_by_user ? _blockedForms : _savedForms;
  auto iterator = std::find_if(
      forms.begin(), forms.end(),
      [&form](const std::unique_ptr<password_manager::PasswordForm>& value) {
        return password_manager::ArePasswordFormUniqueKeysEqual(*value, form);
      });
  // If |form| not found, pop password details view controller.
  if (iterator == forms.end()) {
    [self.navigationController popViewControllerAnimated:YES];
    return;
  }

  forms.erase(iterator);

  password_manager::DuplicatesMap& duplicates = form.blocked_by_user
                                                    ? _blockedPasswordDuplicates
                                                    : _savedPasswordDuplicates;
  std::string key = password_manager::CreateSortKey(form);
  auto duplicatesRange = duplicates.equal_range(key);
  for (auto iterator = duplicatesRange.first;
       iterator != duplicatesRange.second; ++iterator) {
    _passwordStore->RemoveLogin(*(iterator->second));
  }
  duplicates.erase(key);

  [self updateUIForEditState];
  [self reloadData];
  // TODO(crbug.com/1096986): Delete this once
  // LegacyPasswordDetailsTableViewController is removed.
  [self.navigationController popViewControllerAnimated:YES];
}

#pragma mark - Testing

#pragma mark - ChromeIdentityServiceObserver

- (void)identityListChanged {
  [self reloadData];
}

- (void)chromeIdentityServiceWillBeDestroyed {
  _identityServiceObserver.reset();
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  base::RecordAction(
      base::UserMetricsAction("IOSPasswordsSettingsCloseWithSwipe"));
}

@end
