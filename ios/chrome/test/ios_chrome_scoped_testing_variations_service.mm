// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/ios_chrome_scoped_testing_variations_service.h"

#import "components/network_time/network_time_tracker.h"
#import "components/variations/service/variations_service.h"
#import "components/variations/service/variations_service_client.h"
#import "components/variations/synthetic_trial_registry.h"
#import "ios/chrome/test/testing_application_context.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"
#import "services/network/test/test_network_connection_tracker.h"
#import "testing/gtest/include/gtest/gtest.h"

using variations::SyntheticTrialRegistry;
using variations::UIStringOverrider;
using variations::VariationsService;
using variations::VariationsServiceClient;

// Test VariationsServiceClient used to create
// IOSChromeScopedTestingVariationsService.
class TestVariationsServiceClient : public VariationsServiceClient {
 public:
  TestVariationsServiceClient() = default;
  TestVariationsServiceClient(const TestVariationsServiceClient&) = delete;
  TestVariationsServiceClient& operator=(const TestVariationsServiceClient&) =
      delete;
  ~TestVariationsServiceClient() override = default;

  // VariationsServiceClient:
  base::Version GetVersionForSimulation() override { return base::Version(); }
  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory()
      override {
    return nullptr;
  }
  network_time::NetworkTimeTracker* GetNetworkTimeTracker() override {
    return nullptr;
  }
  bool OverridesRestrictParameter(std::string* parameter) override {
    return false;
  }
  bool IsEnterprise() override { return false; }
  void RemoveGoogleGroupsFromPrefsForDeletedProfiles(
      PrefService* local_state) override {}

 private:
  // VariationsServiceClient:
  version_info::Channel GetChannel() override {
    return version_info::Channel::UNKNOWN;
  }
};

IOSChromeScopedTestingVariationsService::
    IOSChromeScopedTestingVariationsService() {
  EXPECT_EQ(nullptr,
            TestingApplicationContext::GetGlobal()->GetVariationsService());
  synthetic_trial_registry_ = std::make_unique<SyntheticTrialRegistry>();

  variations_service_ = VariationsService::Create(
      std::make_unique<TestVariationsServiceClient>(),
      TestingApplicationContext::GetGlobal()->GetLocalState(),
      /*state_manager=*/nullptr, "dummy-disable-background-switch",
      UIStringOverrider(),
      network::TestNetworkConnectionTracker::CreateGetter(),
      synthetic_trial_registry_.get());
  TestingApplicationContext::GetGlobal()->SetVariationsService(
      variations_service_.get());
}

IOSChromeScopedTestingVariationsService::
    ~IOSChromeScopedTestingVariationsService() {
  EXPECT_EQ(variations_service_.get(),
            TestingApplicationContext::GetGlobal()->GetVariationsService());
  TestingApplicationContext::GetGlobal()->SetVariationsService(nullptr);
  variations_service_.reset();
}

VariationsService* IOSChromeScopedTestingVariationsService::Get() {
  return variations_service_.get();
}
