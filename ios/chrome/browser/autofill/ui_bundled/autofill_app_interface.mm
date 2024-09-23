// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/ui_bundled/autofill_app_interface.h"

#import "base/apple/foundation_util.h"
#import "base/check.h"
#import "base/memory/singleton.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "components/autofill/core/browser/address_data_manager.h"
#import "components/autofill/core/browser/autofill_client.h"
#import "components/autofill/core/browser/autofill_test_utils.h"
#import "components/autofill/core/browser/browser_autofill_manager_test_api.h"
#import "components/autofill/core/browser/data_model/autofill_profile_test_api.h"
#import "components/autofill/core/browser/form_data_importer.h"
#import "components/autofill/core/browser/payments/credit_card_save_manager.h"
#import "components/autofill/core/browser/payments/payments_autofill_client.h"
#import "components/autofill/core/browser/payments/payments_network_interface.h"
#import "components/autofill/core/browser/payments/virtual_card_enrollment_manager.h"
#import "components/autofill/core/browser/payments_data_manager.h"
#import "components/autofill/core/browser/personal_data_manager.h"
#import "components/autofill/core/common/autofill_prefs.h"
#import "components/autofill/ios/browser/autofill_driver_ios.h"
#import "components/autofill/ios/browser/autofill_java_script_feature.h"
#import "components/autofill/ios/browser/credit_card_save_manager_test_observer_bridge.h"
#import "components/autofill/ios/browser/ios_test_event_waiter.h"
#import "components/autofill/ios/common/features.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/password_manager/core/browser/password_manager_util.h"
#import "components/password_manager/core/browser/password_store/password_store_consumer.h"
#import "components/password_manager/core/browser/password_store/password_store_interface.h"
#import "ios/chrome/browser/autofill/model/personal_data_manager_factory.h"
#import "ios/chrome/browser/autofill/ui_bundled/scoped_autofill_payment_reauth_module_override.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_profile_password_store_factory.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/chrome/test/app/mock_reauthentication_module.h"
#import "ios/chrome/test/app/tab_test_util.h"
#import "ios/public/provider/chrome/browser/risk_data/risk_data_api.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/web_state.h"
#import "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#import "services/network/test/test_url_loader_factory.h"

namespace {

const char16_t kExampleUsername[] = u"concrete username";
const char16_t kExamplePassword[] = u"concrete password";

// Gets the current profile password store.
scoped_refptr<password_manager::PasswordStoreInterface>
GetPasswordProfileStore() {
  // ServiceAccessType governs behaviour in Incognito: only modifications with
  // EXPLICIT_ACCESS, which correspond to user's explicit gesture, succeed.
  // This test does not deal with Incognito, and should not run in Incognito
  // context. Therefore IMPLICIT_ACCESS is used to let the test fail if in
  // Incognito context.
  return IOSChromeProfilePasswordStoreFactory::GetForBrowserState(
      chrome_test_util::GetOriginalProfile(),
      ServiceAccessType::IMPLICIT_ACCESS);
}

// This class is used to obtain results from the PasswordStore and hence both
// check the success of store updates and ensure that store has finished
// processing.
class TestStoreConsumer : public password_manager::PasswordStoreConsumer {
 public:
  void OnGetPasswordStoreResults(
      std::vector<std::unique_ptr<password_manager::PasswordForm>> obtained)
      override {
    obtained_ = std::move(obtained);
  }

  const std::vector<password_manager::PasswordForm>& GetStoreResults() {
    results_.clear();
    ResetObtained();
    GetPasswordProfileStore()->GetAllLogins(weak_ptr_factory_.GetWeakPtr());
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-result"
    base::test::ios::WaitUntilConditionOrTimeout(
        base::test::ios::kWaitForFileOperationTimeout, ^bool {
          return !AreObtainedReset();
        });
#pragma clang diagnostic pop
    AppendObtainedToResults();
    return results_;
  }

 private:
  // Puts `obtained_` in a known state not corresponding to any PasswordStore
  // state.
  void ResetObtained() {
    obtained_.clear();
    obtained_.emplace_back(nullptr);
  }

  // Returns true if `obtained_` are in the reset state.
  bool AreObtainedReset() { return obtained_.size() == 1 && !obtained_[0]; }

  void AppendObtainedToResults() {
    for (const auto& source : obtained_) {
      results_.emplace_back(*source);
    }
    ResetObtained();
  }

  // Temporary cache of obtained store results.
  std::vector<std::unique_ptr<password_manager::PasswordForm>> obtained_;

  // Combination of fillable and blocked credentials from the store.
  std::vector<password_manager::PasswordForm> results_;

  base::WeakPtrFactory<TestStoreConsumer> weak_ptr_factory_{this};
};

// Saves `form` to the profile password store and waits until the async
// processing is done.
void SaveToPasswordProfileStore(const password_manager::PasswordForm& form) {
  GetPasswordProfileStore()->AddLogin(form);
  // When we retrieve the form from the store, `in_store` should be set.
  password_manager::PasswordForm expected_form = form;
  expected_form.in_store = password_manager::PasswordForm::Store::kProfileStore;
  // Check the result and ensure PasswordStore processed this.
  TestStoreConsumer consumer;
  for (const auto& result : consumer.GetStoreResults()) {
    if (result == expected_form)
      return;
  }
}

// Saves an example form in the profile store.
void SaveExamplePasswordFormInProfileStore() {
  password_manager::PasswordForm example;
  example.username_value = kExampleUsername;
  example.password_value = kExamplePassword;
  example.url = GURL("https://example.com/");
  example.signon_realm = password_manager_util::GetSignonRealm(example.url);
  SaveToPasswordProfileStore(example);
}

// Saves an example form in the profile store for the passed URL.
void SaveLocalPasswordForm(const GURL& url) {
  password_manager::PasswordForm localForm;
  localForm.username_value = kExampleUsername;
  localForm.password_value = kExamplePassword;
  localForm.url = url;
  localForm.signon_realm = password_manager_util::GetSignonRealm(localForm.url);
  SaveToPasswordProfileStore(localForm);
}

// Removes all credentials from the profile store.
void ClearProfilePasswordStore() {
  GetPasswordProfileStore()->RemoveLoginsCreatedBetween(
      FROM_HERE, base::Time(), base::Time(), base::DoNothing());
  TestStoreConsumer consumer;
}

// Saves an example profile in the store.
void AddAutofillProfile(autofill::PersonalDataManager* personalDataManager,
                        bool isAccountProfile) {
  autofill::AutofillProfile profile = autofill::test::GetFullProfile();
  // If the test profile is already in the store, adding it will be a no-op.
  // In that case, early return.
  for (const autofill::AutofillProfile* p :
       personalDataManager->address_data_manager().GetProfiles()) {
    if (p->Compare(profile) == 0) {
      return;
    }
  }
  size_t profileCount =
      personalDataManager->address_data_manager().GetProfiles().size();

  if (isAccountProfile) {
    test_api(profile).set_record_type(
        autofill::AutofillProfile::RecordType::kAccount);
  }
  personalDataManager->address_data_manager().AddProfile(profile);

  ConditionBlock conditionBlock = ^bool {
    return profileCount <
           personalDataManager->address_data_manager().GetProfiles().size();
  };
  CHECK(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForActionTimeout, conditionBlock));
}

}  // namespace

namespace autofill {

class VirtualCardEnrollmentManager;

// Helper class that provides access to private members of
// BrowserAutofillManager, FormDataImporter and CreditCardSaveManager. This
// class is friend with some autofill internal classes to access private fields.
class FakeCreditCardServer : public CreditCardSaveManager::ObserverForTest {
 public:
  static FakeCreditCardServer* SharedInstance() {
    return base::Singleton<FakeCreditCardServer>::get();
  }

  FakeCreditCardServer() {}
  ~FakeCreditCardServer() override {}
  FakeCreditCardServer(const FakeCreditCardServer&) = delete;
  FakeCreditCardServer& operator=(const FakeCreditCardServer&) = delete;

  // Access the CreditCardSaveManager.
  static CreditCardSaveManager* GetCreditCardSaveManager() {
    return GetAutofillClient()
        .GetFormDataImporter()
        ->GetCreditCardSaveManager();
  }

  // Access the VirtualCardEnrollmentManager.
  static VirtualCardEnrollmentManager* GetVirtualCardEnrollmentManager() {
    return GetAutofillClient()
        .GetPaymentsAutofillClient()
        ->GetVirtualCardEnrollmentManager();
  }

  // Access the PaymentsNetworkInterface.
  static payments::PaymentsNetworkInterface* GetPaymentsNetworkInterface() {
    return GetAutofillClient()
        .GetPaymentsAutofillClient()
        ->GetPaymentsNetworkInterface();
  }

  static AutofillClient& GetAutofillClient() {
    web::WebState* web_state = chrome_test_util::GetCurrentWebState();
    web::WebFramesManager* frames_manager =
        autofill::AutofillJavaScriptFeature::GetInstance()->GetWebFramesManager(
            web_state);
    web::WebFrame* main_frame = frames_manager->GetMainWebFrame();
    DCHECK(web_state);
    return AutofillDriverIOS::FromWebStateAndWebFrame(web_state, main_frame)
        ->GetAutofillManager()
        .client();
  }

  // Delete all failed attempds registered on every cards.
  static void ClearCreditCardSaveStrikes() {
    GetCreditCardSaveManager()
        ->GetCreditCardSaveStrikeDatabase()
        ->ClearAllStrikes();
  }

  static void ClearVirtualCardEnrollmentStrikes() {
    GetVirtualCardEnrollmentManager()->ClearAllStrikesForTesting();
  }

  // Set the number of failed attempds registered on a card.
  static void SetFormFillMaxStrikes(NSString* card_key, int max) {
    // The strike key is made of CreditCardSaveStrikeDatabase's project prefix
    // and the last 4 digits of the card used in fillAndSubmitForm(), which can
    // be found in credit_card_upload_form_address_and_cc.html.
    GetCreditCardSaveManager()
        ->GetCreditCardSaveStrikeDatabase()
        ->strike_database_->SetStrikeData(base::SysNSStringToUTF8(card_key),
                                          max);
  }

  // Reset the IOSTestEventWaiter and make it watch `events`.
  void ResetEventWaiterForEvents(NSArray* events, base::TimeDelta timeout) {
    std::list<CreditCardSaveManagerObserverEvent> events_list;
    for (NSNumber* e : events) {
      events_list.push_back(
          static_cast<CreditCardSaveManagerObserverEvent>([e intValue]));
    }
    event_waiter_ = std::make_unique<
        IOSTestEventWaiter<CreditCardSaveManagerObserverEvent>>(
        std::move(events_list), timeout);
  }

  void OnOfferLocalSave() override {
    OnEvent(CreditCardSaveManagerObserverEvent::kOnOfferLocalSaveCalled);
  }

  void OnDecideToRequestUploadSave() override {
    OnEvent(
        CreditCardSaveManagerObserverEvent::kOnDecideToRequestUploadSaveCalled);
  }

  void OnReceivedGetUploadDetailsResponse() override {
    OnEvent(CreditCardSaveManagerObserverEvent::
                kOnReceivedGetUploadDetailsResponseCalled);
  }

  void OnSentUploadCardRequest() override {
    OnEvent(CreditCardSaveManagerObserverEvent::kOnSentUploadCardRequestCalled);
  }

  void OnReceivedUploadCardResponse() override {
    OnEvent(CreditCardSaveManagerObserverEvent::
                kOnReceivedUploadCardResponseCalled);
  }

  void OnShowCardSavedFeedback() override {
    OnEvent(CreditCardSaveManagerObserverEvent::kOnShowCardSavedFeedbackCalled);
  }

  void OnStrikeChangeComplete() override {
    OnEvent(CreditCardSaveManagerObserverEvent::kOnStrikeChangeCompleteCalled);
  }

  // Triggers `event` on the IOSTestEventWaiter.
  bool OnEvent(CreditCardSaveManagerObserverEvent event) {
    return event_waiter_->OnEvent(event);
  }

  // Waits until the expected events are triggered.
  bool WaitForEvents() { return event_waiter_->Wait(); }

  // Sets the response to payment server requests.
  void SetPaymentsResponse(NSString* request, NSString* response, int error) {
    test_url_loader_factory_->AddResponse(
        base::SysNSStringToUTF8(request), base::SysNSStringToUTF8(response),
        static_cast<net::HttpStatusCode>(error));
  }

  void ClearPaymentsResponses() { test_url_loader_factory_->ClearResponses(); }

  void SetPaymentsRiskData(const std::string& risk_data) {
    GetCreditCardSaveManager()->OnDidGetUploadRiskData(risk_data);
  }

  void SetUp() {
    test_url_loader_factory_ =
        std::make_unique<network::TestURLLoaderFactory>();
    // Set up the URL loader factory for the PaymentsNetworkInterface so we can
    // intercept those network requests.
    shared_url_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            test_url_loader_factory_.get());

    payments::PaymentsNetworkInterface* payments_network_interface =
        FakeCreditCardServer::GetPaymentsNetworkInterface();
    payments_network_interface->set_url_loader_factory_for_testing(
        shared_url_loader_factory_);

    // Set a fake access token to avoid fetch requests.
    payments_network_interface->set_access_token_for_testing(
        "fake_access_token");

    // Observe actions in CreditCardSaveManager.
    CreditCardSaveManager* credit_card_save_manager =
        FakeCreditCardServer::GetCreditCardSaveManager();
    credit_card_save_manager->SetEventObserverForTesting(this);
  }

  void TearDown() {
    ClearCreditCardSaveStrikes();
    CreditCardSaveManager* credit_card_save_manager =
        FakeCreditCardServer::GetCreditCardSaveManager();
    credit_card_save_manager->SetEventObserverForTesting(nullptr);
    event_waiter_.reset();
    shared_url_loader_factory_.reset();
    test_url_loader_factory_.reset();
  }

 private:
  std::unique_ptr<network::TestURLLoaderFactory> test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_;
  std::unique_ptr<IOSTestEventWaiter<CreditCardSaveManagerObserverEvent>>
      event_waiter_;
};
}

@implementation AutofillAppInterface

static std::unique_ptr<ScopedAutofillPaymentReauthModuleOverride>
    _scopedReauthModuleOverride;

+ (void)clearProfilePasswordStore {
  ClearProfilePasswordStore();
}

+ (void)saveExamplePasswordFormToProfileStore {
  SaveExamplePasswordFormInProfileStore();
}

+ (void)savePasswordFormForURLSpec:(NSString*)URLSpec {
  SaveLocalPasswordForm(GURL(base::SysNSStringToUTF8(URLSpec)));
}

+ (NSInteger)profilesCount {
  autofill::PersonalDataManager* personalDataManager =
      [self personalDataManager];
  return personalDataManager->address_data_manager().GetProfiles().size();
}

+ (void)clearProfilesStore {
  ProfileIOS* profileIOS = chrome_test_util::GetOriginalProfile();
  autofill::PersonalDataManager* personalDataManager =
      autofill::PersonalDataManagerFactory::GetForProfile(profileIOS);
  for (const autofill::AutofillProfile* profile :
       personalDataManager->address_data_manager().GetProfiles()) {
    personalDataManager->RemoveByGUID(profile->guid());
  }

  ConditionBlock conditionBlock = ^bool {
    return 0 ==
           personalDataManager->address_data_manager().GetProfiles().size();
  };
  CHECK(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForActionTimeout, conditionBlock));

  autofill::prefs::SetAutofillProfileEnabled(profileIOS->GetPrefs(), YES);
}

+ (void)saveExampleProfile {
  AddAutofillProfile([self personalDataManager], false);
}

+ (void)saveExampleAccountProfile {
  AddAutofillProfile([self personalDataManager], true);
}

+ (NSString*)exampleProfileName {
  autofill::AutofillProfile profile = autofill::test::GetFullProfile();
  std::u16string name =
      profile.GetInfo(autofill::AutofillType(autofill::NAME_FULL),
                      GetApplicationContext()->GetApplicationLocale());
  return base::SysUTF16ToNSString(name);
}

+ (void)clearCreditCardStore {
  autofill::PersonalDataManager* personalDataManager =
      [self personalDataManager];
  for (const autofill::CreditCard* creditCard :
       personalDataManager->payments_data_manager().GetCreditCards()) {
    // This will not remove server cards, as they have no guid.
    personalDataManager->RemoveByGUID(creditCard->guid());
  }

  ProfileIOS* profile = chrome_test_util::GetOriginalProfile();
  autofill::prefs::SetAutofillPaymentMethodsEnabled(profile->GetPrefs(), YES);
}

// Clears all server data including server cards.
+ (void)clearAllServerDataForTesting {
  [self personalDataManager]
      ->payments_data_manager()
      .ClearAllServerDataForTesting();
}

+ (NSString*)saveLocalCreditCard {
  autofill::PersonalDataManager* personalDataManager =
      [self personalDataManager];
  autofill::CreditCard card = autofill::test::GetCreditCard();
  size_t card_count =
      personalDataManager->payments_data_manager().GetCreditCards().size();
  personalDataManager->payments_data_manager().AddCreditCard(card);
  ConditionBlock conditionBlock = ^bool {
    return card_count <
           personalDataManager->payments_data_manager().GetCreditCards().size();
  };
  CHECK(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForFileOperationTimeout, conditionBlock));
  personalDataManager->NotifyPersonalDataObserver();
  return base::SysUTF16ToNSString(card.NetworkAndLastFourDigits());
}

+ (NSInteger)localCreditCount {
  return [self personalDataManager]
      ->payments_data_manager()
      .GetCreditCards()
      .size();
}

+ (NSString*)saveMaskedCreditCard {
  autofill::PersonalDataManager* personalDataManager =
      [self personalDataManager];
  autofill::CreditCard card =
      autofill::test::WithCvc(autofill::test::GetMaskedServerCard());
  DCHECK(card.record_type() != autofill::CreditCard::RecordType::kLocalCard);
  personalDataManager->payments_data_manager().AddServerCreditCardForTest(
      std::make_unique<autofill::CreditCard>(card));
  personalDataManager->NotifyPersonalDataObserver();
  return base::SysUTF16ToNSString(card.NetworkAndLastFourDigits());
}

+ (NSString*)saveMaskedCreditCardEnrolledInVirtualCard {
  autofill::PersonalDataManager* personalDataManager =
      [self personalDataManager];
  size_t card_count =
      personalDataManager->payments_data_manager().GetCreditCards().size();
  autofill::CreditCard card =
      autofill::test::GetMaskedServerCardEnrolledIntoVirtualCardNumber();
  CHECK_NE(card.record_type(), autofill::CreditCard::RecordType::kLocalCard);

  personalDataManager->payments_data_manager().AddServerCreditCardForTest(
      std::make_unique<autofill::CreditCard>(card));

  // Confirm card is present in personalDataManager
  CHECK(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForFileOperationTimeout, ^bool {
        return (personalDataManager->payments_data_manager()
                    .GetCreditCards()
                    .size() == card_count + 1);
      }));

  personalDataManager->NotifyPersonalDataObserver();
  return base::SysUTF16ToNSString(card.NetworkAndLastFourDigits());
}

+ (void)setUpFakeCreditCardServer {
  autofill::FakeCreditCardServer::SharedInstance()->SetUp();
}

+ (void)tearDownFakeCreditCardServer {
  autofill::FakeCreditCardServer::SharedInstance()->TearDown();
}

// Clears the virtual card enrollment strike data.
+ (void)clearVirtualCardEnrollmentStrikes {
  autofill::FakeCreditCardServer::SharedInstance()
      ->ClearVirtualCardEnrollmentStrikes();
}

+ (void)resetEventWaiterForEvents:(NSArray*)events
                          timeout:(base::TimeDelta)timeout {
  autofill::FakeCreditCardServer::SharedInstance()->ResetEventWaiterForEvents(
      events, timeout);
}

+ (BOOL)waitForEvents {
  return autofill::FakeCreditCardServer::SharedInstance()->WaitForEvents();
}

+ (void)setPaymentsResponse:(NSString*)response
                 forRequest:(NSString*)request
              withErrorCode:(int)error {
  return autofill::FakeCreditCardServer::SharedInstance()->SetPaymentsResponse(
      request, response, error);
}

+ (void)clearPaymentsResponses {
  return autofill::FakeCreditCardServer::SharedInstance()
      ->ClearPaymentsResponses();
}

+ (void)setAccessToken {
  if (autofill::payments::PaymentsNetworkInterface* payments_network_interface =
          autofill::FakeCreditCardServer::GetPaymentsNetworkInterface()) {
    // Set a fake access token to avoid fetch requests.
    payments_network_interface->set_access_token_for_testing(
        "fake_access_token");
  }
}

+ (void)setFormFillMaxStrikes:(int)max forCard:(NSString*)card {
  return autofill::FakeCreditCardServer::SharedInstance()
      ->SetFormFillMaxStrikes(card, max);
}

+ (void)setPaymentsRiskData:(NSString*)riskData {
  return autofill::FakeCreditCardServer::SharedInstance()->SetPaymentsRiskData(
      base::SysNSStringToUTF8(riskData));
}

+ (void)considerCreditCardFormSecureForTesting {
  web::WebState* web_state = chrome_test_util::GetCurrentWebState();
  web::WebFramesManager* frames_manager =
      autofill::AutofillJavaScriptFeature::GetInstance()->GetWebFramesManager(
          web_state);
  web::WebFrame* main_frame = frames_manager->GetMainWebFrame();
  test_api(autofill::AutofillDriverIOS::FromWebStateAndWebFrame(web_state,
                                                                main_frame)
               ->GetAutofillManager())
      .SetConsiderFormAsSecureForTesting(true);
}

+ (NSString*)paymentsRiskData {
  return ios::provider::GetRiskData();
}

+ (void)setUpMockReauthenticationModule {
  MockReauthenticationModule* mock_reauthentication_module =
      [[MockReauthenticationModule alloc] init];
  _scopedReauthModuleOverride =
      ScopedAutofillPaymentReauthModuleOverride::MakeAndArmForTesting(
          mock_reauthentication_module);
}

+ (void)clearMockReauthenticationModule {
  _scopedReauthModuleOverride = nullptr;
}

+ (void)mockReauthenticationModuleCanAttempt:(BOOL)canAttempt {
  CHECK(_scopedReauthModuleOverride);
  MockReauthenticationModule* mockModule =
      base::apple::ObjCCastStrict<MockReauthenticationModule>(
          _scopedReauthModuleOverride->module);
  mockModule.canAttempt = canAttempt;
}

+ (void)mockReauthenticationModuleExpectedResult:
    (ReauthenticationResult)expectedResult {
  CHECK(_scopedReauthModuleOverride);
  MockReauthenticationModule* mockModule =
      base::apple::ObjCCastStrict<MockReauthenticationModule>(
          _scopedReauthModuleOverride->module);
  mockModule.expectedResult = expectedResult;
}

+ (void)setMandatoryReauthEnabled:(BOOL)enabled {
  autofill::PersonalDataManager* personalDataManager =
      [self personalDataManager];
  personalDataManager->payments_data_manager()
      .SetPaymentMethodsMandatoryReauthEnabled(enabled);
}

+ (BOOL)isKeyboardAccessoryUpgradeEnabled {
  return IsKeyboardAccessoryUpgradeEnabled();
}

+ (BOOL)isDynamicallyLoadFieldsOnInputEnabled {
  return base::FeatureList::IsEnabled(
      kAutofillDynamicallyLoadsFieldsForAddressInput);
}

#pragma mark - Private

// The PersonalDataManager instance for the current profile.
+ (autofill::PersonalDataManager*)personalDataManager {
  ProfileIOS* profile = chrome_test_util::GetOriginalProfile();
  autofill::PersonalDataManager* personalDataManager =
      autofill::PersonalDataManagerFactory::GetForProfile(profile);
  personalDataManager->payments_data_manager().SetSyncingForTest(true);
  return personalDataManager;
}

@end
