// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/credential_provider_extension/credential_provider_view_controller.h"

#import <AuthenticationServices/AuthenticationServices.h>
#import <UIKit/UIKit.h>

#import <memory>
#import <string>
#import <string_view>
#import <utility>
#import <vector>

#import "base/apple/foundation_util.h"
#import "base/functional/callback.h"
#import "base/functional/callback_helpers.h"
#import "base/memory/raw_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/task_environment.h"
#import "base/test/test_future.h"
#import "base/time/time.h"
#import "components/password_manager/core/common/browser_assisted_login_type.h"
#import "components/sync/protocol/webauthn_credential_specifics.pb.h"
#import "components/webauthn/core/browser/passkey_model_utils.h"
#import "components/webauthn/ios/passkey_types.h"
#import "ios/chrome/common/app_group/app_group_constants.h"
#import "ios/chrome/common/app_group/app_group_metrics.h"
#import "ios/chrome/common/credential_provider/ASPasskeyCredentialIdentity+credential.h"
#import "ios/chrome/common/credential_provider/ASPasswordCredentialIdentity+credential.h"
#import "ios/chrome/common/credential_provider/archivable_credential+passkey.h"
#import "ios/chrome/common/credential_provider/archivable_credential.h"
#import "ios/chrome/common/credential_provider/archivable_credential_store.h"
#import "ios/chrome/common/credential_provider/constants.h"
#import "ios/chrome/common/credential_provider/multi_store_credential_store.h"
#import "ios/chrome/common/credential_provider/passkey_keychain_provider.h"
#import "ios/chrome/common/credential_provider/passkey_keychain_provider_bridge.h"
#import "ios/chrome/common/credential_provider/user_defaults_credential_store.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"
#import "ios/chrome/common/ui/promo_style/promo_style_view_controller_delegate.h"
#import "ios/chrome/common/ui/reauthentication/mock_reauthentication_module.h"
#import "ios/chrome/credential_provider_extension/account_verification_provider.h"
#import "ios/chrome/credential_provider_extension/credential_provider_view_controller+Testing.h"
#import "ios/chrome/credential_provider_extension/generated_localized_strings.h"
#import "ios/chrome/credential_provider_extension/passkey_request_details.h"
#import "ios/chrome/credential_provider_extension/ui/consent_view_controller.h"
#import "ios/chrome/credential_provider_extension/ui/credential_list_view_controller.h"
#import "ios/chrome/credential_provider_extension/ui/multi_profile_passkey_creation_view_controller.h"
#import "ios/chrome/credential_provider_extension/ui/passkey_error_alert_view_controller.h"
#import "ios/chrome/credential_provider_extension/ui/stale_credentials_view_controller.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

// Fake implementation of `AccountVerificationProvider` for testing user
// validation.
@interface FakeAccountVerificationProvider
    : NSObject <AccountVerificationProvider>

// Whether the validated account should be considered valid.
@property(nonatomic, assign) BOOL isValid;

@end

@implementation FakeAccountVerificationProvider

- (void)validationIDForAccountID:(NSString*)accountID
               completionHandler:
                   (void (^)(NSString*, NSError*))completionHandler {
  if (completionHandler) {
    completionHandler(accountID, nil);
  }
}

- (void)validateValidationID:(NSString*)validationID
           completionHandler:(void (^)(BOOL, NSError*))handler {
  if (handler) {
    handler(self.isValid, nil);
  }
}

@end

// Subclass of `CredentialProviderViewController` that exposes a controllable
// `extensionContext` and executes view controller presentations synchronously
// without animation in unit tests.
@interface TestCredentialProviderViewController
    : CredentialProviderViewController

// Mock `ASCredentialProviderExtensionContext` returned by `extensionContext`.
@property(nonatomic, strong) id mockExtensionContext;

// Optional callback invoked when `presentViewController:animated:completion:`
// is called.
@property(nonatomic, copy) void (^onPresentViewController)(UIViewController*);

// Whether `dismissViewControllerAnimated:completion:` was called.
@property(nonatomic, assign) BOOL didDismissViewController;

@end

@implementation TestCredentialProviderViewController

- (ASCredentialProviderExtensionContext*)extensionContext {
  return self.mockExtensionContext;
}

- (void)presentViewController:(UIViewController*)viewControllerToPresent
                     animated:(BOOL)flag
                   completion:(void (^)(void))completion {
  [super presentViewController:viewControllerToPresent
                      animated:NO
                    completion:completion];
  if (self.onPresentViewController) {
    self.onPresentViewController(viewControllerToPresent);
  }
}

- (void)dismissViewControllerAnimated:(BOOL)flag
                           completion:(void (^)(void))completion {
  self.didDismissViewController = YES;
  [super dismissViewControllerAnimated:NO completion:completion];
}

@end

namespace {

using ::app_group::GetGroupUserDefaults;
using ::app_group::HistogramCountKey;
using ::base::SysNSStringToUTF8;
using ::base::Time;
using ::base::apple::ObjCCast;
using ::base::test::TestFuture;
using ::password_manager::metrics_util::BrowserAssistedLoginType;
using ::webauthn::SharedKeyList;

constexpr int kPasskeyBucket = static_cast<int>(
    BrowserAssistedLoginType::kPasskeyStoredInGPMFacilitatedThroughIOSUI);
constexpr NSInteger kSupportedAlgorithm = -7;
constexpr NSInteger kUnsupportedAlgorithm = 0;
constexpr size_t kWindowSize = 500;

NSString* const kTestUserDefaultsKey = @"UserDefaultsCredentialStoreTestKey";
NSString* const kTestRPId = @"example.com";
NSString* const kTestServiceIdentifier = @"https://example.com";
NSString* const kTestUsername = @"user";
NSString* const kTestNewUsername = @"newUser";
NSString* const kTestPassword = @"password123";
NSString* const kTestRecordIdentifier = @"recordIdentifier";
NSString* const kTestPasswordRecordIdentifier = @"passwordRecordIdentifier";
NSString* const kTestFavicon = @"favicon";
NSString* const kTestUserDisplayName = @"userDisplayName";
NSString* const kTestGaia = @"gaia123";
NSString* const kTestFallbackGaia = @"fallbackGaia456";
NSString* const kTestEmail = @"user@example.com";
NSString* const kTestManagedUserId = @"managedUser123";
NSString* const kBrowserAssistedLoginHistogram =
    @"PasswordManager.BrowserAssistedLogin.Type";

constexpr std::string_view kTestUserId = "userId";
constexpr std::string_view kTestCredentialId = "credentialId";
constexpr std::string_view kTestSyncId = "syncId";
constexpr std::string_view kTestPrivateKey = "privateKey";
constexpr std::string_view kTestSignature = "sig";
constexpr std::string_view kTestClientDataHash = "hash";
constexpr std::string_view kTestAuthenticatorData = "auth";

NSData* StringToData(std::string_view str) {
  return [NSData dataWithBytes:str.data() length:str.size()];
}

NSURL* TestStorageFileURL() {
  return [[NSURL fileURLWithPath:NSTemporaryDirectory()]
      URLByAppendingPathComponent:@"credentials"];
}

// Returns a valid 32-byte trusted vault key for tests.
webauthn::SharedKey GetTestTrustedVaultKey() {
  return std::vector<uint8_t>{1, 2, 3, 4, 1, 2, 3, 4, 1, 2, 3, 4, 1, 2, 3, 4,
                              1, 2, 3, 4, 1, 2, 3, 4, 1, 2, 3, 4, 1, 2, 3, 4};
}

// Creates a passkey `ArchivableCredential` with static test fields.
ArchivableCredential* TestPasskeyCredential(BOOL edited_by_user = NO) {
  return [[ArchivableCredential alloc]
       initWithFavicon:kTestFavicon
                  gaia:kTestGaia
      recordIdentifier:kTestRecordIdentifier
                syncId:StringToData(kTestSyncId)
              username:kTestUsername
       userDisplayName:kTestUserDisplayName
                userId:StringToData(kTestUserId)
          credentialId:StringToData(kTestCredentialId)
                  rpId:kTestRPId
            privateKey:StringToData(kTestPrivateKey)
             encrypted:nil
          creationTime:0
          lastUsedTime:0
                hidden:NO
            hiddenTime:0
          editedByUser:edited_by_user];
}

// Creates a passkey `ArchivableCredential` with valid encrypted secrets using
// `GetTestTrustedVaultKey()` so assertion signing succeeds.
ArchivableCredential* TestEncryptedPasskeyCredential() {
  std::vector<uint8_t> user_id(kTestUserId.begin(), kTestUserId.end());
  webauthn::PasskeyModel::UserEntity user_entity(
      user_id, SysNSStringToUTF8(kTestUsername),
      SysNSStringToUTF8(kTestUserDisplayName));
  auto generated_passkey =
      webauthn::passkey_model_utils::GeneratePasskeyAndEncryptSecrets(
          SysNSStringToUTF8(kTestRPId), user_entity, GetTestTrustedVaultKey(),
          /*trusted_vault_key_version=*/0,
          /*extension_input_data=*/{}, /*extension_output_data=*/nullptr);
  return [[ArchivableCredential alloc] initWithFavicon:kTestFavicon
                                                  gaia:kTestGaia
                                               passkey:generated_passkey.first];
}

// Creates a password `ArchivableCredential` with static test fields and a
// recent `lastUsedTime`.
ArchivableCredential* TestPasswordCredential(NSString* gaia = kTestGaia) {
  int64_t recent_time = Time::Now().ToDeltaSinceWindowsEpoch().InMicroseconds();
  return [[ArchivableCredential alloc]
               initWithFavicon:kTestFavicon
                          gaia:gaia
                      password:kTestPassword
                          rank:1
              recordIdentifier:kTestPasswordRecordIdentifier
             serviceIdentifier:kTestServiceIdentifier
                   serviceName:kTestRPId
      registryControlledDomain:kTestRPId
                      username:kTestUsername
                          note:nil
                  lastUsedTime:recent_time];
}

// Creates an `ASPasswordCredentialRequest` for `credential`.
ASPasswordCredentialRequest* CreatePasswordRequest(id<Credential> credential) {
  ASPasswordCredentialIdentity* identity =
      [[ASPasswordCredentialIdentity alloc] cr_initWithCredential:credential];
  return [ASPasswordCredentialRequest requestWithCredentialIdentity:identity];
}

// Creates an `ASPasskeyCredentialRequest` for `credential`.
ASPasskeyCredentialRequest* CreatePasskeyRequest(
    id<Credential> credential,
    ASAuthorizationPublicKeyCredentialUserVerificationPreference uv_preference =
        ASAuthorizationPublicKeyCredentialUserVerificationPreferenceDiscouraged,
    NSArray<NSNumber*>* algorithms = @[ @(kSupportedAlgorithm) ]) {
  ASPasskeyCredentialIdentity* identity =
      [[ASPasskeyCredentialIdentity alloc] cr_initWithCredential:credential];
  return [ASPasskeyCredentialRequest
      requestWithCredentialIdentity:identity
                     clientDataHash:StringToData(kTestClientDataHash)
         userVerificationPreference:uv_preference
                supportedAlgorithms:algorithms];
}

// Configures `NSUserDefaults` with all settings required for eligible passkey
// creation.
void ConfigureEligiblePasskeyCreationDefaults(
    BOOL automatic_passkey_upgrade_enabled = YES,
    BOOL multi_profile_enabled = NO) {
  NSUserDefaults* user_defaults = GetGroupUserDefaults();
  [user_defaults setObject:kTestGaia
                    forKey:AppGroupUserDefaultsCredentialProviderUserID()];
  [user_defaults setObject:kTestEmail
                    forKey:AppGroupUserDefaultsCredentialProviderUserEmail()];
  [user_defaults
      setObject:@YES
         forKey:AppGroupUserDefaultsCredentialProviderSavingPasskeysEnabled()];
  [user_defaults
      setObject:@YES
         forKey:AppGroupUserDefaultsCredentialProviderSavingPasswordsEnabled()];
  [user_defaults
      setBool:NO
       forKey:AppGroupUserDefaultsCredentialProviderSavingPasswordsManaged()];
  [user_defaults
      setBool:YES
       forKey:AppGroupUserDefaultsCredentialProviderPasswordSyncSetting()];
  [user_defaults
      setBool:automatic_passkey_upgrade_enabled
       forKey:
           AppGroupUserDefaulsCredentialProviderAutomaticPasskeyUpgradeEnabled()];
  [user_defaults
      setBool:multi_profile_enabled
       forKey:AppGroupUserDefaultsCredentialProviderMultiProfileSetting()];
}

// Cleans up temporary files and shared `NSUserDefaults` keys modified by
// tests.
void CleanStorage() {
  [[NSFileManager defaultManager] removeItemAtURL:TestStorageFileURL()
                                            error:nil];
  NSUserDefaults* user_defaults = GetGroupUserDefaults();
  [user_defaults removeObjectForKey:kTestUserDefaultsKey];
  [user_defaults
      removeObjectForKey:AppGroupUserDefaultsCredentialProviderUserID()];
  [user_defaults
      removeObjectForKey:AppGroupUserDefaultsCredentialProviderUserEmail()];
  [user_defaults
      removeObjectForKey:AppGroupUserDefaultsCredentialProviderManagedUserID()];
  [user_defaults removeObjectForKey:
                     AppGroupUserDefaultsCredentialProviderNewCredentials()];
  [user_defaults
      removeObjectForKey:
          AppGroupUserDefaultsCredentialProviderMultiProfileSetting()];
  [user_defaults
      removeObjectForKey:
          AppGroupUserDefaultsCredentialProviderSavingPasswordsEnabled()];
  [user_defaults
      removeObjectForKey:
          AppGroupUserDefaultsCredentialProviderSavingPasswordsManaged()];
  [user_defaults
      removeObjectForKey:
          AppGroupUserDefaultsCredentialProviderSavingPasskeysEnabled()];
  [user_defaults
      removeObjectForKey:
          AppGroupUserDefaultsCredentialProviderPasswordSyncSetting()];
  [user_defaults
      removeObjectForKey:
          AppGroupUserDefaulsCredentialProviderAutomaticPasskeyUpgradeEnabled()];
  [user_defaults
      removeObjectForKey:HistogramCountKey(kBrowserAssistedLoginHistogram,
                                           kPasskeyBucket)];
  [user_defaults
      removeObjectForKey:app_group::kCredentialExtensionQuickPasswordUseCount];
  [user_defaults
      removeObjectForKey:app_group::kCredentialExtensionQuickPasskeyUseCount];
}

// Fake `PasskeyKeychainProvider` for testing trusted vault key fetches.
class FakePasskeyKeychainProvider : public PasskeyKeychainProvider {
 public:
  FakePasskeyKeychainProvider()
      : PasskeyKeychainProvider(/*metrics_reporting_enabled=*/false),
        keys_({GetTestTrustedVaultKey()}) {}

  ~FakePasskeyKeychainProvider() override = default;

  void SetKeys(SharedKeyList keys) { keys_ = std::move(keys); }

  // `PasskeyKeychainProvider`:
  void CheckEnrolled(NSString* gaia, CheckEnrolledCallback callback) override {
    std::move(callback).Run(/*is_enrolled=*/YES, /*error=*/nil);
  }

  void FetchKeys(NSString* gaia,
                 webauthn::ReauthenticatePurpose purpose,
                 webauthn::KeysFetchedCallback callback) override {
    std::move(callback).Run(keys_, /*error=*/nil);
  }

  void CheckDegradedRecoverability(
      NSString* gaia,
      CheckDegradedRecoverabilityCallback callback) override {
    std::move(callback).Run(
        /*in_degraded_recoverability=*/NO, /*error=*/nil);
  }

 private:
  SharedKeyList keys_;
};

class CredentialProviderViewControllerTest : public PlatformTest {
 protected:
  CredentialProviderViewControllerTest() {
    CleanStorage();

    controller_ = [[TestCredentialProviderViewController alloc] init];
    mock_extension_context_ =
        OCMClassMock([ASCredentialProviderExtensionContext class]);
    controller_.mockExtensionContext = mock_extension_context_;

    OCMStub([mock_extension_context_ cancelRequestWithError:[OCMArg any]])
        .andDo(^(NSInvocation* invocation) {
          void* ptr = nullptr;
          [invocation getArgument:&ptr atIndex:2];
          cancel_error_future_.SetValue((__bridge NSError*)ptr);
        });
    OCMStub([mock_extension_context_
                completeRequestWithSelectedCredential:[OCMArg any]
                                    completionHandler:[OCMArg any]])
        .andDo(^(NSInvocation* invocation) {
          void* ptr = nullptr;
          [invocation getArgument:&ptr atIndex:2];
          completed_password_future_.SetValue(
              (__bridge ASPasswordCredential*)ptr);
        });
    OCMStub(
        [mock_extension_context_
            completeAssertionRequestWithSelectedPasskeyCredential:[OCMArg any]
                                                completionHandler:[OCMArg any]])
        .andDo(^(NSInvocation* invocation) {
          void* ptr = nullptr;
          [invocation getArgument:&ptr atIndex:2];
          completed_assertion_future_.SetValue(
              (__bridge ASPasskeyAssertionCredential*)ptr);
        });
    OCMStub(
        [mock_extension_context_
            completeRegistrationRequestWithSelectedPasskeyCredential:[OCMArg
                                                                         any]
                                                   completionHandler:[OCMArg
                                                                         any]])
        .andDo(^(NSInvocation* invocation) {
          void* ptr = nullptr;
          [invocation getArgument:&ptr atIndex:2];
          completed_registration_future_.SetValue(
              (__bridge ASPasskeyRegistrationCredential*)ptr);
        });
    OCMStub([mock_extension_context_ completeExtensionConfigurationRequest])
        .andDo(^(NSInvocation* invocation) {
          completed_configuration_future_.SetValue(true);
        });
    controller_.onPresentViewController = ^(UIViewController* presented) {
      if (!presented_vc_future_.IsReady()) {
        presented_vc_future_.SetValue(presented);
      }
    };

    mock_reauth_module_ = [[MockReauthenticationModule alloc] init];
    mock_reauth_module_.shouldSkipReAuth = YES;
    mock_reauth_module_.expectedResult = ReauthenticationResult::kSuccess;
    controller_.reauthenticationModule = mock_reauth_module_;

    fake_account_verificator_ = [[FakeAccountVerificationProvider alloc] init];
    fake_account_verificator_.isValid = YES;
    controller_.accountVerificator = fake_account_verificator_;

    auto fake_provider = std::make_unique<FakePasskeyKeychainProvider>();
    fake_keychain_provider_ = fake_provider.get();
    PasskeyKeychainProviderBridge* bridge =
        [[PasskeyKeychainProviderBridge alloc]
            initWithPasskeyKeychainProvider:std::move(fake_provider)];
    bridge.delegate = controller_;
    controller_.passkeyKeychainProviderBridge = bridge;
    CreateStoreWithCredentials(@[]);
  }

  ~CredentialProviderViewControllerTest() override {
    window_.rootViewController = nil;
    window_.hidden = YES;
    CleanStorage();
  }

  // Attaches `controller_` as the root view controller of a visible `UIWindow`.
  void AttachControllerToWindow() {
    window_ = [[UIWindow alloc]
        initWithFrame:CGRectMake(0, 0, kWindowSize, kWindowSize)];
    window_.rootViewController = controller_;
    [window_ makeKeyAndVisible];
    [controller_ loadViewIfNeeded];
  }

  // Creates a `MultiStoreCredentialStore` populated with `credentials` and
  // assigns it to `controller_.credentialStore`.
  void CreateStoreWithCredentials(NSArray<id<Credential>>* credentials) {
    UserDefaultsCredentialStore* user_defaults_store =
        [[UserDefaultsCredentialStore alloc]
            initWithUserDefaults:GetGroupUserDefaults()
                             key:kTestUserDefaultsKey];
    ArchivableCredentialStore* archivable_store =
        [[ArchivableCredentialStore alloc]
            initWithFileURL:TestStorageFileURL()];

    for (id<Credential> credential : credentials) {
      [archivable_store addCredential:credential];
    }
    TestFuture<NSError*> save_future;
    [archivable_store saveDataWithCompletion:base::CallbackToBlock(
                                                 save_future.GetCallback())];
    EXPECT_TRUE(save_future.Wait());

    credential_store_ = [[MultiStoreCredentialStore alloc]
        initWithStores:@[ user_defaults_store, archivable_store ]];
    controller_.credentialStore = credential_store_;
  }

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI,
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  UIWindow* window_;
  TestCredentialProviderViewController* controller_;
  id mock_extension_context_;
  MockReauthenticationModule* mock_reauth_module_;
  FakeAccountVerificationProvider* fake_account_verificator_;
  raw_ptr<FakePasskeyKeychainProvider> fake_keychain_provider_ = nullptr;
  id<CredentialStore> credential_store_;

  TestFuture<NSError*> cancel_error_future_;
  TestFuture<ASPasswordCredential*> completed_password_future_;
  TestFuture<ASPasskeyAssertionCredential*> completed_assertion_future_;
  TestFuture<ASPasskeyRegistrationCredential*> completed_registration_future_;
  TestFuture<bool> completed_configuration_future_;
  TestFuture<UIViewController*> presented_vc_future_;
};

// Test that reporting an unknown public key credential marks the matching
// credential as hidden and records `hiddenTime`.
TEST_F(CredentialProviderViewControllerTest, MarksUnknownCredentialHidden) {
  CreateStoreWithCredentials(@[ TestPasskeyCredential() ]);

  ASSERT_EQ(credential_store_.credentials.count, 1u);
  EXPECT_FALSE(credential_store_.credentials[0].hidden);
  EXPECT_EQ(credential_store_.credentials[0].hiddenTime, 0);

  [controller_
      reportUnknownPublicKeyCredentialForRelyingParty:kTestRPId
                                         credentialID:StringToData(
                                                          kTestCredentialId)];
  ASSERT_EQ(credential_store_.credentials.count, 1u);
  EXPECT_TRUE(credential_store_.credentials[0].hidden);
  EXPECT_EQ(credential_store_.credentials[0].hiddenTime,
            Time::Now().InMillisecondsSinceUnixEpoch());
}

// Test that reporting a username update is ignored when the credential was
// previously edited by the user.
TEST_F(CredentialProviderViewControllerTest,
       IgnoresUsernameUpdateForCredentialEditedByUser) {
  CreateStoreWithCredentials(
      @[ TestPasskeyCredential(/*edited_by_user=*/YES) ]);

  ASSERT_EQ(credential_store_.credentials.count, 1u);
  EXPECT_NSEQ(credential_store_.credentials[0].username, kTestUsername);

  [controller_
      reportPublicKeyCredentialUpdateForRelyingParty:kTestRPId
                                          userHandle:StringToData(kTestUserId)
                                             newName:kTestNewUsername];
  ASSERT_EQ(credential_store_.credentials.count, 1u);
  EXPECT_NSEQ(credential_store_.credentials[0].username, kTestUsername);
}

// Test that reporting a username update modifies the credential's `username`
// when it was not edited by the user.
TEST_F(CredentialProviderViewControllerTest, UpdatesCredentialUsername) {
  CreateStoreWithCredentials(@[ TestPasskeyCredential() ]);

  ASSERT_EQ(credential_store_.credentials.count, 1u);
  EXPECT_NSEQ(credential_store_.credentials[0].username, kTestUsername);

  [controller_
      reportPublicKeyCredentialUpdateForRelyingParty:kTestRPId
                                          userHandle:StringToData(kTestUserId)
                                             newName:kTestNewUsername];
  ASSERT_EQ(credential_store_.credentials.count, 1u);
  EXPECT_NSEQ(credential_store_.credentials[0].username, kTestNewUsername);
}

// Test that reporting an accepted credentials list that excludes a credential
// marks that credential as hidden.
TEST_F(CredentialProviderViewControllerTest,
       MarksCredentialNotPresentOnAcceptedListAsHidden) {
  CreateStoreWithCredentials(@[ TestPasskeyCredential() ]);

  ASSERT_EQ(credential_store_.credentials.count, 1u);
  EXPECT_FALSE(credential_store_.credentials[0].hidden);

  [controller_
      reportAllAcceptedPublicKeyCredentialsForRelyingParty:kTestRPId
                                                userHandle:StringToData(
                                                               kTestUserId)
                                     acceptedCredentialIDs:@[]];
  ASSERT_EQ(credential_store_.credentials.count, 1u);
  EXPECT_TRUE(credential_store_.credentials[0].hidden);
  EXPECT_EQ(credential_store_.credentials[0].hiddenTime,
            Time::Now().InMillisecondsSinceUnixEpoch());
}

// Test that a hidden credential is restored when its ID is included in the
// accepted credentials list.
TEST_F(CredentialProviderViewControllerTest, HiddenCredentialRestored) {
  CreateStoreWithCredentials(@[ TestPasskeyCredential() ]);

  ASSERT_EQ(credential_store_.credentials.count, 1u);
  EXPECT_FALSE(credential_store_.credentials[0].hidden);

  // Mark the credential as unknown, it should be hidden.
  [controller_
      reportUnknownPublicKeyCredentialForRelyingParty:kTestRPId
                                         credentialID:StringToData(
                                                          kTestCredentialId)];
  ASSERT_EQ(credential_store_.credentials.count, 1u);
  EXPECT_TRUE(credential_store_.credentials[0].hidden);

  // After restoring, it shouldn't be hidden anymore.
  [controller_
      reportAllAcceptedPublicKeyCredentialsForRelyingParty:kTestRPId
                                                userHandle:StringToData(
                                                               kTestUserId)
                                     acceptedCredentialIDs:@[
                                       StringToData(kTestCredentialId)
                                     ]];
  ASSERT_EQ(credential_store_.credentials.count, 1u);
  EXPECT_FALSE(credential_store_.credentials[0].hidden);
  EXPECT_EQ(credential_store_.credentials[0].hiddenTime, 0);
}

// Test that completing a passkey assertion records the browser-assisted login
// metric.
TEST_F(CredentialProviderViewControllerTest,
       PasskeyAssertionRecordsBrowserAssistedLoginMetric) {
  NSUserDefaults* user_defaults = GetGroupUserDefaults();
  NSString* key =
      HistogramCountKey(kBrowserAssistedLoginHistogram, kPasskeyBucket);
  EXPECT_EQ([user_defaults integerForKey:key], 0);

  ASPasskeyAssertionCredential* credential = [ASPasskeyAssertionCredential
      credentialWithUserHandle:StringToData(kTestUserId)
                  relyingParty:kTestRPId
                     signature:StringToData(kTestSignature)
                clientDataHash:StringToData(kTestClientDataHash)
             authenticatorData:StringToData(kTestAuthenticatorData)
                  credentialID:StringToData(kTestCredentialId)];
  [controller_ userSelectedPasskey:credential];

  EXPECT_EQ([user_defaults integerForKey:key], 1);
  ASSERT_TRUE(completed_assertion_future_.Wait());
  EXPECT_EQ(completed_assertion_future_.Get(), credential);
}

// Test that a nil passkey assertion fails with
// `ASExtensionErrorCodeCredentialIdentityNotFound` and does not record the
// browser-assisted login metric.
TEST_F(CredentialProviderViewControllerTest,
       PasskeyAssertionFailureDoesNotRecordBrowserAssistedLoginMetric) {
  NSUserDefaults* user_defaults = GetGroupUserDefaults();
  NSString* key =
      HistogramCountKey(kBrowserAssistedLoginHistogram, kPasskeyBucket);
  EXPECT_EQ([user_defaults integerForKey:key], 0);

  [controller_ userSelectedPasskey:nil];
  EXPECT_EQ([user_defaults integerForKey:key], 0);
  ASSERT_TRUE(cancel_error_future_.Wait());
  EXPECT_EQ(cancel_error_future_.Get().code,
            ASExtensionErrorCodeCredentialIdentityNotFound);
}

// Test that `userSelectedPassword:` completes the extension request with the
// selected password credential.
TEST_F(CredentialProviderViewControllerTest,
       UserSelectedPasswordCompletesRequest) {
  ASPasswordCredential* credential =
      [ASPasswordCredential credentialWithUser:kTestUsername
                                      password:kTestPassword];
  [controller_ userSelectedPassword:credential];

  ASSERT_TRUE(completed_password_future_.Wait());
  EXPECT_EQ(completed_password_future_.Get(), credential);
}

// Test that `userSelectedPasskey:passkeyRequestDetails:` fetches trusted vault
// keys, signs the passkey assertion, and completes the request.
TEST_F(CredentialProviderViewControllerTest,
       UserSelectedPasskeyWithRequestDetailsCompletesAssertion) {
  ArchivableCredential* passkey = TestEncryptedPasskeyCredential();
  ASPasskeyCredentialRequest* request = CreatePasskeyRequest(passkey);
  PasskeyRequestDetails* details =
      [[PasskeyRequestDetails alloc] initWithRequest:request
                    isBiometricAuthenticationEnabled:YES
                                 isConditionalCreate:NO];

  [controller_ userSelectedPasskey:passkey passkeyRequestDetails:details];

  ASSERT_TRUE(completed_assertion_future_.Wait());
  ASPasskeyAssertionCredential* assertion = completed_assertion_future_.Get();
  ASSERT_TRUE(assertion);
  EXPECT_NSEQ(assertion.relyingParty, kTestRPId);
  EXPECT_NSEQ(assertion.credentialID, passkey.credentialId);
}

// Test that `userSelectedPasskey:passkeyRequestDetails:` exits with
// `ASExtensionErrorCodeUserInteractionRequired` when the fetched trusted vault
// keys are empty and the view has no window to present the welcome screen.
TEST_F(CredentialProviderViewControllerTest,
       UserSelectedPasskeyWithEmptyKeysExitsWithUserInteractionRequiredError) {
  fake_keychain_provider_->SetKeys({});
  ArchivableCredential* passkey = TestEncryptedPasskeyCredential();
  ASPasskeyCredentialRequest* request = CreatePasskeyRequest(passkey);
  PasskeyRequestDetails* details =
      [[PasskeyRequestDetails alloc] initWithRequest:request
                    isBiometricAuthenticationEnabled:YES
                                 isConditionalCreate:NO];

  [controller_ userSelectedPasskey:passkey passkeyRequestDetails:details];

  ASSERT_TRUE(cancel_error_future_.Wait());
  EXPECT_EQ(cancel_error_future_.Get().code,
            ASExtensionErrorCodeUserInteractionRequired);
}

// Test that `userCancelledRequestWithErrorCode:` cancels the extension request
// with the provided error code.
TEST_F(CredentialProviderViewControllerTest,
       UserCancelledRequestExitsWithErrorCode) {
  [controller_
      userCancelledRequestWithErrorCode:ASExtensionErrorCodeUserCanceled];

  ASSERT_TRUE(cancel_error_future_.Wait());
  EXPECT_EQ(cancel_error_future_.Get().code, ASExtensionErrorCodeUserCanceled);
}

// Test that `completeExtensionConfigurationRequest` dismisses the presented
// `ConsentViewController` and notifies `extensionContext`.
TEST_F(CredentialProviderViewControllerTest,
       CompleteExtensionConfigurationRequestStopsCoordinatorAndCompletes) {
  AttachControllerToWindow();
  CreateStoreWithCredentials(@[]);
  [controller_ prepareInterfaceForExtensionConfiguration];
  EXPECT_TRUE([controller_.presentedViewController
      isKindOfClass:[ConsentViewController class]]);
  EXPECT_FALSE(controller_.didDismissViewController);

  [controller_ completeExtensionConfigurationRequest];

  EXPECT_TRUE(controller_.didDismissViewController);
  ASSERT_TRUE(completed_configuration_future_.Wait());
  EXPECT_TRUE(completed_configuration_future_.Get());
}

// Test that `viewDidLoad` configures the view's background color and adds a
// `UINavigationBar` subview with a branded title view.
TEST_F(CredentialProviderViewControllerTest,
       ViewDidLoadConfiguresBackgroundAndNavigationBar) {
  [controller_ loadViewIfNeeded];

  UITraitCollection* traits = controller_.traitCollection;
  EXPECT_NSEQ([controller_.view.backgroundColor
                  resolvedColorWithTraitCollection:traits],
              [[UIColor colorNamed:kGroupedPrimaryBackgroundColor]
                  resolvedColorWithTraitCollection:traits]);

  UINavigationBar* nav_bar =
      ObjCCast<UINavigationBar>(controller_.view.subviews.firstObject);
  ASSERT_TRUE(nav_bar);
  ASSERT_EQ(nav_bar.items.count, 1u);
  EXPECT_TRUE(nav_bar.items.firstObject.titleView);
}

// Test that `viewWillAppear:` does not present a credential list when no
// `serviceIdentifiers` were prepared.
TEST_F(CredentialProviderViewControllerTest,
       ViewWillAppearWithoutServiceIdentifiersDoesNothing) {
  AttachControllerToWindow();
  [controller_ viewWillAppear:NO];

  EXPECT_FALSE(controller_.presentedViewController);
}

// Test that `prepareCredentialListForServiceIdentifiers:` followed by
// `viewWillAppear:` validates the user, reauthenticates, and presents the
// credential list.
TEST_F(CredentialProviderViewControllerTest,
       ViewWillAppearWithServiceIdentifiersStartsCredentialListCoordinator) {
  AttachControllerToWindow();
  CreateStoreWithCredentials(@[ TestPasswordCredential() ]);

  ASCredentialServiceIdentifier* identifier =
      [[ASCredentialServiceIdentifier alloc]
          initWithIdentifier:kTestServiceIdentifier
                        type:ASCredentialServiceIdentifierTypeURL];
  [controller_ prepareCredentialListForServiceIdentifiers:@[ identifier ]];
  [controller_ viewWillAppear:NO];

  ASSERT_TRUE(presented_vc_future_.Wait());
  UINavigationController* nav_controller =
      ObjCCast<UINavigationController>(presented_vc_future_.Get());
  ASSERT_TRUE(nav_controller);
  EXPECT_TRUE([nav_controller.topViewController
      isKindOfClass:[CredentialListViewController class]]);
  for (UIView* subview in controller_.view.subviews) {
    EXPECT_FALSE([subview isKindOfClass:[UIActivityIndicatorView class]]);
  }
}

// Test that `prepareCredentialListForServiceIdentifiers:requestParameters:`
// followed by `viewWillAppear:` presents the credential list for a passkey
// request and completes user verification after reauthentication.
TEST_F(CredentialProviderViewControllerTest,
       ViewWillAppearWithPasskeyRequestParametersStartsListCoordinator) {
  AttachControllerToWindow();
  CreateStoreWithCredentials(@[ TestPasskeyCredential() ]);

  ASCredentialServiceIdentifier* identifier =
      [[ASCredentialServiceIdentifier alloc]
          initWithIdentifier:kTestRPId
                        type:ASCredentialServiceIdentifierTypeDomain];
  id mock_params = OCMClassMock([ASPasskeyCredentialRequestParameters class]);
  OCMStub([mock_params clientDataHash])
      .andReturn(StringToData(kTestClientDataHash));
  OCMStub([mock_params userVerificationPreference])
      .andReturn(
          ASAuthorizationPublicKeyCredentialUserVerificationPreferenceDiscouraged);
  OCMStub([mock_params relyingPartyIdentifier]).andReturn(kTestRPId);
  OCMStub([mock_params allowedCredentials]).andReturn(@[]);

  [controller_ prepareCredentialListForServiceIdentifiers:@[ identifier ]
                                        requestParameters:mock_params];

  [controller_ viewWillAppear:NO];

  ASSERT_TRUE(presented_vc_future_.Wait());
  UINavigationController* nav_controller =
      ObjCCast<UINavigationController>(presented_vc_future_.Get());
  ASSERT_TRUE(nav_controller);
  EXPECT_TRUE([nav_controller.topViewController
      isKindOfClass:[CredentialListViewController class]]);

  // Verify user verification was marked as completed during `viewWillAppear:`
  // by checking that `performUserVerificationIfNeeded:` immediately succeeds
  // without invoking `mock_reauth_module_`.
  mock_reauth_module_.expectedResult = ReauthenticationResult::kFailure;
  TestFuture<BOOL> uv_future;
  [controller_ performUserVerificationIfNeeded:base::CallbackToBlock(
                                                   uv_future.GetCallback())];
  ASSERT_TRUE(uv_future.Wait());
  EXPECT_TRUE(uv_future.Get());
}

// Test that `viewWillAppear:` presents `StaleCredentialsViewController` when
// managed user validation fails, and exits when adaptive dismissal is
// triggered.
TEST_F(CredentialProviderViewControllerTest,
       ViewWillAppearWithInvalidUserShowsStaleCredentials) {
  AttachControllerToWindow();
  [GetGroupUserDefaults()
      setObject:kTestManagedUserId
         forKey:AppGroupUserDefaultsCredentialProviderManagedUserID()];
  fake_account_verificator_.isValid = NO;

  ASCredentialServiceIdentifier* identifier =
      [[ASCredentialServiceIdentifier alloc]
          initWithIdentifier:kTestServiceIdentifier
                        type:ASCredentialServiceIdentifierTypeURL];
  [controller_ prepareCredentialListForServiceIdentifiers:@[ identifier ]];

  [controller_ viewWillAppear:NO];

  ASSERT_TRUE(presented_vc_future_.Wait());
  UINavigationController* nav_controller =
      ObjCCast<UINavigationController>(presented_vc_future_.Get());
  ASSERT_TRUE(nav_controller);
  EXPECT_TRUE([nav_controller.topViewController
      isKindOfClass:[StaleCredentialsViewController class]]);

  // Trigger adaptive presentation controller dismissal and verify it exits with
  // `ASExtensionErrorCodeUserCanceled`.
  [controller_
      presentationControllerDidDismiss:nav_controller.presentationController];
  ASSERT_TRUE(cancel_error_future_.Wait());
  EXPECT_EQ(cancel_error_future_.Get().code, ASExtensionErrorCodeUserCanceled);
}

// Test that `viewWillAppear:` exits with `ASExtensionErrorCodeFailed` when
// reauthentication fails.
TEST_F(CredentialProviderViewControllerTest,
       ViewWillAppearWithReauthFailureExitsWithFailedError) {
  AttachControllerToWindow();
  mock_reauth_module_.expectedResult = ReauthenticationResult::kFailure;

  ASCredentialServiceIdentifier* identifier =
      [[ASCredentialServiceIdentifier alloc]
          initWithIdentifier:kTestServiceIdentifier
                        type:ASCredentialServiceIdentifierTypeURL];
  [controller_ prepareCredentialListForServiceIdentifiers:@[ identifier ]];
  [controller_ viewWillAppear:NO];

  ASSERT_TRUE(cancel_error_future_.Wait());
  EXPECT_EQ(cancel_error_future_.Get().code, ASExtensionErrorCodeFailed);
  EXPECT_FALSE(controller_.presentedViewController);
}

// Test that `provideCredentialWithoutUserInteractionForRequest:` exits
// immediately with `ASExtensionErrorCodeUserInteractionRequired` when a passkey
// assertion request requires user verification.
TEST_F(
    CredentialProviderViewControllerTest,
    ProvideCredentialWithoutUserInteractionForPasskeyRequiringUVExitsWithUserInteractionRequired) {
  ArchivableCredential* passkey = TestEncryptedPasskeyCredential();
  ASPasskeyCredentialRequest* request = CreatePasskeyRequest(
      passkey,
      ASAuthorizationPublicKeyCredentialUserVerificationPreferenceRequired);

  [controller_ provideCredentialWithoutUserInteractionForRequest:request];

  ASSERT_TRUE(cancel_error_future_.Wait());
  EXPECT_EQ(cancel_error_future_.Get().code,
            ASExtensionErrorCodeUserInteractionRequired);
}

// Test that `provideCredentialWithoutUserInteractionForRequest:` exits with
// `ASExtensionErrorCodeUserInteractionRequired` when `reauthenticationModule`
// cannot attempt reauthentication.
TEST_F(
    CredentialProviderViewControllerTest,
    ProvideCredentialWithoutUserInteractionWhenCannotAttemptReauthExitsWithUserInteractionRequired) {
  mock_reauth_module_.canAttempt = NO;
  ArchivableCredential* password = TestPasswordCredential();
  CreateStoreWithCredentials(@[ password ]);
  ASPasswordCredentialRequest* request = CreatePasswordRequest(password);

  [controller_ provideCredentialWithoutUserInteractionForRequest:request];

  ASSERT_TRUE(cancel_error_future_.Wait());
  EXPECT_EQ(cancel_error_future_.Get().code,
            ASExtensionErrorCodeUserInteractionRequired);
}

// Test that `provideCredentialWithoutUserInteractionForRequest:` exits with
// `ASExtensionErrorCodeUserInteractionRequired` when managed user validation
// fails.
TEST_F(
    CredentialProviderViewControllerTest,
    ProvideCredentialWithoutUserInteractionForInvalidUserExitsWithUserInteractionRequired) {
  [GetGroupUserDefaults()
      setObject:kTestManagedUserId
         forKey:AppGroupUserDefaultsCredentialProviderManagedUserID()];
  fake_account_verificator_.isValid = NO;

  ArchivableCredential* password = TestPasswordCredential();
  CreateStoreWithCredentials(@[ password ]);
  ASPasswordCredentialRequest* request = CreatePasswordRequest(password);

  [controller_ provideCredentialWithoutUserInteractionForRequest:request];

  ASSERT_TRUE(cancel_error_future_.Wait());
  EXPECT_EQ(cancel_error_future_.Get().code,
            ASExtensionErrorCodeUserInteractionRequired);
}

// Test that `provideCredentialWithoutUserInteractionForRequest:` completes with
// the matching `ASPasswordCredential` and increments the quick password use
// count metric.
TEST_F(CredentialProviderViewControllerTest,
       ProvideCredentialWithoutUserInteractionForPasswordCompletesRequest) {
  ArchivableCredential* password = TestPasswordCredential();
  CreateStoreWithCredentials(@[ password ]);
  ASPasswordCredentialRequest* request = CreatePasswordRequest(password);

  [controller_ provideCredentialWithoutUserInteractionForRequest:request];

  ASSERT_TRUE(completed_password_future_.Wait());
  ASPasswordCredential* completed = completed_password_future_.Get();
  ASSERT_TRUE(completed);
  EXPECT_NSEQ(completed.user, kTestUsername);
  EXPECT_NSEQ(completed.password, kTestPassword);
  EXPECT_EQ(
      [GetGroupUserDefaults()
          integerForKey:app_group::kCredentialExtensionQuickPasswordUseCount],
      1);
}

// Test that `provideCredentialWithoutUserInteractionForRequest:` exits with
// `ASExtensionErrorCodeCredentialIdentityNotFound` when the requested password
// credential is not in the store.
TEST_F(
    CredentialProviderViewControllerTest,
    ProvideCredentialWithoutUserInteractionForMissingPasswordExitsWithNotFound) {
  CreateStoreWithCredentials(@[]);
  ASPasswordCredentialRequest* request =
      CreatePasswordRequest(TestPasswordCredential());

  [controller_ provideCredentialWithoutUserInteractionForRequest:request];

  ASSERT_TRUE(cancel_error_future_.Wait());
  EXPECT_EQ(cancel_error_future_.Get().code,
            ASExtensionErrorCodeCredentialIdentityNotFound);
}

// Test that `provideCredentialWithoutUserInteractionForRequest:` completes a
// passkey assertion when user verification is discouraged and increments the
// quick passkey use count metric.
TEST_F(CredentialProviderViewControllerTest,
       ProvideCredentialWithoutUserInteractionForPasskeyCompletesAssertion) {
  mock_reauth_module_.canAttemptWithBiometrics = NO;
  ArchivableCredential* passkey = TestEncryptedPasskeyCredential();
  CreateStoreWithCredentials(@[ passkey ]);
  ASPasskeyCredentialRequest* request = CreatePasskeyRequest(passkey);

  [controller_ provideCredentialWithoutUserInteractionForRequest:request];

  ASSERT_TRUE(completed_assertion_future_.Wait());
  ASPasskeyAssertionCredential* assertion = completed_assertion_future_.Get();
  ASSERT_TRUE(assertion);
  EXPECT_NSEQ(assertion.credentialID, passkey.credentialId);
  EXPECT_EQ(
      [GetGroupUserDefaults()
          integerForKey:app_group::kCredentialExtensionQuickPasskeyUseCount],
      1);
}

// Test that `provideCredentialWithoutUserInteractionForRequest:` exits with
// `ASExtensionErrorCodeCredentialIdentityNotFound` when the requested passkey
// credential is not in the store.
TEST_F(
    CredentialProviderViewControllerTest,
    ProvideCredentialWithoutUserInteractionForMissingPasskeyExitsWithNotFound) {
  mock_reauth_module_.canAttemptWithBiometrics = NO;
  CreateStoreWithCredentials(@[]);
  ArchivableCredential* passkey = TestEncryptedPasskeyCredential();
  ASPasskeyCredentialRequest* request = CreatePasskeyRequest(passkey);

  [controller_ provideCredentialWithoutUserInteractionForRequest:request];

  ASSERT_TRUE(cancel_error_future_.Wait());
  EXPECT_EQ(cancel_error_future_.Get().code,
            ASExtensionErrorCodeCredentialIdentityNotFound);
}

// Test that `prepareInterfaceToProvideCredentialForRequest:` for a passkey
// assertion exits with `ASExtensionErrorCodeFailed` when the user is invalid.
TEST_F(CredentialProviderViewControllerTest,
       PrepareInterfaceToProvidePasskeyForInvalidUserExitsWithFailedError) {
  [GetGroupUserDefaults()
      setObject:kTestManagedUserId
         forKey:AppGroupUserDefaultsCredentialProviderManagedUserID()];
  fake_account_verificator_.isValid = NO;

  ArchivableCredential* passkey = TestEncryptedPasskeyCredential();
  CreateStoreWithCredentials(@[ passkey ]);
  ASPasskeyCredentialRequest* request = CreatePasskeyRequest(
      passkey,
      ASAuthorizationPublicKeyCredentialUserVerificationPreferenceRequired);

  [controller_ prepareInterfaceToProvideCredentialForRequest:request];

  ASSERT_TRUE(cancel_error_future_.Wait());
  EXPECT_EQ(cancel_error_future_.Get().code, ASExtensionErrorCodeFailed);
}

// Test that `prepareInterfaceToProvideCredentialForRequest:` for a passkey
// assertion with required user verification performs user verification during
// key fetching and completes the assertion.
TEST_F(CredentialProviderViewControllerTest,
       PrepareInterfaceToProvidePasskeyWithUVCompletesAssertion) {
  ArchivableCredential* passkey = TestEncryptedPasskeyCredential();
  CreateStoreWithCredentials(@[ passkey ]);
  ASPasskeyCredentialRequest* request = CreatePasskeyRequest(
      passkey,
      ASAuthorizationPublicKeyCredentialUserVerificationPreferenceRequired);

  [controller_ prepareInterfaceToProvideCredentialForRequest:request];

  ASSERT_TRUE(completed_assertion_future_.Wait());
  ASPasskeyAssertionCredential* assertion = completed_assertion_future_.Get();
  ASSERT_TRUE(assertion);
  EXPECT_NSEQ(assertion.credentialID, passkey.credentialId);

  // Verify user verification was marked as completed by checking that
  // `performUserVerificationIfNeeded:` immediately succeeds without invoking
  // `mock_reauth_module_`.
  mock_reauth_module_.expectedResult = ReauthenticationResult::kFailure;
  TestFuture<BOOL> uv_future;
  [controller_ performUserVerificationIfNeeded:base::CallbackToBlock(
                                                   uv_future.GetCallback())];
  ASSERT_TRUE(uv_future.Wait());
  EXPECT_TRUE(uv_future.Get());
}

// Test that `prepareInterfaceToProvideCredentialForRequest:` for a password
// request presents `StaleCredentialsViewController` when the user is invalid,
// and tapping its close button exits with `ASExtensionErrorCodeFailed`.
TEST_F(
    CredentialProviderViewControllerTest,
    PrepareInterfaceToProvidePasswordForInvalidUserShowsStaleCredentialsAndCloseButtonExits) {
  AttachControllerToWindow();
  [GetGroupUserDefaults()
      setObject:kTestManagedUserId
         forKey:AppGroupUserDefaultsCredentialProviderManagedUserID()];
  fake_account_verificator_.isValid = NO;

  ArchivableCredential* password = TestPasswordCredential();
  CreateStoreWithCredentials(@[ password ]);
  ASPasswordCredentialRequest* request = CreatePasswordRequest(password);

  [controller_ prepareInterfaceToProvideCredentialForRequest:request];

  ASSERT_TRUE(presented_vc_future_.Wait());
  UINavigationController* nav_controller =
      ObjCCast<UINavigationController>(presented_vc_future_.Get());
  ASSERT_TRUE(nav_controller);
  StaleCredentialsViewController* top_vc =
      ObjCCast<StaleCredentialsViewController>(
          nav_controller.topViewController);
  ASSERT_TRUE(top_vc);
  UIBarButtonItem* close_button = top_vc.navigationItem.rightBarButtonItem;
  ASSERT_TRUE(close_button);

  // Perform the close button's action (`dismissExtension`).
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Warc-performSelector-leaks"
  [close_button.target performSelector:close_button.action];
#pragma clang diagnostic pop

  ASSERT_TRUE(cancel_error_future_.Wait());
  EXPECT_EQ(cancel_error_future_.Get().code, ASExtensionErrorCodeFailed);
}

// Test that `prepareInterfaceToProvideCredentialForRequest:` for a password
// request exits with `ASExtensionErrorCodeUserCanceled` when reauthentication
// fails.
TEST_F(
    CredentialProviderViewControllerTest,
    PrepareInterfaceToProvidePasswordWithReauthFailureExitsWithUserCanceled) {
  mock_reauth_module_.expectedResult = ReauthenticationResult::kFailure;
  ArchivableCredential* password = TestPasswordCredential();
  CreateStoreWithCredentials(@[ password ]);
  ASPasswordCredentialRequest* request = CreatePasswordRequest(password);

  [controller_ prepareInterfaceToProvideCredentialForRequest:request];

  ASSERT_TRUE(cancel_error_future_.Wait());
  EXPECT_EQ(cancel_error_future_.Get().code, ASExtensionErrorCodeUserCanceled);
}

// Test that `prepareInterfaceToProvideCredentialForRequest:` for a password
// request completes with `ASPasswordCredential` when user validation and
// reauthentication succeed.
TEST_F(CredentialProviderViewControllerTest,
       PrepareInterfaceToProvidePasswordCompletesRequest) {
  ArchivableCredential* password = TestPasswordCredential();
  CreateStoreWithCredentials(@[ password ]);
  ASPasswordCredentialRequest* request = CreatePasswordRequest(password);

  [controller_ prepareInterfaceToProvideCredentialForRequest:request];

  ASSERT_TRUE(completed_password_future_.Wait());
  ASPasswordCredential* completed = completed_password_future_.Get();
  ASSERT_TRUE(completed);
  EXPECT_NSEQ(completed.user, kTestUsername);
  EXPECT_NSEQ(completed.password, kTestPassword);
}

// Test that `performPasskeyRegistrationWithoutUserInteractionIfPossible:` exits
// with `ASExtensionErrorCodeFailed` when there is no matching password in the
// store.
TEST_F(
    CredentialProviderViewControllerTest,
    ConditionalPasskeyRegistrationWithoutMatchingPasswordExitsWithFailedError) {
  ConfigureEligiblePasskeyCreationDefaults();
  CreateStoreWithCredentials(@[]);
  ASPasskeyCredentialRequest* request =
      CreatePasskeyRequest(TestPasskeyCredential());

  [controller_
      performPasskeyRegistrationWithoutUserInteractionIfPossible:request];

  ASSERT_TRUE(cancel_error_future_.Wait());
  EXPECT_EQ(cancel_error_future_.Get().code, ASExtensionErrorCodeFailed);
}

// Test that `performPasskeyRegistrationWithoutUserInteractionIfPossible:` exits
// with `ASExtensionErrorCodeFailed` when passkey creation requires user
// interaction (e.g., automatic passkey upgrade is disabled).
TEST_F(CredentialProviderViewControllerTest,
       ConditionalPasskeyRegistrationWhenIneligibleExitsWithFailedError) {
  ConfigureEligiblePasskeyCreationDefaults(
      /*automatic_passkey_upgrade_enabled=*/NO);
  CreateStoreWithCredentials(@[ TestPasswordCredential() ]);
  ASPasskeyCredentialRequest* request =
      CreatePasskeyRequest(TestPasskeyCredential());

  [controller_
      performPasskeyRegistrationWithoutUserInteractionIfPossible:request];

  ASSERT_TRUE(cancel_error_future_.Wait());
  EXPECT_EQ(cancel_error_future_.Get().code, ASExtensionErrorCodeFailed);
}

// Test that `performPasskeyRegistrationWithoutUserInteractionIfPossible:`
// creates a passkey and completes registration when a matching password exists
// and creation is eligible without user interaction.
TEST_F(CredentialProviderViewControllerTest,
       ConditionalPasskeyRegistrationSucceedsWhenEligible) {
  ConfigureEligiblePasskeyCreationDefaults();
  mock_reauth_module_.canAttemptWithBiometrics = NO;
  CreateStoreWithCredentials(@[ TestPasswordCredential() ]);
  ASPasskeyCredentialRequest* request =
      CreatePasskeyRequest(TestPasskeyCredential());

  [controller_
      performPasskeyRegistrationWithoutUserInteractionIfPossible:request];

  ASSERT_TRUE(completed_registration_future_.Wait());
  EXPECT_TRUE(completed_registration_future_.Get());
}

// Test that `performPasskeyRegistrationWithoutUserInteractionIfPossible:` exits
// with `ASExtensionErrorCodeUserInteractionRequired` when trusted vault keys
// cannot be fetched without user interaction.
TEST_F(
    CredentialProviderViewControllerTest,
    ConditionalPasskeyRegistrationWithEmptyKeysExitsWithUserInteractionRequiredError) {
  ConfigureEligiblePasskeyCreationDefaults();
  mock_reauth_module_.canAttemptWithBiometrics = NO;
  fake_keychain_provider_->SetKeys({});
  CreateStoreWithCredentials(@[ TestPasswordCredential() ]);
  ASPasskeyCredentialRequest* request =
      CreatePasskeyRequest(TestPasskeyCredential());

  [controller_
      performPasskeyRegistrationWithoutUserInteractionIfPossible:request];

  ASSERT_TRUE(cancel_error_future_.Wait());
  EXPECT_EQ(cancel_error_future_.Get().code,
            ASExtensionErrorCodeUserInteractionRequired);
}

// Test that `prepareInterfaceForPasskeyRegistration:` exits with
// `ASExtensionErrorCodeFailed` when the request is not an
// `ASPasskeyCredentialRequest`.
TEST_F(CredentialProviderViewControllerTest,
       PrepareInterfaceForPasskeyRegistrationWithNonPasskeyRequestExits) {
  ASPasswordCredentialRequest* password_request =
      CreatePasswordRequest(TestPasswordCredential());

  [controller_ prepareInterfaceForPasskeyRegistration:password_request];

  ASSERT_TRUE(cancel_error_future_.Wait());
  EXPECT_EQ(cancel_error_future_.Get().code, ASExtensionErrorCodeFailed);
}

// Test that `prepareInterfaceForPasskeyRegistration:` presents the signed-out
// error alert when the passkey creation policy is not set in `NSUserDefaults`.
TEST_F(
    CredentialProviderViewControllerTest,
    PrepareInterfaceForPasskeyRegistrationWhenPolicyUnsetShowsSignedOutAlert) {
  AttachControllerToWindow();
  ASPasskeyCredentialRequest* request =
      CreatePasskeyRequest(TestPasskeyCredential());

  [controller_ prepareInterfaceForPasskeyRegistration:request];

  UINavigationController* nav_controller =
      ObjCCast<UINavigationController>(controller_.presentedViewController);
  ASSERT_TRUE(nav_controller);
  PasskeyErrorAlertViewController* alert_vc =
      ObjCCast<PasskeyErrorAlertViewController>(
          nav_controller.topViewController);
  ASSERT_TRUE(alert_vc);
  [alert_vc loadViewIfNeeded];
  EXPECT_NSEQ(alert_vc.titleString,
              CredentialProviderSignedOutUserTitleString());
}

// Test that `prepareInterfaceForPasskeyRegistration:` presents the enterprise
// disabled alert when the passkey creation policy is set to `NO`.
TEST_F(
    CredentialProviderViewControllerTest,
    PrepareInterfaceForPasskeyRegistrationWhenPasskeyPolicyDisabledShowsEnterpriseAlert) {
  AttachControllerToWindow();
  ConfigureEligiblePasskeyCreationDefaults();
  [GetGroupUserDefaults()
      setObject:@NO
         forKey:AppGroupUserDefaultsCredentialProviderSavingPasskeysEnabled()];

  ASPasskeyCredentialRequest* request =
      CreatePasskeyRequest(TestPasskeyCredential());
  [controller_ prepareInterfaceForPasskeyRegistration:request];

  UINavigationController* nav_controller =
      ObjCCast<UINavigationController>(controller_.presentedViewController);
  ASSERT_TRUE(nav_controller);
  PasskeyErrorAlertViewController* alert_vc =
      ObjCCast<PasskeyErrorAlertViewController>(
          nav_controller.topViewController);
  ASSERT_TRUE(alert_vc);
  [alert_vc loadViewIfNeeded];
  EXPECT_NSEQ(alert_vc.titleString,
              CredentialProviderPasskeyCreationEnterpriseDisabledTitleString());
}

// Test that `prepareInterfaceForPasskeyRegistration:` presents the enterprise
// disabled alert when password creation is disabled and managed by enterprise
// policy.
TEST_F(
    CredentialProviderViewControllerTest,
    PrepareInterfaceForPasskeyRegistrationWhenPasswordSavingManagedDisabledShowsEnterpriseAlert) {
  AttachControllerToWindow();
  ConfigureEligiblePasskeyCreationDefaults();
  [GetGroupUserDefaults()
      setObject:@NO
         forKey:AppGroupUserDefaultsCredentialProviderSavingPasswordsEnabled()];
  [GetGroupUserDefaults()
      setBool:YES
       forKey:AppGroupUserDefaultsCredentialProviderSavingPasswordsManaged()];

  ASPasskeyCredentialRequest* request =
      CreatePasskeyRequest(TestPasskeyCredential());
  [controller_ prepareInterfaceForPasskeyRegistration:request];

  UINavigationController* nav_controller =
      ObjCCast<UINavigationController>(controller_.presentedViewController);
  ASSERT_TRUE(nav_controller);
  PasskeyErrorAlertViewController* alert_vc =
      ObjCCast<PasskeyErrorAlertViewController>(
          nav_controller.topViewController);
  ASSERT_TRUE(alert_vc);
  [alert_vc loadViewIfNeeded];
  EXPECT_NSEQ(alert_vc.titleString,
              CredentialProviderPasskeyCreationEnterpriseDisabledTitleString());
}

// Test that `prepareInterfaceForPasskeyRegistration:` presents the manually
// disabled alert when password creation is disabled by the user, and tapping
// the alert's primary action dismisses it and exits with
// `ASExtensionErrorCodeFailed`.
TEST_F(
    CredentialProviderViewControllerTest,
    PrepareInterfaceForPasskeyRegistrationWhenSavingDisabledByUserShowsAlertAndPrimaryActionExits) {
  AttachControllerToWindow();
  ConfigureEligiblePasskeyCreationDefaults();
  [GetGroupUserDefaults()
      setObject:@NO
         forKey:AppGroupUserDefaultsCredentialProviderSavingPasswordsEnabled()];
  [GetGroupUserDefaults()
      setBool:NO
       forKey:AppGroupUserDefaultsCredentialProviderSavingPasswordsManaged()];

  ASPasskeyCredentialRequest* request =
      CreatePasskeyRequest(TestPasskeyCredential());
  [controller_ prepareInterfaceForPasskeyRegistration:request];

  UINavigationController* nav_controller =
      ObjCCast<UINavigationController>(controller_.presentedViewController);
  ASSERT_TRUE(nav_controller);
  PasskeyErrorAlertViewController* alert_vc =
      ObjCCast<PasskeyErrorAlertViewController>(
          nav_controller.topViewController);
  ASSERT_TRUE(alert_vc);
  [alert_vc loadViewIfNeeded];
  EXPECT_NSEQ(
      alert_vc.subtitleString,
      CredentialProviderPasskeyCreationUserDisabledInPasswordSettingsSubtitleString());

  // Trigger `ConfirmationAlertActionHandler`'s primary action.
  [alert_vc.actionHandler confirmationAlertPrimaryAction];
  ASSERT_TRUE(cancel_error_future_.Wait());
  EXPECT_EQ(cancel_error_future_.Get().code, ASExtensionErrorCodeFailed);
}

// Test that `prepareInterfaceForPasskeyRegistration:` presents the signed-out
// error alert when no gaia ID is available in `NSUserDefaults` or the
// credential store.
TEST_F(CredentialProviderViewControllerTest,
       PrepareInterfaceForPasskeyRegistrationWithoutGaiaShowsSignedOutAlert) {
  AttachControllerToWindow();
  ConfigureEligiblePasskeyCreationDefaults();
  [GetGroupUserDefaults()
      removeObjectForKey:AppGroupUserDefaultsCredentialProviderUserID()];
  CreateStoreWithCredentials(@[]);

  ASPasskeyCredentialRequest* request =
      CreatePasskeyRequest(TestPasskeyCredential());
  [controller_ prepareInterfaceForPasskeyRegistration:request];

  UINavigationController* nav_controller =
      ObjCCast<UINavigationController>(controller_.presentedViewController);
  ASSERT_TRUE(nav_controller);
  PasskeyErrorAlertViewController* alert_vc =
      ObjCCast<PasskeyErrorAlertViewController>(
          nav_controller.topViewController);
  ASSERT_TRUE(alert_vc);
  [alert_vc loadViewIfNeeded];
  EXPECT_NSEQ(alert_vc.titleString,
              CredentialProviderSignedOutUserTitleString());
}

// Test that `prepareInterfaceForPasskeyRegistration:` presents the account sync
// disabled alert when password sync is disabled.
TEST_F(
    CredentialProviderViewControllerTest,
    PrepareInterfaceForPasskeyRegistrationWhenPasswordSyncDisabledShowsAccountAlert) {
  AttachControllerToWindow();
  ConfigureEligiblePasskeyCreationDefaults();
  [GetGroupUserDefaults()
      setBool:NO
       forKey:AppGroupUserDefaultsCredentialProviderPasswordSyncSetting()];

  ASPasskeyCredentialRequest* request =
      CreatePasskeyRequest(TestPasskeyCredential());
  [controller_ prepareInterfaceForPasskeyRegistration:request];

  UINavigationController* nav_controller =
      ObjCCast<UINavigationController>(controller_.presentedViewController);
  ASSERT_TRUE(nav_controller);
  PasskeyErrorAlertViewController* alert_vc =
      ObjCCast<PasskeyErrorAlertViewController>(
          nav_controller.topViewController);
  ASSERT_TRUE(alert_vc);
  [alert_vc loadViewIfNeeded];
  EXPECT_NSEQ(
      alert_vc.subtitleString,
      CredentialProviderPasskeyCreationUserDisabledForAccountSubtitleString());
}

// Test that `prepareInterfaceForPasskeyRegistration:` exits with
// `ASExtensionErrorCodeFailed` when the request specifies no supported
// algorithms.
TEST_F(
    CredentialProviderViewControllerTest,
    PrepareInterfaceForPasskeyRegistrationWithUnsupportedAlgorithmExitsWithFailedError) {
  ConfigureEligiblePasskeyCreationDefaults();
  CreateStoreWithCredentials(@[]);
  ASPasskeyCredentialRequest* request = CreatePasskeyRequest(
      TestPasskeyCredential(),
      ASAuthorizationPublicKeyCredentialUserVerificationPreferenceDiscouraged,
      @[ @(kUnsupportedAlgorithm) ]);

  [controller_ prepareInterfaceForPasskeyRegistration:request];

  ASSERT_TRUE(cancel_error_future_.Wait());
  EXPECT_EQ(cancel_error_future_.Get().code, ASExtensionErrorCodeFailed);
}

// Test that `prepareInterfaceForPasskeyRegistration:` exits with
// `ASExtensionErrorCodeMatchedExcludedCredential` when the request's excluded
// credentials match an existing passkey in the store.
TEST_F(
    CredentialProviderViewControllerTest,
    PrepareInterfaceForPasskeyRegistrationWithExcludedPasskeyExitsWithMatchedExcludedCredential) {
  ConfigureEligiblePasskeyCreationDefaults();
  ArchivableCredential* existing_passkey = TestPasskeyCredential();
  CreateStoreWithCredentials(@[ existing_passkey ]);

  ASPasskeyCredentialRequest* request = CreatePasskeyRequest(existing_passkey);
  ASAuthorizationPlatformPublicKeyCredentialDescriptor* descriptor =
      [[ASAuthorizationPlatformPublicKeyCredentialDescriptor alloc]
          initWithCredentialID:existing_passkey.credentialId];
  id mock_request = OCMPartialMock(request);
  OCMStub([mock_request excludedCredentials]).andReturn(@[ descriptor ]);

  [controller_ prepareInterfaceForPasskeyRegistration:mock_request];

  ASSERT_TRUE(cancel_error_future_.Wait());
  EXPECT_EQ(cancel_error_future_.Get().code,
            ASExtensionErrorCodeMatchedExcludedCredential);
}

// Test that `prepareInterfaceForPasskeyRegistration:` presents
// `MultiProfilePasskeyCreationViewController` when multi-profile is enabled,
// and tapping the primary button creates the passkey.
TEST_F(
    CredentialProviderViewControllerTest,
    PrepareInterfaceForPasskeyRegistrationWithMultiProfilePresentsDialogAndCreatesPasskey) {
  AttachControllerToWindow();
  ConfigureEligiblePasskeyCreationDefaults(
      /*automatic_passkey_upgrade_enabled=*/YES,
      /*multi_profile_enabled=*/YES);
  mock_reauth_module_.canAttemptWithBiometrics = NO;
  CreateStoreWithCredentials(@[ TestPasswordCredential() ]);

  ASPasskeyCredentialRequest* request =
      CreatePasskeyRequest(TestPasskeyCredential());
  [controller_ prepareInterfaceForPasskeyRegistration:request];

  UINavigationController* nav_controller =
      ObjCCast<UINavigationController>(controller_.presentedViewController);
  ASSERT_TRUE(nav_controller);
  MultiProfilePasskeyCreationViewController* top_vc =
      ObjCCast<MultiProfilePasskeyCreationViewController>(
          nav_controller.topViewController);
  ASSERT_TRUE(top_vc);
  [top_vc loadViewIfNeeded];

  // Simulate tapping the primary ("Create") button on the multi-profile dialog.
  [(id<PromoStyleViewControllerDelegate>)top_vc didTapPrimaryActionButton];

  ASSERT_TRUE(completed_registration_future_.Wait());
  EXPECT_TRUE(completed_registration_future_.Get());
}

// Test that tapping the secondary ("Cancel") button on
// `MultiProfilePasskeyCreationViewController` exits with
// `ASExtensionErrorCodeUserCanceled`.
TEST_F(
    CredentialProviderViewControllerTest,
    PrepareInterfaceForPasskeyRegistrationWithMultiProfileCancelExitsWithUserCanceled) {
  AttachControllerToWindow();
  ConfigureEligiblePasskeyCreationDefaults(
      /*automatic_passkey_upgrade_enabled=*/YES,
      /*multi_profile_enabled=*/YES);
  CreateStoreWithCredentials(@[]);

  ASPasskeyCredentialRequest* request =
      CreatePasskeyRequest(TestPasskeyCredential());
  [controller_ prepareInterfaceForPasskeyRegistration:request];

  UINavigationController* nav_controller =
      ObjCCast<UINavigationController>(controller_.presentedViewController);
  ASSERT_TRUE(nav_controller);
  MultiProfilePasskeyCreationViewController* top_vc =
      ObjCCast<MultiProfilePasskeyCreationViewController>(
          nav_controller.topViewController);
  ASSERT_TRUE(top_vc);
  [top_vc loadViewIfNeeded];

  [(id<PromoStyleViewControllerDelegate>)top_vc didTapSecondaryActionButton];

  ASSERT_TRUE(cancel_error_future_.Wait());
  EXPECT_EQ(cancel_error_future_.Get().code, ASExtensionErrorCodeUserCanceled);
}

// Test that `prepareInterfaceForPasskeyRegistration:` exits with
// `ASExtensionErrorCodeFailed` when user validation fails during passkey
// creation.
TEST_F(
    CredentialProviderViewControllerTest,
    PrepareInterfaceForPasskeyRegistrationForInvalidUserExitsWithFailedError) {
  ConfigureEligiblePasskeyCreationDefaults();
  [GetGroupUserDefaults()
      setObject:kTestManagedUserId
         forKey:AppGroupUserDefaultsCredentialProviderManagedUserID()];
  fake_account_verificator_.isValid = NO;
  CreateStoreWithCredentials(@[]);

  ASPasskeyCredentialRequest* request =
      CreatePasskeyRequest(TestPasskeyCredential());
  [controller_ prepareInterfaceForPasskeyRegistration:request];

  ASSERT_TRUE(cancel_error_future_.Wait());
  EXPECT_EQ(cancel_error_future_.Get().code, ASExtensionErrorCodeFailed);
}

// Test that `prepareInterfaceForPasskeyRegistration:` falls back to a gaia ID
// from `credentialStore` when no gaia ID is stored in `NSUserDefaults`, and
// completes passkey registration with user verification.
TEST_F(
    CredentialProviderViewControllerTest,
    PrepareInterfaceForPasskeyRegistrationUsesFallbackGaiaAndCompletesRegistration) {
  ConfigureEligiblePasskeyCreationDefaults();
  [GetGroupUserDefaults()
      removeObjectForKey:AppGroupUserDefaultsCredentialProviderUserID()];
  CreateStoreWithCredentials(@[ TestPasswordCredential(kTestFallbackGaia) ]);

  ASPasskeyCredentialRequest* request = CreatePasskeyRequest(
      TestPasskeyCredential(),
      ASAuthorizationPublicKeyCredentialUserVerificationPreferenceRequired);
  [controller_ prepareInterfaceForPasskeyRegistration:request];

  ASSERT_TRUE(completed_registration_future_.Wait());
  EXPECT_TRUE(completed_registration_future_.Get());

  // Verify user verification was marked as completed by checking that
  // `performUserVerificationIfNeeded:` immediately succeeds without invoking
  // `mock_reauth_module_`.
  mock_reauth_module_.expectedResult = ReauthenticationResult::kFailure;
  TestFuture<BOOL> uv_future;
  [controller_ performUserVerificationIfNeeded:base::CallbackToBlock(
                                                   uv_future.GetCallback())];
  ASSERT_TRUE(uv_future.Wait());
  EXPECT_TRUE(uv_future.Get());
}

}  // namespace
