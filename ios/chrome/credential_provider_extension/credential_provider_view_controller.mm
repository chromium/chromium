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
#import "ios/chrome/common/app_group/app_group_constants.h"
#import "ios/chrome/common/app_group/app_group_metrics.h"
#import "ios/chrome/common/crash_report/crash_helper.h"
#import "ios/chrome/common/credential_provider/archivable_credential_store.h"
#import "ios/chrome/common/credential_provider/constants.h"
#import "ios/chrome/common/credential_provider/credential.h"
#import "ios/chrome/common/credential_provider/multi_store_credential_store.h"
#import "ios/chrome/common/credential_provider/user_defaults_credential_store.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"
#import "ios/chrome/common/ui/reauthentication/reauthentication_module.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/credential_provider_extension/account_verification_provider.h"
#import "ios/chrome/credential_provider_extension/font_provider.h"
#import "ios/chrome/credential_provider_extension/metrics_util.h"
#import "ios/chrome/credential_provider_extension/passkey_keychain_provider_bridge.h"
#import "ios/chrome/credential_provider_extension/passkey_util.h"
#import "ios/chrome/credential_provider_extension/reauthentication_handler.h"
#import "ios/chrome/credential_provider_extension/ui/consent_coordinator.h"
#import "ios/chrome/credential_provider_extension/ui/create_navigation_item_title_view.h"
#import "ios/chrome/credential_provider_extension/ui/credential_list_coordinator.h"
#import "ios/chrome/credential_provider_extension/ui/credential_response_handler.h"
#import "ios/chrome/credential_provider_extension/ui/feature_flags.h"
#import "ios/chrome/credential_provider_extension/ui/generic_error_view_controller.h"
#import "ios/chrome/credential_provider_extension/ui/passkey_welcome_screen_view_controller.h"
#import "ios/chrome/credential_provider_extension/ui/saving_enterprise_disabled_view_controller.h"
#import "ios/chrome/credential_provider_extension/ui/signed_out_user_view_controller.h"
#import "ios/chrome/credential_provider_extension/ui/stale_credentials_view_controller.h"
#import "ios/components/credential_provider_extension/password_util.h"

using credential_provider_extension::AccountInfo;

namespace {
UIColor* BackgroundColor() {
  return [UIColor colorNamed:kGroupedPrimaryBackgroundColor];
}
}

@interface CredentialProviderViewController () <
    ConfirmationAlertActionHandler,
    CredentialResponseHandler,
    PasskeyKeychainProviderBridgeDelegate,
    PasskeyWelcomeScreenViewControllerDelegate,
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

// Identfiers cached in `-prepareCredentialListForServiceIdentifiers:` to show
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

@end

@implementation CredentialProviderViewController {
  // Information about a passkey credential request.
  ASPasskeyCredentialRequestParameters* _requestParameters
      API_AVAILABLE(ios(17.0));

  // Stores whether or not user verification should be performed for passkey
  // creation or assertion.
  BOOL _userVerificationRequired;

  // Stores whether or not the credential list is about to be shown for
  // passkeys. Used to determine which reauthentication reason to provide to the
  // user.
  BOOL _nextCredentialListIsForPasskeys;
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
    BOOL isAccessingPasskeys = _nextCredentialListIsForPasskeys;
    _nextCredentialListIsForPasskeys = NO;

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
  _nextCredentialListIsForPasskeys = NO;
}

// Only available in iOS 17.0+.
// The system calls this method when there’s an active passkey request in the
// app or website.
- (void)prepareCredentialListForServiceIdentifiers:
            (NSArray<ASCredentialServiceIdentifier*>*)serviceIdentifiers
                                 requestParameters:
                                     (ASPasskeyCredentialRequestParameters*)
                                         requestParameters
    API_AVAILABLE(ios(17.0)) {
  self.serviceIdentifiers = serviceIdentifiers;
  _requestParameters = requestParameters;
  _nextCredentialListIsForPasskeys = _requestParameters != nil;
}

// Deprecated in iOS 17.0+.
// Replaced with provideCredentialWithoutUserInteractionForRequest.
- (void)provideCredentialWithoutUserInteractionForIdentity:
    (ASPasswordCredentialIdentity*)credentialIdentity {
  if (@available(iOS 17.0, *)) {
    return;
  }

  [self provideCredentialWithoutUserInteractionForIdentifier:
            credentialIdentity.recordIdentifier];
}

// Only available in iOS 17.0+.
- (void)provideCredentialWithoutUserInteractionForRequest:
    (id<ASCredentialRequest>)credentialRequest API_AVAILABLE(ios(17.0)) {
  if (credentialRequest.type == ASCredentialRequestTypePasskeyAssertion) {
    // Unlike passwords, iOS doesn't already gate passkeys with device auth. If
    // the credential request is for a passkey, first evaluate if a device auth
    // is needed or not. If auth is needed, then the "with user interaction"
    // path needs to be taken.
    ASPasskeyCredentialRequest* passkeyCredentialRequest =
        base::apple::ObjCCastStrict<ASPasskeyCredentialRequest>(
            credentialRequest);
    if ([self shouldPerformUserVerificationForPreference:
                  passkeyCredentialRequest.userVerificationPreference]) {
      [self exitWithErrorCode:ASExtensionErrorCodeUserInteractionRequired];
      return;
    }
  }

  __weak __typeof__(self) weakSelf = self;
  [self validateUserWithCompletion:^(BOOL userIsValid) {
    // `reauthenticationModule` can't attempt reauth when no passscode is set.
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

// Deprecated in iOS 17.0+.
// Replaced with prepareInterfaceToProvideCredentialForRequest.
- (void)prepareInterfaceToProvideCredentialForIdentity:
    (ASPasswordCredentialIdentity*)credentialIdentity {
  if (@available(iOS 17.0, *)) {
    return;
  }

  [self prepareInterfaceToProvideCredentialForIdentifier:credentialIdentity
                                                             .recordIdentifier];
}

// Only available in iOS 17.0+.
- (void)prepareInterfaceToProvideCredentialForRequest:
    (id<ASCredentialRequest>)credentialRequest API_AVAILABLE(ios(17.0)) {
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
  self.consentCoordinator =
      [[ConsentCoordinator alloc] initWithBaseViewController:self
                                   credentialResponseHandler:self];
  [self.consentCoordinator start];
}

// Only available in iOS 18.0+.
- (void)performPasskeyRegistrationWithoutUserInteractionIfPossible:
    (ASPasskeyCredentialRequest*)registrationRequest API_AVAILABLE(ios(18.0)) {
  // This function is called to silently create passkeys.
  // We're always allowed to return an error until we support this flow.
  [self exitWithErrorCode:ASExtensionErrorCodeFailed];
}

- (void)prepareInterfaceForPasskeyRegistration:
    (id<ASCredentialRequest>)registrationRequest {
  if (![registrationRequest isKindOfClass:[ASPasskeyCredentialRequest class]]) {
    [self exitWithErrorCode:ASExtensionErrorCodeFailed];
    return;
  }

  if (!IsPasswordCreationUserEnabled()) {
    if (IsPasswordCreationManaged()) {
      [self showSavingDisabledByEnterpriseAlert];
    } else {
      // TODO(crbug.com/379247744): Replace this generic error with a more
      // appropriate one. Users in this state have manually disabled the "Offer
      // to save passwords" option, and need to enable it to complete saving a
      // passkey.
      [self showGenericErrorAlert];
    }
    return;
  }

  if (!IsPasswordSyncEnabled()) {
    // TODO(crbug.com/379247744): Replace this generic error with a more
    // appropriate one. Users in this state have disabled saving passwords to
    // their account (sync).
    [self showGenericErrorAlert];
    return;
  }

  NSString* gaia = [self gaia];
  if ([gaia length] == 0) {
    // If we don't have a gaia, either the user is signed out of Chrome or has
    // never opened Chrome. Passkeys require the user to be signed in to Chrome.
    [self showSignedOutUserAlert];
    return;
  }

  __weak __typeof__(self) weakSelf = self;
  [self validateUserWithCompletion:^(BOOL userIsValid) {
    if (!userIsValid) {
      [weakSelf exitWithErrorCode:ASExtensionErrorCodeFailed];
      return;
    }
    [weakSelf createPasskeyForRequest:registrationRequest gaia:gaia];
  }];
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

- (void)confirmationAlertDismissAction {
  // Finish the extension. There is no recovery from the stale credentials
  // state.
  [self exitWithErrorCode:ASExtensionErrorCodeFailed];
}

- (void)confirmationAlertPrimaryAction {
  // No-op.
}

#pragma mark - CredentialResponseHandler

- (void)userSelectedPassword:(ASPasswordCredential*)credential {
  [self completeRequestWithSelectedCredential:credential];
}

- (void)userSelectedPasskey:(ASPasskeyAssertionCredential*)credential
    API_AVAILABLE(ios(17.0)) {
  if (credential) {
    [self completeAssertionRequestWithSelectedPasskeyCredential:credential];
  } else {
    [self exitWithErrorCode:ASExtensionErrorCodeCredentialIdentityNotFound];
  }
}

- (void)userSelectedPasskey:(id<Credential>)credential
              clientDataHash:(NSData*)clientDataHash
          allowedCredentials:(NSArray<NSData*>*)allowedCredentials
    userVerificationRequired:(BOOL)userVerificationRequired
    API_AVAILABLE(ios(17.0)) {
  __weak __typeof(self) weakSelf = self;
  auto completion = ^(NSArray<NSData*>* securityDomainSecrets) {
    [weakSelf passkeyAssertionWithCredential:credential
                              clientDataHash:clientDataHash
                          allowedCredentials:allowedCredentials
                       securityDomainSecrets:securityDomainSecrets];
  };

  [self fetchSecurityDomainSecretForGaia:credential.gaia
                              credential:credential
                                 purpose:PasskeyKeychainProvider::
                                             ReauthenticatePurpose::kDecrypt
                userVerificationRequired:userVerificationRequired
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

// Loads and returns the account information (gaia and email) from the Keychain
// Services.
- (AccountInfo)accountInfo {
  return credential_provider_extension::LoadAccountInfoFromKeychain();
}

// Returns the gaia ID associated with the current account.
- (NSString*)gaia {
  NSString* gaia = [self accountInfo].gaia;
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
  return [self accountInfo].email;
}

#pragma mark - PasskeyKeychainProviderBridgeDelegate

- (void)performUserVerificationIfNeeded:(ProceduralBlock)completion {
  if (!_userVerificationRequired) {
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

#pragma mark - PasskeyWelcomeScreenViewControllerDelegate

- (void)passkeyWelcomeScreenViewControllerShouldBeDismissed:
    (id)passkeyWelcomeScreenViewController {
  if (self.passkeyNavigationController.topViewController ==
      passkeyWelcomeScreenViewController) {
    [self.passkeyNavigationController popViewControllerAnimated:YES];
  }
  [self exitWithErrorCode:ASExtensionErrorCodeUserCanceled];
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

// Asks user for hardware reauthentication if needed. `forPasskeys` indicates
// whether the reauthentication is guarding an access to passkeys (when `YES`)
// or an access to passwords (when `NO`).
- (void)reauthenticateIfNeededToAccessPasskeys:(BOOL)forPasskeys
                         withCompletionHandler:
                             (void (^)(ReauthenticationResult))
                                 completionHandler {
  [self.reauthenticationHandler verifyUserToAccessPasskeys:(BOOL)forPasskeys
                                     withCompletionHandler:completionHandler
                           presentReminderOnViewController:self];
}

// Returns whether or not the user should be asked to re-authenticate depending
// on the provided `userVerificationPreference`.
- (BOOL)shouldPerformUserVerificationForPreference:
    (NSString*)userVerificationPreference {
  return ShouldPerformUserVerificationForPreference(
      userVerificationPreference,
      [self.reauthenticationModule canAttemptReauthWithBiometrics]);
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

- (void)provideCredentialForRequest:(id<ASCredentialRequest>)credentialRequest
    API_AVAILABLE(ios(17.0)) {
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
      ASPasskeyCredentialRequest* passkeyCredentialRequest =
          base::apple::ObjCCastStrict<ASPasskeyCredentialRequest>(
              credentialRequest);

      [self userSelectedPasskey:credential
                    clientDataHash:passkeyCredentialRequest.clientDataHash
                allowedCredentials:nil
          userVerificationRequired:
              [self shouldPerformUserVerificationForPreference:
                        passkeyCredentialRequest.userVerificationPreference]];
      return;
    }
  }
  [self exitWithErrorCode:ASExtensionErrorCodeCredentialIdentityNotFound];
}

// Creates a passkey for the provided gaia ID.
- (void)createPasskeyForRequest:(id<ASCredentialRequest>)registrationRequest
                           gaia:(NSString*)gaia API_AVAILABLE(ios(17.0)) {
  ASPasskeyCredentialRequest* passkeyCredentialRequest =
      base::apple::ObjCCastStrict<ASPasskeyCredentialRequest>(
          registrationRequest);

  NSArray<NSNumber*>* supportedAlgorithms =
      [passkeyCredentialRequest.supportedAlgorithms
          filteredArrayUsingPredicate:[NSPredicate predicateWithBlock:^BOOL(
                                                       NSNumber* algorithm,
                                                       NSDictionary* bindings) {
            return webauthn::passkey_model_utils::IsSupportedAlgorithm(
                algorithm.intValue);
          }]];

  if (supportedAlgorithms.count == 0) {
    [self exitWithErrorCode:ASExtensionErrorCodeFailed];
    return;
  }

  ASPasskeyCredentialIdentity* identity =
      base::apple::ObjCCastStrict<ASPasskeyCredentialIdentity>(
          passkeyCredentialRequest.credentialIdentity);

  [self createPasskeyForClient:passkeyCredentialRequest.clientDataHash
        relyingPartyIdentifier:identity.relyingPartyIdentifier
                      username:identity.userName
                    userHandle:identity.userHandle
                          gaia:gaia
      userVerificationRequired:[self shouldPerformUserVerificationForPreference:
                                         passkeyCredentialRequest
                                             .userVerificationPreference]];
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

  NSString* validationID = [app_group::GetGroupUserDefaults()
      stringForKey:AppGroupUserDefaultsCredentialProviderUserID()];
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
  staleCredentialsViewController.modalPresentationStyle =
      UIModalPresentationOverCurrentContext;
  staleCredentialsViewController.actionHandler = self;
  staleCredentialsViewController.presentationController.delegate = self;
  [self presentViewController:staleCredentialsViewController
                     animated:YES
                   completion:nil];
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
  if (@available(iOS 17.0, *)) {
    self.listCoordinator.requestParameters = _requestParameters;
  }
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
    (ASPasskeyAssertionCredential*)credential API_AVAILABLE(ios(17.0)) {
  [self.listCoordinator stop];
  self.listCoordinator = nil;
  [self.extensionContext
      completeAssertionRequestWithSelectedPasskeyCredential:credential
                                          completionHandler:nil];
}

// Convenience wrapper for
// -completeRegistrationRequestWithSelectedPasskeyCredential:completionHandler:.
- (void)completeRegistrationRequestWithSelectedPasskeyCredential:
    (ASPasskeyRegistrationCredential*)credential API_AVAILABLE(ios(17.0)) {
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
  SavingEnterpriseDisabledViewController*
      savingEnterpriseDisabledViewController =
          [[SavingEnterpriseDisabledViewController alloc] init];
  savingEnterpriseDisabledViewController.actionHandler = self;
  savingEnterpriseDisabledViewController.presentationController.delegate = self;
  [self presentViewController:savingEnterpriseDisabledViewController
                     animated:YES
                   completion:nil];
}

// Displays sheet with information that the user is signed out ans needs to sign
// in to Chrome.
- (void)showSignedOutUserAlert {
  SignedOutUserViewController* signedOutUserViewController =
      [[SignedOutUserViewController alloc] init];
  signedOutUserViewController.actionHandler = self;
  signedOutUserViewController.presentationController.delegate = self;
  [self presentViewController:signedOutUserViewController
                     animated:YES
                   completion:nil];
}

// Displays sheet with a generic error message. Should not be used if a more
// specific error could be shown instead.
- (void)showGenericErrorAlert {
  GenericErrorViewController* genericErrorViewController =
      [[GenericErrorViewController alloc] init];
  genericErrorViewController.actionHandler = self;
  genericErrorViewController.presentationController.delegate = self;
  [self presentViewController:genericErrorViewController
                     animated:YES
                   completion:nil];
}

// Attempts to create a passkey.
- (void)createPasskeyForClient:(NSData*)clientDataHash
        relyingPartyIdentifier:(NSString*)relyingPartyIdentifier
                      username:(NSString*)username
                    userHandle:(NSData*)userHandle
                          gaia:(NSString*)gaia
         securityDomainSecrets:(NSArray<NSData*>*)securityDomainSecrets
    API_AVAILABLE(ios(17.0)) {
  ASPasskeyRegistrationCredential* passkeyRegistrationCredential =
      PerformPasskeyCreation(clientDataHash, relyingPartyIdentifier, username,
                             userHandle, gaia, securityDomainSecrets);
  if (passkeyRegistrationCredential) {
    [self completeRegistrationRequestWithSelectedPasskeyCredential:
              passkeyRegistrationCredential];
  } else {
    [self exitWithErrorCode:ASExtensionErrorCodeFailed];
  }
}

// Fetches the security domain secret in order to use it in the passkey creation
// process.
- (void)createPasskeyForClient:(NSData*)clientDataHash
        relyingPartyIdentifier:(NSString*)relyingPartyIdentifier
                      username:(NSString*)username
                    userHandle:(NSData*)userHandle
                          gaia:(NSString*)gaia
      userVerificationRequired:(BOOL)userVerificationRequired
    API_AVAILABLE(ios(17.0)) {
  __weak __typeof(self) weakSelf = self;
  auto completion = ^(NSArray<NSData*>* securityDomainSecrets) {
    [weakSelf createPasskeyForClient:clientDataHash
              relyingPartyIdentifier:relyingPartyIdentifier
                            username:username
                          userHandle:userHandle
                                gaia:gaia
               securityDomainSecrets:securityDomainSecrets];
  };

  [self fetchSecurityDomainSecretForGaia:gaia
                              credential:nil
                                 purpose:PasskeyKeychainProvider::
                                             ReauthenticatePurpose::kEncrypt
                userVerificationRequired:userVerificationRequired
                              completion:completion];
}

// Attempts to perform passkey assertion and retry on failure if allowed.
- (void)passkeyAssertionWithCredential:(id<Credential>)credential
                        clientDataHash:(NSData*)clientDataHash
                    allowedCredentials:(NSArray<NSData*>*)allowedCredentials
                 securityDomainSecrets:(NSArray<NSData*>*)securityDomainSecrets
    API_AVAILABLE(ios(17.0)) {
  ASPasskeyAssertionCredential* passkeyCredential = PerformPasskeyAssertion(
      credential, clientDataHash, allowedCredentials, securityDomainSecrets);
  [self userSelectedPasskey:passkeyCredential];
}

// Triggers the process to fetch the security domain secret and calls the
// completion block with the security domain secret as input.
// "credential" will be used to validate the security domain secret.
- (void)fetchSecurityDomainSecretForGaia:(NSString*)gaia
                              credential:(id<Credential>)credential
                                 purpose:(PasskeyKeychainProvider::
                                              ReauthenticatePurpose)purpose
                userVerificationRequired:(BOOL)userVerificationRequired
                              completion:
                                  (FetchSecurityDomainSecretCompletionBlock)
                                      completion {
  // Store `userVerificationRequired` here as it will be needed at a later stage
  // in the process of fetching the security domain secret.
  _userVerificationRequired = userVerificationRequired;
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

// Creates and presents a PasskeyWelcomeScreenViewController.
- (void)createAndPresentPasskeyWelcomeScreenForPurpose:
            (PasskeyWelcomeScreenPurpose)purpose
                                   primaryButtonAction:
                                       (ProceduralBlock)primaryButtonAction {
  // Early return if the `passkeyNavigationController` is already visible. This
  // early return shouldn't be triggered, but was added to monitor a crash fix.
  // This check should be removed if no crash reports are observed. See
  // crbug.com/377712051.
  if (self.passkeyNavigationController.visibleViewController) {
    NOTREACHED(base::NotFatalUntil::M135);
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
  // Google Passowrd Manager PIN, so no need to also do a device
  // reauthentication before showing the UI.
  if (purpose != PasskeyWelcomeScreenPurpose::kReauthenticate &&
      _userVerificationRequired) {
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
  // Now that the need to perform a device reauthentication has been evaluated
  // and handled, set `_userVerificationRequired` to `NO` so that the user won't
  // be asked to reauthenticate at a later time in the process of handling the
  // passkey request.
  _userVerificationRequired = NO;

  NSString* userEmail;
  if (purpose == PasskeyWelcomeScreenPurpose::kEnroll) {
    userEmail = [self userEmail];
    if (!userEmail) {
      // TODO(crbug.com/381284523): When on M135, show generic alert screen
      // instead.
      [self showSignedOutUserAlert];
      return;
    }
  }

  PasskeyWelcomeScreenViewController* welcomeScreen =
      [[PasskeyWelcomeScreenViewController alloc]
                   initForPurpose:purpose
          navigationItemTitleView:self.passkeyNavigationItemTitleView
                        userEmail:userEmail
                         delegate:self
              primaryButtonAction:action];
  [self.passkeyNavigationController pushViewController:welcomeScreen
                                              animated:NO];
  [self.presentingView presentViewController:self.passkeyNavigationController
                                    animated:NO
                                  completion:nil];
}

@end
