// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/sync/sync_encryption_passphrase_table_view_controller.h"

#import <memory>

#import "base/apple/foundation_util.h"
#import "base/i18n/time_formatting.h"
#import "base/ios/ios_util.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/strings/sys_string_conversions.h"
#import "components/google/core/common/google_util.h"
#import "components/signin/public/identity_manager/objc/identity_manager_observer_bridge.h"
#import "components/strings/grit/components_strings.h"
#import "components/sync/service/sync_service.h"
#import "components/sync/service/sync_user_settings.h"
#import "ios/chrome/browser/net/model/crurl.h"
#import "ios/chrome/browser/settings/model/sync/utils/sync_util.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_link_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_item.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/signin/model/system_identity.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/ui/scoped_ui_blocker/scoped_ui_blocker.h"
#import "ios/chrome/browser/ui/settings/cells/byo_textfield_item.h"
#import "ios/chrome/browser/ui/settings/cells/passphrase_error_item.h"
#import "ios/chrome/browser/ui/settings/google_services/google_services_settings_constants.h"
#import "ios/chrome/browser/ui/settings/settings_navigation_controller.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "url/gurl.h"

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
    IdentityManagerObserverBridgeDelegate> {
  // Whether the decryption progress is currently being shown.
  BOOL _isDecryptionProgressShown;
  NSString* _savedTitle;
  UIBarButtonItem* _savedLeftButton;
  std::unique_ptr<SyncObserverBridge> _syncObserver;
  std::unique_ptr<signin::IdentityManagerObserverBridge>
      _identityManagerObserver;
  UITextField* _passphrase;
  std::unique_ptr<ScopedUIBlocker> _uiBlocker;

  // Whether Settings have been dismissed.
  BOOL _settingsAreDismissed;
}

@property(nonatomic, assign, readonly) Browser* browser;

@end

@implementation SyncEncryptionPassphraseTableViewController

@synthesize browser = _browser;

- (instancetype)initWithBrowser:(Browser*)browser {
  DCHECK(browser);

  self = [super initWithStyle:ChromeTableViewStyle()];
  if (!self) {
    return nullptr;
  }

  _browser = browser;
  _processingMessage = l10n_util::GetNSString(IDS_SYNC_LOGIN_SETTING_UP);
  _footerMessage = l10n_util::GetNSString(IDS_IOS_SYNC_PASSPHRASE_RECOVER);
  self.title = l10n_util::GetNSString(IDS_IOS_SYNC_ENTER_PASSPHRASE_TITLE);
  self.shouldHideDoneButton = YES;

  ProfileIOS* profile = _browser->GetProfile();
  syncer::SyncService* service = SyncServiceFactory::GetForProfile(profile);
  // TODO(crbug.com/40765960): The reason this is an if and not a DCHECK is
  // because SyncCreatePassphraseTableViewController inherits from this class.
  // This should be changed, i.e. either extract the minimum common logic
  // between the 2 to a new base class, or not share code at all.
  if (service->IsEngineInitialized() &&
      service->GetUserSettings()->IsUsingExplicitPassphrase()) {
    base::Time passphraseTime =
        service->GetUserSettings()->GetExplicitPassphraseTime();
    NSString* userEmail =
        AuthenticationServiceFactory::GetForProfile(profile)
            ->GetPrimaryIdentity(signin::ConsentLevel::kSignin)
            .userEmail;
    DCHECK(userEmail);
    _headerMessage =
        passphraseTime.is_null()
            ? l10n_util::GetNSStringF(
                  IDS_IOS_SYNC_ENTER_PASSPHRASE_BODY_WITH_EMAIL,
                  base::SysNSStringToUTF16(userEmail))
            : l10n_util::GetNSStringF(
                  IDS_IOS_SYNC_ENTER_PASSPHRASE_BODY_WITH_EMAIL_AND_DATE,
                  base::SysNSStringToUTF16(userEmail),
                  base::TimeFormatShortDate(passphraseTime));
  }

  _identityManagerObserver =
      std::make_unique<signin::IdentityManagerObserverBridge>(
          IdentityManagerFactory::GetForProfile(profile), self);
  return self;
}

- (UITextField*)passphrase {
  return _passphrase;
}

- (NSString*)syncErrorMessage {
  if (_settingsAreDismissed)
    return nil;
  if (_syncErrorMessage)
    return _syncErrorMessage;
  ProfileIOS* profile = self.browser->GetProfile();
  syncer::SyncService* service = SyncServiceFactory::GetForProfile(profile);
  DCHECK(service);

  // Passphrase error directly set `_syncErrorMessage`.
  if (service->GetUserActionableError() ==
      syncer::SyncService::UserActionableError::kNeedsPassphrase) {
    return nil;
  }

  return GetSyncErrorMessageForProfile(profile);
}

#pragma mark - View lifecycle

- (void)viewDidLoad {
  [super viewDidLoad];
  [self loadModel];
  [self setRightNavBarItem];
  [self setLeftNavBarItem];

  SceneState* sceneState = self.browser->GetSceneState();
  _uiBlocker = std::make_unique<ScopedUIBlocker>(sceneState);
  self.view.accessibilityIdentifier =
      kSyncEncryptionPassphraseTableViewAccessibilityIdentifier;
}

- (void)didReceiveMemoryWarning {
  [super didReceiveMemoryWarning];
  if (![self isViewLoaded]) {
    _passphrase = nil;
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
  _uiBlocker.reset();
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
  return ![_passphrase.text length];
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
  if (_passphrase) {
    [self unregisterTextField:_passphrase];
  }
  _passphrase = [[UITextField alloc] init];
  _passphrase.secureTextEntry = YES;
  _passphrase.backgroundColor = UIColor.clearColor;
  _passphrase.autocorrectionType = UITextAutocorrectionTypeNo;
  _passphrase.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
  _passphrase.adjustsFontForContentSizeCategory = YES;
  _passphrase.placeholder = l10n_util::GetNSString(IDS_SYNC_PASSPHRASE_LABEL);
  _passphrase.accessibilityIdentifier =
      kSyncEncryptionPassphraseTextFieldAccessibilityIdentifier;
  [self registerTextField:_passphrase];

  BYOTextFieldItem* item =
      [[BYOTextFieldItem alloc] initWithType:ItemTypeEnterPassphrase];
  item.textField = _passphrase;
  return item;
}

// Returns a passphrase error item having `errorMessage` as title.
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
  footerItem.urls = @[ [[CrURL alloc]
      initWithGURL:google_util::AppendGoogleLocaleParam(
                       GURL(kSyncGoogleDashboardURL),
                       GetApplicationContext()->GetApplicationLocale())] ];
  return footerItem;
}

#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  [super tableView:tableView didSelectRowAtIndexPath:indexPath];
  NSInteger itemType = [self.tableViewModel itemTypeForIndexPath:indexPath];
  if (itemType == ItemTypeEnterPassphrase) {
    [_passphrase becomeFirstResponder];
  }
  [tableView deselectRowAtIndexPath:indexPath animated:YES];
}

- (UIView*)tableView:(UITableView*)tableView
    viewForFooterInSection:(NSInteger)section {
  UIView* view = [super tableView:tableView viewForFooterInSection:section];
  if (SectionIdentifierPassphrase ==
      [self.tableViewModel sectionIdentifierForSectionIndex:section]) {
    TableViewLinkHeaderFooterView* linkView =
        base::apple::ObjCCastStrict<TableViewLinkHeaderFooterView>(view);
    linkView.delegate = self;
  }
  return view;
}

#pragma mark - Behavior

- (BOOL)forDecryption {
  return YES;
}

- (void)signInPressed {
  DCHECK(!_settingsAreDismissed);
  DCHECK([_passphrase text].length);
  ProfileIOS* profile = self.browser->GetProfile();

  if (!_syncObserver.get()) {
    _syncObserver.reset(new SyncObserverBridge(
        self, SyncServiceFactory::GetForProfile(profile)));
  }

  // Clear out the error message.
  self.syncErrorMessage = nil;

  syncer::SyncService* service = SyncServiceFactory::GetForProfile(profile);
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
  std::string passphrase = base::SysNSStringToUTF8([_passphrase text]);
  if ([self forDecryption]) {
    if (!service->GetUserSettings()->SetDecryptionPassphrase(passphrase)) {
      _syncObserver.reset();
      [self clearFieldsOnError:l10n_util::GetNSString(
                                   IDS_IOS_SYNC_INCORRECT_PASSPHRASE)];
      [self hideDecryptionProgress];
    }
  } else {
    service->GetUserSettings()->SetEncryptionPassphrase(passphrase);
  }
  [self reloadData];
}

- (void)cancelPressed {
  CHECK(self.presentModally);
  [self.navigationController.presentingViewController
      dismissViewControllerAnimated:YES
                         completion:nil];
}

// Sets up the navigation bar's right button. The button will be enabled iff
// `-areAllFieldsFilled` returns YES.
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

- (void)setLeftNavBarItem {
  if (!self.presentModally) {
    return;
  }
  UIBarButtonItem* cancelButton = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemCancel
                           target:self
                           action:@selector(cancelPressed)];
  self.navigationItem.leftBarButtonItem = cancelButton;
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
  if (_isDecryptionProgressShown)
    return;
  _isDecryptionProgressShown = YES;

  // Hide the button.
  self.navigationItem.rightBarButtonItem = nil;

  // Custom title view with spinner.
  DCHECK(!_savedTitle);
  DCHECK(!_savedLeftButton);
  _savedLeftButton = self.navigationItem.leftBarButtonItem;
  self.navigationItem.leftBarButtonItem = [self spinnerButton];
  _savedTitle = [self.title copy];
  self.title = self.processingMessage;
}

// Hides the UI to indicate decryption is in process.
- (void)hideDecryptionProgress {
  if (!_isDecryptionProgressShown)
    return;
  _isDecryptionProgressShown = NO;

  self.navigationItem.leftBarButtonItem = _savedLeftButton;
  _savedLeftButton = nil;
  self.title = _savedTitle;
  _savedTitle = nil;
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

  UIActivityIndicatorView* spinner = GetMediumUIActivityIndicatorView();

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
  DCHECK(!_settingsAreDismissed);
  ProfileIOS* profile = self.browser->GetProfile();
  syncer::SyncService* service = SyncServiceFactory::GetForProfile(profile);

  if (!service->IsEngineInitialized()) {
    return;
  }

  // Checking if the operation succeeded.
  if (!service->GetUserSettings()->IsPassphraseRequired() &&
      (service->GetUserSettings()->IsUsingExplicitPassphrase() ||
       [self forDecryption])) {
    _syncObserver.reset();
    SettingsNavigationController* settingsNavigationController =
        base::apple::ObjCCast<SettingsNavigationController>(
            self.navigationController);
    // During the sign-in flow it is possible for the Sync state to
    // change when the user is in the Advanced Settings (e.g., if the user
    // confirms a Sync passphrase). Because these navigation controllers are
    // not directly related to Settings, we check the type before dismissal.
    // TODO(crbug.com/40158230): Revisit with Advanced Sync Settings changes.
    if (settingsNavigationController) {
      [settingsNavigationController
          popViewControllerOrCloseSettingsAnimated:YES];
    } else if (self.presentModally) {
      [self.navigationController.presentingViewController
          dismissViewControllerAnimated:YES
                             completion:nil];
    } else {
      [self.navigationController popViewControllerAnimated:YES];
    }
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
  DCHECK(!_settingsAreDismissed);
  ProfileIOS* profile = self.browser->GetProfile();
  if (AuthenticationServiceFactory::GetForProfile(profile)->HasPrimaryIdentity(
          signin::ConsentLevel::kSignin)) {
    return;
  }
  if (!self.presentModally) {
    [base::apple::ObjCCastStrict<SettingsNavigationController>(
        self.navigationController) popViewControllerOrCloseSettingsAnimated:NO];
  }
}

#pragma mark - SettingsControllerProtocol callbacks

- (void)reportDismissalUserAction {
  // Sync Passphrase Settings screen can be closed when being presented from
  // an infobar.
  base::RecordAction(
      base::UserMetricsAction("MobileSyncPassphraseSettingsClose"));
}

- (void)reportBackUserAction {
  // No-op for this view controller.
}

- (void)settingsWillBeDismissed {
  if (_settingsAreDismissed) {
    // This method can be called twice when the account is removed. Related to
    // crbug.com/1480441.
    return;
  }

  // Remove observer bridges.
  _syncObserver.reset();
  _identityManagerObserver.reset();

  // Clear C++ ivars.
  _browser = nullptr;

  _settingsAreDismissed = true;
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  base::RecordAction(base::UserMetricsAction(
      "IOSSyncEncryptionPassphraseSettingsCloseWithSwipe"));
}

@end
