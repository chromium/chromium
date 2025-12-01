// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/credential_provider_extension/credential_provider_view_controller.h"

#import <Foundation/Foundation.h>

#import "base/apple/foundation_util.h"
#import "base/check.h"
#import "base/command_line.h"
#import "base/ios/block_types.h"
#import "base/not_fatal_until.h"
#import "base/notreached.h"
#import "components/webauthn/core/browser/passkey_model_utils.h"
#import "ios/chrome/common/app_group/app_group_metrics.h"
#import "ios/chrome/common/app_group/app_group_utils.h"
#import "ios/chrome/common/crash_report/crash_helper.h"
#import "ios/chrome/common/credential_provider/archivable_credential_store.h"
#import "ios/chrome/common/credential_provider/constants.h"
#import "ios/chrome/common/credential_provider/credential.h"
#import "ios/chrome/common/credential_provider/multi_store_credential_store.h"
#import "ios/chrome/common/credential_provider/passkey_keychain_provider_bridge.h"
#import "ios/chrome/common/credential_provider/ui/passkey_welcome_screen_strings.h"
#import "ios/chrome/common/credential_provider/ui/passkey_welcome_screen_view_controller.h"
#import "ios/chrome/common/credential_provider/user_defaults_credential_store.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"
#import "ios/chrome/common/ui/reauthentication/reauthentication_module.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/credential_provider_extension/account_verification_provider.h"
#import "ios/chrome/credential_provider_extension/font_provider.h"
#import "ios/chrome/credential_provider_extension/metrics_util.h"
#import "ios/chrome/credential_provider_extension/passkey_request_details.h"
#import "ios/chrome/credential_provider_extension/passkey_util.h"
#import "ios/chrome/credential_provider_extension/passkey_welcome_screen_util.h"
#import "ios/chrome/credential_provider_extension/reauthentication_handler.h"
#import "ios/chrome/credential_provider_extension/ui/consent_coordinator.h"
#import "ios/chrome/credential_provider_extension/ui/create_navigation_item_title_view.h"
#import "ios/chrome/credential_provider_extension/ui/credential_list_coordinator.h"
#import "ios/chrome/credential_provider_extension/ui/credential_response_handler.h"
#import "ios/chrome/credential_provider_extension/ui/feature_flags.h"
#import "ios/chrome/credential_provider_extension/ui/generic_error_view_controller.h"
#import "ios/chrome/credential_provider_extension/ui/multi_profile_passkey_creation_view_controller.h"
#import "ios/chrome/credential_provider_extension/ui/passkey_error_alert_view_controller.h"
#import "ios/chrome/credential_provider_extension/ui/stale_credentials_view_controller.h"
#import "ios/components/credential_provider_extension/password_util.h"

using app_group::UserDefaultsStringForKey;

namespace {

UIColor* BackgroundColor() {
  return [UIColor colorNamed:kGroupedPrimaryBackgroundColor];
}

// Returns whether the user has at least one saved passkey credential.
BOOL HasSavedPasskeys(NSArray<id<Credential>>* credentials) {
  NSUInteger passkey_credential_index =
      [credentials indexOfObjectPassingTest:^BOOL(id<Credential> credential,
                                                  NSUInteger, BOOL*) {
        return credential.isPasskey;
      }];
  return passkey_credential_index != NSNotFound;
}

enum class PasskeyCreationEligibility {
  kCanCreate,
  kCanCreateWithUserInteraction,
  kSaveDisabledByUser,
  kSaveDisabledByEnterprise,
  kPasswordSyncDisabled,
  kSignedOut,
  kUnsupportedAlgorithm,
  kExcludedPasskey,
};

enum class PasskeyUserVerificationStatus {
  kNotRequired,
  kRequired,
  kCompleted
};

}  // namespace

// TODO(crbug.com/454307667): Add unit tests for the whole file.
@interface CredentialProviderViewController () <
    ConfirmationAlertActionHandler,
    CredentialResponseHandler,
    PasskeyKeychainProviderBridgeDelegate,
    PasskeyWelcomeScreenViewControllerDelegate,
    MultiProfilePasskeyCreationViewControllerDelegate,
    SuccessfulReauthTimeAccessor,
    UIAdaptivePresentationControllerDelegate>

// Interface for the persistent credential store.
@property(nonatomic, strong) id<CredentialStore> credentialStore;

// List coordinator that shows the list of passwords when started.
@property(nonatomic, strong) CredentialListCoordinator* listCoordinator;

// Consent coordinator that shows a view requesting device auth in order to
// enable the extension.
@property(nonatomic, strong) ConsentCoordinator* consentCoordinator;

// Date kept for ReauthenticationModule.
@property(nonatomic, strong) NSDate* lastSuccessfulReauthTime;

// Reauthentication Module used for reauthentication.
@property(nonatomic, strong) ReauthenticationModule* reauthenticationModule;

// Interface for `reauthenticationModule`, handling mostly the case when no
// hardware for authentication is available.
@property(nonatomic, strong) ReauthenticationHandler* reauthenticationHandler;

// Interface for verified that accounts are still valid.
@property(nonatomic, strong) AccountVerificationProvider* accountVerificator;

// Loading indicator used for user validation, which APIs can take a long time.
@property(nonatomic, strong) UIActivityIndicatorView* activityIndicatorView;

// Identifiers cached in `-prepareCredentialListForServiceIdentifiers:` to show
// the next time this view appears.
@property(nonatomic, strong)
    NSArray<ASCredentialServiceIdentifier*>* serviceIdentifiers;

// Navigation controller to present passkey-related UIs.
@property(nonatomic, strong)
    UINavigationController* passkeyNavigationController;

// Navigation title view for the passkeyNavigationController.
@property(nonatomic, strong) UIView* passkeyNavigationItemTitleView;

// Bridge to the PasskeyKeychainProvider that manages passkey vault keys.
@property(nonatomic, strong)
    PasskeyKeychainProviderBridge* passkeyKeychainProviderBridge;

// Indicates the status of user verification (required, completed, or not
// needed) for the current passkey flow. Uninitialized and/or stale if the user
// is not currently in a passkey flow.
@property(nonatomic, assign)
    PasskeyUserVerificationStatus userVerificationStatus;

@end

@implementation CredentialProviderViewController {
  // Information about a passkey credential request.
  PasskeyRequestDetails* _passkeyRequestDetails;
}

+ (void)initialize {
  if (self == [CredentialProviderViewController self]) {
    crash_helper::common::StartCrashpad();
  }
}

- (void)viewDidLoad {
  [super viewDidLoad];
  self.view.backgroundColor = BackgroundColor();

  UINavigationBar* navigationBar = [self createNavigationBar];
  [self.view addSubview:navigationBar];
  AddSameConstraintsToSides(
      navigationBar, self.view.safeAreaLayoutGuide,
      LayoutSides::kTrailing | LayoutSides::kTop | LayoutSides::kLeading);
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];

  // If identifiers were stored in
  // `-prepareCredentialListForServiceIdentifiers:`, handle that now.
  if (self.serviceIdentifiers) {
    NSArray<ASCredentialServiceIdentifier*>* serviceIdentifiers =
        self.serviceIdentifiers;
    self.serviceIdentifiers = nil;
    BOOL isAccessingPasskeys = _passkeyRequestDetails != nil;

    __weak __typeof__(self) weakSelf = self;
    [self validateUserWithCompletion:^(BOOL userIsValid) {
      if (!userIsValid) {
        [weakSelf showStaleCredentials];
        return;
      }
      [weakSelf
          reauthenticateIfNeededToAccessPasskeys:isAccessingPasskeys
                           withCompletionHandler:^(
                               ReauthenticationResult result) {
                             if (result != ReauthenticationResult::kFailure) {
                               [weakSelf
                                   showCredentialListForServiceIdentifiers:
                                       serviceIdentifiers];
                             } else {
                               [weakSelf exitWithErrorCode:
                                             ASExtensionErrorCodeFailed];
                             }
                           }];
    }];

    return;
  }
}

#pragma mark - ASCredentialProviderViewController

- (void)prepareCredentialListForServiceIdentifiers:
    (NSArray<ASCredentialServiceIdentifier*>*)serviceIdentifiers {
  // Sometimes, this method is called while the authentication framework thinks
  // the app is not foregrounded, so authentication fails. Instead of directly
  // authenticating and showing the credentials, store the list of
  // identifiers and authenticate once the extension is visible.
  self.serviceIdentifiers = serviceIdentifiers;
  _passkeyRequestDetails = nil;
}

// The system calls this method when there’s an active passkey request in the
// app or website.
- (void)prepareCredentialListForServiceIdentifiers:
            (NSArray<ASCredentialServiceIdentifier*>*)serviceIdentifiers
                                 requestParameters:
                                     (ASPasskeyCredentialRequestParameters*)
                                         requestParameters {
  self.serviceIdentifiers = serviceIdentifiers;
  _passkeyRequestDetails =
      [self passkeyDetailsFromParameters:requestParameters];
}

- (void)provideCredentialWithoutUserInteractionForRequest:
    (id<ASCredentialRequest>)credentialRequest {
  if (credentialRequest.type == ASCredentialRequestTypePasskeyAssertion) {
    // Unlike passwords, iOS doesn't already gate passkeys with device auth. If
    // the credential request is for a passkey, first evaluate if a device auth
    // is needed or not. If auth is needed, then the "with user interaction"
    // path needs to be taken.
    if ([self passkeyDetailsFromRequest:credentialRequest]
            .userVerificationRequired) {
      [self exitWithErrorCode:ASExtensionErrorCodeUserInteractionRequired];
      return;
    }
  }

  __weak __typeof__(self) weakSelf = self;
  [self validateUserWithCompletion:^(BOOL userIsValid) {
    // `reauthenticationModule` can't attempt reauth when no passcode is set.
    // This means a credential shouldn't be retrieved just yet.
    if (!weakSelf.reauthenticationModule.canAttemptReauth || !userIsValid) {
      [weakSelf exitWithErrorCode:ASExtensionErrorCodeUserInteractionRequired];
      return;
    }
    // iOS already gates the credential of password type with device auth for
    // -provideCredentialWithoutUserInteractionForRequest:. Not using
    // `reauthenticationModule` here to avoid a double authentication request.
    // If the credential is a passkey and reauthentication is needed, it's
    // already been taken care of above.
    [weakSelf provideCredentialForRequest:credentialRequest];
  }];
}

- (void)prepareInterfaceToProvideCredentialForRequest:
    (id<ASCredentialRequest>)credentialRequest {
  __weak __typeof__(self) weakSelf = self;
  if (credentialRequest.type == ASCredentialRequestTypePasskeyAssertion) {
    // Reaching this code means that user reauthentication is needed in order to
    // proceed with the passkey assertion. Reauthentication will be performed
    // later on in the assertion process, so no need to reauthenticate just yet.
    [self validateUserWithCompletion:^(BOOL userIsValid) {
      if (!userIsValid) {
        [weakSelf exitWithErrorCode:ASExtensionErrorCodeFailed];
        return;
      }
      [weakSelf provideCredentialForRequest:credentialRequest];
    }];
    return;
  }

  // TODO(crbug.com/376468308): Investigate whether this code is working as
  // expected or whether it's called at all. Technically, when this method is
  // called, the CPE is not yet foregrounded and, hence, can't present the
  // reauthentication process.
  [self validateUserWithCompletion:^(BOOL userIsValid) {
    if (!userIsValid) {
      [weakSelf showStaleCredentials];
      return;
    }
    [weakSelf
        reauthenticateIfNeededToAccessPasskeys:NO
                         withCompletionHandler:^(
                             ReauthenticationResult result) {
                           if (result != ReauthenticationResult::kFailure) {
                             [weakSelf
                                 provideCredentialForRequest:credentialRequest];
                           } else {
                             [weakSelf exitWithErrorCode:
                                           ASExtensionErrorCodeUserCanceled];
                           }
                         }];
  }];
}

- (void)prepareInterfaceForExtensionConfiguration {
  if (HasSavedPasskeys(self.credentialStore.credentials)) {
    __weak __typeof__(self) weakSelf = self;
    auto completion = ^(NSArray<NSData*>* securityDomainSecrets) {
      [weakSelf completeSecurityDomainSecretFetchForExtensionConfiguration];
    };

    // Trigger a security domain secret fetch to know whether the user needs to
    // bootstrap (create/enter their GPM pin) to use passkeys on their device.
    // If bootstrapping is needed, then the fetching flow will take care of
    // presenting the relevant UI. The `completion` will then take care of
    // dismissing the bootstrapping UI if it was presented. If it wasn't
    // presented, it means that the user was already bootstrapped. In this case,
    // `completion` will present the ConsentViewController.
    [self fetchSecurityDomainSecretForGaia:[self gaia]
                                credential:nil
                                   purpose:webauthn::ReauthenticatePurpose::
                                               kUnspecified
                  userVerificationRequired:NO
                                completion:completion];
  } else {
    [self presentConsentViewController];
  }
}

// Only available in iOS 18.0+.
- (void)performPasskeyRegistrationWithoutUserInteractionIfPossible:
    (ASPasskeyCredentialRequest*)registrationRequest API_AVAILABLE(ios(18.0)) {
  PasskeyRequestDetails* passkeyRequestDetails =
      [self passkeyDetailsFromConditionalCreateRequest:registrationRequest];
  if (![passkeyRequestDetails
          hasMatchingPassword:self.credentialStore.credentials]) {
    [self exitWithErrorCode:ASExtensionErrorCodeFailed];
    return;
  }

  NSString* gaia = [self gaia];
  PasskeyCreationEligibility passkeyCreationEligibility =
      [self passkeyCreationEligibilityForGaia:gaia
                        passkeyRequestDetails:passkeyRequestDetails];

  // For any other state than `kCanCreate`, either passkey creation is not
  // allowed or user interaction is required, so exit immediately.
  if (passkeyCreationEligibility != PasskeyCreationEligibility::kCanCreate) {
    [self exitWithErrorCode:ASExtensionErrorCodeFailed];
    return;
  }

  // Try to create a passkey while user interaction is disallowed.
  [self createPasskeyWithDetails:passkeyRequestDetails gaia:gaia];
}

- (void)prepareInterfaceForPasskeyRegistration:
    (id<ASCredentialRequest>)registrationRequest {
  if (![registrationRequest isKindOfClass:[ASPasskeyCredentialRequest class]]) {
    [self exitWithErrorCode:ASExtensionErrorCodeFailed];
    return;
  }

  PasskeyRequestDetails* passkeyRequestDetails =
      [self passkeyDetailsFromRequest:registrationRequest];
  NSString* gaia = [self gaia];
  PasskeyCreationEligibility passkeyCreationEligibility =
      [self passkeyCreationEligibilityForGaia:gaia
                        passkeyRequestDetails:passkeyRequestDetails];

  switch (passkeyCreationEligibility) {
    case PasskeyCreationEligibility::kSaveDisabledByUser:
      [self showSavingManuallyDisabledAlert];
      return;
    case PasskeyCreationEligibility::kSaveDisabledByEnterprise:
      [self showSavingDisabledByEnterpriseAlert];
      return;
    case PasskeyCreationEligibility::kPasswordSyncDisabled:
      [self showSavingToAccountDisabledAlert];
      return;
    case PasskeyCreationEligibility::kSignedOut:
      [self showSignedOutUserAlert];
      return;
    case PasskeyCreationEligibility::kUnsupportedAlgorithm:
      [self exitWithErrorCode:ASExtensionErrorCodeFailed];
      return;
    case PasskeyCreationEligibility::kExcludedPasskey:
      // Note: ASExtensionErrorCodeMatchedExcludedCredential is iOS 18.0+ only,
      // but so is the excludedCredentials array, so we can't reach this point
      // if the iOS version is below 18.0, which is why there's no need for an
      // else statement.
      if (@available(iOS 18.0, *)) {
        [self exitWithErrorCode:ASExtensionErrorCodeMatchedExcludedCredential];
      }
      return;
    case PasskeyCreationEligibility::kCanCreateWithUserInteraction:
      if ([self isUsingMultiProfile]) {
        [self showMultiProfilePasskeyCreationDialogWithDetails:
                  passkeyRequestDetails
                                                          gaia:gaia];
        return;
      }
      break;
    case PasskeyCreationEligibility::kCanCreate:
      // Passkey creation is allowed.
      break;
  }

  [self validateUserAndCreatePasskeyWithDetails:passkeyRequestDetails
                                           gaia:gaia];
}

- (void)reportUnknownPublicKeyCredentialForRelyingParty:(NSString*)relyingParty
                                           credentialID:(NSData*)credentialID {
  if (!IsSignalAPIEnabled()) {
    return;
  }

  NSArray<id<Credential>>* credentials = self.credentialStore.credentials;
  NSUInteger credentialIndex =
      [credentials indexOfObjectPassingTest:^BOOL(id<Credential> credential,
                                                  NSUInteger idx, BOOL* stop) {
        return [credential.rpId isEqualToString:relyingParty] &&
               [credential.credentialId isEqualToData:credentialID];
      }];
  if (credentialIndex == NSNotFound) {
    return;
  }

  id<Credential> credential = credentials[credentialIndex];
  credential.hidden = YES;
  credential.hiddenTime = base::Time::Now().InMillisecondsSinceUnixEpoch();
  SavePasskeyCredential(credential);
}

- (void)reportPublicKeyCredentialUpdateForRelyingParty:(NSString*)relyingParty
                                            userHandle:(NSData*)userHandle
                                               newName:(NSString*)newName {
  if (!IsSignalAPIEnabled()) {
    return;
  }

  NSArray<id<Credential>>* credentials = self.credentialStore.credentials;
  NSUInteger credentialIndex =
      [credentials indexOfObjectPassingTest:^BOOL(id<Credential> credential,
                                                  NSUInteger idx, BOOL* stop) {
        return [credential.rpId isEqualToString:relyingParty] &&
               [credential.userId isEqualToData:userHandle];
      }];
  if (credentialIndex == NSNotFound) {
    return;
  }

  id<Credential> credential = credentials[credentialIndex];

  // Respect the user's choice and skip the update if the data was explicitly
  // changed by the user previously or if the username did not change.
  if (credential.editedByUser ||
      [credential.username isEqualToString:newName]) {
    return;
  }

  credential.username = newName;
  SavePasskeyCredential(credential);
}

- (void)reportAllAcceptedPublicKeyCredentialsForRelyingParty:
            (NSString*)relyingParty
                                                  userHandle:(NSData*)userHandle
                                       acceptedCredentialIDs:
                                           (NSArray<NSData*>*)
                                               acceptedCredentialIDs {
  if (!IsSignalAPIEnabled()) {
    return;
  }

  NSArray<id<Credential>>* credentials = self.credentialStore.credentials;
  NSUInteger credentialIndex =
      [credentials indexOfObjectPassingTest:^BOOL(id<Credential> credential,
                                                  NSUInteger idx, BOOL* stop) {
        return [credential.rpId isEqualToString:relyingParty] &&
               [credential.userId isEqualToData:userHandle];
      }];
  if (credentialIndex == NSNotFound) {
    return;
  }

  id<Credential> credential = credentials[credentialIndex];
  BOOL credentialShouldBeHidden =
      ![acceptedCredentialIDs containsObject:credential.credentialId];
  if (credential.hidden == credentialShouldBeHidden) {
    return;
  }

  credential.hidden = credentialShouldBeHidden;
  credential.hiddenTime = credentialShouldBeHidden
                              ? base::Time::Now().InMillisecondsSinceUnixEpoch()
                              : 0;
  SavePasskeyCredential(credential);
}

- (void)reportUnusedPasswordCredentialForDomain:(NSString*)domain
                                       userName:(NSString*)userName {
  // Password credential updates are currently not handled.
}

#pragma mark - Properties

- (id<CredentialStore>)credentialStore {
  if (!_credentialStore) {
    ArchivableCredentialStore* archivableStore =
        [[ArchivableCredentialStore alloc]
            initWithFileURL:CredentialProviderSharedArchivableStoreURL()];

    NSString* key = AppGroupUserDefaultsCredentialProviderNewCredentials();
    UserDefaultsCredentialStore* defaultsStore =
        [[UserDefaultsCredentialStore alloc]
            initWithUserDefaults:app_group::GetGroupUserDefaults()
                             key:key];
    _credentialStore = [[MultiStoreCredentialStore alloc]
        initWithStores:@[ defaultsStore, archivableStore ]];
  }
  return _credentialStore;
}

- (ReauthenticationHandler*)reauthenticationHandler {
  if (!_reauthenticationHandler) {
    _reauthenticationHandler = [[ReauthenticationHandler alloc]
        initWithReauthenticationModule:self.reauthenticationModule];
  }
  return _reauthenticationHandler;
}

- (ReauthenticationModule*)reauthenticationModule {
  if (!_reauthenticationModule) {
    _reauthenticationModule = [[ReauthenticationModule alloc]
        initWithSuccessfulReauthTimeAccessor:self];
  }
  return _reauthenticationModule;
}

- (AccountVerificationProvider*)accountVerificator {
  if (!_accountVerificator) {
    _accountVerificator = [[AccountVerificationProvider alloc] init];
  }
  return _accountVerificator;
}

- (UINavigationController*)passkeyNavigationController {
  if (!_passkeyNavigationController) {
    self.passkeyNavigationController = [[UINavigationController alloc] init];
    self.passkeyNavigationController.modalPresentationStyle =
        UIModalPresentationCurrentContext;
  }
  return _passkeyNavigationController;
}

- (UIView*)passkeyNavigationItemTitleView {
  if (!_passkeyNavigationItemTitleView) {
    self.passkeyNavigationItemTitleView =
        credential_provider_extension::CreateNavigationItemTitleView(
            ios::provider::GetBrandedProductRegularFont(UIFont.labelFontSize));
  }
  return _passkeyNavigationItemTitleView;
}

- (PasskeyKeychainProviderBridge*)passkeyKeychainProviderBridge {
  if (!_passkeyKeychainProviderBridge) {
    _passkeyKeychainProviderBridge = [[PasskeyKeychainProviderBridge alloc]
          initWithEnableLogging:[self metricsAreEnabled]
           navigationController:self.passkeyNavigationController
        navigationItemTitleView:self.passkeyNavigationItemTitleView];
    _passkeyKeychainProviderBridge.delegate = self;
  }
  return _passkeyKeychainProviderBridge;
}

#pragma mark - ConfirmationAlertActionHandler

- (void)confirmationAlertPrimaryAction {
  if ([self.presentedViewController
          isKindOfClass:[PasskeyErrorAlertViewController class]]) {
    [self dismissViewControllerAnimated:YES completion:nil];
    [self exitWithErrorCode:ASExtensionErrorCodeFailed];
  }
}

#pragma mark - CredentialResponseHandler

- (void)userSelectedPassword:(ASPasswordCredential*)credential {
  [self completeRequestWithSelectedCredential:credential];
}

- (void)userSelectedPasskey:(ASPasskeyAssertionCredential*)credential {
  if (credential) {
    [self completeAssertionRequestWithSelectedPasskeyCredential:credential];
  } else {
    [self exitWithErrorCode:ASExtensionErrorCodeCredentialIdentityNotFound];
  }
}

- (void)userSelectedPasskey:(id<Credential>)credential
      passkeyRequestDetails:(PasskeyRequestDetails*)passkeyRequestDetails {
  __weak __typeof(self) weakSelf = self;
  auto completion = ^(NSArray<NSData*>* securityDomainSecrets) {
    [weakSelf passkeyAssertionWithCredential:credential
                       passkeyRequestDetails:passkeyRequestDetails
                       securityDomainSecrets:securityDomainSecrets];
  };

  [self
      fetchSecurityDomainSecretForGaia:credential.gaia
                            credential:credential
                               purpose:webauthn::ReauthenticatePurpose::kDecrypt
              userVerificationRequired:passkeyRequestDetails
                                           .userVerificationRequired
                            completion:completion];
}

- (void)userCancelledRequestWithErrorCode:(ASExtensionErrorCode)errorCode {
  [self exitWithErrorCode:errorCode];
}

- (void)completeExtensionConfigurationRequest {
  [self.consentCoordinator stop];
  self.consentCoordinator = nil;
  [self.extensionContext completeExtensionConfigurationRequest];
}

// Returns the gaia ID associated with the current account.
- (NSString*)gaia {
  NSString* gaia = UserDefaultsStringForKey(
      AppGroupUserDefaultsCredentialProviderUserID(), /*default_value=*/@"");
  if (gaia.length > 0) {
    return gaia;
  }

  // As a fallback, attempt to get a valid gaia from existing credentials.
  NSArray<id<Credential>>* credentials = self.credentialStore.credentials;
  NSUInteger credentialIndex =
      [credentials indexOfObjectPassingTest:^BOOL(id<Credential> credential,
                                                  NSUInteger idx, BOOL* stop) {
        return credential.gaia.length > 0;
      }];
  return credentialIndex != NSNotFound ? credentials[credentialIndex].gaia
                                       : nil;
}

// Returns the email address associated with the current account.
- (NSString*)userEmail {
  return UserDefaultsStringForKey(
      AppGroupUserDefaultsCredentialProviderUserEmail(), /*default_value=*/@"");
}

// Returns whether the user is currently using multiple profile in Chrome.
- (BOOL)isUsingMultiProfile {
  return [app_group::GetGroupUserDefaults()
      boolForKey:AppGroupUserDefaultsCredentialProviderMultiProfileSetting()];
}

#pragma mark - PasskeyKeychainProviderBridgeDelegate

- (void)performUserVerificationIfNeeded:(ProceduralBlock)completion {
  if (_userVerificationStatus != PasskeyUserVerificationStatus::kRequired) {
    completion();
    return;
  }

  __weak __typeof(self) weakSelf = self;
  [self
      reauthenticateIfNeededToAccessPasskeys:YES
                       withCompletionHandler:^(ReauthenticationResult result) {
                         if (result != ReauthenticationResult::kFailure) {
                           completion();
                         } else {
                           [weakSelf
                               exitWithErrorCode:ASExtensionErrorCodeFailed];
                         }
                       }];
}

- (void)showEnrollmentWelcomeScreen:(ProceduralBlock)enrollBlock {
  [self createAndPresentPasskeyWelcomeScreenForPurpose:
            PasskeyWelcomeScreenPurpose::kEnroll
                                   primaryButtonAction:enrollBlock];
}

- (void)showFixDegradedRecoverabilityWelcomeScreen:
    (ProceduralBlock)fixDegradedRecoverabilityBlock {
  [self createAndPresentPasskeyWelcomeScreenForPurpose:
            PasskeyWelcomeScreenPurpose::kFixDegradedRecoverability
                                   primaryButtonAction:
                                       fixDegradedRecoverabilityBlock];
}

- (void)showReauthenticationWelcomeScreen:(ProceduralBlock)reauthenticateBlock {
  [self createAndPresentPasskeyWelcomeScreenForPurpose:
            PasskeyWelcomeScreenPurpose::kReauthenticate
                                   primaryButtonAction:reauthenticateBlock];
}

- (void)providerDidCompleteReauthentication {
  _userVerificationStatus = PasskeyUserVerificationStatus::kCompleted;
}

#pragma mark - PasskeyWelcomeScreenViewControllerDelegate

- (void)passkeyWelcomeScreenViewControllerShouldBeDismissed:
    (PasskeyWelcomeScreenViewController*)passkeyWelcomeScreenViewController {
  if (self.passkeyNavigationController.topViewController ==
      passkeyWelcomeScreenViewController) {
    [self.passkeyNavigationController popViewControllerAnimated:YES];
  }
  [self exitWithErrorCode:ASExtensionErrorCodeUserCanceled];
}

#pragma mark - MultiProfilePasskeyCreationViewControllerDelegate

- (void)multiProfilePasskeyCreationViewControllerShouldBeDismissed:
    (MultiProfilePasskeyCreationViewController*)
        multiProfilePasskeyCreationViewController {
  [self exitWithErrorCode:ASExtensionErrorCodeUserCanceled];
}

// Attempts to create a passkey if validation succeeds. Exits with an error code
// otherwise.
- (void)validateUserAndCreatePasskeyWithDetails:
            (PasskeyRequestDetails*)passkeyRequestDetails
                                           gaia:(NSString*)gaia {
  __weak __typeof(self) weakSelf = self;
  [self validateUserWithCompletion:^(BOOL userIsValid) {
    if (!userIsValid) {
      [weakSelf exitWithErrorCode:ASExtensionErrorCodeFailed];
      return;
    }
    [weakSelf createPasskeyWithDetails:passkeyRequestDetails gaia:gaia];
  }];
}

#pragma mark - SuccessfulReauthTimeAccessor

- (void)updateSuccessfulReauthTime {
  self.lastSuccessfulReauthTime = [[NSDate alloc] init];
  UpdateUMACountForKey(app_group::kCredentialExtensionReauthCount);
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  [self exitWithErrorCode:ASExtensionErrorCodeUserCanceled];
}

#pragma mark - Private

// Finishes the extension.
- (void)dismissExtension {
  [self exitWithErrorCode:ASExtensionErrorCodeFailed];
}

// Returns a PasskeyRequestDetails object created from ASCredentialRequest if
// possible. May return nil.
- (PasskeyRequestDetails*)passkeyDetailsFromRequest:
    (id<ASCredentialRequest>)credentialRequest {
  if (!credentialRequest) {
    return nil;
  }

  return [[PasskeyRequestDetails alloc]
                       initWithRequest:credentialRequest
      isBiometricAuthenticationEnabled:[self isBiometricAuthenticationEnabled]
                   isConditionalCreate:NO];
}

// Returns a PasskeyRequestDetails object created from ASCredentialRequest for a
// conditional registration request. May return nil.
- (PasskeyRequestDetails*)passkeyDetailsFromConditionalCreateRequest:
    (id<ASCredentialRequest>)credentialRequest {
  if (!credentialRequest) {
    return nil;
  }

  return [[PasskeyRequestDetails alloc]
                       initWithRequest:credentialRequest
      isBiometricAuthenticationEnabled:[self isBiometricAuthenticationEnabled]
                   isConditionalCreate:YES];
}

// Returns a PasskeyRequestDetails object created from
// ASPasskeyCredentialRequestParameters if possible. May return nil.
- (PasskeyRequestDetails*)passkeyDetailsFromParameters:
    (ASPasskeyCredentialRequestParameters*)requestParameters {
  if (!requestParameters) {
    return nil;
  }

  return [[PasskeyRequestDetails alloc]
                    initWithParameters:requestParameters
      isBiometricAuthenticationEnabled:[self isBiometricAuthenticationEnabled]];
}

- (PasskeyCreationEligibility)passkeyCreationEligibilityForGaia:(NSString*)gaia
                                          passkeyRequestDetails:
                                              (PasskeyRequestDetails*)
                                                  passkeyRequestDetails {
  // Granular policy that allows enterprises to disable just passkey creation.
  std::optional<bool> passkeyCreationPolicy = GetPasskeyCreationPolicy();

  if (!passkeyCreationPolicy) {
    // If the policy isn't set at all, the user has to sign in to Chrome.
    return PasskeyCreationEligibility::kSignedOut;
  } else if (!passkeyCreationPolicy.value()) {
    // If the policy is set to false, the user is not allowed to create
    // passkeys.
    return PasskeyCreationEligibility::kSaveDisabledByEnterprise;
  }

  // Broad policy that allows users or enterprises to disable creation of any
  // credential type.
  if (!IsPasswordCreationUserEnabled()) {
    if (IsPasswordCreationManaged()) {
      return PasskeyCreationEligibility::kSaveDisabledByEnterprise;
    } else {
      return PasskeyCreationEligibility::kSaveDisabledByUser;
    }
  }

  if (!IsPasswordSyncEnabled()) {
    return PasskeyCreationEligibility::kPasswordSyncDisabled;
  }

  if ([gaia length] == 0) {
    return PasskeyCreationEligibility::kSignedOut;
  }

  if (!passkeyRequestDetails.algorithmIsSupported) {
    return PasskeyCreationEligibility::kUnsupportedAlgorithm;
  }

  if ([passkeyRequestDetails
          hasExcludedPasskey:self.credentialStore.credentials]) {
    return PasskeyCreationEligibility::kExcludedPasskey;
  }

  if (passkeyRequestDetails.userVerificationRequired ||
      !IsAutomaticPasskeyUpgradeEnabled() || [self isUsingMultiProfile]) {
    return PasskeyCreationEligibility::kCanCreateWithUserInteraction;
  }

  return PasskeyCreationEligibility::kCanCreate;
}

// Asks user for hardware reauthentication if needed. `forPasskeys` indicates
// whether the reauthentication is guarding an access to passkeys (when `YES`)
// or an access to passwords (when `NO`).
- (void)reauthenticateIfNeededToAccessPasskeys:(BOOL)forPasskeys
                         withCompletionHandler:
                             (void (^)(ReauthenticationResult))
                                 completionHandler {
  __weak __typeof__(self) weakSelf = self;
  auto handlerWrapper = ^(ReauthenticationResult result) {
    if (result == ReauthenticationResult::kSuccess) {
      weakSelf.userVerificationStatus =
          PasskeyUserVerificationStatus::kCompleted;
    }
    completionHandler(result);
  };

  [self.reauthenticationHandler verifyUserToAccessPasskeys:(BOOL)forPasskeys
                                     withCompletionHandler:handlerWrapper
                           presentReminderOnViewController:self];
}

// Returns whether biometric authentication is enabled for the device.
- (BOOL)isBiometricAuthenticationEnabled {
  return [self.reauthenticationModule canAttemptReauthWithBiometrics];
}

- (void)provideCredentialWithoutUserInteractionForIdentifier:
    (NSString*)identifier {
  __weak __typeof__(self) weakSelf = self;
  [self validateUserWithCompletion:^(BOOL userIsValid) {
    // `reauthenticationModule` can't attempt reauth when no passcode is set.
    // This means a password shouldn't be retrieved just yet.
    if (!weakSelf.reauthenticationModule.canAttemptReauth || !userIsValid) {
      [weakSelf exitWithErrorCode:ASExtensionErrorCodeUserInteractionRequired];
      return;
    }
    // iOS already gates the password with device auth for
    // -provideCredentialWithoutUserInteractionForRequest:. Not using
    // `reauthenticationModule` here to avoid a double authentication request.
    [weakSelf provideCredentialForIdentifier:identifier];
  }];
}

- (void)prepareInterfaceToProvideCredentialForIdentifier:(NSString*)identifier {
  __weak __typeof__(self) weakSelf = self;
  [self validateUserWithCompletion:^(BOOL userIsValid) {
    if (!userIsValid) {
      [weakSelf showStaleCredentials];
      return;
    }
    [weakSelf
        reauthenticateIfNeededToAccessPasskeys:NO
                         withCompletionHandler:^(
                             ReauthenticationResult result) {
                           if (result != ReauthenticationResult::kFailure) {
                             [weakSelf
                                 provideCredentialForIdentifier:identifier];
                           } else {
                             [weakSelf exitWithErrorCode:
                                           ASExtensionErrorCodeUserCanceled];
                           }
                         }];
  }];
}

// Completes the extension request providing `ASPasswordCredential` that matches
// the `identifier` or an error if not found.
- (void)provideCredentialForIdentifier:(NSString*)identifier {
  id<Credential> credential =
      [self.credentialStore credentialWithRecordIdentifier:identifier];
  if (credential) {
    UpdateUMACountForKey(app_group::kCredentialExtensionQuickPasswordUseCount);
    ASPasswordCredential* passwordCredential =
        [ASPasswordCredential credentialWithUser:credential.username
                                        password:credential.password];
    [self completeRequestWithSelectedCredential:passwordCredential];
    return;
  }
  [self exitWithErrorCode:ASExtensionErrorCodeCredentialIdentityNotFound];
}

- (void)provideCredentialForRequest:(id<ASCredentialRequest>)credentialRequest {
  NSString* identifier = credentialRequest.credentialIdentity.recordIdentifier;
  if (credentialRequest.type == ASCredentialRequestTypePassword) {
    [self provideCredentialForIdentifier:identifier];
    return;
  }

  if (credentialRequest.type == ASCredentialRequestTypePasskeyAssertion) {
    id<Credential> credential =
        [self.credentialStore credentialWithRecordIdentifier:identifier];
    if (credential) {
      UpdateUMACountForKey(app_group::kCredentialExtensionQuickPasskeyUseCount);

      [self userSelectedPasskey:credential
          passkeyRequestDetails:
              [self passkeyDetailsFromRequest:credentialRequest]];
      return;
    }
  }
  [self exitWithErrorCode:ASExtensionErrorCodeCredentialIdentityNotFound];
}

// Shows a loading indicator,
- (void)showLoadingIndicator {
  DCHECK(!self.activityIndicatorView);
  self.activityIndicatorView = [[UIActivityIndicatorView alloc] init];
  UIActivityIndicatorView* activityView = self.activityIndicatorView;
  activityView.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:activityView];
  [NSLayoutConstraint activateConstraints:@[
    [activityView.centerXAnchor
        constraintEqualToAnchor:self.view.centerXAnchor],
    [activityView.centerYAnchor
        constraintEqualToAnchor:self.view.centerYAnchor],
  ]];
  [activityView startAnimating];
  activityView.color = [UIColor colorNamed:kBlueColor];
}

// Hides the loading indicator.
- (void)hideLoadingIndicator {
  [self.activityIndicatorView removeFromSuperview];
  self.activityIndicatorView = nil;
}

// Verifies that the user is still signed in.
// Return NO in the completion when the user is no longer valid. YES otherwise.
- (void)validateUserWithCompletion:(void (^)(BOOL))completion {
  [self showLoadingIndicator];
  auto handler = ^(BOOL isValid) {
    dispatch_async(dispatch_get_main_queue(), ^{
      [self hideLoadingIndicator];
      if (completion) {
        completion(isValid);
      }
    });
  };

  NSString* validationID = UserDefaultsStringForKey(
      AppGroupUserDefaultsCredentialProviderManagedUserID(),
      /*default_value=*/nil);
  if (validationID) {
    [self.accountVerificator
        validateValidationID:validationID
           completionHandler:^(BOOL isValid, NSError* error) {
             handler(!error && isValid);
           }];
  } else {
    handler(YES);
  }
}

// Presents the stale credentials view controller.
- (void)showStaleCredentials {
  StaleCredentialsViewController* staleCredentialsViewController =
      [[StaleCredentialsViewController alloc] init];
  staleCredentialsViewController.actionHandler = self;
  UINavigationController* navigationController = [[UINavigationController alloc]
      initWithRootViewController:staleCredentialsViewController];
  staleCredentialsViewController.navigationItem.rightBarButtonItem =
      [[UIBarButtonItem alloc]
          initWithBarButtonSystemItem:UIBarButtonSystemItemClose
                               target:self
                               action:@selector(dismissExtension)];
  navigationController.modalPresentationStyle =
      UIModalPresentationOverCurrentContext;
  navigationController.presentationController.delegate = self;
  [self presentViewController:navigationController animated:NO completion:nil];
}

// Starts the credential list feature.
- (void)showCredentialListForServiceIdentifiers:
    (NSArray<ASCredentialServiceIdentifier*>*)serviceIdentifiers {
  // Views in the password creation flow (FormInputAccessoryView) use
  // base::i18n::IsRTL(), which checks some values from the command line.
  // Initialize the command line for the process running this extension here
  // before that.
  if (!base::CommandLine::InitializedForCurrentProcess()) {
    base::CommandLine::Init(0, nullptr);
  }
  self.listCoordinator = [[CredentialListCoordinator alloc]
      initWithBaseViewController:self
                 credentialStore:self.credentialStore
              serviceIdentifiers:serviceIdentifiers
         reauthenticationHandler:self.reauthenticationHandler
       credentialResponseHandler:self];
  self.listCoordinator.passkeyRequestDetails = _passkeyRequestDetails;
  [self.listCoordinator start];
  UpdateUMACountForKey(app_group::kCredentialExtensionDisplayCount);
}

// Convenience wrapper for
// -completeRequestWithSelectedCredential:completionHandler:.
- (void)completeRequestWithSelectedCredential:
    (ASPasswordCredential*)credential {
  [self.listCoordinator stop];
  self.listCoordinator = nil;
  [self.extensionContext completeRequestWithSelectedCredential:credential
                                             completionHandler:nil];
}

// Convenience wrapper for
// -completeAssertionRequestWithSelectedPasskeyCredential:completionHandler:.
- (void)completeAssertionRequestWithSelectedPasskeyCredential:
    (ASPasskeyAssertionCredential*)credential {
  [self.listCoordinator stop];
  self.listCoordinator = nil;
  [self.extensionContext
      completeAssertionRequestWithSelectedPasskeyCredential:credential
                                          completionHandler:nil];
}

// Convenience wrapper for
// -completeRegistrationRequestWithSelectedPasskeyCredential:completionHandler:.
- (void)completeRegistrationRequestWithSelectedPasskeyCredential:
    (ASPasskeyRegistrationCredential*)credential {
  [self.listCoordinator stop];
  self.listCoordinator = nil;
  [self.extensionContext
      completeRegistrationRequestWithSelectedPasskeyCredential:credential
                                             completionHandler:nil];
}

// Convenience wrapper for -cancelRequestWithError.
- (void)exitWithErrorCode:(ASExtensionErrorCode)errorCode {
  [self.listCoordinator stop];
  self.listCoordinator = nil;
  NSError* error = [[NSError alloc] initWithDomain:ASExtensionErrorDomain
                                              code:errorCode
                                          userInfo:nil];
  [self.extensionContext cancelRequestWithError:error];
}

// Displays sheet with information that credential saving is disabled by the
// enterprise policy.
- (void)showSavingDisabledByEnterpriseAlert {
  // TODO(crbug.com/362719658): Check whether it's possible to make the whole
  // VC a half sheet.
  UIViewController* savingEnterpriseDisabledViewController =
      [self createPasskeyErrorAlertForErrorType:
                ErrorType::kEnterpriseDisabledSavingCredentials];
  [self presentViewController:savingEnterpriseDisabledViewController
                     animated:NO
                   completion:nil];
}

// Displays sheet with information that the user is signed out and needs to sign
// in to Chrome.
- (void)showSignedOutUserAlert {
  UIViewController* signedOutUserViewController =
      [self createPasskeyErrorAlertForErrorType:ErrorType::kSignedOut];
  [self presentViewController:signedOutUserViewController
                     animated:NO
                   completion:nil];
}

// Displays sheet with information that credential saving has been manually
// disabled in Password Settings by the user.
- (void)showSavingManuallyDisabledAlert {
  UIViewController* savingDisabledInSettingsViewController =
      [self createPasskeyErrorAlertForErrorType:
                ErrorType::kUserDisabledSavingCredentialsInPasswordSettings];
  [self presentViewController:savingDisabledInSettingsViewController
                     animated:NO
                   completion:nil];
}

// Displays sheet with information that credential saving to account (sync) is
// disabled.
- (void)showSavingToAccountDisabledAlert {
  UIViewController* savingToAccountDisabledViewController =
      [self createPasskeyErrorAlertForErrorType:
                ErrorType::kUserDisabledSavingCredentialsToAccount];
  [self presentViewController:savingToAccountDisabledViewController
                     animated:NO
                   completion:nil];
}

// Displays sheet with a generic error message. Should not be used if a more
// specific error could be shown instead.
- (void)showGenericErrorAlert {
  GenericErrorViewController* genericErrorViewController =
      [[GenericErrorViewController alloc] init];
  genericErrorViewController.actionHandler = self;
  UINavigationController* navigationController = [[UINavigationController alloc]
      initWithRootViewController:genericErrorViewController];

  genericErrorViewController.navigationItem.rightBarButtonItem =
      [[UIBarButtonItem alloc]
          initWithBarButtonSystemItem:UIBarButtonSystemItemClose
                               target:self
                               action:@selector(dismissExtension)];

  navigationController.presentationController.delegate = self;
  [self presentViewController:navigationController animated:YES completion:nil];
}

// Returns the favicon associated with the rpId if it exists.
// Returns nil otherwise.
- (NSString*)faviconForRpId:(NSString*)rpId {
  // Verify if a favicon already exists for the provided rpId.
  NSArray<id<Credential>>* credentials = self.credentialStore.credentials;
  NSUInteger credentialIndex =
      [credentials indexOfObjectPassingTest:^BOOL(id<Credential> credential,
                                                  NSUInteger idx, BOOL* stop) {
        return [credential.rpId isEqualToString:rpId] &&
               credential.favicon.length > 0;
      }];
  return credentialIndex != NSNotFound ? credentials[credentialIndex].favicon
                                       : nil;
}

// Shows a confirmation dialog to the user before performing passkey creation.
- (void)showMultiProfilePasskeyCreationDialogWithDetails:
            (PasskeyRequestDetails*)passkeyRequestDetails
                                                    gaia:(NSString*)gaia {
  NSString* favicon =
      [self faviconForRpId:passkeyRequestDetails.relyingPartyIdentifier];
  MultiProfilePasskeyCreationViewController*
      multiProfilePasskeyCreationViewController =
          [[MultiProfilePasskeyCreationViewController alloc]
                      initWithDetails:passkeyRequestDetails
                                 gaia:gaia
                            userEmail:[self userEmail]
                              favicon:favicon
              navigationItemTitleView:self.passkeyNavigationItemTitleView
                             delegate:self];

  [self.passkeyNavigationController
      pushViewController:multiProfilePasskeyCreationViewController
                animated:NO];
  [self.presentingView presentViewController:self.passkeyNavigationController
                                    animated:NO
                                  completion:nil];
}

// Attempts to create a passkey.
- (void)createPasskeyWithDetails:(PasskeyRequestDetails*)passkeyRequestDetails
                            gaia:(NSString*)gaia
           securityDomainSecrets:(NSArray<NSData*>*)securityDomainSecrets {
  if (!securityDomainSecrets.count) {
    [self exitWithErrorCode:ASExtensionErrorCodeFailed];
    return;
  }

  BOOL didCompleteUserVerification =
      _userVerificationStatus == PasskeyUserVerificationStatus::kCompleted;

  if (passkeyRequestDetails.userVerificationRequired) {
    CHECK(didCompleteUserVerification, base::NotFatalUntil::M144);
  }

  ASPasskeyRegistrationCredential* passkeyRegistrationCredential =
      [passkeyRequestDetails createPasskeyForGaia:gaia
                            securityDomainSecrets:securityDomainSecrets
                      didCompleteUserVerification:didCompleteUserVerification];
  if (passkeyRegistrationCredential) {
    [self completeRegistrationRequestWithSelectedPasskeyCredential:
              passkeyRegistrationCredential];
  } else {
    [self exitWithErrorCode:ASExtensionErrorCodeFailed];
  }
}

// Fetches the security domain secret in order to use it in the passkey creation
// process.
- (void)createPasskeyWithDetails:(PasskeyRequestDetails*)passkeyRequestDetails
                            gaia:(NSString*)gaia {
  __weak __typeof(self) weakSelf = self;
  auto completion = ^(NSArray<NSData*>* securityDomainSecrets) {
    [weakSelf createPasskeyWithDetails:passkeyRequestDetails
                                  gaia:gaia
                 securityDomainSecrets:securityDomainSecrets];
  };

  [self
      fetchSecurityDomainSecretForGaia:gaia
                            credential:nil
                               purpose:webauthn::ReauthenticatePurpose::kEncrypt
              userVerificationRequired:passkeyRequestDetails
                                           .userVerificationRequired
                            completion:completion];
}

// Attempts to perform passkey assertion and retry on failure if allowed.
- (void)
    passkeyAssertionWithCredential:(id<Credential>)credential
             passkeyRequestDetails:(PasskeyRequestDetails*)passkeyRequestDetails
             securityDomainSecrets:(NSArray<NSData*>*)securityDomainSecrets {
  if (!securityDomainSecrets.count) {
    [self exitWithErrorCode:ASExtensionErrorCodeFailed];
    return;
  }

  BOOL didCompleteUserVerification =
      _userVerificationStatus == PasskeyUserVerificationStatus::kCompleted;

  if (passkeyRequestDetails.userVerificationRequired) {
    CHECK(didCompleteUserVerification, base::NotFatalUntil::M144);
  }

  ASPasskeyAssertionCredential* passkeyCredential = [passkeyRequestDetails
          assertPasskeyCredential:credential
            securityDomainSecrets:securityDomainSecrets
      didCompleteUserVerification:didCompleteUserVerification];
  [self userSelectedPasskey:passkeyCredential];
}

// Triggers the process to fetch the security domain secret and calls the
// completion block with the security domain secret as input.
// "credential" will be used to validate the security domain secret.
- (void)
    fetchSecurityDomainSecretForGaia:(NSString*)gaia
                          credential:(id<Credential>)credential
                             purpose:(webauthn::ReauthenticatePurpose)purpose
            userVerificationRequired:(BOOL)userVerificationRequired
                          completion:(FetchSecurityDomainSecretCompletionBlock)
                                         completion {
  // Store `userVerificationRequired` here as it will be needed at a later stage
  // in the process of fetching the security domain secret.
  if (userVerificationRequired) {
    _userVerificationStatus = PasskeyUserVerificationStatus::kRequired;
    // Since UV is required, do not allow a previous reauth to be reused.
    self.lastSuccessfulReauthTime = nil;
  } else {
    _userVerificationStatus = PasskeyUserVerificationStatus::kNotRequired;
  }

  [self.passkeyKeychainProviderBridge
      fetchSecurityDomainSecretForGaia:gaia
                            credential:credential
                               purpose:purpose
                            completion:completion];
}

- (BOOL)metricsAreEnabled {
  // If metrics are enabled, the client ID must be set.
  // If it is not set, metrics are disabled.
  return [app_group::GetGroupUserDefaults()
             objectForKey:@(app_group::kChromeAppClientID)] != nil;
}

// Creates and configures the navigation bar for this view controller.
- (UINavigationBar*)createNavigationBar {
  UINavigationItem* navigationItem = [[UINavigationItem alloc] init];
  navigationItem.titleView = self.passkeyNavigationItemTitleView;

  UINavigationBar* navigationBar = [[UINavigationBar alloc] init];
  navigationBar.translatesAutoresizingMaskIntoConstraints = NO;
  navigationBar.translucent = NO;
  [navigationBar setShadowImage:[[UIImage alloc] init]];
  [navigationBar setBarTintColor:BackgroundColor()];
  [navigationBar setItems:@[ navigationItem ]];

  return navigationBar;
}

// Returns the view currently being presented, which should therefore be the new
// view to present the next one.
- (UIViewController*)presentingView {
  return self.presentedViewController ? self.presentedViewController : self;
}

// Creates and configures a PasskeyErrorAlertViewController for the given
// `errorType`.
- (UIViewController*)createPasskeyErrorAlertForErrorType:(ErrorType)errorType {
  PasskeyErrorAlertViewController* passkeyErrorAlertViewController =
      [[PasskeyErrorAlertViewController alloc] initForErrorType:errorType];
  passkeyErrorAlertViewController.actionHandler = self;
  UINavigationController* navigationController = [[UINavigationController alloc]
      initWithRootViewController:passkeyErrorAlertViewController];
  navigationController.presentationController.delegate = self;

  passkeyErrorAlertViewController.navigationItem.rightBarButtonItem =
      [[UIBarButtonItem alloc]
          initWithBarButtonSystemItem:UIBarButtonSystemItemClose
                               target:self
                               action:@selector(dismissExtension)];

  return navigationController;
}

// Creates and presents a PasskeyWelcomeScreenViewController.
- (void)createAndPresentPasskeyWelcomeScreenForPurpose:
            (PasskeyWelcomeScreenPurpose)purpose
                                   primaryButtonAction:
                                       (ProceduralBlock)primaryButtonAction {
  // Early return if the `passkeyNavigationController` is already visible. This
  // means that a passkey welcome screen is already presented and a new one
  // shouldn't be shown. Hitting this early return is most likely a result of
  // the user tapping a button multiple times, triggering the creation of
  // multiple simultaneous passkey requests. See crbug.com/377712051.
  if (self.passkeyNavigationController.visibleViewController) {
    return;
  }

  if (!self.presentingView.view.window) {
    // If the view of the presenting view controller doesn't have any window, it
    // means that it is currently not visible and that the CPE hasn't been
    // prepared to show some UI.
    [self exitWithErrorCode:ASExtensionErrorCodeUserInteractionRequired];
    return;
  }

  ProceduralBlock action;
  // With the `kReauthenticate` purpose, the user will be asked to enter their
  // Google Password Manager PIN, so no need to also do a device
  // reauthentication before showing the UI.
  if (purpose != PasskeyWelcomeScreenPurpose::kReauthenticate &&
      _userVerificationStatus == PasskeyUserVerificationStatus::kRequired) {
    __weak __typeof(self) weakSelf = self;
    action = ^{
      [weakSelf
          reauthenticateIfNeededToAccessPasskeys:YES
                           withCompletionHandler:^(
                               ReauthenticationResult result) {
                             if (result != ReauthenticationResult::kFailure) {
                               primaryButtonAction();
                             } else {
                               [weakSelf exitWithErrorCode:
                                             ASExtensionErrorCodeFailed];
                             }
                           }];
    };
  } else {
    action = primaryButtonAction;
  }

  NSString* userEmail;
  if (purpose == PasskeyWelcomeScreenPurpose::kEnroll) {
    userEmail = [self userEmail];
    if (!userEmail.length) {
      [self showGenericErrorAlert];
      return;
    }
  }

  PasskeyWelcomeScreenViewController* welcomeScreen =
      [[PasskeyWelcomeScreenViewController alloc]
                   initForPurpose:purpose
          navigationItemTitleView:self.passkeyNavigationItemTitleView
                         delegate:self
              primaryButtonAction:action
                          strings:GetPasskeyWelcomeScreenStrings(purpose,
                                                                 userEmail)];
  [self.passkeyNavigationController pushViewController:welcomeScreen
                                              animated:NO];
  [self.presentingView presentViewController:self.passkeyNavigationController
                                    animated:NO
                                  completion:nil];
}

// Starts the `consentCoordinator` to present the ConsentViewController.
- (void)presentConsentViewController {
  self.consentCoordinator =
      [[ConsentCoordinator alloc] initWithBaseViewController:self
                                   credentialResponseHandler:self];
  [self.consentCoordinator start];
}

// Completes the security domain secret fetch that happens when enabling the app
// as a credential provider in iOS Settings. Dismisses the
// `passkeyNavigationController` if presented for passkey bootstrapping purposes
// during the fetching process. Otherwise, presents the ConsentViewController.
- (void)completeSecurityDomainSecretFetchForExtensionConfiguration {
  // If the `passkeyNavigationController` has a `visibleViewController`, it
  // means that the bootstrapping UI has been presented to the user through the
  // security domain secret fetch (see
  // `-prepareInterfaceForExtensionConfiguration`). In this case, all that's
  // left to do is dismiss the bootstrapping UI. Otherwise, it means that the
  // bootstrapping UI hasn't been shown, hence the ConsentViewController needs
  // to be presented.
  if (self.passkeyNavigationController.visibleViewController) {
    [self.passkeyNavigationController.presentingViewController
        dismissViewControllerAnimated:YES
                           completion:nil];
    [self.extensionContext completeExtensionConfigurationRequest];
  } else {
    [self presentConsentViewController];
  }
}

@end
