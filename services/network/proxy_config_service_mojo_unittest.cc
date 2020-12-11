// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/proxy_config_service_mojo.h"

#include "base/macros.h"
#include "base/test/task_environment.h"
#include "net/proxy_resolution/proxy_config.h"
#include "net/proxy_resolution/proxy_config_service.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/mojom/proxy_config.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {

namespace {

// Test class for observing proxy config changes.
class TestProxyConfigServiceObserver
    : public net::ProxyConfigService::Observer {
 public:
  explicit TestProxyConfigServiceObserver(net::ProxyConfigService* service)
      : service_(service) {}
  ~TestProxyConfigServiceObserver() override {}

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

  net::ProxyConfigService* const service_;
  int config_changes_ = 0;

  DISALLOW_COPY_AND_ASSIGN(TestProxyConfigServiceObserver);
};

// Test fixture for notifying ProxyConfigServiceMojo of changes through the
// client interface, and watching the subsequent values it emits to registered
// net::ProxyConfigService::Observers.
class ProxyConfigServiceMojoTest : public testing::Test {
 public:
  ProxyConfigServiceMojoTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::IO),
        proxy_config_service_(config_client_.BindNewPipeAndPassReceiver(),
                              base::Optional<net::ProxyConfigWithAnnotation>(),
                              mojo::NullRemote()),
        observer_(&proxy_config_service_) {
    proxy_config_service_.AddObserver(&observer_);
  }

  ~ProxyConfigServiceMojoTest() override {
    proxy_config_service_.RemoveObserver(&observer_);
  }

 protected:
  // After notifying a new configuration through |config_client_|, waits for the
  // observers to have been notified.
  void WaitForConfig() { task_environment_.RunUntilIdle(); }

  base::test::TaskEnvironment task_environment_;
  mojo::Remote<mojom::ProxyConfigClient> config_client_;
  ProxyConfigServiceMojo proxy_config_service_;
  TestProxyConfigServiceObserver observer_;

  DISALLOW_COPY_AND_ASSIGN(ProxyConfigServiceMojoTest);
};

// Most tests of this class are in network_context_unittests.

// Makes sure that a ProxyConfigService::Observer is correctly notified of
// changes when the ProxyConfig changes, and is not informed of them in the case
// of "changes" that result in the same ProxyConfig as before.
TEST_F(ProxyConfigServiceMojoTest, ObserveProxyChanges) {
  net::ProxyConfigWithAnnotation proxy_config;
  // The service should start without a config.
  EXPECT_EQ(net::ProxyConfigService::CONFIG_PENDING,
            proxy_config_service_.GetLatestProxyConfig(&proxy_config));

  net::ProxyConfig proxy_configs[3];
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
              proxy_config_service_.GetLatestProxyConfig(&retrieved_config));
    EXPECT_TRUE(proxy_config.Equals(retrieved_config.value()));

    // Set the proxy configuration to the same value again. There should be not
    // be another proxy config changed notification.
    config_client_->OnProxyConfigUpdated(net::ProxyConfigWithAnnotation(
        proxy_config, TRAFFIC_ANNOTATION_FOR_TESTS));
    WaitForConfig();
    EXPECT_EQ(0, observer_.GetAndResetConfigChanges());
    EXPECT_TRUE(proxy_config.Equals(observer_.observed_config().value()));
    EXPECT_EQ(net::ProxyConfigService::CONFIG_VALID,
              proxy_config_service_.GetLatestProxyConfig(&retrieved_config));
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

}  // namespace

}  // namespace network
