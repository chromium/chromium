// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/ui_bundled/clear_browsing_data/quick_delete_other_data/ui/quick_delete_other_data_view_controller.h"

#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/settings/ui_bundled/clear_browsing_data/public/quick_delete_constants.h"
#import "ios/chrome/browser/settings/ui_bundled/clear_browsing_data/public/quick_delete_util.h"
#import "ios/chrome/browser/settings/ui_bundled/clear_browsing_data/quick_delete_other_data/public/quick_delete_other_data_commands.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/content_configuration/colorful_symbol_content_configuration.h"
#import "ios/chrome/browser/shared/ui/table_view/content_configuration/table_view_cell_content_configuration.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ui/base/l10n/l10n_util_mac.h"

using quick_delete_util::DefaultSearchEngineState;

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
    case kPasswordsAndPasskeysIdentifier:
      return l10n_util::GetNSString(IDS_SETTINGS_PASSWORDS_AND_PASSKEYS);
    case kSearchHistoryIdentifier:
      return l10n_util::GetNSString(IDS_SETTINGS_SEARCH_HISTORY);
    case kMyActivityIdentifier:
      return l10n_util::GetNSString(IDS_SETTINGS_MY_ACTIVITY);
  }
  NOTREACHED();
}

// Returns the icon for the given `itemIdentifier`.
UIImage* IconForItemIdentifier(ItemIdentifier item_identifier) {
  switch (item_identifier) {
    case kPasswordsAndPasskeysIdentifier:
      return CustomSymbolTemplateWithPointSize(kPasswordSymbol,
                                               kDefaultSymbolSize);
    case kSearchHistoryIdentifier:
      return DefaultSymbolWithPointSize(kSearchSymbol, kDefaultSymbolSize);
    case kMyActivityIdentifier:
      return GetBrandedGoogleServicesSymbol();
  }
  NOTREACHED();
}

// Returns the accessibilityIdentifier for the given `itemIdentifier`.
NSString* AccessibilityIdentifierForItemIdentifier(
    ItemIdentifier item_identifier) {
  switch (item_identifier) {
    case kPasswordsAndPasskeysIdentifier:
      return kQuickDeleteOtherDataPasswordsAndPasskeysIdentifier;
    case kSearchHistoryIdentifier:
      return kQuickDeleteOtherDataSearchHistoryIdentifier;
    case kMyActivityIdentifier:
      return kQuickDeleteOtherDataMyActivityIdentifier;
  }
  NOTREACHED();
}

}  // namespace

@interface QuickDeleteOtherDataViewController () {
  // The table view for this view controller.
  UITableViewDiffableDataSource<NSNumber*, NSNumber*>* _dataSource;
  // Provides the current default search engine state.
  DefaultSearchEngineState _defaultSearchEngineState;
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
  self.title = _otherDataPageTitle;
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
    @(kPasswordsAndPasskeysSection), @(kGoogleAccountDataSection),
    @(kFooterSection)
  ]];
  [snapshot appendItemsWithIdentifiers:@[ @(kPasswordsAndPasskeysIdentifier) ]
             intoSectionWithIdentifier:@(kPasswordsAndPasskeysSection)];

  if (_shouldShowSearchHistoryCell) {
    [snapshot appendItemsWithIdentifiers:@[ @(kSearchHistoryIdentifier) ]
               intoSectionWithIdentifier:@(kGoogleAccountDataSection)];
  }
  if (_shouldShowMyActivityCell) {
    [snapshot appendItemsWithIdentifiers:@[ @(kMyActivityIdentifier) ]
               intoSectionWithIdentifier:@(kGoogleAccountDataSection)];
  }

  [_dataSource applySnapshot:snapshot animatingDifferences:NO];
}

#pragma mark - UITableViewDelegate

- (UIView*)tableView:(UITableView*)tableView
    viewForFooterInSection:(NSInteger)section {
  SectionIdentifier sectionIdentifier = static_cast<SectionIdentifier>(
      [_dataSource sectionIdentifierForIndex:section].integerValue);
  switch (sectionIdentifier) {
    case kFooterSection: {
      TableViewTextHeaderFooterView* footer =
          DequeueTableViewHeaderFooter<TableViewTextHeaderFooterView>(
              tableView);
      footer.accessibilityIdentifier = kQuickDeleteOtherDataFooterIdentifier;
      [footer setSubtitle:l10n_util::GetNSString(
                              IDS_SETTINGS_OTHER_DATA_DESCRIPTION)
                withColor:[UIColor colorNamed:kTextSecondaryColor]];
      return footer;
    }
    case kPasswordsAndPasskeysSection:
    case kGoogleAccountDataSection:
      // No footer is required for these sections.
      return nil;
  }
  NOTREACHED();
}

- (CGFloat)tableView:(UITableView*)tableView
    heightForFooterInSection:(NSInteger)section {
  SectionIdentifier sectionIdentifier = static_cast<SectionIdentifier>(
      [_dataSource sectionIdentifierForIndex:section].integerValue);
  if (sectionIdentifier == kFooterSection) {
    return UITableViewAutomaticDimension;
  }
  return kSectionFooterHeight;
}

#pragma mark - QuickDeleteOtherDataConsumer

- (void)setDefaultSearchEngineState:
    (DefaultSearchEngineState)defaultSearchEngineState {
  _defaultSearchEngineState = defaultSearchEngineState;
}

- (void)setOtherDataPageTitle:(NSString*)title {
  if ([_otherDataPageTitle isEqualToString:title]) {
    return;
  }
  _otherDataPageTitle = title;
  if (self.viewLoaded) {
    self.title = _otherDataPageTitle;
  }
}

- (void)setSearchHistoryCellSubtitle:(NSString*)subtitle {
  if ([_searchHistoryCellSubtitle isEqualToString:subtitle]) {
    return;
  }
  _searchHistoryCellSubtitle = subtitle;
  if (_shouldShowSearchHistoryCell) {
    [self updateSnapshotForItemIdentifier:kSearchHistoryIdentifier];
  }
}

- (void)setShouldShowMyActivityCell:(BOOL)shouldShowMyActivityCell {
  if (_shouldShowMyActivityCell == shouldShowMyActivityCell) {
    return;
  }
  _shouldShowMyActivityCell = shouldShowMyActivityCell;
}

- (void)setShouldShowSearchHistoryCell:(BOOL)shouldShowSearchHistoryCell {
  _shouldShowSearchHistoryCell = shouldShowSearchHistoryCell;
  // It will update the "Search history" cell and the "My Activity" cell at the
  // same time, resulting in a cleaner UI update.
  [self applySnapshotForGoogleAccountDataSectionAnimatingDifferences];
}

#pragma mark - Private

// Returns the cell for the corresponding `itemIdentifier`.
- (UITableViewCell*)cellForTableView:(UITableView*)tableView
                           indexPath:(NSIndexPath*)indexPath
                      itemIdentifier:(ItemIdentifier)itemIdentifier {
  UITableViewCell* cell;
  cell = [self
          createCellWithTitle:TitleForItemIdentifier(itemIdentifier)
                     subtitle:[self subtitleForItemIdentifier:itemIdentifier]
                         icon:IconForItemIdentifier(itemIdentifier)
      accessibilityIdentifier:AccessibilityIdentifierForItemIdentifier(
                                  itemIdentifier)];

  [self setAccessoryTypeForCell:cell itemIdentifier:itemIdentifier];

  return cell;
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

// Reloads the snapshot for the cell with the given `itemIdentifier`.
- (void)updateSnapshotForItemIdentifier:(ItemIdentifier)itemIdentifier {
  NSDiffableDataSourceSnapshot<NSNumber*, NSNumber*>* snapshot =
      [_dataSource snapshot];
  [snapshot reloadItemsWithIdentifiers:@[ @(itemIdentifier) ]];
  [_dataSource applySnapshot:snapshot animatingDifferences:YES];
}

// Applies a snapshot to the `kGoogleAccountDataSection` based on the visibility
// flags.
- (void)applySnapshotForGoogleAccountDataSectionAnimatingDifferences {
  if (!_dataSource) {
    return;
  }

  NSDiffableDataSourceSnapshot<NSNumber*, NSNumber*>* snapshot =
      [_dataSource snapshot];

  // Remove existing items from the section to ensure they are added in the
  // correct order if they become visible. We delete them first to avoid
  // duplicates and to simplify the ordering logic.
  [snapshot deleteItemsWithIdentifiers:@[
    @(kSearchHistoryIdentifier), @(kMyActivityIdentifier)
  ]];

  // Add items based on visibility flags. The order here determines the order
  // in the table view.
  if (_shouldShowSearchHistoryCell) {
    [snapshot appendItemsWithIdentifiers:@[ @(kSearchHistoryIdentifier) ]
               intoSectionWithIdentifier:@(kGoogleAccountDataSection)];
  }
  if (_shouldShowMyActivityCell) {
    [snapshot appendItemsWithIdentifiers:@[ @(kMyActivityIdentifier) ]
               intoSectionWithIdentifier:@(kGoogleAccountDataSection)];
  }

  [_dataSource applySnapshot:snapshot animatingDifferences:YES];
}

// Sets the accessory type of the cell.
- (void)setAccessoryTypeForCell:(UITableViewCell*)cell
                 itemIdentifier:(ItemIdentifier)itemIdentifier {
  switch (itemIdentifier) {
    case kPasswordsAndPasskeysIdentifier:
      cell.accessoryType = UITableViewCellAccessoryDisclosureIndicator;
      return;
    case kSearchHistoryIdentifier:
      // The "Search history" cell does not have an accessory type when the
      // default search engine is not Google as it will not redirect the user to
      // Google account data.
      if (_defaultSearchEngineState != DefaultSearchEngineState::kGoogle) {
        return;
      }
      [[fallthrough]];
    case kMyActivityIdentifier:
      cell.accessoryView = [[UIImageView alloc]
          initWithImage:DefaultAccessorySymbolConfigurationWithRegularWeight(
                            kExternalLinkSymbol)];
      cell.accessoryView.tintColor = [UIColor colorNamed:kGrey500Color];
      return;
  }
  NOTREACHED();
}

// Returns the subtitle for the given `itemIdentifier`.
- (NSString*)subtitleForItemIdentifier:(ItemIdentifier)itemIdentifier {
  switch (itemIdentifier) {
    case kPasswordsAndPasskeysIdentifier:
      return l10n_util::GetNSString(
          IDS_SETTINGS_MANAGE_IN_GOOGLE_PASSWORD_MANAGER);
    case kSearchHistoryIdentifier:
      return _searchHistoryCellSubtitle;
    case kMyActivityIdentifier:
      return l10n_util::GetNSString(IDS_SETTINGS_MANAGE_IN_YOUR_GOOGLE_ACCOUNT);
  }
  NOTREACHED();
}

@end
