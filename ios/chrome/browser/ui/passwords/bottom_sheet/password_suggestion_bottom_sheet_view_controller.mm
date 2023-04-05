// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/passwords/bottom_sheet/password_suggestion_bottom_sheet_view_controller.h"

#import "components/autofill/ios/browser/form_suggestion.h"
#import "ios/chrome/browser/passwords/password_controller_delegate.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_url_item.h"
#import "ios/chrome/browser/shared/ui/table_view/chrome_table_view_controller.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/icons/symbols.h"
#import "ios/chrome/browser/ui/passwords/bottom_sheet/password_suggestion_bottom_sheet_delegate.h"
#import "ios/chrome/browser/ui/settings/password/branded_navigation_item_title_view.h"
#import "ios/chrome/browser/ui/settings/password/create_password_manager_title_view.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Spacing size before image if there are no navigation bar.
CGFloat const kCustomSpacingBeforeImageIfNoNavigationBar = 24;

// Spacing size after image.
CGFloat const kCustomSpacingAfterImage = 30;

// Row height for each cell in the table view.
CGFloat const kTableViewRowHeight = 75;

// Radius size of the table view.
CGFloat const kTableViewCornerRadius = 10;
}  // namespace

@interface PasswordSuggestionBottomSheetViewController () <
    ConfirmationAlertActionHandler,
    UIGestureRecognizerDelegate,
    UITableViewDataSource,
    UITableViewDelegate> {
  // Row in the table of suggestions of the use selectesd suggestion.
  NSInteger _row;

  // If YES: the table view is currently showing a single suggestion
  // If NO: the table view is currently showing all suggestions
  BOOL _tableViewIsMinimized;

  // Height constraint for the bottom sheet when showing a single suggestion.
  NSLayoutConstraint* _minimizedHeightConstraint;

  // Height constraint for the bottom sheet when showing all suggestions.
  NSLayoutConstraint* _fullHeightConstraint;

  // Table view controller for the list of suggestions.
  ChromeTableViewController* _tableViewController;

  // List of suggestions in the bottom sheet
  // The property is defined by PasswordSuggestionBottomSheetConsumer protocol.
  NSArray<FormSuggestion*>* _suggestions;
}

@end

@implementation PasswordSuggestionBottomSheetViewController

- (instancetype)init {
  self = [super init];
  return self;
}

- (void)viewDidLoad {
  _tableViewIsMinimized = YES;

  self.titleView = [self setUpTitleView];
  self.underTitleView = [self createTableView];

  self.imageHasFixedSize = YES;
  self.showDismissBarButton = NO;
  self.customSpacingBeforeImageIfNoNavigationBar =
      kCustomSpacingBeforeImageIfNoNavigationBar;
  self.customSpacingAfterImage = kCustomSpacingAfterImage;
  self.titleTextStyle = UIFontTextStyleTitle2;
  self.topAlignedLayout = YES;
  self.actionHandler = self;

  self.primaryActionString =
      l10n_util::GetNSString(IDS_IOS_PASSWORD_BOTTOM_SHEET_USE_PASSWORD);
  self.secondaryActionString =
      l10n_util::GetNSString(IDS_IOS_PASSWORD_BOTTOM_SHEET_NO_THANKS);

  [super viewDidLoad];
}

- (void)viewWillDisappear:(BOOL)animated {
  [self.delegate refocus];
}

#pragma mark - PasswordSuggestionBottomSheetConsumer

- (void)setSuggestions:(NSArray<FormSuggestion*>*)suggestions {
  _suggestions = suggestions;
}

- (void)dismiss {
  [self dismissViewControllerAnimated:NO completion:NULL];
}

#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  if (_tableViewIsMinimized) {
    _tableViewIsMinimized = NO;

    _row = indexPath.row;

    // To be used later to show the checkmark icon next to the selected
    // suggestion.
    [_tableViewController.tableView reloadData];

    // Update table view height.
    __weak __typeof(self) weakSelf = self;
    [UIView animateWithDuration:0.1
                     animations:^{
                       [weakSelf expandTableView];
                     }];
  }
}

#pragma mark - UITableViewDataSource

- (NSInteger)tableView:(UITableView*)tableView
    numberOfRowsInSection:(NSInteger)section {
  return _tableViewIsMinimized ? 1 : _suggestions.count;
}

- (NSInteger)numberOfSectionsInTableView:(UITableView*)theTableView {
  return 1;
}

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  TableViewURLCell* cell =
      [tableView dequeueReusableCellWithIdentifier:@"cell"];
  cell.selectionStyle = UITableViewCellSelectionStyleNone;

  cell.titleLabel.text = [self suggestionAtRow:indexPath.row];
  cell.textLabel.lineBreakMode = NSLineBreakByTruncatingHead;
  cell.URLLabel.text = [self descriptionAtRow:indexPath.row];
  cell.URLLabel.hidden = NO;

  cell.titleLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];
  cell.backgroundColor = [UIColor colorNamed:kSecondaryBackgroundColor];

  if (_tableViewIsMinimized && (_suggestions.count > 1)) {
    // The table view is showing a single suggestion and the chevron down
    // symbol, which can be tapped in order to expand the list of suggestions.
    cell.accessoryView = [[UIImageView alloc]
        initWithImage:DefaultSymbolTemplateWithPointSize(
                          kChevronDownSymbol, kSymbolAccessoryPointSize)];
    cell.accessoryView.tintColor = [UIColor colorNamed:kTextQuaternaryColor];
  } else if (_row == indexPath.row) {
    // The table view is showing all suggestions, and this cell contains the
    // currently selected suggestion, so we display a checkmark on this cell.
    cell.accessoryView = [[UIImageView alloc]
        initWithImage:DefaultSymbolTemplateWithPointSize(
                          kCheckmarkSymbol, kSymbolAccessoryPointSize)];
    cell.accessoryView.tintColor = [UIColor colorNamed:kBlueColor];
  } else {
    // The table view is showing all suggestions, and this cell does not contain
    // the currently selected suggestion.
    cell.accessoryView = nil;
  }
  return cell;
}

#pragma mark - ConfirmationAlertActionHandler

- (void)confirmationAlertPrimaryAction {
  // Use password button
  __weak __typeof(self) weakSelf = self;
  [self dismissViewControllerAnimated:NO
                           completion:^{
                             // Send a notification to fill the
                             // username/password fields
                             [weakSelf didSelectSuggestion];
                           }];
}

- (void)confirmationAlertSecondaryAction {
  // "No thanks" button, which dismisses the bottom sheet.
  [self dismiss];
}

#pragma mark - Private

// Configures the title view of this ViewController.
- (UIView*)setUpTitleView {
  NSString* title = l10n_util::GetNSString(IDS_IOS_PASSWORD_MANAGER);
  return password_manager::CreatePasswordManagerTitleView(title);
}

// Returns the string to display at a given row in the table view.
- (NSString*)suggestionAtRow:(NSInteger)row {
  FormSuggestion* formSuggestion = [_suggestions objectAtIndex:row];
  return formSuggestion.value;
}

// Returns the display description at a given row in the table view.
- (NSString*)descriptionAtRow:(NSInteger)row {
  FormSuggestion* formSuggestion = [_suggestions objectAtIndex:row];
  return formSuggestion.displayDescription;
}

// Creates the password bottom sheet's table view, initially at minimized
// height.
- (UITableView*)createTableView {
  _tableViewController =
      [[ChromeTableViewController alloc] initWithStyle:UITableViewStylePlain];

  UITableView* tableView = _tableViewController.tableView;
  tableView.layer.cornerRadius = kTableViewCornerRadius;
  // FIXME(sugoi): The row height below must be dynamic for accessibility.
  tableView.rowHeight = kTableViewRowHeight;
  tableView.showsVerticalScrollIndicator = NO;
  tableView.delegate = self;
  tableView.dataSource = self;
  [self setTableViewWidth];
  [tableView registerClass:TableViewURLCell.class
      forCellReuseIdentifier:@"cell"];

  _minimizedHeightConstraint =
      [tableView.heightAnchor constraintEqualToConstant:tableView.rowHeight];
  _minimizedHeightConstraint.active = YES;

  _fullHeightConstraint = [tableView.heightAnchor
      constraintEqualToConstant:tableView.rowHeight * _suggestions.count];
  _fullHeightConstraint.active = NO;

  tableView.translatesAutoresizingMaskIntoConstraints = NO;

  return tableView;
}

// Sets the table view's width so that it's the same for any orientation.
- (void)setTableViewWidth {
  CGRect frame = [[UIScreen mainScreen] bounds];
  CGFloat tableWidth = MIN(frame.size.width, frame.size.height);
  [_tableViewController.tableView.widthAnchor
      constraintEqualToConstant:tableWidth]
      .active = YES;
}

// Sets the password bottom sheet's table view to full height.
- (void)expandTableView {
  _minimizedHeightConstraint.active = NO;
  _fullHeightConstraint.active = YES;
  [self.view layoutIfNeeded];
}

// Notifies the delegate that a password suggestion was selected by the user.
- (void)didSelectSuggestion {
  [self.delegate didSelectSuggestion:_row];
}

@end
