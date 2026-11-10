// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/manual_fill/coordinator/manual_fill_credentials_mediator.h"

#import "base/memory/raw_ptr.h"
#import "base/test/bind.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/metrics/user_action_tester.h"
#import "base/test/run_until.h"
#import "components/affiliations/core/browser/fake_affiliation_service.h"
#import "components/autofill/core/common/autofill_test_util.h"
#import "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/password_manager/core/browser/fake_form_fetcher.h"
#import "components/password_manager/core/browser/password_manager_test_utils.h"
#import "components/password_manager/core/browser/password_store/password_form_converters.h"
#import "components/password_manager/core/browser/password_store/test_password_store.h"
#import "components/password_manager/core/browser/ui/saved_passwords_presenter.h"
#import "crypto/process_bound_string.h"
#import "ios/chrome/browser/affiliations/model/ios_chrome_affiliation_service_factory.h"
#import "ios/chrome/browser/autofill/manual_fill/coordinator/form_fetcher_consumer_bridge.h"
#import "ios/chrome/browser/autofill/manual_fill/coordinator/manual_fill_credentials_mediator+Testing.h"
#import "ios/chrome/browser/autofill/manual_fill/model/manual_fill_credential.h"
#import "ios/chrome/browser/autofill/manual_fill/public/manual_fill_content_injector.h"
#import "ios/chrome/browser/autofill/manual_fill/ui/manual_fill_password_cell.h"
#import "ios/chrome/browser/autofill/manual_fill/ui/manual_fill_password_cell_mutator.h"
#import "ios/chrome/browser/autofill/manual_fill/ui/password_consumer.h"
#import "ios/chrome/browser/favicon/model/ios_chrome_favicon_loader_factory.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_profile_password_store_factory.h"
#import "ios/chrome/browser/settings/ui_bundled/password/saved_passwords_presenter_observer.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "url/gurl.h"

using password_manager::PasswordForm;
using password_manager::SavedPasswordsPresenter;
using password_manager::TestPasswordStore;

namespace {

constexpr char kExampleURL[] = "http://www.example.com/";
constexpr NSInteger kSampleCellIndex = 2;

constexpr char kSelectUsernameAction[] =
    "ManualFallback_Password_SelectUsername";
constexpr char kSelectPasswordAction[] =
    "ManualFallback_Password_SelectPassword";
constexpr char kSelectBackupPasswordAction[] =
    "ManualFallback_Password_SelectBackupPassword";
constexpr char kPasswordSuggestionAcceptedAction[] =
    "ManualFallback_Password_SuggestionAccepted";
constexpr char kBackupPasswordSuggestionAcceptedAction[] =
    "ManualFallback_Password_SuggestionAccepted_Backup";
constexpr char kAllPasswordsSuggestionAcceptedAction[] =
    "ManualFallback_AllPasswords_SuggestionAccepted";
constexpr char kBackupAllPasswordsSuggestionAcceptedAction[] =
    "ManualFallback_AllPasswords_SuggestionAccepted_Backup";

constexpr char kAcceptedSuggestionHistogram[] =
    "Autofill.UserAcceptedSuggestionAtIndex.Password.ManualFallback";
constexpr char kAcceptedSuggestionAllPasswordsHistogram[] =
    "Autofill.UserAcceptedSuggestionAtIndex.Password.ManualFallback."
    "AllPasswords";

NSString* const kTestUsername = @"test@gmail.com";
NSString* const kTestPassword = @"strongPa55w0rd";
NSString* const kTestBackupPassword = @"backupPa55w0rd";
NSString* const kTestSiteName = @"example.com";
NSString* const kTestHost = @"www.example.com";

// Creates a saved stored credential.
password_manager::StoredCredential CreateStoredCredential() {
  password_manager::StoredCredential cred;
  cred.username_value = u"test@gmail.com";
  cred.password_value = password_manager::PasswordString(u"strongPa55w0rd");
  cred.signon_realm = std::string(kExampleURL);
  cred.in_store = PasswordForm::Store::kProfileStore;
  return cred;
}

// Creates a `ManualFillCredential` for testing cell tap actions.
ManualFillCredential* CreateManualFillCredential(bool is_backup_credential) {
  return [[ManualFillCredential alloc]
        initWithUsername:kTestUsername
                password:is_backup_credential ? kTestBackupPassword
                                              : kTestPassword
             displayName:nil
                siteName:kTestSiteName
                    host:kTestHost
                     URL:GURL(kExampleURL)
      isBackupCredential:is_backup_credential];
}

}  // namespace

// Test fixture for testing the `ManualFillCredentialsMediator` class.
class ManualFillCredentialsMediatorTest : public PlatformTest {
 protected:
  void SetUp() override {
    fake_web_state_ = std::make_unique<web::FakeWebState>();

    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        IOSChromeProfilePasswordStoreFactory::GetInstance(),
        base::BindOnce(
            &password_manager::BuildPasswordStore<ProfileIOS,
                                                  TestPasswordStore>));

    builder.AddTestingFactory(
        IOSChromeAffiliationServiceFactory::GetInstance(),
        base::BindOnce([](ProfileIOS*) -> std::unique_ptr<KeyedService> {
          return std::make_unique<affiliations::FakeAffiliationService>();
        }));

    profile_ = std::move(builder).Build();

    store_ =
        base::WrapRefCounted(static_cast<password_manager::TestPasswordStore*>(
            IOSChromeProfilePasswordStoreFactory::GetForProfile(
                profile_.get(), ServiceAccessType::EXPLICIT_ACCESS)
                .get()));

    affiliation_service_ = static_cast<affiliations::FakeAffiliationService*>(
        IOSChromeAffiliationServiceFactory::GetForProfile(profile_.get()));

    presenter_ = std::make_unique<SavedPasswordsPresenter>(
        affiliation_service_, store_, /*accont_store=*/nullptr);
    presenter_->Init();

    mediator_ = [[ManualFillCredentialsMediator alloc]
           initWithFaviconLoader:IOSChromeFaviconLoaderFactory::GetForProfile(
                                     profile_.get())
                        webState:fake_web_state_.get()
                     syncService:SyncServiceFactory::GetForProfile(
                                     profile_.get())
                             URL:GURL(kExampleURL)
        invokedOnObfuscatedField:NO
            profilePasswordStore:store_
            accountPasswordStore:nil
                    passkeyModel:nullptr
          showAutofillFormButton:YES];

    consumer_ = OCMProtocolMock(@protocol(ManualFillPasswordConsumer));
    content_injector_ =
        OCMStrictProtocolMock(@protocol(ManualFillContentInjector));
    delegate_ =
        OCMStrictProtocolMock(@protocol(ManualFillCredentialsMediatorDelegate));
    mediator_.consumer = consumer_;
    mediator_.contentInjector = content_injector_;
    mediator_.delegate = delegate_;
  }

  void TearDown() override { [mediator_ disconnect]; }

  SavedPasswordsPresenter* presenter() { return presenter_.get(); }

  ManualFillCredentialsMediator* mediator() { return mediator_; }

  OCMockObject<ManualFillPasswordConsumer>* consumer() { return consumer_; }

  OCMockObject<ManualFillContentInjector>* content_injector() {
    return content_injector_;
  }

  OCMockObject<ManualFillCredentialsMediatorDelegate>* delegate() {
    return delegate_;
  }

  TestPasswordStore& GetTestStore() {
    return *static_cast<TestPasswordStore*>(
        IOSChromeProfilePasswordStoreFactory::GetForProfile(
            profile_.get(), ServiceAccessType::EXPLICIT_ACCESS)
            .get());
  }

  void WaitUntilPasswordIsSavedToStore() {
    ASSERT_TRUE(base::test::RunUntil(
        [&]() { return GetAllLoginsSync(&GetTestStore()).size() == 1; }));
  }

  // Expects password field injection for `credential` and triggers a password
  // chip tap.
  void ExpectAndTapPasswordChip(ManualFillCredential* credential,
                                NSString* expected_password) {
    OCMExpect([content_injector() canUserInjectInPasswordField:YES
                                                 requiresHTTPS:YES])
        .andReturn(YES);
    OCMExpect(
        [delegate() manualFillCredentialsMediatorWillInjectContent:mediator()]);
    OCMExpect([content_injector()
        userDidPickContent:expected_password
             passwordField:YES
             requiresHTTPS:YES
           jumpToNextField:YES
                actionType:autofill::mojom::FieldActionType::kReplaceAll]);

    [mediator() didTapPasswordChipForCredential:credential];

    EXPECT_OCMOCK_VERIFY(delegate());
    EXPECT_OCMOCK_VERIFY(content_injector());
  }

  // Expects form autofill for `credential` with `expected_should_reauth` and
  // triggers an autofill form button tap at `kSampleCellIndex`.
  void ExpectAndTapAutofillFormButton(ManualFillCredential* credential,
                                      bool expected_should_reauth) {
    OCMExpect(
        [delegate() manualFillCredentialsMediatorWillInjectContent:mediator()]);
    OCMExpect([content_injector()
        autofillFormWithCredential:credential
                      shouldReauth:expected_should_reauth]);

    [mediator() didTapAutofillFormButtonForCredential:credential
                                            cellIndex:kSampleCellIndex];

    EXPECT_OCMOCK_VERIFY(delegate());
    EXPECT_OCMOCK_VERIFY(content_injector());
  }

 private:
  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  std::unique_ptr<web::FakeWebState> fake_web_state_;
  scoped_refptr<TestPasswordStore> store_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<SavedPasswordsPresenter> presenter_;
  OCMockObject<ManualFillPasswordConsumer>* consumer_;
  OCMockObject<ManualFillContentInjector>* content_injector_;
  OCMockObject<ManualFillCredentialsMediatorDelegate>* delegate_;
  raw_ptr<affiliations::FakeAffiliationService> affiliation_service_ = nullptr;
  ManualFillCredentialsMediator* mediator_;
};

// Tests that the consumer is notified when the passwords related to the current
// site are fetched.
TEST_F(ManualFillCredentialsMediatorTest, NotifiesConsumerOnFetchDidComplete) {
  // Set the mediator's form fetcher.
  [mediator()
      setFormFetcher:std::make_unique<password_manager::FakeFormFetcher>()];
  ASSERT_TRUE([mediator() conformsToProtocol:@protocol(FormFetcherConsumer)]);
  id<FormFetcherConsumer> form_fetcher_consumer =
      static_cast<id<FormFetcherConsumer>>(mediator());

  OCMExpect([consumer() presentCredentials:[OCMArg isNotNil]]);

  [form_fetcher_consumer fetchDidComplete];

  EXPECT_OCMOCK_VERIFY(consumer());
}

// Tests that the consumer is notified through `fetchAllPasswords` when the
// saved passwords change.
TEST_F(ManualFillCredentialsMediatorTest, NotifiesConsumerOnFetchAllPasswords) {
  // Set the mediator's saved passwords presenter.
  mediator().savedPasswordsPresenter = presenter();
  ASSERT_TRUE([mediator()
      conformsToProtocol:@protocol(SavedPasswordsPresenterObserver)]);
  id<SavedPasswordsPresenterObserver> saved_passwords_presenter_observer =
      static_cast<id<SavedPasswordsPresenterObserver>>(mediator());

  // Add password form to store so we get a result when getting the passwords
  // from the saved passwords presenter.
  password_manager::StoredCredential cred = CreateStoredCredential();
  GetTestStore().AddLogin(std::move(cred));
  WaitUntilPasswordIsSavedToStore();

  OCMExpect([consumer()
      presentCredentials:[OCMArg checkWithBlock:^(
                                     NSArray<ManualFillCredentialItem*>*
                                         credential_items) {
        EXPECT_EQ([credential_items count], 1u);
        return YES;
      }]]);

  // Call `savedPasswordsDidChange` to trigger a call to `fetchAllPasswords`.
  [saved_passwords_presenter_observer savedPasswordsDidChange];

  EXPECT_OCMOCK_VERIFY(consumer());
}

// Tests that the consumer is notified with the right credentials when a
// password form with a backup password is fetched.
TEST_F(ManualFillCredentialsMediatorTest,
       NotifiesConsumerWithBackupCredential) {
  // Set the mediator's saved passwords presenter.
  mediator().savedPasswordsPresenter = presenter();
  ASSERT_TRUE([mediator()
      conformsToProtocol:@protocol(SavedPasswordsPresenterObserver)]);
  id<SavedPasswordsPresenterObserver> saved_passwords_presenter_observer =
      static_cast<id<SavedPasswordsPresenterObserver>>(mediator());

  // Add a password form with a backup password to the store.
  password_manager::StoredCredential cred = CreateStoredCredential();
  cred.notes.emplace_back(
      password_manager::PasswordNote::kPasswordChangeBackupNoteName,
      u"backup password", base::Time::Now(), false);
  GetTestStore().AddLogin(std::move(cred));
  WaitUntilPasswordIsSavedToStore();

  auto check_credentials = ^(NSUInteger expected_count) {
    OCMExpect([consumer()
        presentCredentials:[OCMArg checkWithBlock:^BOOL(
                                       NSArray<ManualFillCredentialItem*>*
                                           credential_items) {
          EXPECT_EQ(credential_items.count, expected_count);
          return YES;
        }]]);

    // Call `savedPasswordsDidChange` to trigger a call to `fetchAllPasswords`.
    [saved_passwords_presenter_observer savedPasswordsDidChange];
  };

  check_credentials(2);

  EXPECT_OCMOCK_VERIFY(consumer());
}

// Tests that tapping the username chip injects the username, notifies the
// delegate, and logs `ManualFallback_Password_SelectUsername`.
TEST_F(ManualFillCredentialsMediatorTest, DidTapUsernameChipInjectsUsername) {
  base::UserActionTester user_action_tester;
  ManualFillCredential* credential =
      CreateManualFillCredential(/*is_backup_credential=*/false);

  OCMExpect(
      [delegate() manualFillCredentialsMediatorWillInjectContent:mediator()]);
  OCMExpect([content_injector()
      userDidPickContent:kTestUsername
           passwordField:NO
           requiresHTTPS:NO
         jumpToNextField:YES
              actionType:autofill::mojom::FieldActionType::kReplaceAll]);

  [mediator() didTapUsernameChipForCredential:credential];

  EXPECT_OCMOCK_VERIFY(delegate());
  EXPECT_OCMOCK_VERIFY(content_injector());
  EXPECT_EQ(user_action_tester.GetActionCount(kSelectUsernameAction), 1);
}

// Tests that tapping the password chip neither injects the password nor logs
// metrics when `canUserInjectInPasswordField:requiresHTTPS:` returns `NO`.
TEST_F(ManualFillCredentialsMediatorTest,
       DidTapPasswordChipRespectsInjectionCheck) {
  base::UserActionTester user_action_tester;
  ManualFillCredential* credential =
      CreateManualFillCredential(/*is_backup_credential=*/false);

  OCMExpect([content_injector() canUserInjectInPasswordField:YES
                                               requiresHTTPS:YES])
      .andReturn(NO);

  [mediator() didTapPasswordChipForCredential:credential];

  EXPECT_OCMOCK_VERIFY(delegate());
  EXPECT_OCMOCK_VERIFY(content_injector());
  EXPECT_EQ(user_action_tester.GetActionCount(kSelectPasswordAction), 0);
  EXPECT_EQ(user_action_tester.GetActionCount(kSelectBackupPasswordAction), 0);
}

// Tests that tapping the password chip injects the password and logs the
// regular or backup user action when injection is allowed.
TEST_F(ManualFillCredentialsMediatorTest, DidTapPasswordChipInjectsPassword) {
  base::UserActionTester user_action_tester;

  ExpectAndTapPasswordChip(
      CreateManualFillCredential(/*is_backup_credential=*/false),
      kTestPassword);
  EXPECT_EQ(user_action_tester.GetActionCount(kSelectPasswordAction), 1);
  EXPECT_EQ(user_action_tester.GetActionCount(kSelectBackupPasswordAction), 0);

  ExpectAndTapPasswordChip(
      CreateManualFillCredential(/*is_backup_credential=*/true),
      kTestBackupPassword);
  EXPECT_EQ(user_action_tester.GetActionCount(kSelectBackupPasswordAction), 1);
}

// Tests that tapping the autofill form button in regular manual fallback
// context requests reauthentication and logs regular and backup metrics.
TEST_F(ManualFillCredentialsMediatorTest,
       DidTapAutofillFormButtonInRegularContext) {
  base::UserActionTester user_action_tester;
  base::HistogramTester histogram_tester;
  mediator().actionSectionEnabled = YES;

  ExpectAndTapAutofillFormButton(
      CreateManualFillCredential(/*is_backup_credential=*/false),
      /*expected_should_reauth=*/true);
  EXPECT_EQ(
      user_action_tester.GetActionCount(kPasswordSuggestionAcceptedAction), 1);
  EXPECT_EQ(user_action_tester.GetActionCount(
                kBackupPasswordSuggestionAcceptedAction),
            0);

  ExpectAndTapAutofillFormButton(
      CreateManualFillCredential(/*is_backup_credential=*/true),
      /*expected_should_reauth=*/true);
  EXPECT_EQ(user_action_tester.GetActionCount(
                kBackupPasswordSuggestionAcceptedAction),
            1);
  histogram_tester.ExpectUniqueSample(kAcceptedSuggestionHistogram,
                                      kSampleCellIndex, 2);
  histogram_tester.ExpectTotalCount(kAcceptedSuggestionAllPasswordsHistogram,
                                    0);
}

// Tests that tapping the autofill form button in the All Passwords context
// skips reauthentication and logs All Passwords regular and backup metrics.
TEST_F(ManualFillCredentialsMediatorTest,
       DidTapAutofillFormButtonInAllPasswordsContext) {
  base::UserActionTester user_action_tester;
  base::HistogramTester histogram_tester;
  mediator().actionSectionEnabled = NO;

  ExpectAndTapAutofillFormButton(
      CreateManualFillCredential(/*is_backup_credential=*/false),
      /*expected_should_reauth=*/false);
  EXPECT_EQ(
      user_action_tester.GetActionCount(kAllPasswordsSuggestionAcceptedAction),
      1);
  EXPECT_EQ(user_action_tester.GetActionCount(
                kBackupAllPasswordsSuggestionAcceptedAction),
            0);

  ExpectAndTapAutofillFormButton(
      CreateManualFillCredential(/*is_backup_credential=*/true),
      /*expected_should_reauth=*/false);
  EXPECT_EQ(user_action_tester.GetActionCount(
                kBackupAllPasswordsSuggestionAcceptedAction),
            1);
  histogram_tester.ExpectUniqueSample(kAcceptedSuggestionAllPasswordsHistogram,
                                      kSampleCellIndex, 2);
  histogram_tester.ExpectTotalCount(kAcceptedSuggestionHistogram, 0);
}
