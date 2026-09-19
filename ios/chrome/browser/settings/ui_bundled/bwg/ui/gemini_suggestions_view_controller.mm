// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/ui_bundled/bwg/ui/gemini_suggestions_view_controller.h"

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/net/model/crurl.h"
#import "ios/chrome/browser/settings/ui_bundled/bwg/coordinator/gemini_settings_mutator.h"
#import "ios/chrome/browser/settings/ui_bundled/bwg/utils/gemini_settings_metrics.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_link_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_switch_item.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "url/gurl.h"

namespace {

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  kSectionIdentifierSuggestions = kSectionIdentifierEnumZero,
};

typedef NS_ENUM(NSInteger, ItemType) {
  kItemTypeSuggestions = kItemTypeEnumZero,
  kItemTypeSuggestionsFooter,
};

// Suggestions View Table identifier.
NSString* const kGeminiSuggestionsViewTableIdentifier =
    @"GeminiSuggestionsViewTableIdentifier";

// Row identifiers.
NSString* const kShowSuggestionsCellId = @"ShowSuggestionsCellId";

}  // namespace

@interface GeminiSuggestionsViewController () <
    TableViewLinkHeaderFooterItemDelegate>
@end

@implementation GeminiSuggestionsViewController {
  // Switch item for toggling suggestions.
  TableViewSwitchItem* _suggestionsSwitchItem;
  // Gemini suggestions preference value.
  BOOL _suggestionsEnabled;
}

#pragma mark - ChromeTableViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  self.title =
      l10n_util::GetNSString(IDS_IOS_GEMINI_SETTINGS_SUGGESTIONS_TITLE);
  self.tableView.accessibilityIdentifier =
      kGeminiSuggestionsViewTableIdentifier;

  [self loadModel];
}

- (void)loadModel {
  [super loadModel];

  TableViewModel* model = self.tableViewModel;

  // Suggestions toggle section.
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
}

- (void)setSuggestionsEnabled:(BOOL)enabled {
  _suggestionsEnabled = enabled;
  if ([self isViewLoaded]) {
    _suggestionsSwitchItem.on = _suggestionsEnabled;
    [self reconfigureCellsForItems:@[ _suggestionsSwitchItem ]];
  }
}

#pragma mark - UITableViewDelegate

- (UIView*)tableView:(UITableView*)tableView
    viewForFooterInSection:(NSInteger)section {
  UIView* footerView = [super tableView:tableView
                 viewForFooterInSection:section];
  if (section ==
      [self.tableViewModel
          sectionForSectionIdentifier:kSectionIdentifierSuggestions]) {
    TableViewLinkHeaderFooterView* footer =
        base::apple::ObjCCastStrict<TableViewLinkHeaderFooterView>(footerView);
    footer.delegate = self;
  }
  return footerView;
}

#pragma mark - TableViewLinkHeaderFooterItemDelegate

- (void)view:(TableViewLinkHeaderFooterView*)view didTapLinkURL:(CrURL*)URL {
  [self.mutator openNewTabWithURL:URL.gurl];
}

#pragma mark - SettingsControllerProtocol

- (void)reportDismissalUserAction {
  RecordGeminiSuggestionsSettingsClose();
}

- (void)reportBackUserAction {
  RecordGeminiSuggestionsSettingsBack();
}

#pragma mark - Private

- (void)suggestionsSwitchToggled:(UISwitch*)switchView {
  RecordGeminiSuggestionsSettingsToggled(switchView.isOn);
  [self.mutator setGeminiSuggestionsPref:switchView.isOn];
}

@end
