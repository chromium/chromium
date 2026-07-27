// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/autofill/autofill_and_passwords/ui/identity_docs_table_view_controller.h"

#import <vector>

#import "base/apple/foundation_util.h"
#import "base/check.h"
#import "base/metrics/user_metrics.h"
#import "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/net/model/crurl.h"
#import "ios/chrome/browser/settings/autofill/autofill_ai/ui/autofill_ai_add_entities_menu_builder.h"
#import "ios/chrome/browser/settings/autofill/autofill_ai/ui/autofill_ai_entity_item.h"
#import "ios/chrome/browser/settings/autofill/autofill_and_passwords/ui/autofill_ai_base_item_type.h"
#import "ios/chrome/browser/settings/autofill/autofill_and_passwords/ui/autofill_ai_base_mutator.h"
#import "ios/chrome/browser/settings/autofill/autofill_and_passwords/ui/identity_docs_mutator.h"
#import "ios/chrome/browser/settings/autofill/utils/autofill_settings_ui_util.h"
#import "ios/chrome/browser/settings/ui_bundled/elements/enterprise_info_popover_view_controller.h"
#import "ios/chrome/browser/settings/ui_bundled/settings_root_table_view_controller+toolbar_add.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_info_button_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_link_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_switch_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "ui/strings/grit/ui_strings.h"

namespace {
enum class SectionIdentifier {
  kToggleSection = kSectionIdentifierEnumZero,
  kDriversLicensesSection,
  kNationalIdCardsSection,
  kPassportsSection,
};

enum class ItemType {
  kToggleItem = kAutofillAIBaseItemTypeEntity + 1,
  kFooterItem,
};

}  // namespace

@interface IdentityDocsTableViewController () <
    AutofillAIAddEntitiesMenuDelegate,
    PopoverLabelViewControllerDelegate>
@end

// View controller implementation for Identity Docs.
@implementation IdentityDocsTableViewController {
  NSArray<TableViewItem*>* _driversLicenses;
  NSArray<TableViewItem*>* _nationalIdCards;
  NSArray<TableViewItem*>* _passports;
  BOOL _settingsAreDismissed;
  std::vector<autofill::EntityType> _writableEntityTypes;
  UIBarButtonItem* _addButtonInToolbar;
  BOOL _hasLocalEntities;
  BOOL _identityDocsEnabled;
  BOOL _identityDocsToggleEnabled;
  BOOL _identityDocsToggleManaged;
}

- (instancetype)init {
  self = [super initWithStyle:ChromeTableViewStyle()];
  if (self) {
    _identityDocsToggleEnabled = YES;
    _identityDocsToggleManaged = NO;
  }
  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];
  self.tableView.allowsMultipleSelectionDuringEditing = YES;
  self.title = l10n_util::GetNSString(IDS_AUTOFILL_IDENTITY_DOCS_TITLE);
  [self updateUIForEditState];
  [self loadModel];
}

- (void)didMoveToParentViewController:(UIViewController*)parent {
  [super didMoveToParentViewController:parent];
  if (!parent) {
    [self.delegate identityDocsTableViewControllerDidRemove:self];
  }
}

- (void)loadModel {
  [super loadModel];

  TableViewModel* model = self.tableViewModel;

  [model addSectionWithIdentifier:static_cast<NSInteger>(
                                      SectionIdentifier::kToggleSection)];
  if (_identityDocsToggleManaged) {
    TableViewInfoButtonItem* managedItem = [[TableViewInfoButtonItem alloc]
        initWithType:static_cast<NSInteger>(ItemType::kToggleItem)];
    managedItem.text =
        l10n_util::GetNSString(IDS_AUTOFILL_IDENTITY_DOCS_OPT_IN_TOGGLE_LABEL);
    managedItem.statusText = l10n_util::GetNSString(IDS_IOS_SETTING_OFF);
    managedItem.accessibilityHint = l10n_util::GetNSString(
        IDS_IOS_TOGGLE_SETTING_MANAGED_ACCESSIBILITY_HINT);
    managedItem.target = self;
    managedItem.selector = @selector(didTapManagedUIInfoButton:);
    [model addItem:managedItem
        toSectionWithIdentifier:static_cast<NSInteger>(
                                    SectionIdentifier::kToggleSection)];
  } else {
    TableViewSwitchItem* toggleItem = [[TableViewSwitchItem alloc]
        initWithType:static_cast<NSInteger>(ItemType::kToggleItem)];
    toggleItem.text =
        l10n_util::GetNSString(IDS_AUTOFILL_IDENTITY_DOCS_OPT_IN_TOGGLE_LABEL);
    toggleItem.on = _identityDocsToggleEnabled && _identityDocsEnabled;
    toggleItem.enabled = _identityDocsToggleEnabled;
    toggleItem.target = self;
    toggleItem.selector = @selector(identityDocsToggleChanged:);
    [model addItem:toggleItem
        toSectionWithIdentifier:static_cast<NSInteger>(
                                    SectionIdentifier::kToggleSection)];
  }

  TableViewLinkHeaderFooterItem* footer = [[TableViewLinkHeaderFooterItem alloc]
      initWithType:static_cast<NSInteger>(ItemType::kFooterItem)];
  footer.text = l10n_util::GetNSString(
      IDS_AUTOFILL_IDENTITY_DOCS_OPT_IN_TOGGLE_SUB_LABEL);
  [model setFooter:footer
      forSectionWithIdentifier:static_cast<NSInteger>(
                                   SectionIdentifier::kToggleSection)];

  if (_driversLicenses.count > 0) {
    [model
        addSectionWithIdentifier:
            static_cast<NSInteger>(SectionIdentifier::kDriversLicensesSection)];
    TableViewTextHeaderFooterItem* header =
        [[TableViewTextHeaderFooterItem alloc]
            initWithType:kAutofillAIBaseItemTypeHeader];
    header.text =
        l10n_util::GetNSString(IDS_AUTOFILL_AI_DRIVERS_LICENSES_TITLE);
    [model setHeader:header
        forSectionWithIdentifier:
            static_cast<NSInteger>(SectionIdentifier::kDriversLicensesSection)];
    for (TableViewItem* item in _driversLicenses) {
      [model addItem:item
          toSectionWithIdentifier:
              static_cast<NSInteger>(
                  SectionIdentifier::kDriversLicensesSection)];
    }
  }

  if (_nationalIdCards.count > 0) {
    [model
        addSectionWithIdentifier:
            static_cast<NSInteger>(SectionIdentifier::kNationalIdCardsSection)];
    TableViewTextHeaderFooterItem* header =
        [[TableViewTextHeaderFooterItem alloc]
            initWithType:kAutofillAIBaseItemTypeHeader];
    header.text = l10n_util::GetNSString(IDS_AUTOFILL_AI_NATIONAL_IDS_TITLE);
    [model setHeader:header
        forSectionWithIdentifier:
            static_cast<NSInteger>(SectionIdentifier::kNationalIdCardsSection)];
    for (TableViewItem* item in _nationalIdCards) {
      [model addItem:item
          toSectionWithIdentifier:
              static_cast<NSInteger>(
                  SectionIdentifier::kNationalIdCardsSection)];
    }
  }

  if (_passports.count > 0) {
    [model addSectionWithIdentifier:static_cast<NSInteger>(
                                        SectionIdentifier::kPassportsSection)];
    TableViewTextHeaderFooterItem* header =
        [[TableViewTextHeaderFooterItem alloc]
            initWithType:kAutofillAIBaseItemTypeHeader];
    header.text = l10n_util::GetNSString(IDS_AUTOFILL_AI_PASSPORTS_TITLE);
    [model setHeader:header
        forSectionWithIdentifier:static_cast<NSInteger>(
                                     SectionIdentifier::kPassportsSection)];
    for (TableViewItem* item in _passports) {
      [model addItem:item
          toSectionWithIdentifier:static_cast<NSInteger>(
                                      SectionIdentifier::kPassportsSection)];
    }
  }
}

#pragma mark - IdentityDocsConsumer

- (void)
    setIdentityDocsWithDriversLicenses:(NSArray<TableViewItem*>*)driversLicenses
                       nationalIdCards:(NSArray<TableViewItem*>*)nationalIdCards
                             passports:(NSArray<TableViewItem*>*)passports {
  _driversLicenses = driversLicenses;
  _nationalIdCards = nationalIdCards;
  _passports = passports;
  _hasLocalEntities = [self checkForLocalEntities];
  if (self.isViewLoaded) {
    if (![self hasLocalEntities] && self.tableView.isEditing) {
      [self setEditing:NO animated:YES];
    }
    [self updateUIForEditState];
    [self reloadData];
  }
}

- (void)setIdentityDocsToggleState:(BOOL)on
                           enabled:(BOOL)enabled
                           managed:(BOOL)managed {
  if (_identityDocsEnabled == on && _identityDocsToggleEnabled == enabled &&
      _identityDocsToggleManaged == managed) {
    return;
  }

  BOOL modelNeedsRebuild = (_identityDocsToggleManaged != managed);
  _identityDocsEnabled = on;
  _identityDocsToggleEnabled = enabled;
  _identityDocsToggleManaged = managed;

  if (self.isViewLoaded) {
    if (modelNeedsRebuild) {
      [self reloadData];
    } else {
      TableViewModel* model = self.tableViewModel;
      NSIndexPath* togglePath = [model
          indexPathForItemType:static_cast<NSInteger>(ItemType::kToggleItem)
             sectionIdentifier:static_cast<NSInteger>(
                                   SectionIdentifier::kToggleSection)];
      if (togglePath) {
        if (managed) {
          TableViewInfoButtonItem* managedItem =
              base::apple::ObjCCastStrict<TableViewInfoButtonItem>(
                  [model itemAtIndexPath:togglePath]);
          [self reconfigureCellsForItems:@[ managedItem ]];
        } else {
          TableViewSwitchItem* switchItem =
              base::apple::ObjCCastStrict<TableViewSwitchItem>(
                  [model itemAtIndexPath:togglePath]);
          switchItem.enabled = enabled;
          switchItem.on = enabled ? on : NO;
          [self reconfigureCellsForItems:@[ switchItem ]];
        }
      }
    }
    [self updateAddButtonInToolbar];
  }
}

- (void)identityDocsToggleChanged:(UISwitch*)switchView {
  BOOL switchOn = [switchView isOn];
  _identityDocsEnabled = switchOn;

  TableViewModel* model = self.tableViewModel;
  NSIndexPath* switchPath =
      [model indexPathForItemType:static_cast<NSInteger>(ItemType::kToggleItem)
                sectionIdentifier:static_cast<NSInteger>(
                                      SectionIdentifier::kToggleSection)];
  if (switchPath) {
    TableViewSwitchItem* switchItem =
        base::apple::ObjCCastStrict<TableViewSwitchItem>(
            [model itemAtIndexPath:switchPath]);
    switchItem.on = switchOn;
  }

  [self updateAddButtonInToolbar];
  [self.mutator didToggleIdentityDocs:switchOn];
}

- (void)setWritableEntityTypes:
    (const std::vector<autofill::EntityType>&)writableEntityTypes {
  _writableEntityTypes = writableEntityTypes;
  if (self.isViewLoaded) {
    [self updateAddButtonInToolbar];
  }
}

#pragma mark - SettingsRootTableViewController

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];
  self.navigationController.toolbarHidden = NO;
}

- (void)updateUIForEditState {
  [super updateUIForEditState];
  [self updatedToolbarForEditState];

  for (NSIndexPath* indexPath in self.tableView.indexPathsForVisibleRows) {
    if ([self.tableViewModel itemTypeForIndexPath:indexPath] !=
        kAutofillAIBaseItemTypeEntity) {
      continue;
    }
    UITableViewCell* cell = [self.tableView cellForRowAtIndexPath:indexPath];
    if (cell) {
      [self updateOpacityForCell:cell atIndexPath:indexPath];
    }
  }
}

- (BOOL)shouldHideToolbar {
  return self.navigationController.visibleViewController != self &&
         self.navigationController.topViewController != self;
}

- (BOOL)shouldShowEditDoneButton {
  return NO;
}

- (UIBarButtonItem*)customLeftToolbarButton {
  if (self.tableView.isEditing) {
    return nil;
  }
  return self.addButtonInToolbar;
}

#pragma mark - Toolbar Add Button

- (UIBarButtonItem*)addButtonInToolbar {
  if (!_addButtonInToolbar) {
    _addButtonInToolbar = [self addButtonWithAction:nil];
    [self updateAddButtonInToolbar];
  }
  return _addButtonInToolbar;
}

- (void)updateAddButtonInToolbar {
  if (!_addButtonInToolbar) {
    return;
  }
  _addButtonInToolbar.action = nil;
  _addButtonInToolbar.target = nil;
  _addButtonInToolbar.menu = [AutofillAIAddEntitiesMenuBuilder
      buildMenuWithTypes:_writableEntityTypes
          profileEnabled:NO
         entitiesEnabled:_identityDocsEnabled && _identityDocsToggleEnabled &&
                         !_identityDocsToggleManaged
                delegate:self];
  _addButtonInToolbar.enabled =
      _identityDocsEnabled && _identityDocsToggleEnabled &&
      !_identityDocsToggleManaged && !_writableEntityTypes.empty();
}

#pragma mark - AutofillAIAddEntitiesMenuDelegate

- (void)didSelectAddAutofillProfile {
  NOTREACHED();
}

- (void)didSelectAddEntityWithType:(autofill::EntityType)type {
  [self.mutator didSelectAddEntityWithType:type];
}

- (BOOL)editButtonEnabled {
  return [self hasLocalEntities];
}

- (BOOL)hasLocalEntities {
  if (_settingsAreDismissed) {
    return NO;
  }
  return _hasLocalEntities;
}

// Helper method to check if there are any local entities.
- (BOOL)checkForLocalEntities {
  return ContainsLocalEntity(_driversLicenses) ||
         ContainsLocalEntity(_nationalIdCards) ||
         ContainsLocalEntity(_passports);
}

// Override.
- (void)deleteItems:(NSArray<NSIndexPath*>*)indexPaths {
  NSMutableArray<TableViewItem*>* items =
      [NSMutableArray arrayWithCapacity:indexPaths.count];
  for (NSIndexPath* indexPath in indexPaths) {
    TableViewItem* item = [self.tableViewModel itemAtIndexPath:indexPath];
    if (item) {
      [items addObject:item];
    }
  }

  NSString* deletionConfirmationString =
      GetDeletionConfirmationStringWithEntities(false, std::u16string());

  UIAlertControllerStyle preferredStyle = UIAlertControllerStyleActionSheet;
  if (UIContentSizeCategoryIsAccessibilityCategory(
          UIApplication.sharedApplication.preferredContentSizeCategory)) {
    preferredStyle = UIAlertControllerStyleAlert;
  }

  UIAlertController* alertController =
      [UIAlertController alertControllerWithTitle:deletionConfirmationString
                                          message:nil
                                   preferredStyle:preferredStyle];

  alertController.popoverPresentationController.barButtonItem =
      self.deleteButton;
  alertController.popoverPresentationController.permittedArrowDirections =
      UIPopoverArrowDirectionAny;

  __weak __typeof(self) weakSelf = self;

  UIAlertAction* deleteAction = [UIAlertAction
      actionWithTitle:l10n_util::GetNSString(IDS_IOS_DELETE_ACTION_TITLE)
                style:UIAlertActionStyleDestructive
              handler:^(UIAlertAction* action) {
                [weakSelf confirmDeleteItems:items];
              }];

  UIAlertAction* cancelAction =
      [UIAlertAction actionWithTitle:l10n_util::GetNSString(IDS_APP_CANCEL)
                               style:UIAlertActionStyleCancel
                             handler:^(UIAlertAction* action) {
                               [weakSelf cancelDeleteItems];
                             }];

  [alertController addAction:deleteAction];
  [alertController addAction:cancelAction];

  [self presentViewController:alertController animated:YES completion:nil];
}

#pragma mark - UITableViewDelegate

- (BOOL)tableView:(UITableView*)tableView
    shouldIndentWhileEditingRowAtIndexPath:(NSIndexPath*)indexPath {
  return ![self isServerWalletItemAtIndexPath:indexPath];
}

- (NSIndexPath*)tableView:(UITableView*)tableView
    willSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  if (self.tableView.editing &&
      [self isServerWalletItemAtIndexPath:indexPath]) {
    return nil;
  }
  return [super tableView:tableView willSelectRowAtIndexPath:indexPath];
}

- (void)setEditing:(BOOL)editing animated:(BOOL)animated {
  [super setEditing:editing animated:animated];
  if (_settingsAreDismissed) {
    return;
  }

  [self updateUIForEditState];
}

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  [super tableView:tableView didSelectRowAtIndexPath:indexPath];
  if (_settingsAreDismissed) {
    return;
  }

  if (self.tableView.isEditing) {
    self.deleteButton.enabled = YES;
    return;
  }

  [tableView deselectRowAtIndexPath:indexPath animated:YES];

  TableViewItem* item = [self.tableViewModel itemAtIndexPath:indexPath];
  [self.mutator didSelectEntityItem:item];
}

- (void)tableView:(UITableView*)tableView
    didDeselectRowAtIndexPath:(NSIndexPath*)indexPath {
  [super tableView:tableView didDeselectRowAtIndexPath:indexPath];
  if (_settingsAreDismissed || !self.tableView.editing) {
    return;
  }

  if (self.tableView.indexPathsForSelectedRows.count == 0) {
    self.deleteButton.enabled = NO;
  }
}

#pragma mark - UITableViewDataSource

- (BOOL)tableView:(UITableView*)tableView
    canEditRowAtIndexPath:(NSIndexPath*)indexPath {
  if (_settingsAreDismissed || [self isServerWalletItemAtIndexPath:indexPath]) {
    return NO;
  }

  return [self.tableViewModel itemTypeForIndexPath:indexPath] ==
         kAutofillAIBaseItemTypeEntity;
}

- (void)tableView:(UITableView*)tableView
    commitEditingStyle:(UITableViewCellEditingStyle)editingStyle
     forRowAtIndexPath:(NSIndexPath*)indexPath {
  if (editingStyle != UITableViewCellEditingStyleDelete ||
      _settingsAreDismissed) {
    return;
  }
  [self deleteItems:@[ indexPath ]];
}

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  UITableViewCell* cell = [super tableView:tableView
                     cellForRowAtIndexPath:indexPath];
  UIColor* selectedColor = [UIColor colorNamed:kTertiaryBackgroundColor];
  if (![cell.selectedBackgroundView.backgroundColor isEqual:selectedColor]) {
    UIView* selectedBackgroundView = [[UIView alloc] init];
    selectedBackgroundView.backgroundColor = selectedColor;
    cell.selectedBackgroundView = selectedBackgroundView;
  }
  [self updateOpacityForCell:cell atIndexPath:indexPath];

  return cell;
}

#pragma mark - Private

// Confirms the deletion of the given `items` by resetting the swipe-to-delete
// state if needed and informing the mutator.
- (void)confirmDeleteItems:(NSArray<TableViewItem*>*)items {
  if (!self.isEditing && self.tableView.isEditing) {
    [self.tableView setEditing:NO animated:YES];
  }
  [self.mutator didSelectDeleteEntityItems:items];
}

// Cancels the deletion by resetting the swipe-to-delete state if needed.
- (void)cancelDeleteItems {
  if (!self.isEditing && self.tableView.isEditing) {
    [self.tableView setEditing:NO animated:YES];
  }
}

// Returns YES if the item at the given `indexPath` represents a server-side
// Wallet entity.
- (BOOL)isServerWalletItemAtIndexPath:(NSIndexPath*)indexPath {
  if ([self.tableViewModel itemTypeForIndexPath:indexPath] ==
      kAutofillAIBaseItemTypeEntity) {
    TableViewItem* item = [self.tableViewModel itemAtIndexPath:indexPath];
    AutofillAIEntityItem* aiItem =
        base::apple::ObjCCastStrict<AutofillAIEntityItem>(item);
    return aiItem.isServerWalletItem;
  }
  return NO;
}

// Updates the opacity of the given `cell` based on whether it represents a
// server wallet entity and the table's editing state.
- (void)updateOpacityForCell:(UITableViewCell*)cell
                 atIndexPath:(NSIndexPath*)indexPath {
  BOOL shouldDisable =
      [self isServerWalletItemAtIndexPath:indexPath] && self.tableView.editing;

  cell.contentView.alpha = shouldDisable ? 0.5 : 1.0;
}

#pragma mark - SettingsControllerProtocol

- (void)reportDismissalUserAction {
  base::RecordAction(
      base::UserMetricsAction("MobileIdentityDocsSettingsClose"));
}

- (void)reportBackUserAction {
  base::RecordAction(base::UserMetricsAction("MobileIdentityDocsSettingsBack"));
}

- (void)settingsWillBeDismissed {
  DCHECK(!_settingsAreDismissed);
  _settingsAreDismissed = YES;
}

// Called when the user clicks on the information button of the managed
// setting's UI. Shows a textual bubble with the information of the enterprise.
- (void)didTapManagedUIInfoButton:(UIButton*)buttonView {
  if (_settingsAreDismissed) {
    return;
  }

  EnterpriseInfoPopoverViewController* bubbleViewController =
      [[EnterpriseInfoPopoverViewController alloc] initWithEnterpriseName:nil];
  bubbleViewController.delegate = self;

  // Set the anchor and arrow direction of the bubble.
  bubbleViewController.popoverPresentationController.sourceView = buttonView;
  bubbleViewController.popoverPresentationController.sourceRect =
      buttonView.bounds;
  bubbleViewController.popoverPresentationController.permittedArrowDirections =
      UIPopoverArrowDirectionAny;

  [self presentViewController:bubbleViewController animated:YES completion:nil];
}

#pragma mark - PopoverLabelViewControllerDelegate

- (void)didTapLinkURL:(NSURL*)URL {
  [self view:nil didTapLinkURL:[[CrURL alloc] initWithNSURL:URL]];
}

@end
