// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/autofill/autofill_and_passwords/coordinator/autofill_and_passwords_mediator.h"

#import "base/memory/raw_ptr.h"
#import "base/test/task_environment.h"
#import "components/autofill/core/common/autofill_prefs.h"
#import "components/password_manager/core/common/password_manager_pref_names.h"
#import "components/prefs/testing_pref_service.h"
#import "ios/chrome/browser/settings/autofill/autofill_and_passwords/ui/autofill_and_passwords_consumer.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

class AutofillAndPasswordsMediatorTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    profile_ = TestProfileIOS::Builder().Build();
    pref_service_ = profile_->GetPrefs();
    mediator_ = [[AutofillAndPasswordsMediator alloc]
        initWithUserPrefService:pref_service_
              entityDataManager:nullptr];
    consumer_ = OCMProtocolMock(@protocol(AutofillAndPasswordsConsumer));
  }

  void TearDown() override {
    [mediator_ disconnect];
    PlatformTest::TearDown();
  }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  raw_ptr<PrefService> pref_service_;
  AutofillAndPasswordsMediator* mediator_;
  id consumer_;
};

// Tests that the consumer receives the initial values when it is set.
TEST_F(AutofillAndPasswordsMediatorTest, SetsInitialConsumerValues) {
  pref_service_->SetBoolean(password_manager::prefs::kCredentialsEnableService,
                            YES);
  pref_service_->SetBoolean(autofill::prefs::kAutofillCreditCardEnabled, NO);
  pref_service_->SetBoolean(autofill::prefs::kAutofillProfileEnabled, YES);

  OCMExpect([consumer_ setPasswordsEnabled:YES]);
  OCMExpect([consumer_ setAutofillCreditCardEnabled:NO]);
  OCMExpect([consumer_ setAutofillProfileEnabled:YES]);
  OCMExpect([consumer_ setIdentityDocsEnabled:YES]);
  OCMExpect([consumer_ setTravelInfoEnabled:YES]);
  OCMExpect([consumer_ setShouldShowAutofillAIFeatures:NO]);

  mediator_.consumer = consumer_;

  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests that the consumer receives updates when the preferences change.
TEST_F(AutofillAndPasswordsMediatorTest, UpdatesConsumerOnPreferenceChange) {
  mediator_.consumer = consumer_;

  OCMExpect([consumer_ setPasswordsEnabled:YES]);
  pref_service_->SetBoolean(password_manager::prefs::kCredentialsEnableService,
                            YES);
  EXPECT_OCMOCK_VERIFY(consumer_);

  OCMExpect([consumer_ setAutofillCreditCardEnabled:YES]);
  pref_service_->SetBoolean(autofill::prefs::kAutofillCreditCardEnabled, YES);
  EXPECT_OCMOCK_VERIFY(consumer_);

  OCMExpect([consumer_ setAutofillProfileEnabled:YES]);
  pref_service_->SetBoolean(autofill::prefs::kAutofillProfileEnabled, YES);
  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests that setting the consumer to nil after disconnect does not crash.
// (e.g. during teardown where `setConsumer:nil` might evaluate arguments on a
// nullptr `_userPrefService`).
TEST_F(AutofillAndPasswordsMediatorTest,
       DoesNotCrashOnSetConsumerNilAfterDisconnect) {
  mediator_.consumer = consumer_;
  [mediator_ disconnect];

  mediator_.consumer = nil;
}

// Tests that setting a non-nil consumer after disconnect does not crash.
TEST_F(AutofillAndPasswordsMediatorTest,
       DoesNotCrashOnSetConsumerAfterDisconnect) {
  [mediator_ disconnect];

  mediator_.consumer = consumer_;
}
