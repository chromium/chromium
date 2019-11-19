// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/autofill/autofill_credit_card_table_view_controller.h"

#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/sys_string_conversions.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/common/autofill_prefs.h"
#import "components/autofill/ios/browser/credit_card_util.h"
#import "components/autofill/ios/browser/personal_data_manager_observer_bridge.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/autofill/personal_data_manager_factory.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/ui/settings/autofill/autofill_add_credit_card_coordinator.h"
#import "ios/chrome/browser/ui/settings/autofill/autofill_constants.h"
#import "ios/chrome/browser/ui/settings/autofill/autofill_credit_card_edit_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/autofill/cells/autofill_data_item.h"
#import "ios/chrome/browser/ui/settings/autofill/features.h"
#import "ios/chrome/browser/ui/settings/cells/settings_switch_cell.h"
#import "ios/chrome/browser/ui/settings/cells/settings_switch_item.h"
#include "ios/chrome/browser/ui/table_view/cells/table_view_cells_constants.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_link_header_footer_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_text_header_footer_item.h"
#include "ios/chrome/browser/ui/ui_feature_flags.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/colors/semantic_color_names.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierSwitches = kSectionIdentifierEnumZero,
  SectionIdentifierCards,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeAutofillCardSwitch = kItemTypeEnumZero,
  ItemTypeAutofillCardSwitchSubtitle,
  ItemTypeCard,
  ItemTypeHeader,
};

}  // namespace

#pragma mark - AutofillCreditCardTableViewController

@interface AutofillCreditCardTableViewController () <
    PersonalDataManagerObserver> {
  autofill::PersonalDataManager* _personalDataManager;

  ios::ChromeBrowserState* _browserState;
  std::unique_ptr<autofill::PersonalDataManagerObserverBridge> _observer;
}

@property(nonatomic, getter=isAutofillCreditCardEnabled)
    BOOL autofillCreditCardEnabled;

// Deleting credit cards updates PersonalDataManager resulting in an observer
// callback, which handles general data updates with a reloadData.
// It is better to handle user-initiated changes with more specific actions
// such as inserting or removing items/sections. This boolean is used to
// stop the observer callback from acting on user-initiated changes.
@property(nonatomic, readwrite, assign) BOOL deletionInProgress;

// Button to add a new credit card.
@property(nonatomic, strong) UIBarButtonItem* addPaymentMethodButton;

// Coordinator to add new credit card.
@property(nonatomic, strong)
    AutofillAddCreditCardCoordinator* addCreditCardCoordinator;

@end

@implementation AutofillCreditCardTableViewController

#pragma mark - ViewController Life Cycle.

- (instancetype)initWithBrowserState:(ios::ChromeBrowserState*)browserState {
  DCHECK(browserState);
  UITableViewStyle style = base::FeatureList::IsEnabled(kSettingsRefresh)
                               ? UITableViewStylePlain
                               : UITableViewStyleGrouped;
  self = [super initWithTableViewStyle:style
                           appBarStyle:ChromeTableViewControllerStyleNoAppBar];
  if (self) {
    self.title = l10n_util::GetNSString(IDS_AUTOFILL_PAYMENT_METHODS);
    self.shouldHideDoneButton = YES;
    _browserState = browserState;
    _personalDataManager =
        autofill::PersonalDataManagerFactory::GetForBrowserState(_browserState);
    _observer.reset(new autofill::PersonalDataManagerObserverBridge(self));
    _personalDataManager->AddObserver(_observer.get());
  }
  return self;
}

- (void)dealloc {
  _personalDataManager->RemoveObserver(_observer.get());
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.tableView.allowsMultipleSelectionDuringEditing = YES;
  self.tableView.accessibilityIdentifier = kAutofillCreditCardTableViewId;
  self.navigationController.toolbar.accessibilityIdentifier =
      kAutofillPaymentMethodsToolbarId;

  base::RecordAction(base::UserMetricsAction("AutofillCreditCardsViewed"));
  if (base::FeatureList::IsEnabled(kSettingsAddPaymentMethod)) {
    [self setToolbarItems:@[ [self flexibleSpace], self.addPaymentMethodButton ]
                 animated:YES];
  }
  [self updateUIForEditState];
  [self loadModel];
}

- (void)setEditing:(BOOL)editing animated:(BOOL)animated {
  [super setEditing:editing animated:animated];
  if (editing) {
    self.deleteButton.enabled = NO;
    [self showDeleteButton];
    [self setSwitchItemEnabled:NO itemType:ItemTypeAutofillCardSwitch];
  } else {
    [self hideDeleteButton];
    [self setSwitchItemEnabled:YES itemType:ItemTypeAutofillCardSwitch];
  }
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];
  if (base::FeatureList::IsEnabled(kSettingsAddPaymentMethod)) {
    self.navigationController.toolbarHidden = NO;
  }
}

- (BOOL)shouldHideToolbar {
  // There is a bug from apple that this method might be called in this view
  // controller even if it is not the top view controller.
  if (self.navigationController.topViewController == self &&
      base::FeatureList::IsEnabled(kSettingsAddPaymentMethod)) {
    return NO;
  }

  return [super shouldHideToolbar];
}

#pragma mark - ChromeTableViewController

- (void)loadModel {
  [super loadModel];
  TableViewModel* model = self.tableViewModel;

  [model addSectionWithIdentifier:SectionIdentifierSwitches];
  [model addItem:[self cardSwitchItem]
      toSectionWithIdentifier:SectionIdentifierSwitches];
  [model setFooter:[self cardSwitchFooter]
      forSectionWithIdentifier:SectionIdentifierSwitches];

  [self populateCardSection];
}

#pragma mark - LoadModel Helpers

// Populates card section using personalDataManager.
- (void)populateCardSection {
  TableViewModel* model = self.tableViewModel;
  const std::vector<autofill::CreditCard*>& creditCards =
      _personalDataManager->GetCreditCards();
  if (!creditCards.empty()) {
    [model addSectionWithIdentifier:SectionIdentifierCards];
    [model setHeader:[self cardSectionHeader]
        forSectionWithIdentifier:SectionIdentifierCards];
    for (autofill::CreditCard* creditCard : creditCards) {
      DCHECK(creditCard);
      [model addItem:[self itemForCreditCard:*creditCard]
          toSectionWithIdentifier:SectionIdentifierCards];
    }
  }
}

- (TableViewItem*)cardSwitchItem {
  SettingsSwitchItem* switchItem =
      [[SettingsSwitchItem alloc] initWithType:ItemTypeAutofillCardSwitch];
  switchItem.text =
      l10n_util::GetNSString(IDS_AUTOFILL_ENABLE_CREDIT_CARDS_TOGGLE_LABEL);
  switchItem.on = [self isAutofillCreditCardEnabled];
  switchItem.accessibilityIdentifier = kAutofillCreditCardSwitchViewId;
  return switchItem;
}

- (TableViewHeaderFooterItem*)cardSwitchFooter {
  TableViewLinkHeaderFooterItem* footer = [[TableViewLinkHeaderFooterItem alloc]
      initWithType:ItemTypeAutofillCardSwitchSubtitle];
  footer.text =
      l10n_util::GetNSString(IDS_AUTOFILL_ENABLE_CREDIT_CARDS_TOGGLE_SUBLABEL);
  return footer;
}

- (TableViewHeaderFooterItem*)cardSectionHeader {
  TableViewTextHeaderFooterItem* header =
      [[TableViewTextHeaderFooterItem alloc] initWithType:ItemTypeHeader];
  header.text = l10n_util::GetNSString(IDS_AUTOFILL_PAYMENT_METHODS);
  return header;
}

- (TableViewItem*)itemForCreditCard:(const autofill::CreditCard&)creditCard {
  std::string guid(creditCard.guid());
  NSString* creditCardName = autofill::GetCreditCardName(
      creditCard, GetApplicationContext()->GetApplicationLocale());

  AutofillDataItem* item = [[AutofillDataItem alloc] initWithType:ItemTypeCard];
  item.text = creditCardName;
  item.leadingDetailText = autofill::GetCreditCardObfuscatedNumber(creditCard);
  item.accessoryType = UITableViewCellAccessoryDisclosureIndicator;
  item.accessibilityIdentifier = creditCardName;
  item.deletable = autofill::IsCreditCardLocal(creditCard);
  item.GUID = guid;
  if (![item isDeletable]) {
    item.trailingDetailText =
        l10n_util::GetNSString(IDS_IOS_AUTOFILL_WALLET_SERVER_NAME);
  }
  return item;
}

- (BOOL)localCreditCardsExist {
  return !_personalDataManager->GetLocalCreditCards().empty();
}

#pragma mark - SettingsRootTableViewController

- (BOOL)shouldShowEditButton {
  return YES;
}

- (BOOL)editButtonEnabled {
  DCHECK([self shouldShowEditButton]);
  return [self localCreditCardsExist];
}

- (void)deleteItems:(NSArray<NSIndexPath*>*)indexPaths {
  // Do not call super as this also deletes the section if it is empty.
  [self deleteItemAtIndexPaths:indexPaths];
}

#pragma mark - UITableViewDataSource

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  UITableViewCell* cell = [super tableView:tableView
                     cellForRowAtIndexPath:indexPath];

  ItemType itemType = static_cast<ItemType>(
      [self.tableViewModel itemTypeForIndexPath:indexPath]);
  if (itemType == ItemTypeAutofillCardSwitch) {
    SettingsSwitchCell* switchCell =
        base::mac::ObjCCastStrict<SettingsSwitchCell>(cell);
    [switchCell.switchView addTarget:self
                              action:@selector(autofillCardSwitchChanged:)
                    forControlEvents:UIControlEventValueChanged];
  }

  return cell;
}

#pragma mark - Switch Callbacks

- (void)autofillCardSwitchChanged:(UISwitch*)switchView {
  [self setSwitchItemOn:[switchView isOn] itemType:ItemTypeAutofillCardSwitch];
  [self setAutofillCreditCardEnabled:[switchView isOn]];
}

#pragma mark - Switch Helpers

// Sets switchItem's state to |on|. It is important that there is only one item
// of |switchItemType| in SectionIdentifierSwitches.
- (void)setSwitchItemOn:(BOOL)on itemType:(ItemType)switchItemType {
  NSIndexPath* switchPath =
      [self.tableViewModel indexPathForItemType:switchItemType
                              sectionIdentifier:SectionIdentifierSwitches];
  SettingsSwitchItem* switchItem =
      base::mac::ObjCCastStrict<SettingsSwitchItem>(
          [self.tableViewModel itemAtIndexPath:switchPath]);
  switchItem.on = on;
}

// Sets switchItem's enabled status to |enabled| and reconfigures the
// corresponding cell. It is important that there is no more than one item of
// |switchItemType| in SectionIdentifierSwitches.
- (void)setSwitchItemEnabled:(BOOL)enabled itemType:(ItemType)switchItemType {
  TableViewModel* model = self.tableViewModel;

  if (![model hasItemForItemType:switchItemType
               sectionIdentifier:SectionIdentifierSwitches]) {
    return;
  }
  NSIndexPath* switchPath =
      [model indexPathForItemType:switchItemType
                sectionIdentifier:SectionIdentifierSwitches];
  SettingsSwitchItem* switchItem =
      base::mac::ObjCCastStrict<SettingsSwitchItem>(
          [model itemAtIndexPath:switchPath]);
  [switchItem setEnabled:enabled];
  [self reconfigureCellsForItems:@[ switchItem ]];
}

#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  [super tableView:tableView didSelectRowAtIndexPath:indexPath];

  // Edit mode is the state where the user can select and delete entries. In
  // edit mode, selection is handled by the superclass. When not in edit mode
  // selection presents the editing controller for the selected entry.
  if (self.editing) {
    self.deleteButton.enabled = YES;
    return;
  }

  [tableView deselectRowAtIndexPath:indexPath animated:YES];
  TableViewModel* model = self.tableViewModel;
  NSInteger type = [model itemTypeForIndexPath:indexPath];
  if (type != ItemTypeCard)
    return;

  const std::vector<autofill::CreditCard*>& creditCards =
      _personalDataManager->GetCreditCards();
  AutofillCreditCardEditTableViewController* controller =
      [[AutofillCreditCardEditTableViewController alloc]
           initWithCreditCard:*creditCards[indexPath.item]
          personalDataManager:_personalDataManager];
  controller.dispatcher = self.dispatcher;
  [self.navigationController pushViewController:controller animated:YES];
}

- (void)tableView:(UITableView*)tableView
    didDeselectRowAtIndexPath:(NSIndexPath*)indexPath {
  [super tableView:tableView didDeselectRowAtIndexPath:indexPath];
  if (!self.tableView.editing)
    return;

  if (self.tableView.indexPathsForSelectedRows.count == 0)
    self.deleteButton.enabled = NO;
}

#pragma mark - UITableViewDataSource

- (BOOL)tableView:(UITableView*)tableView
    canEditRowAtIndexPath:(NSIndexPath*)indexPath {
  // Only autofill data cells are editable.
  TableViewItem* item = [self.tableViewModel itemAtIndexPath:indexPath];
  if ([item isKindOfClass:[AutofillDataItem class]]) {
    AutofillDataItem* autofillItem =
        base::mac::ObjCCastStrict<AutofillDataItem>(item);
    return [autofillItem isDeletable];
  }
  return NO;
}

- (void)tableView:(UITableView*)tableView
    commitEditingStyle:(UITableViewCellEditingStyle)editingStyle
     forRowAtIndexPath:(NSIndexPath*)indexPath {
  if (editingStyle != UITableViewCellEditingStyleDelete)
    return;
  [self deleteItemAtIndexPaths:@[ indexPath ]];
}

#pragma mark - helper methods

- (void)deleteItemAtIndexPaths:(NSArray<NSIndexPath*>*)indexPaths {
  self.deletionInProgress = YES;
  for (NSIndexPath* indexPath in indexPaths) {
    AutofillDataItem* item = base::mac::ObjCCastStrict<AutofillDataItem>(
        [self.tableViewModel itemAtIndexPath:indexPath]);
    _personalDataManager->RemoveByGUID(item.GUID);
  }

  self.editing = NO;
  __weak AutofillCreditCardTableViewController* weakSelf = self;
  [self.tableView
      performBatchUpdates:^{
        // Obtain strong reference again.
        AutofillCreditCardTableViewController* strongSelf = weakSelf;
        if (!strongSelf) {
          return;
        }

        NSUInteger section = [self.tableViewModel
            sectionForSectionIdentifier:SectionIdentifierCards];
        NSUInteger currentCount =
            [self.tableViewModel numberOfItemsInSection:section];
        if (currentCount == indexPaths.count) {
          [[strongSelf tableViewModel]
              removeSectionWithIdentifier:SectionIdentifierCards];
          [[strongSelf tableView]
                deleteSections:[NSIndexSet indexSetWithIndex:section]
              withRowAnimation:UITableViewRowAnimationAutomatic];
        } else {
          [strongSelf removeFromModelItemAtIndexPaths:indexPaths];
          [strongSelf.tableView
              deleteRowsAtIndexPaths:indexPaths
                    withRowAnimation:UITableViewRowAnimationAutomatic];
        }
      }
      completion:^(BOOL finished) {
        // Obtain strong reference again.
        AutofillCreditCardTableViewController* strongSelf = weakSelf;
        if (!strongSelf) {
          return;
        }

        // Turn off edit mode if there is nothing to edit.
        if (![strongSelf localCreditCardsExist] && strongSelf.editing) {
          [strongSelf setEditing:NO animated:YES];
        }
        [strongSelf updateUIForEditState];
        strongSelf.deletionInProgress = NO;
      }];
}

// Opens new view controller |AutofillAddCreditCardViewController| for fillig
// credit card details.
- (void)handleAddPayment:(id)sender {
  DCHECK(base::FeatureList::IsEnabled(kSettingsAddPaymentMethod));
  base::RecordAction(
      base::UserMetricsAction("MobileAddCreditCard.AddPaymentMethodButton"));

  self.addCreditCardCoordinator = [[AutofillAddCreditCardCoordinator alloc]
      initWithBaseViewController:self
                    browserState:_browserState];

  [self.addCreditCardCoordinator start];
}

#pragma mark PersonalDataManagerObserver

- (void)onPersonalDataChanged {
  if (self.deletionInProgress)
    return;

  if (![self localCreditCardsExist] && self.editing) {
    // Turn off edit mode if there exists nothing to edit.
    [self setEditing:NO animated:YES];
  }

  [self updateUIForEditState];
  [self reloadData];
}

#pragma mark - Getters and Setter

- (BOOL)isAutofillCreditCardEnabled {
  return autofill::prefs::IsCreditCardAutofillEnabled(
      _browserState->GetPrefs());
}

- (void)setAutofillCreditCardEnabled:(BOOL)isEnabled {
  return autofill::prefs::SetCreditCardAutofillEnabled(
      _browserState->GetPrefs(), isEnabled);
}

- (UIBarButtonItem*)addPaymentMethodButton {
  if (!_addPaymentMethodButton) {
    _addPaymentMethodButton = [[UIBarButtonItem alloc]
        initWithTitle:l10n_util::GetNSString(
                          IDS_IOS_MANUAL_FALLBACK_ADD_PAYMENT_METHOD)
                style:UIBarButtonItemStylePlain
               target:self
               action:@selector(handleAddPayment:)];
    _addPaymentMethodButton.accessibilityIdentifier =
        kSettingsAddPaymentMethodButtonId;
  }
  return _addPaymentMethodButton;
}

#pragma mark - Private

// Create a flexible space item to be used in the toolbar.
- (UIBarButtonItem*)flexibleSpace {
  return [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemFlexibleSpace
                           target:nil
                           action:nil];
}

// Adds delete button to the bottom toolbar.
- (void)showDeleteButton {
  NSArray* customToolbarItems;
  if (base::FeatureList::IsEnabled(kSettingsAddPaymentMethod)) {
    customToolbarItems = @[
      self.deleteButton, [self flexibleSpace], self.addPaymentMethodButton
    ];
  } else {
    customToolbarItems =
        @[ [self flexibleSpace], self.deleteButton, [self flexibleSpace] ];
  }
  [self setToolbarItems:customToolbarItems animated:YES];
}

// Removes delete button from the bottom toolbar.
- (void)hideDeleteButton {
  NSArray* customToolbarItems;
  if (base::FeatureList::IsEnabled(kSettingsAddPaymentMethod)) {
    customToolbarItems = @[ [self flexibleSpace], self.addPaymentMethodButton ];
  }
  [self setToolbarItems:customToolbarItems animated:YES];
}

@end
