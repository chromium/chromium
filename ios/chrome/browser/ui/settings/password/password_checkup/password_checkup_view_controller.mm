// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_checkup/password_checkup_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/metrics/user_metrics.h"
#import "base/strings/string_number_conversions.h"
#import "components/google/core/common/google_util.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/net/model/crurl.h"
#import "ios/chrome/browser/passwords/model/password_checkup_metrics.h"
#import "ios/chrome/browser/passwords/model/password_checkup_utils.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_item.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/settings/cells/settings_check_cell.h"
#import "ios/chrome/browser/ui/settings/cells/settings_check_item.h"
#import "ios/chrome/browser/ui/settings/password/password_checkup/password_checkup_commands.h"
#import "ios/chrome/browser/ui/settings/password/password_checkup/password_checkup_constants.h"
#import "ios/chrome/browser/ui/settings/password/password_checkup/password_checkup_consumer.h"
#import "ios/chrome/browser/ui/settings/password/password_checkup/password_checkup_view_controller_delegate.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

using password_manager::InsecurePasswordCounts;
using password_manager::WarningType;

namespace {

// Height of the image used as a header for the table view.
constexpr CGFloat kHeaderImageHeight = 99;

// Sections of the Password Checkup Homepage UI.
typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierInsecureTypes = kSectionIdentifierEnumZero,
  SectionIdentifierLastPasswordCheckup,
  SectionIdentifierNotificationsOptIn,
};

// Items within the Password Checkup Homepage UI.
typedef NS_ENUM(NSInteger, ItemType) {
  // Section: SectionIdentifierInsecureTypes
  ItemTypeCompromisedPasswords = kItemTypeEnumZero,
  ItemTypeReusedPasswords,
  ItemTypeWeakPasswords,
  // Section: SectionIdentifierLastPasswordCheckup
  ItemTypePasswordCheckupTimestamp,
  ItemTypeCheckPasswordsButton,
  ItemTypePasswordCheckupDescriptionFooter,
  // Section: SectionIdentifierNotificationsOptIn
  ItemTypeNotificationsOptIn,
  ItemTypeNotificationsDescriptionFooter
};

// Helper method to get the right header image depending on the
// `password_checkup_state`.
UIImage* GetHeaderImage(PasswordCheckupHomepageState password_checkup_state,
                        InsecurePasswordCounts counts) {
  bool has_compromised_passwords = counts.compromised_count > 0;
  bool has_insecure_passwords =
      counts.compromised_count > 0 || counts.dismissed_count > 0 ||
      counts.reused_count > 0 || counts.weak_count > 0;
  switch (password_checkup_state) {
    case PasswordCheckupHomepageStateDone:
      if (has_compromised_passwords) {
        return [UIImage
            imageNamed:password_manager::kPasswordCheckupHeaderImageRed];
      } else if (has_insecure_passwords) {
        return [UIImage
            imageNamed:password_manager::kPasswordCheckupHeaderImageYellow];
      }
      return [UIImage
          imageNamed:password_manager::kPasswordCheckupHeaderImageGreen];
    case PasswordCheckupHomepageStateRunning:
      return [UIImage
          imageNamed:password_manager::kPasswordCheckupHeaderImageLoading];
    case PasswordCheckupHomepageStateDisabled:
      return nil;
  }
}

NSString* GetCompromisedPasswordsItemDetailText(bool has_compromised_passwords,
                                                bool has_dismissed_warnings,
                                                int dismissed_count) {
  NSString* detailText;
  if (has_compromised_passwords) {
    detailText = l10n_util::GetNSString(
        IDS_IOS_PASSWORD_CHECKUP_HOMEPAGE_COMPROMISED_PASSWORDS_SUBTITLE);
  } else if (has_dismissed_warnings) {
    detailText = l10n_util::GetPluralNSStringF(
        IDS_IOS_PASSWORD_CHECKUP_HOMEPAGE_DISMISSED_WARNINGS_SUBTITLE,
        dismissed_count);
  } else {
    detailText = l10n_util::GetNSString(
        IDS_IOS_PASSWORD_CHECKUP_HOMEPAGE_NO_COMPROMISED_PASSWORDS_SUBTITLE);
  }
  return detailText;
}

// Sets up the trailing icon and the accessory type of the given `item`
// depending on the `password_checkup_state`, whether there are insecure
// passwords and whether the insecure passwords are compromised. This is used to
// set up the items in the insecure types section.
void SetUpTrailingIconAndAccessoryType(
    PasswordCheckupHomepageState password_checkup_state,
    SettingsCheckItem* item,
    BOOL has_insecure_passwords,
    BOOL has_compromised_passwords = false) {
  item.indicatorHidden = YES;
  item.accessoryType = UITableViewCellAccessoryNone;

  switch (password_checkup_state) {
    case PasswordCheckupHomepageStateDone:
      if (has_insecure_passwords) {
        item.warningState = has_compromised_passwords
                                ? WarningState::kSevereWarning
                                : WarningState::kWarning;
        item.accessoryType = UITableViewCellAccessoryDisclosureIndicator;
      } else {
        item.warningState = WarningState::kSafe;
      }
      break;
    case PasswordCheckupHomepageStateRunning: {
      item.trailingImage = nil;
      item.indicatorHidden = NO;
      break;
    }
    case PasswordCheckupHomepageStateDisabled:
      break;
  }
}

// Returns the appropriate text for the Safety Check notifications opt-in item
// based on the `enabled` state. If notifications are `enabled`, the text
// prompts the user to "Turn off" notifications; otherwise, it prompts them to
// "Turn on" notifications.
NSString* NotificationsOptInItemText(BOOL enabled) {
  if (enabled) {
    return l10n_util::GetNSString(
        IDS_IOS_SAFETY_CHECK_NOTIFICATIONS_TURN_OFF_NOTIFICATIONS_ELLIPSIS);
  }

  return l10n_util::GetNSString(
      IDS_IOS_SAFETY_CHECK_NOTIFICATIONS_TURN_ON_NOTIFICATIONS_ELLIPSIS);
}

}  // namespace

@interface PasswordCheckupViewController () {
  // Whether the consumer has been updated at least once.
  BOOL _consumerHasBeenUpdated;

  // The item related to the compromised passwords.
  SettingsCheckItem* _compromisedPasswordsItem;

  // The item related to the reused passwords.
  SettingsCheckItem* _reusedPasswordsItem;

  // The item related to the weak passwords.
  SettingsCheckItem* _weakPasswordsItem;

  // The item related to the timestamp of the last password check.
  SettingsCheckItem* _passwordCheckupTimestampItem;

  // The button to start password check.
  TableViewTextItem* _checkPasswordsButtonItem;

  // The button to opt-in to Safety Check notifications.
  TableViewTextItem* _notificationsOptInItem;

  // Whether Safety Check notifications are enabled or not.
  BOOL _safetyCheckNotificationsEnabled;

  // The footer item briefly explaining the purpose of Safety Check
  // notifications.
  TableViewLinkHeaderFooterItem* _notificationsDescriptionFooterItem;

  // The footer item briefly explaining the purpose of Password Checkup.
  TableViewLinkHeaderFooterItem* _passwordCheckupDescriptionFooterItem;

  // Whether Settings have been dismissed.
  BOOL _settingsAreDismissed;

  // Current PasswordCheckupHomepageState.
  PasswordCheckupHomepageState _passwordCheckupState;

  // Password counts associated with the different insecure types.
  InsecurePasswordCounts _insecurePasswordCounts;

  // The string containing the timestamp of the last completed check.
  NSString* _formattedElapsedTimeSinceLastCheck;

  // The number of affiliated groups for which the user has saved passwords.
  NSInteger _affiliatedGroupCount;

  // Image view at the top of the screen, indicating the overall Password
  // Checkup status.
  UIImageView* _headerImageView;

  // Whether the previous password checkup state was the running state.
  BOOL _wasRunning;
}

@end

@implementation PasswordCheckupViewController

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  self.tableView.accessibilityIdentifier =
      password_manager::kPasswordCheckupTableViewId;

  self.title = l10n_util::GetNSString(IDS_IOS_PASSWORD_CHECKUP);

  _headerImageView = [self createHeaderImageView];
  [self updateHeaderImage];
  [self updateTableViewHeaderView];

  [self loadModel];

  if (@available(iOS 17, *)) {
    NSArray<UITrait>* traits =
        TraitCollectionSetForTraits(@[ UITraitVerticalSizeClass.self ]);
    [self registerForTraitChanges:traits
                       withAction:@selector(updateUIOnTraitChange)];
  }
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];
  // Update the navigation bar background color as it is different for the
  // PasswordCheckupViewController than for its parent.
  [self updateNavigationBarBackgroundColorForDismissal:NO];
}

- (void)willMoveToParentViewController:(UIViewController*)parent {
  [super willMoveToParentViewController:parent];
  if (!parent) {
    // Reset the navigation bar background color to what it was before getting
    // to the PasswordCheckupViewController.
    [self updateNavigationBarBackgroundColorForDismissal:YES];
  }
}

- (void)didMoveToParentViewController:(UIViewController*)parent {
  [super didMoveToParentViewController:parent];
  if (!parent) {
    [self.handler dismissPasswordCheckupViewController];
  }
}

#if !defined(__IPHONE_17_0) || __IPHONE_OS_VERSION_MIN_REQUIRED < __IPHONE_17_0
- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  if (@available(iOS 17, *)) {
    return;
  }

  if (self.traitCollection.verticalSizeClass !=
      previousTraitCollection.verticalSizeClass) {
    [self updateUIOnTraitChange];
  }
}
#endif

#pragma mark - SettingsRootTableViewController

- (void)loadModel {
  [super loadModel];

  TableViewModel* model = self.tableViewModel;

  // Insecure types section.
  [model addSectionWithIdentifier:SectionIdentifierInsecureTypes];

  if (!_compromisedPasswordsItem) {
    _compromisedPasswordsItem = [self compromisedPasswordsItem];
  }
  [model addItem:_compromisedPasswordsItem
      toSectionWithIdentifier:SectionIdentifierInsecureTypes];

  if (!_reusedPasswordsItem) {
    _reusedPasswordsItem = [self reusedPasswordsItem];
  }
  [model addItem:_reusedPasswordsItem
      toSectionWithIdentifier:SectionIdentifierInsecureTypes];

  if (!_weakPasswordsItem) {
    _weakPasswordsItem = [self weakPasswordsItem];
  }
  [model addItem:_weakPasswordsItem
      toSectionWithIdentifier:SectionIdentifierInsecureTypes];

  // Last password checkup section.
  [model addSectionWithIdentifier:SectionIdentifierLastPasswordCheckup];

  if (!_passwordCheckupTimestampItem) {
    _passwordCheckupTimestampItem = [self passwordCheckupTimestampItem];
  }
  [model addItem:_passwordCheckupTimestampItem
      toSectionWithIdentifier:SectionIdentifierLastPasswordCheckup];

  if (!_checkPasswordsButtonItem) {
    _checkPasswordsButtonItem = [self checkPasswordsButtonItem];
  }
  [model addItem:_checkPasswordsButtonItem
      toSectionWithIdentifier:SectionIdentifierLastPasswordCheckup];

  if (!_passwordCheckupDescriptionFooterItem) {
    _passwordCheckupDescriptionFooterItem =
        [self passwordCheckupDescriptionFooterItem];
  }
  [model setFooter:_passwordCheckupDescriptionFooterItem
      forSectionWithIdentifier:SectionIdentifierLastPasswordCheckup];

  // Notifications opt-in section.
  if (IsSafetyCheckNotificationsEnabled()) {
    [model addSectionWithIdentifier:SectionIdentifierNotificationsOptIn];

    if (!_notificationsOptInItem) {
      _notificationsOptInItem = [self notificationsOptInItem];
    }

    [model addItem:_notificationsOptInItem
        toSectionWithIdentifier:SectionIdentifierNotificationsOptIn];

    if (!_notificationsDescriptionFooterItem) {
      _notificationsDescriptionFooterItem =
          [self notificationsDescriptionFooterItem];
    }

    [model setFooter:_notificationsDescriptionFooterItem
        forSectionWithIdentifier:SectionIdentifierNotificationsOptIn];
  }

  if (_consumerHasBeenUpdated) {
    [self updateItemsDependingOnPasswordCheckupState];
  }
  [self updatePasswordCheckupTimestampDetailText];
}

#pragma mark - Items

- (SettingsCheckItem*)compromisedPasswordsItem {
  SettingsCheckItem* compromisedPasswordsItem =
      [[SettingsCheckItem alloc] initWithType:ItemTypeCompromisedPasswords];
  compromisedPasswordsItem.enabled = YES;
  compromisedPasswordsItem.indicatorHidden = YES;
  compromisedPasswordsItem.infoButtonHidden = YES;
  compromisedPasswordsItem.accessibilityIdentifier =
      password_manager::kPasswordCheckupCompromisedPasswordsItemId;
  return compromisedPasswordsItem;
}

- (SettingsCheckItem*)reusedPasswordsItem {
  SettingsCheckItem* reusedPasswordsItem =
      [[SettingsCheckItem alloc] initWithType:ItemTypeReusedPasswords];
  reusedPasswordsItem.enabled = YES;
  reusedPasswordsItem.indicatorHidden = YES;
  reusedPasswordsItem.infoButtonHidden = YES;
  reusedPasswordsItem.accessibilityIdentifier =
      password_manager::kPasswordCheckupReusedPasswordsItemId;
  return reusedPasswordsItem;
}

- (SettingsCheckItem*)weakPasswordsItem {
  SettingsCheckItem* weakPasswordsItem =
      [[SettingsCheckItem alloc] initWithType:ItemTypeWeakPasswords];
  weakPasswordsItem.enabled = YES;
  weakPasswordsItem.indicatorHidden = YES;
  weakPasswordsItem.infoButtonHidden = YES;
  weakPasswordsItem.accessibilityIdentifier =
      password_manager::kPasswordCheckupWeakPasswordsItemId;
  return weakPasswordsItem;
}

- (SettingsCheckItem*)passwordCheckupTimestampItem {
  SettingsCheckItem* passwordCheckupTimestampItem =
      [[SettingsCheckItem alloc] initWithType:ItemTypePasswordCheckupTimestamp];
  passwordCheckupTimestampItem.enabled = YES;
  passwordCheckupTimestampItem.indicatorHidden = YES;
  passwordCheckupTimestampItem.infoButtonHidden = YES;
  return passwordCheckupTimestampItem;
}

- (TableViewTextItem*)checkPasswordsButtonItem {
  TableViewTextItem* checkPasswordsButtonItem =
      [[TableViewTextItem alloc] initWithType:ItemTypeCheckPasswordsButton];
  checkPasswordsButtonItem.text = l10n_util::GetNSString(
      IDS_IOS_PASSWORD_CHECKUP_HOMEPAGE_CHECK_AGAIN_BUTTON);
  checkPasswordsButtonItem.textColor = [UIColor colorNamed:kBlueColor];
  checkPasswordsButtonItem.accessibilityTraits = UIAccessibilityTraitButton;
  return checkPasswordsButtonItem;
}

- (TableViewTextItem*)notificationsOptInItem {
  CHECK(IsSafetyCheckNotificationsEnabled());

  TableViewTextItem* notificationsOptInItem =
      [[TableViewTextItem alloc] initWithType:ItemTypeNotificationsOptIn];
  notificationsOptInItem.text =
      NotificationsOptInItemText(_safetyCheckNotificationsEnabled);
  notificationsOptInItem.textColor = [UIColor colorNamed:kBlueColor];
  notificationsOptInItem.accessibilityTraits = UIAccessibilityTraitButton;

  return notificationsOptInItem;
}

- (TableViewLinkHeaderFooterItem*)notificationsDescriptionFooterItem {
  CHECK(IsSafetyCheckNotificationsEnabled());

  TableViewLinkHeaderFooterItem* footerItem =
      [[TableViewLinkHeaderFooterItem alloc]
          initWithType:ItemTypeNotificationsDescriptionFooter];
  footerItem.text = l10n_util::GetNSString(
      IDS_IOS_SAFETY_CHECK_NOTIFICATIONS_DESCRIPTION_LONG);

  return footerItem;
}

- (TableViewLinkHeaderFooterItem*)passwordCheckupDescriptionFooterItem {
  TableViewLinkHeaderFooterItem* footerItem =
      [[TableViewLinkHeaderFooterItem alloc]
          initWithType:ItemTypePasswordCheckupDescriptionFooter];
  footerItem.text =
      l10n_util::GetNSString(IDS_IOS_PASSWORD_CHECKUP_HOMEPAGE_FOOTER);
  CrURL* footerURL = [[CrURL alloc]
      initWithGURL:
          google_util::AppendGoogleLocaleParam(
              GURL(password_manager::
                       kPasswordManagerHelpCenterChangeUnsafePasswordsURL),
              GetApplicationContext()->GetApplicationLocale())];
  footerItem.urls = @[ footerURL ];
  return footerItem;
}

#pragma mark - SettingsControllerProtocol

- (void)reportDismissalUserAction {
  base::RecordAction(
      base::UserMetricsAction("MobilePasswordCheckupSettingsClose"));
}

- (void)reportBackUserAction {
  base::RecordAction(
      base::UserMetricsAction("MobilePasswordCheckupSettingsBack"));
}

- (void)settingsWillBeDismissed {
  DCHECK(!_settingsAreDismissed);

  _settingsAreDismissed = YES;
}

#pragma mark - PasswordCheckupConsumer

- (void)setPasswordCheckupHomepageState:(PasswordCheckupHomepageState)state
                 insecurePasswordCounts:
                     (InsecurePasswordCounts)insecurePasswordCounts
     formattedElapsedTimeSinceLastCheck:
         (NSString*)formattedElapsedTimeSinceLastCheck {
  // If the consumer has been updated at least once and the state and insecure
  // password counts haven't changed, there is no need to update anything.
  if (_consumerHasBeenUpdated && _passwordCheckupState == state &&
      _insecurePasswordCounts == insecurePasswordCounts) {
    return;
  }

  // If state is PasswordCheckupHomepageStateDisabled, it means that there is no
  // saved password to check, so we return to the Password Manager.
  if (state == PasswordCheckupHomepageStateDisabled) {
    [self.handler dismissAfterAllPasswordsGone];
  }

  // If the previous state was PasswordCheckupHomepageStateRunning, focus
  // accessibility on the Compromised Passwords cell to let the user know that
  // the Password Checkup results are available.
  if (_passwordCheckupState == PasswordCheckupHomepageStateRunning) {
    [self focusAccessibilityOnCellForItemType:ItemTypeCompromisedPasswords
                            sectionIdentifier:SectionIdentifierInsecureTypes];
  }

  _passwordCheckupState = state;
  _insecurePasswordCounts = insecurePasswordCounts;
  _formattedElapsedTimeSinceLastCheck = formattedElapsedTimeSinceLastCheck;
  [self updateItemsDependingOnPasswordCheckupState];

  _consumerHasBeenUpdated = YES;
}

- (void)setSafetyCheckNotificationsEnabled:(BOOL)enabled {
  CHECK(IsSafetyCheckNotificationsEnabled());

  _safetyCheckNotificationsEnabled = enabled;

  [self updateNotificationsOptInItem];
}

- (void)setAffiliatedGroupCount:(NSInteger)affiliatedGroupCount {
  // If the affiliated group count hasn't changed, there is no need to update
  // the item.
  if (_affiliatedGroupCount == affiliatedGroupCount) {
    return;
  }

  _affiliatedGroupCount = affiliatedGroupCount;
  [self updatePasswordCheckupTimestampDetailText];
}

- (void)showErrorDialogWithMessage:(NSString*)message {
  NSString* title = l10n_util::GetNSString(
      IDS_IOS_PASSWORD_CHECKUP_HOMEPAGE_ERROR_DIALOG_TITLE);
  UIAlertController* alert =
      [UIAlertController alertControllerWithTitle:title
                                          message:message
                                   preferredStyle:UIAlertControllerStyleAlert];

  UIAlertAction* okAction =
      [UIAlertAction actionWithTitle:l10n_util::GetNSString(IDS_OK)
                               style:UIAlertActionStyleDefault
                             handler:nil];
  okAction.accessibilityIdentifier =
      [l10n_util::GetNSString(IDS_OK) stringByAppendingString:@"AlertAction"];
  [alert addAction:okAction];

  [self presentViewController:alert animated:YES completion:nil];
}

#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  [super tableView:tableView didSelectRowAtIndexPath:indexPath];

  TableViewModel* model = self.tableViewModel;
  ItemType itemType =
      static_cast<ItemType>([model itemTypeForIndexPath:indexPath]);
  switch (itemType) {
    case ItemTypeCompromisedPasswords:
      base::RecordAction(
          base::UserMetricsAction("MobilePasswordIssuesCompromisedOpen"));
      [self showPasswordIssuesWithWarningType:WarningType::
                                                  kCompromisedPasswordsWarning];
      break;
    case ItemTypeReusedPasswords:
      base::RecordAction(
          base::UserMetricsAction("MobilePasswordIssuesReusedOpen"));
      [self showPasswordIssuesWithWarningType:WarningType::
                                                  kReusedPasswordsWarning];
      break;
    case ItemTypeWeakPasswords:
      base::RecordAction(
          base::UserMetricsAction("MobilePasswordIssuesWeakOpen"));
      [self
          showPasswordIssuesWithWarningType:WarningType::kWeakPasswordsWarning];
      break;
    case ItemTypePasswordCheckupTimestamp:
    case ItemTypePasswordCheckupDescriptionFooter:
    case ItemTypeNotificationsDescriptionFooter:
      break;
    case ItemTypeCheckPasswordsButton:
      if (_checkPasswordsButtonItem.isEnabled) {
        password_manager::LogStartPasswordCheckManually();
        [self.delegate startPasswordCheck];

        // Focus accessibility on the Password Checkup Timestamp cell to let the
        // user know that their passwords are being checked.
        [self
            focusAccessibilityOnCellForItemType:ItemTypePasswordCheckupTimestamp
                              sectionIdentifier:
                                  SectionIdentifierLastPasswordCheckup];
      }
      break;
    case ItemTypeNotificationsOptIn:
      CHECK(IsSafetyCheckNotificationsEnabled());
      [self.delegate toggleSafetyCheckNotifications];
  }
  [tableView deselectRowAtIndexPath:indexPath animated:YES];
}

- (BOOL)tableView:(UITableView*)tableView
    shouldHighlightRowAtIndexPath:(NSIndexPath*)indexPath {
  NSInteger itemType = [self.tableViewModel itemTypeForIndexPath:indexPath];
  switch (itemType) {
    case ItemTypeCompromisedPasswords:
      return _compromisedPasswordsItem.accessoryType ==
             UITableViewCellAccessoryDisclosureIndicator;
    case ItemTypeReusedPasswords:
      return _reusedPasswordsItem.accessoryType ==
             UITableViewCellAccessoryDisclosureIndicator;
    case ItemTypeWeakPasswords:
      return _weakPasswordsItem.accessoryType ==
             UITableViewCellAccessoryDisclosureIndicator;
    case ItemTypePasswordCheckupTimestamp:
      return NO;
    case ItemTypeCheckPasswordsButton:
      return _checkPasswordsButtonItem.isEnabled;
  }
  return YES;
}

- (UIView*)tableView:(UITableView*)tableView
    viewForFooterInSection:(NSInteger)section {
  UIView* view = [super tableView:tableView viewForFooterInSection:section];
  NSInteger sectionIdentifier =
      [self.tableViewModel sectionIdentifierForSectionIndex:section];
  if (sectionIdentifier == SectionIdentifierLastPasswordCheckup &&
      [self.tableViewModel footerForSectionIndex:section]) {
    // Attach self as delegate to handle clicks in page footer.
    TableViewLinkHeaderFooterView* footerView =
        base::apple::ObjCCastStrict<TableViewLinkHeaderFooterView>(view);
    footerView.delegate = self;
  }

  return view;
}

#pragma mark - TableViewLinkHeaderFooterItemDelegate

- (void)view:(TableViewLinkHeaderFooterView*)view didTapLinkURL:(CrURL*)URL {
  [self.handler dismissAndOpenURL:URL];
}

#pragma mark - Private

// Creates the header image view.
- (UIImageView*)createHeaderImageView {
  UIImageView* headerImageView = [[UIImageView alloc] init];
  headerImageView.contentMode = UIViewContentModeScaleAspectFill;
  headerImageView.frame = CGRectMake(0, 0, 0, kHeaderImageHeight);
  headerImageView.accessibilityIdentifier =
      password_manager::kPasswordCheckupHeaderImageViewId;
  return headerImageView;
}

// Updates the background color of the navigation bar. When iPhones are in
// landscape mode, we want to hide the header image, and so we want to update
// the background color of the navigation bar accordingly. We also want to set
// the background color back to `nil` when returning to the previous view
// controller to cleanup the color change made in this view controller.
- (void)updateNavigationBarBackgroundColorForDismissal:
    (BOOL)viewControllerWillBeDismissed {
  if (viewControllerWillBeDismissed || IsCompactHeight(self)) {
    self.navigationController.navigationBar.backgroundColor = nil;
    return;
  }
  self.navigationController.navigationBar.backgroundColor =
      [UIColor colorNamed:@"password_checkup_header_background_color"];
}

// Updates the table view's header view depending on whether the header image
// view should be shown or not. When we're in iPhone landscape mode, we want to
// hide the image header view.
- (void)updateTableViewHeaderView {
  if (IsCompactHeight(self)) {
    self.tableView.tableHeaderView = nil;
  } else {
    self.tableView.tableHeaderView = _headerImageView;
  }
}

// Updates the header image according to the current
// PasswordCheckupHomepageState.
- (void)updateHeaderImage {
  switch (_passwordCheckupState) {
    case PasswordCheckupHomepageStateDone:
    case PasswordCheckupHomepageStateRunning: {
      UIImage* headerImage =
          GetHeaderImage(_passwordCheckupState, _insecurePasswordCounts);
      [_headerImageView setImage:headerImage];
      break;
    }
    case PasswordCheckupHomepageStateDisabled:
      break;
  }
}

// Updates the `_compromisedPasswordsItem`.
- (void)updateCompromisedPasswordsItem {
  if (!_compromisedPasswordsItem) {
    return;
  }

  BOOL hasCompromisedPasswords = _insecurePasswordCounts.compromised_count > 0;
  BOOL hasDismissedWarnings = _insecurePasswordCounts.dismissed_count > 0;

  _compromisedPasswordsItem.text = l10n_util::GetPluralNSStringF(
      IDS_IOS_PASSWORD_CHECKUP_HOMEPAGE_COMPROMISED_PASSWORDS_TITLE,
      _insecurePasswordCounts.compromised_count);
  _compromisedPasswordsItem.detailText = GetCompromisedPasswordsItemDetailText(
      hasCompromisedPasswords, hasDismissedWarnings,
      _insecurePasswordCounts.dismissed_count);

  SetUpTrailingIconAndAccessoryType(
      _passwordCheckupState, _compromisedPasswordsItem,
      hasDismissedWarnings || hasCompromisedPasswords, hasCompromisedPasswords);

  [self reconfigureCellsForItems:@[ _compromisedPasswordsItem ]];
}

// Updates the `_reusedPasswordsItem`.
- (void)updateReusedPasswordsItem {
  if (!_reusedPasswordsItem) {
    return;
  }

  BOOL hasReusedPasswords = _insecurePasswordCounts.reused_count > 0;

  NSString* text;
  NSString* detailText;
  if (hasReusedPasswords) {
    text = l10n_util::GetNSStringF(
        IDS_IOS_PASSWORD_CHECKUP_HOMEPAGE_REUSED_PASSWORDS_TITLE,
        base::NumberToString16(_insecurePasswordCounts.reused_count));
    detailText = l10n_util::GetNSString(
        IDS_IOS_PASSWORD_CHECKUP_HOMEPAGE_REUSED_PASSWORDS_SUBTITLE);
  } else {
    text = l10n_util::GetNSString(
        IDS_IOS_PASSWORD_CHECKUP_HOMEPAGE_NO_REUSED_PASSWORDS_TITLE);
    detailText = l10n_util::GetNSString(
        IDS_IOS_PASSWORD_CHECKUP_HOMEPAGE_NO_REUSED_PASSWORDS_SUBTITLE);
  }
  _reusedPasswordsItem.text = text;
  _reusedPasswordsItem.detailText = detailText;

  SetUpTrailingIconAndAccessoryType(_passwordCheckupState, _reusedPasswordsItem,
                                    hasReusedPasswords);

  [self reconfigureCellsForItems:@[ _reusedPasswordsItem ]];
}

// Updates the `_weakPasswordsItem`.
- (void)updateWeakPasswordsItem {
  if (!_weakPasswordsItem) {
    return;
  }

  BOOL hasWeakPasswords = _insecurePasswordCounts.weak_count > 0;

  NSString* text;
  NSString* detailText;
  if (hasWeakPasswords) {
    text = l10n_util::GetPluralNSStringF(
        IDS_IOS_PASSWORD_CHECKUP_HOMEPAGE_WEAK_PASSWORDS_TITLE,
        _insecurePasswordCounts.weak_count);
    detailText = l10n_util::GetNSString(
        IDS_IOS_PASSWORD_CHECKUP_HOMEPAGE_WEAK_PASSWORDS_SUBTITLE);
  } else {
    text = l10n_util::GetNSString(
        IDS_IOS_PASSWORD_CHECKUP_HOMEPAGE_NO_WEAK_PASSWORDS_TITLE);
    detailText = l10n_util::GetNSString(
        IDS_IOS_PASSWORD_CHECKUP_HOMEPAGE_NO_WEAK_PASSWORDS_SUBTITLE);
  }
  _weakPasswordsItem.text = text;
  _weakPasswordsItem.detailText = detailText;

  SetUpTrailingIconAndAccessoryType(_passwordCheckupState, _weakPasswordsItem,
                                    hasWeakPasswords);

  [self reconfigureCellsForItems:@[ _weakPasswordsItem ]];
}

// Updates the `_passwordCheckupTimestampItem` text.
- (void)updatePasswordCheckupTimestampText {
  if (!_passwordCheckupTimestampItem) {
    return;
  }

  switch (_passwordCheckupState) {
    case PasswordCheckupHomepageStateRunning: {
      _passwordCheckupTimestampItem.text =
          l10n_util::GetNSString(IDS_IOS_PASSWORD_CHECKUP_ONGOING);
      _passwordCheckupTimestampItem.indicatorHidden = NO;
      break;
    }
    case PasswordCheckupHomepageStateDone:
    case PasswordCheckupHomepageStateDisabled:
      _passwordCheckupTimestampItem.text = _formattedElapsedTimeSinceLastCheck;
      _passwordCheckupTimestampItem.indicatorHidden = YES;
      break;
  }

  [self reconfigureCellsForItems:@[ _passwordCheckupTimestampItem ]];
}

// Updates the `_passwordCheckupTimestampItem` detail text.
- (void)updatePasswordCheckupTimestampDetailText {
  if (!_passwordCheckupTimestampItem) {
    return;
  }

  _passwordCheckupTimestampItem.detailText = l10n_util::GetPluralNSStringF(
      IDS_IOS_PASSWORD_CHECKUP_SITES_AND_APPS_COUNT, _affiliatedGroupCount);

  [self reconfigureCellsForItems:@[ _passwordCheckupTimestampItem ]];
}

// Updates the `_checkPasswordsButtonItem`.
- (void)updateCheckPasswordsButtonItem {
  if (!_checkPasswordsButtonItem) {
    return;
  }

  if (_passwordCheckupState == PasswordCheckupHomepageStateRunning) {
    _checkPasswordsButtonItem.enabled = NO;
    _checkPasswordsButtonItem.textColor =
        [UIColor colorNamed:kTextSecondaryColor];
    _checkPasswordsButtonItem.accessibilityTraits |=
        UIAccessibilityTraitNotEnabled;
  } else {
    _checkPasswordsButtonItem.enabled = YES;
    _checkPasswordsButtonItem.textColor = [UIColor colorNamed:kBlueColor];
    _checkPasswordsButtonItem.accessibilityTraits &=
        ~UIAccessibilityTraitNotEnabled;
  }

  [self reconfigureCellsForItems:@[ _checkPasswordsButtonItem ]];
}

// Updates the `_notificationsOptInItem`.
- (void)updateNotificationsOptInItem {
  CHECK(IsSafetyCheckNotificationsEnabled());

  _notificationsOptInItem.text =
      NotificationsOptInItemText(_safetyCheckNotificationsEnabled);

  [self reconfigureCellsForItems:@[ _notificationsOptInItem ]];
}

// Updates all items whose content is depending on `_passwordCheckupState`.
- (void)updateItemsDependingOnPasswordCheckupState {
  // Make these updates in a `performBatchUpdates` completion block to make sure
  // the cell's height adjust if the new content takes up more lines than the
  // current one.
  [self.tableView
      performBatchUpdates:^{
        [self updateHeaderImage];
        [self updateCompromisedPasswordsItem];
        [self updateReusedPasswordsItem];
        [self updateWeakPasswordsItem];
        [self updatePasswordCheckupTimestampText];
        [self updateCheckPasswordsButtonItem];
      }
               completion:nil];
}

// Opens the Password Issues list for the given `warningType` and resets the
// navigation bar background color to what it was before getting to the
// PasswordCheckupViewController.
- (void)showPasswordIssuesWithWarningType:(WarningType)warningType {
  [self.handler showPasswordIssuesWithWarningType:warningType];
  [self updateNavigationBarBackgroundColorForDismissal:YES];
}

// Notifies accessibility to focus on the cell for the given ItemType and
// SectionIdentifierCompromised when its layout changed.
- (void)focusAccessibilityOnCellForItemType:(ItemType)itemType
                          sectionIdentifier:
                              (SectionIdentifier)sectionIdentifier {
  if (!UIAccessibilityIsVoiceOverRunning() ||
      ![self.tableViewModel hasItemForItemType:itemType
                             sectionIdentifier:sectionIdentifier]) {
    return;
  }

  NSIndexPath* indexPath =
      [self.tableViewModel indexPathForItemType:itemType
                              sectionIdentifier:sectionIdentifier];
  UITableViewCell* cell = [self.tableView cellForRowAtIndexPath:indexPath];
  UIAccessibilityPostNotification(UIAccessibilityLayoutChangedNotification,
                                  cell);
}

// Updates the navigation bar's background color & the header views when the
// UITraitVerticalSizeClass changes.
- (void)updateUIOnTraitChange {
  [self updateNavigationBarBackgroundColorForDismissal:NO];
  [self updateTableViewHeaderView];
}
@end
