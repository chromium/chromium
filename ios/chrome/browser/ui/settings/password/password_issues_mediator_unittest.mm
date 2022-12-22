// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_issues_mediator.h"

#import "base/strings/string_piece.h"
#import "base/strings/string_util.h"
#import "base/strings/utf_string_conversions.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/password_manager/core/browser/password_manager_test_utils.h"
#import "components/password_manager/core/browser/test_password_store.h"
#import "components/password_manager/core/browser/ui/credential_ui_entry.h"
#import "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/favicon/favicon_loader.h"
#import "ios/chrome/browser/favicon/ios_chrome_favicon_loader_factory.h"
#import "ios/chrome/browser/main/test_browser.h"
#import "ios/chrome/browser/passwords/ios_chrome_password_check_manager.h"
#import "ios/chrome/browser/passwords/ios_chrome_password_check_manager_factory.h"
#import "ios/chrome/browser/passwords/ios_chrome_password_store_factory.h"
#import "ios/chrome/browser/passwords/password_check_observer_bridge.h"
#import "ios/chrome/browser/sync/sync_setup_service_factory.h"
#import "ios/chrome/browser/sync/sync_setup_service_mock.h"
#import "ios/chrome/browser/ui/settings/password/password_issues_consumer.h"
#import "ios/chrome/browser/ui/table_view/chrome_table_view_controller_test.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
constexpr char kExampleCom[] = "https://example.com";
constexpr char kExampleCom2[] = "https://example2.com";
constexpr char kExampleCom3[] = "https://example3.com";

constexpr char kUsername[] = "alice";
constexpr char kUsername2[] = "bob";

constexpr char kPassword[] = "s3cre3t";

using password_manager::PasswordForm;
using password_manager::InsecureCredential;
using password_manager::TestPasswordStore;

}  // namespace

// Test class that conforms to PasswordIssuesConsumer in order to test the
// consumer methods are called correctly.
@interface FakePasswordIssuesConsumer : NSObject <PasswordIssuesConsumer>

@property(nonatomic) NSArray<PasswordIssue*>* passwords;

@property(nonatomic, assign) BOOL passwordIssuesListChangedWasCalled;

@end

@implementation FakePasswordIssuesConsumer

- (void)setPasswordIssues:(NSArray<PasswordIssue*>*)passwords {
  _passwords = passwords;
  _passwordIssuesListChangedWasCalled = YES;
}

@end

// Tests for Password Issues mediator.
class PasswordIssuesMediatorTest : public BlockCleanupTest {
 protected:
  void SetUp() override {
    BlockCleanupTest::SetUp();
    // Create BrowserState.
    TestChromeBrowserState::Builder test_cbs_builder;
    test_cbs_builder.AddTestingFactory(
        SyncSetupServiceFactory::GetInstance(),
        base::BindRepeating(&SyncSetupServiceMock::CreateKeyedService));
    test_cbs_builder.AddTestingFactory(
        IOSChromePasswordStoreFactory::GetInstance(),
        base::BindRepeating(
            &password_manager::BuildPasswordStore<web::BrowserState,
                                                  TestPasswordStore>));
    chrome_browser_state_ = test_cbs_builder.Build();

    store_ =
        base::WrapRefCounted(static_cast<password_manager::TestPasswordStore*>(
            IOSChromePasswordStoreFactory::GetForBrowserState(
                chrome_browser_state_.get(), ServiceAccessType::EXPLICIT_ACCESS)
                .get()));

    password_check_ = IOSChromePasswordCheckManagerFactory::GetForBrowserState(
        chrome_browser_state_.get());

    consumer_ = [[FakePasswordIssuesConsumer alloc] init];

    mediator_ = [[PasswordIssuesMediator alloc]
        initWithPasswordCheckManager:password_check_.get()
                       faviconLoader:IOSChromeFaviconLoaderFactory::
                                         GetForBrowserState(
                                             chrome_browser_state_.get())
                         syncService:SyncServiceFactory::GetForBrowserState(
                                         chrome_browser_state_.get())];
    mediator_.consumer = consumer_;
  }

  // Adds password form and compromised password to the store.
  void MakeTestPasswordIssue(std::string website = kExampleCom,
                             std::string username = kUsername,
                             std::string password = kPassword) {
    PasswordForm form;
    form.signon_realm = website;
    form.username_value = base::ASCIIToUTF16(username);
    form.password_value = base::ASCIIToUTF16(password);
    form.url = GURL(website + "/login");
    form.action = GURL(website + "/action");
    form.username_element = u"email";
    form.password_issues = {
        {password_manager::InsecureType::kLeaked,
         password_manager::InsecurityMetadata(
             base::Time::Now(), password_manager::IsMuted(false))}};
    form.in_store = PasswordForm::Store::kProfileStore;
    store()->AddLogin(form);
  }

  TestPasswordStore* store() { return store_.get(); }

  FakePasswordIssuesConsumer* consumer() { return consumer_; }

  PasswordIssuesMediator* mediator() { return mediator_; }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

 private:
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
  scoped_refptr<TestPasswordStore> store_;
  scoped_refptr<IOSChromePasswordCheckManager> password_check_;
  FakePasswordIssuesConsumer* consumer_;
  PasswordIssuesMediator* mediator_;
};

// Tests that changes to password store are reflected to the consumer.
TEST_F(PasswordIssuesMediatorTest, TestPasswordIssuesChanged) {
  EXPECT_EQ(0u, [[consumer() passwords] count]);
  consumer().passwordIssuesListChangedWasCalled = NO;

  MakeTestPasswordIssue();
  RunUntilIdle();

  EXPECT_TRUE([consumer() passwordIssuesListChangedWasCalled]);

  EXPECT_EQ(1u, [[consumer() passwords] count]);

  PasswordIssue* password = [[consumer() passwords] objectAtIndex:0];

  EXPECT_NSEQ(@"alice", password.username);
  EXPECT_NSEQ(@"example.com", password.website);
}

// Tests that mediator deletes password from the store.
TEST_F(PasswordIssuesMediatorTest, TestPasswordDeletion) {
  MakeTestPasswordIssue();
  RunUntilIdle();

  EXPECT_EQ(1u, [[consumer() passwords] count]);

  auto password = store()->stored_passwords().at(kExampleCom).at(0);
  [mediator() deleteCredential:password_manager::CredentialUIEntry(password)];
  RunUntilIdle();
  EXPECT_EQ(0u, [[consumer() passwords] count]);
}

// Tests that passwords are sorted properly.
TEST_F(PasswordIssuesMediatorTest, TestPasswordSorting) {
  EXPECT_EQ(0u, [[consumer() passwords] count]);

  MakeTestPasswordIssue(kExampleCom3);
  MakeTestPasswordIssue(kExampleCom2, kUsername2);
  RunUntilIdle();
  EXPECT_EQ(2u, [[consumer() passwords] count]);

  EXPECT_NSEQ(@"example2.com",
              [[consumer() passwords] objectAtIndex:0].website);
  EXPECT_NSEQ(@"example3.com",
              [[consumer() passwords] objectAtIndex:1].website);

  MakeTestPasswordIssue(kExampleCom, kUsername2);
  MakeTestPasswordIssue(kExampleCom);
  RunUntilIdle();

  EXPECT_EQ(4u, [[consumer() passwords] count]);
  EXPECT_NSEQ(@"alice", [[consumer() passwords] objectAtIndex:0].username);
  EXPECT_NSEQ(@"example.com", [[consumer() passwords] objectAtIndex:0].website);

  EXPECT_NSEQ(@"bob", [[consumer() passwords] objectAtIndex:1].username);
  EXPECT_NSEQ(@"example.com", [[consumer() passwords] objectAtIndex:1].website);

  EXPECT_NSEQ(@"bob", [[consumer() passwords] objectAtIndex:2].username);
  EXPECT_NSEQ(@"example2.com",
              [[consumer() passwords] objectAtIndex:2].website);

  EXPECT_NSEQ(@"alice", [[consumer() passwords] objectAtIndex:3].username);
  EXPECT_NSEQ(@"example3.com",
              [[consumer() passwords] objectAtIndex:3].website);
}
