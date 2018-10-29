// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/sync_encryption_passphrase_collection_view_controller.h"

#include <memory>

#include "base/i18n/time_formatting.h"
#include "base/mac/foundation_util.h"

#include "base/strings/sys_string_conversions.h"
#include "components/browser_sync/profile_sync_service.h"
#include "components/google/core/common/google_util.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#import "components/signin/ios/browser/oauth2_token_service_observer_bridge.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#include "ios/chrome/browser/signin/authentication_service_factory.h"
#include "ios/chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "ios/chrome/browser/sync/profile_sync_service_factory.h"
#include "ios/chrome/browser/sync/sync_setup_service.h"
#include "ios/chrome/browser/sync/sync_setup_service_factory.h"
#import "ios/chrome/browser/ui/collection_view/cells/MDCCollectionViewCell+Chrome.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_cell_constants.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_footer_item.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_item.h"
#import "ios/chrome/browser/ui/collection_view/collection_view_model.h"
#import "ios/chrome/browser/ui/colors/MDCPalette+CrAdditions.h"
#import "ios/chrome/browser/ui/settings/cells/byo_textfield_item.h"
#import "ios/chrome/browser/ui/settings/cells/card_multiline_item.h"
#import "ios/chrome/browser/ui/settings/cells/passphrase_error_item.h"
#import "ios/chrome/browser/ui/settings/settings_navigation_controller.h"
#import "ios/chrome/browser/ui/settings/settings_utils.h"
#import "ios/chrome/browser/ui/settings/sync_utils/sync_util.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/signin/chrome_identity.h"
#import "ios/third_party/material_components_ios/src/components/Typography/src/MaterialTypography.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using namespace sync_encryption_passphrase;

namespace {

const CGFloat kSpinnerButtonCustomViewSize = 48;
const CGFloat kSpinnerButtonPadding = 18;

}  // namespace

@interface SyncEncryptionPassphraseCollectionViewController ()<
    OAuth2TokenServiceObserverBridgeDelegate,
    SettingsControllerProtocol> {
  ios::ChromeBrowserState* browserState_;
  // Whether the decryption progress is currently being shown.
  BOOL isDecryptionProgressShown_;
  NSString* savedTitle_;
  UIBarButtonItem* savedLeftButton_;
  std::unique_ptr<SyncObserverBridge> syncObserver_;
  std::unique_ptr<OAuth2TokenServiceObserverBridge> tokenServiceObserver_;
  UITextField* passphrase_;
}

// Sets up the navigation bar's right button. The button will be enabled iff
// |-areAllFieldsFilled| returns YES.
- (void)setRightNavBarItem;

// Returns a passphrase message item.
- (CollectionViewItem*)passphraseMessageItem;

// Returns a passphrase item.
- (CollectionViewItem*)passphraseItem;

// Returns a passphrase error item having |errorMessage| as title.
- (CollectionViewItem*)passphraseErrorItemWithMessage:(NSString*)errorMessage;

// Shows the UI to indicate the decryption is being attempted.
- (void)showDecryptionProgress;

// Hides the UI to indicate decryption is in process.
- (void)hideDecryptionProgress;

// Returns a transparent content view object to be used as a footer, or nil
// for no footer.
- (CollectionViewItem*)footerItem;

// Creates a new UIBarButtonItem with a spinner.
- (UIBarButtonItem*)spinnerButton;

@end

@implementation SyncEncryptionPassphraseCollectionViewController

@synthesize headerMessage = headerMessage_;
@synthesize footerMessage = footerMessage_;
@synthesize processingMessage = processingMessage_;
@synthesize syncErrorMessage = syncErrorMessage_;

- (instancetype)initWithBrowserState:(ios::ChromeBrowserState*)browserState {
  DCHECK(browserState);
  UICollectionViewLayout* layout = [[MDCCollectionViewFlowLayout alloc] init];
  self =
      [super initWithLayout:layout style:CollectionViewControllerStyleAppBar];
  if (self) {
    self.title = l10n_util::GetNSString(IDS_IOS_SYNC_ENTER_PASSPHRASE_TITLE);
    self.shouldHideDoneButton = YES;
    browserState_ = browserState;
    NSString* userEmail =
        AuthenticationServiceFactory::GetForBrowserState(browserState_)
            ->GetAuthenticatedUserEmail();
    DCHECK(userEmail);
    browser_sync::ProfileSyncService* service =
        ProfileSyncServiceFactory::GetForBrowserState(browserState_);
    if (service->IsEngineInitialized() &&
        service->IsUsingSecondaryPassphrase()) {
      base::Time passphrase_time = service->GetExplicitPassphraseTime();
      if (!passphrase_time.is_null()) {
        base::string16 passphrase_time_str =
            base::TimeFormatShortDate(passphrase_time);
        self.headerMessage = l10n_util::GetNSStringF(
            IDS_IOS_SYNC_ENTER_PASSPHRASE_BODY_WITH_EMAIL_AND_DATE,
            base::SysNSStringToUTF16(userEmail), passphrase_time_str);
      } else {
        self.headerMessage = l10n_util::GetNSStringF(
            IDS_IOS_SYNC_ENTER_PASSPHRASE_BODY_WITH_EMAIL,
            base::SysNSStringToUTF16(userEmail));
      }
    } else {
      self.headerMessage =
          l10n_util::GetNSString(IDS_SYNC_ENTER_GOOGLE_PASSPHRASE_BODY);
    }
    self.processingMessage = l10n_util::GetNSString(IDS_SYNC_LOGIN_SETTING_UP);
    footerMessage_ = l10n_util::GetNSString(IDS_IOS_SYNC_PASSPHRASE_RECOVER);

    tokenServiceObserver_.reset(new OAuth2TokenServiceObserverBridge(
        ProfileOAuth2TokenServiceFactory::GetForBrowserState(browserState_),
        self));

    // TODO(crbug.com/764578): -loadModel should not be called from
    // initializer. A possible fix is to move this call to -viewDidLoad.
    [self loadModel];
  }
  return self;
}

- (UITextField*)passphrase {
  return passphrase_;
}

- (NSString*)syncErrorMessage {
  if (syncErrorMessage_)
    return syncErrorMessage_;
  SyncSetupService* service =
      SyncSetupServiceFactory::GetForBrowserState(browserState_);
  DCHECK(service);
  SyncSetupService::SyncServiceState syncServiceState =
      service->GetSyncServiceState();

  // Passphrase error directly set |syncErrorMessage_|.
  if (syncServiceState == SyncSetupService::kSyncServiceNeedsPassphrase)
    return nil;

  return GetSyncErrorMessageForBrowserState(browserState_);
}

#pragma mark - View lifecycle

- (void)viewDidLoad {
  [super viewDidLoad];
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

#pragma mark - SettingsRootCollectionViewController

- (void)loadModel {
  [super loadModel];
  CollectionViewModel* model = self.collectionViewModel;

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
  // TODO(crbug.com/650424): Footer items must currently go into a separate
  // section, to work around a drawing bug in MDC.
  [model addSectionWithIdentifier:SectionIdentifierFooter];
  [model addItem:[self footerItem]
      toSectionWithIdentifier:SectionIdentifierFooter];
}

#pragma mark - Items

- (CollectionViewItem*)passphraseMessageItem {
  CardMultilineItem* item =
      [[CardMultilineItem alloc] initWithType:ItemTypeMessage];
  item.text = headerMessage_;
  return item;
}

- (CollectionViewItem*)passphraseItem {
  if (passphrase_) {
    [self unregisterTextField:passphrase_];
  }
  passphrase_ = [[UITextField alloc] init];
  [passphrase_ setSecureTextEntry:YES];
  [passphrase_ setBackgroundColor:[UIColor clearColor]];
  [passphrase_ setAutoresizingMask:UIViewAutoresizingFlexibleWidth];
  [passphrase_ setAutocorrectionType:UITextAutocorrectionTypeNo];
  [passphrase_
      setPlaceholder:l10n_util::GetNSString(IDS_SYNC_PASSPHRASE_LABEL)];
  [self registerTextField:passphrase_];

  BYOTextFieldItem* item =
      [[BYOTextFieldItem alloc] initWithType:ItemTypeEnterPassphrase];
  item.textField = passphrase_;
  return item;
}

- (CollectionViewItem*)passphraseErrorItemWithMessage:(NSString*)errorMessage {
  PassphraseErrorItem* item =
      [[PassphraseErrorItem alloc] initWithType:ItemTypeError];
  item.text = errorMessage;
  return item;
}

- (CollectionViewItem*)footerItem {
  CollectionViewFooterItem* footerItem =
      [[CollectionViewFooterItem alloc] initWithType:ItemTypeFooter];
  footerItem.cellStyle = CollectionViewCellStyle::kUIKit;
  footerItem.text = self.footerMessage;
  footerItem.linkURL = google_util::AppendGoogleLocaleParam(
      GURL(kSyncGoogleDashboardURL),
      GetApplicationContext()->GetApplicationLocale());
  footerItem.linkDelegate = self;
  return footerItem;
}

#pragma mark - MDCCollectionViewStylingDelegate

- (MDCCollectionViewCellStyle)collectionView:(UICollectionView*)collectionView
                         cellStyleForSection:(NSInteger)section {
  NSInteger sectionIdentifier =
      [self.collectionViewModel sectionIdentifierForSection:section];
  switch (sectionIdentifier) {
    case SectionIdentifierFooter:
      // Display the Learn More footer in the default style with no "card" UI
      // and no section padding.
      return MDCCollectionViewCellStyleDefault;
    default:
      return self.styler.cellStyle;
  }
}

- (BOOL)collectionView:(UICollectionView*)collectionView
    shouldHideItemBackgroundAtIndexPath:(NSIndexPath*)indexPath {
  NSInteger sectionIdentifier =
      [self.collectionViewModel sectionIdentifierForSection:indexPath.section];
  switch (sectionIdentifier) {
    case SectionIdentifierFooter:
      // Display the Learn More footer without any background image or
      // shadowing.
      return YES;
    default:
      return NO;
  }
}

- (CGFloat)collectionView:(UICollectionView*)collectionView
    cellHeightAtIndexPath:(NSIndexPath*)indexPath {
  CollectionViewItem* item =
      [self.collectionViewModel itemAtIndexPath:indexPath];
  if (item.type == ItemTypeMessage || item.type == ItemTypeFooter) {
    return [MDCCollectionViewCell
        cr_preferredHeightForWidth:CGRectGetWidth(collectionView.bounds)
                           forItem:item];
  }
  return MDCCellDefaultOneLineHeight;
}

#pragma mark - UICollectionViewDelegate

- (void)collectionView:(UICollectionView*)collectionView
    didSelectItemAtIndexPath:(NSIndexPath*)indexPath {
  [super collectionView:collectionView didSelectItemAtIndexPath:indexPath];
  NSInteger itemType =
      [self.collectionViewModel itemTypeForIndexPath:indexPath];
  if (itemType == ItemTypeEnterPassphrase) {
    [passphrase_ becomeFirstResponder];
  }
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

  browser_sync::ProfileSyncService* service =
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
    if (!service->SetDecryptionPassphrase(passphrase)) {
      syncObserver_.reset();
      [self clearFieldsOnError:l10n_util::GetNSString(
                                   IDS_IOS_SYNC_INCORRECT_PASSPHRASE)];
      [self hideDecryptionProgress];
    }
  } else {
    service->EnableEncryptEverything();
    service->SetEncryptionPassphrase(passphrase);
  }
  [self reloadData];
}

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
  self.title = processingMessage_;
}

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
  tokenServiceObserver_.reset();
}

#pragma mark - UIControl events listener

- (void)textFieldDidBeginEditing:(id)sender {
  // Remove the error cell if there is one.
  CollectionViewModel* model = self.collectionViewModel;
  NSInteger section =
      [model sectionForSectionIdentifier:SectionIdentifierPassphrase];
  NSIndexPath* errorIndexPath =
      [NSIndexPath indexPathForItem:ItemTypeError inSection:section];
  if ([model hasItemAtIndexPath:errorIndexPath] &&
      [model itemTypeForIndexPath:errorIndexPath] == ItemTypeError) {
    DCHECK(self.syncErrorMessage);
    [model removeItemWithType:ItemTypeError
        fromSectionWithIdentifier:SectionIdentifierPassphrase];
    [self.collectionView deleteItemsAtIndexPaths:@[ errorIndexPath ]];
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
  browser_sync::ProfileSyncService* service =
      ProfileSyncServiceFactory::GetForBrowserState(browserState_);

  if (!service->IsEngineInitialized()) {
    return;
  }

  // Checking if the operation succeeded.
  if (!service->IsPassphraseRequired() &&
      (service->IsUsingSecondaryPassphrase() || [self forDecryption])) {
    syncObserver_.reset();
    [base::mac::ObjCCastStrict<SettingsNavigationController>(
        self.navigationController)
        popViewControllerOrCloseSettingsAnimated:YES];
    return;
  }

  // Handling passphrase error case.
  if (service->IsPassphraseRequired()) {
    self.syncErrorMessage =
        l10n_util::GetNSString(IDS_IOS_SYNC_INCORRECT_PASSPHRASE);
  }
  [self hideDecryptionProgress];
  [self reloadData];
}

#pragma mark - OAuth2TokenServiceObserverBridgeDelegate

- (void)onEndBatchChanges {
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
