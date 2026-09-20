// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/ui_bundled/bwg/ui/gemini_suggestions_view_controller.h"

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/net/model/crurl.h"
#import "ios/chrome/browser/settings/ui_bundled/bwg/coordinator/gemini_settings_mutator.h"
#import "ios/chrome/browser/settings/ui_bundled/bwg/utils/gemini_settings_metrics.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/legacy_table_view_cell.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_detail_icon_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_link_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_switch_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/content_configuration/table_view_cell_content_configuration.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/common/string_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "url/gurl.h"

namespace {

// Section identifiers in the Gemini Suggestions settings table view.
enum SectionIdentifier {
  kSectionIdentifierSuggestions = kSectionIdentifierEnumZero,
  kSectionIdentifierWhenUsed,
  kSectionIdentifierThingsToConsider,
};

// Item types in the Gemini Suggestions settings table view.
enum ItemType {
  kItemTypeSuggestions = kItemTypeEnumZero,
  kItemTypeSuggestionsFooter,
  kItemTypeWhenUsedHeader,
  kItemTypePageTitlesAndUrls,
  kItemTypeAdapt,
  kItemTypeThingsToConsiderHeader,
  kItemTypePrivateSpace,
  kItemTypeTurningOff,
};

// Table identifier.
NSString* const kGeminiSuggestionsViewTableIdentifier =
    @"GeminiSuggestionsViewTableIdentifier";

// Row identifiers.
NSString* const kShowSuggestionsCellId = @"ShowSuggestionsCellId";

// Helper to create a detail icon item for information sections.
TableViewDetailIconItem* DetailIconItem(NSInteger item_type,
                                        NSInteger string_id,
                                        UIImage* icon_image) {
  TableViewDetailIconItem* detail_item =
      [[TableViewDetailIconItem alloc] initWithType:item_type];
  detail_item.text = l10n_util::GetNSString(string_id);
  detail_item.textNumberOfLines = 0;
  detail_item.textFont =
      [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
  detail_item.textColor = [UIColor colorNamed:kTextSecondaryColor];
  detail_item.selectionStyle = UITableViewCellSelectionStyleNone;
  detail_item.iconImage = icon_image;
  detail_item.iconTintColor = [UIColor colorNamed:kTextPrimaryColor];
  return detail_item;
}

// Helper to create a section header item.
TableViewTextHeaderFooterItem* HeaderItem(NSInteger item_type,
                                          NSInteger string_id) {
  TableViewTextHeaderFooterItem* header =
      [[TableViewTextHeaderFooterItem alloc] initWithType:item_type];
  header.text = l10n_util::GetNSString(string_id);
  return header;
}

}  // namespace

// Custom detail icon item that supports custom NSAttributedString title.
@interface TableViewDetailIconItemWithAttributedTitle : TableViewDetailIconItem

// Attributed title to display.
@property(nonatomic, copy) NSAttributedString* attributedTitle;

@end

@implementation TableViewDetailIconItemWithAttributedTitle

- (void)configureCell:(LegacyTableViewCell*)cell {
  [super configureCell:cell];
  if (self.attributedTitle &&
      [cell.contentConfiguration
          isKindOfClass:[TableViewCellContentConfiguration class]]) {
    TableViewCellContentConfiguration* contentConfiguration =
        base::apple::ObjCCastStrict<TableViewCellContentConfiguration>(
            cell.contentConfiguration);
    contentConfiguration.attributedTitle = self.attributedTitle;
    cell.contentConfiguration = contentConfiguration;
  }
}

@end

@interface GeminiSuggestionsViewController () <
    TableViewLinkHeaderFooterItemDelegate>
@end

@implementation GeminiSuggestionsViewController {
  // Switch item for toggling suggestions.
  TableViewSwitchItem* _suggestionsSwitchItem;
  // Gemini suggestions preference value.
  BOOL _suggestionsEnabled;
}

- (void)viewDidLoad {
  [super viewDidLoad];
  self.tableView.accessibilityIdentifier =
      kGeminiSuggestionsViewTableIdentifier;
  self.title =
      l10n_util::GetNSString(IDS_IOS_GEMINI_SETTINGS_SUGGESTIONS_TITLE);
  [self loadModel];
}

- (void)loadModel {
  [super loadModel];
  TableViewModel* model = self.tableViewModel;

  // Suggestions Section (Toggle + Footer).
  [model addSectionWithIdentifier:kSectionIdentifierSuggestions];

  _suggestionsSwitchItem =
      [[TableViewSwitchItem alloc] initWithType:kItemTypeSuggestions];
  _suggestionsSwitchItem.text =
      l10n_util::GetNSString(IDS_IOS_GEMINI_SETTINGS_SHOW_SUGGESTIONS_TITLE);
  _suggestionsSwitchItem.on = _suggestionsEnabled;
  _suggestionsSwitchItem.accessibilityIdentifier = kShowSuggestionsCellId;
  _suggestionsSwitchItem.target = self;
  _suggestionsSwitchItem.selector = @selector(suggestionsSwitchToggled:);
  [model addItem:_suggestionsSwitchItem
      toSectionWithIdentifier:kSectionIdentifierSuggestions];

  TableViewLinkHeaderFooterItem* suggestionsFooterItem =
      [[TableViewLinkHeaderFooterItem alloc]
          initWithType:kItemTypeSuggestionsFooter];
  suggestionsFooterItem.text =
      l10n_util::GetNSString(IDS_IOS_GEMINI_SETTINGS_SUGGESTIONS_FOOTER_TEXT);
  NSMutableArray* urls = [[NSMutableArray alloc] init];
  [urls addObject:[[CrURL alloc]
                      initWithGURL:GURL(kGeminiPageContentSharingURL)]];
  suggestionsFooterItem.urls = urls;
  [model setFooter:suggestionsFooterItem
      forSectionWithIdentifier:kSectionIdentifierSuggestions];

  // 2. When Used Section.
  [model addSectionWithIdentifier:kSectionIdentifierWhenUsed];
  [model setHeader:HeaderItem(kItemTypeWhenUsedHeader,
                              IDS_IOS_GEMINI_SUGGESTIONS_WHEN_USED_HEADER)
      forSectionWithIdentifier:kSectionIdentifierWhenUsed];

  TableViewDetailIconItem* pageTitlesItem = DetailIconItem(
      kItemTypePageTitlesAndUrls,
      IDS_IOS_GEMINI_SUGGESTIONS_PAGE_TITLES_AND_URLS_DESCRIPTION,
      SymbolWithPointSize(SymbolChartBarXAxis,
                          kSettingsRootSymbolImagePointSize));
  [model addItem:pageTitlesItem
      toSectionWithIdentifier:kSectionIdentifierWhenUsed];

  TableViewDetailIconItem* adaptItem = DetailIconItem(
      kItemTypeAdapt, IDS_IOS_GEMINI_SUGGESTIONS_ADAPT_DESCRIPTION,
      SymbolWithPointSize(SymbolMagnifyingglass,
                          kSettingsRootSymbolImagePointSize));
  [model addItem:adaptItem toSectionWithIdentifier:kSectionIdentifierWhenUsed];

  // 3. Things To Consider Section.
  [model addSectionWithIdentifier:kSectionIdentifierThingsToConsider];
  [model setHeader:HeaderItem(
                       kItemTypeThingsToConsiderHeader,
                       IDS_IOS_GEMINI_SUGGESTIONS_THINGS_TO_CONSIDER_HEADER)
      forSectionWithIdentifier:kSectionIdentifierThingsToConsider];

  TableViewDetailIconItem* privateSpaceItem = DetailIconItem(
      kItemTypePrivateSpace,
      IDS_IOS_GEMINI_SUGGESTIONS_PRIVATE_SPACE_DESCRIPTION,
      SymbolWithPointSize(SymbolInfoCircle, kSettingsRootSymbolImagePointSize));
  [model addItem:privateSpaceItem
      toSectionWithIdentifier:kSectionIdentifierThingsToConsider];

  TableViewDetailIconItemWithAttributedTitle* turningOffItem =
      [[TableViewDetailIconItemWithAttributedTitle alloc]
          initWithType:kItemTypeTurningOff];
  turningOffItem.textNumberOfLines = 0;
  turningOffItem.selectionStyle = UITableViewCellSelectionStyleNone;
  turningOffItem.iconImage = SymbolWithPointSize(
      SymbolPersonCropCircle, kSettingsRootSymbolImagePointSize);
  turningOffItem.iconTintColor = [UIColor colorNamed:kTextPrimaryColor];

  NSDictionary* textAttributes = @{
    NSFontAttributeName :
        [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote],
    NSForegroundColorAttributeName : [UIColor colorNamed:kTextSecondaryColor]
  };
  NSDictionary* linkAttributes = @{
    NSFontAttributeName :
        [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote],
    NSForegroundColorAttributeName : [UIColor colorNamed:kBlueColor]
  };
  turningOffItem.attributedTitle = AttributedStringFromStringWithLink(
      l10n_util::GetNSString(
          IDS_IOS_GEMINI_SUGGESTIONS_TURNING_OFF_DESCRIPTION),
      textAttributes, linkAttributes);

  [model addItem:turningOffItem
      toSectionWithIdentifier:kSectionIdentifierThingsToConsider];
}

- (void)setSuggestionsEnabled:(BOOL)enabled {
  _suggestionsEnabled = enabled;
  if ([self isViewLoaded]) {
    _suggestionsSwitchItem.on = _suggestionsEnabled;
    [self reconfigureCellsForItems:@[ _suggestionsSwitchItem ]];
  }
}

#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView*)tableView
    performPrimaryActionForRowAtIndexPath:(NSIndexPath*)indexPath {
  if ([self.tableViewModel itemTypeForIndexPath:indexPath] ==
      kItemTypeTurningOff) {
    [self.mutator openSyncSettings];
  }
}

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  [super tableView:tableView didSelectRowAtIndexPath:indexPath];

  if ([self.tableViewModel itemTypeForIndexPath:indexPath] ==
      kItemTypeTurningOff) {
    [self.mutator openSyncSettings];
  }
}

- (UIView*)tableView:(UITableView*)tableView
    viewForFooterInSection:(NSInteger)section {
  UIView* footerView = [super tableView:tableView
                 viewForFooterInSection:section];
  TableViewLinkHeaderFooterView* footer =
      base::apple::ObjCCast<TableViewLinkHeaderFooterView>(footerView);
  footer.delegate = self;
  return footerView;
}

#pragma mark - TableViewLinkHeaderFooterItemDelegate

- (void)view:(TableViewLinkHeaderFooterView*)view didTapLinkURL:(CrURL*)URL {
  [self.mutator openNewTabWithURL:URL.gurl];
}

#pragma mark - Private

// Called from the Suggestions setting's UIControlEventValueChanged. Updates
// underlying suggestions pref.
- (void)suggestionsSwitchToggled:(UISwitch*)switchView {
  RecordGeminiSuggestionsSettingsToggled(switchView.isOn);
  [self.mutator setGeminiSuggestionsPref:switchView.isOn];
}

#pragma mark - SettingsControllerProtocol

- (void)reportDismissalUserAction {
  RecordGeminiSuggestionsSettingsClose();
}

- (void)reportBackUserAction {
  RecordGeminiSuggestionsSettingsBack();
}

@end
