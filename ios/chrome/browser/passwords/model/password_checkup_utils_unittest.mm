// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/model/password_checkup_utils.h"

#import <string_view>

#import "base/location.h"
#import "base/test/bind.h"
#import "base/test/scoped_feature_list.h"
#import "components/affiliations/core/browser/fake_affiliation_service.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/password_manager/core/browser/password_form.h"
#import "components/password_manager/core/browser/password_manager_test_utils.h"
#import "components/password_manager/core/browser/password_store/test_password_store.h"
#import "components/password_manager/core/common/password_manager_pref_names.h"
#import "components/prefs/testing_pref_service.h"
#import "ios/chrome/browser/affiliations/model/ios_chrome_affiliation_service_factory.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_password_check_manager.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_password_check_manager_factory.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_profile_password_store_factory.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

using password_manager::CredentialUIEntry;
using password_manager::InsecureType;
using password_manager::PasswordForm;
using password_manager::TestPasswordStore;
using ::testing::UnorderedElementsAre;

namespace {

constexpr char kExampleCom1[] = "https://example1.com";
constexpr char kExampleCom2[] = "https://example2.com";
constexpr char kExampleCom3[] = "https://example3.com";
constexpr char kExampleCom4[] = "https://example4.com";
constexpr char kExampleCom5[] = "https://example5.com";
constexpr char kExampleCom6[] = "https://example6.com";
constexpr char kExampleCom7[] = "https://example7.com";
constexpr char kExampleCom8[] = "https://example8.com";
constexpr char kExampleCom9[] = "https://example9.com";
constexpr char kExampleCom10[] = "https://example10.com";

constexpr char16_t kUsername116[] = u"alice";
constexpr char16_t kPassword116[] = u"strongPa55w0rd!1";
constexpr char16_t kPassword216[] = u"strongPa55w0rd!2";
constexpr char16_t kPassword316[] = u"strongPa55w0rd!3";
constexpr char16_t kPassword416[] = u"strongPa55w0rd!4";
constexpr char16_t kPassword516[] = u"strongPa55w0rd!5";
constexpr char16_t kPassword616[] = u"strongPa55w0rd!6";
constexpr char16_t kPassword716[] = u"strongPa55w0rd!7";
constexpr char16_t kPassword816[] = u"strongPa55w0rd!8";
constexpr char16_t kPassword916[] = u"strongPa55w0rd!9";
constexpr char16_t kPassword1016[] = u"strongPa55w0rd!10";

using password_manager::CredentialUIEntry;
using password_manager::FormatElapsedTimeSinceLastCheck;
using password_manager::InsecureType;
using password_manager::PasswordForm;
using password_manager::TestPasswordStore;
using password_manager::WarningType;

PasswordForm MakeSavedPassword(std::string_view signon_realm,
                               std::u16string_view password) {
  PasswordForm form;
  form.url = GURL(signon_realm);
  form.signon_realm = std::string(signon_realm);
  form.username_value = std::u16string(kUsername116);
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

class PasswordCheckupUtilsTest : public PlatformTest {
 protected:
  PasswordCheckupUtilsTest() {
    TestProfileIOS::Builder builder;
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
    store_ =
        base::WrapRefCounted(static_cast<password_manager::TestPasswordStore*>(
            IOSChromeProfilePasswordStoreFactory::GetForProfile(
                profile_.get(), ServiceAccessType::EXPLICIT_ACCESS)
                .get()));
    manager_ =
        IOSChromePasswordCheckManagerFactory::GetForProfile(profile_.get());
  }

  void RunUntilIdle() { task_env_.RunUntilIdle(); }

  ProfileIOS* profile() { return profile_.get(); }
  TestPasswordStore& store() { return *store_; }
  IOSChromePasswordCheckManager& manager() { return *manager_; }

 private:
  base::test::ScopedFeatureList feature_list_;
  web::WebTaskEnvironment task_env_;
  std::unique_ptr<ProfileIOS> profile_;
  scoped_refptr<TestPasswordStore> store_;
  scoped_refptr<IOSChromePasswordCheckManager> manager_;
};

}  // namespace

// Tests that the correct warning type is returned.
TEST_F(PasswordCheckupUtilsTest, CheckHighestPriorityWarningType) {
  std::vector<CredentialUIEntry> insecure_credentials;
  // The "no insecure passwords" warning is the highest priority warning.
  EXPECT_THAT(GetWarningOfHighestPriority(insecure_credentials),
              WarningType::kNoInsecurePasswordsWarning);

  // Add a muted password.
  PasswordForm form1 = MakeSavedPassword(kExampleCom1, kPassword116);
  AddIssueToForm(&form1, InsecureType::kLeaked, base::Minutes(1),
                 /*is_muted=*/true);
  insecure_credentials.emplace_back(form1);

  // The "dismissed warnings" warning becomes the highest priority warning.
  EXPECT_THAT(GetWarningOfHighestPriority(insecure_credentials),
              WarningType::kDismissedWarningsWarning);

  // Add a muted password that is also weak.
  PasswordForm form2 = MakeSavedPassword(kExampleCom2, kPassword216);
  AddIssueToForm(&form2, InsecureType::kLeaked, base::Minutes(1),
                 /*is_muted=*/true);
  AddIssueToForm(&form2, InsecureType::kWeak, base::Minutes(1));
  insecure_credentials.emplace_back(form2);
  // The "weak passwords" warning becomes the highest priority warning.
  EXPECT_THAT(GetWarningOfHighestPriority(insecure_credentials),
              WarningType::kWeakPasswordsWarning);

  // Add a weak password.
  PasswordForm form3 = MakeSavedPassword(kExampleCom3, kPassword316);
  AddIssueToForm(&form3, InsecureType::kWeak, base::Minutes(1));
  insecure_credentials.emplace_back(form3);
  // The "weak passwords" warning stays the highest priority warning.
  EXPECT_THAT(GetWarningOfHighestPriority(insecure_credentials),
              WarningType::kWeakPasswordsWarning);

  // Add 2 reused passwords.
  PasswordForm form4 = MakeSavedPassword(kExampleCom4, kPassword416);
  AddIssueToForm(&form4, InsecureType::kReused, base::Minutes(1));
  PasswordForm form5 = MakeSavedPassword(kExampleCom5, kPassword516);
  AddIssueToForm(&form5, InsecureType::kReused, base::Minutes(1));
  insecure_credentials.emplace_back(form4);
  insecure_credentials.emplace_back(form5);
  // The "reused passwords" warning becomes the highest priority warning.
  EXPECT_THAT(GetWarningOfHighestPriority(insecure_credentials),
              WarningType::kReusedPasswordsWarning);

  // Add an unmuted compromised password.
  PasswordForm form6 = MakeSavedPassword(kExampleCom6, kPassword616);
  AddIssueToForm(&form6, InsecureType::kLeaked, base::Minutes(1));
  insecure_credentials.emplace_back(form6);
  // The "compromised passwords" warning becomes the highest priority warning.
  EXPECT_THAT(GetWarningOfHighestPriority(insecure_credentials),
              WarningType::kCompromisedPasswordsWarning);
}

// Tests that the correct number of saved passwords is returned depending on the
// warning type of highest priority.
TEST_F(PasswordCheckupUtilsTest, CheckPasswordCountForWarningType) {
  std::vector<CredentialUIEntry> insecure_credentials;
  WarningType warning_type = GetWarningOfHighestPriority(insecure_credentials);
  EXPECT_EQ(GetPasswordCountForWarningType(warning_type, insecure_credentials),
            0);

  // Add a muted password.
  PasswordForm form1 = MakeSavedPassword(kExampleCom1, kPassword116);
  AddIssueToForm(&form1, InsecureType::kLeaked, base::Minutes(1),
                 /*is_muted=*/true);
  insecure_credentials.emplace_back(form1);
  warning_type = GetWarningOfHighestPriority(insecure_credentials);
  // The number of passwords for which the compromised warning was dismissed
  // should be returned.
  EXPECT_EQ(GetPasswordCountForWarningType(warning_type, insecure_credentials),
            1);

  // Add 2 weak passwords.
  PasswordForm form2 = MakeSavedPassword(kExampleCom2, kPassword216);
  PasswordForm form3 = MakeSavedPassword(kExampleCom3, kPassword316);
  AddIssueToForm(&form2, InsecureType::kWeak, base::Minutes(1));
  AddIssueToForm(&form3, InsecureType::kWeak, base::Minutes(1));
  insecure_credentials.emplace_back(form2);
  insecure_credentials.emplace_back(form3);
  warning_type = GetWarningOfHighestPriority(insecure_credentials);
  // The number of weak passwords should be returned.
  EXPECT_EQ(GetPasswordCountForWarningType(warning_type, insecure_credentials),
            2);

  // Add 3 reused passwords.
  PasswordForm form4 = MakeSavedPassword(kExampleCom4, kPassword416);
  PasswordForm form5 = MakeSavedPassword(kExampleCom5, kPassword516);
  PasswordForm form6 = MakeSavedPassword(kExampleCom6, kPassword616);
  AddIssueToForm(&form4, InsecureType::kReused, base::Minutes(1));
  AddIssueToForm(&form5, InsecureType::kReused, base::Minutes(1));
  AddIssueToForm(&form6, InsecureType::kReused, base::Minutes(1));
  insecure_credentials.emplace_back(form4);
  insecure_credentials.emplace_back(form5);
  insecure_credentials.emplace_back(form6);
  warning_type = GetWarningOfHighestPriority(insecure_credentials);
  // The number of reused passwords should be returned.
  EXPECT_EQ(GetPasswordCountForWarningType(warning_type, insecure_credentials),
            3);

  // Add 4 unmuted compromised passwords.
  PasswordForm form7 = MakeSavedPassword(kExampleCom7, kPassword716);
  PasswordForm form8 = MakeSavedPassword(kExampleCom8, kPassword816);
  PasswordForm form9 = MakeSavedPassword(kExampleCom9, kPassword916);
  PasswordForm form10 = MakeSavedPassword(kExampleCom10, kPassword1016);
  AddIssueToForm(&form7, InsecureType::kLeaked, base::Minutes(1));
  AddIssueToForm(&form8, InsecureType::kLeaked, base::Minutes(1));
  AddIssueToForm(&form9, InsecureType::kLeaked, base::Minutes(1));
  AddIssueToForm(&form10, InsecureType::kLeaked, base::Minutes(1));
  insecure_credentials.emplace_back(form7);
  insecure_credentials.emplace_back(form8);
  insecure_credentials.emplace_back(form9);
  insecure_credentials.emplace_back(form10);
  warning_type = GetWarningOfHighestPriority(insecure_credentials);
  // The number of compromised passwords should be returned.
  EXPECT_EQ(GetPasswordCountForWarningType(warning_type, insecure_credentials),
            4);
}

// Tests that the correct string is returned with the right timestamp.
TEST_F(PasswordCheckupUtilsTest, ElapsedTimeSinceLastCheck) {
  EXPECT_NSEQ(@"Check never run", FormatElapsedTimeSinceLastCheck(
                                      manager().GetLastPasswordCheckTime()));

  base::Time expected1 = base::Time::Now() - base::Seconds(10);
  profile()->GetPrefs()->SetDouble(
      password_manager::prefs::kLastTimePasswordCheckCompleted,
      expected1.InSecondsFSinceUnixEpoch());

  EXPECT_NSEQ(@"Checked just now", FormatElapsedTimeSinceLastCheck(
                                       manager().GetLastPasswordCheckTime()));

  base::Time expected2 = base::Time::Now() - base::Minutes(5);
  profile()->GetPrefs()->SetDouble(
      password_manager::prefs::kLastTimePasswordCheckCompleted,
      expected2.InSecondsFSinceUnixEpoch());

  EXPECT_NSEQ(
      @"Checked 5 minutes ago",
      FormatElapsedTimeSinceLastCheck(manager().GetLastPasswordCheckTime()));
}

// Tests that the correct passwords are returned for each warning type.
TEST_F(PasswordCheckupUtilsTest, CheckPasswordsForWarningType) {
  // Add a muted password.
  PasswordForm muted_form = MakeSavedPassword(kExampleCom1, kPassword116);
  AddIssueToForm(&muted_form, InsecureType::kLeaked, base::Minutes(1),
                 /*is_muted=*/true);
  store().AddLogin(muted_form);

  // Add a weak password.
  PasswordForm weak_form = MakeSavedPassword(kExampleCom2, kPassword216);
  AddIssueToForm(&weak_form, InsecureType::kWeak, base::Minutes(1));
  store().AddLogin(weak_form);

  // Add 2 reused passwords.
  PasswordForm reused_form1 = MakeSavedPassword(kExampleCom3, kPassword316);
  AddIssueToForm(&reused_form1, InsecureType::kReused, base::Minutes(1));
  store().AddLogin(reused_form1);

  PasswordForm reused_form2 = MakeSavedPassword(kExampleCom4, kPassword416);
  AddIssueToForm(&reused_form2, InsecureType::kReused, base::Minutes(1));
  store().AddLogin(reused_form2);

  // Add two unmuted compromised passwords, a leaked one and a phished one.
  PasswordForm leaked_form = MakeSavedPassword(kExampleCom5, kPassword516);
  AddIssueToForm(&leaked_form, InsecureType::kLeaked, base::Minutes(1));
  store().AddLogin(leaked_form);

  PasswordForm phished_form = MakeSavedPassword(kExampleCom6, kPassword616);
  AddIssueToForm(&phished_form, InsecureType::kPhished, base::Minutes(1));
  store().AddLogin(phished_form);

  RunUntilIdle();

  std::vector<CredentialUIEntry> insecure_credentials =
      manager().GetInsecureCredentials();

  std::vector<CredentialUIEntry> filtered_credentials;

  // Verify Dismissed Passwords.
  filtered_credentials = GetPasswordsForWarningType(
      WarningType::kDismissedWarningsWarning, insecure_credentials);
  EXPECT_THAT(filtered_credentials,
              UnorderedElementsAre(CredentialUIEntry(muted_form)));

  // Verify Compromised Passwords.
  filtered_credentials = GetPasswordsForWarningType(
      WarningType::kCompromisedPasswordsWarning, insecure_credentials);
  EXPECT_THAT(filtered_credentials,
              UnorderedElementsAre(CredentialUIEntry(leaked_form),
                                   CredentialUIEntry(phished_form)));

  // Verify Weak Passwords.
  filtered_credentials = GetPasswordsForWarningType(
      WarningType::kWeakPasswordsWarning, insecure_credentials);
  EXPECT_THAT(filtered_credentials,
              UnorderedElementsAre(CredentialUIEntry(weak_form)));

  // Verify Reused Passwords.
  filtered_credentials = GetPasswordsForWarningType(
      WarningType::kReusedPasswordsWarning, insecure_credentials);
  EXPECT_THAT(filtered_credentials,
              UnorderedElementsAre(CredentialUIEntry(reused_form1),
                                   CredentialUIEntry(reused_form2)));
}

// Tests that `CountInsecurePasswordsPerInsecureType` doesn't take into account
// a password marked as reused if there is no other credential with the same
// password.
TEST_F(PasswordCheckupUtilsTest,
       CheckInsecurePasswordCountWhenOneReusedPassword) {
  // Add reused passwords.
  PasswordForm reused_form1 = MakeSavedPassword(kExampleCom1, kPassword116);
  PasswordForm reused_form2 = MakeSavedPassword(kExampleCom2, kPassword116);
  store().AddLogin(reused_form1);
  store().AddLogin(reused_form2);
  RunUntilIdle();

  // Run password check.
  manager().StartPasswordCheck(
      password_manager::LeakDetectionInitiator::kBulkSyncedPasswordsCheck);
  RunUntilIdle();

  // Remove one of the reused passwords.
  store().RemoveLogin(FROM_HERE, reused_form1);
  RunUntilIdle();

  std::vector<CredentialUIEntry> insecure_credentials =
      manager().GetInsecureCredentials();

  EXPECT_EQ(
      CountInsecurePasswordsPerInsecureType(insecure_credentials).reused_count,
      0);
}
