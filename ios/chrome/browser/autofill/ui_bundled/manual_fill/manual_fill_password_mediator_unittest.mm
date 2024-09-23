// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_password_mediator.h"

#import "base/memory/raw_ptr.h"
#import "base/test/bind.h"
#import "components/affiliations/core/browser/fake_affiliation_service.h"
#import "components/autofill/core/common/autofill_test_utils.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/password_manager/core/browser/fake_form_fetcher.h"
#import "components/password_manager/core/browser/password_manager_test_utils.h"
#import "components/password_manager/core/browser/password_store/test_password_store.h"
#import "components/password_manager/core/browser/ui/saved_passwords_presenter.h"
#import "ios/chrome/browser/affiliations/model/ios_chrome_affiliation_service_factory.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/form_fetcher_consumer_bridge.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_password_cell.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_password_mediator+Testing.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/password_consumer.h"
#import "ios/chrome/browser/favicon/model/ios_chrome_favicon_loader_factory.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_profile_password_store_factory.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/ui/settings/password/saved_passwords_presenter_observer.h"
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

// Creates a password form.
PasswordForm CreatePasswordForm() {
  PasswordForm form;
  form.username_value = u"test@egmail.com";
  form.password_value = u"strongPa55w0rd";
  form.signon_realm = "http://www.example.com/";
  form.in_store = PasswordForm::Store::kProfileStore;
  return form;
}

}  // namespace

// Test fixture for testing the ManualFillPasswordMediator class.
class ManualFillPasswordMediatorTest : public PlatformTest {
 protected:
  void SetUp() override {
    fake_web_state_ = std::make_unique<web::FakeWebState>();

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
            IOSChromeProfilePasswordStoreFactory::GetForBrowserState(
                profile_.get(), ServiceAccessType::EXPLICIT_ACCESS)
                .get()));

    affiliation_service_ = static_cast<affiliations::FakeAffiliationService*>(
        IOSChromeAffiliationServiceFactory::GetForBrowserState(profile_.get()));

    presenter_ = std::make_unique<SavedPasswordsPresenter>(
        affiliation_service_, store_, /*accont_store=*/nullptr);
    presenter_->Init();

    mediator_ = [[ManualFillPasswordMediator alloc]
           initWithFaviconLoader:IOSChromeFaviconLoaderFactory::GetForProfile(
                                     profile_.get())
                        webState:fake_web_state_.get()
                     syncService:SyncServiceFactory::GetForBrowserState(
                                     profile_.get())
                             URL:GURL("http://www.example.com/")
        invokedOnObfuscatedField:NO
            profilePasswordStore:store_
            accountPasswordStore:nil
          showAutofillFormButton:NO];

    consumer_ = OCMProtocolMock(@protocol(ManualFillPasswordConsumer));
    mediator_.consumer = consumer_;
  }

  void TearDown() override { [mediator_ disconnect]; }

  SavedPasswordsPresenter* presenter() { return presenter_.get(); }

  ManualFillPasswordMediator* mediator() { return mediator_; }

  id consumer() { return consumer_; }

  TestPasswordStore& GetTestStore() {
    return *static_cast<TestPasswordStore*>(
        IOSChromeProfilePasswordStoreFactory::GetForBrowserState(
            profile_.get(), ServiceAccessType::EXPLICIT_ACCESS)
            .get());
  }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

 private:
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<web::FakeWebState> fake_web_state_;
  scoped_refptr<TestPasswordStore> store_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<SavedPasswordsPresenter> presenter_;
  id consumer_;
  raw_ptr<affiliations::FakeAffiliationService> affiliation_service_;
  ManualFillPasswordMediator* mediator_;
};

// Tests that the consumer is notified when the passwords related to the current
// site are fetched.
TEST_F(ManualFillPasswordMediatorTest, NotifiesConsumerOnFetchDidComplete) {
  // Set the mediator's form fetcher.
  [mediator()
      setFormFetcher:std::make_unique<password_manager::FakeFormFetcher>()];
  EXPECT_TRUE([mediator() conformsToProtocol:@protocol(FormFetcherConsumer)]);

  OCMExpect([consumer() presentCredentials:[OCMArg isNotNil]]);

  ManualFillPasswordMediator<FormFetcherConsumer>* form_fetcher_consumer =
      static_cast<ManualFillPasswordMediator<FormFetcherConsumer>*>(mediator());

  [form_fetcher_consumer fetchDidComplete];

  EXPECT_OCMOCK_VERIFY(consumer());
}

// Tests that the consumer is notified through `fetchAllPasswords` when the
// saved passwords change.
TEST_F(ManualFillPasswordMediatorTest, NotifiesConsumerOnFetchAllPasswords) {
  // Set the mediator's saved passwords presenter.
  mediator().savedPasswordsPresenter = presenter();
  EXPECT_TRUE([mediator()
      conformsToProtocol:@protocol(SavedPasswordsPresenterObserver)]);

  // Add password form to store so we get a result when getting the passwords
  // from the saved passwords presenter.
  PasswordForm form = CreatePasswordForm();
  GetTestStore().AddLogin(form);
  RunUntilIdle();

  OCMExpect([consumer()
      presentCredentials:[OCMArg checkWithBlock:^(
                                     NSArray<ManualFillCredentialItem*>*
                                         credential_items) {
        EXPECT_EQ([credential_items count], 1u);
        return YES;
      }]]);

  ManualFillPasswordMediator<
      SavedPasswordsPresenterObserver>* saved_passwords_presenter_observer =
      static_cast<ManualFillPasswordMediator<SavedPasswordsPresenterObserver>*>(
          mediator());

  // Call `savedPasswordsDidChange` to trigger a call to `fetchAllPasswords`.
  [saved_passwords_presenter_observer savedPasswordsDidChange];

  EXPECT_OCMOCK_VERIFY(consumer());
}
