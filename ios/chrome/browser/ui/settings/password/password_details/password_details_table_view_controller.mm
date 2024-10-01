// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_details/password_details_table_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/i18n/time_formatting.h"
#import "base/ios/ios_util.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/histogram_macros.h"
#import "base/metrics/user_metrics.h"
#import "base/strings/sys_string_conversions.h"
#import "components/crash/core/common/crash_key.h"
#import "components/password_manager/core/browser/password_manager_metrics_util.h"
#import "components/password_manager/core/common/password_manager_constants.h"
#import "ios/chrome/browser/passwords/model/password_checkup_metrics.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
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
#import "ios/chrome/browser/ui/settings/elements/enterprise_info_popover_view_controller.h"
#import "ios/chrome/browser/ui/settings/password/password_details/cells/table_view_stacked_details_item.h"
#import "ios/chrome/browser/ui/settings/password/password_details/credential_details.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details_consumer.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details_handler.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details_metrics_utils.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details_table_view_constants.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details_table_view_controller+Testing.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details_table_view_controller_delegate.h"
#import "ios/chrome/browser/ui/settings/password/passwords_table_view_constants.h"
#import "ios/chrome/browser/ui/settings/settings_navigation_controller.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/elements/popover_label_view_controller.h"
#import "ios/chrome/common/ui/reauthentication/reauthentication_module.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

using base::UmaHistogramEnumeration;
using password_manager::GetWarningTypeForDetailsContext;
using password_manager::constants::kMaxPasswordNoteLength;
using password_manager::constants::kPasswordManagerAuthValidity;
using password_manager::metrics_util::LogPasswordNoteActionInSettings;
using password_manager::metrics_util::PasswordNoteAction;

namespace {

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierPassword = kSectionIdentifierEnumZero,
  SectionIdentifierSite,
  SectionIdentifierCompromisedInfo,
  SectionIdentifierMoveToAccount,
};

typedef NS_ENUM(NSInteger, PasswordAccessReason) {
  PasswordAccessReasonShow = 0,
  PasswordAccessReasonCopy,
  PasswordAccessReasonEdit,
};

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
      return is_compromised;
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
      return is_muted;
  }
}

}  // namespace

#pragma mark - PasswordDetailsInfoItem

// Contains the website, username and password text items.
@interface PasswordDetailsInfoItem : NSObject

// Displays one or more websites on which this credential is used.
@property(nonatomic, strong) TableViewStackedDetailsItem* websiteItem;

// The text item related to the user display name value.
@property(nonatomic, strong) TableViewTextEditItem* userDisplayNameTextItem;

// The text item related to the username value.
@property(nonatomic, strong) TableViewTextEditItem* usernameTextItem;

// The text item related to the password value.
@property(nonatomic, strong) TableViewTextEditItem* passwordTextItem;

// The text item related to the password note.
@property(nonatomic, strong) TableViewMultiLineTextEditItem* passwordNoteItem;

// The text item related to the creation date value.
@property(nonatomic, strong) TableViewTextEditItem* creationDateTextItem;

// If yes, the footer informing about the max note length is shown.
@property(nonatomic, assign) BOOL isNoteFooterShown;

@end
@implementation PasswordDetailsInfoItem
@end

#pragma mark - PasswordDetailsTableViewController

@interface PasswordDetailsTableViewController () <
    TableViewTextEditItemDelegate,
    TableViewMultiLineTextEditItemDelegate,
    UIEditMenuInteractionDelegate> {
  // Index of the password the user wants to reveal.
  NSInteger _passwordIndexToReveal;

  // Title label displayed in the navigation bar.
  UILabel* _titleLabel;

  // Whether Settings have been dismissed.
  BOOL _settingsAreDismissed;

  // The button for password sharing.
  UIBarButtonItem* _shareButton;
}

// Array of credentials that are shown on the screen.
@property(nonatomic, strong) NSArray<CredentialDetails*>* credentials;

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

// Used to avoid recording the "move to account offered" histogram twice for
// the same credential.
@property(nonatomic, strong)
    NSMutableSet<NSString*>* usernamesWithMoveToAccountOfferRecorded;

// Used to create and show the actions users can execute when they tap on a row
// in the tableView. These actions are displayed a pop-up.
// TODO(crbug.com/40284033): Remove available guard when min deployment target
// is bumped to iOS 16.0.
@property(nonatomic, strong)
    UIEditMenuInteraction* interactionMenu API_AVAILABLE(ios(16));

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

  self.tableView.accessibilityIdentifier = kPasswordDetailsViewControllerID;
  self.tableView.allowsSelectionDuringEditing = YES;

  _interactionMenu = [[UIEditMenuInteraction alloc] initWithDelegate:self];
  [self.tableView addInteraction:self.interactionMenu];
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];
  // Title may change between the call to -init and -viewWillAppear, so we want
  // to wait until the last moment possible before setting the titleView.
  self.navigationItem.titleView = _titleLabel;

  SettingsNavigationController* navigationController =
      base::apple::ObjCCast<SettingsNavigationController>(
          self.navigationController);
  if (!navigationController) {
    return;
  }

  // Add a "Done" button to the navigation bar if this view controller is the
  // first in the navigation stack. This "Done" button's purpose being to
  // dismiss the presented view.
  if (navigationController.viewControllers.count > 0 &&
      navigationController.viewControllers.firstObject == self) {
    UIBarButtonItem* doneButton = [navigationController doneButton];

    // If not in edit mode, set the newly created "Done" button as the left bar
    // button item. Otherwise, don't override the "Cancel" button that's shown
    // when in edit mode.
    if (!self.tableView.editing) {
      self.navigationItem.leftBarButtonItem = doneButton;
    }

    // Set `customLeftBarButtonItem` with the "Done" button, so that it'll be
    // used as the left bar button item when exiting edit mode.
    self.customLeftBarButtonItem = doneButton;
  }
}

- (void)didMoveToParentViewController:(UIViewController*)parent {
  [super didMoveToParentViewController:parent];

  if (!parent) {
    [self.handler passwordDetailsTableViewControllerWasDismissed];
  }
}

#pragma mark - LegacyChromeTableViewController

- (void)editButtonPressed {
  // If there are no passwords or passkeys, proceed with editing without
  // reauthentication.
  if (![self hasAtLeastOnePasswordOrPasskey]) {
    [super editButtonPressed];

    // Reload view to show the delete button.
    [self reloadData];
    return;
  }

  // Enter editing mode.
  if (!self.tableView.editing && !self.isPasswordShown) {
    [self showPasswordFor:PasswordAccessReasonEdit];
    return;
  }

  if (self.tableView.editing) {
    // If password value was changed show confirmation dialog before saving.
    // Editing mode will be exited only if user confirms saving.
    if ([self passwordsDidChange]) {
      DCHECK(self.handler);
      // TODO(crbug.com/40884045): Show Password Edit Dialog when Password
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

  for (CredentialDetails* credentialsDetails in _credentials) {
    [self addPasswordDetailsToModel:credentialsDetails];
  }
}

- (BOOL)showCancelDuringEditing {
  return YES;
}

#pragma mark - Items

- (TableViewStackedDetailsItem*)websiteItemForPasswordDetails:
    (CredentialDetails*)passwordDetails {
  TableViewStackedDetailsItem* item = [[TableViewStackedDetailsItem alloc]
      initWithType:PasswordDetailsItemTypeWebsite];
  item.titleText = l10n_util::GetNSString(IDS_IOS_SHOW_PASSWORD_VIEW_SITES);
  item.detailTexts = passwordDetails.websites;
  item.detailTextColor = [UIColor colorNamed:kTextSecondaryColor];
  item.accessibilityTraits = UIAccessibilityTraitNotEnabled;
  return item;
}

- (TableViewTextEditItem*)usernameItemForPasswordDetails:
    (CredentialDetails*)passwordDetails {
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

  // For testing: only use this custom accessibility identifier if there are
  // more than one password shown on the Password Details.
  if (_credentials.count > 1) {
    item.customTextfieldAccessibilityIdentifier = [NSString
        stringWithFormat:@"%@%@%@", kUsernameTextfieldForPasswordDetailsID,
                         passwordDetails.username, passwordDetails.websites[0]];
  }
  return item;
}

- (TableViewTextEditItem*)userDisplayNameItemForPasswordDetails:
    (CredentialDetails*)passwordDetails {
  TableViewTextEditItem* item = [[TableViewTextEditItem alloc]
      initWithType:PasswordDetailsItemTypeUsername];
  item.textFieldBackgroundColor = [UIColor clearColor];
  item.fieldNameLabelText =
      l10n_util::GetNSString(IDS_IOS_SHOW_PASSKEY_DISPLAY_NAME);
  item.textFieldValue = passwordDetails.userDisplayName;
  item.textFieldEnabled = self.tableView.editing;
  item.autoCapitalizationType = UITextAutocapitalizationTypeNone;
  item.delegate = self;
  item.hideIcon = YES;
  if (!self.tableView.editing) {
    item.textFieldTextColor = [UIColor colorNamed:kTextSecondaryColor];
  }

  // For testing: only use this custom accessibility identifier if there are
  // more than one password shown on the Password Details.
  if (_credentials.count > 1) {
    item.customTextfieldAccessibilityIdentifier = [NSString
        stringWithFormat:@"%@%@%@",
                         kUserDisplayNameTextfieldForPasswordDetailsID,
                         passwordDetails.userDisplayName,
                         passwordDetails.websites[0]];
  }
  return item;
}

- (TableViewTextEditItem*)creationDateItemForPasswordDetails:
    (CredentialDetails*)passwordDetails {
  TableViewTextEditItem* item = [[TableViewTextEditItem alloc]
      initWithType:PasswordDetailsItemTypeUsername];
  item.textFieldBackgroundColor = [UIColor clearColor];
  item.fieldNameLabelText =
      l10n_util::GetNSString(IDS_IOS_SHOW_PASSKEY_CREATION_DATE);
  item.textFieldValue =
      passwordDetails.creationTime.has_value()
          ? l10n_util::GetNSStringF(
                IDS_IOS_PASSKEY_CREATION_DATE,
                base::TimeFormatShortDate(*(passwordDetails.creationTime)))
          : @"";
  item.textFieldEnabled = NO;
  item.autoCapitalizationType = UITextAutocapitalizationTypeNone;
  item.delegate = self;
  item.hideIcon = YES;
  if (!self.tableView.editing) {
    item.textFieldTextColor = [UIColor colorNamed:kTextSecondaryColor];
  }

  // For testing: only use this custom accessibility identifier if there are
  // more than one password shown on the Password Details.
  if (_credentials.count > 1) {
    item.customTextfieldAccessibilityIdentifier = [NSString
        stringWithFormat:@"%@%@%@", kCreationDateTextfieldForPasswordDetailsID,
                         item.textFieldValue, passwordDetails.websites[0]];
  }
  return item;
}

- (TableViewTextEditItem*)passwordItemForPasswordDetails:
    (CredentialDetails*)passwordDetails {
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

  // For testing: only use this custom accessibility identifier if there are
  // more than one password shown on the Password Details.
  if (_credentials.count > 1) {
    item.customTextfieldAccessibilityIdentifier = [NSString
        stringWithFormat:@"%@%@%@", kPasswordTextfieldForPasswordDetailsID,
                         passwordDetails.username, passwordDetails.websites[0]];
  }
  return item;
}

- (TableViewMultiLineTextEditItem*)noteItemForPasswordDetails:
    (CredentialDetails*)passwordDetails {
  TableViewMultiLineTextEditItem* item = [[TableViewMultiLineTextEditItem alloc]
      initWithType:PasswordDetailsItemTypeNote];
  item.label = l10n_util::GetNSString(IDS_IOS_SHOW_PASSWORD_VIEW_NOTE);
  item.text = passwordDetails.note;
  item.editingEnabled = self.tableView.editing;
  item.delegate = self;

  // Make the text field non-interactable in non-editing table view state and
  // when the note is empty, so that UITextView does not capture taps in that
  // case and they can be handled by `didSelectRowAtIndexPath`.
  item.textFieldInteractionEnabled =
      self.tableView.editing || passwordDetails.note.length > 0;
  return item;
}

- (TableViewTextEditItem*)federationItemForPasswordDetails:
    (CredentialDetails*)passwordDetails {
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
  item.accessibilityIdentifier = kCompromisedWarningID;
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
    (CredentialDetails*)passwordDetails {
  TableViewTextItem* item = [[TableViewTextItem alloc]
      initWithType:PasswordDetailsItemTypeDeleteButton];
  int itemText = IDS_IOS_CONFIRM_PASSWORD_DELETION;
  if (self.isBlockedSite) {
    itemText = IDS_IOS_DELETE_ACTION_TITLE;
  } else if (passwordDetails.credentialType == CredentialTypePasskey) {
    itemText = IDS_IOS_CONFIRM_PASSKEY_DELETION;
  }
  item.text = l10n_util::GetNSString(itemText);
  item.textColor = [UIColor colorNamed:kRedColor];
  item.accessibilityTraits = UIAccessibilityTraitButton;
  item.accessibilityIdentifier = [NSString
      stringWithFormat:@"%@%@%@", kDeleteButtonForPasswordDetailsID,
                       passwordDetails.username, passwordDetails.websites[0]];
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
  item.accessibilityIdentifier = kMovePasswordToAccountButtonID;
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
        base::apple::ObjCCastStrict<TableViewTextHeaderFooterView>(view);
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
            base::apple::ObjCCastStrict<TableViewTextEditCell>(cell);
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
            base::apple::ObjCCastStrict<TableViewTextEditCell>(cell);
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
        CredentialDetails* passwordDetails =
            self.credentials[indexPath.section];
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
          base::apple::ObjCCastStrict<TableViewMultiLineTextEditCell>(cell);
      if (!self.tableView.editing && textFieldCell.textView.text.length == 0) {
        [self switchToEditingOnEmptyNoteTapAtIndexPath:indexPath];
      } else {
        [textFieldCell.textView becomeFirstResponder];
      }
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
        [self moveCredentialToAccountStore:indexPath.section anchorView:cell];
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
  CGRect row = [tableView rectForRowAtIndexPath:indexPath];
  CGPoint editMenuLocation =
      CGPointMake(row.origin.x + row.size.width / 2, row.origin.y);
  UIEditMenuConfiguration* configuration = [UIEditMenuConfiguration
      configurationWithIdentifier:[NSNumber numberWithInt:itemType]
                      sourcePoint:editMenuLocation];
  [self.interactionMenu presentEditMenuWithConfiguration:configuration];
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
      return self.editing;
    case PasswordDetailsItemTypeNote: {
      UITableViewCell* cell = [self.tableView cellForRowAtIndexPath:indexPath];
      TableViewMultiLineTextEditCell* textFieldCell =
          base::apple::ObjCCastStrict<TableViewMultiLineTextEditCell>(cell);
      return self.editing || textFieldCell.textView.text.length == 0;
    }
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
          base::apple::ObjCCastStrict<TableViewTextEditCell>(cell);
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
          base::apple::ObjCCastStrict<TableViewTextEditCell>(cell);
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
              containsObject:self.credentials[indexPath.section].username]) {
        [self.usernamesWithMoveToAccountOfferRecorded
            addObject:self.credentials[indexPath.section].username];
        // TODO(crbug.com/40880533): Use a common function for recording sites.
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

#pragma mark - SettingsControllerProtocol

- (void)reportDismissalUserAction {
  base::RecordAction(
      base::UserMetricsAction("MobilePasswordDetailsSettingsClose"));
}

- (void)reportBackUserAction {
  base::RecordAction(
      base::UserMetricsAction("MobilePasswordDetailsSettingsBack"));
}

- (void)settingsWillBeDismissed {
  DCHECK(!_settingsAreDismissed);

  _settingsAreDismissed = YES;
}

#pragma mark - PasswordDetailsConsumer

- (void)setCredentials:(NSArray<CredentialDetails*>*)credentials
              andTitle:(NSString*)title {
  BOOL hadCredentials = [_credentials count];
  _credentials = credentials;
  _pageTitle = title;

  [self updateNavigationTitle];
  // Update the model even if all credentials are deleted and the view
  // controller will be dismissed. UIKit could still trigger events that execute
  // CHECK in this file that would fail if `_credentials` and the model are not
  // in sync.
  [self reloadData];

  if (![credentials count]) {
    // onAllPasswordsDeleted() mustn't be called twice.
    if (hadCredentials) {
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

- (void)setupRightShareButton:(BOOL)policyEnabled {
  SEL selector = policyEnabled ? @selector(onShareButtonPressed)
                               : @selector(onPolicyDisabledShareButtonPressed:);
  UIBarButtonItem* shareButton = [[UIBarButtonItem alloc]
      initWithImage:DefaultSymbolWithPointSize(kShareSymbol,
                                               kSymbolActionPointSize)
              style:UIBarButtonItemStylePlain
             target:self
             action:selector];
  shareButton.accessibilityIdentifier = kPasswordShareButtonID;
  shareButton.enabled = [self hasAtLeastOnePassword];
  _shareButton = shareButton;
  self.navigationItem.rightBarButtonItems =
      @[ self.navigationItem.rightBarButtonItem, shareButton ];
}

#pragma mark - TableViewTextEditItemDelegate

- (void)tableViewItemDidBeginEditing:(TableViewTextEditItem*)tableViewItem {
  [self reconfigureCellsForItems:@[ tableViewItem ]];
}

- (void)tableViewItemDidChange:(TableViewTextEditItem*)tableViewItem {
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
        base::apple::ObjCCastStrict<TableViewTextHeaderFooterView>(footer);
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

#pragma mark - SettingsRootTableViewController

- (BOOL)shouldHideToolbar {
  return YES;
}

- (void)updateUIForEditState {
  [super updateUIForEditState];

  // Share button should be hidden when editing.
  _shareButton.hidden = self.tableView.editing;
}

#pragma mark - Private

// Applies tint colour and resizes image.
- (UIImage*)compromisedIcon {
  return DefaultSymbolTemplateWithPointSize(kErrorCircleFillSymbol,
                                            kRecommendationSymbolSize);
}

// Reveals password to the user.
- (void)showPasswordFor:(PasswordAccessReason)reason {
  switch (reason) {
    case PasswordAccessReasonShow: {
      self.passwordShown = YES;
      self.passwordDetailsInfoItems[_passwordIndexToReveal]
          .passwordTextItem.textFieldValue =
          self.credentials[_passwordIndexToReveal].password;
      self.passwordDetailsInfoItems[_passwordIndexToReveal]
          .passwordTextItem.identifyingIcon =
          DefaultSymbolWithPointSize(kHideActionSymbol, kSymbolSize);
      self.passwordDetailsInfoItems[_passwordIndexToReveal]
          .passwordTextItem.identifyingIconAccessibilityLabel =
          l10n_util::GetNSString(IDS_IOS_SETTINGS_PASSWORD_HIDE_BUTTON);
      [self reconfigureCellsForItems:@[
        self.passwordDetailsInfoItems[_passwordIndexToReveal].passwordTextItem
      ]];

      CredentialDetails* passwordDetails =
          self.credentials[_passwordIndexToReveal];
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
    case PasswordAccessReasonCopy: {
      NSString* copiedString =
          self.credentials[self.tableView.indexPathForSelectedRow.section]
              .password;
      StoreTextInPasteboard(copiedString);

      [self showToast:l10n_util::GetNSString(
                          IDS_IOS_SETTINGS_PASSWORD_WAS_COPIED_MESSAGE)
           forSuccess:YES];
      break;
    }
    case PasswordAccessReasonEdit:
      // Called super because we want to update only `tableView.editing`.
      [super editButtonPressed];
      [self reloadData];
      break;
  }
  [self logPasswordAccessWith:reason];
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
  DCHECK(self.credentials.count == self.passwordDetailsInfoItems.count);

  for (NSUInteger i = 0; i < self.passwordDetailsInfoItems.count; i++) {
    NSString* newUsernameValue =
        self.passwordDetailsInfoItems[i].usernameTextItem.textFieldValue;
    BOOL usernameChanged =
        ![newUsernameValue isEqualToString:self.credentials[i].username];
    BOOL showUsernameAlreadyUsed =
        usernameChanged &&
        [self.delegate isUsernameReused:newUsernameValue
                              forDomain:self.credentials[i].signonRealm];
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
  DCHECK(self.credentials.count == self.passwordDetailsInfoItems.count);

  for (NSUInteger i = 0; i < self.passwordDetailsInfoItems.count; i++) {
    if (self.credentials[i].credentialType == CredentialTypePasskey) {
      continue;
    }

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
  DCHECK(self.credentials.count == self.passwordDetailsInfoItems.count);

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
  for (CredentialDetails* credentialDetails in self.credentials) {
    if (credentialDetails.password.length > 0) {
      return YES;
    }
  }
  return NO;
}

- (BOOL)hasAtLeastOnePasswordOrPasskey {
  for (CredentialDetails* credentialDetails in self.credentials) {
    if (credentialDetails.credentialType == CredentialTypePasskey ||
        credentialDetails.password.length > 0) {
      return YES;
    }
  }
  return NO;
}

- (BOOL)passwordsDidChange {
  DCHECK(self.credentials.count == self.passwordDetailsInfoItems.count);

  for (NSUInteger i = 0; i < self.passwordDetailsInfoItems.count; i++) {
    if (self.credentials[i].credentialType != CredentialTypePasskey &&
        ![self.credentials[i].password
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
    CredentialDetails* firstPassword = self.credentials.firstObject;
    self.pageTitle = firstPassword.origins.firstObject;
  }
  _titleLabel.text = self.pageTitle;
}

// Creates the model items corresponding to a `PasswordDetails` and adds them to
// the `model`.
- (void)addPasswordDetailsToModel:(CredentialDetails*)credentialDetails {
  TableViewModel* model = self.tableViewModel;
  PasswordDetailsInfoItem* passwordItem =
      [[PasswordDetailsInfoItem alloc] init];

  NSInteger sectionForWebsite;
  NSInteger sectionForPassword;
  NSInteger sectionForCompromisedInfo;
  NSInteger sectionForMoveCredential;

  // Password details are displayed in its own section when Grouping is enabled.
  NSInteger nextSection = kSectionIdentifierEnumZero + [model numberOfSections];
  [model addSectionWithIdentifier:nextSection];

  sectionForWebsite = nextSection;
  sectionForPassword = nextSection;
  sectionForCompromisedInfo = nextSection;
  sectionForMoveCredential = nextSection;

  // Add sites to section.
  passwordItem.websiteItem =
      [self websiteItemForPasswordDetails:credentialDetails];
  [model addItem:passwordItem.websiteItem
      toSectionWithIdentifier:sectionForWebsite];

  // Add username and password to section according to credential type.
  switch (credentialDetails.credentialType) {
    case CredentialTypeRegularPassword: {
      passwordItem.usernameTextItem =
          [self usernameItemForPasswordDetails:credentialDetails];
      [model addItem:passwordItem.usernameTextItem
          toSectionWithIdentifier:sectionForPassword];

      passwordItem.passwordTextItem =
          [self passwordItemForPasswordDetails:credentialDetails];
      [model addItem:passwordItem.passwordTextItem
          toSectionWithIdentifier:sectionForPassword];

      passwordItem.passwordNoteItem =
          [self noteItemForPasswordDetails:credentialDetails];
      [model addItem:passwordItem.passwordNoteItem
          toSectionWithIdentifier:sectionForPassword];

      passwordItem.isNoteFooterShown =
          self.tableView.editing && passwordItem.passwordNoteItem.text.length >=
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

      if (credentialDetails.isCompromised || credentialDetails.isMuted) {
        [model addItem:[self changePasswordRecommendationItem]
            toSectionWithIdentifier:sectionForCompromisedInfo];

        if (credentialDetails.changePasswordURL.has_value()) {
          [model addItem:[self changePasswordItem]
              toSectionWithIdentifier:sectionForCompromisedInfo];
        }

        if (ShouldAllowToDismissWarning(credentialDetails.context,
                                        credentialDetails.compromised)) {
          [model addItem:[self dismissWarningItem]
              toSectionWithIdentifier:sectionForCompromisedInfo];
        } else if (ShouldAllowToRestoreWarning(credentialDetails.context,
                                               credentialDetails.muted)) {
          [model addItem:[self restoreWarningItem]
              toSectionWithIdentifier:sectionForCompromisedInfo];
        }
      }
      break;
    }
    case CredentialTypeFederation: {
      passwordItem.usernameTextItem =
          [self usernameItemForPasswordDetails:credentialDetails];
      [model addItem:passwordItem.usernameTextItem
          toSectionWithIdentifier:sectionForPassword];

      // Federated password forms don't have password value.
      [model addItem:[self federationItemForPasswordDetails:credentialDetails]
          toSectionWithIdentifier:sectionForPassword];
      break;
    }

    case CredentialTypeBlocked: {
      break;
    }

    case CredentialTypePasskey: {
      passwordItem.userDisplayNameTextItem =
          [self userDisplayNameItemForPasswordDetails:credentialDetails];
      [model addItem:passwordItem.userDisplayNameTextItem
          toSectionWithIdentifier:sectionForPassword];

      passwordItem.usernameTextItem =
          [self usernameItemForPasswordDetails:credentialDetails];
      [model addItem:passwordItem.usernameTextItem
          toSectionWithIdentifier:sectionForPassword];

      passwordItem.creationDateTextItem =
          [self creationDateItemForPasswordDetails:credentialDetails];
      [model addItem:passwordItem.creationDateTextItem
          toSectionWithIdentifier:sectionForPassword];
      break;
    }
  }

  if (credentialDetails.shouldOfferToMoveToAccount) {
    [model addItem:[self moveToAccountRecommendationItem]
        toSectionWithIdentifier:sectionForMoveCredential];
    [model addItem:[self moveToAccountButtonItem]
        toSectionWithIdentifier:sectionForMoveCredential];
  }

  if (self.tableView.editing) {
    [model addItem:[self deleteButtonItemForPasswordDetails:credentialDetails]
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
  [self.handler moveCredentialToAccountStore:self.credentials[passwordIndex]
                                  anchorView:anchorView
                             movedCompletion:^{
                               [weakSelf showToast:toastMessage forSuccess:YES];
                             }];
}

// Notifies the handler that the share button was pressed by the user.
- (void)onShareButtonPressed {
  CHECK(self.handler);
  [self.handler onShareButtonPressed];
}

// Displays the popup informing that password sharing is disabled by the
// administrator.
- (void)onPolicyDisabledShareButtonPressed:(UIBarButtonItem*)button {
  EnterpriseInfoPopoverViewController* popoverViewController =
      [[EnterpriseInfoPopoverViewController alloc]
                 initWithMessage:
                     l10n_util::GetNSString(
                         IDS_IOS_PASSWORD_SHARING_ENTERPRISE_POLICY_DISABLED_MESSAGE)
                  enterpriseName:nil
          isPresentingFromButton:YES
                addLearnMoreLink:NO];
  popoverViewController.popoverPresentationController.barButtonItem = button;
  popoverViewController.popoverPresentationController.permittedArrowDirections =
      UIPopoverArrowDirectionAny;

  [self presentViewController:popoverViewController
                     animated:YES
                   completion:nil];
}

// Switches to editing mode and makes the note text field at `indexPath` the
// first responder.
- (void)switchToEditingOnEmptyNoteTapAtIndexPath:(NSIndexPath*)indexPath {
  [super editButtonPressed];
  [self reloadData];

  // After switching to editing mode, the note that was originally tapped might
  // not be visible on the screen anymore when there's a lot of credential
  // groups (since there are additional elements appearing, e.g. "Delete
  // password" buttons).
  UITableView* tableView = self.tableView;
  [tableView scrollToRowAtIndexPath:indexPath
                   atScrollPosition:UITableViewScrollPositionBottom
                           animated:NO];
  UITableViewCell* cell = [tableView cellForRowAtIndexPath:indexPath];
  TableViewMultiLineTextEditCell* textFieldCell =
      base::apple::ObjCCastStrict<TableViewMultiLineTextEditCell>(cell);
  [textFieldCell.textView becomeFirstResponder];
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

#pragma mark - UIEditMenuInteractionDelegate

// TODO(crbug.com/40284033): Remove available guard when min deployment target
// is bumped to iOS 16.0.
- (UIMenu*)editMenuInteraction:(UIEditMenuInteraction*)interaction
          menuForConfiguration:(UIEditMenuConfiguration*)configuration
              suggestedActions:(NSArray<UIMenuElement*>*)suggestedActions
    API_AVAILABLE(ios(16)) {
  NSUInteger itemType =
      [base::apple::ObjCCast<NSNumber>(configuration.identifier) intValue];

  // If `configuration.identifier` can't be casted to an NSNumber, it probably
  // means that the current function was triggered by the system, and so it
  // shouldn't be acted upon.
  if (!itemType) {
    return nil;
  }

  UIAction* copy = [UIAction
      actionWithTitle:l10n_util::GetNSString(
                          IDS_IOS_SETTINGS_SITE_COPY_MENU_ITEM)
                image:nil
           identifier:nil
              handler:^(__kindof UIAction* _Nonnull action) {
                base::RecordAction(
                    base::UserMetricsAction("MobilePasswordDetailsCopy"));
                [self copyPasswordDetailsHelper:itemType];
              }];
  return [UIMenu menuWithChildren:@[ copy ]];
}

#pragma mark - Actions

// Called when the user tapped on the show/hide button near password.
- (void)didTapShowHideButton:(UIButton*)buttonView {
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
    [self showPasswordFor:PasswordAccessReasonShow];
    base::RecordAction(
        base::UserMetricsAction("MobilePasswordDetailsViewPassword"));
  }
}

// Called when the user tap error info icon in the username input.
- (void)didTapUsernameErrorInfo:(UIButton*)buttonView {
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

// A helper function that copies the password information to system pasteboard
// and shows a toast of success/failure.
- (void)copyPasswordDetailsHelper:(NSInteger)itemType {
  NSString* message = nil;

  switch (itemType) {
    case PasswordDetailsItemTypeWebsite: {
      CredentialDetails* detailsToCopy;
      detailsToCopy =
          self.credentials[self.tableView.indexPathForSelectedRow.section];
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
          self.credentials[self.tableView.indexPathForSelectedRow.section]
              .username;

      StoreTextInPasteboard(copiedString);
      message =
          l10n_util::GetNSString(IDS_IOS_SETTINGS_USERNAME_WAS_COPIED_MESSAGE);
      break;
    }
    case PasswordDetailsItemTypeFederation: {
      NSString* copiedString =
          self.credentials[self.tableView.indexPathForSelectedRow.section]
              .federation;
      StoreTextInPasteboard(copiedString);
      return;
    }
    case PasswordDetailsItemTypePassword: {
      [self showPasswordFor:PasswordAccessReasonCopy];
      return;
    }
  }

  if (message.length) {
    [self showToast:message forSuccess:YES];
  }
}

- (void)didTapDismissWarningButtonAtPasswordIndex:(NSUInteger)passwordIndex {
  CHECK(passwordIndex >= 0 && passwordIndex < self.credentials.count);
  CHECK(self.delegate);

  password_manager::LogMuteCompromisedWarning();

  [self.delegate dismissWarningForPassword:self.credentials[passwordIndex]];
}

- (void)didTapRestoreWarningButtonAtPasswordIndex:(NSUInteger)passwordIndex {
  CHECK(passwordIndex >= 0 && passwordIndex < self.credentials.count);
  CHECK(self.delegate);

  password_manager::LogUnmuteCompromisedWarning();

  [self.delegate restoreWarningForCurrentPassword];
}

- (void)didTapDeleteButton:(UITableViewCell*)cell
           atPasswordIndex:(NSUInteger)passwordIndex {
  CHECK(passwordIndex >= 0 && passwordIndex < self.credentials.count);
  CHECK(self.handler);
  [self.handler showCredentialDeleteDialogWithCredentialDetails:
                    self.credentials[passwordIndex]
                                                     anchorView:cell];
}

- (void)dismissView {
  [self.view endEditing:YES];
  [self.handler dismissPasswordDetailsTableViewController];
}

#pragma mark - Metrics

- (void)logPasswordAccessWith:(PasswordAccessReason)reason {
  switch (reason) {
    case PasswordAccessReasonShow:
      UMA_HISTOGRAM_ENUMERATION(
          "PasswordManager.AccessPasswordInSettings",
          password_manager::metrics_util::ACCESS_PASSWORD_VIEWED,
          password_manager::metrics_util::ACCESS_PASSWORD_COUNT);
      break;
    case PasswordAccessReasonCopy:
      UMA_HISTOGRAM_ENUMERATION(
          "PasswordManager.AccessPasswordInSettings",
          password_manager::metrics_util::ACCESS_PASSWORD_COPIED,
          password_manager::metrics_util::ACCESS_PASSWORD_COUNT);
      break;
    case PasswordAccessReasonEdit:
      UMA_HISTOGRAM_ENUMERATION(
          "PasswordManager.AccessPasswordInSettings",
          password_manager::metrics_util::ACCESS_PASSWORD_EDITED,
          password_manager::metrics_util::ACCESS_PASSWORD_COUNT);
      break;
  }
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
  DCHECK(self.credentials.count == self.passwordDetailsInfoItems.count);
  for (NSUInteger i = 0; i < self.passwordDetailsInfoItems.count; i++) {
    CredentialDetails* credential = self.credentials[i];
    NSString* oldUsername = credential.username;
    NSString* oldUserDisplayName = credential.userDisplayName;
    NSString* oldPassword = credential.password;
    NSString* oldNote = credential.note;

    PasswordDetailsInfoItem* passwordDetailsInfoItem =
        self.passwordDetailsInfoItems[i];

    credential.username =
        passwordDetailsInfoItem.usernameTextItem.textFieldValue;
    credential.userDisplayName =
        passwordDetailsInfoItem.userDisplayNameTextItem.textFieldValue;
    credential.password =
        passwordDetailsInfoItem.passwordTextItem.textFieldValue;
    credential.note = passwordDetailsInfoItem.passwordNoteItem.text;

    [self logChangeBetweenOldNote:oldNote currentNote:credential.note];
    [self.delegate passwordDetailsViewController:self
                        didEditCredentialDetails:credential
                                 withOldUsername:oldUsername
                              oldUserDisplayName:oldUserDisplayName
                                     oldPassword:oldPassword
                                         oldNote:oldNote];

    if (credential.credentialType != CredentialTypePasskey &&
        (oldUsername != credential.username ||
         oldPassword != credential.password)) {
      DetailsContext detailsContext = credential.context;
      // When details was opened from the Password Manager, only log password
      // check actions if the password is compromised.
      if (password_manager::ShouldRecordPasswordCheckUserAction(
              detailsContext, credential.compromised)) {
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

- (void)showShareButton {
  self.navigationItem.rightBarButtonItems =
      @[ self.navigationItem.rightBarButtonItem, _shareButton ];
}

- (void)showSpinnerOnRightNavigationBar {
  UIActivityIndicatorView* spinner = GetMediumUIActivityIndicatorView();
  [spinner startAnimating];
  UIBarButtonItem* spinnerBarButtonItem =
      [[UIBarButtonItem alloc] initWithCustomView:spinner];
  self.navigationItem.rightBarButtonItems =
      @[ self.navigationItem.rightBarButtonItem, spinnerBarButtonItem ];
}

@end
