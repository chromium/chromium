// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/ui_bundled/clear_browsing_data/quick_delete_other_data/ui/quick_delete_other_data_view_controller.h"

#import "components/browsing_data/core/browsing_data_utils.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/settings/ui_bundled/clear_browsing_data/public/quick_delete_constants.h"
#import "ios/chrome/browser/settings/ui_bundled/clear_browsing_data/quick_delete_other_data/public/quick_delete_other_data_commands.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/content_configuration/colorful_symbol_content_configuration.h"
#import "ios/chrome/browser/shared/ui/table_view/content_configuration/table_view_cell_content_configuration.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

// Browsing data type icon size.
constexpr CGFloat kDefaultSymbolSize = 24;

// TableView's footer section height.
constexpr CGFloat kSectionFooterHeight = 0;

// Section identifiers in the "Other data" page table view.
enum SectionIdentifier {
  kPasswordsAndPasskeysSection,
  kGoogleAccountDataSection,
  kFooterSection,
};

// Item identifiers in the "Other data" page.
enum ItemIdentifier {
  kPasswordsAndPasskeysIdentifier,
  kSearchHistoryIdentifier,
  kMyActivityIdentifier,
};

// Returns the branded version of the Google Services symbol.
UIImage* GetBrandedGoogleServicesSymbol() {
#if BUILDFLAG(IOS_USE_BRANDED_ASSETS)
  return MakeSymbolMonochrome(
      CustomSymbolWithPointSize(kGoogleIconSymbol, kDefaultSymbolSize));
#else
  return DefaultSymbolWithPointSize(kGearshape2Symbol, kDefaultSymbolSize);
#endif
}

// Returns the title for the given `itemIdentifier`.
NSString* TitleForItemIdentifier(ItemIdentifier item_identifier) {
  switch (item_identifier) {
    case ItemIdentifier::kPasswordsAndPasskeysIdentifier:
      return l10n_util::GetNSString(IDS_SETTINGS_PASSWORDS_AND_PASSKEYS);
    case ItemIdentifier::kSearchHistoryIdentifier:
      return l10n_util::GetNSString(IDS_SETTINGS_SEARCH_HISTORY);
    case ItemIdentifier::kMyActivityIdentifier:
      return l10n_util::GetNSString(IDS_SETTINGS_MY_ACTIVITY);
  }
  NOTREACHED();
}

// Returns the subtitle for the given `itemIdentifier`.
NSString* SubtitleForItemIdentifier(ItemIdentifier item_identifier) {
  switch (item_identifier) {
    case ItemIdentifier::kPasswordsAndPasskeysIdentifier:
      return l10n_util::GetNSString(
          IDS_SETTINGS_MANAGE_IN_GOOGLE_PASSWORD_MANAGER);
    case ItemIdentifier::kSearchHistoryIdentifier:
      // TODO(crbug.com/482036587) Replace the below
      // placeholder string with the subtitle variable coming
      // from the QuickDeleteOtherDataConsumer.
      return @"Change this";
    case ItemIdentifier::kMyActivityIdentifier:
      return l10n_util::GetNSString(IDS_SETTINGS_MANAGE_IN_YOUR_GOOGLE_ACCOUNT);
  }
  NOTREACHED();
}

// Returns the icon for the given `itemIdentifier`.
UIImage* IconForItemIdentifier(ItemIdentifier item_identifier) {
  switch (item_identifier) {
    case ItemIdentifier::kPasswordsAndPasskeysIdentifier:
      return CustomSymbolTemplateWithPointSize(kPasswordSymbol,
                                               kDefaultSymbolSize);
    case ItemIdentifier::kSearchHistoryIdentifier:
      return DefaultSymbolWithPointSize(kSearchSymbol, kDefaultSymbolSize);
    case ItemIdentifier::kMyActivityIdentifier:
      return GetBrandedGoogleServicesSymbol();
  }
  NOTREACHED();
}

// Returns the accessibilityIdentifier for the given `itemIdentifier`.
NSString* AccessibilityIdentifierForItemIdentifier(
    ItemIdentifier item_identifier) {
  switch (item_identifier) {
    case ItemIdentifier::kPasswordsAndPasskeysIdentifier:
      return kQuickDeleteOtherDataPasswordsAndPasskeysIdentifier;
    case ItemIdentifier::kSearchHistoryIdentifier:
      return kQuickDeleteOtherDataSearchHistoryIdentifier;
    case ItemIdentifier::kMyActivityIdentifier:
      return kQuickDeleteOtherDataMyActivityIdentifier;
  }
  NOTREACHED();
}

}  // namespace

@interface QuickDeleteOtherDataViewController () {
  UITableViewDiffableDataSource<NSNumber*, NSNumber*>* _dataSource;
}
@end

@interface QuickDeleteOtherDataViewController () {
  // The title for the "Quick Delete Other Data" page.
  NSString* _otherDataPageTitle;
  // The subtitle for the "Search history" cell.
  NSString* _searchHistoryCellSubtitle;
  // Tells if the "My Activity" cell is visible in the table view.
  BOOL _shouldShowMyActivityCell;
  // Tells if the "Search history" cell is visible in the table view.
  BOOL _shouldShowSearchHistoryCell;
}
@end

@implementation QuickDeleteOtherDataViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.navigationItem.largeTitleDisplayMode =
      UINavigationItemLargeTitleDisplayModeNever;
  // TODO(crbug.com/482036587) Replace the below placeholder title with the
  // title coming from the QuickDeleteOtherDataConsumer.
  self.title = l10n_util::GetNSString(IDS_SETTINGS_OTHER_GOOGLE_DATA_TITLE);
  [self loadModel];
}

- (void)didMoveToParentViewController:(UIViewController*)parent {
  [super didMoveToParentViewController:parent];
  if (!parent) {
    [self.quickDeleteOtherDataHandler hideQuickDeleteOtherDataPage];
  }
}

- (void)loadModel {
  __weak __typeof(self) weakSelf = self;
  _dataSource = [[UITableViewDiffableDataSource alloc]
      initWithTableView:self.tableView
           cellProvider:^UITableViewCell*(UITableView* tableView,
                                          NSIndexPath* indexPath,
                                          NSNumber* itemIdentifier) {
             return
                 [weakSelf cellForTableView:tableView
                                  indexPath:indexPath
                             itemIdentifier:static_cast<ItemIdentifier>(
                                                itemIdentifier.integerValue)];
           }];
  [TableViewCellContentConfiguration registerCellForTableView:self.tableView];
  RegisterTableViewHeaderFooter<TableViewTextHeaderFooterView>(self.tableView);

  NSDiffableDataSourceSnapshot* snapshot =
      [[NSDiffableDataSourceSnapshot alloc] init];
  [snapshot appendSectionsWithIdentifiers:@[
    @(SectionIdentifier::kPasswordsAndPasskeysSection),
    @(SectionIdentifier::kGoogleAccountDataSection),
    @(SectionIdentifier::kFooterSection)
  ]];
  [snapshot appendItemsWithIdentifiers:@[
    @(ItemIdentifier::kPasswordsAndPasskeysIdentifier)
  ]
             intoSectionWithIdentifier:
                 @(SectionIdentifier::kPasswordsAndPasskeysSection)];
  [snapshot appendItemsWithIdentifiers:@[
    @(ItemIdentifier::kSearchHistoryIdentifier),
    @(ItemIdentifier::kMyActivityIdentifier)
  ]
             intoSectionWithIdentifier:
                 @(SectionIdentifier::kGoogleAccountDataSection)];

  [_dataSource applySnapshot:snapshot animatingDifferences:NO];
}

#pragma mark - UITableViewDelegate

- (UIView*)tableView:(UITableView*)tableView
    viewForFooterInSection:(NSInteger)section {
  SectionIdentifier sectionIdentifier = static_cast<SectionIdentifier>(
      [_dataSource sectionIdentifierForIndex:section].integerValue);
  switch (sectionIdentifier) {
    case SectionIdentifier::kFooterSection: {
      TableViewTextHeaderFooterView* footer =
          DequeueTableViewHeaderFooter<TableViewTextHeaderFooterView>(
              tableView);
      footer.accessibilityIdentifier = kQuickDeleteOtherDataFooterIdentifier;
      [footer setSubtitle:l10n_util::GetNSString(
                              IDS_SETTINGS_OTHER_DATA_DESCRIPTION)
                withColor:[UIColor colorNamed:kTextSecondaryColor]];
      return footer;
    }
    case SectionIdentifier::kPasswordsAndPasskeysSection:
    case SectionIdentifier::kGoogleAccountDataSection:
      return nil;
  }
  NOTREACHED();
}

- (CGFloat)tableView:(UITableView*)tableView
    heightForFooterInSection:(NSInteger)section {
  SectionIdentifier sectionIdentifier = static_cast<SectionIdentifier>(
      [_dataSource sectionIdentifierForIndex:section].integerValue);
  if (sectionIdentifier == SectionIdentifier::kFooterSection) {
    return UITableViewAutomaticDimension;
  }
  return kSectionFooterHeight;
}

#pragma mark - QuickDeleteOtherDataConsumer

// TODO(crbug.com/471025894) Make the table view use the consumer's methods to
// show different UI.
- (void)setOtherDataPageTitle:(NSString*)title {
  _otherDataPageTitle = title;
}

- (void)setSearchHistoryCellSubtitle:(NSString*)subtitle {
  _searchHistoryCellSubtitle = subtitle;
}

- (void)setShouldShowMyActivityCell:(BOOL)shouldShowMyActivityCell {
  _shouldShowMyActivityCell = shouldShowMyActivityCell;
}

- (void)setShouldShowSearchHistoryCell:(BOOL)shouldShowSearchHistoryCell {
  _shouldShowSearchHistoryCell = shouldShowSearchHistoryCell;
}

#pragma mark - Private

// Sets the accessory type of the cell.
- (void)setAccessoryTypeForCell:(UITableViewCell*)cell
                 itemIdentifier:(ItemIdentifier)itemIdentifier {
  switch (itemIdentifier) {
    case ItemIdentifier::kPasswordsAndPasskeysIdentifier:
      cell.accessoryType = UITableViewCellAccessoryDisclosureIndicator;
      return;
    case ItemIdentifier::kSearchHistoryIdentifier:
    case ItemIdentifier::kMyActivityIdentifier:
      cell.accessoryView = [[UIImageView alloc]
          initWithImage:DefaultAccessorySymbolConfigurationWithRegularWeight(
                            kExternalLinkSymbol)];
      cell.accessoryView.tintColor = [UIColor colorNamed:kGrey500Color];
      return;
  }
  NOTREACHED();
}

// Creates a cell for the table view.
- (UITableViewCell*)createCellWithTitle:(NSString*)title
                               subtitle:(NSString*)subtitle
                                   icon:(UIImage*)icon
                accessibilityIdentifier:(NSString*)accessibilityIdentifier {
  TableViewCellContentConfiguration* configuration =
      [[TableViewCellContentConfiguration alloc] init];
  configuration.title = title;
  configuration.subtitle = subtitle;

  ColorfulSymbolContentConfiguration* symbolConfiguration =
      [[ColorfulSymbolContentConfiguration alloc] init];
  symbolConfiguration.symbolImage = icon;
  symbolConfiguration.symbolTintColor = [UIColor colorNamed:kGrey500Color];
  configuration.leadingConfiguration = symbolConfiguration;

  UITableViewCell* cell =
      [TableViewCellContentConfiguration dequeueTableViewCell:self.tableView];
  cell.contentConfiguration = configuration;
  cell.accessibilityIdentifier = accessibilityIdentifier;
  cell.accessibilityTraits |= UIAccessibilityTraitButton;
  return cell;
}

// Returns the cell for the corresponding `itemIdentifier`.
- (UITableViewCell*)cellForTableView:(UITableView*)tableView
                           indexPath:(NSIndexPath*)indexPath
                      itemIdentifier:(ItemIdentifier)itemIdentifier {
  UITableViewCell* cell;
  cell = [self createCellWithTitle:TitleForItemIdentifier(itemIdentifier)
                          subtitle:SubtitleForItemIdentifier(itemIdentifier)
                              icon:IconForItemIdentifier(itemIdentifier)
           accessibilityIdentifier:AccessibilityIdentifierForItemIdentifier(
                                       itemIdentifier)];

  [self setAccessoryTypeForCell:cell itemIdentifier:itemIdentifier];

  return cell;
}

@end
