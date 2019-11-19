// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/passwords_table_view_controller.h"

#import <UIKit/UIKit.h>

#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/common/password_form.h"
#include "components/google/core/common/google_util.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/password_manager/core/browser/password_list_sorter.h"
#include "components/password_manager/core/browser/password_manager_constants.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/password_manager/core/browser/password_ui_utils.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_member.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/url_formatter.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/passwords/ios_chrome_password_store_factory.h"
#import "ios/chrome/browser/passwords/save_passwords_consumer.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#include "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/signin/chrome_identity_service_observer_bridge.h"
#include "ios/chrome/browser/system_flags.h"
#import "ios/chrome/browser/ui/settings/cells/settings_cells_constants.h"
#import "ios/chrome/browser/ui/settings/cells/settings_switch_cell.h"
#import "ios/chrome/browser/ui/settings/cells/settings_switch_item.h"
#import "ios/chrome/browser/ui/settings/password/password_details_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/password/password_details_table_view_controller_delegate.h"
#import "ios/chrome/browser/ui/settings/password/password_exporter.h"
#import "ios/chrome/browser/ui/settings/password/reauthentication_module.h"
#import "ios/chrome/browser/ui/settings/utils/pref_backed_boolean.h"
#import "ios/chrome/browser/ui/settings/utils/settings_utils.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_cells_constants.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_detail_text_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_link_header_footer_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_text_header_footer_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_text_item.h"
#import "ios/chrome/browser/ui/table_view/table_view_navigation_controller_constants.h"
#include "ios/chrome/browser/ui/ui_feature_flags.h"
#include "ios/chrome/browser/ui/util/ui_util.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/colors/UIColor+cr_semantic_colors.h"
#import "ios/chrome/common/colors/semantic_color_names.h"
#import "ios/chrome/common/ui_util/constraints_ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

NSString* const kPasswordsTableViewId = @"PasswordsTableViewId";
NSString* const kPasswordsExportConfirmViewId = @"PasswordsExportConfirmViewId";
NSString* const kPasswordsSearchBarId = @"PasswordsSearchBar";
NSString* const kPasswordsScrimViewId = @"PasswordsScrimViewId";

namespace {

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierSavePasswordsSwitch = kSectionIdentifierEnumZero,
  SectionIdentifierSavedPasswords,
  SectionIdentifierBlacklist,
  SectionIdentifierExportPasswordsButton,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeLinkHeader = kItemTypeEnumZero,
  ItemTypeHeader,
  ItemTypeSavePasswordsSwitch,
  ItemTypePasswordLeakCheckSwitch,
  ItemTypeSavedPassword,  // This is a repeated item type.
  ItemTypeBlacklisted,    // This is a repeated item type.
  ItemTypeExportPasswordsButton,
};

std::vector<std::unique_ptr<autofill::PasswordForm>> CopyOf(
    const std::vector<std::unique_ptr<autofill::PasswordForm>>& password_list) {
  std::vector<std::unique_ptr<autofill::PasswordForm>> password_list_copy;
  for (const auto& form : password_list) {
    password_list_copy.push_back(
        std::make_unique<autofill::PasswordForm>(*form));
  }
  return password_list_copy;
}

}  // namespace

@interface PasswordFormContentItem : TableViewDetailTextItem
@property(nonatomic) autofill::PasswordForm* form;
@end
@implementation PasswordFormContentItem
@end

// Use the type of the items to convey the Saved/Blacklisted status.
@interface SavedFormContentItem : PasswordFormContentItem
@end
@implementation SavedFormContentItem
@end
@interface BlacklistedFormContentItem : PasswordFormContentItem
@end
@implementation BlacklistedFormContentItem
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
    PasswordDetailsTableViewControllerDelegate,
    PasswordExporterDelegate,
    PasswordExportActivityViewControllerDelegate,
    SavePasswordsConsumerDelegate,
    UISearchControllerDelegate,
    UISearchBarDelegate,
    SuccessfulReauthTimeAccessor> {
  // The observable boolean that binds to the password manager setting state.
  // Saved passwords are only on if the password manager is enabled.
  PrefBackedBoolean* passwordManagerEnabled_;
  // The observable boolean that binds to the password leak check settings
  // state.
  PrefBackedBoolean* passwordLeakCheckEnabled_;
  // The header for save passwords switch section.
  TableViewLinkHeaderFooterItem* manageAccountLinkItem_;
  // The item related to the switch for the password manager setting.
  SettingsSwitchItem* savePasswordsItem_;
  // The item related to the switch for the automatic password leak detection
  // setting.
  SettingsSwitchItem* leakCheckItem_;
  // The item related to the button for exporting passwords.
  TableViewTextItem* exportPasswordsItem_;
  // The interface for getting and manipulating a user's saved passwords.
  scoped_refptr<password_manager::PasswordStore> passwordStore_;
  // A helper object for passing data about saved passwords from a finished
  // password store request to the PasswordsTableViewController.
  std::unique_ptr<ios::SavePasswordsConsumer> savedPasswordsConsumer_;
  // The list of the user's saved passwords.
  std::vector<std::unique_ptr<autofill::PasswordForm>> savedForms_;
  // The list of the user's blacklisted sites.
  std::vector<std::unique_ptr<autofill::PasswordForm>> blacklistedForms_;
  // Map containing duplicates of saved passwords.
  password_manager::DuplicatesMap savedPasswordDuplicates_;
  // Map containing duplicates of blacklisted passwords.
  password_manager::DuplicatesMap blacklistedPasswordDuplicates_;
  // The current Chrome browser state.
  ios::ChromeBrowserState* browserState_;
  // Authentication Service Observer.
  std::unique_ptr<ChromeIdentityServiceObserverBridge> identityServiceObserver_;
  // Object storing the time of the previous successful re-authentication.
  // This is meant to be used by the |ReauthenticationModule| for keeping
  // re-authentications valid for a certain time interval within the scope
  // of the Save Passwords Settings.
  NSDate* successfulReauthTime_;
  // Module containing the reauthentication mechanism for viewing and copying
  // passwords.
  ReauthenticationModule* reauthenticationModule_;
  // Boolean containing whether the export operation is ready. This implies that
  // the exporter is idle and there is at least one saved passwords to export.
  BOOL exportReady_;
  // Alert informing the user that passwords are being prepared for
  // export.
  UIAlertController* preparingPasswordsAlert_;
}

// Kick off async request to get logins from password store.
- (void)getLoginsFromPasswordStore;

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

@end

@implementation PasswordsTableViewController

#pragma mark - Initialization

- (instancetype)initWithBrowserState:(ios::ChromeBrowserState*)browserState {
  DCHECK(browserState);
  UITableViewStyle style = base::FeatureList::IsEnabled(kSettingsRefresh)
                               ? UITableViewStylePlain
                               : UITableViewStyleGrouped;
  self = [super initWithTableViewStyle:style
                           appBarStyle:ChromeTableViewControllerStyleNoAppBar];
  if (self) {
    browserState_ = browserState;
    reauthenticationModule_ = [[ReauthenticationModule alloc]
        initWithSuccessfulReauthTimeAccessor:self];
    _passwordExporter = [[PasswordExporter alloc]
        initWithReauthenticationModule:reauthenticationModule_
                              delegate:self];
    self.exampleHeaders = [[NSMutableDictionary alloc] init];
    self.title = l10n_util::GetNSString(IDS_IOS_PASSWORDS);
    self.shouldHideDoneButton = YES;
    self.searchTerm = @"";
    passwordStore_ = IOSChromePasswordStoreFactory::GetForBrowserState(
        browserState_, ServiceAccessType::EXPLICIT_ACCESS);
    DCHECK(passwordStore_);
    passwordManagerEnabled_ = [[PrefBackedBoolean alloc]
        initWithPrefService:browserState_->GetPrefs()
                   prefName:password_manager::prefs::kCredentialsEnableService];
    [passwordManagerEnabled_ setObserver:self];
    if (base::FeatureList::IsEnabled(
            password_manager::features::kLeakDetection)) {
      passwordLeakCheckEnabled_ = [[PrefBackedBoolean alloc]
          initWithPrefService:browserState_->GetPrefs()
                     prefName:password_manager::prefs::
                                  kPasswordLeakDetectionEnabled];
      [passwordLeakCheckEnabled_ setObserver:self];
      identityServiceObserver_.reset(
          new ChromeIdentityServiceObserverBridge(self));
    }
    [self getLoginsFromPasswordStore];
    [self updateUIForEditState];
    [self updateExportPasswordsButton];
  }
  return self;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.tableView.allowsMultipleSelectionDuringEditing = YES;
  self.tableView.accessibilityIdentifier = kPasswordsTableViewId;

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

- (void)setEditing:(BOOL)editing animated:(BOOL)animated {
  [super setEditing:editing animated:animated];
  if (editing) {
    [self setSavePasswordsSwitchItemEnabled:NO];
    [self setLeakCheckSwitchItemEnabled:NO];
    [self setExportPasswordsButtonEnabled:NO];
    [self setSearchBarEnabled:NO];
  } else {
    [self setSavePasswordsSwitchItemEnabled:YES];
    [self setLeakCheckSwitchItemEnabled:YES];
    if (exportReady_) {
      [self setExportPasswordsButtonEnabled:YES];
    }
    [self setSearchBarEnabled:YES];
  }
}

#pragma mark - SettingsRootTableViewController

- (void)loadModel {
  [super loadModel];
  TableViewModel* model = self.tableViewModel;

  // Save passwords switch and manage account message. Only show this section
  // when the searchController is not active.
  if (!self.navigationItem.searchController.active) {
    [model addSectionWithIdentifier:SectionIdentifierSavePasswordsSwitch];
    savePasswordsItem_ = [self savePasswordsItem];
    [model addItem:savePasswordsItem_
        toSectionWithIdentifier:SectionIdentifierSavePasswordsSwitch];
    if (base::FeatureList::IsEnabled(
            password_manager::features::kLeakDetection)) {
      leakCheckItem_ = [self leakCheckItem];
      [self updateDetailTextLeakCheckItem];
      [model addItem:leakCheckItem_
          toSectionWithIdentifier:SectionIdentifierSavePasswordsSwitch];
    }
    manageAccountLinkItem_ = [self manageAccountLinkItem];
    [model setHeader:manageAccountLinkItem_
        forSectionWithIdentifier:SectionIdentifierSavePasswordsSwitch];
  }

  // Saved passwords.
  if (!savedForms_.empty()) {
    [model addSectionWithIdentifier:SectionIdentifierSavedPasswords];
    TableViewTextHeaderFooterItem* headerItem =
        [[TableViewTextHeaderFooterItem alloc] initWithType:ItemTypeHeader];
    headerItem.text =
        l10n_util::GetNSString(IDS_IOS_SETTINGS_PASSWORDS_SAVED_HEADING);
    [model setHeader:headerItem
        forSectionWithIdentifier:SectionIdentifierSavedPasswords];
  }

  // Blacklisted passwords.
  if (!blacklistedForms_.empty()) {
    [model addSectionWithIdentifier:SectionIdentifierBlacklist];
    TableViewTextHeaderFooterItem* headerItem =
        [[TableViewTextHeaderFooterItem alloc] initWithType:ItemTypeHeader];
    headerItem.text =
        l10n_util::GetNSString(IDS_IOS_SETTINGS_PASSWORDS_EXCEPTIONS_HEADING);
    [model setHeader:headerItem
        forSectionWithIdentifier:SectionIdentifierBlacklist];
  }

  // Export passwords button.
  [model addSectionWithIdentifier:SectionIdentifierExportPasswordsButton];
  exportPasswordsItem_ = [self exportPasswordsItem];
  [model addItem:exportPasswordsItem_
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
  return !savedForms_.empty() || !blacklistedForms_.empty();
}

#pragma mark - SettingsControllerProtocol

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
  savePasswordsItem.on = [passwordManagerEnabled_ value];
  savePasswordsItem.accessibilityIdentifier = @"savePasswordsItem_switch";
  return savePasswordsItem;
}

- (SettingsSwitchItem*)leakCheckItem {
  SettingsSwitchItem* leakCheckItem =
      [[SettingsSwitchItem alloc] initWithType:ItemTypePasswordLeakCheckSwitch];
  leakCheckItem.text = l10n_util::GetNSString(IDS_IOS_LEAK_CHECK_SWITCH);
  leakCheckItem.on = [self leakCheckItemOnState];
  leakCheckItem.accessibilityIdentifier = @"leakCheckItem_switch";

  AuthenticationService* authService =
      AuthenticationServiceFactory::GetForBrowserState(browserState_);
  leakCheckItem.enabled = authService->IsAuthenticated();

  return leakCheckItem;
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
                                       forForm:(autofill::PasswordForm*)form {
  SavedFormContentItem* passwordItem =
      [[SavedFormContentItem alloc] initWithType:ItemTypeSavedPassword];
  passwordItem.text = text;
  passwordItem.form = form;
  passwordItem.detailText = detailText;
  passwordItem.accessibilityTraits |= UIAccessibilityTraitButton;
  passwordItem.accessoryType = UITableViewCellAccessoryDisclosureIndicator;
  return passwordItem;
}

- (BlacklistedFormContentItem*)
    blacklistedFormItemWithText:(NSString*)text
                        forForm:(autofill::PasswordForm*)form {
  BlacklistedFormContentItem* passwordItem =
      [[BlacklistedFormContentItem alloc] initWithType:ItemTypeBlacklisted];
  passwordItem.text = text;
  passwordItem.form = form;
  passwordItem.accessibilityTraits |= UIAccessibilityTraitButton;
  passwordItem.accessoryType = UITableViewCellAccessoryDisclosureIndicator;
  return passwordItem;
}

#pragma mark - BooleanObserver

- (void)booleanDidChange:(id<ObservableBoolean>)observableBoolean {
  if (observableBoolean == passwordManagerEnabled_) {
    // Update the item.
    savePasswordsItem_.on = [passwordManagerEnabled_ value];

    // Update the cell if it's not removed by presenting search controller.
    if ([self.tableViewModel
            hasItemForItemType:ItemTypeSavePasswordsSwitch
             sectionIdentifier:SectionIdentifierSavePasswordsSwitch]) {
      [self reconfigureCellsForItems:@[ savePasswordsItem_ ]];
    }
  } else if (observableBoolean == passwordLeakCheckEnabled_) {
    DCHECK(base::FeatureList::IsEnabled(
        password_manager::features::kLeakDetection));
    // Update the item.
    leakCheckItem_.on = [self leakCheckItemOnState];

    // Update the cell if it's not removed by presenting search controller.
    if ([self.tableViewModel
            hasItemForItemType:ItemTypePasswordLeakCheckSwitch
             sectionIdentifier:SectionIdentifierSavePasswordsSwitch]) {
      [self updateDetailTextLeakCheckItem];
      [self reconfigureCellsForItems:@[ leakCheckItem_ ]];
    }
  } else {
    NOTREACHED();
  }
}

#pragma mark - Actions

- (void)savePasswordsSwitchChanged:(UISwitch*)switchView {
  // Update the setting.
  [passwordManagerEnabled_ setValue:switchView.on];

  // Update the item.
  savePasswordsItem_.on = [passwordManagerEnabled_ value];
}

- (void)passwordLeakCheckSwitchChanged:(UISwitch*)switchView {
  // Update the setting.
  [passwordLeakCheckEnabled_ setValue:switchView.on];

  // Update the item.
  leakCheckItem_.on = [self leakCheckItemOnState];
  [self updateDetailTextLeakCheckItem];
  [self reconfigureCellsForItems:@[ leakCheckItem_ ]];
}

#pragma mark - SavePasswordsConsumerDelegate

- (void)onGetPasswordStoreResults:
    (std::vector<std::unique_ptr<autofill::PasswordForm>>)results {
  if (results.empty()) {
    return;
  }
  for (auto& form : results) {
    if (form->blacklisted_by_user)
      blacklistedForms_.push_back(std::move(form));
    else
      savedForms_.push_back(std::move(form));
  }

  password_manager::SortEntriesAndHideDuplicates(&savedForms_,
                                                 &savedPasswordDuplicates_);
  password_manager::SortEntriesAndHideDuplicates(
      &blacklistedForms_, &blacklistedPasswordDuplicates_);

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
  // Remove save passwords switch section.
  [self
      performBatchTableViewUpdates:^{
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
        [model setHeader:manageAccountLinkItem_
            forSectionWithIdentifier:SectionIdentifierSavePasswordsSwitch];
        [self.tableView insertSections:[NSIndexSet indexSetWithIndex:0]
                      withRowAnimation:UITableViewRowAnimationTop];
        [model addItem:savePasswordsItem_
            toSectionWithIdentifier:SectionIdentifierSavePasswordsSwitch];
        if (base::FeatureList::IsEnabled(
                password_manager::features::kLeakDetection)) {
          [model addItem:leakCheckItem_
              toSectionWithIdentifier:SectionIdentifierSavePasswordsSwitch];
        }
        [self.tableView
            insertRowsAtIndexPaths:@[ [NSIndexPath indexPathForRow:0
                                                         inSection:0] ]
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
  if ([model hasSectionForSectionIdentifier:SectionIdentifierBlacklist]) {
    NSInteger blacklistedSection =
        [model sectionForSectionIdentifier:SectionIdentifierBlacklist];
    [indexSet addIndex:blacklistedSection];
  }
  if (indexSet.count > 0) {
    BOOL animationsWereEnabled = [UIView areAnimationsEnabled];
    [UIView setAnimationsEnabled:NO];
    [self.tableView reloadSections:indexSet
                  withRowAnimation:UITableViewRowAnimationAutomatic];
    [UIView setAnimationsEnabled:animationsWereEnabled];
  }
}

// Builds the filtered list of passwords/blacklisted based on given
// |searchTerm|.
- (void)filterItems:(NSString*)searchTerm {
  TableViewModel* model = self.tableViewModel;

  if (!savedForms_.empty()) {
    [model deleteAllItemsFromSectionWithIdentifier:
               SectionIdentifierSavedPasswords];
    for (const auto& form : savedForms_) {
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

  if (!blacklistedForms_.empty()) {
    [model deleteAllItemsFromSectionWithIdentifier:SectionIdentifierBlacklist];
    for (const auto& form : blacklistedForms_) {
      NSString* text = base::SysUTF8ToNSString(
          password_manager::GetShownOriginAndLinkUrl(*form).first);
      bool hidden = searchTerm.length > 0 &&
                    ![text localizedCaseInsensitiveContainsString:searchTerm];
      if (hidden)
        continue;
      [model addItem:[self blacklistedFormItemWithText:text forForm:form.get()]
          toSectionWithIdentifier:SectionIdentifierBlacklist];
    }
  }
}

// Starts requests for saved and blacklisted passwords to the store.
- (void)getLoginsFromPasswordStore {
  savedPasswordsConsumer_.reset(new ios::SavePasswordsConsumer(self));
  passwordStore_->GetAllLogins(savedPasswordsConsumer_.get());
}

- (void)updateExportPasswordsButton {
  if (!exportPasswordsItem_)
    return;
  if (!savedForms_.empty() &&
      self.passwordExporter.exportState == ExportState::IDLE) {
    exportReady_ = YES;
    if (!self.editing) {
      [self setExportPasswordsButtonEnabled:YES];
    }
  } else {
    exportReady_ = NO;
    [self setExportPasswordsButtonEnabled:NO];
  }
}

- (void)setExportPasswordsButtonEnabled:(BOOL)enabled {
  if (enabled) {
    DCHECK(exportReady_ && !self.editing);
    exportPasswordsItem_.textColor = [UIColor colorNamed:kBlueColor];
    exportPasswordsItem_.accessibilityTraits &= ~UIAccessibilityTraitNotEnabled;
  } else {
    exportPasswordsItem_.textColor = UIColor.cr_labelColor;
    exportPasswordsItem_.accessibilityTraits |= UIAccessibilityTraitNotEnabled;
  }
  [self reconfigureCellsForItems:@[ exportPasswordsItem_ ]];
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
                    startExportFlow:CopyOf(strongSelf->savedForms_)];
              }];

  [exportConfirmation addAction:exportAction];

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

- (void)openDetailedViewForForm:(const autofill::PasswordForm&)form {
  PasswordDetailsTableViewController* controller =
      [[PasswordDetailsTableViewController alloc]
            initWithPasswordForm:form
                        delegate:self
          reauthenticationModule:reauthenticationModule_];
  controller.dispatcher = self.dispatcher;
  [self.navigationController pushViewController:controller animated:YES];
}

- (void)deleteItemAtIndexPaths:(NSArray<NSIndexPath*>*)indexPaths {
  // Ensure indexPaths are sorted to maintain delete logic, and keep track of
  // number of items deleted to adjust index for accessing elements in the
  // forms vectors.
  NSArray* sortedIndexPaths =
      [indexPaths sortedArrayUsingSelector:@selector(compare:)];
  auto passwordIterator = savedForms_.begin();
  auto passwordEndIterator = savedForms_.end();
  auto blacklistedIterator = blacklistedForms_.begin();
  auto blacklistedEndIterator = blacklistedForms_.end();
  for (NSIndexPath* indexPath in sortedIndexPaths) {
    // Only form items are editable.
    PasswordFormContentItem* item =
        base::mac::ObjCCastStrict<PasswordFormContentItem>(
            [self.tableViewModel itemAtIndexPath:indexPath]);
    BOOL blacklisted = [item isKindOfClass:[BlacklistedFormContentItem class]];
    auto& forms = blacklisted ? blacklistedForms_ : savedForms_;
    auto& duplicates =
        blacklisted ? blacklistedPasswordDuplicates_ : savedPasswordDuplicates_;

    const autofill::PasswordForm& deletedForm = *item.form;
    auto begin = blacklisted ? blacklistedIterator : passwordIterator;
    auto end = blacklisted ? blacklistedEndIterator : passwordEndIterator;

    auto formIterator = std::find_if(
        begin, end,
        [&deletedForm](const std::unique_ptr<autofill::PasswordForm>& value) {
          return *value == deletedForm;
        });
    DCHECK(formIterator != end);

    std::unique_ptr<autofill::PasswordForm> form = std::move(*formIterator);
    std::string key = password_manager::CreateSortKey(*form);
    auto duplicatesRange = duplicates.equal_range(key);
    for (auto iterator = duplicatesRange.first;
         iterator != duplicatesRange.second; ++iterator) {
      passwordStore_->RemoveLogin(*(iterator->second));
    }
    duplicates.erase(key);

    formIterator = forms.erase(formIterator);
    passwordStore_->RemoveLogin(*form);

    // Keep track of where we are in the current list.
    if (blacklisted) {
      blacklistedIterator = formIterator;
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
        if (strongSelf->blacklistedForms_.empty()) {
          [self clearSectionWithIdentifier:SectionIdentifierBlacklist
                          withRowAnimation:UITableViewRowAnimationAutomatic];
        }
        if (strongSelf->savedForms_.empty()) {
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
        if (strongSelf->savedForms_.empty() &&
            strongSelf->blacklistedForms_.empty())
          [strongSelf setEditing:NO animated:YES];
        [strongSelf updateUIForEditState];
        [strongSelf updateExportPasswordsButton];
      }];
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
    case ItemTypePasswordLeakCheckSwitch:
      break;
    case ItemTypeSavedPassword: {
      DCHECK_EQ(SectionIdentifierSavedPasswords,
                [model sectionIdentifierForSection:indexPath.section]);
      SavedFormContentItem* saveFormItem =
          base::mac::ObjCCastStrict<SavedFormContentItem>(
              [model itemAtIndexPath:indexPath]);
      [self openDetailedViewForForm:*saveFormItem.form];
      break;
    }
    case ItemTypeBlacklisted: {
      DCHECK_EQ(SectionIdentifierBlacklist,
                [model sectionIdentifierForSection:indexPath.section]);
      BlacklistedFormContentItem* blacklistedItem =
          base::mac::ObjCCastStrict<BlacklistedFormContentItem>(
              [model itemAtIndexPath:indexPath]);
      [self openDetailedViewForForm:*blacklistedItem.form];
      break;
    }
    case ItemTypeExportPasswordsButton:
      DCHECK_EQ(SectionIdentifierExportPasswordsButton,
                [model sectionIdentifierForSection:indexPath.section]);
      if (exportReady_) {
        [self startPasswordsExportFlow];
      }
      break;
    default:
      NOTREACHED();
  }
  [tableView deselectRowAtIndexPath:indexPath animated:YES];
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
         [item isKindOfClass:[BlacklistedFormContentItem class]];
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
    case ItemTypePasswordLeakCheckSwitch: {
      DCHECK(base::FeatureList::IsEnabled(
          password_manager::features::kLeakDetection));
      SettingsSwitchCell* switchCell =
          base::mac::ObjCCastStrict<SettingsSwitchCell>(cell);
      [switchCell.switchView
                 addTarget:self
                    action:@selector(passwordLeakCheckSwitchChanged:)
          forControlEvents:UIControlEventValueChanged];
      break;
    }
  }
  return cell;
}

#pragma mark PasswordDetailsTableViewControllerDelegate

- (void)passwordDetailsTableViewController:
            (PasswordDetailsTableViewController*)controller
                            deletePassword:(const autofill::PasswordForm&)form {
  passwordStore_->RemoveLogin(form);

  std::vector<std::unique_ptr<autofill::PasswordForm>>& forms =
      form.blacklisted_by_user ? blacklistedForms_ : savedForms_;
  auto iterator = std::find_if(
      forms.begin(), forms.end(),
      [&form](const std::unique_ptr<autofill::PasswordForm>& value) {
        return *value == form;
      });
  DCHECK(iterator != forms.end());
  forms.erase(iterator);

  password_manager::DuplicatesMap& duplicates =
      form.blacklisted_by_user ? blacklistedPasswordDuplicates_
                               : savedPasswordDuplicates_;
  std::string key = password_manager::CreateSortKey(form);
  auto duplicatesRange = duplicates.equal_range(key);
  for (auto iterator = duplicatesRange.first;
       iterator != duplicatesRange.second; ++iterator) {
    passwordStore_->RemoveLogin(*(iterator->second));
  }
  duplicates.erase(key);

  [self updateUIForEditState];
  [self reloadData];
  [self.navigationController popViewControllerAnimated:YES];
}

#pragma mark SuccessfulReauthTimeAccessor

- (void)updateSuccessfulReauthTime {
  successfulReauthTime_ = [[NSDate alloc] init];
}

- (NSDate*)lastSuccessfulReauthTime {
  return successfulReauthTime_;
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
  preparingPasswordsAlert_ = [UIAlertController
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
  [preparingPasswordsAlert_ addAction:cancelAction];
  [self presentViewController:preparingPasswordsAlert_
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
  if (IsIPadIdiom() && !IsCompactWidth()) {
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
  if (self.presentedViewController == preparingPasswordsAlert_ &&
      !preparingPasswordsAlert_.beingDismissed) {
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
  [savePasswordsItem_ setEnabled:enabled];
  [self reconfigureCellsForItems:@[ savePasswordsItem_ ]];
}

// Returns a boolean indicating if the switch should appear as "On" or "Off"
// based on the sync preference and the sign in status.
- (BOOL)leakCheckItemOnState {
  AuthenticationService* authService =
      AuthenticationServiceFactory::GetForBrowserState(browserState_);
  return [passwordLeakCheckEnabled_ value] && authService->IsAuthenticated();
}

// Sets the leak check switch item's enabled status to |enabled| and
// reconfigures the corresponding cell. If the user is not signed in, |enabled|
// is overriden with |NO|.
- (void)setLeakCheckSwitchItemEnabled:(BOOL)enabled {
  if (!base::FeatureList::IsEnabled(password_manager::features::kLeakDetection))
    return;
  AuthenticationService* authService =
      AuthenticationServiceFactory::GetForBrowserState(browserState_);
  [leakCheckItem_ setEnabled:enabled && authService->IsAuthenticated()];
  [self updateDetailTextLeakCheckItem];
  [self reconfigureCellsForItems:@[ leakCheckItem_ ]];
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

// Updates the detail text of the leak check item based on the state.
- (void)updateDetailTextLeakCheckItem {
  if (!leakCheckItem_) {
    return;
  }
  if (self.editing) {
    // When editing keep the current detail text.
    return;
  }
  AuthenticationService* authService =
      AuthenticationServiceFactory::GetForBrowserState(browserState_);
  if (!authService->IsAuthenticated() && [passwordLeakCheckEnabled_ value]) {
    // If the user is signed out and the sync preference is enabled, this
    // informs that it will be turned on on sign in.
    leakCheckItem_.detailText =
        l10n_util::GetNSString(IDS_IOS_LEAK_CHECK_SIGNED_OUT_ENABLED_DESC);
    return;
  }
  leakCheckItem_.detailText = nil;
}

#pragma mark - Testing

- (void)setReauthenticationModuleForExporter:
    (id<ReauthenticationProtocol>)reauthenticationModule {
  _passwordExporter = [[PasswordExporter alloc]
      initWithReauthenticationModule:reauthenticationModule
                            delegate:self];
}

- (PasswordExporter*)getPasswordExporter {
  return _passwordExporter;
}

#pragma mark - ChromeIdentityServiceObserver

- (void)identityListChanged {
  [self reloadData];
}

- (void)chromeIdentityServiceWillBeDestroyed {
  identityServiceObserver_.reset();
}

@end
