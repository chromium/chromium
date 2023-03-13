// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_checkup/password_checkup_view_controller.h"

#import "base/metrics/user_metrics.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/settings/cells/settings_check_cell.h"
#import "ios/chrome/browser/ui/settings/cells/settings_check_item.h"
#import "ios/chrome/browser/ui/settings/password/password_checkup/password_checkup_commands.h"
#import "ios/chrome/browser/ui/settings/password/password_checkup/password_checkup_consumer.h"
#import "ios/chrome/browser/ui/settings/password/password_checkup/password_checkup_utils.h"
#import "ios/chrome/browser/ui/settings/password/password_checkup/password_checkup_view_controller_delegate.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_text_item.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using password_manager::InsecurePasswordCounts;

namespace {

// Height of the image used as a header for the table view.
CGFloat const kHeaderImageHeight = 99;

// Sections of the Password Checkup Homepage UI.
typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierLastPasswordCheckup = kSectionIdentifierEnumZero,
};

// Items within the Password Checkup Homepage UI.
typedef NS_ENUM(NSInteger, ItemType) {
  // Section: SectionIdentifierLastPasswordCheckup
  ItemTypePasswordCheckupTimestamp = kItemTypeEnumZero,
  ItemTypeCheckPasswordsButton,
};

// Helper method to get the right trailing image for the Password Check cell
// depending on the check state.
UIImage* GetHeaderImage(PasswordCheckupHomepageState password_checkup_state,
                        InsecurePasswordCounts counts) {
  bool has_compromised_passwords = counts.compromised_count > 0;
  bool has_insecure_passwords =
      counts.compromised_count > 0 || counts.dismissed_count > 0 ||
      counts.reused_count > 0 || counts.weak_count > 0;
  switch (password_checkup_state) {
    case PasswordCheckupHomepageStateDone:
      if (has_compromised_passwords) {
        return [UIImage imageNamed:@"password_checkup_header_red"];
      } else if (has_insecure_passwords) {
        return [UIImage imageNamed:@"password_checkup_header_yellow"];
      }
      return [UIImage imageNamed:@"password_checkup_header_green"];
    case PasswordCheckupHomepageStateRunning:
      return [UIImage imageNamed:@"password_checkup_header_loading"];
    case PasswordCheckupHomepageStateError:
    case PasswordCheckupHomepageStateDisabled:
      return nil;
  }
}

}  // namespace

@interface PasswordCheckupViewController () {
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

- (SettingsCheckItem*)passwordCheckupTimestampItem {
  SettingsCheckItem* passwordCheckupTimestampItem =
      [[SettingsCheckItem alloc] initWithType:ItemTypePasswordCheckupTimestamp];
  passwordCheckupTimestampItem.enabled = YES;
  passwordCheckupTimestampItem.detailText =
      l10n_util::GetNSString(IDS_IOS_PASSWORD_CHECKUP_DESCRIPTION);
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
  // If the state and the insecure password counts both haven't changed, there
  // is no need to update anything.
  if (_passwordCheckupState == state &&
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
  [self updatePasswordCheckupTimestampTextWith:
            formattedElapsedTimeSinceLastCheck];
  [self updateCheckPasswordsButtonItem];
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
    case ItemTypePasswordCheckupTimestamp:
      break;
    case ItemTypeCheckPasswordsButton:
      if (_checkPasswordsButtonItem.enabled) {
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
    case ItemTypePasswordCheckupTimestamp:
      return NO;
    case ItemTypeCheckPasswordsButton:
      return _checkPasswordsButtonItem.enabled;
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
