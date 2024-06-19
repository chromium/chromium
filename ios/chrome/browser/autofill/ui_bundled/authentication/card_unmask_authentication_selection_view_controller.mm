// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/ui_bundled/authentication/card_unmask_authentication_selection_view_controller.h"

#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_detail_icon_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_link_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/autofill/ui_bundled/authentication/card_unmask_authentication_selection_constants.h"
#import "ios/chrome/browser/autofill/ui_bundled/cells/card_unmask_header_item.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// The reuse identifier for detail icon cells.
const NSString* kDetailIconCellReuseIdentifier =
    @"DetailIconCellReuseIdentifier";

}  // namespace

// The section id for the challenge options.
static NSString* kSectionIdChallengeOptions = @"SectionIdChallengeOptions";

@implementation CardUnmaskAuthenticationSelectionViewController {
  NSString* _headerTitle;
  NSString* _headerText;
  NSString* _footerText;
  NSArray<CardUnmaskChallengeOptionIOS*>* _challengeOptions;
  NSInteger _selectedChallengeOptionIndex;
  UITableViewDiffableDataSource<NSString*, NSNumber*>*
      _challengeOptionsDataSource;
}

- (instancetype)init {
  return [super initWithStyle:UITableViewStyleInsetGrouped];
}

- (void)viewDidLoad {
  self.title = l10n_util::GetNSString(
      IDS_AUTOFILL_CARD_UNMASK_PROMPT_NAVIGATION_TITLE_VERIFICATION);
  // Configure the cancel button.
  UIBarButtonItem* cancelBarButtonItem = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemCancel
                           target:self
                           action:@selector(didTapCancelButton)];
  cancelBarButtonItem.accessibilityIdentifier =
      kCardUnmaskAuthenticationSelectionCancelButtonAccessibilityIdentifier;
  self.navigationItem.leftBarButtonItem = cancelBarButtonItem;

  // Configure the table items.
  self.tableView.allowsSelection = YES;
  RegisterTableViewCell<TableViewDetailIconCell>(self.tableView);
  RegisterTableViewHeaderFooter<CardUnmaskHeaderView>(self.tableView);
  RegisterTableViewHeaderFooter<TableViewLinkHeaderFooterView>(self.tableView);
  __weak __typeof(self) weakSelf = self;
  _challengeOptionsDataSource = [[UITableViewDiffableDataSource alloc]
      initWithTableView:self.tableView
           cellProvider:^(UITableView* tableView, NSIndexPath* indexPath,
                          NSNumber* itemId) {
             return [weakSelf provideCellForTableView:tableView
                                            indexPath:indexPath
                                 challengeOptionIndex:itemId.integerValue];
           }];
  self.tableView.dataSource = _challengeOptionsDataSource;
  NSDiffableDataSourceSnapshot<NSString*, NSNumber*>* snapshot =
      [_challengeOptionsDataSource snapshot];
  [snapshot appendSectionsWithIdentifiers:@[ kSectionIdChallengeOptions ]];
  [snapshot appendItemsWithIdentifiers:[self arrayOfChallangeOptionIndicies]
             intoSectionWithIdentifier:kSectionIdChallengeOptions];
  [_challengeOptionsDataSource applySnapshotUsingReloadData:snapshot];
}

#pragma mark - CardUnmaskAuthenticationSelectionConsumer

- (void)setHeaderTitle:(NSString*)headerTitle {
  if ([_headerTitle isEqualToString:headerTitle]) {
    return;
  }
  _headerTitle = headerTitle;
  [self reloadSection];
}

- (void)setHeaderText:(NSString*)headerText {
  if ([_headerText isEqualToString:headerText]) {
    return;
  }
  _headerText = headerText;
  [self reloadSection];
}

- (void)setCardUnmaskOptions:
    (NSArray<CardUnmaskChallengeOptionIOS*>*)cardUnmaskChallengeOptions {
  if ([_challengeOptions isEqual:cardUnmaskChallengeOptions]) {
    return;
  }
  _challengeOptions = cardUnmaskChallengeOptions;
  [self reloadSection];
}

- (void)setFooterText:(NSString*)footerText {
  if ([_footerText isEqualToString:footerText]) {
    return;
  }
  _footerText = footerText;
  [self reloadSection];
}

- (void)setChallengeAcceptanceLabel:(NSString*)challengeAcceptanceLabel {
  UIBarButtonItem* barButtonItem = [[UIBarButtonItem alloc]
      initWithTitle:challengeAcceptanceLabel
              style:UIBarButtonItemStylePlain
             target:self
             action:@selector(didTapChallengeAcceptanceButton)];
  barButtonItem.accessibilityIdentifier =
      kCardUnmaskAuthenticationSelectionAcceptanceButtonAccessibilityIdentifier;
  self.navigationItem.rightBarButtonItem = barButtonItem;
}

- (void)enterPendingState {
  self.tableView.allowsSelection = NO;
  UIActivityIndicatorView* activityIndicator = [[UIActivityIndicatorView alloc]
      initWithActivityIndicatorStyle:UIActivityIndicatorViewStyleMedium];
  [activityIndicator startAnimating];
  self.navigationItem.rightBarButtonItem =
      [[UIBarButtonItem alloc] initWithCustomView:activityIndicator];
}

#pragma mark - UITableViewDelegate

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  // Override the default selection style.
  UITableViewCell* cell = [super tableView:tableView
                     cellForRowAtIndexPath:indexPath];
  cell.selectionStyle = UITableViewCellSelectionStyleNone;
  return cell;
}

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  // Set a checkmark on the cell when being selected.
  CHECK((NSUInteger)indexPath.row < [_challengeOptions count]);
  [self setSelectedChallengeOptionIndex:indexPath.row];
  [self.mutator didSelectChallengeOption:_challengeOptions[indexPath.row]];
}

- (UIView*)tableView:(UITableView*)tableView
    viewForHeaderInSection:(NSInteger)section {
  CardUnmaskHeaderView* view =
      DequeueTableViewHeaderFooter<CardUnmaskHeaderView>(self.tableView);
  view.titleLabel.text = _headerTitle;
  view.instructionsLabel.text = _headerText;
  return view;
}

- (UIView*)tableView:(UITableView*)tableView
    viewForFooterInSection:(NSInteger)section {
  TableViewLinkHeaderFooterView* view =
      DequeueTableViewHeaderFooter<TableViewLinkHeaderFooterView>(
          self.tableView);
  [view setText:_footerText withColor:[UIColor colorNamed:kTextSecondaryColor]];
  [view setForceIndents:YES];
  return view;
}

#pragma mark - Private

// Deques and sets up a cell for the challenge option at index.
- (UITableViewCell*)provideCellForTableView:(UITableView*)tableView
                                  indexPath:(NSIndexPath*)indexPath
                       challengeOptionIndex:(NSInteger)challengeOptionIndex {
  TableViewDetailIconCell* cell =
      DequeueTableViewCell<TableViewDetailIconCell>(self.tableView);
  cell.textLabel.text = _challengeOptions[indexPath.row].modeLabel;
  [cell setDetailText:_challengeOptions[indexPath.row].challengeInfo];

  [cell setAccessoryType:_selectedChallengeOptionIndex == challengeOptionIndex
                             ? UITableViewCellAccessoryCheckmark
                             : UITableViewCellAccessoryNone];
  [cell setTextLayoutConstraintAxis:UILayoutConstraintAxisVertical];

  [cell setDetailTextNumberOfLines:0];  // Use as many lines as needed.
  [cell setIconCenteredVertically:YES];
  return cell;
}

// Reloads the challenge options section on the challenge options data source.
- (void)reloadSection {
  NSDiffableDataSourceSnapshot<NSString*, NSNumber*>* snapshot =
      [_challengeOptionsDataSource snapshot];
  [snapshot reloadSectionsWithIdentifiers:@[ kSectionIdChallengeOptions ]];
  [_challengeOptionsDataSource
      applySnapshotUsingReloadData:[_challengeOptionsDataSource snapshot]];
}

// Returns an array of challenge option indicies.
- (NSArray*)arrayOfChallangeOptionIndicies {
  NSMutableArray<NSNumber*>* identifiers =
      [NSMutableArray arrayWithCapacity:[_challengeOptions count]];
  for (NSUInteger i = 0; i < [_challengeOptions count]; i++) {
    [identifiers addObject:[NSNumber numberWithUnsignedInteger:i]];
  }
  return identifiers;
}

// Sets the selected challenge option, updating the circle/checkmark icon.
- (void)setSelectedChallengeOptionIndex:(NSInteger)selectedIndex {
  if (_selectedChallengeOptionIndex == selectedIndex) {
    return;
  }
  NSDiffableDataSourceSnapshot<NSString*, NSNumber*>* snapshot =
      [_challengeOptionsDataSource snapshot];
  [snapshot reloadItemsWithIdentifiers:@[ @(_selectedChallengeOptionIndex) ]];
  _selectedChallengeOptionIndex = selectedIndex;
  [snapshot reloadItemsWithIdentifiers:@[ @(_selectedChallengeOptionIndex) ]];
  [_challengeOptionsDataSource applySnapshotUsingReloadData:snapshot];
}

// This method is a callback for the right bar button item used to accept the
// selected challenge option.
- (void)didTapChallengeAcceptanceButton {
  [self.mutator didAcceptSelection];
}

// This method is a callback for the left bar button, a cancel button.
- (void)didTapCancelButton {
  [self.mutator didCancelSelection];
}

@end
