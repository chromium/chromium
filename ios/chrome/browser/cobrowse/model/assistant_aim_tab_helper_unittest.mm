// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/cobrowse/model/assistant_aim_tab_helper.h"

#import "base/functional/callback.h"
#import "base/test/scoped_feature_list.h"
#import "components/omnibox/browser/mock_aim_eligibility_service.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/web/public/test/fakes/fake_navigation_context.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/lens_server_proto/aim_communication.pb.h"

class AssistantAimTabHelperTest : public PlatformTest {
 protected:
  web::WebTaskEnvironment task_environment_;
  web::FakeWebState web_state_;
};

// Tests that IsEligible returns true only when AimEligibilityService and
// PrefService are eligible for AIM Cobrowse.
TEST_F(AssistantAimTabHelperTest, IsEligible) {
  base::test::ScopedFeatureList feature_list(kAimCobrowse);
  TestProfileIOS::Builder builder;
  builder.AddTestingFactory(
      ios::TemplateURLServiceFactory::GetInstance(),
      ios::TemplateURLServiceFactory::GetDefaultFactory());
  std::unique_ptr<TestProfileIOS> profile = std::move(builder).Build();

  testing::NiceMock<MockAimEligibilityService> eligibility_service(
      *profile->GetPrefs(),
      ios::TemplateURLServiceFactory::GetForProfile(profile.get()), nullptr,
      IdentityManagerFactory::GetForProfile(profile.get()));
  ON_CALL(eligibility_service, IsFuseboxEligible())
      .WillByDefault(testing::Return(true));
  ON_CALL(eligibility_service, IsCobrowseEligible())
      .WillByDefault(testing::Return(true));

  web::FakeWebState web_state;
  AssistantAimTabHelper::CreateForWebState(&web_state, &eligibility_service,
                                           profile->GetPrefs());
  AssistantAimTabHelper* tab_helper =
      AssistantAimTabHelper::FromWebState(&web_state);
  ASSERT_NE(tab_helper, nullptr);
  EXPECT_TRUE(tab_helper->IsEligible());

  ON_CALL(eligibility_service, IsCobrowseEligible())
      .WillByDefault(testing::Return(false));
  EXPECT_FALSE(tab_helper->IsEligible());
}

// Tests that receiving a handshake response sets IsHandshakeReceived to true.
TEST_F(AssistantAimTabHelperTest, HandshakeResponseReceived) {
  AssistantAimTabHelper::CreateForWebState(&web_state_, nullptr, nullptr);
  AssistantAimTabHelper* tab_helper =
      AssistantAimTabHelper::FromWebState(&web_state_);
  ASSERT_NE(tab_helper, nullptr);
  EXPECT_FALSE(tab_helper->IsHandshakeReceived());

  lens::AimToClientMessage handshake_response;
  handshake_response.mutable_handshake_response();

  tab_helper->OnMessageReceived(handshake_response);
  EXPECT_TRUE(tab_helper->IsHandshakeReceived());
}

// Tests that starting a navigation resets the handshake status.
TEST_F(AssistantAimTabHelperTest, NavigationResetsHandshake) {
  AssistantAimTabHelper::CreateForWebState(&web_state_, nullptr, nullptr);
  AssistantAimTabHelper* tab_helper =
      AssistantAimTabHelper::FromWebState(&web_state_);
  ASSERT_NE(tab_helper, nullptr);

  // Set handshake to received.
  lens::AimToClientMessage handshake_response;
  handshake_response.mutable_handshake_response();
  tab_helper->OnMessageReceived(handshake_response);
  ASSERT_TRUE(tab_helper->IsHandshakeReceived());

  // Simulate navigation to a new document.
  web::FakeNavigationContext context;
  context.SetIsSameDocument(false);
  tab_helper->DidStartNavigation(&web_state_, &context);

  EXPECT_FALSE(tab_helper->IsHandshakeReceived());
}

// Tests that same document navigations do not reset the handshake status.
TEST_F(AssistantAimTabHelperTest, SameDocumentNavigationDoesNotResetHandshake) {
  AssistantAimTabHelper::CreateForWebState(&web_state_, nullptr, nullptr);
  AssistantAimTabHelper* tab_helper =
      AssistantAimTabHelper::FromWebState(&web_state_);
  ASSERT_NE(tab_helper, nullptr);

  // Set handshake to received.
  lens::AimToClientMessage handshake_response;
  handshake_response.mutable_handshake_response();
  tab_helper->OnMessageReceived(handshake_response);
  ASSERT_TRUE(tab_helper->IsHandshakeReceived());

  // Simulate same-document navigation.
  web::FakeNavigationContext context;
  context.SetIsSameDocument(true);
  tab_helper->DidStartNavigation(&web_state_, &context);

  EXPECT_TRUE(tab_helper->IsHandshakeReceived());
}

// Tests that the message callback is successfully invoked with the parsed
// message.
TEST_F(AssistantAimTabHelperTest, MessageCallbackInvoked) {
  AssistantAimTabHelper::CreateForWebState(&web_state_, nullptr, nullptr);
  AssistantAimTabHelper* tab_helper =
      AssistantAimTabHelper::FromWebState(&web_state_);
  ASSERT_NE(tab_helper, nullptr);

  bool callback_invoked = false;
  bool hide_input_received = false;
  tab_helper->SetMessageCallback(base::BindRepeating(
      [](bool* invoked, bool* hide_received,
         const lens::AimToClientMessage& msg) {
        *invoked = true;
        if (msg.has_hide_input()) {
          *hide_received = true;
        }
      },
      &callback_invoked, &hide_input_received));

  lens::AimToClientMessage aim_message;
  aim_message.mutable_hide_input();

  tab_helper->OnMessageReceived(aim_message);

  EXPECT_TRUE(callback_invoked);
  EXPECT_TRUE(hide_input_received);
}
