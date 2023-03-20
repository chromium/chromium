// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_checkup/password_checkup_view_controller.h"

#import "base/metrics/user_metrics.h"
#import "base/strings/string_number_conversions.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_item.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/icons/symbols.h"
#import "ios/chrome/browser/ui/settings/cells/settings_check_cell.h"
#import "ios/chrome/browser/ui/settings/cells/settings_check_item.h"
#import "ios/chrome/browser/ui/settings/password/password_checkup/password_checkup_commands.h"
#import "ios/chrome/browser/ui/settings/password/password_checkup/password_checkup_constants.h"
#import "ios/chrome/browser/ui/settings/password/password_checkup/password_checkup_consumer.h"
#import "ios/chrome/browser/ui/settings/password/password_checkup/password_checkup_utils.h"
#import "ios/chrome/browser/ui/settings/password/password_checkup/password_checkup_view_controller_delegate.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using password_manager::InsecurePasswordCounts;

namespace {

// Height of the image used as a header for the table view.
constexpr CGFloat kHeaderImageHeight = 99;

// The size of the trailing icons for the items in the insecure types section.
constexpr NSInteger kTrailingIconSize = 22;

// Sections of the Password Checkup Homepage UI.
typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierInsecureTypes = kSectionIdentifierEnumZero,
  SectionIdentifierLastPasswordCheckup,
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
};

// Possible states for the items in the insecure types section.
enum class ItemState {
  kSafe,
  kWarning,
  kSevereWarning,
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
    case PasswordCheckupHomepageStateError:
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

// Sets up the trailing icon and its tint color for the given item. This is used
// to set up the trailing icon of the items in the insecure types section.
void SetUpTrailingIcon(SettingsCheckItem* item, ItemState item_state) {
  if (item_state == ItemState::kSafe) {
    item.trailingImage = DefaultSymbolTemplateWithPointSize(
        kCheckmarkCircleFillSymbol, kTrailingIconSize);
    item.trailingImageTintColor = [UIColor colorNamed:kGreen500Color];
    return;
  }

  item.trailingImage = DefaultSymbolTemplateWithPointSize(
      kErrorCircleFillSymbol, kTrailingIconSize);
  if (item_state == ItemState::kWarning) {
    item.trailingImageTintColor = [UIColor colorNamed:kYellow500Color];
  } else {
    item.trailingImageTintColor = [UIColor colorNamed:kRed500Color];
  }
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
        SetUpTrailingIcon(item, has_compromised_passwords
                                    ? ItemState::kSevereWarning
                                    : ItemState::kWarning);
        item.accessoryType = UITableViewCellAccessoryDisclosureIndicator;
      } else {
        SetUpTrailingIcon(item, ItemState::kSafe);
      }
      break;
    case PasswordCheckupHomepageStateRunning: {
      item.trailingImage = nil;
      item.indicatorHidden = NO;
      break;
    }
    case PasswordCheckupHomepageStateError:
    case PasswordCheckupHomepageStateDisabled:
      break;
  }
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

  // Whether Settings have been dismissed.
  BOOL _settingsAreDismissed;

  // Current PasswordCheckupHomepageState.
  PasswordCheckupHomepageState _passwordCheckupState;

  // Password counts associated with the different insecure types.
  InsecurePasswordCounts _insecurePasswordCounts;

  // Image view at the top of the screen, indicating the overall Password
  // Checkup status.
  UIImageView* _headerImageView;
}

@end

@implementation PasswordCheckupViewController

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  self.title = l10n_util::GetNSString(IDS_IOS_PASSWORD_CHECKUP);

  _headerImageView = [self createHeaderImageView];
  self.tableView.tableHeaderView = _headerImageView;
  [self updateHeaderImage];

  [self loadModel];
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

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  if (self.traitCollection.verticalSizeClass !=
      previousTraitCollection.verticalSizeClass) {
    [self updateNavigationBarBackgroundColorForDismissal:NO];
    [self updateTableViewHeaderView];
  }
}

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
}

#pragma mark - Items

- (SettingsCheckItem*)compromisedPasswordsItem {
  SettingsCheckItem* compromisedPasswordsItem =
      [[SettingsCheckItem alloc] initWithType:ItemTypeCompromisedPasswords];
  compromisedPasswordsItem.enabled = YES;
  compromisedPasswordsItem.indicatorHidden = YES;
  compromisedPasswordsItem.infoButtonHidden = YES;
  return compromisedPasswordsItem;
}

- (SettingsCheckItem*)reusedPasswordsItem {
  SettingsCheckItem* reusedPasswordsItem =
      [[SettingsCheckItem alloc] initWithType:ItemTypeReusedPasswords];
  reusedPasswordsItem.enabled = YES;
  reusedPasswordsItem.indicatorHidden = YES;
  reusedPasswordsItem.infoButtonHidden = YES;
  return reusedPasswordsItem;
}

- (SettingsCheckItem*)weakPasswordsItem {
  SettingsCheckItem* weakPasswordsItem =
      [[SettingsCheckItem alloc] initWithType:ItemTypeWeakPasswords];
  weakPasswordsItem.enabled = YES;
  weakPasswordsItem.indicatorHidden = YES;
  weakPasswordsItem.infoButtonHidden = YES;
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
    [self.handler dismissPasswordCheckupViewController];
  }

  _passwordCheckupState = state;
  _insecurePasswordCounts = insecurePasswordCounts;
  [self updateHeaderImage];
  [self updateCompromisedPasswordsItem];
  [self updateReusedPasswordsItem];
  [self updateWeakPasswordsItem];
  [self updatePasswordCheckupTimestampTextWith:
            formattedElapsedTimeSinceLastCheck];
  [self updateCheckPasswordsButtonItem];

  _consumerHasBeenUpdated = YES;
}

- (void)setAffiliatedGroupCount:(NSInteger)affiliatedGroupCount {
  [self updatePasswordCheckupTimestampDetailTextWithAffiliatedGroupCount:
            affiliatedGroupCount];
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
    case ItemTypeReusedPasswords:
    case ItemTypeWeakPasswords:
      break;
    case ItemTypePasswordCheckupTimestamp:
      break;
    case ItemTypeCheckPasswordsButton:
      if (_checkPasswordsButtonItem.isEnabled) {
        [self.delegate startPasswordCheck];
      }
      break;
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

#pragma mark - Private

// Creates the header image view.
- (UIImageView*)createHeaderImageView {
  UIImageView* headerImageView = [[UIImageView alloc] init];
  headerImageView.contentMode = UIViewContentModeScaleAspectFill;
  headerImageView.frame = CGRectMake(0, 0, 0, kHeaderImageHeight);
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
    case PasswordCheckupHomepageStateError:
    case PasswordCheckupHomepageStateDisabled:
      break;
  }
  [self.tableView layoutIfNeeded];
}

// Updates the `_compromisedPasswordsItem`.
- (void)updateCompromisedPasswordsItem {
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
- (void)updatePasswordCheckupTimestampTextWith:
    (NSString*)formattedElapsedTimeSinceLastCheck {
  switch (_passwordCheckupState) {
    case PasswordCheckupHomepageStateRunning: {
      _passwordCheckupTimestampItem.text =
          l10n_util::GetNSString(IDS_IOS_PASSWORD_CHECKUP_ONGOING);
      _passwordCheckupTimestampItem.indicatorHidden = NO;
      break;
    }
    case PasswordCheckupHomepageStateDone:
    case PasswordCheckupHomepageStateError:
    case PasswordCheckupHomepageStateDisabled:
      _passwordCheckupTimestampItem.text = formattedElapsedTimeSinceLastCheck;
      _passwordCheckupTimestampItem.indicatorHidden = YES;
      break;
  }

  [self reconfigureCellsForItems:@[ _passwordCheckupTimestampItem ]];
}

// Updates the `_passwordCheckupTimestampItem` detail text.
- (void)updatePasswordCheckupTimestampDetailTextWithAffiliatedGroupCount:
    (NSInteger)affiliatedGroupCount {
  _passwordCheckupTimestampItem.detailText = l10n_util::GetPluralNSStringF(
      IDS_IOS_PASSWORD_CHECKUP_SITES_AND_APPS_COUNT, affiliatedGroupCount);

  [self reconfigureCellsForItems:@[ _passwordCheckupTimestampItem ]];
}

// Updates the `_checkPasswordsButtonItem`.
- (void)updateCheckPasswordsButtonItem {
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

@end
