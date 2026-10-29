// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/contextual_cueing/ui/gemini_contextual_cue_settings_view_controller.h"

#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/intelligence/contextual_cueing/ui/gemini_contextual_cue_settings_constants.h"
#import "ios/chrome/browser/intelligence/contextual_cueing/ui/gemini_contextual_cue_settings_mutator.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/content_configuration/table_view_cell_content_configuration.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

// Default height for a section footer when none is set.
constexpr CGFloat kDefaultHeaderFooterHeight = 10;

// Estimated height of a section header/footer.
constexpr CGFloat kEstimatedHeaderFooterHeight = 50;

// Section identifiers in the Gemini contextual cue settings table view.
enum SectionIdentifier {
  kSectionIdentifierManageSuggestions,
  kSectionIdentifierEditPrompt,
};

// Item identifiers in the Gemini contextual cue settings table view.
enum ItemIdentifier {
  kItemIdentifierManageSuggestions,
  kItemIdentifierEditPrompt,
};

// Returns the title for `item_identifier`.
NSString* TitleForItemIdentifier(ItemIdentifier item_identifier) {
  switch (item_identifier) {
    case kItemIdentifierManageSuggestions:
      return l10n_util::GetNSString(IDS_IOS_GEMINI_SETTINGS_MANAGE_SUGGESTIONS);
    case kItemIdentifierEditPrompt:
      return l10n_util::GetNSString(IDS_IOS_GEMINI_SETTINGS_EDIT_PROMPT);
  }
  NOTREACHED();
}

// Returns the accessibility identifier for `item_identifier`.
NSString* AccessibilityIdentifierForItemIdentifier(
    ItemIdentifier item_identifier) {
  switch (item_identifier) {
    case kItemIdentifierManageSuggestions:
      return kGeminiContextualCueSettingsManageSuggestionsCellId;
    case kItemIdentifierEditPrompt:
      return kGeminiContextualCueSettingsEditPromptCellId;
  }
  NOTREACHED();
}

}  // namespace

@implementation GeminiContextualCueSettingsViewController {
  // Diffable data source for the table view.
  UITableViewDiffableDataSource<NSNumber*, NSNumber*>* _dataSource;
  // The contextual cue prompt displayed in quotes below the "Edit prompt" row.
  NSString* _prompt;
}

- (instancetype)init {
  return [super initWithStyle:ChromeTableViewStyle()];
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.tableView.accessibilityIdentifier =
      kGeminiContextualCueSettingsTableViewAccessibilityIdentifier;
  self.tableView.estimatedSectionHeaderHeight = kEstimatedHeaderFooterHeight;
  self.tableView.estimatedSectionFooterHeight = kEstimatedHeaderFooterHeight;
  self.navigationItem.largeTitleDisplayMode =
      UINavigationItemLargeTitleDisplayModeNever;
  self.navigationItem.rightBarButtonItem = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemDone
                           target:self
                           action:@selector(doneButtonTapped)];
  self.navigationItem.rightBarButtonItem.accessibilityIdentifier =
      kGeminiContextualCueSettingsDoneButtonAccessibilityIdentifier;
  self.title =
      l10n_util::GetNSString(IDS_IOS_GEMINI_SETTINGS_SUGGESTIONS_TITLE);
  [self loadModel];
}

#pragma mark - GeminiContextualCueSettingsConsumer

- (void)setPrompt:(NSString*)prompt {
  if ([_prompt isEqualToString:prompt]) {
    return;
  }
  _prompt = [prompt copy];
  if (self.viewLoaded && _dataSource) {
    NSDiffableDataSourceSnapshot<NSNumber*, NSNumber*>* snapshot =
        [_dataSource snapshot];
    [snapshot
        reloadSectionsWithIdentifiers:@[ @(kSectionIdentifierEditPrompt) ]];
    [_dataSource applySnapshot:snapshot animatingDifferences:NO];
  }
}

#pragma mark - UITableViewDelegate

- (UIView*)tableView:(UITableView*)tableView
    viewForFooterInSection:(NSInteger)section {
  SectionIdentifier sectionIdentifier = static_cast<SectionIdentifier>(
      [_dataSource sectionIdentifierForIndex:section].integerValue);
  if (sectionIdentifier != kSectionIdentifierEditPrompt ||
      _prompt.length == 0) {
    return nil;
  }

  TableViewTextHeaderFooterView* footer =
      DequeueTableViewHeaderFooter<TableViewTextHeaderFooterView>(tableView);
  [footer setSubtitle:l10n_util::GetNSStringF(
                          IDS_IOS_GEMINI_SETTINGS_EDIT_PROMPT_FOOTER,
                          base::SysNSStringToUTF16(_prompt))];
  return footer;
}

- (CGFloat)tableView:(UITableView*)tableView
    heightForHeaderInSection:(NSInteger)section {
  return ChromeTableViewHeightForHeaderInSection(section);
}

- (CGFloat)tableView:(UITableView*)tableView
    heightForFooterInSection:(NSInteger)section {
  SectionIdentifier sectionIdentifier = static_cast<SectionIdentifier>(
      [_dataSource sectionIdentifierForIndex:section].integerValue);
  if (sectionIdentifier == kSectionIdentifierEditPrompt && _prompt.length > 0) {
    return UITableViewAutomaticDimension;
  }
  return kDefaultHeaderFooterHeight;
}

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  [tableView deselectRowAtIndexPath:indexPath animated:YES];

  ItemIdentifier itemIdentifier = static_cast<ItemIdentifier>(
      [_dataSource itemIdentifierForIndexPath:indexPath].integerValue);
  switch (itemIdentifier) {
    case kItemIdentifierManageSuggestions:
      [self.mutator didTapManageSuggestions];
      break;
    case kItemIdentifierEditPrompt:
      [self.mutator didTapEditPrompt];
      break;
  }
}

#pragma mark - Private

// Configures the diffable data source and applies the initial snapshot.
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

  NSDiffableDataSourceSnapshot<NSNumber*, NSNumber*>* snapshot =
      [[NSDiffableDataSourceSnapshot alloc] init];
  [snapshot appendSectionsWithIdentifiers:@[
    @(kSectionIdentifierManageSuggestions),
    @(kSectionIdentifierEditPrompt),
  ]];
  [snapshot appendItemsWithIdentifiers:@[ @(kItemIdentifierManageSuggestions) ]
             intoSectionWithIdentifier:@(kSectionIdentifierManageSuggestions)];
  [snapshot appendItemsWithIdentifiers:@[ @(kItemIdentifierEditPrompt) ]
             intoSectionWithIdentifier:@(kSectionIdentifierEditPrompt)];

  [_dataSource applySnapshot:snapshot animatingDifferences:NO];
}

// Dequeues and configures the cell for `itemIdentifier` at `indexPath`.
- (UITableViewCell*)cellForTableView:(UITableView*)tableView
                           indexPath:(NSIndexPath*)indexPath
                      itemIdentifier:(ItemIdentifier)itemIdentifier {
  TableViewCellContentConfiguration* configuration =
      [[TableViewCellContentConfiguration alloc] init];
  configuration.title = TitleForItemIdentifier(itemIdentifier);
  configuration.titleColor = [UIColor colorNamed:kBlueColor];

  UITableViewCell* cell =
      [TableViewCellContentConfiguration dequeueTableViewCell:tableView
                                                 forIndexPath:indexPath];
  cell.contentConfiguration = configuration;
  cell.accessibilityIdentifier =
      AccessibilityIdentifierForItemIdentifier(itemIdentifier);
  cell.accessibilityTraits |= UIAccessibilityTraitButton;
  return cell;
}

// Notifies the presentation delegate that the Done button was tapped.
- (void)doneButtonTapped {
  [self.presentationDelegate
      geminiContextualCueSettingsViewControllerDidTapDone:self];
}

@end
