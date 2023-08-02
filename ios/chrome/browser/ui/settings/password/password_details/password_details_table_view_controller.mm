// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_details/password_details_table_view_controller.h"

#import "base/ios/ios_util.h"
#import "base/mac/foundation_util.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/histogram_macros.h"
#import "base/strings/sys_string_conversions.h"
#import "components/password_manager/core/browser/password_manager_metrics_util.h"
#import "components/password_manager/core/common/password_manager_constants.h"
#import "components/password_manager/core/common/password_manager_features.h"
#import "components/sync/base/features.h"
#import "ios/chrome/browser/passwords/password_checkup_metrics.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_multi_line_text_edit_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_multi_line_text_edit_item_delegate.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_button_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_edit_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_edit_item_delegate.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_item.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/shared/ui/util/pasteboard_util.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/settings/cells/settings_image_detail_text_item.h"
#import "ios/chrome/browser/ui/settings/password/password_details/cells/table_view_stacked_details_item.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details_consumer.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details_handler.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details_menu_item.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details_metrics_utils.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details_table_view_constants.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details_table_view_controller+private.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details_table_view_controller_delegate.h"
#import "ios/chrome/browser/ui/settings/password/passwords_table_view_constants.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/elements/popover_label_view_controller.h"
#import "ios/chrome/common/ui/reauthentication/reauthentication_module.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"
#import "ios/chrome/grit/ios_chromium_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

using base::UmaHistogramEnumeration;
using password_manager::GetWarningTypeForDetailsContext;
using password_manager::constants::kMaxPasswordNoteLength;
using password_manager::features::IsPasswordCheckupEnabled;
using password_manager::metrics_util::LogPasswordNoteActionInSettings;
using password_manager::metrics_util::LogPasswordSettingsReauthResult;
using password_manager::metrics_util::PasswordNoteAction;
using password_manager::metrics_util::ReauthResult;

namespace {

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierPassword = kSectionIdentifierEnumZero,
  SectionIdentifierSite,
  SectionIdentifierCompromisedInfo,
  SectionIdentifierMoveToAccount,
};

typedef NS_ENUM(NSInteger, ReauthenticationReason) {
  ReauthenticationReasonShow = 0,
  ReauthenticationReasonCopy,
  ReauthenticationReasonEdit,
};

bool IsPasswordNotesWithBackupEnabled() {
  return base::FeatureList::IsEnabled(syncer::kPasswordNotesWithBackup);
}

bool IsSendingPasswordsEnabled() {
  return base::FeatureList::IsEnabled(
      password_manager::features::kSendPasswords);
}

// Size of the symbols.
const CGFloat kSymbolSize = 15;
const CGFloat kRecommendationSymbolSize = 22;
// Minimal amount of characters in password note to display the warning.
const int kMinNoteCharAmountForWarning = 901;

// Returns true if the "Dismiss Warning" button should be shown.
bool ShouldAllowToDismissWarning(DetailsContext context, bool is_compromised) {
  switch (context) {
    case DetailsContext::kPasswordSettings:
    case DetailsContext::kOutsideSettings:
    case DetailsContext::kCompromisedIssues:
    case DetailsContext::kDismissedWarnings:
      return IsPasswordCheckupEnabled() && is_compromised;
    case DetailsContext::kReusedIssues:
    case DetailsContext::kWeakIssues:
      return false;
  }
}

// Returns true if the "Restore Warning" button should be shown.
bool ShouldAllowToRestoreWarning(DetailsContext context, bool is_muted) {
  switch (context) {
    case DetailsContext::kPasswordSettings:
    case DetailsContext::kOutsideSettings:
    case DetailsContext::kCompromisedIssues:
    case DetailsContext::kReusedIssues:
    case DetailsContext::kWeakIssues:
      return false;
    case DetailsContext::kDismissedWarnings:
      return IsPasswordCheckupEnabled() && is_muted;
  }
}

}  // namespace

#pragma mark - PasswordDetailsInfoItem

// Contains the website, username and password text items.
@interface PasswordDetailsInfoItem : NSObject

// Displays one or more websites on which this credential is used.
@property(nonatomic, strong) TableViewStackedDetailsItem* websiteItem;

// The text item related to the username value.
@property(nonatomic, strong) TableViewTextEditItem* usernameTextItem;

// The text item related to the password value.
@property(nonatomic, strong) TableViewTextEditItem* passwordTextItem;

// The text item related to the password note.
@property(nonatomic, strong) TableViewMultiLineTextEditItem* passwordNoteItem;

// If yes, the footer informing about the max note length is shown.
@property(nonatomic, assign) BOOL isNoteFooterShown;

@end
@implementation PasswordDetailsInfoItem
@end

#pragma mark - PasswordDetailsTableViewController

@interface PasswordDetailsTableViewController () <
    TableViewTextEditItemDelegate,
    TableViewMultiLineTextEditItemDelegate> {
  // Index of the password the user wants to reveal.
  NSInteger _passwordIndexToReveal;

  // Title label displayed in the navigation bar.
  UILabel* _titleLabel;
}

// Array of passwords that are shown on the screen.
@property(nonatomic, strong) NSArray<PasswordDetails*>* passwords;

@property(nonatomic, strong) NSString* pageTitle;

// Whether the password is shown in plain text form or in masked form.
@property(nonatomic, assign, getter=isPasswordShown) BOOL passwordShown;

// Array of the password details info items used by the table view model.
@property(nonatomic, strong)
    NSMutableArray<PasswordDetailsInfoItem*>* passwordDetailsInfoItems;

// The view used to anchor error alert which is shown for the username. This is
// image icon in the `usernameTextItem` cell.
@property(nonatomic, weak) UIView* usernameErrorAnchorView;

// Denotes that the Done button in editing mode can be enabled after
// basic validation of data on all the fields. Does not account for whether the
// duplicate credential exists or not.
@property(nonatomic, assign) BOOL shouldEnableEditDoneButton;

// If YES, the password details are shown without requiring any authentication.
@property(nonatomic, assign) BOOL showPasswordWithoutAuth;

// YES if this is the details view for a blocked site (never saved password).
@property(nonatomic, assign) BOOL isBlockedSite;

// Stores the signed in user email, or the empty string if the user is not
// signed-in.
@property(nonatomic, readonly) NSString* userEmail;

// Timer used to keep track of the time that passed after user passed the
// authentication and navigated to the details view. Once it runs out, view
// navigates to the password list view.
@property(nonatomic, strong) NSTimer* authValidityTimer;

// Used to avoid recording the "move to account offered" histogram twice for
// the same credential.
@property(nonatomic, strong)
    NSMutableSet<NSString*>* usernamesWithMoveToAccountOfferRecorded;

@end

@implementation PasswordDetailsTableViewController

#pragma mark - ViewController Life Cycle.

- (instancetype)init {
  self = [super initWithStyle:ChromeTableViewStyle()];
  if (self) {
    _shouldEnableEditDoneButton = NO;
    _showPasswordWithoutAuth = NO;
    _passwordIndexToReveal = 0;

    _titleLabel = [[UILabel alloc] init];
    _titleLabel.lineBreakMode = NSLineBreakByTruncatingHead;
    _titleLabel.font =
        [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline];
    _titleLabel.adjustsFontForContentSizeCategory = YES;
    self.usernamesWithMoveToAccountOfferRecorded = [[NSMutableSet alloc] init];
  }
  return self;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  self.tableView.accessibilityIdentifier = kPasswordDetailsViewControllerId;
  self.tableView.allowsSelectionDuringEditing = YES;
  if (IsSendingPasswordsEnabled()) {
    UIBarButtonItem* shareButton = [[UIBarButtonItem alloc]
        initWithImage:DefaultSymbolWithPointSize(kShareSymbol,
                                                 kSymbolActionPointSize)
                style:UIBarButtonItemStylePlain
               target:self
               action:@selector(onShareButtonPressed)];
    self.navigationItem.rightBarButtonItems =
        @[ self.navigationItem.rightBarButtonItem, shareButton ];
  }

  [self setOrExtendAuthValidityTimer];
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];
  // Title may change between the call to -init and -viewWillAppear, so we want
  // to wait until the last moment possible before setting the titleView.
  self.navigationItem.titleView = _titleLabel;
}

- (void)viewDidDisappear:(BOOL)animated {
  [self.handler passwordDetailsTableViewControllerWasDismissed];
  [super viewDidDisappear:animated];
}

#pragma mark - ChromeTableViewController

- (void)editButtonPressed {
  [self setOrExtendAuthValidityTimer];
  // If there are no passwords, proceed with editing without
  // reauthentication.
  if (![self hasAtLeastOnePassword]) {
    [super editButtonPressed];

    // Reload view to show the delete button.
    [self reloadData];
    return;
  }

  // Request reauthentication before revealing password during editing.
  // Editing mode will be entered on successful reauth.
  if (!self.tableView.editing && !self.isPasswordShown) {
    [self attemptToShowPasswordFor:ReauthenticationReasonEdit];
    return;
  }

  if (self.tableView.editing) {
    // If password value was changed show confirmation dialog before saving.
    // Editing mode will be exited only if user confirms saving.
    if ([self passwordsDidChange]) {
      DCHECK(self.handler);
      // TODO(crbug.com/1401035): Show Password Edit Dialog when Password
      // Grouping is enabled.
      [self.handler showPasswordEditDialogWithOrigin:self.pageTitle];
    } else {
      [self passwordEditingConfirmed];
    }
    return;
  }

  [super editButtonPressed];
  [self reloadData];
}

- (void)loadModel {
  [super loadModel];

  self.passwordDetailsInfoItems = [[NSMutableArray alloc] init];

  for (PasswordDetails* passwordDetails in _passwords) {
    [self addPasswordDetailsToModel:passwordDetails];
  }
}

- (BOOL)showCancelDuringEditing {
  return YES;
}

#pragma mark - Items

- (TableViewStackedDetailsItem*)websiteItemForPasswordDetails:
    (PasswordDetails*)passwordDetails {
  TableViewStackedDetailsItem* item = [[TableViewStackedDetailsItem alloc]
      initWithType:PasswordDetailsItemTypeWebsite];
  item.titleText = l10n_util::GetNSString(IDS_IOS_SHOW_PASSWORD_VIEW_SITES);
  item.detailTexts = passwordDetails.websites;
  item.detailTextColor = [UIColor colorNamed:kTextSecondaryColor];
  item.accessibilityTraits = UIAccessibilityTraitNotEnabled;
  return item;
}

- (TableViewTextEditItem*)usernameItemForPasswordDetails:
    (PasswordDetails*)passwordDetails {
  TableViewTextEditItem* item = [[TableViewTextEditItem alloc]
      initWithType:PasswordDetailsItemTypeUsername];
  item.textFieldBackgroundColor = [UIColor clearColor];
  item.fieldNameLabelText =
      l10n_util::GetNSString(IDS_IOS_SHOW_PASSWORD_VIEW_USERNAME);
  item.textFieldValue = passwordDetails.username;  // Empty for a new form.
  // If password is missing (federated credential) don't allow to edit username.
  if (passwordDetails.credentialType != CredentialTypeFederation) {
    item.textFieldEnabled = self.tableView.editing;
    item.autoCapitalizationType = UITextAutocapitalizationTypeNone;
    item.delegate = self;
  } else {
    item.textFieldEnabled = NO;
  }
  item.hideIcon = YES;
  item.textFieldPlaceholder = l10n_util::GetNSString(
      IDS_IOS_PASSWORD_SETTINGS_USERNAME_PLACEHOLDER_TEXT);
  if (!self.tableView.editing) {
    item.textFieldTextColor = [UIColor colorNamed:kTextSecondaryColor];
  }
  return item;
}

- (TableViewTextEditItem*)passwordItemForPasswordDetails:
    (PasswordDetails*)passwordDetails {
  TableViewTextEditItem* item = [[TableViewTextEditItem alloc]
      initWithType:PasswordDetailsItemTypePassword];
  item.textFieldBackgroundColor = [UIColor clearColor];
  item.fieldNameLabelText =
      l10n_util::GetNSString(IDS_IOS_SHOW_PASSWORD_VIEW_PASSWORD);
  item.textFieldValue = [self isPasswordShown] || self.tableView.editing
                            ? passwordDetails.password
                            : kMaskedPassword;
  item.textFieldEnabled = self.tableView.editing;
  item.hideIcon = YES;
  item.autoCapitalizationType = UITextAutocapitalizationTypeNone;
  item.keyboardType = UIKeyboardTypeURL;
  item.returnKeyType = UIReturnKeyDone;
  item.delegate = self;
  item.textFieldPlaceholder = l10n_util::GetNSString(
      IDS_IOS_PASSWORD_SETTINGS_PASSWORD_PLACEHOLDER_TEXT);

  // During editing password is exposed so eye icon shouldn't be shown.
  if (!self.tableView.editing) {
    UIImage* image =
        [self isPasswordShown]
            ? DefaultSymbolWithPointSize(kHideActionSymbol, kSymbolSize)
            : DefaultSymbolWithPointSize(kShowActionSymbol, kSymbolSize);
    item.identifyingIcon = image;
    item.identifyingIconEnabled = YES;
    item.identifyingIconAccessibilityLabel = l10n_util::GetNSString(
        [self isPasswordShown] ? IDS_IOS_SETTINGS_PASSWORD_HIDE_BUTTON
                               : IDS_IOS_SETTINGS_PASSWORD_SHOW_BUTTON);
  }
  if (!self.tableView.editing) {
    item.textFieldTextColor = [UIColor colorNamed:kTextSecondaryColor];
  }
  return item;
}

- (TableViewMultiLineTextEditItem*)noteItemForPasswordDetails:
    (PasswordDetails*)passwordDetails {
  TableViewMultiLineTextEditItem* item = [[TableViewMultiLineTextEditItem alloc]
      initWithType:PasswordDetailsItemTypeNote];
  item.label = l10n_util::GetNSString(IDS_IOS_SHOW_PASSWORD_VIEW_NOTE);
  item.text = passwordDetails.note;
  item.editingEnabled = self.tableView.editing;
  item.delegate = self;
  return item;
}

- (TableViewTextEditItem*)federationItemForPasswordDetails:
    (PasswordDetails*)passwordDetails {
  TableViewTextEditItem* item = [[TableViewTextEditItem alloc]
      initWithType:PasswordDetailsItemTypeFederation];
  item.textFieldBackgroundColor = [UIColor clearColor];
  item.fieldNameLabelText =
      l10n_util::GetNSString(IDS_IOS_SHOW_PASSWORD_VIEW_FEDERATION);
  item.textFieldValue = passwordDetails.federation;
  item.textFieldEnabled = NO;
  item.hideIcon = YES;
  return item;
}

- (TableViewTextItem*)changePasswordItem {
  TableViewTextItem* item = [[TableViewTextItem alloc]
      initWithType:PasswordDetailsItemTypeChangePasswordButton];
  item.text = l10n_util::GetNSString(IDS_IOS_CHANGE_COMPROMISED_PASSWORD);
  item.textColor = self.tableView.editing
                       ? [UIColor colorNamed:kTextSecondaryColor]
                       : [UIColor colorNamed:kBlueColor];
  item.accessibilityTraits = UIAccessibilityTraitButton;
  return item;
}

- (SettingsImageDetailTextItem*)changePasswordRecommendationItem {
  SettingsImageDetailTextItem* item = [[SettingsImageDetailTextItem alloc]
      initWithType:PasswordDetailsItemTypeChangePasswordRecommendation];
  item.detailText = l10n_util::GetNSString(
      IDS_IOS_CHANGE_COMPROMISED_PASSWORD_DESCRIPTION_BRANDED);
  item.image = [self compromisedIcon];
  item.imageViewTintColor = [UIColor colorNamed:kRed500Color];
  item.accessibilityIdentifier = kCompromisedWarningId;
  return item;
}

- (TableViewTextItem*)dismissWarningItem {
  TableViewTextItem* item = [[TableViewTextItem alloc]
      initWithType:PasswordDetailsItemTypeDismissWarningButton];
  item.text = l10n_util::GetNSString(IDS_IOS_DISMISS_WARNING);
  item.textColor = self.tableView.editing
                       ? [UIColor colorNamed:kTextSecondaryColor]
                       : [UIColor colorNamed:kBlueColor];
  item.accessibilityTraits = UIAccessibilityTraitButton;
  return item;
}

- (TableViewTextItem*)restoreWarningItem {
  TableViewTextItem* item = [[TableViewTextItem alloc]
      initWithType:PasswordDetailsItemTypeRestoreWarningButton];
  item.text = l10n_util::GetNSString(IDS_IOS_RESTORE_WARNING);
  item.textColor = self.tableView.editing
                       ? [UIColor colorNamed:kTextSecondaryColor]
                       : [UIColor colorNamed:kBlueColor];
  item.accessibilityTraits = UIAccessibilityTraitButton;
  return item;
}

- (TableViewTextItem*)deleteButtonItemForPasswordDetails:
    (PasswordDetails*)passwordDetails {
  TableViewTextItem* item = [[TableViewTextItem alloc]
      initWithType:PasswordDetailsItemTypeDeleteButton];
  item.text = l10n_util::GetNSString(self.isBlockedSite
                                         ? IDS_IOS_DELETE_ACTION_TITLE
                                         : IDS_IOS_CONFIRM_PASSWORD_DELETION);
  item.textColor = [UIColor colorNamed:kRedColor];
  item.accessibilityTraits = UIAccessibilityTraitButton;
  item.accessibilityIdentifier = [NSString
      stringWithFormat:@"%@%@%@", kDeleteButtonForPasswordDetailsId,
                       passwordDetails.username, passwordDetails.password];
  return item;
}

- (TableViewTextItem*)moveToAccountButtonItem {
  TableViewTextItem* item = [[TableViewTextItem alloc]
      initWithType:PasswordDetailsItemTypeMoveToAccountButton];
  item.text = l10n_util::GetNSString(IDS_IOS_SAVE_PASSWORD_TO_ACCOUNT_STORE);
  item.textColor = self.tableView.editing
                       ? [UIColor colorNamed:kTextSecondaryColor]
                       : [UIColor colorNamed:kBlueColor];
  item.enabled = !self.tableView.editing;
  item.accessibilityIdentifier = kMovePasswordToAccountButtonId;
  return item;
}

- (SettingsImageDetailTextItem*)moveToAccountRecommendationItem {
  DCHECK(_userEmail.length)
      << "User must be signed-in to move a password to the "
         "account;";
  SettingsImageDetailTextItem* item = [[SettingsImageDetailTextItem alloc]
      initWithType:PasswordDetailsItemTypeMoveToAccountRecommendation];
  item.detailText = l10n_util::GetNSStringF(
      IDS_IOS_SAVE_PASSWORD_TO_ACCOUNT_STORE_DESCRIPTION,
      base::SysNSStringToUTF16(self.userEmail));
  item.image = CustomSymbolWithPointSize(kCloudAndArrowUpSymbol,
                                         kRecommendationSymbolSize);
  item.imageViewTintColor = [UIColor colorNamed:kBlueColor];
  return item;
}

#pragma mark - UITableViewDelegate

// Makes sure that the note footer is displayed correctly when it is scrolled to
// during password editing.
- (void)tableView:(UITableView*)tableView
    willDisplayFooterView:(UIView*)view
               forSection:(NSInteger)section {
  if ([view isKindOfClass:[TableViewTextHeaderFooterView class]]) {
    TableViewTextHeaderFooterView* footer =
        base::mac::ObjCCastStrict<TableViewTextHeaderFooterView>(view);
    NSString* footerText =
        self.passwordDetailsInfoItems[section].isNoteFooterShown
            ? l10n_util::GetNSStringF(
                  IDS_IOS_SETTINGS_PASSWORDS_TOO_LONG_NOTE_DESCRIPTION,
                  base::NumberToString16(kMaxPasswordNoteLength))
            : @"";
    [footer setSubtitle:footerText];
  }
}

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  [self setOrExtendAuthValidityTimer];
  TableViewModel* model = self.tableViewModel;
  NSInteger itemType = [model itemTypeForIndexPath:indexPath];
  switch (itemType) {
    case PasswordDetailsItemTypeWebsite:
    case PasswordDetailsItemTypeFederation:
      [self ensureContextMenuShownForItemType:itemType
                                    tableView:tableView
                                  atIndexPath:indexPath];
      break;
    case PasswordDetailsItemTypeUsername: {
      if (self.tableView.editing) {
        UITableViewCell* cell =
            [self.tableView cellForRowAtIndexPath:indexPath];
        TableViewTextEditCell* textFieldCell =
            base::mac::ObjCCastStrict<TableViewTextEditCell>(cell);
        [textFieldCell.textField becomeFirstResponder];
      } else {
        [self ensureContextMenuShownForItemType:itemType
                                      tableView:tableView
                                    atIndexPath:indexPath];
      }
      break;
    }
    case PasswordDetailsItemTypePassword: {
      if (self.tableView.editing) {
        UITableViewCell* cell =
            [self.tableView cellForRowAtIndexPath:indexPath];
        TableViewTextEditCell* textFieldCell =
            base::mac::ObjCCastStrict<TableViewTextEditCell>(cell);
        [textFieldCell.textField becomeFirstResponder];
      } else {
        [self ensureContextMenuShownForItemType:itemType
                                      tableView:tableView
                                    atIndexPath:indexPath];
      }
      break;
    }
    case PasswordDetailsItemTypeChangePasswordButton:
      if (!self.tableView.editing) {
        DCHECK(self.applicationCommandsHandler);
        PasswordDetails* passwordDetails = self.passwords[indexPath.section];
        DCHECK(passwordDetails.changePasswordURL.has_value());

        CHECK(password_manager::ShouldRecordPasswordCheckUserAction(
            passwordDetails.context, passwordDetails.compromised));

        password_manager::LogChangePasswordOnWebsite(
            GetWarningTypeForDetailsContext(passwordDetails.context));

        OpenNewTabCommand* command = [OpenNewTabCommand
            commandWithURLFromChrome:passwordDetails.changePasswordURL.value()];
        [self.applicationCommandsHandler closeSettingsUIAndOpenURL:command];
      }
      break;
    case PasswordDetailsItemTypeNote: {
      UITableViewCell* cell = [self.tableView cellForRowAtIndexPath:indexPath];
      TableViewMultiLineTextEditCell* textFieldCell =
          base::mac::ObjCCastStrict<TableViewMultiLineTextEditCell>(cell);
      [textFieldCell.textView becomeFirstResponder];
      break;
    }
    case PasswordDetailsItemTypeDismissWarningButton:
      if (!self.tableView.editing) {
        [self didTapDismissWarningButtonAtPasswordIndex:indexPath.section];
        [self.tableView deselectRowAtIndexPath:indexPath animated:YES];
      }
      break;
    case PasswordDetailsItemTypeRestoreWarningButton:
      if (!self.tableView.editing) {
        [self didTapRestoreWarningButtonAtPasswordIndex:indexPath.section];
        [self.tableView deselectRowAtIndexPath:indexPath animated:YES];
      }
      break;
    case PasswordDetailsItemTypeDeleteButton:
      if (self.tableView.editing) {
        UITableViewCell* cell =
            [self.tableView cellForRowAtIndexPath:indexPath];
        [self didTapDeleteButton:cell atPasswordIndex:indexPath.section];
        [self.tableView deselectRowAtIndexPath:indexPath animated:YES];
      }
      break;
    case PasswordDetailsItemTypeMoveToAccountButton:
      if (!self.tableView.editing) {
        UITableViewCell* cell =
            [self.tableView cellForRowAtIndexPath:indexPath];
        [self didTapMoveButton:cell atPasswordIndex:indexPath.section];
        [self.tableView deselectRowAtIndexPath:indexPath animated:YES];
      }
      break;
    case PasswordDetailsItemTypeChangePasswordRecommendation:
    case PasswordDetailsItemTypeMoveToAccountRecommendation:
      break;
  }
}

- (UITableViewCellEditingStyle)tableView:(UITableView*)tableView
           editingStyleForRowAtIndexPath:(NSIndexPath*)indexPath {
  return UITableViewCellEditingStyleNone;
}

- (BOOL)tableView:(UITableView*)tableview
    shouldIndentWhileEditingRowAtIndexPath:(NSIndexPath*)indexPath {
  return NO;
}

// If the context menu is not shown for a given item type, constructs that
// menu and shows it. This method should only be called for item types
// representing the cells with the site, username and password.
- (void)ensureContextMenuShownForItemType:(NSInteger)itemType
                                tableView:(UITableView*)tableView
                              atIndexPath:(NSIndexPath*)indexPath {
  UIMenuController* menu = [UIMenuController sharedMenuController];
  if (![menu isMenuVisible]) {
    menu.menuItems = [self menuItemsForItemType:itemType];

    [menu showMenuFromView:tableView
                      rect:[tableView rectForRowAtIndexPath:indexPath]];
  }
}

- (BOOL)tableView:(UITableView*)tableView
    shouldHighlightRowAtIndexPath:(NSIndexPath*)indexPath {
  NSInteger itemType = [self.tableViewModel itemTypeForIndexPath:indexPath];
  switch (itemType) {
    case PasswordDetailsItemTypeWebsite:
    case PasswordDetailsItemTypeFederation:
    case PasswordDetailsItemTypeUsername:
    case PasswordDetailsItemTypePassword:
    case PasswordDetailsItemTypeChangePasswordButton:
    case PasswordDetailsItemTypeMoveToAccountButton:
      return !self.editing;
    case PasswordDetailsItemTypeDeleteButton:
    case PasswordDetailsItemTypeNote:
      return self.editing;
    case PasswordDetailsItemTypeChangePasswordRecommendation:
    case PasswordDetailsItemTypeMoveToAccountRecommendation:
      return NO;
  }
  return YES;
}

- (CGFloat)tableView:(UITableView*)tableView
    heightForFooterInSection:(NSInteger)section {
  if (!self.passwordDetailsInfoItems[section].isNoteFooterShown) {
    return 0;
  }

  return [super tableView:tableView heightForFooterInSection:section];
}

#pragma mark - UITableViewDataSource

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  UITableViewCell* cell = [super tableView:tableView
                     cellForRowAtIndexPath:indexPath];

  NSInteger itemType = [self.tableViewModel itemTypeForIndexPath:indexPath];
  cell.tag = itemType;
  cell.selectionStyle = UITableViewCellSelectionStyleDefault;

  switch (itemType) {
    case PasswordDetailsItemTypeUsername: {
      TableViewTextEditCell* textFieldCell =
          base::mac::ObjCCastStrict<TableViewTextEditCell>(cell);
      textFieldCell.textField.delegate = self;
      [textFieldCell.identifyingIconButton
                 addTarget:self
                    action:@selector(didTapUsernameErrorInfo:)
          forControlEvents:UIControlEventTouchUpInside];
      self.usernameErrorAnchorView = textFieldCell.iconView;
      break;
    }
    case PasswordDetailsItemTypePassword: {
      TableViewTextEditCell* textFieldCell =
          base::mac::ObjCCastStrict<TableViewTextEditCell>(cell);
      textFieldCell.textField.delegate = self;
      [textFieldCell.identifyingIconButton
                 addTarget:self
                    action:@selector(didTapShowHideButton:)
          forControlEvents:UIControlEventTouchUpInside];
      textFieldCell.identifyingIconButton.tag = indexPath.section;
      break;
    }
    case PasswordDetailsItemTypeChangePasswordRecommendation:
    case PasswordDetailsItemTypeMoveToAccountRecommendation:
    case PasswordDetailsItemTypeNote: {
      cell.selectionStyle = UITableViewCellSelectionStyleNone;
      break;
    }
    case PasswordDetailsItemTypeMoveToAccountButton: {
      // Record the "move to account offered" metric.
      // 1) The metric mustn't be recorded for credentials that are not visible
      // in the scroll view yet, so do it when the button cell is configured for
      // display (the button, not the text that comes before).
      // 2) The metric mustn't be recorded for the same credential again upon
      // model changes, e.g. credential removed or moved to account. Such events
      // reconfigure cells, so check if this username was already seen before
      // recording. The username is the closest thing to a stable identifier of
      // the credential. It can be edited, leading to a second recording, but
      // that shouldn't happen often. This approach is good enough.
      if (![self.usernamesWithMoveToAccountOfferRecorded
              containsObject:self.passwords[indexPath.section].username]) {
        [self.usernamesWithMoveToAccountOfferRecorded
            addObject:self.passwords[indexPath.section].username];
        // TODO(crbug.com/1392747): Use a common function for recording sites.
        base::UmaHistogramEnumeration(
            "PasswordManager.AccountStorage.MoveToAccountStoreFlowOffered",
            password_manager::metrics_util::MoveToAccountStoreTrigger::
                kExplicitlyTriggeredInSettings);
      }
      break;
    }
    case PasswordDetailsItemTypeNoteFooter:
    case PasswordDetailsItemTypeWebsite:
    case PasswordDetailsItemTypeFederation:
    case PasswordDetailsItemTypeChangePasswordButton:
    case PasswordDetailsItemTypeDismissWarningButton:
    case PasswordDetailsItemTypeRestoreWarningButton:
    case PasswordDetailsItemTypeDeleteButton:
      break;
  }
  return cell;
}

- (BOOL)tableView:(UITableView*)tableView
    canEditRowAtIndexPath:(NSIndexPath*)indexPath {
  NSInteger itemType = [self.tableViewModel itemTypeForIndexPath:indexPath];
  switch (itemType) {
    case PasswordDetailsItemTypeWebsite:
    case PasswordDetailsItemTypeFederation:
    case PasswordDetailsItemTypeUsername:
    case PasswordDetailsItemTypePassword:
    case PasswordDetailsItemTypeNote:
    case PasswordDetailsItemTypeDeleteButton:
      return YES;
  }
  return NO;
}

#pragma mark - PasswordDetailsConsumer

- (void)setPasswords:(NSArray<PasswordDetails*>*)passwords
            andTitle:(NSString*)title {
  BOOL hadPasswords = [_passwords count];
  _passwords = passwords;
  _pageTitle = title;

  [self updateNavigationTitle];
  // Update the model even if all passwords are deleted and the view controller
  // will be dismissed. UIKit could still trigger events that execute CHECK in
  // this file that would fail if `_passwords` and the model are not in sync.
  [self reloadData];

  if (![passwords count]) {
    // onAllPasswordsDeleted() mustn't be called twice.
    if (hadPasswords) {
      [self.handler onAllPasswordsDeleted];
    }
  }
}

- (void)setIsBlockedSite:(BOOL)isBlockedSite {
  _isBlockedSite = isBlockedSite;
}

- (void)setUserEmail:(NSString*)userEmail {
  _userEmail = userEmail;
}

#pragma mark - TableViewTextEditItemDelegate

- (void)tableViewItemDidBeginEditing:(TableViewTextEditItem*)tableViewItem {
  [self reconfigureCellsForItems:@[ tableViewItem ]];
}

- (void)tableViewItemDidChange:(TableViewTextEditItem*)tableViewItem {
  [self setOrExtendAuthValidityTimer];
  BOOL usernameValid = [self checkIfValidUsernames];
  BOOL passwordValid = [self checkIfValidPasswords];
  BOOL noteValid = [self checkIfValidNotes];

  self.shouldEnableEditDoneButton = usernameValid && passwordValid && noteValid;
  [self toggleNavigationBarRightButtonItem];
}

- (void)tableViewItemDidEndEditing:(TableViewTextEditItem*)tableViewItem {
  if ([tableViewItem.fieldNameLabelText
          isEqualToString:l10n_util::GetNSString(
                              IDS_IOS_SHOW_PASSWORD_VIEW_PASSWORD)]) {
    [self checkIfValidPasswords];
  }
  [self reconfigureCellsForItems:@[ tableViewItem ]];
}

#pragma mark - TableViewMultiLineTextEditItemDelegate

- (void)textViewItemDidChange:(TableViewMultiLineTextEditItem*)tableViewItem {
  [self setOrExtendAuthValidityTimer];
  // Update save button state based on the note's length and validity of other
  // input fields.
  BOOL noteValid = tableViewItem.text.length <= kMaxPasswordNoteLength;
  tableViewItem.validText = noteValid;
  self.shouldEnableEditDoneButton =
      noteValid && [self checkIfValidUsernames] && [self checkIfValidPasswords];
  [self toggleNavigationBarRightButtonItem];
  [self reconfigureCellsForItems:@[ tableViewItem ]];

  // Notify that the note character limit has been reached via VoiceOver.
  if (!noteValid) {
    NSString* tooLongNoteMessage = l10n_util::GetNSStringF(
        IDS_IOS_SETTINGS_PASSWORDS_TOO_LONG_NOTE_DESCRIPTION,
        base::NumberToString16(kMaxPasswordNoteLength));
    UIAccessibilityPostNotification(UIAccessibilityAnnouncementNotification,
                                    tooLongNoteMessage);
  }

  BOOL shouldDisplayNoteFooter =
      tableViewItem.text.length >= kMinNoteCharAmountForWarning;
  NSIndexPath* indexPath = [self.tableViewModel
      indexPathForItem:static_cast<TableViewItem*>(tableViewItem)];

  // Refresh the cells' height and update note footer based on note's length.
  [self.tableView beginUpdates];
  if (shouldDisplayNoteFooter !=
      self.passwordDetailsInfoItems[indexPath.section].isNoteFooterShown) {
    self.passwordDetailsInfoItems[indexPath.section].isNoteFooterShown =
        shouldDisplayNoteFooter;

    UITableViewHeaderFooterView* footer =
        [self.tableView footerViewForSection:indexPath.section];
    TableViewTextHeaderFooterView* textFooter =
        base::mac::ObjCCastStrict<TableViewTextHeaderFooterView>(footer);
    NSString* footerText =
        shouldDisplayNoteFooter
            ? l10n_util::GetNSStringF(
                  IDS_IOS_SETTINGS_PASSWORDS_TOO_LONG_NOTE_DESCRIPTION,
                  base::NumberToString16(kMaxPasswordNoteLength))
            : @"";
    [textFooter setSubtitle:footerText];
  }
  [self.tableView endUpdates];
}

#pragma mark - Private

// Applies tint colour and resizes image.
- (UIImage*)compromisedIcon {
  return DefaultSymbolTemplateWithPointSize(kErrorCircleFillSymbol,
                                            kRecommendationSymbolSize);
}

// Shows reauthentication dialog if needed. If the reauthentication is
// successful reveals the password.
- (void)attemptToShowPasswordFor:(ReauthenticationReason)reason {
  // If password was already shown (before editing or copying) or the flag to
  // override auth is YES, we don't need to request reauth again.
  // With password notes feature enabled the authentication happens during
  // navigation from the password list view to the password details view.
  if (self.isPasswordShown || self.showPasswordWithoutAuth ||
      IsPasswordNotesWithBackupEnabled()) {
    [self showPasswordFor:reason];
    return;
  }

  if ([self.reauthModule canAttemptReauth]) {
    __weak __typeof(self) weakSelf = self;
    void (^showPasswordHandler)(ReauthenticationResult) = ^(
        ReauthenticationResult result) {
      PasswordDetailsTableViewController* strongSelf = weakSelf;
      if (!strongSelf) {
        return;
      }
      [strongSelf logPasswordSettingsReauthResult:result];

      if (result == ReauthenticationResult::kFailure) {
        if (reason == ReauthenticationReasonCopy) {
          [strongSelf
               showToast:l10n_util::GetNSString(
                             IDS_IOS_SETTINGS_PASSWORD_WAS_NOT_COPIED_MESSAGE)
              forSuccess:NO];
        }
        return;
      }

      [strongSelf showPasswordFor:reason];
    };

    [self.reauthModule
        attemptReauthWithLocalizedReason:[self localizedStringForReason:reason]
                    canReusePreviousAuth:YES
                                 handler:showPasswordHandler];
  } else {
    DCHECK(self.handler);
    [self.handler showPasscodeDialogForReason:PasscodeDialogReasonShowPassword];
  }
}

// Reveals password to the user.
- (void)showPasswordFor:(ReauthenticationReason)reason {
  switch (reason) {
    case ReauthenticationReasonShow: {
      self.passwordShown = YES;
      self.passwordDetailsInfoItems[_passwordIndexToReveal]
          .passwordTextItem.textFieldValue =
          self.passwords[_passwordIndexToReveal].password;
      self.passwordDetailsInfoItems[_passwordIndexToReveal]
          .passwordTextItem.identifyingIcon =
          DefaultSymbolWithPointSize(kHideActionSymbol, kSymbolSize);
      self.passwordDetailsInfoItems[_passwordIndexToReveal]
          .passwordTextItem.identifyingIconAccessibilityLabel =
          l10n_util::GetNSString(IDS_IOS_SETTINGS_PASSWORD_HIDE_BUTTON);
      [self reconfigureCellsForItems:@[
        self.passwordDetailsInfoItems[_passwordIndexToReveal].passwordTextItem
      ]];

      PasswordDetails* passwordDetails = self.passwords[_passwordIndexToReveal];
      DetailsContext detailsContext = passwordDetails.context;
      // When details was opened from the Password Manager, only log password
      // check actions if the password is compromised.
      if (password_manager::ShouldRecordPasswordCheckUserAction(
              detailsContext, passwordDetails.compromised)) {
        password_manager::LogRevealPassword(
            GetWarningTypeForDetailsContext(detailsContext));
      }
      break;
    }
    case ReauthenticationReasonCopy: {
      NSString* copiedString =
          self.passwords[self.tableView.indexPathForSelectedRow.section]
              .password;
      StoreTextInPasteboard(copiedString);

      [self showToast:l10n_util::GetNSString(
                          IDS_IOS_SETTINGS_PASSWORD_WAS_COPIED_MESSAGE)
           forSuccess:YES];
      DCHECK(self.handler);
      [self.handler onPasswordCopiedByUser];
      break;
    }
    case ReauthenticationReasonEdit:
      // Called super because we want to update only `tableView.editing`.
      [super editButtonPressed];
      [self reloadData];
      break;
  }
  [self logPasswordAccessWith:reason];
}

// Returns localized reason for reauthentication dialog.
- (NSString*)localizedStringForReason:(ReauthenticationReason)reason {
  switch (reason) {
    case ReauthenticationReasonShow:
      return l10n_util::GetNSString(
          IDS_IOS_SETTINGS_PASSWORD_REAUTH_REASON_SHOW);
    case ReauthenticationReasonCopy:
      return l10n_util::GetNSString(
          IDS_IOS_SETTINGS_PASSWORD_REAUTH_REASON_COPY);
    case ReauthenticationReasonEdit:
      return l10n_util::GetNSString(
          IDS_IOS_SETTINGS_PASSWORD_REAUTH_REASON_EDIT);
  }
}

// Shows a snack bar with `message` and provides haptic feedback. The haptic
// feedback is either for success or for error, depending on `success`. Deselect
// cell if there was one selected.
- (void)showToast:(NSString*)message forSuccess:(BOOL)success {
  TriggerHapticFeedbackForNotification(success
                                           ? UINotificationFeedbackTypeSuccess
                                           : UINotificationFeedbackTypeError);
  [self.snackbarCommandsHandler showSnackbarWithMessage:message
                                             buttonText:nil
                                          messageAction:nil
                                       completionAction:nil];

  if ([self.tableView indexPathForSelectedRow]) {
    [self.tableView
        deselectRowAtIndexPath:[self.tableView indexPathForSelectedRow]
                      animated:YES];
  }
}

// Checks if the usernames are valid and updates items accordingly.
- (BOOL)checkIfValidUsernames {
  DCHECK(self.passwords.count == self.passwordDetailsInfoItems.count);

  for (NSUInteger i = 0; i < self.passwordDetailsInfoItems.count; i++) {
    NSString* newUsernameValue =
        self.passwordDetailsInfoItems[i].usernameTextItem.textFieldValue;
    BOOL usernameChanged =
        ![newUsernameValue isEqualToString:self.passwords[i].username];
    BOOL showUsernameAlreadyUsed =
        usernameChanged &&
        [self.delegate isUsernameReused:newUsernameValue
                              forDomain:self.passwords[i].signonRealm];
    self.passwordDetailsInfoItems[i].usernameTextItem.hasValidText =
        !showUsernameAlreadyUsed;
    self.passwordDetailsInfoItems[i].usernameTextItem.identifyingIconEnabled =
        showUsernameAlreadyUsed;
    [self reconfigureCellsForItems:@[
      self.passwordDetailsInfoItems[i].usernameTextItem
    ]];

    if (showUsernameAlreadyUsed) {
      return NO;
    }
  }
  return YES;
}

// Checks if the passwords are valid and updates items accordingly.
- (BOOL)checkIfValidPasswords {
  DCHECK(self.passwords.count == self.passwordDetailsInfoItems.count);

  for (NSUInteger i = 0; i < self.passwordDetailsInfoItems.count; i++) {
    BOOL passwordEmpty = [self.passwordDetailsInfoItems[i]
                                 .passwordTextItem.textFieldValue length] == 0;
    self.passwordDetailsInfoItems[i].passwordTextItem.hasValidText =
        !passwordEmpty;
    [self reconfigureCellsForItems:@[
      self.passwordDetailsInfoItems[i].passwordTextItem
    ]];

    if (passwordEmpty) {
      return NO;
    }
  }
  return YES;
}

// Checks if notes are valid.
- (BOOL)checkIfValidNotes {
  DCHECK(self.passwords.count == self.passwordDetailsInfoItems.count);

  for (NSUInteger i = 0; i < self.passwordDetailsInfoItems.count; i++) {
    if (self.passwordDetailsInfoItems[i].passwordNoteItem.text.length >
        kMaxPasswordNoteLength) {
      return NO;
    }
  }
  return YES;
}

// Removes the given section if it exists.
- (void)removeSectionWithIdentifier:(NSInteger)sectionIdentifier
                   withRowAnimation:(UITableViewRowAnimation)animation {
  TableViewModel* model = self.tableViewModel;
  if ([model hasSectionForSectionIdentifier:sectionIdentifier]) {
    NSInteger section = [model sectionForSectionIdentifier:sectionIdentifier];
    [model removeSectionWithIdentifier:sectionIdentifier];
    [[self tableView] deleteSections:[NSIndexSet indexSetWithIndex:section]
                    withRowAnimation:animation];
  }
}

// Enables/Disables the right bar button item in the navigation bar.
- (void)toggleNavigationBarRightButtonItem {
  self.navigationItem.rightBarButtonItem.enabled =
      self.shouldEnableEditDoneButton;
}

- (BOOL)hasAtLeastOnePassword {
  for (PasswordDetails* passwordDetails in self.passwords) {
    if (passwordDetails.password.length > 0) {
      return YES;
    }
  }
  return NO;
}

- (BOOL)passwordsDidChange {
  DCHECK(self.passwords.count == self.passwordDetailsInfoItems.count);

  for (NSUInteger i = 0; i < self.passwordDetailsInfoItems.count; i++) {
    if (![self.passwords[i].password
            isEqualToString:self.passwordDetailsInfoItems[i]
                                .passwordTextItem.textFieldValue]) {
      return YES;
    }
  }
  return NO;
}

// Updates the title displayed in the navigation bar.
- (void)updateNavigationTitle {
  if (self.pageTitle.length == 0) {
    // When no pageTitle is supplied, use origin of first password.
    PasswordDetails* firstPassword = self.passwords.firstObject;
    self.pageTitle = firstPassword.origins.firstObject;
  }
  _titleLabel.text = self.pageTitle;
}

// Creates the model items corresponding to a `PasswordDetails` and adds them to
// the `model`.
- (void)addPasswordDetailsToModel:(PasswordDetails*)passwordDetails {
  TableViewModel* model = self.tableViewModel;
  PasswordDetailsInfoItem* passwordItem =
      [[PasswordDetailsInfoItem alloc] init];

  NSInteger sectionForWebsite;
  NSInteger sectionForPassword;
  NSInteger sectionForCompromisedInfo;
  NSInteger sectionForMoveCredential;

    // Password details are displayed in its own section when Grouping is
    // enabled.
    NSInteger nextSection =
        kSectionIdentifierEnumZero + [model numberOfSections];
    [model addSectionWithIdentifier:nextSection];

    sectionForWebsite = nextSection;
    sectionForPassword = nextSection;
    sectionForCompromisedInfo = nextSection;
    sectionForMoveCredential = nextSection;

    // Add sites to section.
    passwordItem.websiteItem =
        [self websiteItemForPasswordDetails:passwordDetails];
    [model addItem:passwordItem.websiteItem
        toSectionWithIdentifier:sectionForWebsite];

    // Add username and password to section according to credential type.
    switch (passwordDetails.credentialType) {
    case CredentialTypeRegular: {
      passwordItem.usernameTextItem =
          [self usernameItemForPasswordDetails:passwordDetails];
      [model addItem:passwordItem.usernameTextItem
          toSectionWithIdentifier:sectionForPassword];

      passwordItem.passwordTextItem =
          [self passwordItemForPasswordDetails:passwordDetails];
      [model addItem:passwordItem.passwordTextItem
          toSectionWithIdentifier:sectionForPassword];

      if (IsPasswordNotesWithBackupEnabled()) {
        passwordItem.passwordNoteItem =
            [self noteItemForPasswordDetails:passwordDetails];
        [model addItem:passwordItem.passwordNoteItem
            toSectionWithIdentifier:sectionForPassword];

        passwordItem.isNoteFooterShown =
            self.tableView.editing &&
            passwordItem.passwordNoteItem.text.length >=
                kMinNoteCharAmountForWarning;
        TableViewTextHeaderFooterItem* footer =
            [[TableViewTextHeaderFooterItem alloc]
                initWithType:PasswordDetailsItemTypeNoteFooter];
        footer.subtitle =
            passwordItem.isNoteFooterShown
                ? l10n_util::GetNSStringF(
                      IDS_IOS_SETTINGS_PASSWORDS_TOO_LONG_NOTE_DESCRIPTION,
                      base::NumberToString16(kMaxPasswordNoteLength))
                : @"";
        [model setFooter:footer forSectionWithIdentifier:sectionForPassword];
      }

      if (passwordDetails.isCompromised || passwordDetails.isMuted) {
        [model addItem:[self changePasswordRecommendationItem]
            toSectionWithIdentifier:sectionForCompromisedInfo];

        if (passwordDetails.changePasswordURL.has_value()) {
          [model addItem:[self changePasswordItem]
              toSectionWithIdentifier:sectionForCompromisedInfo];
        }

        if (ShouldAllowToDismissWarning(passwordDetails.context,
                                        passwordDetails.compromised)) {
          [model addItem:[self dismissWarningItem]
              toSectionWithIdentifier:sectionForCompromisedInfo];
        } else if (ShouldAllowToRestoreWarning(passwordDetails.context,
                                               passwordDetails.muted)) {
          [model addItem:[self restoreWarningItem]
              toSectionWithIdentifier:sectionForCompromisedInfo];
        }
      }
      break;
    }
    case CredentialTypeFederation: {
      passwordItem.usernameTextItem =
          [self usernameItemForPasswordDetails:passwordDetails];
      [model addItem:passwordItem.usernameTextItem
          toSectionWithIdentifier:sectionForPassword];

      // Federated password forms don't have password value.
      [model addItem:[self federationItemForPasswordDetails:passwordDetails]
          toSectionWithIdentifier:sectionForPassword];
      break;
    }

    case CredentialTypeBlocked: {
      break;
    }
  }

  if (passwordDetails.shouldOfferToMoveToAccount) {
    [model addItem:[self moveToAccountRecommendationItem]
        toSectionWithIdentifier:sectionForMoveCredential];
    [model addItem:[self moveToAccountButtonItem]
        toSectionWithIdentifier:sectionForMoveCredential];
  }

  if (self.tableView.editing) {
    [model addItem:[self deleteButtonItemForPasswordDetails:passwordDetails]
        toSectionWithIdentifier:sectionForPassword];
  }
  [self.passwordDetailsInfoItems addObject:passwordItem];
}

// Moves password at specified index from profile store to account store.
- (void)moveCredentialToAccountStore:(int)passwordIndex
                          anchorView:(UIView*)anchorView {
  DCHECK_GE(passwordIndex, 0);
  DCHECK(self.handler);

  __weak __typeof(self) weakSelf = self;
  NSString* toastMessage = l10n_util::GetNSStringF(
      IDS_IOS_PASSWORD_SAVED_TO_ACCOUNT_SNACKBAR_MESSAGE,
      base::SysNSStringToUTF16(self.userEmail));
  [self.handler moveCredentialToAccountStore:self.passwords[passwordIndex]
                                  anchorView:anchorView
                             movedCompletion:^{
                               [weakSelf showToast:toastMessage forSuccess:YES];
                             }];
}

// Navigates to password manager list view when the timeout for a valid
// authentication has passed.
- (void)authValidityTimerFired:(NSTimer*)timer {
  [self.navigationController popViewControllerAnimated:YES];
}

// Starts the timer after passing an authentication to open password details
// view or extends it on an interaction with the details view.
- (void)setOrExtendAuthValidityTimer {
  if (!IsPasswordNotesWithBackupEnabled()) {
    return;
  }

  [self.authValidityTimer invalidate];
  self.authValidityTimer = [NSTimer
      scheduledTimerWithTimeInterval:syncer::kPasswordNotesAuthValidity.Get()
                                         .InSeconds()
                              target:self
                            selector:@selector(authValidityTimerFired:)
                            userInfo:nil
                             repeats:NO];
}

- (void)onShareButtonPressed {
  CHECK(self.handler);
  [self.handler onShareButtonPressed];
}

#pragma mark - AutofillEditTableViewController

- (BOOL)isItemAtIndexPathTextEditCell:(NSIndexPath*)cellPath {
  NSInteger itemType = [self.tableViewModel itemTypeForIndexPath:cellPath];
  switch (static_cast<PasswordDetailsItemType>(itemType)) {
    case PasswordDetailsItemTypeUsername:
    case PasswordDetailsItemTypePassword:
      return YES;
    case PasswordDetailsItemTypeWebsite:
    case PasswordDetailsItemTypeFederation:
    case PasswordDetailsItemTypeChangePasswordButton:
    case PasswordDetailsItemTypeChangePasswordRecommendation:
    case PasswordDetailsItemTypeDismissWarningButton:
    case PasswordDetailsItemTypeRestoreWarningButton:
    case PasswordDetailsItemTypeDeleteButton:
    case PasswordDetailsItemTypeMoveToAccountButton:
    case PasswordDetailsItemTypeMoveToAccountRecommendation:
    case PasswordDetailsItemTypeNoteFooter:
    case PasswordDetailsItemTypeNote:
      return NO;
  }
}

#pragma mark - Actions

// Called when the user tapped on the show/hide button near password.
- (void)didTapShowHideButton:(UIButton*)buttonView {
  [self setOrExtendAuthValidityTimer];
  [self.tableView deselectRowAtIndexPath:self.tableView.indexPathForSelectedRow
                                animated:NO];
  _passwordIndexToReveal = [buttonView tag];

  if (self.isPasswordShown) {
    self.passwordShown = NO;
    self.passwordDetailsInfoItems[_passwordIndexToReveal]
        .passwordTextItem.textFieldValue = kMaskedPassword;

    self.passwordDetailsInfoItems[_passwordIndexToReveal]
        .passwordTextItem.identifyingIcon =
        DefaultSymbolWithPointSize(kShowActionSymbol, kSymbolSize);
    self.passwordDetailsInfoItems[_passwordIndexToReveal]
        .passwordTextItem.identifyingIconAccessibilityLabel =
        l10n_util::GetNSString(IDS_IOS_SETTINGS_PASSWORD_SHOW_BUTTON);
    [self reconfigureCellsForItems:@[
      self.passwordDetailsInfoItems[_passwordIndexToReveal].passwordTextItem
    ]];
  } else {
    [self attemptToShowPasswordFor:ReauthenticationReasonShow];
  }
}

// Called when the user tap error info icon in the username input.
- (void)didTapUsernameErrorInfo:(UIButton*)buttonView {
  [self setOrExtendAuthValidityTimer];
  NSString* text = l10n_util::GetNSString(IDS_IOS_USERNAME_ALREADY_USED);

  NSAttributedString* attributedText = [[NSAttributedString alloc]
      initWithString:text
          attributes:@{
            NSForegroundColorAttributeName :
                [UIColor colorNamed:kTextSecondaryColor],
            NSFontAttributeName :
                [UIFont preferredFontForTextStyle:UIFontTextStyleSubheadline]
          }];

  PopoverLabelViewController* errorInfoPopover =
      [[PopoverLabelViewController alloc]
          initWithPrimaryAttributedString:attributedText
                secondaryAttributedString:nil];

  errorInfoPopover.popoverPresentationController.sourceView =
      self.usernameErrorAnchorView;
  errorInfoPopover.popoverPresentationController.sourceRect =
      self.usernameErrorAnchorView.bounds;
  errorInfoPopover.popoverPresentationController.permittedArrowDirections =
      UIPopoverArrowDirectionAny;
  [self presentViewController:errorInfoPopover animated:YES completion:nil];
}

// Returns an array of UIMenuItems to display in a context menu on the site
// cell.
- (NSArray*)menuItemsForItemType:(NSInteger)itemType {
  PasswordDetailsMenuItem* copyOption = [[PasswordDetailsMenuItem alloc]
      initWithTitle:l10n_util::GetNSString(IDS_IOS_SETTINGS_SITE_COPY_MENU_ITEM)
             action:@selector(copyPasswordDetails:)];
  copyOption.itemType = itemType;
  return @[ copyOption ];
}

// Copies the password information to system pasteboard and shows a toast of
// success/failure.
- (void)copyPasswordDetails:(id)sender {
  [self setOrExtendAuthValidityTimer];
  UIMenuController* menu = base::mac::ObjCCastStrict<UIMenuController>(sender);
  PasswordDetailsMenuItem* menuItem =
      base::mac::ObjCCastStrict<PasswordDetailsMenuItem>(
          menu.menuItems.firstObject);

  NSString* message = nil;

  switch (menuItem.itemType) {
    case PasswordDetailsItemTypeWebsite: {
      PasswordDetails* detailsToCopy;
      detailsToCopy =
          self.passwords[self.tableView.indexPathForSelectedRow.section];
      message =
          l10n_util::GetNSString(IDS_IOS_SETTINGS_SITES_WERE_COPIED_MESSAGE);
      // Copy websites to pasteboard separated by a whitespace.
      NSArray<NSString*>* websites = detailsToCopy.websites;
      NSMutableString* websitesForPasteboard =
          [websites.firstObject mutableCopy];

      for (NSUInteger index = 1U; index < websites.count; index++) {
        [websitesForPasteboard appendFormat:@" %@", websites[index]];
      }
      StoreTextInPasteboard(websitesForPasteboard);
      break;
    }
    case PasswordDetailsItemTypeUsername: {
      NSString* copiedString =
          self.passwords[self.tableView.indexPathForSelectedRow.section]
              .username;

      StoreTextInPasteboard(copiedString);
      message =
          l10n_util::GetNSString(IDS_IOS_SETTINGS_USERNAME_WAS_COPIED_MESSAGE);
      break;
    }
    case PasswordDetailsItemTypeFederation: {
      NSString* copiedString =
          self.passwords[self.tableView.indexPathForSelectedRow.section]
              .federation;
      StoreTextInPasteboard(copiedString);
      [self logCopyPasswordDetailsFailure:NO];
      return;
    }
    case PasswordDetailsItemTypePassword: {
      [self attemptToShowPasswordFor:ReauthenticationReasonCopy];
      [self logCopyPasswordDetailsFailure:NO];
      return;
    }
  }

  if (message.length) {
    [self logCopyPasswordDetailsFailure:NO];
    [self showToast:message forSuccess:YES];
  } else {
    // TODO(crbug.com/1359331): There's a bug that is caused by `menu` being
    // nil, which leads to a nil message and a crash. Avoiding the crash and
    // logging for monitoring the issue. Since `menu` is an instance of
    // `UIMenuController` which is deprecated on iOS 16, this crash should go
    // away once we switch to `UIEditMenuInteraction`.
    [self logCopyPasswordDetailsFailure:YES];
  }
}

- (void)didTapDismissWarningButtonAtPasswordIndex:(NSUInteger)passwordIndex {
  CHECK(passwordIndex >= 0 && passwordIndex < self.passwords.count);
  CHECK(self.delegate);

  password_manager::LogMuteCompromisedWarning();

  [self.delegate dismissWarningForPassword:self.passwords[passwordIndex]];
}

- (void)didTapRestoreWarningButtonAtPasswordIndex:(NSUInteger)passwordIndex {
  CHECK(passwordIndex >= 0 && passwordIndex < self.passwords.count);
  CHECK(self.delegate);

  password_manager::LogUnmuteCompromisedWarning();

  [self.delegate restoreWarningForCurrentPassword];
}

- (void)didTapDeleteButton:(UITableViewCell*)cell
           atPasswordIndex:(NSUInteger)passwordIndex {
  CHECK(passwordIndex >= 0 && passwordIndex < self.passwords.count);
  CHECK(self.handler);
  [self.handler
      showPasswordDeleteDialogWithPasswordDetails:self.passwords[passwordIndex]
                                       anchorView:cell];
}

- (void)didTapMoveButton:(UITableViewCell*)cell
         atPasswordIndex:(int)passwordIndex {
  [self setOrExtendAuthValidityTimer];

  // With password notes feature enabled the authentication happens during
  // navigation from the password list view to the password details view.
  if (IsPasswordNotesWithBackupEnabled()) {
    [self moveCredentialToAccountStore:passwordIndex anchorView:cell];
    return;
  }

  if (![self.reauthModule canAttemptReauth]) {
    [self.handler
        showPasscodeDialogForReason:PasscodeDialogReasonMovePasswordToAccount];
    return;
  }
  __weak __typeof(self) weakSelf = self;
  void (^movePasswordHandler)(ReauthenticationResult) =
      ^(ReauthenticationResult result) {
        PasswordDetailsTableViewController* strongSelf = weakSelf;
        if (!strongSelf) {
          return;
        }
        if (result == ReauthenticationResult::kFailure) {
          return;
        }

        [self moveCredentialToAccountStore:passwordIndex anchorView:cell];
      };
  [self.reauthModule
      attemptReauthWithLocalizedReason:
          l10n_util::GetNSString(IDS_IOS_AUTH_TO_SAVE_PASSWORD_TO_ACCOUNT_STORE)
                  canReusePreviousAuth:YES
                               handler:movePasswordHandler];
}

- (void)dismissView {
  [self.view endEditing:YES];
  [self.handler dismissPasswordDetailsTableViewController];
}

#pragma mark - UIResponder

- (BOOL)canBecomeFirstResponder {
  return YES;
}

- (BOOL)canPerformAction:(SEL)action withSender:(id)sender {
  if (action == @selector(copyPasswordDetails:)) {
    return YES;
  }
  return NO;
}

#pragma mark - Metrics

// Logs metrics for the given reauthentication `result` (success, failure or
// skipped).
- (void)logPasswordSettingsReauthResult:(ReauthenticationResult)result {
  switch (result) {
    case ReauthenticationResult::kSuccess:
      LogPasswordSettingsReauthResult(ReauthResult::kSuccess);
      break;
    case ReauthenticationResult::kFailure:
      LogPasswordSettingsReauthResult(ReauthResult::kFailure);
      break;
    case ReauthenticationResult::kSkipped:
      LogPasswordSettingsReauthResult(ReauthResult::kSkipped);
      break;
  }
}

- (void)logPasswordAccessWith:(ReauthenticationReason)reason {
  switch (reason) {
    case ReauthenticationReasonShow:
      UMA_HISTOGRAM_ENUMERATION(
          "PasswordManager.AccessPasswordInSettings",
          password_manager::metrics_util::ACCESS_PASSWORD_VIEWED,
          password_manager::metrics_util::ACCESS_PASSWORD_COUNT);
      break;
    case ReauthenticationReasonCopy:
      UMA_HISTOGRAM_ENUMERATION(
          "PasswordManager.AccessPasswordInSettings",
          password_manager::metrics_util::ACCESS_PASSWORD_COPIED,
          password_manager::metrics_util::ACCESS_PASSWORD_COUNT);
      break;
    case ReauthenticationReasonEdit:
      UMA_HISTOGRAM_ENUMERATION(
          "PasswordManager.AccessPasswordInSettings",
          password_manager::metrics_util::ACCESS_PASSWORD_EDITED,
          password_manager::metrics_util::ACCESS_PASSWORD_COUNT);
      break;
  }
}

- (void)logCopyPasswordDetailsFailure:(BOOL)failure {
  base::UmaHistogramBoolean(
      "PasswordManager.iOS.PasswordDetails.CopyDetailsFailed", failure);
}

- (void)logChangeBetweenOldNote:(NSString*)oldNote
                    currentNote:(NSString*)currentNote {
  PasswordNoteAction action;
  if (oldNote == currentNote) {
    action = PasswordNoteAction::kNoteNotChanged;
  } else if (oldNote.length != 0 && currentNote.length != 0) {
    action = PasswordNoteAction::kNoteEditedInEditDialog;
  } else if (oldNote.length == 0) {
    action = PasswordNoteAction::kNoteAddedInEditDialog;
  } else {
    action = PasswordNoteAction::kNoteRemovedInEditDialog;
  }
  LogPasswordNoteActionInSettings(action);
}

#pragma mark - Public

- (void)passwordEditingConfirmed {
  DCHECK(self.passwords.count == self.passwordDetailsInfoItems.count);
  for (NSUInteger i = 0; i < self.passwordDetailsInfoItems.count; i++) {
    PasswordDetails* password = self.passwords[i];
    NSString* oldUsername = password.username;
    NSString* oldPassword = password.password;
    NSString* oldNote = password.note;

    PasswordDetailsInfoItem* passwordDetailsInfoItem =
        self.passwordDetailsInfoItems[i];
    password.username = passwordDetailsInfoItem.usernameTextItem.textFieldValue;
    password.password = passwordDetailsInfoItem.passwordTextItem.textFieldValue;
    if (IsPasswordNotesWithBackupEnabled()) {
      password.note = passwordDetailsInfoItem.passwordNoteItem.text;
      [self logChangeBetweenOldNote:oldNote currentNote:password.note];
    }
    [self.delegate passwordDetailsViewController:self
                          didEditPasswordDetails:password
                                 withOldUsername:oldUsername
                                     oldPassword:oldPassword
                                         oldNote:oldNote];

    if (oldUsername != password.username || oldPassword != password.password) {
      DetailsContext detailsContext = password.context;
      // When details was opened from the Password Manager, only log password
      // check actions if the password is compromised.
      if (password_manager::ShouldRecordPasswordCheckUserAction(
              detailsContext, password.compromised)) {
        password_manager::LogEditPassword(
            GetWarningTypeForDetailsContext(detailsContext));
      }
    }
  }
  [self.delegate didFinishEditingPasswordDetails];
  [super editButtonPressed];
  [self reloadData];
}

- (void)showEditViewWithoutAuthentication {
  self.showPasswordWithoutAuth = YES;
  [self editButtonPressed];
}

- (void)setupLeftCancelButton {
  UIBarButtonItem* cancelButton = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemCancel
                           target:self
                           action:@selector(dismissView)];
  self.backButtonItem = cancelButton;
  self.navigationItem.leftBarButtonItem = self.backButtonItem;
}

@end
