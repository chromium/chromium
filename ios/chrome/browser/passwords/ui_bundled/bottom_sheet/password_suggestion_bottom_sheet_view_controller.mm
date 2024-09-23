// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/ui_bundled/bottom_sheet/password_suggestion_bottom_sheet_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/metrics/user_metrics.h"
#import "base/strings/sys_string_conversions.h"
#import "components/autofill/ios/browser/form_suggestion.h"
#import "components/password_manager/core/browser/ui/credential_ui_entry.h"
#import "components/password_manager/ios/shared_password_controller.h"
#import "components/url_formatter/elide_url.h"
#import "ios/chrome/browser/passwords/ui_bundled/bottom_sheet/password_suggestion_bottom_sheet_delegate.h"
#import "ios/chrome/browser/passwords/ui_bundled/bottom_sheet/password_suggestion_bottom_sheet_handler.h"
#import "ios/chrome/browser/shared/ui/bottom_sheet/table_view_bottom_sheet_view_controller+subclassing.h"
#import "ios/chrome/browser/shared/ui/elements/branded_navigation_item_title_view.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_url_item.h"
#import "ios/chrome/browser/ui/settings/password/create_password_manager_title_view.h"
#import "ios/chrome/common/string_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"
#import "ios/chrome/common/ui/favicon/favicon_view.h"
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

// Whether the bottom sheet will be disabled on exit. Default is YES.
@property(nonatomic, assign) BOOL disableBottomSheetOnExit;

@end

@implementation PasswordSuggestionBottomSheetViewController

- (instancetype)initWithHandler:
                    (id<PasswordSuggestionBottomSheetHandler>)handler
                            URL:(const GURL&)URL {
  self = [super init];
  if (self) {
    self.handler = handler;
    _URL = URL;
    self.disableBottomSheetOnExit = YES;
  }
  return self;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  // Image needs to be above title view, which is the case only when the latter
  // is a `titleView`. In more common case without the image, title should be an
  // `aboveTitleView`.
  if (self.image) {
    self.titleView = [self setUpTitleView];
    self.customSpacing = 0;
  } else {
    self.aboveTitleView = [self setUpTitleView];
    self.customSpacing = kSpacingAfterTitle;
    self.customSpacingBeforeImageIfNoNavigationBar = kSpacingBeforeTitle;
  }

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

  [self adjustTransactionsPrimaryActionButtonHorizontalConstraints];
}

- (void)viewDidAppear:(BOOL)animated {
  [super viewDidAppear:animated];
  UIAccessibilityPostNotification(UIAccessibilityAnnouncementNotification,
                                  self.aboveTitleView.accessibilityLabel);
}

- (void)viewDidDisappear:(BOOL)animated {
  [super viewDidDisappear:animated];
  if (self.disableBottomSheetOnExit) {
    [self.delegate disableBottomSheet];
  }
  [self.handler viewDidDisappear];
}

#pragma mark - PasswordSuggestionBottomSheetConsumer

- (void)setSuggestions:(NSArray<FormSuggestion*>*)suggestions
             andDomain:(NSString*)domain {
  BOOL requiresUpdate = (_suggestions != nil);
  _suggestions = suggestions;
  _domain = domain;
  if (requiresUpdate) {
    [self reloadTableViewData];
  }
}

- (void)setTitle:(NSString*)title subtitle:(NSString*)subtitle {
  _title = title;
  _subtitle = subtitle;
}

- (void)setAvatarImage:(UIImage*)avatarImage {
  self.image = avatarImage;
}

- (void)dismiss {
  [self dismissViewControllerAnimated:NO completion:NULL];
}

#pragma mark - UITableViewDelegate

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
  return [self rowCount];
}

- (NSInteger)numberOfSectionsInTableView:(UITableView*)tableView {
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
  base::RecordAction(
      base::UserMetricsAction("BottomSheet_Password_SuggestionAccepted"));
  NSInteger index = [self selectedRow];
  base::UmaHistogramSparse(
      "Autofill.UserAcceptedSuggestionAtIndex.Password.BottomSheet", index);
  [self.handler primaryButtonTappedForSuggestion:_suggestions[index]
                                         atIndex:index];

  if ([self rowCount] > 1) {
    base::UmaHistogramCounts100("PasswordManager.TouchToFill.CredentialIndex",
                                (int)index);
  }
}

- (void)confirmationAlertSecondaryAction {
  [self.handler secondaryButtonTapped];
}

#pragma mark - ConfirmationAlertViewController

- (void)customizeSubtitle:(UITextView*)subtitle {
  if (_subtitle) {
    subtitle.attributedText =
        PutBoldPartInString(_subtitle, UIFontTextStyleBody);
    subtitle.textAlignment = NSTextAlignmentCenter;
    // Setting `attributedText` overrides `textColor` set in the parent class,
    // which does not render visibly in dark mode.
    subtitle.textColor = [UIColor colorNamed:kTextSecondaryColor];
  }
}

#pragma mark - TableViewBottomSheetViewController

- (UITableView*)createTableView {
  UITableView* tableView = [super createTableView];

  tableView.dataSource = self;
  [tableView registerClass:TableViewURLCell.class
      forCellReuseIdentifier:@"cell"];

  return tableView;
}

- (NSUInteger)rowCount {
  return _suggestions.count;
}

- (CGFloat)computeTableViewCellHeightAtIndex:(NSUInteger)index {
  TableViewURLCell* cell = [[TableViewURLCell alloc] init];
  // Setup UI same as real cell.
  CGFloat tableWidth = [self tableViewWidth];
  cell = [self layoutCell:cell
        forTableViewWidth:tableWidth
              atIndexPath:[NSIndexPath indexPathForRow:index inSection:0]];
  return [cell systemLayoutSizeFittingSize:CGSizeMake(tableWidth, 1)].height;
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

// Creates the UI action used to open the password manager.
- (UIAction*)openPasswordManagerAction {
  __weak __typeof(self) weakSelf = self;
  void (^passwordManagerButtonTapHandler)(UIAction*) = ^(UIAction* action) {
    // Open Password Manager.
    weakSelf.disableBottomSheetOnExit = NO;
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
    weakSelf.disableBottomSheetOnExit = NO;
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

// Returns the accessibility label for the given cell.
- (NSString*)cellAccessibilityLabel:(TableViewURLCell*)cell {
  return l10n_util::GetNSStringF(IDS_IOS_AUTOFILL_ACCNAME_SUGGESTION,
                                 base::SysNSStringToUTF16(cell.titleLabel.text),
                                 base::SysNSStringToUTF16(_domain));
}

// Returns the accessibility value for the cell at the provided index path.
- (NSString*)cellAccessibilityValueAtIndexPath:(NSIndexPath*)indexPath {
  return l10n_util::GetNSStringF(IDS_IOS_AUTOFILL_SUGGESTION_INDEX_VALUE,
                                 base::NumberToString16(indexPath.row + 1),
                                 base::NumberToString16([self rowCount]));
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
  cell.titleLabel.numberOfLines = 1;
  cell.URLLabel.text = _domain;
  cell.URLLabel.lineBreakMode = NSLineBreakByTruncatingMiddle;
  cell.URLLabel.numberOfLines = 1;
  cell.URLLabel.hidden = NO;
  cell.accessibilityLabel = [self cellAccessibilityLabel:cell];
  cell.accessibilityValue = [self cellAccessibilityValueAtIndexPath:indexPath];
  cell.separatorInset = [self separatorInsetForTableViewWidth:tableViewWidth
                                                  atIndexPath:indexPath];
  cell.accessoryType = [self accessoryType:indexPath];

  [cell
      setFaviconContainerBackgroundColor:
          (self.traitCollection.userInterfaceStyle == UIUserInterfaceStyleDark)
              ? [UIColor colorNamed:kSeparatorColor]
              : [UIColor colorNamed:kPrimaryBackgroundColor]];
  [cell setFaviconContainerBorderColor:UIColor.clearColor];
  cell.titleLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];
  cell.backgroundColor = [UIColor colorNamed:kSecondaryBackgroundColor];

  [self loadFaviconAtIndexPath:indexPath forCell:cell];
  return cell;
}

@end
