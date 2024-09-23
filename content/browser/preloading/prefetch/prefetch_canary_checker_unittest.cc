// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prefetch/prefetch_canary_checker.h"

#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "net/base/address_list.h"
#include "net/base/host_port_pair.h"
#include "net/base/net_errors.h"
#include "net/dns/public/resolve_error_info.h"
#include "services/network/public/mojom/host_resolver.mojom.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "services/network/test/test_network_context.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

const base::TimeDelta kCacheRevalidateAfter = base::Days(1);

class FakeNetworkContext : public network::TestNetworkContext {
 public:
  explicit FakeNetworkContext(
      mojo::PendingReceiver<network::mojom::NetworkContext> receiver)
      : receiver_(this, std::move(receiver)) {}

  void ResolveHost(
      network::mojom::HostResolverHostPtr host,
      const net::NetworkAnonymizationKey& network_anonymization_key,
      network::mojom::ResolveHostParametersPtr optional_parameters,
      mojo::PendingRemote<network::mojom::ResolveHostClient> response_client)
      override {
    net::HostPortPair host_port_pair =
        host->is_host_port_pair()
            ? host->get_host_port_pair()
            : net::HostPortPair(host->get_scheme_host_port().host(),
                                host->get_scheme_host_port().port());
    EXPECT_TRUE(pending_requests_.find(host_port_pair) ==
                pending_requests_.end());
    auto request = std::make_unique<ResolveHostRequest>(
        this, host_port_pair, std::move(response_client),
        std::move(optional_parameters->control_handle));
    pending_requests_.emplace(host_port_pair, std::move(request));
    num_requests_made_++;
  }

  void MakeDNSResolveSuccess(const GURL& url) {
    const net::IPEndPoint kFakeIPAddress{
        net::IPEndPoint(net::IPAddress::IPv4Localhost(), /*port=*/1234)};
    std::optional<net::AddressList> resolved_addresses =
        net::AddressList(kFakeIPAddress);
    auto it = pending_requests_.find(net::HostPortPair::FromURL(url));
    // Make sure a request has actually been made.
    EXPECT_TRUE(it != pending_requests_.end());
    it->second->OnComplete(net::OK, resolved_addresses,
                           /*endpoint_results_with_metadata=*/std::nullopt);
    pending_requests_.erase(it);
  }

  void MakeDNSResolveError(const GURL& url, net::Error err) {
    MakeDNSResolveError(net::HostPortPair::FromURL(url), err);
  }

  void MakeDNSResolveError(const net::HostPortPair& host, net::Error err) {
    auto it = pending_requests_.find(host);
    // Make sure a request has actually been made.
    EXPECT_TRUE(it != pending_requests_.end());

    it->second->OnComplete(err, /*resolved_addresses=*/std::nullopt,
                           /*endpoint_results_with_metadata=*/std::nullopt);
    pending_requests_.erase(it);
  }

  size_t NumPendingRequests() { return pending_requests_.size(); }
  size_t NumRequestsMade() { return num_requests_made_; }

 private:
  class ResolveHostRequest : public network::mojom::ResolveHostHandle {
   public:
    ResolveHostRequest(
        FakeNetworkContext* network_context,
        net::HostPortPair host,
        mojo::PendingRemote<network::mojom::ResolveHostClient> response_client,
        mojo::PendingReceiver<network::mojom::ResolveHostHandle> control_handle)
        : network_context_(network_context),
          host_(host),
          response_client_(std::move(response_client)) {
      control_handle_receiver_.Bind(std::move(control_handle));
    }

    // ResolveHostHandle override.
    void Cancel(int error) override {
      network_context_->MakeDNSResolveError(host_,
                                            static_cast<net::Error>(error));
    }

    void OnComplete(net::Error err,
                    std::optional<net::AddressList> resolved_addresses,
                    std::optional<net::HostResolverEndpointResults>
                        endpoint_results_with_metadata) {
      response_client_->OnComplete(err, net::ResolveErrorInfo(),
                                   resolved_addresses,
                                   endpoint_results_with_metadata);
    }

   private:
    raw_ptr<FakeNetworkContext> network_context_;
    net::HostPortPair host_;
    mojo::Receiver<network::mojom::ResolveHostHandle> control_handle_receiver_{
        this};
    mojo::Remote<network::mojom::ResolveHostClient> response_client_;
  };

  mojo::Receiver<network::mojom::NetworkContext> receiver_;
  std::map<net::HostPortPair, std::unique_ptr<ResolveHostRequest>>
      pending_requests_;
  size_t num_requests_made_ = 0;
};

class PrefetchCanaryCheckerTest : public RenderViewHostTestHarness {
 public:
  PrefetchCanaryCheckerTest()
      : RenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    SetNetworkConnectionTrackerForTesting(nullptr);

    RenderViewHostTestHarness::SetUp();

    mojo::PendingRemote<network::mojom::NetworkContext> network_context_remote;
    network_context_ = std::make_unique<FakeNetworkContext>(
        network_context_remote.InitWithNewPipeAndPassReceiver());
    browser_context()
        ->GetDefaultStoragePartition()
        ->SetNetworkContextForTesting(std::move(network_context_remote));
  }

  void TearDown() override {
    network_context_.reset();
    RenderViewHostTestHarness::TearDown();
  }

  std::unique_ptr<PrefetchCanaryChecker> MakeChecker(const GURL& url) {
    PrefetchCanaryChecker::RetryPolicy retry_policy;
    return MakeCheckerWithRetries(url, retry_policy,
                                  base::TimeDelta::FiniteMax());
  }

  std::unique_ptr<PrefetchCanaryChecker> MakeCheckerWithRetries(
      const GURL& url,
      PrefetchCanaryChecker::RetryPolicy retry_policy,
      base::TimeDelta timeout) {
    return std::make_unique<PrefetchCanaryChecker>(
        browser_context(), PrefetchCanaryChecker::CheckType::kDNS, url,
        retry_policy, timeout, kCacheRevalidateAfter);
  }

  void RunUntilIdle() { task_environment()->RunUntilIdle(); }

  FakeNetworkContext* NetworkContext() { return network_context_.get(); }

 private:
  std::unique_ptr<FakeNetworkContext> network_context_;
};

TEST_F(PrefetchCanaryCheckerTest, OK) {
  base::HistogramTester histogram_tester;
  GURL probe_url("https://probe-url.com");
  std::unique_ptr<PrefetchCanaryChecker> checker = MakeChecker(probe_url);
  std::optional<bool> result = checker->CanaryCheckSuccessful();
  EXPECT_EQ(result, std::nullopt);
  // Make sure a cache miss was logged.
  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.CanaryChecker.CacheLookupResult.DNS", 2, 1);

  RunUntilIdle();
  NetworkContext()->MakeDNSResolveSuccess(probe_url);
  RunUntilIdle();

  result = checker->CanaryCheckSuccessful();
  EXPECT_TRUE(result.value());
  EXPECT_FALSE(checker->IsActive());

  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.CanaryChecker.FinalState.DNS", true, 1);
  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.CanaryChecker.NumAttemptsBeforeSuccess.DNS", 1, 1);
  histogram_tester.ExpectBucketCount(
      "PrefetchProxy.CanaryChecker.CacheLookupResult.DNS", 0, 1);
}

TEST_F(PrefetchCanaryCheckerTest, MultipleStart) {
  base::HistogramTester histogram_tester;
  GURL probe_url("https://probe-url.com");
  std::unique_ptr<PrefetchCanaryChecker> checker = MakeChecker(probe_url);
  // Make sure calling RunChecksIfNeeded multiple times only results in one
  // pending DNS lookup.
  checker->RunChecksIfNeeded();
  checker->RunChecksIfNeeded();
  RunUntilIdle();

  // Resolve a single DNS lookup.
  NetworkContext()->MakeDNSResolveSuccess(probe_url);
  // Make sure only one lookup was made.
  EXPECT_EQ(NetworkContext()->NumPendingRequests(), 0u);

  // Allow the checker to process and cache the response.
  RunUntilIdle();

  std::optional<bool> result = checker->CanaryCheckSuccessful();
  EXPECT_TRUE(result.value());
  EXPECT_FALSE(checker->IsActive());
}

TEST_F(PrefetchCanaryCheckerTest, CacheHit) {
  GURL probe_url("https://probe-url.com");
  std::unique_ptr<PrefetchCanaryChecker> checker = MakeChecker(probe_url);
  std::optional<bool> result = checker->CanaryCheckSuccessful();
  EXPECT_EQ(result, std::nullopt);

  RunUntilIdle();
  NetworkContext()->MakeDNSResolveSuccess(probe_url);

  // Allow the checker to process and cache the response.
  RunUntilIdle();

  // Make sure that future calls don't cause DNS lookups since there should
  // already be a cached result.
  result = checker->CanaryCheckSuccessful();
  RunUntilIdle();
  EXPECT_EQ(NetworkContext()->NumPendingRequests(), 0u);

  EXPECT_TRUE(result.value());
  EXPECT_FALSE(checker->IsActive());
}

// TODO(crbug.com/40828450): Re-enable; flaky.
TEST_F(PrefetchCanaryCheckerTest, DISABLED_NetworkConnectionShardsCache) {
  network::TestNetworkConnectionTracker::GetInstance()->SetConnectionType(
      network::mojom::ConnectionType::CONNECTION_3G);
  RunUntilIdle();

  GURL probe_url("https://probe-url.com");
  std::unique_ptr<PrefetchCanaryChecker> checker = MakeChecker(probe_url);
  checker->RunChecksIfNeeded();
  RunUntilIdle();
  NetworkContext()->MakeDNSResolveSuccess(probe_url);
  RunUntilIdle();

  std::optional<bool> result = checker->CanaryCheckSuccessful();
  // Make sure result is cached.
  EXPECT_TRUE(result.has_value());

  // Changing the network to 4G should reuse the cache.
  network::TestNetworkConnectionTracker::GetInstance()->SetConnectionType(
      network::mojom::ConnectionType::CONNECTION_4G);
  RunUntilIdle();
  result = checker->CanaryCheckSuccessful();
  EXPECT_TRUE(result.has_value());

  // Changing the network to wifi should result in a cache miss and a new check.
  network::TestNetworkConnectionTracker::GetInstance()->SetConnectionType(
      network::mojom::ConnectionType::CONNECTION_WIFI);
  RunUntilIdle();
  result = checker->CanaryCheckSuccessful();
  EXPECT_EQ(result, std::nullopt);

  // Finish the check and make sure the result is cached.
  RunUntilIdle();
  NetworkContext()->MakeDNSResolveSuccess(probe_url);
  RunUntilIdle();
  result = checker->CanaryCheckSuccessful();
  EXPECT_TRUE(result.value());
}

TEST_F(PrefetchCanaryCheckerTest, NetError) {
  base::HistogramTester histogram_tester;
  GURL probe_url("https://probe-url.com");
  std::unique_ptr<PrefetchCanaryChecker> checker = MakeChecker(probe_url);
  RunUntilIdle();
  checker->RunChecksIfNeeded();
  RunUntilIdle();
  NetworkContext()->MakeDNSResolveError(probe_url, net::ERR_FAILED);
  RunUntilIdle();

  std::optional<bool> result = checker->CanaryCheckSuccessful();
  EXPECT_FALSE(result.value());
  EXPECT_FALSE(checker->IsActive());

  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.CanaryChecker.FinalState.DNS", false, 1);
  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.CanaryChecker.NetError.DNS", std::abs(net::ERR_FAILED), 1);
  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.CanaryChecker.CacheLookupResult.DNS", 1, 1);
}

TEST_F(PrefetchCanaryCheckerTest, TimeUntilSuccess) {
  base::HistogramTester histogram_tester;
  GURL probe_url("https://probe-url.com");
  std::unique_ptr<PrefetchCanaryChecker> checker = MakeChecker(probe_url);
  checker->RunChecksIfNeeded();
  RunUntilIdle();

  task_environment()->FastForwardBy(base::Milliseconds(11000));

  NetworkContext()->MakeDNSResolveSuccess(probe_url);
  RunUntilIdle();

  EXPECT_TRUE(checker->CanaryCheckSuccessful().value());
  EXPECT_FALSE(checker->IsActive());

  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.CanaryChecker.TimeUntilSuccess.DNS", 11000, 1);
  histogram_tester.ExpectTotalCount(
      "PrefetchProxy.CanaryChecker.TimeUntilFailure.DNS", 0);
}

TEST_F(PrefetchCanaryCheckerTest, TimeUntilFailure) {
  base::HistogramTester histogram_tester;
  GURL probe_url("https://probe-url.com");
  std::unique_ptr<PrefetchCanaryChecker> checker = MakeChecker(probe_url);
  checker->RunChecksIfNeeded();
  RunUntilIdle();

  task_environment()->FastForwardBy(base::Milliseconds(11000));

  NetworkContext()->MakeDNSResolveError(probe_url, net::ERR_FAILED);
  RunUntilIdle();

  EXPECT_FALSE(checker->CanaryCheckSuccessful().value());
  EXPECT_FALSE(checker->IsActive());

  histogram_tester.ExpectTotalCount(
      "PrefetchProxy.CanaryChecker.TimeUntilSuccess.DNS", 0);
  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.CanaryChecker.TimeUntilFailure.DNS", 11000, 1);
}

TEST_F(PrefetchCanaryCheckerTest, Retries) {
  base::HistogramTester histogram_tester;
  GURL probe_url("https://probe-url.com");

  PrefetchCanaryChecker::RetryPolicy retry_policy;
  retry_policy.max_retries = 2;
  retry_policy.backoff_policy = {
      .num_errors_to_ignore = 0,
      .initial_delay_ms = 1000,
      .multiply_factor = 2,
      .jitter_factor = 0.0,
      // No maximum backoff.
      .maximum_backoff_ms = -1,
      .entry_lifetime_ms = -1,
      .always_use_initial_delay = false,
  };
  base::TimeDelta timeout = base::Days(1);
  std::unique_ptr<PrefetchCanaryChecker> checker =
      MakeCheckerWithRetries(probe_url, retry_policy, timeout);
  checker->RunChecksIfNeeded();
  RunUntilIdle();
  NetworkContext()->MakeDNSResolveError(probe_url, net::ERR_FAILED);

  RunUntilIdle();
  EXPECT_EQ(NetworkContext()->NumPendingRequests(), 0u);
  // Make sure the failure was not cached: we're not done with retries.
  EXPECT_EQ(checker->CanaryCheckSuccessful(), std::nullopt);

  task_environment()->FastForwardBy(base::Milliseconds(900));
  // There should still be no retry attempted.
  EXPECT_EQ(NetworkContext()->NumPendingRequests(), 0u);
  task_environment()->FastForwardBy(base::Milliseconds(100));
  EXPECT_EQ(NetworkContext()->NumPendingRequests(), 1u);
  NetworkContext()->MakeDNSResolveError(probe_url, net::ERR_FAILED);
  RunUntilIdle();
  EXPECT_EQ(NetworkContext()->NumPendingRequests(), 0u);
  // Make sure the failure was not cached: we're not done with retries.
  EXPECT_EQ(checker->CanaryCheckSuccessful(), std::nullopt);

  // Exponential backoff: the next retry should go off in 2s.
  task_environment()->FastForwardBy(base::Milliseconds(1900));
  EXPECT_EQ(NetworkContext()->NumPendingRequests(), 0u);
  task_environment()->FastForwardBy(base::Milliseconds(100));
  EXPECT_EQ(NetworkContext()->NumPendingRequests(), 1u);
  NetworkContext()->MakeDNSResolveError(probe_url, net::ERR_FAILED);
  RunUntilIdle();
  EXPECT_EQ(NetworkContext()->NumPendingRequests(), 0u);
  // Make sure the failure was cached: we're done with retries.
  EXPECT_FALSE(checker->CanaryCheckSuccessful().value());

  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.CanaryChecker.FinalState.DNS", false, 1);
  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.CanaryChecker.NetError.DNS", std::abs(net::ERR_FAILED), 3);
}

TEST_F(PrefetchCanaryCheckerTest, Timeout) {
  base::HistogramTester histogram_tester;
  GURL probe_url("https://probe-url.com");

  PrefetchCanaryChecker::RetryPolicy retry_policy;
  retry_policy.max_retries = 2;
  retry_policy.backoff_policy = {
      .num_errors_to_ignore = 0,
      .initial_delay_ms = 1000,
      .multiply_factor = 2,
      .jitter_factor = 0.0,
      // No maximum backoff.
      .maximum_backoff_ms = -1,
      .entry_lifetime_ms = -1,
      .always_use_initial_delay = false,
  };
  base::TimeDelta timeout = base::Milliseconds(1500);
  std::unique_ptr<PrefetchCanaryChecker> checker =
      MakeCheckerWithRetries(probe_url, retry_policy, timeout);
  checker->RunChecksIfNeeded();

  task_environment()->FastForwardBy(base::Milliseconds(1400));
  // Still one pending DNS lookup.
  EXPECT_EQ(NetworkContext()->NumPendingRequests(), 1u);
  EXPECT_EQ(NetworkContext()->NumRequestsMade(), 1u);

  task_environment()->FastForwardBy(base::Milliseconds(100));
  // It's been 1500 ms. The first lookup should haved timed out. A new one
  // will be sent in 1s since the initial backoff is 1s.
  EXPECT_EQ(NetworkContext()->NumPendingRequests(), 0u);
  EXPECT_EQ(NetworkContext()->NumRequestsMade(), 1u);

  task_environment()->FastForwardBy(base::Milliseconds(1000));
  // The first retry should go out now.
  EXPECT_EQ(NetworkContext()->NumPendingRequests(), 1u);
  EXPECT_EQ(NetworkContext()->NumRequestsMade(), 2u);

  task_environment()->FastForwardBy(base::Milliseconds(1500));
  // By now the first retry should have timed out.  The exponential backoff will
  // delay the next retry until 2s have passed.  Make sure no new lookup has
  // been triggered.
  EXPECT_EQ(NetworkContext()->NumPendingRequests(), 0u);
  EXPECT_EQ(NetworkContext()->NumRequestsMade(), 2u);

  task_environment()->FastForwardBy(base::Milliseconds(1900));
  EXPECT_EQ(NetworkContext()->NumPendingRequests(), 0u);
  EXPECT_EQ(NetworkContext()->NumRequestsMade(), 2u);

  task_environment()->FastForwardBy(base::Milliseconds(100));
  // The second retry should go out now.
  EXPECT_EQ(NetworkContext()->NumPendingRequests(), 1u);
  EXPECT_EQ(NetworkContext()->NumRequestsMade(), 3u);
  NetworkContext()->MakeDNSResolveSuccess(probe_url);
  RunUntilIdle();
  EXPECT_TRUE(checker->CanaryCheckSuccessful().value());

  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.CanaryChecker.FinalState.DNS", true, 1);
  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.CanaryChecker.NumAttemptsBeforeSuccess.DNS", 3, 1);
}

TEST_F(PrefetchCanaryCheckerTest, CacheEntryAge) {
  base::HistogramTester histogram_tester;
  GURL probe_url("https://probe-url.com");

  std::unique_ptr<PrefetchCanaryChecker> checker = MakeChecker(probe_url);
  checker->RunChecksIfNeeded();
  RunUntilIdle();
  NetworkContext()->MakeDNSResolveSuccess(probe_url);
  RunUntilIdle();
  EXPECT_TRUE(checker->CanaryCheckSuccessful().value());

  task_environment()->FastForwardBy(base::Hours(24));
  EXPECT_TRUE(checker->CanaryCheckSuccessful().value());

  histogram_tester.ExpectBucketCount(
      "PrefetchProxy.CanaryChecker.CacheEntryAge.DNS", 0, 1);
  histogram_tester.ExpectBucketCount(
      "PrefetchProxy.CanaryChecker.CacheEntryAge.DNS", 24, 1);
  histogram_tester.ExpectTotalCount(
      "PrefetchProxy.CanaryChecker.CacheEntryAge.DNS", 2);
}

}  // namespace
}  // namespace content