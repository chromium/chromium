// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/proxy_config_service_mojo.h"

#include <array>
#include <memory>
#include <optional>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/metrics/field_trial.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "net/proxy_resolution/proxy_config.h"
#include "net/proxy_resolution/proxy_config_service.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/proxy_config.mojom.h"
#include "services/network/public/mojom/proxy_config_with_annotation.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {

namespace {

// Test class for observing proxy config changes.
class TestProxyConfigServiceObserver
    : public net::ProxyConfigService::Observer {
 public:
  TestProxyConfigServiceObserver() = default;

  TestProxyConfigServiceObserver(const TestProxyConfigServiceObserver&) =
      delete;
  TestProxyConfigServiceObserver& operator=(
      const TestProxyConfigServiceObserver&) = delete;

  ~TestProxyConfigServiceObserver() override { Observe(nullptr); }

  // Switches observation to `service` (or detaches when `service` is null) so
  // `service_` never dangles when `ProxyConfigServiceMojoTest` recreates its
  // `ProxyConfigServiceMojo` instance.
  void Observe(net::ProxyConfigService* service) {
    if (service_) {
      service_->RemoveObserver(this);
    }
    service_ = service;
    if (service_) {
      service_->AddObserver(this);
    }
  }

  void OnProxyConfigChanged(
      const net::ProxyConfigWithAnnotation& config,
      net::ProxyConfigService::ConfigAvailability availability) override {
    // The ProxyConfigServiceMojo only sends on availability state.
    EXPECT_EQ(net::ProxyConfigService::CONFIG_VALID, availability);

    observed_config_ = config;

    // The passed in config should match the one that GetLatestProxyConfig
    // returns.
    net::ProxyConfigWithAnnotation retrieved_config;
    EXPECT_EQ(net::ProxyConfigService::CONFIG_VALID,
              service_->GetLatestProxyConfig(&retrieved_config));
    EXPECT_TRUE(observed_config_.value().Equals(retrieved_config.value()));
    ++config_changes_;
  }

  // Returns number of observed config changes since it was last called.
  int GetAndResetConfigChanges() {
    int result = config_changes_;
    config_changes_ = 0;
    return result;
  }

  // Returns last observed config.
  const net::ProxyConfigWithAnnotation& observed_config() const {
    return observed_config_;
  }

 private:
  net::ProxyConfigWithAnnotation observed_config_;

  raw_ptr<net::ProxyConfigService> service_ = nullptr;
  int config_changes_ = 0;
};

// Test implementation of `mojom::ProxyConfigPollerClient` that counts incoming
// `OnLazyProxyConfigPoll()` IPCs sent by `ProxyConfigServiceMojo`.
class TestProxyConfigPollerClient : public mojom::ProxyConfigPollerClient {
 public:
  TestProxyConfigPollerClient() = default;
  TestProxyConfigPollerClient(const TestProxyConfigPollerClient&) = delete;
  TestProxyConfigPollerClient& operator=(const TestProxyConfigPollerClient&) =
      delete;
  ~TestProxyConfigPollerClient() override = default;

  mojo::PendingRemote<mojom::ProxyConfigPollerClient>
  BindNewPipeAndPassRemote() {
    receiver_.reset();
    return receiver_.BindNewPipeAndPassRemote();
  }

  void OnLazyProxyConfigPoll() override { ++poll_count_; }

  int GetAndResetPollCount() {
    int count = poll_count_;
    poll_count_ = 0;
    return count;
  }

 private:
  int poll_count_ = 0;
  mojo::Receiver<mojom::ProxyConfigPollerClient> receiver_{this};
};

// Test fixture for notifying ProxyConfigServiceMojo of changes through the
// client interface, watching the subsequent values it emits to registered
// net::ProxyConfigService::Observers, and verifying lazy poll rate-limiting.
class ProxyConfigServiceMojoTest : public testing::Test {
 public:
  ProxyConfigServiceMojoTest() { CreateProxyConfigService(); }

  ProxyConfigServiceMojoTest(const ProxyConfigServiceMojoTest&) = delete;
  ProxyConfigServiceMojoTest& operator=(const ProxyConfigServiceMojoTest&) =
      delete;

  ~ProxyConfigServiceMojoTest() override = default;

 protected:
  // Creates (or recreates) `proxy_config_service_` and rebinds
  // `config_client_`, `poller_client_` (when `bind_poller_client` is true), and
  // `observer_`. Tests that override `base::FeatureList` via
  // `ScopedFeatureList` call this after initializing feature overrides so
  // `ProxyConfigServiceMojo`'s constructor caches the overridden feature state.
  void CreateProxyConfigService(bool bind_poller_client = true) {
    observer_.Observe(nullptr);
    proxy_config_service_.reset();
    config_client_.reset();

    proxy_config_service_ = std::make_unique<ProxyConfigServiceMojo>(
        config_client_.BindNewPipeAndPassReceiver(),
        /*initial_proxy_config=*/std::nullopt,
        bind_poller_client ? poller_client_.BindNewPipeAndPassRemote()
                           : mojo::NullRemote());
    observer_.Observe(proxy_config_service_.get());
  }

  // After notifying a new configuration through |config_client_|, waits for the
  // observers to have been notified.
  void WaitForConfig() { config_client_.FlushForTesting(); }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::IO,
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  mojo::Remote<mojom::ProxyConfigClient> config_client_;
  TestProxyConfigPollerClient poller_client_;
  std::unique_ptr<ProxyConfigServiceMojo> proxy_config_service_;
  TestProxyConfigServiceObserver observer_;
};

// Most tests of this class are in network_context_unittests.

// Makes sure that a ProxyConfigService::Observer is correctly notified of
// changes when the ProxyConfig changes, and is not informed of them in the case
// of "changes" that result in the same ProxyConfig as before.
TEST_F(ProxyConfigServiceMojoTest, ObserveProxyChanges) {
  net::ProxyConfigWithAnnotation temp;
  // The service should start without a config.
  EXPECT_EQ(net::ProxyConfigService::CONFIG_PENDING,
            proxy_config_service_->GetLatestProxyConfig(&temp));

  std::array<net::ProxyConfig, 3> proxy_configs;
  proxy_configs[0].proxy_rules().ParseFromString("http=foopy:80");
  proxy_configs[1].proxy_rules().ParseFromString("http=foopy:80;ftp=foopy2");
  proxy_configs[2] = net::ProxyConfig::CreateDirect();

  for (const auto& proxy_config : proxy_configs) {
    // Set the proxy configuration to something that does not match the old one.
    config_client_->OnProxyConfigUpdated(net::ProxyConfigWithAnnotation(
        proxy_config, TRAFFIC_ANNOTATION_FOR_TESTS));
    WaitForConfig();
    EXPECT_EQ(1, observer_.GetAndResetConfigChanges());
    EXPECT_TRUE(proxy_config.Equals(observer_.observed_config().value()));
    net::ProxyConfigWithAnnotation retrieved_config;
    EXPECT_EQ(net::ProxyConfigService::CONFIG_VALID,
              proxy_config_service_->GetLatestProxyConfig(&retrieved_config));
    EXPECT_TRUE(proxy_config.Equals(retrieved_config.value()));

    // Set the proxy configuration to the same value again. There should be not
    // be another proxy config changed notification.
    config_client_->OnProxyConfigUpdated(net::ProxyConfigWithAnnotation(
        proxy_config, TRAFFIC_ANNOTATION_FOR_TESTS));
    WaitForConfig();
    EXPECT_EQ(0, observer_.GetAndResetConfigChanges());
    EXPECT_TRUE(proxy_config.Equals(observer_.observed_config().value()));
    EXPECT_EQ(net::ProxyConfigService::CONFIG_VALID,
              proxy_config_service_->GetLatestProxyConfig(&retrieved_config));
    EXPECT_TRUE(proxy_config.Equals(retrieved_config.value()));
  }
}

// Creates a URL that has length |url::kMaxURLChars + 1|.
GURL CreateLargeURL() {
  std::string spec;
  spec.reserve(url::kMaxURLChars + 1);
  spec.assign("http://test.invalid/");
  spec.append(url::kMaxURLChars + 1 - spec.size(), 'x');
  return GURL(spec);
}

// Tests what happens when ProxyConfigServiceMojo is updated to using a
// ProxyConfig with a large URL. GURL does not impose size limits, however some
// internals like url.mojom.Url do.
TEST_F(ProxyConfigServiceMojoTest, LargePacUrlNotTruncated) {
  // Create a config using a large, valid, PAC URL.
  net::ProxyConfig orig_config;
  GURL large_url = CreateLargeURL();
  EXPECT_TRUE(large_url.is_valid());
  EXPECT_EQ(url::kMaxURLChars + 1, large_url.possibly_invalid_spec().size());
  orig_config.set_pac_url(large_url);

  // Notify the ProxyConfigServiceMojo of this URL through the client interface.
  config_client_->OnProxyConfigUpdated(net::ProxyConfigWithAnnotation(
      orig_config, TRAFFIC_ANNOTATION_FOR_TESTS));

  WaitForConfig();

  // Read back the ProxyConfig that was observed (which has been serialized
  // through a Mojo pipe).
  const GURL& observed_url = observer_.observed_config().value().pac_url();

  // The URL should be unchanged, and not changed by the Mojo serialization.
  EXPECT_EQ(large_url, observed_url);
  EXPECT_EQ(url::kMaxURLChars + 1, observed_url.possibly_invalid_spec().size());
}

// Verifies that when `kRateLimitProxyConfigPolls` is disabled (Control arm),
// every `OnLazyPoll()` call forwards an `OnLazyProxyConfigPoll()` Mojo message
// without throttling.
TEST_F(ProxyConfigServiceMojoTest, OnLazyPollUnthrottledWhenFeatureDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      features::kRateLimitProxyConfigPolls);
  CreateProxyConfigService();

  for (int i = 0; i < 5; ++i) {
    proxy_config_service_->OnLazyPoll();
  }
  proxy_config_service_->FlushPollerForTesting();
  EXPECT_EQ(5, poller_client_.GetAndResetPollCount());
}

// Verifies that when `kRateLimitProxyConfigPolls` is enabled with its default
// 1-second interval, the first `OnLazyPoll()` forwards immediately, burst calls
// within the cooldown window are suppressed, and polling resumes once the
// minimum interval elapses.
TEST_F(ProxyConfigServiceMojoTest, OnLazyPollRateLimitedWhenFeatureEnabled) {
  // Matches the default of `features::kRateLimitProxyConfigPollsInterval` so
  // this can switch to the production constant once the feature is permanently
  // enabled with a fixed interval.
  constexpr base::TimeDelta kDefaultInterval = base::Seconds(1);
  constexpr base::TimeDelta kDelta = base::Milliseconds(1);

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kRateLimitProxyConfigPolls);
  CreateProxyConfigService();

  // The initial poll at t=0 must forward immediately, while subsequent burst
  // calls at the same timestamp are suppressed.
  for (int i = 0; i < 50; ++i) {
    proxy_config_service_->OnLazyPoll();
  }
  proxy_config_service_->FlushPollerForTesting();
  EXPECT_EQ(1, poller_client_.GetAndResetPollCount());

  // Advancing just short of `kDefaultInterval` keeps suppression active.
  task_environment_.FastForwardBy(kDefaultInterval - kDelta);
  for (int i = 0; i < 50; ++i) {
    proxy_config_service_->OnLazyPoll();
  }
  proxy_config_service_->FlushPollerForTesting();
  EXPECT_EQ(0, poller_client_.GetAndResetPollCount());

  // Reaching `kDefaultInterval` allows the next lazy poll through and starts a
  // fresh cooldown window.
  task_environment_.FastForwardBy(kDelta);
  for (int i = 0; i < 10; ++i) {
    proxy_config_service_->OnLazyPoll();
  }
  proxy_config_service_->FlushPollerForTesting();
  EXPECT_EQ(1, poller_client_.GetAndResetPollCount());
}

// Verifies that a custom `RateLimitProxyConfigPollsInterval` feature parameter
// overrides the default cooldown window.
TEST_F(ProxyConfigServiceMojoTest, OnLazyPollCustomIntervalParam) {
  constexpr base::TimeDelta kCustomInterval = base::Seconds(5);
  constexpr base::TimeDelta kDelta = base::Milliseconds(1);

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kRateLimitProxyConfigPolls,
      {{features::kRateLimitProxyConfigPollsInterval.name, "5s"}});
  CreateProxyConfigService();

  proxy_config_service_->OnLazyPoll();
  proxy_config_service_->FlushPollerForTesting();
  EXPECT_EQ(1, poller_client_.GetAndResetPollCount());

  // Advancing just short of `kCustomInterval` keeps suppression active.
  task_environment_.FastForwardBy(kCustomInterval - kDelta);
  proxy_config_service_->OnLazyPoll();
  proxy_config_service_->FlushPollerForTesting();
  EXPECT_EQ(0, poller_client_.GetAndResetPollCount());

  // Reaching `kCustomInterval` allows the next lazy poll through.
  task_environment_.FastForwardBy(kDelta);
  proxy_config_service_->OnLazyPoll();
  proxy_config_service_->FlushPollerForTesting();
  EXPECT_EQ(1, poller_client_.GetAndResetPollCount());
}

// Verifies that platforms passing `mojo::NullRemote()` for
// `proxy_poller_client` (such as macOS, Linux, ChromeOS, and Android) do not
// query `kRateLimitProxyConfigPolls` or activate its associated field trial,
// whereas polling platforms do activate the trial upon construction.
TEST_F(ProxyConfigServiceMojoTest,
       NonPollingPlatformDoesNotActivateFieldTrial) {
  constexpr char kTrialName[] = "RateLimitProxyConfigPollsStudy";
  base::test::ScopedFeatureList scoped_feature_list;
  auto feature_list = std::make_unique<base::FeatureList>();
  base::FieldTrial* trial =
      base::FieldTrialList::CreateFieldTrial(kTrialName, "Enabled");
  feature_list->RegisterFieldTrialOverride(
      features::kRateLimitProxyConfigPolls.name,
      base::FeatureList::OVERRIDE_ENABLE_FEATURE, trial);
  scoped_feature_list.InitWithFeatureList(std::move(feature_list));

  ASSERT_FALSE(base::FieldTrialList::IsTrialActive(kTrialName));

  // Constructing without a poller client must not activate the field trial.
  CreateProxyConfigService(/*bind_poller_client=*/false);
  proxy_config_service_->OnLazyPoll();
  EXPECT_FALSE(base::FieldTrialList::IsTrialActive(kTrialName));

  // Constructing with a valid poller client must activate the field trial.
  CreateProxyConfigService(/*bind_poller_client=*/true);
  EXPECT_TRUE(base::FieldTrialList::IsTrialActive(kTrialName));
}

}  // namespace

}  // namespace network
