// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/ui_bundled/bottom_sheet/credential_suggestion_bottom_sheet_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/feature_list.h"
#import "base/metrics/user_metrics.h"
#import "base/strings/string_number_conversions.h"
#import "base/strings/sys_string_conversions.h"
#import "components/autofill/ios/browser/form_suggestion.h"
#import "components/password_manager/core/browser/features/password_features.h"
#import "components/password_manager/core/browser/ui/credential_ui_entry.h"
#import "components/password_manager/ios/shared_password_controller.h"
#import "components/url_formatter/elide_url.h"
#import "ios/chrome/browser/passwords/ui_bundled/bottom_sheet/credential_suggestion_bottom_sheet_delegate.h"
#import "ios/chrome/browser/passwords/ui_bundled/bottom_sheet/credential_suggestion_bottom_sheet_handler.h"
#import "ios/chrome/browser/settings/ui_bundled/password/create_password_manager_title_view.h"
#import "ios/chrome/browser/shared/ui/bottom_sheet/table_view_bottom_sheet_view_controller+subclassing.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_url_item.h"
#import "ios/chrome/browser/shared/ui/table_view/content_configuration/favicon_content_configuration.h"
#import "ios/chrome/browser/shared/ui/table_view/content_configuration/image_content_configuration.h"
#import "ios/chrome/browser/shared/ui/table_view/content_configuration/table_view_cell_content_configuration.h"
#import "ios/chrome/common/string_util.h"
#import "ios/chrome/common/ui/button_stack/button_stack_configuration.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"
#import "ios/chrome/common/ui/elements/branded_navigation_item_title_view.h"
#import "ios/chrome/common/ui/favicon/favicon_view.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "url/gurl.h"

using autofill::SuggestionType;

namespace {

// Width of the image for suggestion.
CGFloat const kSuggestionImageWidth = 30;

// Returns the username to display for the given `suggestion`.
NSString* GetSuggestionDisplayUsername(FormSuggestion* suggestion) {
  NSString* username = suggestion.value;
  return ([username length] == 0)
             ? l10n_util::GetNSString(
                   IDS_IOS_CREDENTIAL_BOTTOM_SHEET_NO_USERNAME)
             : username;
}

// Logs the relevant metrics for when a password suggestion is accepted from the
// bottom sheet.
void LogSuggestionAcceptedMetrics(BOOL is_backup_suggestion,
                                  NSInteger index,
                                  NSUInteger suggestion_count) {
  const char* backup_string = is_backup_suggestion ? "_Backup" : "";
  std::string user_action =
      base::StrCat({"BottomSheet_Password_SuggestionAccepted", backup_string});
  base::RecordAction(base::UserMetricsAction(user_action.c_str()));

  base::UmaHistogramSparse(
      "Autofill.UserAcceptedSuggestionAtIndex.Password.BottomSheet", index);

  if (suggestion_count > 1) {
    base::UmaHistogramCounts100("PasswordManager.TouchToFill.CredentialIndex",
                                (int)index);
  }
}

}  // namespace

@interface CredentialSuggestionBottomSheetViewController () <
    ConfirmationAlertActionHandler,
    UITableViewDataSource,
    UITableViewDelegate> {
  // List of suggestions in the bottom sheet
  // The property is defined by CredentialSuggestionBottomSheetConsumer
  // protocol.
  NSArray<FormSuggestion*>* _suggestions;

  // The current's page domain. This is used for the credential bottom sheet
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
@property(nonatomic, weak) id<CredentialSuggestionBottomSheetHandler> handler;

// Whether the bottom sheet will be disabled on exit. Default is YES.
@property(nonatomic, assign) BOOL disableBottomSheetOnExit;

@end

@implementation CredentialSuggestionBottomSheetViewController

- (instancetype)initWithHandler:
                    (id<CredentialSuggestionBottomSheetHandler>)handler
                            URL:(const GURL&)URL {
  ButtonStackConfiguration* configuration =
      [[ButtonStackConfiguration alloc] init];
  configuration.secondaryActionString =
      l10n_util::GetNSString(IDS_IOS_CREDENTIAL_BOTTOM_SHEET_USE_KEYBOARD);
  configuration.secondaryActionImage =
      DefaultSymbolWithPointSize(kKeyboardSymbol, kSymbolActionPointSize);
  self = [super initWithConfiguration:configuration];
  if (self) {
    self.handler = handler;
    _URL = URL;
    self.disableBottomSheetOnExit = YES;
  }
  return self;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  self.navigationItem.titleView = [self setUpTitleView];

  // Set the properties read by the super when constructing the
  // views in `-[ConfirmationAlertViewController viewDidLoad]`.
  self.actionHandler = self;

  self.titleString = _title;
  self.titleTextStyle = UIFontTextStyleTitle2;

  if (_subtitle) {
    self.subtitleString = _subtitle;
  } else {
    self.subtitleTextStyle = UIFontTextStyleFootnote;
    std::u16string formattedURL =
        url_formatter::FormatUrlForDisplayOmitSchemePathAndTrivialSubdomains(
            _URL);
    self.subtitleString = l10n_util::GetNSStringF(
        IDS_IOS_CREDENTIAL_BOTTOM_SHEET_SUBTITLE, formattedURL);
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
  [self.handler viewDidDisappear];
}

#pragma mark - Getters

- (NSArray<FormSuggestion*>*)suggestions {
  return _suggestions;
}

#pragma mark - CredentialSuggestionBottomSheetConsumer

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

- (void)setPrimaryActionString:(NSString*)primaryActionString {
  self.configuration.primaryActionString = primaryActionString;
  [self reloadConfiguration];
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

    __typeof(self) strongSelf = weakSelf;
    if (strongSelf) {
      [menuElements
          addObject:[UIMenu menuWithTitle:@""
                                    image:nil
                               identifier:nil
                                  options:UIMenuOptionsDisplayInline
                                 children:@[
                                   [strongSelf openPasswordManagerAction]
                                 ]]];

      // The option to open password details shouldn't be available for recovery
      // passwords as they are not displayed in the Password Manager for now.
      FormSuggestion* formSuggestion =
          [strongSelf.suggestions objectAtIndex:indexPath.row];
      if (formSuggestion.type != SuggestionType::kBackupPasswordEntry) {
        [menuElements
            addObject:[UIMenu
                          menuWithTitle:@""
                                  image:nil
                             identifier:nil
                                options:UIMenuOptionsDisplayInline
                               children:@[ [strongSelf
                                            openPasswordDetailsForSuggestion:
                                                formSuggestion] ]]];
      }
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
  UITableViewCell* cell =
      [TableViewCellContentConfiguration dequeueTableViewCell:tableView];
  return [self layoutCell:cell
        forTableViewWidth:tableView.frame.size.width
              atIndexPath:indexPath];
}

#pragma mark - ConfirmationAlertActionHandler

- (void)confirmationAlertPrimaryAction {
  NSInteger index = [self selectedRow];
  FormSuggestion* suggestion = _suggestions[index];
  BOOL isBackupSuggestion =
      suggestion.type == SuggestionType::kBackupPasswordEntry;

  LogSuggestionAcceptedMetrics(isBackupSuggestion, index, [self rowCount]);

  [self.handler primaryButtonTappedForSuggestion:suggestion atIndex:index];
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
  [TableViewCellContentConfiguration registerCellForTableView:tableView];

  return tableView;
}

- (NSUInteger)rowCount {
  return _suggestions.count;
}

- (CGFloat)computeTableViewCellHeightAtIndex:(NSUInteger)index {
  UITableViewCell* cell = [[UITableViewCell alloc] init];
  // Setup UI same as real cell.
  CGFloat tableWidth = [self tableViewWidth];
  cell = [self layoutCell:cell
        forTableViewWidth:tableWidth
              atIndexPath:[NSIndexPath indexPathForRow:index inSection:0]];
  return [cell systemLayoutSizeFittingSize:CGSizeMake(tableWidth, 1)].height;
}

#pragma mark - UIResponder

- (BOOL)canBecomeFirstResponder {
  // Allow the sheet to become a first responder to not allow the keyboard
  // popping over the sheet when there is a focus event on the WebView
  // underneath the sheet.
  return YES;
}

#pragma mark - Private

// Configures the title view of this ViewController.
- (UIView*)setUpTitleView {
  NSString* title =
      l10n_util::GetNSString(IDS_IOS_CREDENTIAL_BOTTOM_SHEET_TITLE);
  UIView* titleView = password_manager::CreatePasswordManagerTitleView(title);
  titleView.accessibilityLabel = [NSString
      stringWithFormat:@"%@. %@", title,
                       l10n_util::GetNSString(
                           IDS_IOS_CREDENTIAL_BOTTOM_SHEET_SELECT_PASSWORD)];
  return titleView;
}

// Loads the favicon associated with the provided cell.
// Defaults to the globe symbol if no URL is associated with the cell.
// In case of a recovery password suggestion, the favicon is replaced by a
// symbol.
- (void)loadFaviconForConfiguration:
            (TableViewCellContentConfiguration*)configuration
           associatedWithSuggestion:(FormSuggestion*)suggestion
                        atIndexPath:(NSIndexPath*)indexPath {
  if (suggestion.icon) {
    ImageContentConfiguration* imageConfiguration =
        [[ImageContentConfiguration alloc] init];
    imageConfiguration.imageSize =
        CGSizeMake(kSuggestionImageWidth, kSuggestionImageWidth);
    imageConfiguration.image = suggestion.icon;

    configuration.leadingConfiguration = imageConfiguration;
    return;
  }
  __weak __typeof(self) weakSelf = self;
  auto faviconLoadedBlock = ^(FaviconAttributes* attributes, bool cached) {
    DCHECK(attributes);
    if (cached) {
      FaviconContentConfiguration* faviconConfiguration =
          [[FaviconContentConfiguration alloc] init];
      faviconConfiguration.faviconAttributes = attributes;

      configuration.leadingConfiguration = faviconConfiguration;
    } else if (attributes.faviconImage) {
      [weakSelf reconfigureCellAtIndexPath:indexPath];
    }
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
                          IDS_IOS_CREDENTIAL_BOTTOM_SHEET_PASSWORD_MANAGER)
                image:keyIcon
           identifier:nil
              handler:passwordManagerButtonTapHandler];
}

// Creates the UI action used to open the password details for the given
// `formSuggestion`.
- (UIAction*)openPasswordDetailsForSuggestion:(FormSuggestion*)formSuggestion {
  __weak __typeof(self) weakSelf = self;
  void (^showDetailsButtonTapHandler)(UIAction*) = ^(UIAction* action) {
    // Open Password Details.
    weakSelf.disableBottomSheetOnExit = NO;
    [weakSelf.handler displayPasswordDetailsForFormSuggestion:formSuggestion];
  };

  UIImage* infoIcon =
      DefaultSymbolWithPointSize(kInfoCircleSymbol, kSymbolActionPointSize);
  return [UIAction
      actionWithTitle:l10n_util::GetNSString(
                          IDS_IOS_CREDENTIAL_BOTTOM_SHEET_SHOW_DETAILS)
                image:infoIcon
           identifier:nil
              handler:showDetailsButtonTapHandler];
}

// Returns the accessibility value for the cell at the provided index path.
- (NSString*)cellAccessibilityValueAtIndexPath:(NSIndexPath*)indexPath {
  return l10n_util::GetNSStringF(IDS_IOS_AUTOFILL_SUGGESTION_INDEX_VALUE,
                                 base::NumberToString16(indexPath.row + 1),
                                 base::NumberToString16([self rowCount]));
}

// Lays out the cell for the table view with the credential form suggestion at
// the specific index path.
- (UITableViewCell*)layoutCell:(UITableViewCell*)cell
             forTableViewWidth:(CGFloat)tableViewWidth
                   atIndexPath:(NSIndexPath*)indexPath {
  CHECK(_suggestions.count);
  FormSuggestion* formSuggestion = [_suggestions objectAtIndex:indexPath.row];

  TableViewCellContentConfiguration* configuration =
      [[TableViewCellContentConfiguration alloc] init];
  configuration.title = GetSuggestionDisplayUsername(formSuggestion);
  configuration.titleNumberOfLines = 1;
  configuration.titleLineBreakMode = NSLineBreakByTruncatingMiddle;
  configuration.subtitle = _domain;
  configuration.subtitleNumberOfLines = 1;
  configuration.subtitleLineBreakMode = NSLineBreakByTruncatingMiddle;
  // Note that both the credentials and URLs will use middle truncation, as it
  // generally makes it easier to differentiate between different ones, without
  // having to resort to displaying multiple lines to show the full username
  // and URL.
  if (formSuggestion.type == SuggestionType::kBackupPasswordEntry) {
    configuration.secondSubtitle = l10n_util::GetNSString(
        IDS_IOS_CREDENTIAL_BOTTOM_SHEET_RECOVERY_PASSWORD_LABEL);
  }

  [self loadFaviconForConfiguration:configuration
           associatedWithSuggestion:formSuggestion
                        atIndexPath:indexPath];

  cell.contentConfiguration = configuration;

  cell.accessibilityIdentifier = configuration.title;

  cell.accessibilityValue = [self cellAccessibilityValueAtIndexPath:indexPath];
  cell.accessoryType = [self accessoryType:indexPath];
  cell.selectionStyle = UITableViewCellSelectionStyleNone;

  cell.backgroundColor = [UIColor colorNamed:kSecondaryBackgroundColor];

  return cell;
}

@end
