// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/send_tab_to_self/model/send_tab_to_self_util.h"

#import <memory>
#import <optional>

#import "base/functional/bind.h"
#import "components/send_tab_to_self/entry_point_display_reason.h"
#import "components/send_tab_to_self/stub_send_tab_to_self_sync_service.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/sync/model/send_tab_to_self_sync_service_factory.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "url/gurl.h"

namespace send_tab_to_self {
namespace {

using SendTabToSelfUtilTest = PlatformTest;

// Tests that the omnibox IPH requires `kOfferFeature`, unlike the entry point.
TEST_F(SendTabToSelfUtilTest, OmniboxIPHEligibility) {
  web::WebTaskEnvironment task_environment;
  TestProfileIOS::Builder builder;
  builder.AddTestingFactory(
      SendTabToSelfSyncServiceFactory::GetInstance(),
      base::BindRepeating(
          [](ProfileIOS* profile) -> std::unique_ptr<KeyedService> {
            return std::make_unique<StubSendTabToSelfSyncService>();
          }));
  std::unique_ptr<TestProfileIOS> profile = std::move(builder).Build();
  auto* service = static_cast<StubSendTabToSelfSyncService*>(
      SendTabToSelfSyncServiceFactory::GetForProfile(profile.get()));
  web::FakeWebState web_state;
  web_state.SetCurrentURL(GURL("https://www.example.com"));

  service->SetEntryPointDisplayReason(EntryPointDisplayReason::kOfferFeature);
  EXPECT_TRUE(IsOmniboxIPHEligible(&web_state, profile.get()));
  EXPECT_FALSE(IsOmniboxIPHEligible(nullptr, profile.get()));
  EXPECT_FALSE(IsOmniboxIPHEligible(&web_state, nullptr));
  EXPECT_FALSE(
      IsOmniboxIPHEligible(&web_state, profile->GetOffTheRecordProfile()));

  service->SetEntryPointDisplayReason(EntryPointDisplayReason::kOfferSignIn);
  EXPECT_FALSE(IsOmniboxIPHEligible(&web_state, profile.get()));
  EXPECT_TRUE(IsOmniboxEntryPointEligible(&web_state, profile.get()));

  service->SetEntryPointDisplayReason(EntryPointDisplayReason::kOfferReauth);
  EXPECT_FALSE(IsOmniboxIPHEligible(&web_state, profile.get()));
  EXPECT_TRUE(IsOmniboxEntryPointEligible(&web_state, profile.get()));

  service->SetEntryPointDisplayReason(
      EntryPointDisplayReason::kInformNoTargetDevice);
  EXPECT_FALSE(IsOmniboxIPHEligible(&web_state, profile.get()));
  EXPECT_TRUE(IsOmniboxEntryPointEligible(&web_state, profile.get()));

  service->SetEntryPointDisplayReason(std::nullopt);
  EXPECT_FALSE(IsOmniboxIPHEligible(&web_state, profile.get()));
  EXPECT_FALSE(IsOmniboxEntryPointEligible(&web_state, profile.get()));
}

}  // namespace
}  // namespace send_tab_to_self
