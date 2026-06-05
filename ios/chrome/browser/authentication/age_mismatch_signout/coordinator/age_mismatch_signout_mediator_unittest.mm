// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/age_mismatch_signout/coordinator/age_mismatch_signout_mediator.h"

#import "base/test/metrics/histogram_tester.h"
#import "components/signin/public/identity_manager/identity_test_environment.h"
#import "components/signin/public/identity_manager/identity_test_utils.h"
#import "ios/chrome/browser/authentication/age_mismatch_signout/coordinator/age_mismatch_signout_constants.h"
#import "ios/chrome/browser/authentication/age_mismatch_signout/ui/age_mismatch_signout_consumer.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/signin/model/avatar/avatar_provider.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/signin/model/identity_test_environment_browser_state_adaptor.h"
#import "ios/chrome/test/testing_application_context.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

namespace {

class AgeMismatchSignoutMediatorTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        IdentityManagerFactory::GetInstance(),
        base::BindRepeating(IdentityTestEnvironmentBrowserStateAdaptor::
                                BuildIdentityManagerForTests));
    profile_ = std::move(builder).Build();

    identity_ = [FakeSystemIdentity fakeIdentity1];

    // Mock Consumer
    consumer_mock_ = OCMProtocolMock(@protocol(AgeMismatchSignoutConsumer));
  }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  signin::IdentityTestEnvironment identity_test_environment_;
  FakeSystemIdentity* identity_;
  id consumer_mock_;
};

// Tests that the mediator notifies the consumer to hide the button when signed
// in.
TEST_F(AgeMismatchSignoutMediatorTest, UpdateConsumerHideButton) {
  base::HistogramTester histogram_tester;
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile_.get());

  // Sign in.
  signin::MakePrimaryAccountAvailable(identity_manager, "test@example.com",
                                      signin::ConsentLevel::kSignin);

  signin::AvatarProvider* avatar_provider =
      GetApplicationContext()->GetIdentityAvatarProvider();

  AgeMismatchSignoutMediator* mediator =
      [[AgeMismatchSignoutMediator alloc] initWithIdentity:identity_
                                    identityAvatarProvider:avatar_provider
                                           identityManager:identity_manager];

  OCMExpect([consumer_mock_ setShowStaySignedOutButton:NO]);
  OCMExpect([consumer_mock_ setPrimaryIdentityName:identity_.userFullName
                                             email:identity_.userEmail
                                            avatar:[OCMArg any]
                                           managed:NO]);
  mediator.consumer = consumer_mock_;
  EXPECT_OCMOCK_VERIFY(consumer_mock_);

  histogram_tester.ExpectBucketCount(
      kAgeMismatchSignoutStaySignedOutButtonHistogram,
      AgeMismatchStaySignedOutButtonState::kHidden, 1);
}

// Tests that the mediator does not notify the consumer to hide the button when
// not signed in.
TEST_F(AgeMismatchSignoutMediatorTest, UpdateConsumerDoNotHideButton) {
  base::HistogramTester histogram_tester;
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile_.get());
  signin::AvatarProvider* avatar_provider =
      GetApplicationContext()->GetIdentityAvatarProvider();

  AgeMismatchSignoutMediator* mediator =
      [[AgeMismatchSignoutMediator alloc] initWithIdentity:identity_
                                    identityAvatarProvider:avatar_provider
                                           identityManager:identity_manager];

  OCMExpect([consumer_mock_ setShowStaySignedOutButton:YES]);
  OCMExpect([consumer_mock_ setPrimaryIdentityName:identity_.userFullName
                                             email:identity_.userEmail
                                            avatar:[OCMArg any]
                                           managed:NO]);
  mediator.consumer = consumer_mock_;
  EXPECT_OCMOCK_VERIFY(consumer_mock_);

  histogram_tester.ExpectBucketCount(
      kAgeMismatchSignoutStaySignedOutButtonHistogram,
      AgeMismatchStaySignedOutButtonState::kShown, 1);
}

}  // namespace
