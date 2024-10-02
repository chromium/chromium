// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_issues/password_issues_mediator.h"

#import "base/strings/sys_string_conversions.h"
#import "base/test/bind.h"
#import "base/test/scoped_feature_list.h"
#import "components/affiliations/core/browser/fake_affiliation_service.h"
#import "components/google/core/common/google_util.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/password_manager/core/browser/password_form.h"
#import "components/password_manager/core/browser/password_manager_test_utils.h"
#import "components/password_manager/core/browser/password_store/test_password_store.h"
#import "components/password_manager/core/browser/ui/credential_ui_entry.h"
#import "ios/chrome/browser/affiliations/model/ios_chrome_affiliation_service_factory.h"
#import "ios/chrome/browser/favicon/model/favicon_loader.h"
#import "ios/chrome/browser/favicon/model/ios_chrome_favicon_loader_factory.h"
#import "ios/chrome/browser/net/model/crurl.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_password_check_manager.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_password_check_manager_factory.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_profile_password_store_factory.h"
#import "ios/chrome/browser/passwords/model/password_check_observer_bridge.h"
#import "ios/chrome/browser/passwords/model/password_checkup_utils.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_controller_test.h"
#import "ios/chrome/browser/ui/settings/password/password_checkup/password_checkup_constants.h"
#import "ios/chrome/browser/ui/settings/password/password_issues/password_issues_consumer.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "ui/base/l10n/l10n_util_mac.h"

using password_manager::InsecureType;
using password_manager::PasswordForm;
using password_manager::TestPasswordStore;
using password_manager::WarningType;

namespace {

constexpr char kExampleCom[] = "https://example.com";
constexpr char kExampleCom2[] = "https://example2.com";
constexpr char kExampleCom3[] = "https://example3.com";

constexpr NSString* kExampleString = @"example.com";
constexpr NSString* kExample2String = @"example2.com";
constexpr NSString* kExample3String = @"example3.com";

constexpr char kUsername[] = "alice";
constexpr char kUsername2[] = "bob";

constexpr char kPassword[] = "s3cre3t";
constexpr char kPassword2[] = "s3cre3t2";
constexpr char kStrongPassword[] = "pmsFlsnoab4nsl#losb@skpfnsbkjb^klsnbs!cns";
constexpr char kStrongPassword2[] = "sfdf#losb@sdf^klsnbs!cns";
constexpr char kStrongPassword3[] = "sdfsdfwer@313QaDSdsd!cns";

NSString* GetUsername() {
  return base::SysUTF8ToNSString(kUsername);
}

NSString* GetUsername2() {
  return base::SysUTF8ToNSString(kUsername2);
}

// Returns a URL with localized according to the Application Locale.
GURL GetLocalizedURL(const GURL& original) {
  return google_util::AppendGoogleLocaleParam(
      original, GetApplicationContext()->GetApplicationLocale());
}

}  // namespace

// Test class that conforms to PasswordIssuesConsumer in order to test the
// consumer methods are called correctly.
@interface FakePasswordIssuesConsumer : NSObject <PasswordIssuesConsumer>

@property(nonatomic, strong) NSArray<PasswordIssueGroup*>* passwordIssueGroups;

@property(nonatomic, assign) BOOL passwordIssuesListChangedWasCalled;

@property(nonatomic, copy) NSString* title;

@property(nonatomic, copy) NSString* headerText;

@property(nonatomic, copy) CrURL* headerURL;

@property(nonatomic, assign) NSInteger dismissedWarningsCount;

@end

@implementation FakePasswordIssuesConsumer

- (void)setPasswordIssues:(NSArray<PasswordIssueGroup*>*)passwordIssueGroups
    dismissedWarningsCount:(NSInteger)dismissedWarningsCount {
  _passwordIssueGroups = passwordIssueGroups;
  _dismissedWarningsCount = dismissedWarningsCount;
  _passwordIssuesListChangedWasCalled = YES;
}

- (void)setNavigationBarTitle:(NSString*)title {
  _title = title;
}

- (void)setHeader:(NSString*)text URL:(CrURL*)URL {
  _headerText = text;
  _headerURL = URL;
}

@end

// Tests for Password Issues mediator.
class PasswordIssuesMediatorTest : public BlockCleanupTest {
 protected:
  void SetUp() override {
    BlockCleanupTest::SetUp();
    // Create profile.
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

    password_check_ =
        IOSChromePasswordCheckManagerFactory::GetForProfile(profile_.get());

    consumer_ = [[FakePasswordIssuesConsumer alloc] init];

    CreateMediator(WarningType::kCompromisedPasswordsWarning);
  }

  // Creates a mediator for the given warning type.
  void CreateMediator(WarningType warning_type) {
    mediator_ = [[PasswordIssuesMediator alloc]
          initForWarningType:warning_type
        passwordCheckManager:password_check_.get()
               faviconLoader:IOSChromeFaviconLoaderFactory::GetForProfile(
                                 profile_.get())
                 syncService:SyncServiceFactory::GetForProfile(profile_.get())];
    mediator_.consumer = consumer_;
  }

  // Adds password form and insecure password to the store.
  void MakeTestPasswordIssue(std::string website = kExampleCom,
                             std::string username = kUsername,
                             std::string password = kPassword,
                             InsecureType insecure_type = InsecureType::kLeaked,
                             bool muted = false) {
    PasswordForm form;
    form.signon_realm = website;
    form.username_value = base::ASCIIToUTF16(username);
    form.password_value = base::ASCIIToUTF16(password);
    form.url = GURL(website + "/login");
    form.action = GURL(website + "/action");
    form.username_element = u"email";
    form.password_issues = {
        {insecure_type,
         password_manager::InsecurityMetadata(
             base::Time::Now(), password_manager::IsMuted(muted),
             password_manager::TriggerBackendNotification(false))}};
    form.in_store = PasswordForm::Store::kProfileStore;
    store()->AddLogin(form);
  }

  void CheckIssue(NSUInteger group = 0,
                  NSUInteger index = 0,
                  NSString* expected_website = kExampleString,
                  NSString* expected_username = GetUsername()) {
    ASSERT_LT(group, consumer().passwordIssueGroups.count);

    PasswordIssueGroup* issue_group = consumer().passwordIssueGroups[group];
    ASSERT_LT(index, issue_group.passwordIssues.count);

    PasswordIssue* issue = issue_group.passwordIssues[index];

    EXPECT_NSEQ(expected_username, issue.username);
    EXPECT_NSEQ(expected_website, issue.website);
  }

  void CheckGroupsCount(NSUInteger expected_count) {
    EXPECT_EQ(expected_count, consumer().passwordIssueGroups.count);
  }

  void CheckGroupSize(NSUInteger group, NSUInteger expected_size) {
    ASSERT_LT(group, consumer().passwordIssueGroups.count);
    EXPECT_EQ(expected_size,
              consumer().passwordIssueGroups[group].passwordIssues.count);
  }

  TestPasswordStore* store() { return store_.get(); }

  FakePasswordIssuesConsumer* consumer() { return consumer_; }

  PasswordIssuesMediator* mediator() { return mediator_; }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

 private:
  base::test::ScopedFeatureList feature_list_;
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  scoped_refptr<TestPasswordStore> store_;
  scoped_refptr<IOSChromePasswordCheckManager> password_check_;
  FakePasswordIssuesConsumer* consumer_;
  PasswordIssuesMediator* mediator_;
};

// Tests that changes to password store are reflected to the consumer.
TEST_F(PasswordIssuesMediatorTest, TestPasswordIssuesChanged) {
  CheckGroupsCount(0);
  consumer().passwordIssuesListChangedWasCalled = NO;

  MakeTestPasswordIssue();
  RunUntilIdle();

  EXPECT_TRUE([consumer() passwordIssuesListChangedWasCalled]);

  CheckGroupsCount(1);
  CheckGroupSize(/*group=*/0, /*expected_size=*/1);
  CheckIssue();
}

// Tests that changes to password store are not sent to the consumer if the
// credentials with the current warning type did not change.
TEST_F(PasswordIssuesMediatorTest, TestPasswordIssuesChangedNotCalled) {
  CreateMediator(WarningType::kCompromisedPasswordsWarning);

  CheckGroupsCount(0);
  consumer().passwordIssuesListChangedWasCalled = NO;

  // Add other types of insecure passwords that shouldn't be sent to the
  // consumer.
  MakeTestPasswordIssue(kExampleCom, kUsername, kPassword, InsecureType::kWeak);
  MakeTestPasswordIssue(kExampleCom2, kUsername, kPassword,
                        InsecureType::kReused);
  RunUntilIdle();

  EXPECT_FALSE([consumer() passwordIssuesListChangedWasCalled]);

  // Add compromised password that should be sent to consumer.
  MakeTestPasswordIssue(kExampleCom, kUsername2, kPassword,
                        InsecureType::kLeaked);
  RunUntilIdle();

  EXPECT_TRUE([consumer() passwordIssuesListChangedWasCalled]);

  CheckGroupsCount(1);
  CheckGroupSize(/*group=*/0, /*expected_size=*/1);
  CheckIssue(/*group=*/0, /*index=*/0, /*expected_website=*/kExampleString,
             /*expected_username=*/GetUsername2());
}

// Tests that only passwords issues of the current warning type are sent to the
// consumer.
TEST_F(PasswordIssuesMediatorTest, TestPasswordIssuesFilteredByWarningType) {
  // Create all types of insecure passwords.
  // Weak.
  MakeTestPasswordIssue(kExampleCom, kUsername, kPassword, InsecureType::kWeak);
  // Reused.
  MakeTestPasswordIssue(kExampleCom2, kUsername, kStrongPassword,
                        InsecureType::kReused);
  MakeTestPasswordIssue(kExampleCom3, kUsername2, kStrongPassword,
                        InsecureType::kReused);
  // Dismissed Compromised
  MakeTestPasswordIssue(kExampleCom3, kUsername, kStrongPassword2,
                        InsecureType::kLeaked, /*muted=*/true);
  // Compromised.
  MakeTestPasswordIssue(kExampleCom, kUsername2, kStrongPassword3,
                        InsecureType::kPhished);
  RunUntilIdle();

  // Send only compromised passwords to consumer.
  CreateMediator(WarningType::kCompromisedPasswordsWarning);

  CheckIssue(/*group=*/0, /*index=*/0, /*expected_website=*/kExampleString,
             /*expected_username=*/GetUsername2());

  EXPECT_EQ(consumer().dismissedWarningsCount, 1);

  // Send only weak passwords to consumer.
  CreateMediator(WarningType::kWeakPasswordsWarning);

  CheckIssue();

  EXPECT_EQ(0, consumer().dismissedWarningsCount);

  // Send only reused passwords to consumer.
  CreateMediator(WarningType::kReusedPasswordsWarning);

  CheckIssue(/*group=*/0, /*index=*/0, /*expected_website=*/kExample2String);
  CheckIssue(/*group=*/0, /*index=*/1, /*expected_website=*/kExample3String,
             /*expected_username=*/GetUsername2());

  EXPECT_EQ(0, consumer().dismissedWarningsCount);

  // Send only dismissed passwords to consumer.
  CreateMediator(WarningType::kDismissedWarningsWarning);

  CheckIssue(/*group=*/0, /*index=*/0, /*expected_website=*/kExample3String);

  EXPECT_EQ(0, consumer().dismissedWarningsCount);
}

/// Tests the mediator sets the consumer title for compromised passwords.
TEST_F(PasswordIssuesMediatorTest, TestSetConsumerCompromisedTitle) {
  CreateMediator(WarningType::kCompromisedPasswordsWarning);

  EXPECT_NSEQ(@"Compromised passwords", consumer().title);

  MakeTestPasswordIssue();
  RunUntilIdle();

  EXPECT_NSEQ(@"1 compromised password", consumer().title);

  MakeTestPasswordIssue(kExampleCom2);
  RunUntilIdle();

  EXPECT_NSEQ(@"2 compromised passwords", consumer().title);
}

/// Tests the mediator sets the consumer title for weak passwords.
TEST_F(PasswordIssuesMediatorTest, TestSetConsumerWeakTitle) {
  CreateMediator(WarningType::kWeakPasswordsWarning);

  MakeTestPasswordIssue(kExampleCom, kUsername, kPassword, InsecureType::kWeak);
  RunUntilIdle();

  EXPECT_NSEQ(@"1 weak password", consumer().title);

  MakeTestPasswordIssue(kExampleCom2, kUsername, kPassword,
                        InsecureType::kWeak);
  RunUntilIdle();

  EXPECT_NSEQ(@"2 weak passwords", consumer().title);
}

/// Tests the mediator sets the consumer title for dismissed warnings.
TEST_F(PasswordIssuesMediatorTest, TestSetConsumerDismissedTitle) {
  CreateMediator(WarningType::kDismissedWarningsWarning);

  MakeTestPasswordIssue();
  RunUntilIdle();

  EXPECT_NSEQ(@"Dismissed warnings", consumer().title);
}

/// Tests the mediator sets the consumer title for reused passwords.
TEST_F(PasswordIssuesMediatorTest, TestSetConsumerReusedTitle) {
  CreateMediator(WarningType::kReusedPasswordsWarning);

  MakeTestPasswordIssue(kExampleCom, kUsername, kPassword,
                        InsecureType::kReused);
  MakeTestPasswordIssue(kExampleCom2, kUsername, kPassword,
                        InsecureType::kReused);
  RunUntilIdle();

  EXPECT_NSEQ(@"2 reused passwords", consumer().title);
}

/// Tests the mediator sets the consumer header for compromised passwords.
TEST_F(PasswordIssuesMediatorTest, TestSetConsumerCompromisedHeader) {
  CreateMediator(WarningType::kCompromisedPasswordsWarning);

  EXPECT_NSEQ(
      l10n_util::GetNSString(IDS_IOS_COMPROMISED_PASSWORD_ISSUES_DESCRIPTION),
      consumer().headerText);
  EXPECT_EQ(GetLocalizedURL(
                GURL(password_manager::
                         kPasswordManagerHelpCenterChangeUnsafePasswordsURL)),
            consumer().headerURL.gurl);
}

/// Tests the mediator sets the consumer header for weak passwords.
TEST_F(PasswordIssuesMediatorTest, TestSetConsumerWeakHeader) {
  CreateMediator(WarningType::kWeakPasswordsWarning);

  EXPECT_NSEQ(l10n_util::GetNSString(IDS_IOS_WEAK_PASSWORD_ISSUES_DESCRIPTION),
              consumer().headerText);
  EXPECT_EQ(GetLocalizedURL(
                GURL(password_manager::
                         kPasswordManagerHelpCenterCreateStrongPasswordsURL)),
            consumer().headerURL.gurl);
}

/// Tests the mediator sets the consumer header for reused passwords.
TEST_F(PasswordIssuesMediatorTest, TestSetConsumerReusedHeader) {
  CreateMediator(WarningType::kReusedPasswordsWarning);

  EXPECT_NSEQ(
      l10n_util::GetNSString(IDS_IOS_REUSED_PASSWORD_ISSUES_DESCRIPTION),
      consumer().headerText);

  EXPECT_FALSE(consumer().headerURL);
}

/// Tests the mediator doesn't set a header for dismissed warnings.
TEST_F(PasswordIssuesMediatorTest, TestSetConsumerDismissedHeader) {
  consumer().headerText = nil;
  consumer().headerURL = nil;

  CreateMediator(WarningType::kDismissedWarningsWarning);

  EXPECT_FALSE(consumer().headerText);
  EXPECT_FALSE(consumer().headerURL);
}

// Tests that passwords are sorted properly.
TEST_F(PasswordIssuesMediatorTest, TestPasswordSorting) {
  CheckGroupsCount(0);

  MakeTestPasswordIssue(kExampleCom3);
  MakeTestPasswordIssue(kExampleCom2, kUsername2);
  RunUntilIdle();

  CheckGroupsCount(1);
  CheckGroupSize(/*group=*/0, /*expected_size=*/2);

  CheckIssue(/*group=*/0, /*index=*/0, /*expected_website=*/kExample2String,
             /*expected_username=*/GetUsername2());
  CheckIssue(/*group=*/0, /*index=*/1, /*expected_website=*/kExample3String);

  MakeTestPasswordIssue(kExampleCom, kUsername2);
  MakeTestPasswordIssue(kExampleCom);
  RunUntilIdle();

  CheckGroupsCount(1);
  CheckGroupSize(/*group=*/0, /*expected_size=*/4);

  CheckIssue();
  CheckIssue(/*group=*/0, /*index=*/1, /*expected_website=*/kExampleString,
             /*expected_username=*/GetUsername2());
  CheckIssue(/*group=*/0, /*index=*/2, /*expected_website=*/kExample2String,
             /*expected_username=*/GetUsername2());
  CheckIssue(/*group=*/0, /*index=*/3, /*expected_website=*/kExample3String);
}

// Tests that reused password issues are grouped by password.
TEST_F(PasswordIssuesMediatorTest, TestReusedPasswordsGrouping) {
  CreateMediator(WarningType::kReusedPasswordsWarning);
  CheckGroupsCount(0);

  // Create group of reused passwords.
  MakeTestPasswordIssue(kExampleCom3, kUsername, kPassword,
                        InsecureType::kReused);
  MakeTestPasswordIssue(kExampleCom2, kUsername2, kPassword,
                        InsecureType::kReused);
  MakeTestPasswordIssue(kExampleCom2, kUsername, kPassword,
                        InsecureType::kReused);

  // Create another group of reused passwords.
  MakeTestPasswordIssue(kExampleCom, kUsername2, kPassword2,
                        InsecureType::kReused);
  MakeTestPasswordIssue(kExampleCom3, kUsername2, kPassword2,
                        InsecureType::kReused);

  RunUntilIdle();

  CheckGroupsCount(2);

  // Validate first group.
  CheckGroupSize(/*group=*/0, /*expected_size=*/2);
  CheckIssue(/*group=*/0, /*index=*/0, /*expected_website=*/kExampleString,
             /*expected_username=*/GetUsername2());
  CheckIssue(/*group=*/0, /*index=*/1, /*expected_website=*/kExample3String,
             /*expected_username=*/GetUsername2());

  // Validate second group.
  CheckGroupSize(/*group=*/1, /*expected_size=*/3);
  CheckIssue(/*group=*/1, /*index=*/0, /*expected_website=*/kExample2String);
  CheckIssue(/*group=*/1, /*index=*/1, /*expected_website=*/kExample2String,
             /*expected_username=*/GetUsername2());
  CheckIssue(/*group=*/1, /*index=*/2, /*expected_website=*/kExample3String);
}
