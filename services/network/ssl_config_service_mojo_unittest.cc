// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "services/network/ssl_config_service_mojo.h"

#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "crypto/sha2.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/test_completion_callback.h"
#include "net/cert/asn1_util.h"
#include "net/cert/cert_verifier.h"
#include "net/cert/cert_verify_result.h"
#include "net/cert/crl_set.h"
#include "net/cert/test_root_certs.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"
#include "net/log/net_log_with_source.h"
#include "net/ssl/ssl_config.h"
#include "net/ssl/ssl_config_service.h"
#include "net/test/cert_test_util.h"
#include "net/test/gtest_util.h"
#include "net/test/test_data_directory.h"
#include "net/url_request/url_request_context.h"
#include "services/network/network_context.h"
#include "services/network/network_service.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "services/network/public/mojom/ssl_config.mojom.h"
#include "services/network/test/fake_test_cert_verifier_params_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {
namespace {

class TestSSLConfigServiceObserver : public net::SSLConfigService::Observer {
 public:
  explicit TestSSLConfigServiceObserver(
      net::SSLConfigService* ssl_config_service)
      : ssl_config_service_(ssl_config_service) {
    ssl_config_service_->AddObserver(this);
  }

  ~TestSSLConfigServiceObserver() override {
    EXPECT_FALSE(config_changed_call_.IsReady())
        << "Unexpected calls to OnSSLContextConfigChanged()";
    ssl_config_service_->RemoveObserver(this);
  }

  // net::SSLConfigService::Observer implementation:
  void OnSSLContextConfigChanged() override {
    config_changed_call_.SetValue(ssl_config_service_->GetSSLContextConfig());
  }

  // Waits for a SSLContextConfig change. The first time it's called, waits for
  // the first change, if one hasn't been observed already, the second time,
  // waits for the second, etc. Must be called once for each change that
  // happens, and fails if more than once change happens between calls, or
  // during a call.
  net::SSLContextConfig WaitForChange() {
    EXPECT_TRUE(config_changed_call_.Wait())
        << "Missing call to OnSSLContextConfigChanged()";
    return config_changed_call_.Take();
  }

 private:
  const raw_ptr<net::SSLConfigService> ssl_config_service_;

  // Most recent call to OnSSLContextConfigChanged().
  base::test::TestFuture<net::SSLContextConfig> config_changed_call_;
};

class TestCertVerifierConfigObserver : public net::CertVerifier {
 public:
  TestCertVerifierConfigObserver() = default;
  ~TestCertVerifierConfigObserver() override {
    EXPECT_FALSE(set_config_call_.IsReady())
        << "Unexpected call to SetConfig()";
  }

  // CertVerifier implementation:
  int Verify(const net::CertVerifier::RequestParams& params,
             net::CertVerifyResult* verify_result,
             net::CompletionOnceCallback callback,
             std::unique_ptr<net::CertVerifier::Request>* out_req,
             const net::NetLogWithSource& net_log) override {
    ADD_FAILURE() << "Verify should not be called by tests";
    return net::ERR_FAILED;
  }
  void SetConfig(const Config& config) override {
    set_config_call_.SetValue(config);
  }
  void AddObserver(Observer* observer) override {}
  void RemoveObserver(Observer* observer) override {}

  // Waits for a SSLConfig change. The first time it's called, waits for the
  // first change, if one hasn't been observed already, the second time, waits
  // for the second, etc. Must be called once for each change that happens, and
  // fails it more than once change happens between calls, or during a call.
  net::CertVerifier::Config WaitForChange() {
    EXPECT_TRUE(set_config_call_.Wait()) << "Missing call to SetConfig()";
    return set_config_call_.Take();
  }

 private:
  // Most recent call to SetConfig().
  base::test::TestFuture<Config> set_config_call_;
};

class NetworkServiceSSLConfigServiceTest : public testing::Test {
 public:
  NetworkServiceSSLConfigServiceTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::IO),
        network_service_(NetworkService::CreateForTesting()) {}
  ~NetworkServiceSSLConfigServiceTest() override {
    NetworkContext::SetCertVerifierForTesting(nullptr);
  }

  // Creates a NetworkContext using the specified NetworkContextParams, and
  // stores it in |network_context_|.
  void SetUpNetworkContext(
      mojom::NetworkContextParamsPtr network_context_params) {
    // Use a dummy CertVerifier that always passes cert verification, since
    // these unittests don't need to test the behavior of a real CertVerifier.
    // There are a parallel set of tests in services/cert_verifier/ that *do*
    // test CertVerifier behavior.
    network_context_params->cert_verifier_params =
        FakeTestCertVerifierParamsFactory::GetCertVerifierParams();
    ssl_config_client_.reset();
    network_context_params->ssl_config_client_receiver =
        ssl_config_client_.BindNewPipeAndPassReceiver();
    network_context_remote_.reset();
    network_context_ = std::make_unique<NetworkContext>(
        network_service_.get(),
        network_context_remote_.BindNewPipeAndPassReceiver(),
        std::move(network_context_params));
  }

  // Returns the current SSLContextConfig for |network_context_|.
  net::SSLContextConfig GetSSLContextConfig() {
    return network_context_->url_request_context()
        ->ssl_config_service()
        ->GetSSLContextConfig();
  }

  // Runs two conversion tests for |mojo_config|.  Uses it as a initial
  // SSLConfig for a NetworkContext, making sure it matches
  // |expected_net_config|. Then switches to the default configuration and then
  // back to |mojo_config|, to make sure it works as a new configuration. The
  // expected configuration must not be the default configuration.
  void RunConversionTests(const mojom::SSLConfig& mojo_config,
                          const net::SSLContextConfig& expected_net_config) {
    // The expected configuration must not be the default configuration, or the
    // change test won't send an event.
    EXPECT_NE(net::SSLContextConfig(), expected_net_config);

    // Set up |mojo_config| as the initial configuration of a NetworkContext.
    mojom::NetworkContextParamsPtr network_context_params =
        mojom::NetworkContextParams::New();
    network_context_params->initial_ssl_config = mojo_config.Clone();
    SetUpNetworkContext(std::move(network_context_params));
    EXPECT_EQ(GetSSLContextConfig(), expected_net_config);
    // Sanity check.
    EXPECT_NE(GetSSLContextConfig(), net::SSLContextConfig());

    // Reset the configuration to the default ones, and check the results.
    TestSSLConfigServiceObserver observer(
        network_context_->url_request_context()->ssl_config_service());
    ssl_config_client_->OnSSLConfigUpdated(mojom::SSLConfig::New());
    net::SSLContextConfig config_during_change = observer.WaitForChange();
    EXPECT_EQ(GetSSLContextConfig(), net::SSLContextConfig());
    EXPECT_EQ(config_during_change, net::SSLContextConfig());
    // Sanity check.
    EXPECT_NE(GetSSLContextConfig(), expected_net_config);

    // Set the configuration to |mojo_config| again, and check the results.
    ssl_config_client_->OnSSLConfigUpdated(mojo_config.Clone());
    config_during_change = observer.WaitForChange();
    EXPECT_EQ(GetSSLContextConfig(), expected_net_config);
    EXPECT_EQ(config_during_change, expected_net_config);
  }

  // Runs two conversion tests for |mojo_config|.  Uses it as an initial
  // net::CertVerifier::Config for a NetworkContext, making sure it matches
  // |expected_net_config|. Then switches to the default configuration and then
  // back to |mojo_config|, to make sure it works as a new configuration. The
  // expected configuration must not be the default configuration.
  void RunCertConversionTests(
      const mojom::SSLConfig& mojo_config,
      const net::CertVerifier::Config& expected_net_config) {
    TestCertVerifierConfigObserver observer;
    NetworkContext::SetCertVerifierForTesting(&observer);

    EXPECT_NE(net::CertVerifier::Config(), expected_net_config);

    // Set up |mojo_config| as the initial configuration of a NetworkContext.
    mojom::NetworkContextParamsPtr network_context_params =
        mojom::NetworkContextParams::New();
    network_context_params->initial_ssl_config = mojo_config.Clone();
    SetUpNetworkContext(std::move(network_context_params));

    // Make sure the initial configuration is set.
    net::CertVerifier::Config config_during_change = observer.WaitForChange();
    EXPECT_EQ(config_during_change, expected_net_config);
    // Sanity check.
    EXPECT_NE(config_during_change, net::CertVerifier::Config());

    // Reset the configuration to the default ones, and check the results.
    ssl_config_client_->OnSSLConfigUpdated(mojom::SSLConfig::New());
    config_during_change = observer.WaitForChange();
    EXPECT_EQ(config_during_change, net::CertVerifier::Config());
    // Sanity check.
    EXPECT_NE(config_during_change, expected_net_config);

    // Set the configuration to |mojo_config| again, and check the results.
    ssl_config_client_->OnSSLConfigUpdated(mojo_config.Clone());
    config_during_change = observer.WaitForChange();
    EXPECT_EQ(config_during_change, expected_net_config);

    // Reset the CertVerifier for subsequent invocations.
    NetworkContext::SetCertVerifierForTesting(nullptr);
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<NetworkService> network_service_;
  mojo::Remote<mojom::SSLConfigClient> ssl_config_client_;
  mojo::Remote<mojom::NetworkContext> network_context_remote_;
  std::unique_ptr<NetworkContext> network_context_;
};

// Check that passing in a no mojom::SSLConfig matches the default
// net::SSLConfig.
TEST_F(NetworkServiceSSLConfigServiceTest, NoSSLConfig) {
  SetUpNetworkContext(mojom::NetworkContextParams::New());
  EXPECT_EQ(GetSSLContextConfig(), net::SSLContextConfig());

  // Make sure the default TLS version range is as expected.
  EXPECT_EQ(net::kDefaultSSLVersionMin, GetSSLContextConfig().version_min);
  EXPECT_EQ(net::kDefaultSSLVersionMax, GetSSLContextConfig().version_max);
}

// Check that passing in the default mojom::SSLConfig matches the default
// net::SSLConfig.
TEST_F(NetworkServiceSSLConfigServiceTest, Default) {
  mojom::NetworkContextParamsPtr network_context_params =
      mojom::NetworkContextParams::New();
  network_context_params->initial_ssl_config = mojom::SSLConfig::New();
  SetUpNetworkContext(std::move(network_context_params));
  EXPECT_EQ(GetSSLContextConfig(), net::SSLContextConfig());

  // Make sure the default TLS version range is as expected.
  EXPECT_EQ(net::kDefaultSSLVersionMin, GetSSLContextConfig().version_min);
  EXPECT_EQ(net::kDefaultSSLVersionMax, GetSSLContextConfig().version_max);
}

// Check that passing in the default mojom::SSLConfig matches the default
// net::CertVerifier::Config.
TEST_F(NetworkServiceSSLConfigServiceTest, DefaultCertConfig) {
  TestCertVerifierConfigObserver observer;
  NetworkContext::SetCertVerifierForTesting(&observer);

  mojom::NetworkContextParamsPtr network_context_params =
      mojom::NetworkContextParams::New();
  network_context_params->initial_ssl_config = mojom::SSLConfig::New();
  SetUpNetworkContext(std::move(network_context_params));

  net::CertVerifier::Config config_during_change = observer.WaitForChange();

  net::CertVerifier::Config default_config;
  EXPECT_EQ(config_during_change, default_config);

  NetworkContext::SetCertVerifierForTesting(nullptr);
}

TEST_F(NetworkServiceSSLConfigServiceTest, RevCheckingEnabled) {
  net::CertVerifier::Config expected_net_config;
  // Use the opposite of the default value.
  expected_net_config.enable_rev_checking =
      !expected_net_config.enable_rev_checking;

  mojom::SSLConfigPtr mojo_config = mojom::SSLConfig::New();
  mojo_config->rev_checking_enabled = expected_net_config.enable_rev_checking;

  RunCertConversionTests(*mojo_config, expected_net_config);
}

TEST_F(NetworkServiceSSLConfigServiceTest,
       RevCheckingRequiredLocalTrustAnchors) {
  net::CertVerifier::Config expected_net_config;
  // Use the opposite of the default value.
  expected_net_config.require_rev_checking_local_anchors =
      !expected_net_config.require_rev_checking_local_anchors;

  mojom::SSLConfigPtr mojo_config = mojom::SSLConfig::New();
  mojo_config->rev_checking_required_local_anchors =
      expected_net_config.require_rev_checking_local_anchors;

  RunCertConversionTests(*mojo_config, expected_net_config);
}

TEST_F(NetworkServiceSSLConfigServiceTest, Sha1LocalAnchorsEnabled) {
  net::CertVerifier::Config expected_net_config;
  // Use the opposite of the default value.
  expected_net_config.enable_sha1_local_anchors =
      !expected_net_config.enable_sha1_local_anchors;

  mojom::SSLConfigPtr mojo_config = mojom::SSLConfig::New();
  mojo_config->sha1_local_anchors_enabled =
      expected_net_config.enable_sha1_local_anchors;

  RunCertConversionTests(*mojo_config, expected_net_config);
}

TEST_F(NetworkServiceSSLConfigServiceTest, SymantecEnforcementDisabled) {
  net::CertVerifier::Config expected_net_config;
  // Use the opposite of the default value.
  expected_net_config.disable_symantec_enforcement =
      !expected_net_config.disable_symantec_enforcement;

  mojom::SSLConfigPtr mojo_config = mojom::SSLConfig::New();
  mojo_config->symantec_enforcement_disabled =
      expected_net_config.disable_symantec_enforcement;

  RunCertConversionTests(*mojo_config, expected_net_config);
}

TEST_F(NetworkServiceSSLConfigServiceTest, SSLVersion) {
  const struct {
    mojom::SSLVersion mojo_ssl_version;
    int net_ssl_version;
  } kVersionTable[] = {
      {mojom::SSLVersion::kTLS12, net::SSL_PROTOCOL_VERSION_TLS1_2},
      {mojom::SSLVersion::kTLS13, net::SSL_PROTOCOL_VERSION_TLS1_3},
  };

  for (size_t min_index = 0; min_index < std::size(kVersionTable);
       ++min_index) {
    for (size_t max_index = min_index; max_index < std::size(kVersionTable);
         ++max_index) {
      // If the versions match the default values, skip this value in the table.
      // The defaults will get plenty of testing anyways, when switching back to
      // the default values in RunConversionTests().
      if (kVersionTable[min_index].net_ssl_version ==
              net::SSLContextConfig().version_min &&
          kVersionTable[max_index].net_ssl_version ==
              net::SSLContextConfig().version_max) {
        continue;
      }
      net::SSLContextConfig expected_net_config;
      expected_net_config.version_min =
          kVersionTable[min_index].net_ssl_version;
      expected_net_config.version_max =
          kVersionTable[max_index].net_ssl_version;

      mojom::SSLConfigPtr mojo_config = mojom::SSLConfig::New();
      mojo_config->version_min = kVersionTable[min_index].mojo_ssl_version;
      mojo_config->version_max = kVersionTable[max_index].mojo_ssl_version;

      RunConversionTests(*mojo_config, expected_net_config);
    }
  }
}

TEST_F(NetworkServiceSSLConfigServiceTest, InitialConfigDisableCipherSuite) {
  net::SSLContextConfig expected_net_config;
  expected_net_config.disabled_cipher_suites.push_back(0x0004);

  mojom::SSLConfigPtr mojo_config = mojom::SSLConfig::New();
  mojo_config->disabled_cipher_suites =
      expected_net_config.disabled_cipher_suites;

  RunConversionTests(*mojo_config, expected_net_config);
}

TEST_F(NetworkServiceSSLConfigServiceTest,
       InitialConfigDisableTwoCipherSuites) {
  net::SSLContextConfig expected_net_config;
  expected_net_config.disabled_cipher_suites.push_back(0x0004);
  expected_net_config.disabled_cipher_suites.push_back(0x0005);

  mojom::SSLConfigPtr mojo_config = mojom::SSLConfig::New();
  mojo_config->disabled_cipher_suites =
      expected_net_config.disabled_cipher_suites;

  RunConversionTests(*mojo_config, expected_net_config);
}

TEST_F(NetworkServiceSSLConfigServiceTest, CanShareConnectionWithClientCerts) {
  // Create a default NetworkContext and test that
  // CanShareConnectionWithClientCerts returns false.
  SetUpNetworkContext(mojom::NetworkContextParams::New());

  net::SSLConfigService* config_service =
      network_context_->url_request_context()->ssl_config_service();

  EXPECT_FALSE(
      config_service->CanShareConnectionWithClientCerts("example.com"));
  EXPECT_FALSE(
      config_service->CanShareConnectionWithClientCerts("example.net"));

  // Configure policy to allow example.com (but no subdomains), and example.net
  // (including subdomains), update the config, and test that pooling is allowed
  // with this policy.
  mojom::SSLConfigPtr mojo_config = mojom::SSLConfig::New();
  mojo_config->client_cert_pooling_policy = {".example.com", "example.net"};

  TestSSLConfigServiceObserver observer(config_service);
  ssl_config_client_->OnSSLConfigUpdated(std::move(mojo_config));
  observer.WaitForChange();

  EXPECT_TRUE(config_service->CanShareConnectionWithClientCerts("example.com"));
  EXPECT_FALSE(
      config_service->CanShareConnectionWithClientCerts("sub.example.com"));

  EXPECT_TRUE(config_service->CanShareConnectionWithClientCerts("example.net"));
  EXPECT_TRUE(
      config_service->CanShareConnectionWithClientCerts("sub.example.net"));
  EXPECT_TRUE(
      config_service->CanShareConnectionWithClientCerts("sub.sub.example.net"));
  EXPECT_FALSE(
      config_service->CanShareConnectionWithClientCerts("notexample.net"));

  EXPECT_FALSE(
      config_service->CanShareConnectionWithClientCerts("example.org"));

  // Reset the configuration to the default and check that pooling is no longer
  // allowed.
  ssl_config_client_->OnSSLConfigUpdated(mojom::SSLConfig::New());
  observer.WaitForChange();

  EXPECT_FALSE(
      config_service->CanShareConnectionWithClientCerts("example.com"));
  EXPECT_FALSE(
      config_service->CanShareConnectionWithClientCerts("example.net"));
}

}  // namespace
}  // namespace network
