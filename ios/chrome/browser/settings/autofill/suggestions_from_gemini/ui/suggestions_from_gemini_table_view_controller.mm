// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/autofill/suggestions_from_gemini/ui/suggestions_from_gemini_table_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/notreached.h"
#import "ios/chrome/browser/settings/autofill/suggestions_from_gemini/ui/suggestions_from_gemini_mutator.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_detail_text_item.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierSuggestionsFromGemini = kSectionIdentifierEnumZero,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeManageConnectedApps = kItemTypeEnumZero,
};

}  // namespace

@implementation SuggestionsFromGeminiTableViewController {
  BOOL _settingsAreDismissed;
}

- (instancetype)init {
  self = [super initWithStyle:ChromeTableViewStyle()];
  if (self) {
    self.title = l10n_util::GetNSString(
        IDS_IOS_PERSONAL_CONTEXT_AUTOFILL_SETTINGS_TITLE);
  }
  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];
  [self loadModel];
}

- (void)loadModel {
  [super loadModel];
  if (_settingsAreDismissed) {
    return;
  }

  TableViewModel* model = self.tableViewModel;
  [model addSectionWithIdentifier:SectionIdentifierSuggestionsFromGemini];

  TableViewDetailTextItem* item = [[TableViewDetailTextItem alloc]
      initWithType:ItemTypeManageConnectedApps];
  item.text = l10n_util::GetNSString(
      IDS_IOS_PERSONAL_CONTEXT_AUTOFILL_SETTINGS_MANAGE_CONNECTED_APPS_TITLE);
  item.detailText = l10n_util::GetNSString(
      IDS_IOS_PERSONAL_CONTEXT_AUTOFILL_SETTINGS_MANAGE_CONNECTED_APPS_SUMMARY);
  item.accessorySymbol = TableViewDetailTextCellAccessorySymbolExternalLink;
  item.accessibilityTraits |= UIAccessibilityTraitLink;

  [model addItem:item
      toSectionWithIdentifier:SectionIdentifierSuggestionsFromGemini];
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
      break;
    default:
      NOTREACHED();
  }
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

@end
