// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/autofill/suggestions_from_gemini/ui/suggestions_from_gemini_table_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/notreached.h"
#import "ios/chrome/browser/settings/autofill/suggestions_from_gemini/ui/suggestions_from_gemini_mutator.h"
#import "ios/chrome/browser/settings/ui_bundled/autofill/autofill_settings_constants.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_detail_text_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_switch_item.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierSuggestionsFromGemini = kSectionIdentifierEnumZero,
  SectionIdentifierHelpImprove,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeFindAndFillSwitch = kItemTypeEnumZero,
  ItemTypeManageConnectedApps,
  ItemTypeHelpImprove,
};

}  // namespace

@implementation SuggestionsFromGeminiTableViewController {
  BOOL _settingsAreDismissed;
  // Tracks whether the Suggestions from Gemini setting switch is toggled on.
  BOOL _suggestionsFromGeminiSwitchOn;
}

- (instancetype)init {
  self = [super initWithStyle:ChromeTableViewStyle()];
  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];
  self.title =
      l10n_util::GetNSString(IDS_IOS_PERSONAL_CONTEXT_AUTOFILL_SETTINGS_TITLE);
  [self loadModel];
}

- (void)loadModel {
  [super loadModel];
  if (_settingsAreDismissed) {
    return;
  }

  TableViewModel* model = self.tableViewModel;
  [model addSectionWithIdentifier:SectionIdentifierSuggestionsFromGemini];

  [model addItem:[self findAndFillSwitchItem]
      toSectionWithIdentifier:SectionIdentifierSuggestionsFromGemini];

  [model addItem:[self manageConnectedAppsItem]
      toSectionWithIdentifier:SectionIdentifierSuggestionsFromGemini];

  [model addSectionWithIdentifier:SectionIdentifierHelpImprove];

  [model addItem:[self helpImproveItem]
      toSectionWithIdentifier:SectionIdentifierHelpImprove];
}

#pragma mark - SuggestionsFromGeminiConsumer

- (void)setSuggestionsFromGeminiSwitchOn:(BOOL)on {
  if (_suggestionsFromGeminiSwitchOn == on) {
    return;
  }

  _suggestionsFromGeminiSwitchOn = on;

  [self updateSwitchItemState:on];
  if (self.isViewLoaded) {
    TableViewModel* model = self.tableViewModel;
    if ([model hasItemForItemType:ItemTypeFindAndFillSwitch
                sectionIdentifier:SectionIdentifierSuggestionsFromGemini]) {
      NSIndexPath* indexPath =
          [model indexPathForItemType:ItemTypeFindAndFillSwitch
                    sectionIdentifier:SectionIdentifierSuggestionsFromGemini];
      [self.tableView reloadRowsAtIndexPaths:@[ indexPath ]
                            withRowAnimation:UITableViewRowAnimationNone];
    }
  }
}

#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  if (_settingsAreDismissed) {
    return;
  }

  [super tableView:tableView didSelectRowAtIndexPath:indexPath];

  NSInteger itemType = [self.tableViewModel itemTypeForIndexPath:indexPath];
  switch (itemType) {
    case ItemTypeManageConnectedApps:
      [self.tableView deselectRowAtIndexPath:indexPath animated:YES];
      [self.mutator didSelectManageConnectedApps];
      return;
    case ItemTypeFindAndFillSwitch:
      [self.tableView deselectRowAtIndexPath:indexPath animated:NO];
      return;
    case ItemTypeHelpImprove:
      [self.tableView deselectRowAtIndexPath:indexPath animated:YES];
      [self.mutator didSelectHelpImprove];
      return;
  }
  NOTREACHED();
}

#pragma mark - SettingsControllerProtocol

- (void)reportDismissalUserAction {
  // TODO(crbug.com/539811785): Implement navigation metrics.
}

- (void)reportBackUserAction {
  // TODO(crbug.com/539811785): Implement navigation metrics.
}

- (void)settingsWillBeDismissed {
  _settingsAreDismissed = YES;
}

#pragma mark - Switch Callbacks

// Callback invoked when the Suggestions from Gemini setting switch is toggled.
- (void)personalContextSwitchChanged:(UISwitch*)switchView {
  _suggestionsFromGeminiSwitchOn = switchView.isOn;

  [self updateSwitchItemState:switchView.isOn];
  [self.mutator didToggleSuggestionsFromGeminiSwitch:switchView.isOn];
}

#pragma mark - Private

// Updates the switch item's state in the table view model if the view is
// loaded.
- (void)updateSwitchItemState:(BOOL)on {
  if (!self.isViewLoaded) {
    return;
  }

  TableViewModel* model = self.tableViewModel;
  if ([model hasItemForItemType:ItemTypeFindAndFillSwitch
              sectionIdentifier:SectionIdentifierSuggestionsFromGemini]) {
    NSIndexPath* indexPath =
        [model indexPathForItemType:ItemTypeFindAndFillSwitch
                  sectionIdentifier:SectionIdentifierSuggestionsFromGemini];
    TableViewSwitchItem* switchItem =
        base::apple::ObjCCastStrict<TableViewSwitchItem>(
            [model itemAtIndexPath:indexPath]);
    switchItem.on = on;
  }
}

// Returns a configured switch item for the "Suggestions from Gemini" setting.
- (TableViewSwitchItem*)findAndFillSwitchItem {
  TableViewSwitchItem* switchItem =
      [[TableViewSwitchItem alloc] initWithType:ItemTypeFindAndFillSwitch];
  switchItem.text = l10n_util::GetNSString(
      IDS_IOS_PERSONAL_CONTEXT_AUTOFILL_SETTINGS_SWITCH_TITLE);
  switchItem.detailText = l10n_util::GetNSString(
      IDS_IOS_PERSONAL_CONTEXT_AUTOFILL_SETTINGS_SWITCH_SUMMARY);
  switchItem.on = _suggestionsFromGeminiSwitchOn;
  switchItem.target = self;
  switchItem.selector = @selector(personalContextSwitchChanged:);
  switchItem.accessibilityIdentifier = kSuggestionsFromGeminiSwitchViewId;
  return switchItem;
}

// Returns a configured detail text item for the "Manage connected apps" link.
- (TableViewDetailTextItem*)manageConnectedAppsItem {
  TableViewDetailTextItem* item = [[TableViewDetailTextItem alloc]
      initWithType:ItemTypeManageConnectedApps];
  item.text = l10n_util::GetNSString(
      IDS_IOS_PERSONAL_CONTEXT_AUTOFILL_SETTINGS_MANAGE_CONNECTED_APPS_TITLE);
  item.detailText = l10n_util::GetNSString(
      IDS_IOS_PERSONAL_CONTEXT_AUTOFILL_SETTINGS_MANAGE_CONNECTED_APPS_SUMMARY);
  item.accessorySymbol = TableViewDetailTextCellAccessorySymbolExternalLink;
  item.accessibilityTraits |= UIAccessibilityTraitLink;
  return item;
}

// Returns a configured detail text item for the "Help improve enhanced
// autofill" subpage row.
- (TableViewDetailTextItem*)helpImproveItem {
  TableViewDetailTextItem* helpImproveItem =
      [[TableViewDetailTextItem alloc] initWithType:ItemTypeHelpImprove];
  helpImproveItem.text = l10n_util::GetNSString(
      IDS_IOS_PERSONAL_CONTEXT_AUTOFILL_SETTINGS_HELPING_IMPROVE_NOTICE_TITLE);
  helpImproveItem.accessoryType = UITableViewCellAccessoryDisclosureIndicator;
  helpImproveItem.accessibilityTraits |= UIAccessibilityTraitButton;
  return helpImproveItem;
}

@end
