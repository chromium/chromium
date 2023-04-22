// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/passwords/bottom_sheet/password_suggestion_bottom_sheet_view_controller.h"

#import "base/mac/foundation_util.h"
#import "base/memory/raw_ptr.h"
#import "components/autofill/ios/browser/form_suggestion.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_url_item.h"
#import "ios/chrome/browser/shared/ui/table_view/chrome_table_view_controller.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/passwords/bottom_sheet/password_suggestion_bottom_sheet_delegate.h"
#import "ios/chrome/browser/ui/passwords/bottom_sheet/password_suggestion_bottom_sheet_handler.h"
#import "ios/chrome/browser/ui/settings/password/branded_navigation_item_title_view.h"
#import "ios/chrome/browser/ui/settings/password/create_password_manager_title_view.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/favicon/favicon_attributes.h"
#import "ios/chrome/common/ui/favicon/favicon_view.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Base height value for the bottom sheet without the table view.
// TODO(crbug.com/1422350): This needs some proper calculation.
CGFloat const kBaseHeightForBottomSheet = 225;

// Spacing size before image if there are no navigation bar.
CGFloat const kCustomSpacingBeforeImageIfNoNavigationBar = 24;

// Spacing size after image.
CGFloat const kCustomSpacingAfterImage = 30;

// Sets a custom radius for the half sheet presentation.
CGFloat const kHalfSheetCornerRadius = 20;

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

  // Table view for the list of suggestions.
  UITableView* _tableView;

  // List of suggestions in the bottom sheet
  // The property is defined by PasswordSuggestionBottomSheetConsumer protocol.
  NSArray<FormSuggestion*>* _suggestions;

  // The password controller handler used to open the password manager.
  id<PasswordSuggestionBottomSheetHandler> _handler;
}

@end

@implementation PasswordSuggestionBottomSheetViewController

- (instancetype)initWithHandler:
    (id<PasswordSuggestionBottomSheetHandler>)handler {
  self = [super init];
  if (self) {
    _handler = handler;

    [self setUpBottomSheet];
  }
  return self;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  _tableViewIsMinimized = YES;

  self.titleView = [self setUpTitleView];
  self.underTitleView = [self createTableView];

  // Set the properties read by the super when constructing the
  // views in `-[ConfirmationAlertViewController viewDidLoad]`.
  self.imageHasFixedSize = YES;
  self.showsVerticalScrollIndicator = NO;
  self.showDismissBarButton = NO;
  self.customSpacingBeforeImageIfNoNavigationBar =
      kCustomSpacingBeforeImageIfNoNavigationBar;
  self.customSpacingAfterImage = kCustomSpacingAfterImage;
  self.titleTextStyle = UIFontTextStyleTitle2;
  self.topAlignedLayout = YES;
  self.actionHandler = self;
  self.scrollEnabled = NO;

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
  _row = indexPath.row;

  if (_tableViewIsMinimized) {
    _tableViewIsMinimized = NO;

    // Update table view height.
    __weak __typeof(self) weakSelf = self;
    [UIView animateWithDuration:0.1
                     animations:^{
                       [weakSelf expandTableView];
                     }];

    [self expand];
  }

  // Refresh cells to show the checkmark icon next to the selected suggestion.
  [_tableView reloadData];
}

// Long press open context menu.
- (UIContextMenuConfiguration*)tableView:(UITableView*)tableView
    contextMenuConfigurationForRowAtIndexPath:(NSIndexPath*)indexPath
                                        point:(CGPoint)point {
  __weak __typeof(self) weakSelf = self;
  UIContextMenuActionProvider actionProvider =
      ^(NSArray<UIMenuElement*>* suggestedActions) {
        NSMutableArray<UIMenuElement*>* menuElements =
            [[NSMutableArray alloc] initWithArray:suggestedActions];

        PasswordSuggestionBottomSheetViewController* strongSelf = weakSelf;
        if (strongSelf) {
          [menuElements addObject:[strongSelf openPasswordManagerAction]];
        }

        return [UIMenu menuWithTitle:@"" children:menuElements];
      };

  return
      [UIContextMenuConfiguration configurationWithIdentifier:nil
                                              previewProvider:nil
                                               actionProvider:actionProvider];
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

  // Note that both the credentials and URLs will use middle truncation, as it
  // generally makes it easier to differentiate between different ones, without
  // having to resort to displaying multiple lines to show the full username
  // and URL.
  cell.titleLabel.text = [self suggestionAtRow:indexPath.row];
  cell.titleLabel.lineBreakMode = NSLineBreakByTruncatingMiddle;
  cell.URLLabel.text = [self descriptionAtRow:indexPath.row];
  cell.URLLabel.lineBreakMode = NSLineBreakByTruncatingMiddle;
  cell.URLLabel.hidden = NO;

  // Make separator invisible on last cell
  CGFloat separatorLeftMargin =
      (_tableViewIsMinimized || [self isLastRow:indexPath])
          ? _tableView.bounds.size.width
          : kTableViewHorizontalSpacing;
  cell.separatorInset = UIEdgeInsetsMake(0.f, separatorLeftMargin, 0.f, 0.f);

  [cell setFaviconContainerBackgroundColor:
            [UIColor colorNamed:kPrimaryBackgroundColor]];
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
  [self loadFaviconAtIndexPath:indexPath forCell:cell];
  return cell;
}

#pragma mark - ConfirmationAlertActionHandler

- (void)confirmationAlertPrimaryAction {
  // Use password button
  [self.delegate disableRefocus];
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

// Configures the bottom sheet's appearance and detents.
- (void)setUpBottomSheet {
  self.modalPresentationStyle = UIModalPresentationPageSheet;
  UISheetPresentationController* presentationController =
      self.sheetPresentationController;
  presentationController.prefersEdgeAttachedInCompactHeight = YES;
  presentationController.widthFollowsPreferredContentSizeWhenEdgeAttached = YES;
  if (@available(iOS 16, *)) {
    CGFloat bottomSheetHeight = [self initialHeight];
    auto detentBlock = ^CGFloat(
        id<UISheetPresentationControllerDetentResolutionContext> context) {
      return bottomSheetHeight;
    };
    UISheetPresentationControllerDetent* customDetent =
        [UISheetPresentationControllerDetent
            customDetentWithIdentifier:@"customDetent"
                              resolver:detentBlock];
    presentationController.detents = @[ customDetent ];
    presentationController.selectedDetentIdentifier = @"customDetent";
  } else {
    presentationController.detents = @[
      [UISheetPresentationControllerDetent mediumDetent],
      [UISheetPresentationControllerDetent largeDetent]
    ];
  }
  presentationController.preferredCornerRadius = kHalfSheetCornerRadius;
}

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
  CGRect frame = [[UIScreen mainScreen] bounds];
  _tableView = [[UITableView alloc] initWithFrame:frame
                                            style:UITableViewStylePlain];

  _tableView.layer.cornerRadius = kTableViewCornerRadius;
  _tableView.rowHeight = [self rowHeight];
  _tableView.scrollEnabled = NO;
  _tableView.showsVerticalScrollIndicator = NO;
  _tableView.delegate = self;
  _tableView.dataSource = self;
  [_tableView registerClass:TableViewURLCell.class
      forCellReuseIdentifier:@"cell"];

  // Set the table view's width so that it's the same for any orientation.
  CGFloat tableWidth = MIN(frame.size.width, frame.size.height);
  // TODO(crbug.com/1422350): Adjust this constraint properly
  [_tableView.widthAnchor constraintEqualToConstant:tableWidth].active = YES;

  _minimizedHeightConstraint =
      [_tableView.heightAnchor constraintEqualToConstant:_tableView.rowHeight];
  _minimizedHeightConstraint.active = YES;

  _fullHeightConstraint = [_tableView.heightAnchor
      constraintEqualToConstant:_tableView.rowHeight * _suggestions.count];
  _fullHeightConstraint.active = NO;

  _tableView.translatesAutoresizingMaskIntoConstraints = NO;

  return _tableView;
}

// Loads the favicon associated with the provided cell.
// Defaults to the globe symbol if no URL is associated with the cell.
- (void)loadFaviconAtIndexPath:(NSIndexPath*)indexPath
                       forCell:(UITableViewCell*)cell {
  DCHECK(cell);

  TableViewURLCell* URLCell = base::mac::ObjCCastStrict<TableViewURLCell>(cell);

  // Set the cell identifier to the associated URL, which we use to fetch the
  // favicon.
  NSString* cellIdentifier = [self descriptionAtRow:indexPath.row];
  URLCell.cellUniqueIdentifier = cellIdentifier;

  auto faviconLoadedBlock = ^(FaviconAttributes* attributes) {
    // If the user scrolls quickly, the cell could be reused, so make sure the
    // favicon still matches the URL used to fetch the favicon (as the favicon
    // fetch is asynchronous).
    if ([URLCell.cellUniqueIdentifier isEqualToString:cellIdentifier]) {
      DCHECK(attributes);
      [URLCell.faviconView configureWithAttributes:attributes];
    }
  };
  [self.delegate loadFaviconAtIndexPath:indexPath
                    faviconBlockHandler:faviconLoadedBlock];
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

// Returns whether the provided index path points to the last row of the table
// view.
- (BOOL)isLastRow:(NSIndexPath*)indexPath {
  return NSUInteger(indexPath.row) == (_suggestions.count - 1);
}

// Height of 1 row in the table view
- (CGFloat)rowHeight {
  // TODO(crbug.com/1422350): The row height below must be dynamic for
  // accessibility.
  return kTableViewRowHeight;
}

// Returns the initial height of the bottom sheet.
- (CGFloat)initialHeight {
  // Initial height for the bottom sheet while showing a single row.
  return kBaseHeightForBottomSheet + [self rowHeight];
}

// Returns the desired height for the bottom sheet (can be larger than the
// screen).
- (CGFloat)fullHeight {
  // Desired height for the bottom sheet while showing all rows.
  return kBaseHeightForBottomSheet + ([self rowHeight] * _suggestions.count);
}

// Enables scrolling of the table view
- (void)enableScrolling {
  _tableView.scrollEnabled = YES;
  self.scrollEnabled = YES;
}

// Performs the expand bottom sheet animation.
- (void)expand {
  UISheetPresentationController* presentationController =
      self.sheetPresentationController;
  if (@available(iOS 16, *)) {
    // Expand to custom size (only available for iOS 16+).
    CGFloat fullHeight = [self fullHeight];

    __weak __typeof(self) weakSelf = self;
    auto fullHeightBlock = ^CGFloat(
        id<UISheetPresentationControllerDetentResolutionContext> context) {
      BOOL tooLarge = (fullHeight > context.maximumDetentValue);
      if (tooLarge) {
        [weakSelf enableScrolling];
      }
      return tooLarge ? context.maximumDetentValue : fullHeight;
    };
    UISheetPresentationControllerDetent* customDetentExpand =
        [UISheetPresentationControllerDetent
            customDetentWithIdentifier:@"customDetentExpand"
                              resolver:fullHeightBlock];
    NSMutableArray* currentDetents =
        [presentationController.detents mutableCopy];
    [currentDetents addObject:customDetentExpand];
    presentationController.detents = [currentDetents copy];
    [presentationController animateChanges:^{
      presentationController.selectedDetentIdentifier = @"customDetentExpand";
    }];
  } else {
    // Expand to large detent.
    [self enableScrolling];
    [presentationController animateChanges:^{
      presentationController.selectedDetentIdentifier =
          UISheetPresentationControllerDetentIdentifierLarge;
    }];
  }
}

// Opens the password manager settings page.
- (void)displayPasswordManager {
  [_handler displayPasswordManager];
}

// Creates the UI action used to open the password manager.
- (UIAction*)openPasswordManagerAction {
  __weak __typeof(self) weakSelf = self;
  void (^passwordManagerButtonTapHandler)(UIAction*) = ^(UIAction* action) {
    // Open Password Manager.
    [weakSelf.delegate disableRefocus];
    [weakSelf dismissViewControllerAnimated:NO
                                 completion:^{
                                   // Send some message to open the password
                                   // manager.
                                   [weakSelf displayPasswordManager];
                                 }];
  };
  UIImage* keyIcon =
      CustomSymbolWithPointSize(kPasswordSymbol, kSymbolActionPointSize);
  return [UIAction
      actionWithTitle:l10n_util::GetNSString(
                          IDS_IOS_PASSWORD_BOTTOM_SHEET_PASSWORD_MANAGER)
                image:keyIcon
           identifier:@"kBottomSheetPopupMenuPasswordManagerActionIdentifier"
              handler:passwordManagerButtonTapHandler];
}

@end
