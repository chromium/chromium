// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/passwords/bottom_sheet/password_suggestion_bottom_sheet_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/strings/sys_string_conversions.h"
#import "components/autofill/ios/browser/form_suggestion.h"
#import "components/password_manager/core/browser/ui/credential_ui_entry.h"
#import "components/password_manager/ios/shared_password_controller.h"
#import "components/url_formatter/elide_url.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_url_item.h"
#import "ios/chrome/browser/ui/passwords/bottom_sheet/password_suggestion_bottom_sheet_delegate.h"
#import "ios/chrome/browser/ui/passwords/bottom_sheet/password_suggestion_bottom_sheet_handler.h"
#import "ios/chrome/browser/ui/settings/password/branded_navigation_item_title_view.h"
#import "ios/chrome/browser/ui/settings/password/create_password_manager_title_view.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"
#import "ios/chrome/common/ui/favicon/favicon_view.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "url/gurl.h"

namespace {

// Spacing use for the spacing before the logo title in the bottom sheet.
CGFloat const kSpacingBeforeTitle = 16;

// Spacing use for the spacing after the logo title in the bottom sheet.
CGFloat const kSpacingAfterTitle = 4;

}  // namespace

@interface PasswordSuggestionBottomSheetViewController () <
    ConfirmationAlertActionHandler,
    UITableViewDataSource,
    UITableViewDelegate> {
  // If YES: the table view is currently showing a single suggestion
  // If NO: the table view is currently showing all suggestions
  BOOL _tableViewIsMinimized;

  // Height constraint for the bottom sheet when showing a single suggestion.
  NSLayoutConstraint* _minimizedHeightConstraint;

  // Height constraint for the bottom sheet when showing all suggestions.
  NSLayoutConstraint* _fullHeightConstraint;

  // List of suggestions in the bottom sheet
  // The property is defined by PasswordSuggestionBottomSheetConsumer protocol.
  NSArray<FormSuggestion*>* _suggestions;

  // The current's page domain. This is used for the password bottom sheet
  // description label.
  NSString* _domain;

  // URL of the current page the bottom sheet is being displayed on.
  GURL _URL;

  // The following are displayed to the user whenever they receive some new
  // passwords via password sharing that they have not acknowledged before. Nil
  // otherwise.
  NSString* _title;
  NSString* _subtitle;
}

// The password controller handler used to open the password manager.
@property(nonatomic, weak) id<PasswordSuggestionBottomSheetHandler> handler;

@end

@implementation PasswordSuggestionBottomSheetViewController

- (instancetype)initWithHandler:
                    (id<PasswordSuggestionBottomSheetHandler>)handler
                            URL:(const GURL&)URL {
  self = [super init];
  if (self) {
    self.handler = handler;
    _URL = URL;
  }
  return self;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  _tableViewIsMinimized = YES;

  self.view.accessibilityViewIsModal = YES;
  self.aboveTitleView = [self setUpTitleView];
  self.customSpacing = kSpacingAfterTitle;
  self.customSpacingBeforeImageIfNoNavigationBar = kSpacingBeforeTitle;

  // Set the properties read by the super when constructing the
  // views in `-[ConfirmationAlertViewController viewDidLoad]`.
  self.actionHandler = self;

  self.titleString = _title;
  self.titleTextStyle = UIFontTextStyleTitle2;

  self.secondaryActionString =
      l10n_util::GetNSString(IDS_IOS_PASSWORD_BOTTOM_SHEET_USE_KEYBOARD);
  self.secondaryActionImage =
      DefaultSymbolWithPointSize(kKeyboardSymbol, kSymbolActionPointSize);

  if (_subtitle) {
    self.subtitleString = _subtitle;
  } else {
    self.subtitleTextStyle = UIFontTextStyleFootnote;
    std::u16string formattedURL =
        url_formatter::FormatUrlForDisplayOmitSchemePathAndTrivialSubdomains(
            _URL);
    self.subtitleString = l10n_util::GetNSStringF(
        IDS_IOS_PASSWORD_BOTTOM_SHEET_SUBTITLE, formattedURL);
  }

  [super viewDidLoad];
}

- (void)viewWillTransitionToSize:(CGSize)size
       withTransitionCoordinator:
           (id<UIViewControllerTransitionCoordinator>)coordinator {
  [super viewWillTransitionToSize:size withTransitionCoordinator:coordinator];
  if (!_tableViewIsMinimized) {
    // Recompute sheet height and enable/disable scrolling if required.
    __weak __typeof(self) weakSelf = self;
    [coordinator
        animateAlongsideTransition:nil
                        completion:^(
                            id<UIViewControllerTransitionCoordinatorContext>
                                context) {
                          [weakSelf expand];
                        }];
  }
}

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];

  if (self.traitCollection.preferredContentSizeCategory !=
      previousTraitCollection.preferredContentSizeCategory) {
    [self updateHeightConstraints];
  }
}

- (void)viewIsAppearing:(BOOL)animated {
#if __IPHONE_OS_VERSION_MAX_ALLOWED >= 170000
  [super viewIsAppearing:animated];
#endif

  [self updateHeightConstraints];
}

- (void)viewDidAppear:(BOOL)animated {
  [super viewDidAppear:animated];
  UIAccessibilityPostNotification(UIAccessibilityAnnouncementNotification,
                                  self.aboveTitleView.accessibilityLabel);
}

- (void)viewWillDisappear:(BOOL)animated {
  [self.delegate dismiss];
}

#pragma mark - PasswordSuggestionBottomSheetConsumer

- (void)setSuggestions:(NSArray<FormSuggestion*>*)suggestions
             andDomain:(NSString*)domain {
  _suggestions = suggestions;
  _domain = domain;
}

- (void)setTitle:(NSString*)title subtitle:(NSString*)subtitle {
  _title = title;
  _subtitle = subtitle;
}

- (void)dismiss {
  __weak __typeof(self) weakSelf = self;
  [self dismissViewControllerAnimated:NO
                           completion:^{
                             [weakSelf.handler stop];
                           }];
}

#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  base::UmaHistogramBoolean(
      "IOS.PasswordBottomSheet.UsernameTapped.MinimizedState",
      _tableViewIsMinimized);

  if (_suggestions.count <= 1) {
    return;
  }

  if (_tableViewIsMinimized) {
    _tableViewIsMinimized = NO;
    [tableView cellForRowAtIndexPath:indexPath].accessoryView = nil;
    // Make separator visible on first cell.
    [tableView cellForRowAtIndexPath:indexPath].separatorInset =
        UIEdgeInsetsMake(0.f, kTableViewHorizontalSpacing, 0.f, 0.f);
    [self addRemainingRowsToTableView:tableView];

    // Update table view height.
    __weak __typeof(self) weakSelf = self;
    [UIView animateWithDuration:0.1
                     animations:^{
                       [weakSelf expandTableView];
                     }];

    [self expand];
  }

  [super tableView:tableView didSelectRowAtIndexPath:indexPath];
}

// Long press open context menu.
- (UIContextMenuConfiguration*)tableView:(UITableView*)tableView
    contextMenuConfigurationForRowAtIndexPath:(NSIndexPath*)indexPath
                                        point:(CGPoint)point {
  __weak __typeof(self) weakSelf = self;
  UIContextMenuActionProvider actionProvider = ^(
      NSArray<UIMenuElement*>* suggestedActions) {
    NSMutableArray<UIMenu*>* menuElements =
        [[NSMutableArray alloc] initWithArray:suggestedActions];

    PasswordSuggestionBottomSheetViewController* strongSelf = weakSelf;
    if (strongSelf) {
      [menuElements
          addObject:[UIMenu menuWithTitle:@""
                                    image:nil
                               identifier:nil
                                  options:UIMenuOptionsDisplayInline
                                 children:@[
                                   [strongSelf openPasswordManagerAction]
                                 ]]];
      [menuElements
          addObject:[UIMenu
                        menuWithTitle:@""
                                image:nil
                           identifier:nil
                              options:UIMenuOptionsDisplayInline
                             children:@[
                               [strongSelf
                                   openPasswordDetailsForIndexPath:indexPath]
                             ]]];
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
  return _tableViewIsMinimized ? [self initialNumberOfVisibleCells]
                               : _suggestions.count;
}

- (NSInteger)numberOfSectionsInTableView:(UITableView*)theTableView {
  return 1;
}

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  TableViewURLCell* cell =
      [tableView dequeueReusableCellWithIdentifier:@"cell"];
  return [self layoutCell:cell
        forTableViewWidth:tableView.frame.size.width
              atIndexPath:indexPath];
}

#pragma mark - ConfirmationAlertActionHandler

- (void)confirmationAlertPrimaryAction {
  // Use password button
  __weak __typeof(self) weakSelf = self;
  [self.delegate willSelectSuggestion];
  [self dismissViewControllerAnimated:NO
                           completion:^{
                             // Send a notification to fill the
                             // username/password fields
                             [weakSelf didSelectSuggestion];
                           }];
}

- (void)confirmationAlertSecondaryAction {
  // "Use Keyboard" button, which dismisses the bottom sheet.
  [self dismiss];
}

#pragma mark - Private

// Configures the title view of this ViewController.
- (UIView*)setUpTitleView {
  NSString* title = l10n_util::GetNSString(IDS_IOS_PASSWORD_BOTTOM_SHEET_TITLE);
  UIView* titleView = password_manager::CreatePasswordManagerTitleView(title);
  titleView.backgroundColor = [UIColor colorNamed:kPrimaryBackgroundColor];
  titleView.accessibilityLabel = [NSString
      stringWithFormat:@"%@. %@", title,
                       l10n_util::GetNSString(
                           IDS_IOS_PASSWORD_BOTTOM_SHEET_SELECT_PASSWORD)];
  return titleView;
}

// Returns the string to display at a given row in the table view.
- (NSString*)suggestionAtRow:(NSInteger)row {
  NSString* username = [self.delegate usernameAtRow:row];
  return ([username length] == 0)
             ? l10n_util::GetNSString(IDS_IOS_PASSWORD_BOTTOM_SHEET_NO_USERNAME)
             : username;
}

// Creates the password bottom sheet's table view, initially at minimized
// height.
- (UITableView*)createTableView {
  UITableView* tableView = [super createTableView];

  tableView.dataSource = self;
  [tableView registerClass:TableViewURLCell.class
      forCellReuseIdentifier:@"cell"];

  _minimizedHeightConstraint = [tableView.heightAnchor
      constraintEqualToConstant:[self tableViewEstimatedRowHeight] *
                                [self initialNumberOfVisibleCells]];
  _minimizedHeightConstraint.active = YES;

  _fullHeightConstraint = [tableView.heightAnchor
      constraintEqualToConstant:[self tableViewEstimatedRowHeight] *
                                _suggestions.count];
  _fullHeightConstraint.active = NO;

  return tableView;
}

// Loads the favicon associated with the provided cell.
// Defaults to the globe symbol if no URL is associated with the cell.
- (void)loadFaviconAtIndexPath:(NSIndexPath*)indexPath
                       forCell:(UITableViewCell*)cell {
  DCHECK(cell);

  TableViewURLCell* URLCell =
      base::apple::ObjCCastStrict<TableViewURLCell>(cell);
  auto faviconLoadedBlock = ^(FaviconAttributes* attributes) {
    DCHECK(attributes);
    // It doesn't matter which cell the user sees here, all the credentials
    // listed are for the same page and thus share the same favicon.
    [URLCell.faviconView configureWithAttributes:attributes];
  };
  [self.delegate loadFaviconWithBlockHandler:faviconLoadedBlock];
}

// Sets the password bottom sheet's table view to full height.
- (void)expandTableView {
  _minimizedHeightConstraint.active = NO;
  _fullHeightConstraint.active = YES;
  [self.view layoutIfNeeded];
}

// Notifies the delegate that a password suggestion was selected by the user.
- (void)didSelectSuggestion {
  NSInteger index = [self selectedRow];
  [self.delegate didSelectSuggestion:index];

  if (_suggestions.count > 1) {
    base::UmaHistogramCounts100("PasswordManager.TouchToFill.CredentialIndex",
                                (int)index);
  }
}

// Returns whether the provided index path points to the last row of the table
// view.
- (BOOL)isLastRow:(NSIndexPath*)indexPath {
  return NSUInteger(indexPath.row) == (_suggestions.count - 1);
}

// Performs the expand bottom sheet animation.
- (void)expand {
  [self expand:_suggestions.count];
}

// Starting with a table view containing a single suggestion, add all other
// suggestions to the table view.
- (void)addRemainingRowsToTableView:(UITableView*)tableView {
  NSUInteger currentNumberOfRows = [tableView numberOfRowsInSection:0];
  NSUInteger maximumNumberOfRows = _suggestions.count;
  if (maximumNumberOfRows > currentNumberOfRows) {
    [tableView beginUpdates];
    NSMutableArray<NSIndexPath*>* indexPaths = [NSMutableArray
        arrayWithCapacity:maximumNumberOfRows - currentNumberOfRows];
    for (NSUInteger i = currentNumberOfRows; i < maximumNumberOfRows; i++) {
      [indexPaths addObject:[NSIndexPath indexPathForRow:i inSection:0]];
    }
    [tableView insertRowsAtIndexPaths:indexPaths
                     withRowAnimation:UITableViewRowAnimationNone];
    [tableView endUpdates];
  }
}

// Creates the UI action used to open the password manager.
- (UIAction*)openPasswordManagerAction {
  __weak __typeof(self) weakSelf = self;
  void (^passwordManagerButtonTapHandler)(UIAction*) = ^(UIAction* action) {
    // Open Password Manager.
    [weakSelf.delegate disableRefocus];
    [weakSelf.handler displayPasswordManager];
  };
  UIImage* keyIcon =
      CustomSymbolWithPointSize(kPasswordSymbol, kSymbolActionPointSize);
  return [UIAction
      actionWithTitle:l10n_util::GetNSString(
                          IDS_IOS_PASSWORD_BOTTOM_SHEET_PASSWORD_MANAGER)
                image:keyIcon
           identifier:nil
              handler:passwordManagerButtonTapHandler];
}

// Creates the UI action used to open the password details for form suggestion
// at index path.
- (UIAction*)openPasswordDetailsForIndexPath:(NSIndexPath*)indexPath {
  __weak __typeof(self) weakSelf = self;
  FormSuggestion* formSuggestion = [_suggestions objectAtIndex:indexPath.row];
  void (^showDetailsButtonTapHandler)(UIAction*) = ^(UIAction* action) {
    // Open Password Details.
    [weakSelf.delegate disableRefocus];
    [weakSelf.handler displayPasswordDetailsForFormSuggestion:formSuggestion];
  };

  UIImage* infoIcon =
      DefaultSymbolWithPointSize(kInfoCircleSymbol, kSymbolActionPointSize);
  return
      [UIAction actionWithTitle:l10n_util::GetNSString(
                                    IDS_IOS_PASSWORD_BOTTOM_SHEET_SHOW_DETAILS)
                          image:infoIcon
                     identifier:nil
                        handler:showDetailsButtonTapHandler];
}

// Mocks the cells to calculate the real table view height.
- (CGFloat)computeTableViewHeightForCellCount:(NSUInteger)count {
  CGFloat height = 0;
  for (NSUInteger i = 0; i < count; i++) {
    TableViewURLCell* cell = [[TableViewURLCell alloc] init];
    // Setup UI same as real cell.
    cell = [self layoutCell:cell
          forTableViewWidth:[self tableViewWidth]
                atIndexPath:[NSIndexPath indexPathForRow:i inSection:0]];
    CGFloat cellHeight =
        [cell systemLayoutSizeFittingSize:CGSizeMake([self tableViewWidth], 1)]
            .height;
    height += cellHeight;
  }
  return height;
}

// Updates the bottom sheet's height constraints.
- (void)updateHeightConstraints {
  if (_suggestions.count) {
    [self.view layoutIfNeeded];
    // Update height constraints for the table view.
    CGFloat fullHeight =
        [self computeTableViewHeightForCellCount:_suggestions.count];
    if (fullHeight > 0) {
      _fullHeightConstraint.constant = fullHeight;
    }
    CGFloat minimizedHeight = [self
        computeTableViewHeightForCellCount:[self initialNumberOfVisibleCells]];
    if (minimizedHeight > 0) {
      _minimizedHeightConstraint.constant = minimizedHeight;
    }
  }
}

// Layouts the cell for the table view with the password form suggestion at the
// specific index path.
- (TableViewURLCell*)layoutCell:(TableViewURLCell*)cell
              forTableViewWidth:(CGFloat)tableViewWidth
                    atIndexPath:(NSIndexPath*)indexPath {
  cell.selectionStyle = UITableViewCellSelectionStyleNone;

  // Note that both the credentials and URLs will use middle truncation, as it
  // generally makes it easier to differentiate between different ones, without
  // having to resort to displaying multiple lines to show the full username
  // and URL.
  cell.titleLabel.text = [self suggestionAtRow:indexPath.row];
  cell.titleLabel.lineBreakMode = NSLineBreakByTruncatingMiddle;
  cell.URLLabel.text = _domain;
  cell.URLLabel.lineBreakMode = NSLineBreakByTruncatingMiddle;
  cell.URLLabel.hidden = NO;
  cell.accessibilityLabel =
      l10n_util::GetNSStringF(IDS_IOS_AUTOFILL_ACCNAME_SUGGESTION,
                              base::SysNSStringToUTF16(cell.titleLabel.text),
                              base::SysNSStringToUTF16(_domain),
                              base::NumberToString16(indexPath.row + 1),
                              base::NumberToString16(_suggestions.count));

  cell.userInteractionEnabled = YES;

  // Make separator invisible on last cell
  CGFloat separatorLeftMargin =
      (_tableViewIsMinimized || [self isLastRow:indexPath])
          ? tableViewWidth
          : kTableViewHorizontalSpacing;
  cell.separatorInset = UIEdgeInsetsMake(0.f, separatorLeftMargin, 0.f, 0.f);

  [cell
      setFaviconContainerBackgroundColor:
          (self.traitCollection.userInterfaceStyle == UIUserInterfaceStyleDark)
              ? [UIColor colorNamed:kSeparatorColor]
              : [UIColor colorNamed:kPrimaryBackgroundColor]];
  [cell setFaviconContainerBorderColor:UIColor.clearColor];
  cell.titleLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];
  cell.backgroundColor = [UIColor colorNamed:kSecondaryBackgroundColor];

  if (_tableViewIsMinimized && (_suggestions.count > 1)) {
    // The table view is showing a single suggestion and the chevron down
    // symbol, which can be tapped in order to expand the list of suggestions.
    cell.accessoryView = [[UIImageView alloc]
        initWithImage:DefaultSymbolTemplateWithPointSize(
                          kChevronDownSymbol, kSymbolAccessoryPointSize)];
    cell.accessoryView.tintColor = [UIColor colorNamed:kTextQuaternaryColor];
  }
  [self loadFaviconAtIndexPath:indexPath forCell:cell];
  return cell;
}

@end
