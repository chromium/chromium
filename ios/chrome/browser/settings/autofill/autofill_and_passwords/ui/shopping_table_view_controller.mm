// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/autofill/autofill_and_passwords/ui/shopping_table_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/check.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/net/model/crurl.h"
#import "ios/chrome/browser/settings/autofill/autofill_and_passwords/ui/autofill_ai_base_item_type.h"
#import "ios/chrome/browser/settings/autofill/autofill_and_passwords/ui/shopping_mutator.h"
#import "ios/chrome/browser/settings/ui_bundled/elements/enterprise_info_popover_view_controller.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_info_button_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_switch_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

// Section identifiers in the "Shopping" page table view.
enum SectionIdentifier {
  kToggleSection = kSectionIdentifierEnumZero,
  kOrderSection,
  kShipmentSection,
};

// Item types in the "Shopping" page.
enum ItemType {
  ItemTypeToggle = kAutofillAIBaseItemTypeEntity + 1,
  ItemTypeFooter,
};

}  // namespace

@interface ShoppingTableViewController () <PopoverLabelViewControllerDelegate>
@end

@implementation ShoppingTableViewController {
  NSArray<TableViewItem*>* _orders;
  NSArray<TableViewItem*>* _shipments;
  BOOL _settingsAreDismissed;
  BOOL _shoppingEnabled;
  BOOL _shoppingToggleEnabled;
  BOOL _shoppingToggleManaged;
}

- (instancetype)init {
  self = [super initWithStyle:ChromeTableViewStyle()];
  if (self) {
    _shoppingToggleEnabled = YES;
    _shoppingToggleManaged = NO;
  }
  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];
  self.title = l10n_util::GetNSString(IDS_AUTOFILL_SHOPPING_TITLE);
  [self loadModel];
}

- (void)didMoveToParentViewController:(UIViewController*)parent {
  [super didMoveToParentViewController:parent];
  if (!parent) {
    [self.delegate shoppingTableViewControllerDidRemove:self];
  }
}

- (void)loadModel {
  [super loadModel];

  TableViewModel* model = self.tableViewModel;
  [model addSectionWithIdentifier:kToggleSection];
  if (_shoppingToggleManaged) {
    TableViewInfoButtonItem* managedItem =
        [[TableViewInfoButtonItem alloc] initWithType:ItemTypeToggle];
    managedItem.text =
        l10n_util::GetNSString(IDS_AUTOFILL_SHOPPING_OPT_IN_TOGGLE_LABEL);
    // The status can only be off when the pref is managed.
    managedItem.statusText = l10n_util::GetNSString(IDS_IOS_SETTING_OFF);
    managedItem.accessibilityHint = l10n_util::GetNSString(
        IDS_IOS_TOGGLE_SETTING_MANAGED_ACCESSIBILITY_HINT);
    managedItem.target = self;
    managedItem.selector = @selector(didTapManagedUIInfoButton:);
    [model addItem:managedItem toSectionWithIdentifier:kToggleSection];
  } else {
    TableViewSwitchItem* toggleItem =
        [[TableViewSwitchItem alloc] initWithType:ItemTypeToggle];
    toggleItem.text =
        l10n_util::GetNSString(IDS_AUTOFILL_SHOPPING_OPT_IN_TOGGLE_LABEL);
    toggleItem.on = _shoppingToggleEnabled && _shoppingEnabled;
    toggleItem.enabled = _shoppingToggleEnabled;
    toggleItem.target = self;
    toggleItem.selector = @selector(shoppingToggleChanged:);
    [model addItem:toggleItem toSectionWithIdentifier:kToggleSection];
  }

  TableViewTextHeaderFooterItem* footer =
      [[TableViewTextHeaderFooterItem alloc] initWithType:ItemTypeFooter];
  footer.subtitle =
      l10n_util::GetNSString(IDS_AUTOFILL_SHOPPING_OPT_IN_TOGGLE_SUB_LABEL);
  [model setFooter:footer forSectionWithIdentifier:kToggleSection];

  if (_orders.count > 0) {
    [model addSectionWithIdentifier:kOrderSection];
    TableViewTextHeaderFooterItem* header =
        [[TableViewTextHeaderFooterItem alloc]
            initWithType:kAutofillAIBaseItemTypeHeader];
    header.text = l10n_util::GetNSString(IDS_AUTOFILL_AI_ORDERS_TITLE);
    [model setHeader:header forSectionWithIdentifier:kOrderSection];
    for (TableViewItem* item in _orders) {
      [model addItem:item toSectionWithIdentifier:kOrderSection];
    }
  }

  if (_shipments.count > 0) {
    [model addSectionWithIdentifier:kShipmentSection];
    TableViewTextHeaderFooterItem* header =
        [[TableViewTextHeaderFooterItem alloc]
            initWithType:kAutofillAIBaseItemTypeHeader];
    header.text = l10n_util::GetNSString(IDS_AUTOFILL_AI_SHIPMENTS_TITLE);
    [model setHeader:header forSectionWithIdentifier:kShipmentSection];
    for (TableViewItem* item in _shipments) {
      [model addItem:item toSectionWithIdentifier:kShipmentSection];
    }
  }
}

#pragma mark - ShoppingConsumer

- (void)setShoppingWithOrders:(NSArray<TableViewItem*>*)orders
                    shipments:(NSArray<TableViewItem*>*)shipments {
  _orders = orders;
  _shipments = shipments;
  if (self.isViewLoaded) {
    [self reloadData];
  }
}

- (void)setShoppingToggleState:(BOOL)on
                       enabled:(BOOL)enabled
                       managed:(BOOL)managed {
  if (_shoppingEnabled == on && _shoppingToggleEnabled == enabled &&
      _shoppingToggleManaged == managed) {
    return;
  }

  CHECK(enabled || !on);
  BOOL modelNeedsRebuild = (_shoppingToggleManaged != managed);
  _shoppingEnabled = on;
  _shoppingToggleEnabled = enabled;
  _shoppingToggleManaged = managed;

  if (self.isViewLoaded) {
    if (modelNeedsRebuild) {
      [self reloadData];
    } else {
      TableViewModel* model = self.tableViewModel;
      NSIndexPath* togglePath = [model indexPathForItemType:ItemTypeToggle
                                          sectionIdentifier:kToggleSection];
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
  }
}

- (void)shoppingToggleChanged:(UISwitch*)switchView {
  BOOL switchOn = [switchView isOn];
  _shoppingEnabled = switchOn;

  TableViewModel* model = self.tableViewModel;
  NSIndexPath* switchPath = [model indexPathForItemType:ItemTypeToggle
                                      sectionIdentifier:kToggleSection];
  if (switchPath) {
    TableViewSwitchItem* switchItem =
        base::apple::ObjCCastStrict<TableViewSwitchItem>(
            [model itemAtIndexPath:switchPath]);
    switchItem.on = switchOn;
  }

  [self.mutator didToggleShopping:switchOn];
}

#pragma mark - SettingsRootTableViewController

- (BOOL)shouldHideToolbar {
  return YES;
}

#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  [super tableView:tableView didSelectRowAtIndexPath:indexPath];

  TableViewItem* item = [self.tableViewModel itemAtIndexPath:indexPath];
  [self.mutator didSelectEntityItem:item];
  [self.tableView deselectRowAtIndexPath:indexPath animated:YES];
}

#pragma mark - SettingsControllerProtocol

- (void)reportDismissalUserAction {
  // no-op: This metric is not recorded for shopping entities.
}

- (void)reportBackUserAction {
  // no-op: This metric is not recorded for shopping entities.
}

- (void)settingsWillBeDismissed {
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
