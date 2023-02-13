// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_checkup/password_checkup_utils.h"

#import "base/strings/string_piece.h"
#import "base/test/scoped_feature_list.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/password_manager/core/browser/password_form.h"
#import "components/password_manager/core/browser/password_manager_test_utils.h"
#import "components/password_manager/core/browser/test_password_store.h"
#import "components/password_manager/core/common/password_manager_features.h"
#import "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/passwords/ios_chrome_password_check_manager.h"
#import "ios/chrome/browser/passwords/ios_chrome_password_check_manager_factory.h"
#import "ios/chrome/browser/passwords/ios_chrome_password_store_factory.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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
constexpr char16_t kPassword116[] = u"s3cre3t";

using password_manager::CredentialUIEntry;
using password_manager::InsecureType;
using password_manager::PasswordForm;
using password_manager::TestPasswordStore;

PasswordForm MakeSavedPassword(
    base::StringPiece signon_realm,
    base::StringPiece16 username,
    base::StringPiece16 password = kPassword116,
    base::StringPiece16 username_element = base::StringPiece16()) {
  PasswordForm form;
  form.url = GURL(signon_realm);
  form.signon_realm = std::string(signon_realm);
  form.username_value = std::u16string(username);
  form.password_value = std::u16string(password);
  form.username_element = std::u16string(username_element);
  form.in_store = PasswordForm::Store::kProfileStore;
  // TODO(crbug.com/1223022): Once all places that operate changes on forms
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
                password_manager::IsMuted(is_muted)));
}

class PasswordCheckupUtilsTest : public PlatformTest {
 protected:
  PasswordCheckupUtilsTest() {
    TestChromeBrowserState::Builder builder;
    builder.AddTestingFactory(
        IOSChromePasswordStoreFactory::GetInstance(),
        base::BindRepeating(
            &password_manager::BuildPasswordStore<web::BrowserState,
                                                  TestPasswordStore>));
    browser_state_ = builder.Build();
    store_ =
        base::WrapRefCounted(static_cast<password_manager::TestPasswordStore*>(
            IOSChromePasswordStoreFactory::GetForBrowserState(
                browser_state_.get(), ServiceAccessType::EXPLICIT_ACCESS)
                .get()));
    manager_ = IOSChromePasswordCheckManagerFactory::GetForBrowserState(
        browser_state_.get());
  }

  void RunUntilIdle() { task_env_.RunUntilIdle(); }

  ChromeBrowserState* browser_state() { return browser_state_.get(); }
  TestPasswordStore& store() { return *store_; }
  IOSChromePasswordCheckManager& manager() { return *manager_; }

 private:
  web::WebTaskEnvironment task_env_;
  std::unique_ptr<ChromeBrowserState> browser_state_;
  scoped_refptr<TestPasswordStore> store_;
  scoped_refptr<IOSChromePasswordCheckManager> manager_;
};

}  // namespace

// Tests that the correct warning type is returned.
TEST_F(PasswordCheckupUtilsTest, CheckHighestPriorityWarningType) {
  // Enable Password Checkup feature.
  base::test::ScopedFeatureList featureList;
  featureList.InitAndEnableFeature(
      password_manager::features::kIOSPasswordCheckup);

  std::vector<CredentialUIEntry> insecure_credentials =
      manager().GetInsecureCredentials();
  // The "no insecure passwords" warning is the highest priority warning.
  EXPECT_THAT(GetWarningOfHighestPriority(insecure_credentials),
              WarningType::kNoInsecurePasswordsWarning);

  // Add a muted password.
  PasswordForm form1 = MakeSavedPassword(kExampleCom1, kUsername116);
  AddIssueToForm(&form1, InsecureType::kLeaked, base::Minutes(1),
                 /*is_muted=*/true);
  store().AddLogin(form1);
  RunUntilIdle();
  insecure_credentials = manager().GetInsecureCredentials();
  // The "dismissed warnings" warning becomes the highest priority warning.
  EXPECT_THAT(GetWarningOfHighestPriority(insecure_credentials),
              WarningType::kDismissedWarningsWarning);

  // Add a weak password.
  PasswordForm form2 = MakeSavedPassword(kExampleCom2, kUsername116);
  AddIssueToForm(&form2, InsecureType::kWeak, base::Minutes(1));
  store().AddLogin(form2);
  RunUntilIdle();
  insecure_credentials = manager().GetInsecureCredentials();
  // The "weak passwords" warning becomes the highest priority warning.
  EXPECT_THAT(GetWarningOfHighestPriority(insecure_credentials),
              WarningType::kWeakPasswordsWarning);

  // Add a reused password.
  PasswordForm form3 = MakeSavedPassword(kExampleCom3, kUsername116);
  AddIssueToForm(&form3, InsecureType::kReused, base::Minutes(1));
  store().AddLogin(form3);
  RunUntilIdle();
  insecure_credentials = manager().GetInsecureCredentials();
  // The "reused passwords" warning becomes the highest priority warning.
  EXPECT_THAT(GetWarningOfHighestPriority(insecure_credentials),
              WarningType::kReusedPasswordsWarning);

  // Add an unmuted compromised password.
  PasswordForm form4 = MakeSavedPassword(kExampleCom4, kUsername116);
  AddIssueToForm(&form4, InsecureType::kLeaked, base::Minutes(1));
  store().AddLogin(form4);
  RunUntilIdle();
  insecure_credentials = manager().GetInsecureCredentials();
  // The "compromised passwords" warning becomes the highest priority warning.
  EXPECT_THAT(GetWarningOfHighestPriority(insecure_credentials),
              WarningType::kCompromisedPasswordsWarning);
}

// Tests that the correct number of saved passwords is returned depending on the
// warning type of highest priority.
TEST_F(PasswordCheckupUtilsTest, CheckPasswordCountForWarningType) {
  // Enable Password Checkup feature.
  base::test::ScopedFeatureList featureList;
  featureList.InitAndEnableFeature(
      password_manager::features::kIOSPasswordCheckup);

  std::vector<CredentialUIEntry> insecure_credentials =
      manager().GetInsecureCredentials();
  WarningType warning_type = GetWarningOfHighestPriority(insecure_credentials);
  EXPECT_EQ(GetPasswordCountForWarningType(warning_type, insecure_credentials),
            0);

  // Add a muted password.
  PasswordForm form1 = MakeSavedPassword(kExampleCom1, kUsername116);
  AddIssueToForm(&form1, InsecureType::kLeaked, base::Minutes(1),
                 /*is_muted=*/true);
  store().AddLogin(form1);
  RunUntilIdle();
  insecure_credentials = manager().GetInsecureCredentials();
  warning_type = GetWarningOfHighestPriority(insecure_credentials);
  // The number of passwords for which the compromised warning was dismissed
  // should be returned.
  EXPECT_EQ(GetPasswordCountForWarningType(warning_type, insecure_credentials),
            1);

  // Add 2 weak passwords.
  PasswordForm form2 = MakeSavedPassword(kExampleCom2, kUsername116);
  PasswordForm form3 = MakeSavedPassword(kExampleCom3, kUsername116);
  AddIssueToForm(&form2, InsecureType::kWeak, base::Minutes(1));
  AddIssueToForm(&form3, InsecureType::kWeak, base::Minutes(1));
  store().AddLogin(form2);
  store().AddLogin(form3);
  RunUntilIdle();
  insecure_credentials = manager().GetInsecureCredentials();
  warning_type = GetWarningOfHighestPriority(insecure_credentials);
  // The number of weak passwords should be returned.
  EXPECT_EQ(GetPasswordCountForWarningType(warning_type, insecure_credentials),
            2);

  // Add 3 reused passwords.
  PasswordForm form4 = MakeSavedPassword(kExampleCom4, kUsername116);
  PasswordForm form5 = MakeSavedPassword(kExampleCom5, kUsername116);
  PasswordForm form6 = MakeSavedPassword(kExampleCom6, kUsername116);
  AddIssueToForm(&form4, InsecureType::kReused, base::Minutes(1));
  AddIssueToForm(&form5, InsecureType::kReused, base::Minutes(1));
  AddIssueToForm(&form6, InsecureType::kReused, base::Minutes(1));
  store().AddLogin(form4);
  store().AddLogin(form5);
  store().AddLogin(form6);
  RunUntilIdle();
  insecure_credentials = manager().GetInsecureCredentials();
  warning_type = GetWarningOfHighestPriority(insecure_credentials);
  // The number of reused passwords should be returned.
  EXPECT_EQ(GetPasswordCountForWarningType(warning_type, insecure_credentials),
            3);

  // Add 4 unmuted compromised passwords.
  PasswordForm form7 = MakeSavedPassword(kExampleCom7, kUsername116);
  PasswordForm form8 = MakeSavedPassword(kExampleCom8, kUsername116);
  PasswordForm form9 = MakeSavedPassword(kExampleCom9, kUsername116);
  PasswordForm form10 = MakeSavedPassword(kExampleCom10, kUsername116);
  AddIssueToForm(&form7, InsecureType::kLeaked, base::Minutes(1));
  AddIssueToForm(&form8, InsecureType::kLeaked, base::Minutes(1));
  AddIssueToForm(&form9, InsecureType::kLeaked, base::Minutes(1));
  AddIssueToForm(&form10, InsecureType::kLeaked, base::Minutes(1));
  store().AddLogin(form7);
  store().AddLogin(form8);
  store().AddLogin(form9);
  store().AddLogin(form10);
  RunUntilIdle();
  insecure_credentials = manager().GetInsecureCredentials();
  warning_type = GetWarningOfHighestPriority(insecure_credentials);
  // The number of compromised passwords should be returned.
  EXPECT_EQ(GetPasswordCountForWarningType(warning_type, insecure_credentials),
            4);
}
