// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/passwords/bottom_sheet/password_suggestion_bottom_sheet_view_controller.h"

#import "base/mac/foundation_util.h"
#import "base/memory/raw_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "components/autofill/ios/browser/form_suggestion.h"
#import "components/password_manager/core/browser/password_ui_utils.h"
#import "components/password_manager/core/browser/ui/credential_ui_entry.h"
#import "components/password_manager/ios/shared_password_controller.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_url_item.h"
#import "ios/chrome/browser/shared/ui/table_view/chrome_table_view_controller.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/passwords/bottom_sheet/password_suggestion_bottom_sheet_constants.h"
#import "ios/chrome/browser/ui/passwords/bottom_sheet/password_suggestion_bottom_sheet_delegate.h"
#import "ios/chrome/browser/ui/passwords/bottom_sheet/password_suggestion_bottom_sheet_handler.h"
#import "ios/chrome/browser/ui/settings/password/branded_navigation_item_title_view.h"
#import "ios/chrome/browser/ui/settings/password/create_password_manager_title_view.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/favicon/favicon_attributes.h"
#import "ios/chrome/common/ui/favicon/favicon_view.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"
#import "ios/chrome/grit/ios_google_chrome_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Estimated base height value for the bottom sheet without the table view.
CGFloat const kEstimatedBaseHeightForBottomSheet = 195;

// Sets a custom radius for the half sheet presentation.
CGFloat const kHalfSheetCornerRadius = 20;

// Estimated row height for each cell in the table view.
CGFloat const kTableViewEstimatedRowHeight = 75;

// Radius size of the table view.
CGFloat const kTableViewCornerRadius = 10;

// TableView's width constraint multiplier in Portrait mode for iPhone only.
CGFloat const kPortraitIPhoneTableViewWidthMultiplier = 0.95;

// TableView's width constraint multiplier in all mode (except iPhone Portrait).
CGFloat const kTableViewWidthMultiplier = 0.65;

// Scroll view's bottom anchor constant.
CGFloat const kScrollViewBottomAnchorConstant = 10;

// Initial height's extra bottom height padding so it does not crop the cell.
CGFloat const kInitialHeightPadding = 5;

}  // namespace

@interface PasswordSuggestionBottomSheetViewController () <
    ConfirmationAlertActionHandler,
    UIGestureRecognizerDelegate,
    UITableViewDataSource,
    UITableViewDelegate> {
  // If YES: the table view is currently showing a single suggestion
  // If NO: the table view is currently showing all suggestions
  BOOL _tableViewIsMinimized;

  // Height constraint for the bottom sheet when showing a single suggestion.
  NSLayoutConstraint* _minimizedHeightConstraint;

  // Height constraint for the bottom sheet when showing all suggestions.
  NSLayoutConstraint* _fullHeightConstraint;

  // Table view for the list of suggestions.
  UITableView* _tableView;

  // TableView's width constraint in portrait mode.
  NSLayoutConstraint* _portraitTableWidthConstraint;

  // TableView's width constraint in landscape mode.
  NSLayoutConstraint* _landscapeTableWidthConstraint;

  // List of suggestions in the bottom sheet
  // The property is defined by PasswordSuggestionBottomSheetConsumer protocol.
  NSArray<FormSuggestion*>* _suggestions;

  // The current's page domain. This is used for the password bottom sheet
  // description label.
  NSString* _domain;
}

// The password controller handler used to open the password manager.
@property(nonatomic, weak) id<PasswordSuggestionBottomSheetHandler> handler;

@end

@implementation PasswordSuggestionBottomSheetViewController

- (instancetype)initWithHandler:
    (id<PasswordSuggestionBottomSheetHandler>)handler {
  self = [super init];
  if (self) {
    self.handler = handler;
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
  self.customSpacing = 0;
  self.customSpacingAfterImage = 0;
  self.titleTextStyle = UIFontTextStyleTitle2;
  self.topAlignedLayout = YES;
  self.actionHandler = self;
  self.scrollEnabled = NO;
  self.customScrollViewBottomInsets = 0;

  [self updateCustomGradientViewHeight:0];

  self.primaryActionString =
      l10n_util::GetNSString(IDS_IOS_PASSWORD_BOTTOM_SHEET_USE_PASSWORD);
  self.secondaryActionString =
      l10n_util::GetNSString(IDS_IOS_PASSWORD_BOTTOM_SHEET_NO_THANKS);

  [super viewDidLoad];

  // Assign table view's width anchor now that it is in the same hierarchy as
  // the top view.
  [self createTableViewWidthConstraint:self.view.layoutMarginsGuide];

  [self setUpBottomSheet];
}

- (void)viewWillTransitionToSize:(CGSize)size
       withTransitionCoordinator:
           (id<UIViewControllerTransitionCoordinator>)coordinator {
  [super viewWillTransitionToSize:size withTransitionCoordinator:coordinator];
  [self adjustTableViewWidthConstraint];
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

- (void)viewWillAppear:(BOOL)animated {
  // Update height constraints for the table view.
  [self.view layoutIfNeeded];
  CGFloat minimizedTableViewHeight = _tableView.contentSize.height;
  if (minimizedTableViewHeight > 0 &&
      minimizedTableViewHeight != kTableViewEstimatedRowHeight) {
    _minimizedHeightConstraint.constant = minimizedTableViewHeight;
    _fullHeightConstraint.constant =
        minimizedTableViewHeight * _suggestions.count;
  }

  // Update the custom detent with the correct initial height for the bottom
  // sheet. (Initial height is not calculated properly in -viewDidLoad, but we
  // need to setup the bottom sheet in that method so there is not a delay when
  // showing the table view and the action buttons.
  UISheetPresentationController* presentationController =
      self.sheetPresentationController;
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
  }
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
  if (_suggestions.count <= 1) {
    return;
  }

  if (_tableViewIsMinimized) {
    _tableViewIsMinimized = NO;
    [_tableView cellForRowAtIndexPath:indexPath].accessoryView = nil;
    // Make separator visible on first cell.
    [_tableView cellForRowAtIndexPath:indexPath].separatorInset =
        UIEdgeInsetsMake(0.f, kTableViewHorizontalSpacing, 0.f, 0.f);
    [self addSuggestionsToTableView];

    // Update table view height.
    __weak __typeof(self) weakSelf = self;
    [UIView animateWithDuration:0.1
                     animations:^{
                       [weakSelf expandTableView];
                     }];

    [self expand];
  }

  [_tableView cellForRowAtIndexPath:indexPath].accessoryType =
      UITableViewCellAccessoryCheckmark;
}

- (void)tableView:(UITableView*)tableView
    didDeselectRowAtIndexPath:(NSIndexPath*)indexPath {
  [_tableView cellForRowAtIndexPath:indexPath].accessoryType =
      UITableViewCellAccessoryNone;
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
          [menuElements
              addObject:[strongSelf openPasswordDetailsForIndexPath:indexPath]];
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
  cell.URLLabel.text = _domain;
  cell.URLLabel.lineBreakMode = NSLineBreakByTruncatingMiddle;
  cell.URLLabel.hidden = NO;

  cell.userInteractionEnabled = YES;

  // Make separator invisible on last cell
  CGFloat separatorLeftMargin =
      (_tableViewIsMinimized || [self isLastRow:indexPath])
          ? _tableView.bounds.size.width
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

#pragma mark - ConfirmationAlertActionHandler

- (void)confirmationAlertPrimaryAction {
  // Use password button
  [self.delegate willSelectSuggestion:_tableView.indexPathForSelectedRow.row];
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
  NSString* title = l10n_util::GetNSString(IDS_IOS_PASSWORD_BOTTOM_SHEET_TITLE);
  UIView* titleView = password_manager::CreatePasswordManagerTitleView(title);
  titleView.backgroundColor = [UIColor colorNamed:kPrimaryBackgroundColor];
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
  CGRect frame = [[UIScreen mainScreen] bounds];
  _tableView = [[UITableView alloc] initWithFrame:frame
                                            style:UITableViewStylePlain];

  _tableView.layer.cornerRadius = kTableViewCornerRadius;
  _tableView.estimatedRowHeight = kTableViewEstimatedRowHeight;
  _tableView.scrollEnabled = NO;
  _tableView.showsVerticalScrollIndicator = NO;
  _tableView.delegate = self;
  _tableView.dataSource = self;
  _tableView.userInteractionEnabled = YES;
  _tableView.accessibilityIdentifier =
      kPasswordSuggestionBottomSheetTableViewId;
  [_tableView registerClass:TableViewURLCell.class
      forCellReuseIdentifier:@"cell"];

  _minimizedHeightConstraint = [_tableView.heightAnchor
      constraintEqualToConstant:kTableViewEstimatedRowHeight];
  _minimizedHeightConstraint.active = YES;

  _fullHeightConstraint = [_tableView.heightAnchor
      constraintEqualToConstant:kTableViewEstimatedRowHeight *
                                _suggestions.count];
  _fullHeightConstraint.active = NO;

  _tableView.translatesAutoresizingMaskIntoConstraints = NO;

  [_tableView selectRowAtIndexPath:[NSIndexPath indexPathForRow:0 inSection:0]
                          animated:NO
                    scrollPosition:UITableViewScrollPositionNone];

  return _tableView;
}

// Creates the tableview's width constraints and set their initial active state.
- (void)createTableViewWidthConstraint:(UILayoutGuide*)margins {
  UIUserInterfaceIdiom idiom = [[UIDevice currentDevice] userInterfaceIdiom];
  _portraitTableWidthConstraint = [_tableView.widthAnchor
      constraintGreaterThanOrEqualToAnchor:margins.widthAnchor
                                multiplier:
                                    (idiom == UIUserInterfaceIdiomPad)
                                        ? kTableViewWidthMultiplier
                                        : kPortraitIPhoneTableViewWidthMultiplier];
  _landscapeTableWidthConstraint = [_tableView.widthAnchor
      constraintGreaterThanOrEqualToAnchor:margins.widthAnchor
                                multiplier:kTableViewWidthMultiplier];
  [self adjustTableViewWidthConstraint];
}

// Change the tableview's width constraint based on the screen's orientation.
- (void)adjustTableViewWidthConstraint {
  BOOL isLandscape =
      UIDeviceOrientationIsLandscape([UIDevice currentDevice].orientation);
  _landscapeTableWidthConstraint.active = isLandscape;
  _portraitTableWidthConstraint.active = !isLandscape;
}

// Loads the favicon associated with the provided cell.
// Defaults to the globe symbol if no URL is associated with the cell.
- (void)loadFaviconAtIndexPath:(NSIndexPath*)indexPath
                       forCell:(UITableViewCell*)cell {
  DCHECK(cell);

  TableViewURLCell* URLCell = base::mac::ObjCCastStrict<TableViewURLCell>(cell);
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
  [self.delegate didSelectSuggestion:_tableView.indexPathForSelectedRow.row];
}

// Returns whether the provided index path points to the last row of the table
// view.
- (BOOL)isLastRow:(NSIndexPath*)indexPath {
  return NSUInteger(indexPath.row) == (_suggestions.count - 1);
}

// Returns the height of the bottom sheet view.
- (CGFloat)bottomSheetHeight {
  return
      [self.view
          systemLayoutSizeFittingSize:CGSizeMake(self.view.frame.size.width, 1)]
          .height;
}

// Returns the initial height of the bottom sheet while showing a single row.
- (CGFloat)initialHeight {
  CGFloat bottomSheetHeight = [self bottomSheetHeight];
  if (bottomSheetHeight > 0) {
    return bottomSheetHeight + kInitialHeightPadding;
  }
  // Return an estimated height if we can't calculate the actual height.
  return kEstimatedBaseHeightForBottomSheet + kTableViewEstimatedRowHeight;
}

// Returns the desired height for the bottom sheet (can be larger than the
// screen).
- (CGFloat)fullHeight {
  CGFloat bottomSheetHeight = [self bottomSheetHeight];
  if (bottomSheetHeight > 0) {
    return bottomSheetHeight;
  }

  // Return an estimated height for the bottom sheet while showing all rows
  // (using estimated heights).
  return kEstimatedBaseHeightForBottomSheet +
         (kTableViewEstimatedRowHeight * _suggestions.count);
}

// Enables scrolling of the table view
- (void)setTableViewScrollEnabled:(BOOL)enabled {
  _tableView.scrollEnabled = enabled;
  self.scrollEnabled = enabled;

  // Add gradient view to show that the user can scroll.
  if (enabled) {
    [self updateCustomGradientViewHeight:16];
  }
}

// Performs the expand bottom sheet animation.
- (void)expand {
  UISheetPresentationController* presentationController =
      self.sheetPresentationController;
  if (@available(iOS 16, *)) {
    // Update the bottom anchor constant value only for iPhone.
    if ([UIDevice currentDevice].userInterfaceIdiom ==
        UIUserInterfaceIdiomPhone) {
      [self
          changeScrollViewBottomAnchorConstant:kScrollViewBottomAnchorConstant];
    }

    // Expand to custom size (only available for iOS 16+).
    CGFloat fullHeight = [self fullHeight];

    __weak __typeof(self) weakSelf = self;
    auto fullHeightBlock = ^CGFloat(
        id<UISheetPresentationControllerDetentResolutionContext> context) {
      BOOL tooLarge = (fullHeight > context.maximumDetentValue);
      [weakSelf setTableViewScrollEnabled:tooLarge];
      if (tooLarge) {
        // Reset bottom anchor constant value so there is enough space for the
        // gradient view.
        [self resetScrollViewBottomAnchorConstant];
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
    [self setTableViewScrollEnabled:YES];
    [presentationController animateChanges:^{
      presentationController.selectedDetentIdentifier =
          UISheetPresentationControllerDetentIdentifierLarge;
    }];
  }
}

// Starting with a table view containing a single suggestion, add all other
// suggestions to the table view.
- (void)addSuggestionsToTableView {
  [_tableView beginUpdates];
  NSMutableArray<NSIndexPath*>* indexPaths =
      [NSMutableArray arrayWithCapacity:_suggestions.count - 1];
  for (NSUInteger i = 1; i < _suggestions.count; i++) {
    [indexPaths addObject:[NSIndexPath indexPathForRow:i inSection:0]];
  }
  [_tableView insertRowsAtIndexPaths:indexPaths
                    withRowAnimation:UITableViewRowAnimationNone];
  [_tableView endUpdates];
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

@end
