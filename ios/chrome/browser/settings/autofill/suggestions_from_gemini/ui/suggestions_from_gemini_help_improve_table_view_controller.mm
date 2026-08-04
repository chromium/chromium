// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/autofill/suggestions_from_gemini/ui/suggestions_from_gemini_help_improve_table_view_controller.h"

#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_detail_icon_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_item.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierHelpingImprove = kSectionIdentifierEnumZero,
  SectionIdentifierWhenUsed,
  SectionIdentifierThingsToConsider,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeHeader = kItemTypeEnumZero,
  ItemTypeFooter,
  ItemTypeLabel,
  ItemTypeDetailIcon,
};

// Creates and returns a configured TableViewTextHeaderFooterItem.
TableViewTextHeaderFooterItem* CreateHeaderItem(int messageId) {
  TableViewTextHeaderFooterItem* header =
      [[TableViewTextHeaderFooterItem alloc] initWithType:ItemTypeHeader];
  header.text = l10n_util::GetNSString(messageId);
  return header;
}

// Creates and returns a configured TableViewDetailIconItem for the help
// improve subpage.
TableViewDetailIconItem* HelpImproveDetailItem(NSInteger titleId,
                                               Symbol symbol) {
  TableViewDetailIconItem* detailItem =
      [[TableViewDetailIconItem alloc] initWithType:ItemTypeDetailIcon];
  detailItem.text = l10n_util::GetNSString(titleId);
  detailItem.textNumberOfLines = 0;
  detailItem.textFont =
      [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
  detailItem.textColor = [UIColor colorNamed:kTextSecondaryColor];
  detailItem.selectionStyle = UITableViewCellSelectionStyleNone;
  detailItem.iconImage =
      SymbolWithPointSize(symbol, kSettingsRootSymbolImagePointSize);
  detailItem.iconTintColor = [UIColor colorNamed:kTextPrimaryColor];
  return detailItem;
}

}  // namespace

@implementation SuggestionsFromGeminiHelpImproveTableViewController {
  BOOL _settingsAreDismissed;
}

- (instancetype)init {
  self = [super initWithStyle:ChromeTableViewStyle()];
  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];
  self.title = l10n_util::GetNSString(
      IDS_IOS_PERSONAL_CONTEXT_AUTOFILL_SETTINGS_HELPING_IMPROVE_NOTICE_TITLE);
  [self loadModel];
}

- (void)loadModel {
  [super loadModel];
  if (_settingsAreDismissed) {
    return;
  }

  TableViewModel* model = self.tableViewModel;

  // Helping improve section.
  [model addSectionWithIdentifier:SectionIdentifierHelpingImprove];

  TableViewTextItem* helpingImproveItem =
      [[TableViewTextItem alloc] initWithType:ItemTypeLabel];
  helpingImproveItem.text = l10n_util::GetNSString(
      IDS_IOS_PERSONAL_CONTEXT_AUTOFILL_SETTINGS_HELPING_IMPROVE_TITLE);
  helpingImproveItem.selectionStyle = UITableViewCellSelectionStyleNone;
  [model addItem:helpingImproveItem
      toSectionWithIdentifier:SectionIdentifierHelpingImprove];

  TableViewTextHeaderFooterItem* helpingImproveFooter =
      [[TableViewTextHeaderFooterItem alloc] initWithType:ItemTypeFooter];
  helpingImproveFooter.text = l10n_util::GetNSString(
      IDS_SETTINGS_SUGGESTIONS_FROM_GEMINI_QUALITY_LOGGING_SUBTITLE);
  [model setFooter:helpingImproveFooter
      forSectionWithIdentifier:SectionIdentifierHelpingImprove];

  // When used section.
  [model addSectionWithIdentifier:SectionIdentifierWhenUsed];
  [model setHeader:
             CreateHeaderItem(
                 IDS_IOS_PERSONAL_CONTEXT_AUTOFILL_SETTINGS_WHEN_USED_TITLE)
      forSectionWithIdentifier:SectionIdentifierWhenUsed];

  [model addItem:HelpImproveDetailItem(
                     IDS_SETTINGS_SUGGESTIONS_FROM_GEMINI_WHEN_USED_1,
                     SymbolChartBarXAxis)
      toSectionWithIdentifier:SectionIdentifierWhenUsed];

  [model addItem:HelpImproveDetailItem(
                     IDS_SETTINGS_SUGGESTIONS_FROM_GEMINI_WHEN_USED_2,
                     SymbolMagnifyingglassSpark)
      toSectionWithIdentifier:SectionIdentifierWhenUsed];

  // Things to consider section.
  [model addSectionWithIdentifier:SectionIdentifierThingsToConsider];
  [model setHeader:
             CreateHeaderItem(
                 IDS_IOS_PERSONAL_CONTEXT_AUTOFILL_SETTINGS_THINGS_TO_CONSIDER_TITLE)
      forSectionWithIdentifier:SectionIdentifierThingsToConsider];

  [model addItem:HelpImproveDetailItem(
                     IDS_SETTINGS_SUGGESTIONS_FROM_GEMINI_CONSIDER_1,
                     SymbolLinkAction)
      toSectionWithIdentifier:SectionIdentifierThingsToConsider];

  [model addItem:HelpImproveDetailItem(
                     IDS_SETTINGS_SUGGESTIONS_FROM_GEMINI_CONSIDER_2,
                     SymbolPersonCropCircle)
      toSectionWithIdentifier:SectionIdentifierThingsToConsider];

  // TODO(crbug.com/541220712): Add the enterprise item based on the correct
  // policy.
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
