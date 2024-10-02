// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/autofill/autofill_credit_card_table_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/check.h"
#import "base/feature_list.h"
#import "base/memory/raw_ptr.h"
#import "base/metrics/user_metrics.h"
#import "base/strings/sys_string_conversions.h"
#import "components/autofill/core/browser/metrics/payments/mandatory_reauth_metrics.h"
#import "components/autofill/core/browser/payments_data_manager.h"
#import "components/autofill/core/browser/personal_data_manager.h"
#import "components/autofill/core/common/autofill_payments_features.h"
#import "components/autofill/core/common/autofill_prefs.h"
#import "components/autofill/ios/browser/credit_card_util.h"
#import "components/autofill/ios/browser/personal_data_manager_observer_bridge.h"
#import "components/password_manager/core/common/password_manager_features.h"
#import "components/prefs/pref_service.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/autofill/model/personal_data_manager_factory.h"
#import "ios/chrome/browser/autofill/ui_bundled/scoped_autofill_payment_reauth_module_override.h"
#import "ios/chrome/browser/net/model/crurl.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_info_button_cell.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_info_button_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_link_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_switch_cell.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_switch_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/settings/autofill/autofill_add_credit_card_coordinator.h"
#import "ios/chrome/browser/ui/settings/autofill/autofill_add_credit_card_coordinator_delegate.h"
#import "ios/chrome/browser/ui/settings/autofill/autofill_credit_card_edit_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/autofill/autofill_settings_constants.h"
#import "ios/chrome/browser/ui/settings/autofill/cells/autofill_card_item.h"
#import "ios/chrome/browser/ui/settings/elements/enterprise_info_popover_view_controller.h"
#import "ios/chrome/browser/ui/settings/settings_root_table_view_controller+toolbar_add.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/reauthentication/reauthentication_module.h"
#import "ios/chrome/common/ui/reauthentication/reauthentication_protocol.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "net/base/apple/url_conversions.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

constexpr base::TimeDelta kUpdatePrefDelay = base::Seconds(0.3);

enum SectionIdentifier : NSInteger {
  SectionIdentifierAutofillCardSwitch = kSectionIdentifierEnumZero,
  SectionIdentifierMandatoryReauthSwitch,
  SectionIdentifierCards,
};

enum ItemType : NSInteger {
  ItemTypeAutofillCardSwitch = kItemTypeEnumZero,
  ItemTypeAutofillCardManaged,
  ItemTypeAutofillCardSwitchSubtitle,
  ItemTypeCard,
  ItemTypeHeader,
  ItemTypeMandatoryReauthSwitch,
  ItemTypeMandatoryReauthSwitchSubtitle,
};

}  // namespace

using autofill::autofill_metrics::LogMandatoryReauthOptInOrOutUpdateEvent;
using autofill::autofill_metrics::LogMandatoryReauthSettingsPageEditCardEvent;
using autofill::autofill_metrics::MandatoryReauthAuthenticationFlowEvent;
using autofill::autofill_metrics::MandatoryReauthOptInOrOutSource;

#pragma mark - AutofillCreditCardTableViewController

@interface AutofillCreditCardTableViewController () <
    AutofillAddCreditCardCoordinatorDelegate,
    PersonalDataManagerObserver,
    PopoverLabelViewControllerDelegate,
    SuccessfulReauthTimeAccessor> {
  raw_ptr<autofill::PersonalDataManager> _personalDataManager;

  raw_ptr<Browser> _browser;
  std::unique_ptr<autofill::PersonalDataManagerObserverBridge> _observer;

  // Whether Settings have been dismissed.
  BOOL _settingsAreDismissed;

  // Timestamp for last successful reauth attempt by the ReauthenticationModule.
  NSDate* _lastSuccessfulReauthTime;
}

@property(nonatomic, getter=isAutofillCreditCardEnabled)
    BOOL autofillCreditCardEnabled;

// Deleting credit cards updates PersonalDataManager resulting in an observer
// callback, which handles general data updates with a reloadData.
// It is better to handle user-initiated changes with more specific actions
// such as inserting or removing items/sections. This boolean is used to
// stop the observer callback from acting on user-initiated changes.
@property(nonatomic, readwrite, assign) BOOL deletionInProgress;

// Coordinator to add new credit card.
@property(nonatomic, strong)
    AutofillAddCreditCardCoordinator* addCreditCardCoordinator;

// Add button for the toolbar.
@property(nonatomic, strong) UIBarButtonItem* addButtonInToolbar;

// Reauthentication module.
@property(nonatomic, strong) ReauthenticationModule* reauthenticationModule;

@end

@implementation AutofillCreditCardTableViewController

#pragma mark - ViewController Life Cycle.

- (instancetype)initWithBrowser:(Browser*)browser {
  DCHECK(browser);

  self = [super initWithStyle:ChromeTableViewStyle()];
  if (self) {
    self.title = l10n_util::GetNSString(IDS_AUTOFILL_PAYMENT_METHODS);
    self.shouldDisableDoneButtonOnEdit = YES;
    _browser = browser;
    _personalDataManager = autofill::PersonalDataManagerFactory::GetForProfile(
        _browser->GetProfile());
    _observer.reset(new autofill::PersonalDataManagerObserverBridge(self));
    _personalDataManager->AddObserver(_observer.get());
  }
  return self;
}

#pragma mark - properties

- (ReauthenticationModule*)reauthenticationModule {
  id<ReauthenticationProtocol> overrideModule =
      ScopedAutofillPaymentReauthModuleOverride::Get();
  if (overrideModule) {
    return overrideModule;
  }

  if (!_reauthenticationModule) {
    _reauthenticationModule = [[ReauthenticationModule alloc]
        initWithSuccessfulReauthTimeAccessor:self];
  }
  return _reauthenticationModule;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.tableView.allowsMultipleSelectionDuringEditing = YES;
  self.tableView.accessibilityIdentifier = kAutofillCreditCardTableViewId;
  self.navigationController.toolbar.accessibilityIdentifier =
      kAutofillPaymentMethodsToolbarId;
  [self updateUIForEditState];
  [self loadModel];
}

- (void)setEditing:(BOOL)editing animated:(BOOL)animated {
  [super setEditing:editing animated:animated];
  if (editing) {
    self.deleteButton.enabled = NO;
  }

  // We don't want to update this preference when it is in editing mode.
  [self setSwitchItemEnabled:!editing
                    itemType:ItemTypeAutofillCardSwitch
           sectionIdentifier:SectionIdentifierAutofillCardSwitch];
  [self setSwitchItemEnabled:!editing
                    itemType:ItemTypeMandatoryReauthSwitch
           sectionIdentifier:SectionIdentifierMandatoryReauthSwitch];

  [self updateUIForEditState];
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];
  self.navigationController.toolbarHidden = NO;
}

#pragma mark - LegacyChromeTableViewController

- (void)loadModel {
  [super loadModel];
  if (_settingsAreDismissed) {
    return;
  }

  TableViewModel* model = self.tableViewModel;

  [model addSectionWithIdentifier:SectionIdentifierAutofillCardSwitch];
  if (_browser->GetProfile()->GetPrefs()->IsManagedPreference(
          autofill::prefs::kAutofillCreditCardEnabled)) {
    [model addItem:[self cardManagedItem]
        toSectionWithIdentifier:SectionIdentifierAutofillCardSwitch];
  } else {
    [model addItem:[self cardSwitchItem]
        toSectionWithIdentifier:SectionIdentifierAutofillCardSwitch];
  }

  [model setFooter:[self cardSwitchFooter]
      forSectionWithIdentifier:SectionIdentifierAutofillCardSwitch];

  [model addSectionWithIdentifier:SectionIdentifierMandatoryReauthSwitch];
  [model addItem:[self mandatoryReauthSwitchItem]
      toSectionWithIdentifier:SectionIdentifierMandatoryReauthSwitch];
  [model setFooter:[self mandatoryReauthSwitchFooter]
      forSectionWithIdentifier:SectionIdentifierMandatoryReauthSwitch];

  [self populateCardSection];
}

#pragma mark - LoadModel Helpers

// Populates card section using personalDataManager.
- (void)populateCardSection {
  if (_settingsAreDismissed) {
    return;
  }

  TableViewModel* model = self.tableViewModel;
  const std::vector<autofill::CreditCard*>& creditCards =
      _personalDataManager->payments_data_manager().GetCreditCards();
  if (!creditCards.empty()) {
    [model addSectionWithIdentifier:SectionIdentifierCards];
    [model setHeader:[self cardSectionHeader]
        forSectionWithIdentifier:SectionIdentifierCards];
    for (const autofill::CreditCard* creditCard : creditCards) {
      DCHECK(creditCard);
      [model addItem:[self itemForCreditCard:*creditCard]
          toSectionWithIdentifier:SectionIdentifierCards];
    }
  }
}

- (TableViewItem*)cardSwitchItem {
  TableViewSwitchItem* switchItem =
      [[TableViewSwitchItem alloc] initWithType:ItemTypeAutofillCardSwitch];
  switchItem.text =
      l10n_util::GetNSString(IDS_AUTOFILL_ENABLE_CREDIT_CARDS_TOGGLE_LABEL);
  switchItem.on = [self isAutofillCreditCardEnabled];
  switchItem.accessibilityIdentifier = kAutofillCreditCardSwitchViewId;
  return switchItem;
}

- (TableViewInfoButtonItem*)cardManagedItem {
  TableViewInfoButtonItem* cardManagedItem = [[TableViewInfoButtonItem alloc]
      initWithType:ItemTypeAutofillCardManaged];
  cardManagedItem.text =
      l10n_util::GetNSString(IDS_AUTOFILL_ENABLE_CREDIT_CARDS_TOGGLE_LABEL);
  // The status could only be off when the pref is managed.
  cardManagedItem.statusText = l10n_util::GetNSString(IDS_IOS_SETTING_OFF);
  cardManagedItem.accessibilityHint =
      l10n_util::GetNSString(IDS_IOS_TOGGLE_SETTING_MANAGED_ACCESSIBILITY_HINT);
  cardManagedItem.accessibilityIdentifier = kAutofillCreditCardManagedViewId;
  return cardManagedItem;
}

- (TableViewHeaderFooterItem*)cardSwitchFooter {
  TableViewLinkHeaderFooterItem* footer = [[TableViewLinkHeaderFooterItem alloc]
      initWithType:ItemTypeAutofillCardSwitchSubtitle];
  footer.text =
      l10n_util::GetNSString(IDS_AUTOFILL_ENABLE_CREDIT_CARDS_TOGGLE_SUBLABEL);
  return footer;
}

- (TableViewItem*)mandatoryReauthSwitchItem {
  TableViewSwitchItem* switchItem =
      [[TableViewSwitchItem alloc] initWithType:ItemTypeMandatoryReauthSwitch];
  switchItem.text = l10n_util::GetNSString(
      IDS_PAYMENTS_AUTOFILL_ENABLE_MANDATORY_REAUTH_TOGGLE_LABEL);
  switchItem.accessibilityIdentifier = kAutofillMandatoryReauthSwitchViewId;
  BOOL canAttemptReauth = [self.reauthenticationModule canAttemptReauth];
  switchItem.enabled = canAttemptReauth;
  switchItem.on =
      canAttemptReauth && _personalDataManager->payments_data_manager()
                              .IsPaymentMethodsMandatoryReauthEnabled();
  return switchItem;
}

- (TableViewHeaderFooterItem*)mandatoryReauthSwitchFooter {
  TableViewLinkHeaderFooterItem* footer = [[TableViewLinkHeaderFooterItem alloc]
      initWithType:ItemTypeMandatoryReauthSwitchSubtitle];
  footer.text = l10n_util::GetNSString(
      IDS_PAYMENTS_AUTOFILL_ENABLE_MANDATORY_REAUTH_TOGGLE_SUBLABEL);
  return footer;
}

- (TableViewHeaderFooterItem*)cardSectionHeader {
  TableViewTextHeaderFooterItem* header =
      [[TableViewTextHeaderFooterItem alloc] initWithType:ItemTypeHeader];
  header.text = l10n_util::GetNSString(IDS_AUTOFILL_PAYMENT_METHODS);
  return header;
}

// TODO(crbug.com/40123293): Add egtest for server cards.
- (TableViewItem*)itemForCreditCard:(const autofill::CreditCard&)creditCard {
  std::string guid(creditCard.guid());
  NSString* creditCardName = autofill::GetCreditCardName(
      creditCard, GetApplicationContext()->GetApplicationLocale());

  AutofillCardItem* item = [[AutofillCardItem alloc] initWithType:ItemTypeCard];
  item.text = creditCardName;
  item.leadingDetailText =
      autofill::GetCreditCardNameAndLastFourDigits(creditCard);
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
  return !_settingsAreDismissed &&
         !_personalDataManager->payments_data_manager()
              .GetLocalCreditCards()
              .empty();
}

#pragma mark - SettingsControllerProtocol

- (void)reportDismissalUserAction {
  base::RecordAction(base::UserMetricsAction("MobileCreditCardSettingsClose"));
}

- (void)reportBackUserAction {
  base::RecordAction(base::UserMetricsAction("MobileCreditCardSettingsBack"));
}

- (void)settingsWillBeDismissed {
  DCHECK(!_settingsAreDismissed);

  _personalDataManager->RemoveObserver(_observer.get());
  [self stopAutofillAddCreditCardCoordinator];

  // Remove observer bridges.
  _observer.reset();

  // Clear C++ ivars.
  _personalDataManager = nullptr;
  _browser = nullptr;

  _settingsAreDismissed = YES;
}

#pragma mark - SettingsRootTableViewController

- (BOOL)editButtonEnabled {
  return [self localCreditCardsExist];
}

// Override editButtonPressed to support triggering mandatory reauth when the
// user wants to edit/delete the card.
- (void)editButtonPressed {
  // If 1. reauth is not available or 2. reauth succeeded, we
  // proceed by calling the parent's editButtonPressed. Otherwise return
  // early and do nothing.
  if (_personalDataManager->payments_data_manager()
          .IsPaymentMethodsMandatoryReauthEnabled() &&
      [self.reauthenticationModule canAttemptReauth]) {
    LogMandatoryReauthSettingsPageDeleteCardEvent(
        MandatoryReauthAuthenticationFlowEvent::kFlowStarted);

    auto completionHandler = ^(ReauthenticationResult result) {
      switch (result) {
        case ReauthenticationResult::kSuccess:
          LogMandatoryReauthSettingsPageDeleteCardEvent(
              MandatoryReauthAuthenticationFlowEvent::kFlowSucceeded);
          [super editButtonPressed];
          break;
        case ReauthenticationResult::kSkipped:
          LogMandatoryReauthSettingsPageDeleteCardEvent(
              MandatoryReauthAuthenticationFlowEvent::kFlowSkipped);
          [super editButtonPressed];
          break;
        case ReauthenticationResult::kFailure:
          LogMandatoryReauthSettingsPageDeleteCardEvent(
              MandatoryReauthAuthenticationFlowEvent::kFlowFailed);
          break;
      }
    };
    [self.reauthenticationModule
        attemptReauthWithLocalizedReason:
            l10n_util::GetNSString(
                IDS_PAYMENTS_AUTOFILL_SETTINGS_EDIT_MANDATORY_REAUTH)
                    canReusePreviousAuth:YES
                                 handler:completionHandler];
  } else {
    [super editButtonPressed];
  }
}

- (void)deleteItems:(NSArray<NSIndexPath*>*)indexPaths {
  // Do not call super as this also deletes the section if it is empty.
  [self deleteItemAtIndexPaths:indexPaths];
}

- (BOOL)shouldHideToolbar {
  // There is a bug from apple that this method might be called in this view
  // controller even if it is not the top view controller.
  if (self.navigationController.topViewController == self) {
    return NO;
  }

  return [super shouldHideToolbar];
}

- (BOOL)shouldShowEditDoneButton {
  // The "Done" button in the navigation bar closes the sheet.
  return NO;
}

- (void)updateUIForEditState {
  [super updateUIForEditState];
  [self updatedToolbarForEditState];
}

- (UIBarButtonItem*)customLeftToolbarButton {
  if (self.tableView.isEditing) {
    return nil;
  }

  return self.addButtonInToolbar;
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

#pragma mark - UITableViewDataSource

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  UITableViewCell* cell = [super tableView:tableView
                     cellForRowAtIndexPath:indexPath];

  switch (static_cast<ItemType>(
      [self.tableViewModel itemTypeForIndexPath:indexPath])) {
    case ItemTypeAutofillCardSwitchSubtitle:
    case ItemTypeCard:
    case ItemTypeHeader:
    case ItemTypeMandatoryReauthSwitchSubtitle:
      break;
    case ItemTypeMandatoryReauthSwitch: {
      TableViewSwitchCell* switchCell =
          base::apple::ObjCCastStrict<TableViewSwitchCell>(cell);
      [switchCell.switchView addTarget:self
                                action:@selector(mandatoryReauthSwitchChanged:)
                      forControlEvents:UIControlEventValueChanged];
      break;
    }
    case ItemTypeAutofillCardSwitch: {
      TableViewSwitchCell* switchCell =
          base::apple::ObjCCastStrict<TableViewSwitchCell>(cell);
      [switchCell.switchView addTarget:self
                                action:@selector(autofillCardSwitchChanged:)
                      forControlEvents:UIControlEventValueChanged];
      break;
    }
    case ItemTypeAutofillCardManaged: {
      TableViewInfoButtonCell* managedCell =
          base::apple::ObjCCastStrict<TableViewInfoButtonCell>(cell);
      [managedCell.trailingButton
                 addTarget:self
                    action:@selector(didTapManagedUIInfoButton:)
          forControlEvents:UIControlEventTouchUpInside];
      break;
    }
  }

  return cell;
}

#pragma mark - Switch Callbacks

- (void)autofillCardSwitchChanged:(UISwitch*)switchView {
  [self setSwitchItemOn:[switchView isOn]
               itemType:ItemTypeAutofillCardSwitch
      sectionIdentifier:SectionIdentifierAutofillCardSwitch];

  // Delay updating the pref when VoiceOver is running to prevent a temporary
  // focus shift due to simultaneous UI updates, see crbug.com/326923292.
  if (UIAccessibilityIsVoiceOverRunning()) {
    __weak __typeof(self) weakSelf = self;
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, base::BindOnce(^{
          [weakSelf
              updateAutofillCreditCardPrefAndToolbarForState:[switchView isOn]];
        }),
        kUpdatePrefDelay);
  } else {
    [self updateAutofillCreditCardPrefAndToolbarForState:[switchView isOn]];
  }
}

- (void)mandatoryReauthSwitchChanged:(UISwitch*)switchView {
  if ([self.reauthenticationModule canAttemptReauth]) {
    // Get the original value.
    BOOL mandatoryReauthEnabled = _personalDataManager->payments_data_manager()
                                      .IsPaymentMethodsMandatoryReauthEnabled();
    LogMandatoryReauthOptInOrOutUpdateEvent(
        MandatoryReauthOptInOrOutSource::kSettingsPage,
        /*opt_in=*/!mandatoryReauthEnabled,
        MandatoryReauthAuthenticationFlowEvent::kFlowStarted);

    __weak __typeof(self) weakSelf = self;
    [self.reauthenticationModule
        attemptReauthWithLocalizedReason:
            l10n_util::GetNSString(
                IDS_PAYMENTS_AUTOFILL_SETTINGS_TOGGLE_MANDATORY_REAUTH)
                    canReusePreviousAuth:YES
                                 handler:^(ReauthenticationResult result) {
                                   [weakSelf
                                       handleReauthenticationResult:result];
                                 }];
  } else {
    // Reauth is not supported. Disable the Mandatory Reauth switch and set its
    // value to switched-off.
    [self setSwitchItemEnabled:NO
                      itemType:ItemTypeMandatoryReauthSwitch
             sectionIdentifier:SectionIdentifierMandatoryReauthSwitch];
    [self setSwitchItemOn:NO
                 itemType:ItemTypeMandatoryReauthSwitch
        sectionIdentifier:SectionIdentifierMandatoryReauthSwitch];
  }
}

#pragma mark - Switch Helpers

// Sets switchItem's state to `on`. It is important that there is only one item
// of `switchItemType` in section with `sectionIdentifier`.
- (void)setSwitchItemOn:(BOOL)on
               itemType:(ItemType)switchItemType
      sectionIdentifier:(SectionIdentifier)sectionIdentifier {
  NSIndexPath* switchPath =
      [self.tableViewModel indexPathForItemType:switchItemType
                              sectionIdentifier:sectionIdentifier];
  TableViewSwitchItem* switchItem =
      base::apple::ObjCCastStrict<TableViewSwitchItem>(
          [self.tableViewModel itemAtIndexPath:switchPath]);
  switchItem.on = on;
  [self reconfigureCellsForItems:@[ switchItem ]];
}

// Sets switchItem's enabled status to `enabled` and reconfigures the
// corresponding cell. It is important that there is no more than one item of
// `switchItemType` in section with `sectionIdentifier`.
- (void)setSwitchItemEnabled:(BOOL)enabled
                    itemType:(ItemType)switchItemType
           sectionIdentifier:(SectionIdentifier)sectionIdentifier {
  TableViewModel* model = self.tableViewModel;

  if (![model hasItemForItemType:switchItemType
               sectionIdentifier:sectionIdentifier]) {
    return;
  }
  NSIndexPath* switchPath = [model indexPathForItemType:switchItemType
                                      sectionIdentifier:sectionIdentifier];
  TableViewSwitchItem* switchItem =
      base::apple::ObjCCastStrict<TableViewSwitchItem>(
          [model itemAtIndexPath:switchPath]);
  [switchItem setEnabled:enabled];
  [self reconfigureCellsForItems:@[ switchItem ]];
}

#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  [super tableView:tableView didSelectRowAtIndexPath:indexPath];
  if (_settingsAreDismissed) {
    return;
  }

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
      _personalDataManager->payments_data_manager().GetCreditCards();
  autofill::CreditCard selectedCard = *creditCards[indexPath.item];
  if (autofill::IsCreditCardLocal(selectedCard) &&
      _personalDataManager->payments_data_manager()
          .IsPaymentMethodsMandatoryReauthEnabled() &&
      [self.reauthenticationModule canAttemptReauth]) {
    [self attemptReauthenticationForEditCard:selectedCard];
  } else {
    [self openCreditCardDetails:selectedCard];
  }
}

// Attempt reauthentication, if all goes well proceed to card details page.
- (void)attemptReauthenticationForEditCard:(autofill::CreditCard)selectedCard {
  LogMandatoryReauthSettingsPageEditCardEvent(
      MandatoryReauthAuthenticationFlowEvent::kFlowStarted);
  auto completionHandler = ^(ReauthenticationResult result) {
    MandatoryReauthAuthenticationFlowEvent event =
        result == ReauthenticationResult::kFailure
            ? MandatoryReauthAuthenticationFlowEvent::kFlowFailed
            : MandatoryReauthAuthenticationFlowEvent::kFlowSucceeded;
    LogMandatoryReauthSettingsPageEditCardEvent(event);

    if (result != ReauthenticationResult::kFailure) {
      [self openCreditCardDetails:selectedCard];
    }
  };
  [self.reauthenticationModule
      attemptReauthWithLocalizedReason:
          l10n_util::GetNSString(
              IDS_PAYMENTS_AUTOFILL_SETTINGS_EDIT_MANDATORY_REAUTH)
                  canReusePreviousAuth:YES
                               handler:completionHandler];
}

- (void)openCreditCardDetails:(autofill::CreditCard)creditCard {
  AutofillCreditCardEditTableViewController* controller =
      [[AutofillCreditCardEditTableViewController alloc]
           initWithCreditCard:creditCard
          personalDataManager:_personalDataManager];
  [self configureHandlersForRootViewController:controller];
  [self.navigationController pushViewController:controller animated:YES];
}

- (void)tableView:(UITableView*)tableView
    didDeselectRowAtIndexPath:(NSIndexPath*)indexPath {
  [super tableView:tableView didDeselectRowAtIndexPath:indexPath];
  if (_settingsAreDismissed || !self.tableView.editing) {
    return;
  }

  if (self.tableView.indexPathsForSelectedRows.count == 0)
    self.deleteButton.enabled = NO;
}

#pragma mark - UITableViewDataSource

- (BOOL)tableView:(UITableView*)tableView
    canEditRowAtIndexPath:(NSIndexPath*)indexPath {
  if (_settingsAreDismissed) {
    return NO;
  }

  // Only autofill card cells are editable.
  TableViewItem* item = [self.tableViewModel itemAtIndexPath:indexPath];
  if ([item isKindOfClass:[AutofillCardItem class]]) {
    AutofillCardItem* autofillItem =
        base::apple::ObjCCastStrict<AutofillCardItem>(item);
    return [autofillItem isDeletable];
  }
  return NO;
}

- (void)tableView:(UITableView*)tableView
    commitEditingStyle:(UITableViewCellEditingStyle)editingStyle
     forRowAtIndexPath:(NSIndexPath*)indexPath {
  if (_settingsAreDismissed ||
      editingStyle != UITableViewCellEditingStyleDelete) {
    return;
  }
  [self deleteItemAtIndexPaths:@[ indexPath ]];
}

#pragma mark - helper methods

- (void)deleteItemAtIndexPaths:(NSArray<NSIndexPath*>*)indexPaths {
  if (_settingsAreDismissed) {
    return;
  }

  self.deletionInProgress = YES;
  for (NSIndexPath* indexPath in indexPaths) {
    AutofillCardItem* item = base::apple::ObjCCastStrict<AutofillCardItem>(
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

// Opens new view controller `AutofillAddCreditCardViewController` for fillig
// credit card details.
- (void)handleAddPayment {
  if (_settingsAreDismissed) {
    return;
  }

  base::RecordAction(
      base::UserMetricsAction("MobileAddCreditCard.AddPaymentMethodButton"));

  self.addCreditCardCoordinator = [[AutofillAddCreditCardCoordinator alloc]
      initWithBaseViewController:self
                         browser:_browser];
  self.addCreditCardCoordinator.delegate = self;
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

#pragma mark - SuccessfulReauthTimeAccessor

- (void)updateSuccessfulReauthTime {
  _lastSuccessfulReauthTime = [[NSDate alloc] init];
}

- (NSDate*)lastSuccessfulReauthTime {
  return _lastSuccessfulReauthTime;
}

#pragma mark - Getters and Setter

- (BOOL)isAutofillCreditCardEnabled {
  return autofill::prefs::IsAutofillPaymentMethodsEnabled(
      _browser->GetProfile()->GetPrefs());
}

- (void)setAutofillCreditCardEnabled:(BOOL)isEnabled {
  return autofill::prefs::SetAutofillPaymentMethodsEnabled(
      _browser->GetProfile()->GetPrefs(), isEnabled);
}

#pragma mark - PopoverLabelViewControllerDelegate

- (void)didTapLinkURL:(NSURL*)URL {
  [self view:nil didTapLinkURL:[[CrURL alloc] initWithNSURL:URL]];
}

#pragma mark - Private

// Returns a toolbar button for starting the "Add Credit Card" flow.
- (UIBarButtonItem*)addButtonInToolbar {
  if (!_addButtonInToolbar) {
    _addButtonInToolbar =
        [self addButtonWithAction:@selector(addButtonCallback)];
    _addButtonInToolbar.enabled = [self isAutofillCreditCardEnabled];
  }
  return _addButtonInToolbar;
}

- (void)addButtonCallback {
  [self handleAddPayment];
}

- (void)stopAutofillAddCreditCardCoordinator {
  [self.addCreditCardCoordinator stop];
  self.addCreditCardCoordinator.delegate = nil;
  self.addCreditCardCoordinator = nil;
}

// Function that is invoked when the reauth is finished, and handles the reauth
// result.
- (void)handleReauthenticationResult:(ReauthenticationResult)result {
  // Get the original value.
  BOOL mandatoryReauthEnabled = _personalDataManager->payments_data_manager()
                                    .IsPaymentMethodsMandatoryReauthEnabled();

  MandatoryReauthAuthenticationFlowEvent flow_event;
  if (result == ReauthenticationResult::kFailure) {
    // If authentication fails, restore the switch to its original state.
    [self setSwitchItemOn:mandatoryReauthEnabled
                 itemType:ItemTypeMandatoryReauthSwitch
        sectionIdentifier:SectionIdentifierMandatoryReauthSwitch];
    flow_event = MandatoryReauthAuthenticationFlowEvent::kFlowFailed;

  } else {
    // Upon success, update the mandatory reauth pref and the switch.
    _personalDataManager->payments_data_manager()
        .SetPaymentMethodsMandatoryReauthEnabled(!mandatoryReauthEnabled);
    [self setSwitchItemOn:!mandatoryReauthEnabled
                 itemType:ItemTypeMandatoryReauthSwitch
        sectionIdentifier:SectionIdentifierMandatoryReauthSwitch];
    flow_event = MandatoryReauthAuthenticationFlowEvent::kFlowSucceeded;
  }
  LogMandatoryReauthOptInOrOutUpdateEvent(
      MandatoryReauthOptInOrOutSource::kSettingsPage,
      /*opt_in=*/!mandatoryReauthEnabled, flow_event);
}

// Updates the Autofill Credit Card pref and the view controller's toolbar
// according to the provided `enabled` state.
- (void)updateAutofillCreditCardPrefAndToolbarForState:(BOOL)enabled {
  [self setAutofillCreditCardEnabled:enabled];
  self.addButtonInToolbar.enabled = [self isAutofillCreditCardEnabled];
}

#pragma mark - AutofillAddCreditCardCoordinatorDelegate

- (void)autofillAddCreditCardCoordinatorWantsToBeStopped:
    (AutofillAddCreditCardCoordinator*)coordinator {
  CHECK_EQ(coordinator, self.addCreditCardCoordinator);
  [self stopAutofillAddCreditCardCoordinator];
}

@end
