// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/model/ios_chrome_password_check_manager.h"

#import <memory>
#import <string>
#import <string_view>
#import <vector>

#import "base/functional/bind.h"
#import "base/memory/raw_ptr.h"
#import "base/memory/scoped_refptr.h"
#import "base/strings/strcat.h"
#import "base/strings/string_number_conversions.h"
#import "base/strings/string_util.h"
#import "base/strings/utf_string_conversions.h"
#import "base/test/bind.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/time/time.h"
#import "components/affiliations/core/browser/fake_affiliation_service.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/password_manager/core/browser/leak_detection/bulk_leak_check_service.h"
#import "components/password_manager/core/browser/leak_detection/mock_bulk_leak_check_service.h"
#import "components/password_manager/core/browser/password_form.h"
#import "components/password_manager/core/browser/password_manager_test_utils.h"
#import "components/password_manager/core/browser/password_store/test_password_store.h"
#import "components/password_manager/core/common/password_manager_features.h"
#import "components/password_manager/core/common/password_manager_pref_names.h"
#import "components/prefs/pref_registry_simple.h"
#import "components/prefs/testing_pref_service.h"
#import "ios/chrome/browser/affiliations/model/ios_chrome_affiliation_service_factory.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_bulk_leak_check_service_factory.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_password_check_manager_factory.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_profile_password_store_factory.h"
#import "ios/chrome/browser/passwords/model/password_checkup_metrics.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {
constexpr char kExampleCom1[] = "https://example1.com";
constexpr char kExampleCom2[] = "https://example2.com";

constexpr char16_t kUsername116[] = u"alice";
constexpr char16_t kUsername216[] = u"bob";

constexpr char16_t kPassword116[] = u"strongPa55w0rd!1";
constexpr char16_t kPassword216[] = u"strongPa55w0rd!2";
constexpr char16_t kWeakPassword[] = u"123456";

using password_manager::BulkLeakCheckServiceInterface;
using password_manager::CredentialUIEntry;
using password_manager::InsecureType;
using password_manager::IsLeaked;
using password_manager::LeakCheckCredential;
using password_manager::MockBulkLeakCheckService;
using password_manager::PasswordForm;
using password_manager::TestPasswordStore;
using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::Field;
using ::testing::IsEmpty;
using ::testing::Pair;
using ::testing::StrictMock;
using ::testing::UnorderedElementsAre;

struct MockPasswordCheckManagerObserver
    : IOSChromePasswordCheckManager::Observer {
  MOCK_METHOD(void, InsecureCredentialsChanged, (), (override));
  MOCK_METHOD(void,
              PasswordCheckStatusChanged,
              (PasswordCheckState),
              (override));
  MOCK_METHOD(void,
              ManagerWillShutdown,
              (IOSChromePasswordCheckManager*),
              (override));
};

std::unique_ptr<KeyedService> MakeMockPasswordCheckManagerObserver(
    web::BrowserState*) {
  return std::make_unique<MockBulkLeakCheckService>();
}

PasswordForm MakeSavedPassword(std::string_view signon_realm,
                               std::u16string_view username,
                               std::u16string_view password = kPassword116) {
  PasswordForm form;
  form.url = GURL(signon_realm);
  form.signon_realm = std::string(signon_realm);
  form.username_value = std::u16string(username);
  form.password_value = std::u16string(password);
  form.in_store = PasswordForm::Store::kProfileStore;
  // TODO(crbug.com/40774419): Once all places that operate changes on forms
  // via UpdateLogin properly set `password_issues`, setting them to an empty
  // map should be part of the default constructor.
  form.password_issues =
      base::flat_map<InsecureType, password_manager::InsecurityMetadata>();
  return form;
}

void AddIssueToForm(PasswordForm* form,
                    InsecureType type = InsecureType::kLeaked,
                    base::TimeDelta time_since_creation = base::TimeDelta(),
                    bool is_muted = false) {
  form->password_issues.insert_or_assign(
      type, password_manager::InsecurityMetadata(
                base::Time::Now() - time_since_creation,
                password_manager::IsMuted(is_muted),
                password_manager::TriggerBackendNotification(false)));
}

class IOSChromePasswordCheckManagerTest : public PlatformTest {
 public:
  IOSChromePasswordCheckManagerTest() {
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        IOSChromeBulkLeakCheckServiceFactory::GetInstance(),
        base::BindRepeating(&MakeMockPasswordCheckManagerObserver));
    builder.AddTestingFactory(
        IOSChromeProfilePasswordStoreFactory::GetInstance(),
        base::BindRepeating(
            &password_manager::BuildPasswordStore<web::BrowserState,
                                                  TestPasswordStore>));
    builder.AddTestingFactory(
        IOSChromeAffiliationServiceFactory::GetInstance(),
        base::BindRepeating(base::BindLambdaForTesting([](web::BrowserState*) {
          return std::unique_ptr<KeyedService>(
              std::make_unique<affiliations::FakeAffiliationService>());
        })));

    profile_ = std::move(builder).Build();
    bulk_leak_check_service_ = static_cast<MockBulkLeakCheckService*>(
        IOSChromeBulkLeakCheckServiceFactory::GetForProfile(profile_.get()));
    store_ =
        base::WrapRefCounted(static_cast<password_manager::TestPasswordStore*>(
            IOSChromeProfilePasswordStoreFactory::GetForProfile(
                profile_.get(), ServiceAccessType::EXPLICIT_ACCESS)
                .get()));
    manager_ =
        IOSChromePasswordCheckManagerFactory::GetForProfile(profile_.get());
  }

  void RunUntilIdle() { task_env_.RunUntilIdle(); }

  void FastForwardBy(base::TimeDelta time) { task_env_.FastForwardBy(time); }

  ProfileIOS* profile() { return profile_.get(); }
  TestPasswordStore& store() { return *store_; }
  MockBulkLeakCheckService* service() { return bulk_leak_check_service_; }
  IOSChromePasswordCheckManager& manager() { return *manager_; }

 private:
  web::WebTaskEnvironment task_env_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<ProfileIOS> profile_;
  raw_ptr<MockBulkLeakCheckService> bulk_leak_check_service_;
  scoped_refptr<TestPasswordStore> store_;
  scoped_refptr<IOSChromePasswordCheckManager> manager_;
};
}  // namespace

// Sets up the password store with a password and unmuted compromised
// credential. Verifies that the result is matching expectation.
TEST_F(IOSChromePasswordCheckManagerTest, GetInsecureCredentials) {
  PasswordForm form = MakeSavedPassword(kExampleCom1, kUsername116);
  AddIssueToForm(&form, InsecureType::kLeaked, base::Minutes(1));
  store().AddLogin(form);
  RunUntilIdle();

  EXPECT_THAT(manager().GetInsecureCredentials(),
              ElementsAre(CredentialUIEntry(form)));
}

// Test that we don't create an entry in the password store if IsLeaked is
// false.
TEST_F(IOSChromePasswordCheckManagerTest, NoLeakedFound) {
  store().AddLogin(MakeSavedPassword(kExampleCom1, kUsername116, kPassword116));
  RunUntilIdle();

  static_cast<BulkLeakCheckServiceInterface::Observer*>(&manager())
      ->OnCredentialDone(LeakCheckCredential(kUsername116, kPassword116),
                         IsLeaked(false));
  RunUntilIdle();

  EXPECT_THAT(manager().GetInsecureCredentials(), IsEmpty());
}

// Test that a found leak creates a compromised credential in the password
// store.
TEST_F(IOSChromePasswordCheckManagerTest, OnLeakFoundCreatesCredential) {
  PasswordForm form =
      MakeSavedPassword(kExampleCom1, kUsername116, kPassword116);
  store().AddLogin(form);
  RunUntilIdle();

  static_cast<BulkLeakCheckServiceInterface::Observer*>(&manager())
      ->OnCredentialDone(LeakCheckCredential(kUsername116, kPassword116),
                         IsLeaked(true));
  RunUntilIdle();

  AddIssueToForm(&form, InsecureType::kLeaked, base::Minutes(0));
  EXPECT_THAT(manager().GetInsecureCredentials(),
              ElementsAre(CredentialUIEntry(form)));
}

// Verifies that the case where the user has no saved passwords is reported
// correctly.
TEST_F(IOSChromePasswordCheckManagerTest, GetPasswordCheckStatusNoPasswords) {
  EXPECT_EQ(PasswordCheckState::kNoPasswords,
            manager().GetPasswordCheckState());
}

// Verifies that the case where the user has saved passwords is reported
// correctly.
TEST_F(IOSChromePasswordCheckManagerTest, GetPasswordCheckStatusIdle) {
  store().AddLogin(MakeSavedPassword(kExampleCom1, kUsername116));
  RunUntilIdle();

  EXPECT_EQ(PasswordCheckState::kIdle, manager().GetPasswordCheckState());
}

// Checks that the default kLastTimePasswordCheckCompleted pref value is
// treated as no completed run yet.
TEST_F(IOSChromePasswordCheckManagerTest,
       LastTimePasswordCheckCompletedNotSet) {
  EXPECT_FALSE(manager().GetLastPasswordCheckTime().has_value());
}

// Checks that a transition into the idle state after starting a check results
// in resetting the kLastTimePasswordCheckCompleted pref to the current time.
TEST_F(IOSChromePasswordCheckManagerTest, LastTimePasswordCheckCompletedReset) {
  FastForwardBy(base::Days(1));

  manager().StartPasswordCheck(
      password_manager::LeakDetectionInitiator::kIosProactivePasswordCheckup);
  RunUntilIdle();

  static_cast<BulkLeakCheckServiceInterface::Observer*>(&manager())
      ->OnStateChanged(BulkLeakCheckServiceInterface::State::kIdle);

  EXPECT_NE(base::Time(), manager().GetLastPasswordCheckTime());
}

// Checks that insecure credential count metrics are logged after a check has
// finished.
TEST_F(IOSChromePasswordCheckManagerTest, InsecureCredentialCountsMetrics) {
  base::HistogramTester histogram_tester;

  PasswordForm leaked_form = MakeSavedPassword(kExampleCom1, kUsername116);
  AddIssueToForm(&leaked_form, InsecureType::kLeaked, base::Minutes(1));
  store().AddLogin(leaked_form);

  PasswordForm phished_form =
      MakeSavedPassword("https://site1.com", kUsername116);
  AddIssueToForm(&phished_form, InsecureType::kPhished, base::Minutes(1));
  store().AddLogin(phished_form);

  PasswordForm weak_form = MakeSavedPassword("https://site1.com", kUsername116);
  AddIssueToForm(&weak_form, InsecureType::kWeak, base::Minutes(1));
  store().AddLogin(weak_form);

  PasswordForm reused_form =
      MakeSavedPassword("https://site2.com", kUsername116);
  AddIssueToForm(&reused_form, InsecureType::kReused, base::Minutes(1));
  store().AddLogin(reused_form);

  // Adding a muted warning. This shouldn't be counted in the unmuted histogram.
  PasswordForm muted_form =
      MakeSavedPassword("https://site32.com", kUsername216, kPassword216);
  AddIssueToForm(&muted_form, InsecureType::kLeaked, base::Minutes(1),
                 /*is_muted=*/true);
  store().AddLogin(muted_form);

  manager().StartPasswordCheck(
      password_manager::LeakDetectionInitiator::kIosProactivePasswordCheckup);
  RunUntilIdle();

  static_cast<BulkLeakCheckServiceInterface::Observer*>(&manager())
      ->OnStateChanged(BulkLeakCheckServiceInterface::State::kIdle);

  EXPECT_EQ(histogram_tester.GetTotalSum(
                password_manager::kInsecureCredentialsCountHistogram),
            2);
  EXPECT_EQ(histogram_tester.GetTotalSum(
                password_manager::kUnmutedInsecureCredentialsCountHistogram),
            1);
}

// Tests whether adding and removing an observer works as expected.
TEST_F(IOSChromePasswordCheckManagerTest,
       NotifyObserversAboutInsecureCredentialChanges) {
  PasswordForm form = MakeSavedPassword(kExampleCom1, kUsername116);
  store().AddLogin(form);
  RunUntilIdle();

  StrictMock<MockPasswordCheckManagerObserver> observer;
  manager().AddObserver(&observer);

  // Adding a compromised credential should notify observers.
  EXPECT_CALL(observer, PasswordCheckStatusChanged);
  EXPECT_CALL(observer, InsecureCredentialsChanged);
  AddIssueToForm(&form, InsecureType::kLeaked, base::Minutes(1));
  store().UpdateLogin(form);
  RunUntilIdle();

  // After an observer is removed it should no longer receive notifications.
  manager().RemoveObserver(&observer);
  EXPECT_CALL(observer, InsecureCredentialsChanged).Times(0);
  AddIssueToForm(&form, InsecureType::kPhished, base::Minutes(1));
  store().UpdateLogin(form);
  RunUntilIdle();
}

// Tests whether adding and removing an observer works as expected.
TEST_F(IOSChromePasswordCheckManagerTest, NotifyObserversAboutStateChanges) {
  store().AddLogin(MakeSavedPassword(kExampleCom1, kUsername116));
  RunUntilIdle();
  StrictMock<MockPasswordCheckManagerObserver> observer;
  manager().AddObserver(&observer);

  EXPECT_CALL(observer, PasswordCheckStatusChanged(PasswordCheckState::kIdle));
  static_cast<BulkLeakCheckServiceInterface::Observer*>(&manager())
      ->OnStateChanged(BulkLeakCheckServiceInterface::State::kIdle);

  RunUntilIdle();

  EXPECT_EQ(PasswordCheckState::kIdle, manager().GetPasswordCheckState());

  // After an observer is removed it should no longer receive notifications.
  manager().RemoveObserver(&observer);
  EXPECT_CALL(observer, PasswordCheckStatusChanged).Times(0);
  static_cast<BulkLeakCheckServiceInterface::Observer*>(&manager())
      ->OnStateChanged(BulkLeakCheckServiceInterface::State::kRunning);
  RunUntilIdle();
}

// Tests expected delay is being added.
TEST_F(IOSChromePasswordCheckManagerTest, CheckFinishedWithDelay) {
  store().AddLogin(MakeSavedPassword(kExampleCom1, kUsername116));

  RunUntilIdle();
  StrictMock<MockPasswordCheckManagerObserver> observer;
  manager().AddObserver(&observer);

  EXPECT_CALL(observer, InsecureCredentialsChanged).Times(2);
  EXPECT_CALL(observer, PasswordCheckStatusChanged(PasswordCheckState::kIdle))
      .Times(2);
  manager().StartPasswordCheck(
      password_manager::LeakDetectionInitiator::kIosProactivePasswordCheckup);
  RunUntilIdle();

  static_cast<BulkLeakCheckServiceInterface::Observer*>(&manager())
      ->OnStateChanged(BulkLeakCheckServiceInterface::State::kIdle);

  // Validate the minimum password check duration of 3 seconds is respected.
  // The test will fail if any PasswordCheckStatusChanged calls are observed in
  // the first 2 seconds after the check was started.
  FastForwardBy(base::Seconds(2));
  // After the minimum delay passes, the check status update should be received.
  EXPECT_CALL(observer, PasswordCheckStatusChanged(PasswordCheckState::kIdle));
  // Advance the clock 1 more second simulating that 3 seconds have passed so
  // the check status update should have been received.
  FastForwardBy(base::Seconds(1));

  manager().RemoveObserver(&observer);
}

// Verify that GetInsecureCredentials returns weak credentials.
TEST_F(IOSChromePasswordCheckManagerTest, WeakCredentialsAreReturned) {
  PasswordForm weak_form =
      MakeSavedPassword(kExampleCom1, kUsername116, kWeakPassword);
  store().AddLogin(weak_form);

  RunUntilIdle();
  manager().StartPasswordCheck(
      password_manager::LeakDetectionInitiator::kIosProactivePasswordCheckup);
  RunUntilIdle();

  EXPECT_THAT(manager().GetInsecureCredentials(),
              ElementsAre(CredentialUIEntry(weak_form)));
}

// Verify that GetInsecureCredentials returns reused credentials.
TEST_F(IOSChromePasswordCheckManagerTest, ReusedCredentialsAreReturned) {
  PasswordForm form_with_same_password_1 =
      MakeSavedPassword(kExampleCom1, kUsername116, kPassword116);
  store().AddLogin(form_with_same_password_1);

  PasswordForm form_with_same_password_2 =
      MakeSavedPassword(kExampleCom2, kUsername216, kPassword116);
  store().AddLogin(form_with_same_password_2);

  RunUntilIdle();
  manager().StartPasswordCheck(
      password_manager::LeakDetectionInitiator::kIosProactivePasswordCheckup);
  RunUntilIdle();

  std::vector<CredentialUIEntry> insecure_credentials =
      manager().GetInsecureCredentials();

  EXPECT_THAT(
      insecure_credentials,
      UnorderedElementsAre(CredentialUIEntry(form_with_same_password_1),
                           CredentialUIEntry(form_with_same_password_2)));
  EXPECT_TRUE(insecure_credentials[0].IsReused());
  EXPECT_TRUE(insecure_credentials[1].IsReused());
}
