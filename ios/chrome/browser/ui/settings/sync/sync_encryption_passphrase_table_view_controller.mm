// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/sync/sync_encryption_passphrase_table_view_controller.h"

#include <memory>

#include "base/i18n/time_formatting.h"
#include "base/mac/foundation_util.h"
#include "base/strings/sys_string_conversions.h"
#include "components/google/core/common/google_util.h"
#import "components/signin/public/identity_manager/objc/identity_manager_observer_bridge.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/driver/sync_user_settings.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#include "ios/chrome/browser/signin/authentication_service_factory.h"
#include "ios/chrome/browser/signin/identity_manager_factory.h"
#include "ios/chrome/browser/sync/profile_sync_service_factory.h"
#include "ios/chrome/browser/sync/sync_setup_service.h"
#include "ios/chrome/browser/sync/sync_setup_service_factory.h"
#import "ios/chrome/browser/ui/settings/cells/byo_textfield_item.h"
#import "ios/chrome/browser/ui/settings/cells/passphrase_error_item.h"
#import "ios/chrome/browser/ui/settings/settings_navigation_controller.h"
#import "ios/chrome/browser/ui/settings/sync/utils/sync_util.h"
#import "ios/chrome/browser/ui/settings/utils/settings_utils.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_link_header_footer_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_text_item.h"
#include "ios/chrome/browser/ui/ui_feature_flags.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/signin/chrome_identity.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using sync_encryption_passphrase::ItemTypeEnterPassphrase;
using sync_encryption_passphrase::ItemTypeError;
using sync_encryption_passphrase::ItemTypeFooter;
using sync_encryption_passphrase::ItemTypeMessage;
using sync_encryption_passphrase::SectionIdentifierPassphrase;

namespace {

const CGFloat kSpinnerButtonCustomViewSize = 48;
const CGFloat kSpinnerButtonPadding = 18;

}  // namespace

@interface SyncEncryptionPassphraseTableViewController () <
    IdentityManagerObserverBridgeDelegate,
    SettingsControllerProtocol> {
  ios::ChromeBrowserState* browserState_;
  // Whether the decryption progress is currently being shown.
  BOOL isDecryptionProgressShown_;
  NSString* savedTitle_;
  UIBarButtonItem* savedLeftButton_;
  std::unique_ptr<SyncObserverBridge> syncObserver_;
  std::unique_ptr<signin::IdentityManagerObserverBridge>
      identityManagerObserver_;
  UITextField* passphrase_;
}

@end

@implementation SyncEncryptionPassphraseTableViewController

- (instancetype)initWithBrowserState:(ios::ChromeBrowserState*)browserState {
  DCHECK(browserState);
  UITableViewStyle style = base::FeatureList::IsEnabled(kSettingsRefresh)
                               ? UITableViewStylePlain
                               : UITableViewStyleGrouped;
  self = [super initWithTableViewStyle:style
                           appBarStyle:ChromeTableViewControllerStyleNoAppBar];
  if (self) {
    self.title = l10n_util::GetNSString(IDS_IOS_SYNC_ENTER_PASSPHRASE_TITLE);
    self.shouldHideDoneButton = YES;
    browserState_ = browserState;
    NSString* userEmail =
        [AuthenticationServiceFactory::GetForBrowserState(browserState_)
                ->GetAuthenticatedIdentity() userEmail];
    DCHECK(userEmail);
    syncer::SyncService* service =
        ProfileSyncServiceFactory::GetForBrowserState(browserState_);
    if (service->IsEngineInitialized() &&
        service->GetUserSettings()->IsUsingSecondaryPassphrase()) {
      base::Time passphrase_time =
          service->GetUserSettings()->GetExplicitPassphraseTime();
      if (!passphrase_time.is_null()) {
        base::string16 passphrase_time_str =
            base::TimeFormatShortDate(passphrase_time);
        _headerMessage = l10n_util::GetNSStringF(
            IDS_IOS_SYNC_ENTER_PASSPHRASE_BODY_WITH_EMAIL_AND_DATE,
            base::SysNSStringToUTF16(userEmail), passphrase_time_str);
      } else {
        _headerMessage = l10n_util::GetNSStringF(
            IDS_IOS_SYNC_ENTER_PASSPHRASE_BODY_WITH_EMAIL,
            base::SysNSStringToUTF16(userEmail));
      }
    } else {
      _headerMessage =
          l10n_util::GetNSString(IDS_SYNC_ENTER_GOOGLE_PASSPHRASE_BODY);
    }
    _processingMessage = l10n_util::GetNSString(IDS_SYNC_LOGIN_SETTING_UP);
    _footerMessage = l10n_util::GetNSString(IDS_IOS_SYNC_PASSPHRASE_RECOVER);

    identityManagerObserver_ =
        std::make_unique<signin::IdentityManagerObserverBridge>(
            IdentityManagerFactory::GetForBrowserState(browserState_), self);
  }
  return self;
}

- (UITextField*)passphrase {
  return passphrase_;
}

- (NSString*)syncErrorMessage {
  if (_syncErrorMessage)
    return _syncErrorMessage;
  SyncSetupService* service =
      SyncSetupServiceFactory::GetForBrowserState(browserState_);
  DCHECK(service);
  SyncSetupService::SyncServiceState syncServiceState =
      service->GetSyncServiceState();

  // Passphrase error directly set |_syncErrorMessage|.
  if (syncServiceState == SyncSetupService::kSyncServiceNeedsPassphrase)
    return nil;

  return GetSyncErrorMessageForBrowserState(browserState_);
}

#pragma mark - View lifecycle

- (void)viewDidLoad {
  [super viewDidLoad];
  [self loadModel];
  [self setRightNavBarItem];
}

- (void)didReceiveMemoryWarning {
  [super didReceiveMemoryWarning];
  if (![self isViewLoaded]) {
    passphrase_ = nil;
  }
}

- (void)viewWillDisappear:(BOOL)animated {
  [super viewWillDisappear:animated];
  [self.passphrase resignFirstResponder];
}

- (void)viewDidDisappear:(BOOL)animated {
  [super viewDidDisappear:animated];
  if ([self isMovingFromParentViewController]) {
    [self unregisterTextField:self.passphrase];
  }
}

#pragma mark - SettingsRootTableViewController

- (void)loadModel {
  [super loadModel];
  TableViewModel* model = self.tableViewModel;

  [model addSectionWithIdentifier:SectionIdentifierPassphrase];
  if (self.headerMessage) {
    [model addItem:[self passphraseMessageItem]
        toSectionWithIdentifier:SectionIdentifierPassphrase];
  }
  [model addItem:[self passphraseItem]
      toSectionWithIdentifier:SectionIdentifierPassphrase];

  NSString* errorMessage = [self syncErrorMessage];
  if (errorMessage) {
    [model addItem:[self passphraseErrorItemWithMessage:errorMessage]
        toSectionWithIdentifier:SectionIdentifierPassphrase];
  }
  [model setFooter:[self footerItem]
      forSectionWithIdentifier:SectionIdentifierPassphrase];
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (BOOL)presentationControllerShouldDismiss:
    (UIPresentationController*)presentationController {
  return ![passphrase_.text length];
}

#pragma mark - Items

// Returns a passphrase message item.
- (TableViewItem*)passphraseMessageItem {
  TableViewTextItem* item =
      [[TableViewTextItem alloc] initWithType:ItemTypeMessage];
  item.text = self.headerMessage;
  item.enabled = NO;
  return item;
}

// Returns a passphrase item.
- (TableViewItem*)passphraseItem {
  if (passphrase_) {
    [self unregisterTextField:passphrase_];
  }
  passphrase_ = [[UITextField alloc] init];
  passphrase_.secureTextEntry = YES;
  passphrase_.backgroundColor = UIColor.clearColor;
  passphrase_.autocorrectionType = UITextAutocorrectionTypeNo;
  passphrase_.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
  passphrase_.adjustsFontForContentSizeCategory = YES;
  passphrase_.placeholder = l10n_util::GetNSString(IDS_SYNC_PASSPHRASE_LABEL);
  [self registerTextField:passphrase_];

  BYOTextFieldItem* item =
      [[BYOTextFieldItem alloc] initWithType:ItemTypeEnterPassphrase];
  item.textField = passphrase_;
  return item;
}

// Returns a passphrase error item having |errorMessage| as title.
- (TableViewItem*)passphraseErrorItemWithMessage:(NSString*)errorMessage {
  PassphraseErrorItem* item =
      [[PassphraseErrorItem alloc] initWithType:ItemTypeError];
  item.text = errorMessage;
  return item;
}

// Returns the footer item for passphrase section.
- (TableViewHeaderFooterItem*)footerItem {
  TableViewLinkHeaderFooterItem* footerItem =
      [[TableViewLinkHeaderFooterItem alloc] initWithType:ItemTypeFooter];
  footerItem.text = self.footerMessage;
  footerItem.linkURL = google_util::AppendGoogleLocaleParam(
      GURL(kSyncGoogleDashboardURL),
      GetApplicationContext()->GetApplicationLocale());
  return footerItem;
}

#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  [super tableView:tableView didSelectRowAtIndexPath:indexPath];
  NSInteger itemType = [self.tableViewModel itemTypeForIndexPath:indexPath];
  if (itemType == ItemTypeEnterPassphrase) {
    [passphrase_ becomeFirstResponder];
  }
  [tableView deselectRowAtIndexPath:indexPath animated:YES];
}

- (UIView*)tableView:(UITableView*)tableView
    viewForFooterInSection:(NSInteger)section {
  UIView* view = [super tableView:tableView viewForFooterInSection:section];
  if (SectionIdentifierPassphrase ==
      [self.tableViewModel sectionIdentifierForSection:section]) {
    TableViewLinkHeaderFooterView* linkView =
        base::mac::ObjCCastStrict<TableViewLinkHeaderFooterView>(view);
    linkView.delegate = self;
  }
  return view;
}

#pragma mark - Behavior

- (BOOL)forDecryption {
  return YES;
}

- (void)signInPressed {
  DCHECK([passphrase_ text].length);

  if (!syncObserver_.get()) {
    syncObserver_.reset(new SyncObserverBridge(
        self, ProfileSyncServiceFactory::GetForBrowserState(browserState_)));
  }

  // Clear out the error message.
  self.syncErrorMessage = nil;

  syncer::SyncService* service =
      ProfileSyncServiceFactory::GetForBrowserState(browserState_);
  DCHECK(service);
  // It is possible for a race condition to happen where a user is allowed
  // to call the backend with the passphrase before the backend is
  // initialized.
  // See crbug/276714. As a temporary measure, ignore the tap on sign-in
  // button. A better fix may be to disable the rightBarButtonItem (submit)
  // until backend is initialized.
  if (!service->IsEngineInitialized())
    return;

  [self showDecryptionProgress];
  std::string passphrase = base::SysNSStringToUTF8([passphrase_ text]);
  if ([self forDecryption]) {
    if (!service->GetUserSettings()->SetDecryptionPassphrase(passphrase)) {
      syncObserver_.reset();
      [self clearFieldsOnError:l10n_util::GetNSString(
                                   IDS_IOS_SYNC_INCORRECT_PASSPHRASE)];
      [self hideDecryptionProgress];
    }
  } else {
    service->GetUserSettings()->EnableEncryptEverything();
    service->GetUserSettings()->SetEncryptionPassphrase(passphrase);
  }
  [self reloadData];
}

// Sets up the navigation bar's right button. The button will be enabled iff
// |-areAllFieldsFilled| returns YES.
- (void)setRightNavBarItem {
  UIBarButtonItem* submitButtonItem = self.navigationItem.rightBarButtonItem;
  if (!submitButtonItem) {
    submitButtonItem = [[UIBarButtonItem alloc]
        initWithTitle:l10n_util::GetNSString(IDS_IOS_SYNC_DECRYPT_BUTTON)
                style:UIBarButtonItemStylePlain
               target:self
               action:@selector(signInPressed)];
  }
  submitButtonItem.enabled = [self areAllFieldsFilled];

  // Only setting the enabled state doesn't make the item redraw. As a
  // workaround, set it again.
  self.navigationItem.rightBarButtonItem = submitButtonItem;
}

- (BOOL)areAllFieldsFilled {
  return [self.passphrase text].length > 0;
}

- (void)clearFieldsOnError:(NSString*)errorMessage {
  self.syncErrorMessage = errorMessage;
  [self.passphrase setText:@""];
}

// Shows the UI to indicate the decryption is being attempted.
- (void)showDecryptionProgress {
  if (isDecryptionProgressShown_)
    return;
  isDecryptionProgressShown_ = YES;

  // Hide the button.
  self.navigationItem.rightBarButtonItem = nil;

  // Custom title view with spinner.
  DCHECK(!savedTitle_);
  DCHECK(!savedLeftButton_);
  savedLeftButton_ = self.navigationItem.leftBarButtonItem;
  self.navigationItem.leftBarButtonItem = [self spinnerButton];
  savedTitle_ = [self.title copy];
  self.title = self.processingMessage;
}

// Hides the UI to indicate decryption is in process.
- (void)hideDecryptionProgress {
  if (!isDecryptionProgressShown_)
    return;
  isDecryptionProgressShown_ = NO;

  self.navigationItem.leftBarButtonItem = savedLeftButton_;
  savedLeftButton_ = nil;
  self.title = savedTitle_;
  savedTitle_ = nil;
  [self setRightNavBarItem];
}

- (void)registerTextField:(UITextField*)textField {
  [textField addTarget:self
                action:@selector(textFieldDidBeginEditing:)
      forControlEvents:UIControlEventEditingDidBegin];
  [textField addTarget:self
                action:@selector(textFieldDidChange:)
      forControlEvents:UIControlEventEditingChanged];
  [textField addTarget:self
                action:@selector(textFieldDidEndEditing:)
      forControlEvents:UIControlEventEditingDidEndOnExit];
}

- (void)unregisterTextField:(UITextField*)textField {
  [textField removeTarget:self
                   action:@selector(textFieldDidBeginEditing:)
         forControlEvents:UIControlEventEditingDidBegin];
  [textField removeTarget:self
                   action:@selector(textFieldDidChange:)
         forControlEvents:UIControlEventEditingChanged];
  [textField removeTarget:self
                   action:@selector(textFieldDidEndEditing:)
         forControlEvents:UIControlEventEditingDidEndOnExit];
}

// Creates a new UIBarButtonItem with a spinner.
- (UIBarButtonItem*)spinnerButton {
  CGRect customViewFrame = CGRectMake(0, 0, kSpinnerButtonCustomViewSize,
                                      kSpinnerButtonCustomViewSize);
  UIView* customView = [[UIView alloc] initWithFrame:customViewFrame];

  UIActivityIndicatorView* spinner = [[UIActivityIndicatorView alloc]
      initWithActivityIndicatorStyle:UIActivityIndicatorViewStyleGray];

  CGRect spinnerFrame = [spinner bounds];
  spinnerFrame.origin.x = kSpinnerButtonPadding;
  spinnerFrame.origin.y = kSpinnerButtonPadding;
  [spinner setFrame:spinnerFrame];
  [customView addSubview:spinner];

  UIBarButtonItem* leftBarButtonItem =
      [[UIBarButtonItem alloc] initWithCustomView:customView];

  [spinner setHidesWhenStopped:NO];
  [spinner startAnimating];

  return leftBarButtonItem;
}

- (void)stopObserving {
  // Stops observing the sync service. This is required during the shutdown
  // phase to avoid observing sync events for a browser state that is being
  // killed.
  syncObserver_.reset();
  identityManagerObserver_.reset();
}

#pragma mark - UIControl events listener

- (void)textFieldDidBeginEditing:(id)sender {
  // Remove the error cell if there is one.
  TableViewModel* model = self.tableViewModel;
  if ([model hasItemForItemType:ItemTypeError
              sectionIdentifier:SectionIdentifierPassphrase]) {
    DCHECK(self.syncErrorMessage);
    NSIndexPath* path =
        [model indexPathForItemType:ItemTypeError
                  sectionIdentifier:SectionIdentifierPassphrase];
    [model removeItemWithType:ItemTypeError
        fromSectionWithIdentifier:SectionIdentifierPassphrase];
    [self.tableView deleteRowsAtIndexPaths:@[ path ]
                          withRowAnimation:UITableViewRowAnimationAutomatic];
    self.syncErrorMessage = nil;
  }
}

- (void)textFieldDidChange:(id)sender {
  [self setRightNavBarItem];
}

- (void)textFieldDidEndEditing:(id)sender {
  if (sender == self.passphrase) {
    if ([self areAllFieldsFilled]) {
      [self signInPressed];
    } else {
      [self clearFieldsOnError:l10n_util::GetNSString(
                                   IDS_SYNC_EMPTY_PASSPHRASE_ERROR)];
      [self reloadData];
    }
  }
}

#pragma mark - SyncObserverModelBridge

- (void)onSyncStateChanged {
  syncer::SyncService* service =
      ProfileSyncServiceFactory::GetForBrowserState(browserState_);

  if (!service->IsEngineInitialized()) {
    return;
  }

  // Checking if the operation succeeded.
  if (!service->GetUserSettings()->IsPassphraseRequired() &&
      (service->GetUserSettings()->IsUsingSecondaryPassphrase() ||
       [self forDecryption])) {
    syncObserver_.reset();
    [base::mac::ObjCCastStrict<SettingsNavigationController>(
        self.navigationController)
        popViewControllerOrCloseSettingsAnimated:YES];
    return;
  }

  // Handling passphrase error case.
  if (service->GetUserSettings()->IsPassphraseRequired()) {
    self.syncErrorMessage =
        l10n_util::GetNSString(IDS_IOS_SYNC_INCORRECT_PASSPHRASE);
  }
  [self hideDecryptionProgress];
  [self reloadData];
}

#pragma mark - IdentityManagerObserverBridgeDelegate

- (void)onEndBatchOfRefreshTokenStateChanges {
  if (AuthenticationServiceFactory::GetForBrowserState(browserState_)
          ->IsAuthenticated()) {
    return;
  }
  [base::mac::ObjCCastStrict<SettingsNavigationController>(
      self.navigationController) popViewControllerOrCloseSettingsAnimated:NO];
}

#pragma mark - SettingsControllerProtocol callbacks

- (void)settingsWillBeDismissed {
  [self stopObserving];
}

@end
