// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/sync/sync_encryption_passphrase_table_view_controller.h"

#import <memory>

#import "base/i18n/time_formatting.h"
#import "base/ios/ios_util.h"
#import "base/mac/foundation_util.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/strings/sys_string_conversions.h"
#import "components/google/core/common/google_util.h"
#import "components/signin/public/identity_manager/objc/identity_manager_observer_bridge.h"
#import "components/strings/grit/components_strings.h"
#import "components/sync/driver/sync_service.h"
#import "components/sync/driver/sync_user_settings.h"
#import "ios/chrome/browser/application_context/application_context.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/net/crurl.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/signin/identity_manager_factory.h"
#import "ios/chrome/browser/signin/system_identity.h"
#import "ios/chrome/browser/sync/sync_service_factory.h"
#import "ios/chrome/browser/sync/sync_setup_service.h"
#import "ios/chrome/browser/sync/sync_setup_service_factory.h"
#import "ios/chrome/browser/ui/main/scene_state.h"
#import "ios/chrome/browser/ui/main/scene_state_browser_agent.h"
#import "ios/chrome/browser/ui/scoped_ui_blocker/scoped_ui_blocker.h"
#import "ios/chrome/browser/ui/settings/cells/byo_textfield_item.h"
#import "ios/chrome/browser/ui/settings/cells/passphrase_error_item.h"
#import "ios/chrome/browser/ui/settings/google_services/google_services_settings_constants.h"
#import "ios/chrome/browser/ui/settings/settings_navigation_controller.h"
#import "ios/chrome/browser/ui/settings/sync/utils/sync_util.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_link_header_footer_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_text_item.h"
#import "ios/chrome/browser/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/ui/ui_feature_flags.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/url/chrome_url_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "url/gurl.h"

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

  ChromeBrowserState* browserState = _browser->GetBrowserState();
  syncer::SyncService* service =
      SyncServiceFactory::GetForBrowserState(browserState);
  // TODO(crbug.com/1208307): The reason this is an if and not a DCHECK is
  // because SyncCreatePassphraseTableViewController inherits from this class.
  // This should be changed, i.e. either extract the minimum common logic
  // between the 2 to a new base class, or not share code at all.
  if (service->IsEngineInitialized() &&
      service->GetUserSettings()->IsUsingExplicitPassphrase()) {
    base::Time passphraseTime =
        service->GetUserSettings()->GetExplicitPassphraseTime();
    NSString* userEmail =
        AuthenticationServiceFactory::GetForBrowserState(browserState)
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
          IdentityManagerFactory::GetForBrowserState(browserState), self);
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
  ChromeBrowserState* browserState = self.browser->GetBrowserState();
  SyncSetupService* service =
      SyncSetupServiceFactory::GetForBrowserState(browserState);
  DCHECK(service);
  SyncSetupService::SyncServiceState syncServiceState =
      service->GetSyncServiceState();

  // Passphrase error directly set `_syncErrorMessage`.
  if (syncServiceState == SyncSetupService::kSyncServiceNeedsPassphrase)
    return nil;

  return GetSyncErrorMessageForBrowserState(browserState);
}

#pragma mark - View lifecycle

- (void)viewDidLoad {
  [super viewDidLoad];
  [self loadModel];
  [self setRightNavBarItem];

  SceneState* sceneState =
      SceneStateBrowserAgent::FromBrowser(self.browser)->GetSceneState();
  _uiBlocker = std::make_unique<ScopedUIBlocker>(sceneState);
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
  DCHECK(!_settingsAreDismissed);
  DCHECK([_passphrase text].length);
  ChromeBrowserState* browserState = self.browser->GetBrowserState();

  if (!_syncObserver.get()) {
    _syncObserver.reset(new SyncObserverBridge(
        self, SyncServiceFactory::GetForBrowserState(browserState)));
  }

  // Clear out the error message.
  self.syncErrorMessage = nil;

  syncer::SyncService* service =
      SyncServiceFactory::GetForBrowserState(browserState);
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
  ChromeBrowserState* browserState = self.browser->GetBrowserState();
  syncer::SyncService* service =
      SyncServiceFactory::GetForBrowserState(browserState);

  if (!service->IsEngineInitialized()) {
    return;
  }

  // Checking if the operation succeeded.
  if (!service->GetUserSettings()->IsPassphraseRequired() &&
      (service->GetUserSettings()->IsUsingExplicitPassphrase() ||
       [self forDecryption])) {
    _syncObserver.reset();
    SettingsNavigationController* settingsNavigationController =
        base::mac::ObjCCast<SettingsNavigationController>(
            self.navigationController);
    // During the sign-in flow it is possible for the Sync state to
    // change when the user is in the Advanced Settings (e.g., if the user
    // confirms a Sync passphrase). Because these navigation controllers are
    // not directly related to Settings, we check the type before dismissal.
    // TODO(crbug.com/1151287): Revisit with Advanced Sync Settings changes.
    if (settingsNavigationController) {
      [settingsNavigationController
          popViewControllerOrCloseSettingsAnimated:YES];
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
  ChromeBrowserState* browserState = self.browser->GetBrowserState();
  if (AuthenticationServiceFactory::GetForBrowserState(browserState)
          ->HasPrimaryIdentity(signin::ConsentLevel::kSignin)) {
    return;
  }
  [base::mac::ObjCCastStrict<SettingsNavigationController>(
      self.navigationController) popViewControllerOrCloseSettingsAnimated:NO];
}

#pragma mark - SettingsControllerProtocol callbacks

- (void)reportDismissalUserAction {
  // Sync Passphrase Settings screen can be closed when being presented from
  // an infobar.
  base::RecordAction(
      base::UserMetricsAction("MobileSyncPassphraseSettingsClose"));
}

- (void)reportBackUserAction {
  NOTREACHED();
}

- (void)settingsWillBeDismissed {
  DCHECK(!_settingsAreDismissed);

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
