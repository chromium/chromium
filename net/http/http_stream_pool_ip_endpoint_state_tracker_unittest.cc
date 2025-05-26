// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_stream_pool_ip_endpoint_state_tracker.h"

#include <memory>
#include <optional>
#include <string_view>
#include <vector>

#include "net/base/network_anonymization_key.h"
#include "net/dns/host_resolver.h"
#include "net/http/http_stream_pool.h"
#include "net/http/http_stream_pool_test_util.h"
#include "net/log/net_log_with_source.h"
#include "net/test/test_with_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"
#include "url/scheme_host_port.h"

using ::testing::Optional;

namespace net {

using IPEndPointStateTracker = HttpStreamPool::IPEndPointStateTracker;
using IPEndPointState = IPEndPointStateTracker::IPEndPointState;

namespace {

static const url::SchemeHostPort kDestination("https", "www.example.org", 443);

IPEndPoint MakeIPEndPoint(std::string_view addr, uint16_t port = 80) {
  return IPEndPoint(*IPAddress::FromIPLiteral(addr), port);
}

class FakeDelegate : public IPEndPointStateTracker::Delegate {
 public:
  explicit FakeDelegate(std::unique_ptr<HostResolver::ServiceEndpointRequest>
                            service_endpoint_request)
      : service_endpoint_request_(std::move(service_endpoint_request)) {}

  ~FakeDelegate() override = default;

  FakeDelegate(const FakeDelegate&) = delete;
  FakeDelegate& operator=(const FakeDelegate&) = delete;

  // HttpStreamPool::IPEndPointStateTracker::Delegate:
  HostResolver::ServiceEndpointRequest* GetServiceEndpointRequest() override {
    return service_endpoint_request_.get();
  }

  bool IsSvcbOptional() override { return false; }

  bool IsEndpointUsableForTcpBasedAttempt(const ServiceEndpoint& endpoint,
                                          bool svcb_optional) override {
    return true;
  }

  bool HasEnoughTcpBasedAttemptsForSlowIPEndPoint(
      const IPEndPoint& ip_endpoint) override {
    return false;
  }

 private:
  std::unique_ptr<HostResolver::ServiceEndpointRequest>
      service_endpoint_request_;
};

}  // namespace

class HttpStreamPoolIPEndPointStateTrackerTest
    : public TestWithTaskEnvironment {
 public:
  HttpStreamPoolIPEndPointStateTrackerTest()
      : TestWithTaskEnvironment(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  ~HttpStreamPoolIPEndPointStateTrackerTest() override = default;

 protected:
  std::unique_ptr<FakeDelegate> CreateDelegate() {
    std::unique_ptr<HostResolver::ServiceEndpointRequest>
        service_endpoint_request = resolver_.CreateServiceEndpointRequest(
            HostResolver::Host(kDestination), NetworkAnonymizationKey(),
            NetLogWithSource(), HostResolver::ResolveHostParameters());
    auto delegate =
        std::make_unique<FakeDelegate>(std::move(service_endpoint_request));
    return delegate;
  }

  FakeServiceEndpointResolver& resolver() { return resolver_; }

 private:
  FakeServiceEndpointResolver resolver_;
};

TEST_F(HttpStreamPoolIPEndPointStateTrackerTest, GetIPEndPointToAttempt) {
  struct TestCase {
    std::string_view description = "";
    std::vector<std::pair<IPEndPoint, std::optional<IPEndPointState>>>
        endpoint_states;
    std::optional<IPEndPoint> exclude_ip_endpoint = std::nullopt;
    std::optional<IPEndPoint> expected;
  } kTestCases[] = {
      {
          .description = "Prefer fast or non-attempted endpoint",
          .endpoint_states =
              {
                  {MakeIPEndPoint("2001:db8::1"),
                   IPEndPointState::kSlowAttempting},
                  {MakeIPEndPoint("2001:db8::2"), std::nullopt},
                  {MakeIPEndPoint("192.0.2.1"),
                   IPEndPointState::kSlowSucceeded},
              },
          .expected = MakeIPEndPoint("2001:db8::2"),
      },
      {
          .description = "Prefer slow-succeeded to slow-attempting",
          .endpoint_states =
              {
                  {MakeIPEndPoint("2001:db8::1"),
                   IPEndPointState::kSlowAttempting},
                  {MakeIPEndPoint("192.0.2.1"),
                   IPEndPointState::kSlowSucceeded},
                  {MakeIPEndPoint("192.0.2.2"),
                   IPEndPointState::kSlowSucceeded},
              },
          .expected = MakeIPEndPoint("192.0.2.1"),
      },
      {
          .description = "Slow-attempting and failed only",
          .endpoint_states =
              {
                  {MakeIPEndPoint("2001:db8::1"),
                   IPEndPointState::kSlowAttempting},
                  {MakeIPEndPoint("2001:db8::2"),
                   IPEndPointState::kSlowAttempting},
                  {MakeIPEndPoint("192.0.2.1"), IPEndPointState::kFailed},
              },
          .expected = MakeIPEndPoint("2001:db8::1"),
      },
      {
          .description = "Failed endpoint only",
          .endpoint_states = {{
              {MakeIPEndPoint("2001:db8::1"), IPEndPointState::kFailed},
              {MakeIPEndPoint("192.0.2.1"), IPEndPointState::kFailed},

          }},
          .expected = std::nullopt,
      },
      {
          .description = "Exclude an endpoint that is the only endpoint",
          .endpoint_states = {{
              {MakeIPEndPoint("2001:db8::1"), IPEndPointState::kSlowAttempting},
          }},
          .exclude_ip_endpoint = MakeIPEndPoint("2001:db8::1"),
          .expected = std::nullopt,
      },
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.description);

    // Setup ServiceEndpointRequest.
    ServiceEndpointBuilder service_endpoint_builder;
    for (const auto& [ip_endpoint, state] : test_case.endpoint_states) {
      service_endpoint_builder.add_ip_endpoint(ip_endpoint);
    }
    resolver()
        .ConfigureDefaultResolution()
        .add_endpoint(service_endpoint_builder.endpoint())
        .CompleteStartSynchronously(OK);

    std::unique_ptr<FakeDelegate> delegate = CreateDelegate();
    IPEndPointStateTracker tracker(delegate.get());

    // Setup IPEndPoint states.
    for (const auto& [ip_endpoint, state] : test_case.endpoint_states) {
      if (!state.has_value()) {
        continue;
      }
      switch (*state) {
        case IPEndPointState::kFailed:
          tracker.OnEndpointFailed(ip_endpoint);
          break;
        case IPEndPointState::kSlowAttempting:
          tracker.OnEndpointSlow(ip_endpoint);
          break;
        case IPEndPointState::kSlowSucceeded:
          // Mark slow first then mark slow succeeded.
          tracker.OnEndpointSlow(ip_endpoint);
          tracker.OnEndpointSlowSucceeded(ip_endpoint);
          break;
        default:
          NOTREACHED();
      }
    }

    std::optional<IPEndPoint> actual =
        tracker.GetIPEndPointToAttemptTcpBased(test_case.exclude_ip_endpoint);
    EXPECT_THAT(actual, test_case.expected);
  }
}

TEST_F(HttpStreamPoolIPEndPointStateTrackerTest,
       IPEndPointSlowSuccessSlowFail) {
  const IPEndPoint ip_endpoint = MakeIPEndPoint("2001:db8::1");

  resolver()
      .ConfigureDefaultResolution()
      .add_endpoint(
          ServiceEndpointBuilder().add_ip_endpoint(ip_endpoint).endpoint())
      .CompleteStartSynchronously(OK);

  std::unique_ptr<FakeDelegate> delegate = CreateDelegate();
  IPEndPointStateTracker tracker(delegate.get());

  // The first attempt is slow and succeeds.
  tracker.OnEndpointSlow(ip_endpoint);
  EXPECT_THAT(tracker.GetState(ip_endpoint),
              Optional(IPEndPointState::kSlowAttempting));
  tracker.OnEndpointSlowSucceeded(ip_endpoint);
  EXPECT_THAT(tracker.GetState(ip_endpoint),
              Optional(IPEndPointState::kSlowSucceeded));

  // The second attempt is slow and succeeds.
  tracker.OnEndpointSlow(ip_endpoint);
  // The state should not be updated.
  EXPECT_THAT(tracker.GetState(ip_endpoint),
              Optional(IPEndPointState::kSlowSucceeded));
  tracker.OnEndpointSlowSucceeded(ip_endpoint);
  EXPECT_THAT(tracker.GetState(ip_endpoint),
              Optional(IPEndPointState::kSlowSucceeded));

  // The third attempt is slow and fails.
  tracker.OnEndpointSlow(ip_endpoint);
  EXPECT_THAT(tracker.GetState(ip_endpoint),
              Optional(IPEndPointState::kSlowSucceeded));
  tracker.OnEndpointFailed(ip_endpoint);
  EXPECT_THAT(tracker.GetState(ip_endpoint),
              Optional(IPEndPointState::kFailed));
}

}  // namespace net
