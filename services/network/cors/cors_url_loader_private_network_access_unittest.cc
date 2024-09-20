// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/cors/cors_url_loader.h"

#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "net/base/net_errors.h"
#include "services/network/cors/cors_url_loader_test_util.h"
#include "services/network/public/cpp/cors/cors_error_status.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/cors.mojom.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "services/network/public/mojom/ip_address_space.mojom.h"
#include "services/network/test/client_security_state_builder.h"
#include "services/network/test/mock_devtools_observer.h"
#include "services/network/test/test_url_loader_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/origin.h"

namespace network::cors {
namespace {

using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::IsSupersetOf;
using ::testing::Optional;
using ::testing::Pair;

constexpr char kPreflightErrorHistogramName[] = "Net.Cors.PreflightCheckError2";
constexpr char kPreflightWarningHistogramName[] =
    "Net.Cors.PreflightCheckWarning";

base::Bucket MakeBucket(mojom::CorsError error,
                        base::HistogramBase::Count count) {
  return base::Bucket(static_cast<base::HistogramBase::Sample>(error), count);
}

std::vector<std::pair<std::string, std::string>> MakeHeaderPairs(
    const net::HttpRequestHeaders& headers) {
  std::vector<std::pair<std::string, std::string>> result;
  for (const auto& header : headers.GetHeaderVector()) {
    result.emplace_back(header.key, header.value);
  }
  return result;
}

class RequestTrustedParamsBuilder {
 public:
  RequestTrustedParamsBuilder() = default;
  ~RequestTrustedParamsBuilder() = default;

  RequestTrustedParamsBuilder& WithClientSecurityState(
      mojom::ClientSecurityStatePtr client_security_state) {
    params_.client_security_state = std::move(client_security_state);
    return *this;
  }

  // Convenience shortcut for a default `ClientSecurityState` with a `policy`.
  RequestTrustedParamsBuilder& WithPrivateNetworkRequestPolicy(
      mojom::PrivateNetworkRequestPolicy policy) {
    return WithClientSecurityState(ClientSecurityStateBuilder()
                                       .WithPrivateNetworkRequestPolicy(policy)
                                       .Build());
  }

  RequestTrustedParamsBuilder& WithDevToolsObserver(
      mojo::PendingRemote<mojom::DevToolsObserver> devtools_observer) {
    params_.devtools_observer = std::move(devtools_observer);
    return *this;
  }

  ResourceRequest::TrustedParams Build() const { return params_; }

 private:
  ResourceRequest::TrustedParams params_;
};

class CorsURLLoaderPrivateNetworkAccessTest : public CorsURLLoaderTestBase {};

TEST_F(CorsURLLoaderPrivateNetworkAccessTest, TargetIpAddressSpaceSimple) {
  ResourceRequest request;
  request.method = "GET";
  request.mode = mojom::RequestMode::kCors;
  request.url = GURL("https://example.com/");
  request.request_initiator = url::Origin::Create(request.url);

  CreateLoaderAndStart(request);
  RunUntilCreateLoaderAndStartCalled();

  // No target yet.
  EXPECT_EQ(GetRequest().target_ip_address_space,
            mojom::IPAddressSpace::kUnknown);

  // Pretend we just hit a private IP address unexpectedly.
  NotifyLoaderClientOnComplete(CorsErrorStatus(
      mojom::CorsError::kUnexpectedPrivateNetworkAccess,
      mojom::IPAddressSpace::kUnknown, mojom::IPAddressSpace::kPrivate));

  // The CORS URL loader restarts a new preflight request.
  RunUntilCreateLoaderAndStartCalled();

  // The second request expects the same IP address space.
  EXPECT_EQ(GetRequest().target_ip_address_space,
            mojom::IPAddressSpace::kPrivate);

  NotifyLoaderClientOnReceiveResponse({
      {"Access-Control-Allow-Origin", "https://example.com"},
      {"Access-Control-Allow-Credentials", "true"},
      {"Access-Control-Allow-Private-Network", "true"},
  });
  NotifyLoaderClientOnComplete(net::OK);

  // The CORS URL loader sends the actual request.
  RunUntilCreateLoaderAndStartCalled();

  // The actual request expects the same IP address space.
  EXPECT_EQ(GetRequest().target_ip_address_space,
            mojom::IPAddressSpace::kPrivate);
}

TEST_F(CorsURLLoaderPrivateNetworkAccessTest, TargetIpAddressSpacePreflight) {
  auto initiator = url::Origin::Create(GURL("https://foo.example"));
  ResetFactory(initiator, mojom::kBrowserProcessId);

  ResourceRequest request;
  request.method = "PUT";
  request.mode = mojom::RequestMode::kCors;
  request.url = GURL("https://example.com/");
  request.request_initiator = initiator;

  CreateLoaderAndStart(request);
  RunUntilCreateLoaderAndStartCalled();

  // No target yet.
  EXPECT_EQ(GetRequest().target_ip_address_space,
            mojom::IPAddressSpace::kUnknown);

  // Pretend we just hit a private IP address unexpectedly.
  NotifyLoaderClientOnComplete(CorsErrorStatus(
      mojom::CorsError::kUnexpectedPrivateNetworkAccess,
      mojom::IPAddressSpace::kUnknown, mojom::IPAddressSpace::kPrivate));

  // The CORS URL loader restarts a new preflight request.
  RunUntilCreateLoaderAndStartCalled();

  // The second request expects the same IP address space.
  EXPECT_EQ(GetRequest().target_ip_address_space,
            mojom::IPAddressSpace::kPrivate);

  NotifyLoaderClientOnReceiveResponse({
      {"Access-Control-Allow-Methods", "PUT"},
      {"Access-Control-Allow-Origin", "https://foo.example"},
      {"Access-Control-Allow-Credentials", "true"},
      {"Access-Control-Allow-Private-Network", "true"},
  });
  NotifyLoaderClientOnComplete(net::OK);

  // The CORS URL loader sends the actual request.
  RunUntilCreateLoaderAndStartCalled();

  // The actual request expects the same IP address space.
  EXPECT_EQ(GetRequest().target_ip_address_space,
            mojom::IPAddressSpace::kPrivate);
}

TEST_F(CorsURLLoaderPrivateNetworkAccessTest, RequestHeadersSimple) {
  ResourceRequest request;
  request.method = "GET";
  request.mode = mojom::RequestMode::kCors;
  request.url = GURL("https://example.com/");
  request.request_initiator = url::Origin::Create(request.url);

  CreateLoaderAndStart(request);
  RunUntilCreateLoaderAndStartCalled();

  EXPECT_EQ(GetRequest().method, "GET");
  EXPECT_FALSE(
      GetRequest().headers.HasHeader("Access-Control-Request-Private-Network"));

  NotifyLoaderClientOnComplete(CorsErrorStatus(
      mojom::CorsError::kUnexpectedPrivateNetworkAccess,
      mojom::IPAddressSpace::kUnknown, mojom::IPAddressSpace::kPrivate));

  RunUntilCreateLoaderAndStartCalled();

  EXPECT_EQ(GetRequest().method, "OPTIONS");
  EXPECT_THAT(MakeHeaderPairs(GetRequest().headers),
              IsSupersetOf({
                  Pair("Origin", "https://example.com"),
                  Pair("Access-Control-Request-Method", "GET"),
                  Pair("Access-Control-Request-Private-Network", "true"),
              }));
}

TEST_F(CorsURLLoaderPrivateNetworkAccessTest, RequestHeadersPreflight) {
  auto initiator = url::Origin::Create(GURL("https://foo.example"));
  ResetFactory(initiator, mojom::kBrowserProcessId);

  ResourceRequest request;
  request.method = "PUT";
  request.mode = mojom::RequestMode::kCors;
  request.url = GURL("https://example.com/");
  request.request_initiator = initiator;

  CreateLoaderAndStart(request);
  RunUntilCreateLoaderAndStartCalled();

  EXPECT_EQ(GetRequest().method, "OPTIONS");
  EXPECT_FALSE(
      GetRequest().headers.HasHeader("Access-Control-Request-Private-Network"));

  NotifyLoaderClientOnComplete(CorsErrorStatus(
      mojom::CorsError::kUnexpectedPrivateNetworkAccess,
      mojom::IPAddressSpace::kUnknown, mojom::IPAddressSpace::kPrivate));

  RunUntilCreateLoaderAndStartCalled();

  EXPECT_EQ(GetRequest().method, "OPTIONS");
  EXPECT_THAT(MakeHeaderPairs(GetRequest().headers),
              IsSupersetOf({
                  Pair("Origin", "https://foo.example"),
                  Pair("Access-Control-Request-Method", "PUT"),
                  Pair("Access-Control-Request-Private-Network", "true"),
              }));
}

TEST_F(CorsURLLoaderPrivateNetworkAccessTest, MissingResponseHeaderSimple) {
  ResourceRequest request;
  request.method = "GET";
  request.mode = mojom::RequestMode::kCors;
  request.url = GURL("https://example.com/");
  request.request_initiator = url::Origin::Create(request.url);

  CreateLoaderAndStart(request);
  RunUntilCreateLoaderAndStartCalled();
  NotifyLoaderClientOnComplete(CorsErrorStatus(
      mojom::CorsError::kUnexpectedPrivateNetworkAccess,
      mojom::IPAddressSpace::kUnknown, mojom::IPAddressSpace::kPrivate));

  RunUntilCreateLoaderAndStartCalled();
  NotifyLoaderClientOnReceiveResponse({
      {"Access-Control-Allow-Origin", "https://example.com"},
      {"Access-Control-Allow-Credentials", "true"},
  });
  RunUntilComplete();

  EXPECT_EQ(client().completion_status().error_code,
            net::ERR_BLOCKED_BY_PRIVATE_NETWORK_ACCESS_CHECKS);
  EXPECT_EQ(
      client().completion_status().private_network_access_preflight_result,
      mojom::PrivateNetworkAccessPreflightResult::kError);

  CorsErrorStatus expected_status(
      mojom::CorsError::kPreflightMissingAllowPrivateNetwork);
  expected_status.target_address_space = mojom::IPAddressSpace::kPrivate;
  EXPECT_THAT(client().completion_status().cors_error_status,
              Optional(expected_status));
}

TEST_F(CorsURLLoaderPrivateNetworkAccessTest, MissingResponseHeaderPreflight) {
  auto initiator = url::Origin::Create(GURL("https://foo.example"));
  ResetFactory(initiator, mojom::kBrowserProcessId);

  ResourceRequest request;
  request.method = "PUT";
  request.mode = mojom::RequestMode::kCors;
  request.url = GURL("https://example.com/");
  request.request_initiator = initiator;

  CreateLoaderAndStart(request);
  RunUntilCreateLoaderAndStartCalled();
  NotifyLoaderClientOnComplete(CorsErrorStatus(
      mojom::CorsError::kUnexpectedPrivateNetworkAccess,
      mojom::IPAddressSpace::kUnknown, mojom::IPAddressSpace::kPrivate));

  RunUntilCreateLoaderAndStartCalled();
  NotifyLoaderClientOnReceiveResponse({
      {"Access-Control-Allow-Origin", "https://foo.example"},
      {"Access-Control-Allow-Credentials", "true"},
  });
  RunUntilComplete();

  EXPECT_EQ(client().completion_status().error_code,
            net::ERR_BLOCKED_BY_PRIVATE_NETWORK_ACCESS_CHECKS);
  EXPECT_EQ(
      client().completion_status().private_network_access_preflight_result,
      mojom::PrivateNetworkAccessPreflightResult::kError);

  // Even though `Access-Control-Allow-Methods` is also missing, the PNA header
  // missing is noticed first. See https://crbug.com/1424847.
  CorsErrorStatus expected_status(
      mojom::CorsError::kPreflightMissingAllowPrivateNetwork);
  expected_status.target_address_space = mojom::IPAddressSpace::kPrivate;
  EXPECT_THAT(client().completion_status().cors_error_status,
              Optional(expected_status));
}

TEST_F(CorsURLLoaderPrivateNetworkAccessTest, InvalidResponseHeaderSimple) {
  ResourceRequest request;
  request.method = "GET";
  request.mode = mojom::RequestMode::kCors;
  request.url = GURL("https://example.com/");
  request.request_initiator = url::Origin::Create(request.url);

  CreateLoaderAndStart(request);
  RunUntilCreateLoaderAndStartCalled();
  NotifyLoaderClientOnComplete(CorsErrorStatus(
      mojom::CorsError::kUnexpectedPrivateNetworkAccess,
      mojom::IPAddressSpace::kUnknown, mojom::IPAddressSpace::kPrivate));

  RunUntilCreateLoaderAndStartCalled();
  NotifyLoaderClientOnReceiveResponse({
      {"Access-Control-Allow-Origin", "https://example.com"},
      {"Access-Control-Allow-Credentials", "true"},
      {"Access-Control-Allow-Private-Network", "invalid-value"},
  });
  RunUntilComplete();

  EXPECT_EQ(client().completion_status().error_code,
            net::ERR_BLOCKED_BY_PRIVATE_NETWORK_ACCESS_CHECKS);
  EXPECT_EQ(
      client().completion_status().private_network_access_preflight_result,
      mojom::PrivateNetworkAccessPreflightResult::kError);

  CorsErrorStatus expected_status(
      mojom::CorsError::kPreflightInvalidAllowPrivateNetwork, "invalid-value");
  expected_status.target_address_space = mojom::IPAddressSpace::kPrivate;
  EXPECT_THAT(client().completion_status().cors_error_status,
              Optional(expected_status));
}

TEST_F(CorsURLLoaderPrivateNetworkAccessTest, InvalidResponseHeaderPreflight) {
  auto initiator = url::Origin::Create(GURL("https://foo.example"));
  ResetFactory(initiator, mojom::kBrowserProcessId);

  ResourceRequest request;
  request.method = "PUT";
  request.mode = mojom::RequestMode::kCors;
  request.url = GURL("https://example.com/");
  request.request_initiator = initiator;

  CreateLoaderAndStart(request);
  RunUntilCreateLoaderAndStartCalled();
  NotifyLoaderClientOnComplete(CorsErrorStatus(
      mojom::CorsError::kUnexpectedPrivateNetworkAccess,
      mojom::IPAddressSpace::kUnknown, mojom::IPAddressSpace::kPrivate));

  RunUntilCreateLoaderAndStartCalled();
  NotifyLoaderClientOnReceiveResponse({
      {"Access-Control-Allow-Methods", "PUT"},
      {"Access-Control-Allow-Origin", "https://foo.example"},
      {"Access-Control-Allow-Credentials", "true"},
      {"Access-Control-Allow-Private-Network", "invalid-value"},
  });
  RunUntilComplete();

  EXPECT_EQ(client().completion_status().error_code,
            net::ERR_BLOCKED_BY_PRIVATE_NETWORK_ACCESS_CHECKS);
  EXPECT_EQ(
      client().completion_status().private_network_access_preflight_result,
      mojom::PrivateNetworkAccessPreflightResult::kError);

  CorsErrorStatus expected_status(
      mojom::CorsError::kPreflightInvalidAllowPrivateNetwork, "invalid-value");
  expected_status.target_address_space = mojom::IPAddressSpace::kPrivate;
  EXPECT_THAT(client().completion_status().cors_error_status,
              Optional(expected_status));
}

TEST_F(CorsURLLoaderPrivateNetworkAccessTest, ErrorAfterPreflight) {
  auto initiator = url::Origin::Create(GURL("https://foo.example"));
  ResetFactory(initiator, mojom::kBrowserProcessId);

  ResourceRequest request;
  request.method = "GET";
  request.mode = mojom::RequestMode::kCors;
  request.url = GURL("https://example.com/");
  request.request_initiator = initiator;

  CreateLoaderAndStart(request);
  RunUntilCreateLoaderAndStartCalled();
  NotifyLoaderClientOnComplete(CorsErrorStatus(
      mojom::CorsError::kUnexpectedPrivateNetworkAccess,
      mojom::IPAddressSpace::kUnknown, mojom::IPAddressSpace::kPrivate));

  RunUntilCreateLoaderAndStartCalled();
  NotifyLoaderClientOnReceiveResponse({
      {"Access-Control-Allow-Methods", "GET"},
      {"Access-Control-Allow-Origin", "https://foo.example"},
      {"Access-Control-Allow-Credentials", "true"},
      {"Access-Control-Allow-Private-Network", "true"},
  });
  NotifyLoaderClientOnComplete(net::OK);

  RunUntilCreateLoaderAndStartCalled();

  // The actual request fails.
  NotifyLoaderClientOnComplete(net::ERR_TIMED_OUT);
  RunUntilComplete();

  EXPECT_EQ(client().completion_status().error_code, net::ERR_TIMED_OUT);

  // The preflight is reported here, since there was no `URLResponseHead`.
  EXPECT_EQ(
      client().completion_status().private_network_access_preflight_result,
      mojom::PrivateNetworkAccessPreflightResult::kSuccess);
}

TEST_F(CorsURLLoaderPrivateNetworkAccessTest, SuccessSimple) {
  ResourceRequest request;
  request.method = "GET";
  request.mode = mojom::RequestMode::kCors;
  request.url = GURL("https://example.com/");
  request.request_initiator = url::Origin::Create(request.url);

  CreateLoaderAndStart(request);
  RunUntilCreateLoaderAndStartCalled();
  NotifyLoaderClientOnComplete(CorsErrorStatus(
      mojom::CorsError::kUnexpectedPrivateNetworkAccess,
      mojom::IPAddressSpace::kUnknown, mojom::IPAddressSpace::kPrivate));

  RunUntilCreateLoaderAndStartCalled();
  NotifyLoaderClientOnReceiveResponse({
      {"Access-Control-Allow-Origin", "https://example.com"},
      {"Access-Control-Allow-Credentials", "true"},
      {"Access-Control-Allow-Private-Network", "true"},
  });
  NotifyLoaderClientOnComplete(net::OK);

  RunUntilCreateLoaderAndStartCalled();

  EXPECT_EQ(GetRequest().method, "GET");

  NotifyLoaderClientOnReceiveResponse();
  NotifyLoaderClientOnComplete(net::OK);
  RunUntilComplete();

  EXPECT_EQ(client().response_head()->private_network_access_preflight_result,
            mojom::PrivateNetworkAccessPreflightResult::kSuccess);

  EXPECT_EQ(client().completion_status().error_code, net::OK);
  EXPECT_EQ(
      client().completion_status().private_network_access_preflight_result,
      mojom::PrivateNetworkAccessPreflightResult::kNone);
}

TEST_F(CorsURLLoaderPrivateNetworkAccessTest, SuccessPreflight) {
  auto initiator = url::Origin::Create(GURL("https://foo.example"));
  ResetFactory(initiator, mojom::kBrowserProcessId);

  ResourceRequest request;
  request.method = "PUT";
  request.mode = mojom::RequestMode::kCors;
  request.url = GURL("https://example.com/");
  request.request_initiator = initiator;

  CreateLoaderAndStart(request);
  RunUntilCreateLoaderAndStartCalled();
  NotifyLoaderClientOnComplete(CorsErrorStatus(
      mojom::CorsError::kUnexpectedPrivateNetworkAccess,
      mojom::IPAddressSpace::kUnknown, mojom::IPAddressSpace::kPrivate));

  RunUntilCreateLoaderAndStartCalled();
  NotifyLoaderClientOnReceiveResponse({
      {"Access-Control-Allow-Methods", "PUT"},
      {"Access-Control-Allow-Origin", "https://foo.example"},
      {"Access-Control-Allow-Credentials", "true"},
      {"Access-Control-Allow-Private-Network", "true"},
  });
  NotifyLoaderClientOnComplete(net::OK);

  RunUntilCreateLoaderAndStartCalled();

  EXPECT_EQ(GetRequest().method, "PUT");

  NotifyLoaderClientOnReceiveResponse({
      {"Access-Control-Allow-Methods", "PUT"},
      {"Access-Control-Allow-Origin", "https://foo.example"},
      {"Access-Control-Allow-Credentials", "true"},
  });
  NotifyLoaderClientOnComplete(net::OK);
  RunUntilComplete();

  EXPECT_EQ(client().response_head()->private_network_access_preflight_result,
            mojom::PrivateNetworkAccessPreflightResult::kSuccess);

  EXPECT_EQ(client().completion_status().error_code, net::OK);
  EXPECT_EQ(
      client().completion_status().private_network_access_preflight_result,
      mojom::PrivateNetworkAccessPreflightResult::kNone);
}

TEST_F(CorsURLLoaderPrivateNetworkAccessTest, SuccessNoCors) {
  ResourceRequest request;
  request.method = "GET";
  request.mode = mojom::RequestMode::kNoCors;
  request.url = GURL("https://example.com/");
  request.request_initiator = url::Origin::Create(request.url);

  CreateLoaderAndStart(request);
  RunUntilCreateLoaderAndStartCalled();
  NotifyLoaderClientOnComplete(CorsErrorStatus(
      mojom::CorsError::kUnexpectedPrivateNetworkAccess,
      mojom::IPAddressSpace::kUnknown, mojom::IPAddressSpace::kPrivate));

  RunUntilCreateLoaderAndStartCalled();
  NotifyLoaderClientOnReceiveResponse({
      {"Access-Control-Allow-Origin", "https://example.com"},
      {"Access-Control-Allow-Credentials", "true"},
      {"Access-Control-Allow-Private-Network", "true"},
  });
  NotifyLoaderClientOnComplete(net::OK);

  RunUntilCreateLoaderAndStartCalled();
  NotifyLoaderClientOnReceiveResponse();
  NotifyLoaderClientOnComplete(net::OK);
  RunUntilComplete();

  EXPECT_EQ(client().response_head()->private_network_access_preflight_result,
            mojom::PrivateNetworkAccessPreflightResult::kSuccess);

  EXPECT_EQ(client().completion_status().error_code, net::OK);
  EXPECT_EQ(
      client().completion_status().private_network_access_preflight_result,
      mojom::PrivateNetworkAccessPreflightResult::kNone);
}

TEST_F(CorsURLLoaderPrivateNetworkAccessTest, RedirectBeforePreflight) {
  ResourceRequest request;
  request.method = "GET";
  request.mode = mojom::RequestMode::kCors;
  request.url = GURL("https://example.com/");
  request.request_initiator = url::Origin::Create(request.url);

  CreateLoaderAndStart(request);
  RunUntilCreateLoaderAndStartCalled();

  // Redirect.
  NotifyLoaderClientOnReceiveRedirect(
      CreateRedirectInfo(302, "GET", GURL("https://other.example")));
  RunUntilRedirectReceived();

  // No preflight reported for the redirect response.
  EXPECT_EQ(client().response_head()->private_network_access_preflight_result,
            mojom::PrivateNetworkAccessPreflightResult::kNone);

  FollowRedirect();

  // Private network request.
  RunUntilCreateLoaderAndStartCalled();
  NotifyLoaderClientOnComplete(CorsErrorStatus(
      mojom::CorsError::kUnexpectedPrivateNetworkAccess,
      mojom::IPAddressSpace::kUnknown, mojom::IPAddressSpace::kPrivate));

  // Private Network Access preflight.
  RunUntilCreateLoaderAndStartCalled();
  NotifyLoaderClientOnReceiveResponse({
      {"Access-Control-Allow-Origin", "https://example.com"},
      {"Access-Control-Allow-Credentials", "true"},
      {"Access-Control-Allow-Private-Network", "true"},
  });
  NotifyLoaderClientOnComplete(net::OK);

  // Actual request.
  RunUntilCreateLoaderAndStartCalled();
  EXPECT_EQ(GetRequest().method, "GET");

  NotifyLoaderClientOnReceiveResponse({
      {"Access-Control-Allow-Origin", "https://example.com"},
      {"Access-Control-Allow-Credentials", "true"},
  });
  NotifyLoaderClientOnComplete(net::OK);
  RunUntilComplete();

  // The preflight is reported correctly.
  EXPECT_EQ(client().response_head()->private_network_access_preflight_result,
            mojom::PrivateNetworkAccessPreflightResult::kSuccess);

  EXPECT_EQ(client().completion_status().error_code, net::OK);
  EXPECT_EQ(
      client().completion_status().private_network_access_preflight_result,
      mojom::PrivateNetworkAccessPreflightResult::kNone);
}

TEST_F(CorsURLLoaderPrivateNetworkAccessTest, RedirectAfterPreflight) {
  ResourceRequest request;
  request.method = "GET";
  request.mode = mojom::RequestMode::kCors;
  request.url = GURL("https://example.com/");
  request.request_initiator = url::Origin::Create(request.url);

  CreateLoaderAndStart(request);

  // Private network request.
  RunUntilCreateLoaderAndStartCalled();
  NotifyLoaderClientOnComplete(CorsErrorStatus(
      mojom::CorsError::kUnexpectedPrivateNetworkAccess,
      mojom::IPAddressSpace::kUnknown, mojom::IPAddressSpace::kPrivate));

  // Private Network Access preflight.
  RunUntilCreateLoaderAndStartCalled();
  NotifyLoaderClientOnReceiveResponse({
      {"Access-Control-Allow-Origin", "https://example.com"},
      {"Access-Control-Allow-Credentials", "true"},
      {"Access-Control-Allow-Private-Network", "true"},
  });
  NotifyLoaderClientOnComplete(net::OK);

  // Actual request.
  RunUntilCreateLoaderAndStartCalled();
  EXPECT_EQ(GetRequest().method, "GET");

  // Redirect.
  NotifyLoaderClientOnReceiveRedirect(
      CreateRedirectInfo(302, "GET", GURL("https://other.example")));
  RunUntilRedirectReceived();

  // The preflight is reported correctly.
  EXPECT_EQ(client().response_head()->private_network_access_preflight_result,
            mojom::PrivateNetworkAccessPreflightResult::kSuccess);

  FollowRedirect();

  RunUntilCreateLoaderAndStartCalled();
  NotifyLoaderClientOnReceiveResponse({
      {"Access-Control-Allow-Origin", "https://example.com"},
      {"Access-Control-Allow-Credentials", "true"},
  });
  NotifyLoaderClientOnComplete(net::OK);
  RunUntilComplete();

  // No preflight reported for the redirected request.
  EXPECT_EQ(client().response_head()->private_network_access_preflight_result,
            mojom::PrivateNetworkAccessPreflightResult::kNone);

  EXPECT_EQ(client().completion_status().error_code, net::OK);
  EXPECT_EQ(
      client().completion_status().private_network_access_preflight_result,
      mojom::PrivateNetworkAccessPreflightResult::kNone);
}

// Regression test for https://crbug.com/1432684.
TEST_F(CorsURLLoaderPrivateNetworkAccessTest,
       RedirectAfterPnaOnlyWarningPreflight) {
  auto initiator_origin = url::Origin::Create(GURL("https://example.com"));

  ResetFactoryParams factory_params;
  factory_params.is_trusted = true;
  ResetFactory(initiator_origin, kRendererProcessId, factory_params);

  ResourceRequest request;
  request.method = "PUT";
  request.mode = mojom::RequestMode::kCors;
  request.url = GURL("https://example.com/");
  request.request_initiator = initiator_origin;
  request.trusted_params =
      RequestTrustedParamsBuilder()
          .WithClientSecurityState(
              ClientSecurityStateBuilder()
                  .WithPrivateNetworkRequestPolicy(
                      mojom::PrivateNetworkRequestPolicy::kPreflightWarn)
                  .WithIsSecureContext(true)
                  .WithIPAddressSpace(mojom::IPAddressSpace::kPublic)
                  .Build())
          .Build();

  CreateLoaderAndStart(request);

  // Private network request.
  RunUntilCreateLoaderAndStartCalled();
  NotifyLoaderClientOnComplete(CorsErrorStatus(
      mojom::CorsError::kUnexpectedPrivateNetworkAccess,
      mojom::IPAddressSpace::kUnknown, mojom::IPAddressSpace::kPrivate));

  // Private Network Access preflight.
  RunUntilCreateLoaderAndStartCalled();
  NotifyLoaderClientOnReceiveResponse({
      {"Access-Control-Allow-Methods", "PUT"},
      {"Access-Control-Allow-Origin", "https://example.com"},
      {"Access-Control-Allow-Credentials", "true"},
      {"Access-Control-Allow-Private-Network", "true"},
  });
  NotifyLoaderClientOnComplete(net::OK);

  // Actual request.
  RunUntilCreateLoaderAndStartCalled();
  EXPECT_EQ(GetRequest().method, "PUT");

  // Redirect.
  NotifyLoaderClientOnReceiveRedirect(
      CreateRedirectInfo(302, "PUT", GURL("https://other.example")));
  RunUntilRedirectReceived();

  // The preflight is reported correctly.
  EXPECT_EQ(client().response_head()->private_network_access_preflight_result,
            mojom::PrivateNetworkAccessPreflightResult::kSuccess);

  FollowRedirect();

  // CORS preflight.
  RunUntilCreateLoaderAndStartCalled();
  EXPECT_EQ(GetRequest().method, "OPTIONS");

  // Preflight response is missing CORS headers.
  // Due to https://crbug.com/1432684, `CorsURLLoader` used to encounter a CHECK
  // failure at this point.
  NotifyLoaderClientOnReceiveResponse();
  RunUntilComplete();

  EXPECT_EQ(client().completion_status().error_code, net::ERR_FAILED);
  EXPECT_THAT(client().completion_status().cors_error_status,
              Optional(CorsErrorStatus(
                  mojom::CorsError::kPreflightMissingAllowOriginHeader,
                  network::mojom::IPAddressSpace::kUnknown,
                  network::mojom::IPAddressSpace::kUnknown)));

  // No PNA preflight reported for the redirected request, since the preflight
  // was CORS-only.
  EXPECT_EQ(
      client().completion_status().private_network_access_preflight_result,
      mojom::PrivateNetworkAccessPreflightResult::kNone);
}

// This test verifies that PNA preflight results are cached per target IP
// address space.
TEST_F(CorsURLLoaderPrivateNetworkAccessTest, CachesPreflightResult) {
  ResourceRequest request;
  request.method = "GET";
  request.mode = mojom::RequestMode::kCors;
  request.url = GURL("https://example.com/");
  request.request_initiator = url::Origin::Create(request.url);

  CreateLoaderAndStart(request);
  RunUntilCreateLoaderAndStartCalled();
  NotifyLoaderClientOnComplete(CorsErrorStatus(
      mojom::CorsError::kUnexpectedPrivateNetworkAccess,
      mojom::IPAddressSpace::kUnknown, mojom::IPAddressSpace::kPrivate));

  RunUntilCreateLoaderAndStartCalled();
  NotifyLoaderClientOnReceiveResponse({
      {"Access-Control-Allow-Origin", "https://example.com"},
      {"Access-Control-Allow-Credentials", "true"},
      {"Access-Control-Allow-Private-Network", "true"},
  });
  NotifyLoaderClientOnComplete(net::OK);

  RunUntilCreateLoaderAndStartCalled();
  NotifyLoaderClientOnReceiveResponse();
  NotifyLoaderClientOnComplete(net::OK);
  RunUntilComplete();

  EXPECT_EQ(client().completion_status().error_code, net::OK);

  // Make a second request, observe that it does use the preflight cache.

  CreateLoaderAndStart(request);
  RunUntilCreateLoaderAndStartCalled();
  NotifyLoaderClientOnComplete(CorsErrorStatus(
      mojom::CorsError::kUnexpectedPrivateNetworkAccess,
      mojom::IPAddressSpace::kUnknown, mojom::IPAddressSpace::kPrivate));

  RunUntilCreateLoaderAndStartCalled();

  // Second request is not preflight because it is found in the cache.
  EXPECT_EQ(GetRequest().method, "GET");

  NotifyLoaderClientOnReceiveResponse();
  NotifyLoaderClientOnComplete(net::OK);
  RunUntilComplete();

  // Send the same request again. This time the initial connection observes a
  // private network access to a different IP address space: `kLocal`.
  // A preflight request should be sent with its `target_ip_address_space` set
  // to `kLocal`.  CreateLoaderAndStart(request);
  CreateLoaderAndStart(request);
  RunUntilCreateLoaderAndStartCalled();
  NotifyLoaderClientOnComplete(CorsErrorStatus(
      mojom::CorsError::kUnexpectedPrivateNetworkAccess,
      mojom::IPAddressSpace::kUnknown, mojom::IPAddressSpace::kLocal));

  RunUntilCreateLoaderAndStartCalled();

  // Third request should be a preflight, because preflight cache entries are
  // keyed by `target_ip_address_space`, so the previously cached preflight
  // result cannot be reused for this request.
  EXPECT_EQ(GetRequest().method, "OPTIONS");
}

// This test verifies that successful PNA preflights do not place entries in the
// preflight cache that are shared with non-PNA preflights. In other words, a
// non-PNA preflight cannot be skipped because a PNA preflght previously
// succeeded.
TEST_F(CorsURLLoaderPrivateNetworkAccessTest, DoesNotShareCache) {
  auto initiator = url::Origin::Create(GURL("https://foo.example"));
  ResetFactory(initiator, mojom::kBrowserProcessId);

  ResourceRequest request;
  request.method = "PUT";
  request.mode = mojom::RequestMode::kCors;
  request.url = GURL("https://example.com/");
  request.request_initiator = initiator;

  CreateLoaderAndStart(request);
  RunUntilCreateLoaderAndStartCalled();
  NotifyLoaderClientOnComplete(CorsErrorStatus(
      mojom::CorsError::kUnexpectedPrivateNetworkAccess,
      mojom::IPAddressSpace::kUnknown, mojom::IPAddressSpace::kPrivate));

  RunUntilCreateLoaderAndStartCalled();
  NotifyLoaderClientOnReceiveResponse({
      {"Access-Control-Allow-Methods", "PUT"},
      {"Access-Control-Allow-Origin", "https://foo.example"},
      {"Access-Control-Allow-Credentials", "true"},
      {"Access-Control-Allow-Private-Network", "true"},
  });
  NotifyLoaderClientOnComplete(net::OK);

  RunUntilCreateLoaderAndStartCalled();
  NotifyLoaderClientOnReceiveResponse({
      {"Access-Control-Allow-Methods", "PUT"},
      {"Access-Control-Allow-Origin", "https://foo.example"},
      {"Access-Control-Allow-Credentials", "true"},
  });
  NotifyLoaderClientOnComplete(net::OK);
  RunUntilComplete();

  EXPECT_EQ(client().completion_status().error_code, net::OK);

  // Make a second request, observe that it does not use the preflight cache.

  CreateLoaderAndStart(request);
  RunUntilCreateLoaderAndStartCalled();

  // Second preflight request.
  EXPECT_EQ(GetRequest().method, "OPTIONS");
}

// The following `PrivateNetworkAccessPolicyWarn*` tests verify the correct
// functioning of the `kPreflightWarn` private network request policy. That is,
// preflight errors caused exclusively by Private Network Access logic should
// be ignored.
//
// The `*PolicyWarnSimple*` variants test what happens in the "simple request"
// case, when a preflight would not have been sent were it not for Private
// Network Access. The `*PolicyWarnPreflight*` variants test what happens when
// a preflight was attempted before noticing the private network access.
//
// TODO(crbug.com/40204695): Remove these tests once the policy is never
// set to `kPreflightWarn` anymore.

// This test verifies that when:
//
//  - the private network request policy is set to `kPreflightWarn`
//  - a simple request detects a private network request
//  - the following PNA preflight fails due to a network error
//
// ... the error is ignored and the request proceeds.
TEST_F(CorsURLLoaderPrivateNetworkAccessTest, PolicyWarnSimpleNetError) {
  auto initiator_origin = url::Origin::Create(GURL("https://example.com"));

  ResetFactoryParams factory_params;
  factory_params.is_trusted = true;
  ResetFactory(initiator_origin, kRendererProcessId, factory_params);

  MockDevToolsObserver devtools_observer;
  ResourceRequest request;
  request.method = "GET";
  request.mode = mojom::RequestMode::kCors;
  request.url = GURL("https://example.com/");
  request.request_initiator = initiator_origin;
  request.trusted_params =
      RequestTrustedParamsBuilder()
          .WithClientSecurityState(
              ClientSecurityStateBuilder()
                  .WithPrivateNetworkRequestPolicy(
                      mojom::PrivateNetworkRequestPolicy::kPreflightWarn)
                  .WithIsSecureContext(true)
                  .WithIPAddressSpace(mojom::IPAddressSpace::kPublic)
                  .Build())
          .WithDevToolsObserver(devtools_observer.Bind())
          .Build();
  request.devtools_request_id = "devtools";

  base::HistogramTester histogram_tester;

  CreateLoaderAndStart(request);
  RunUntilCreateLoaderAndStartCalled();
  NotifyLoaderClientOnComplete(CorsErrorStatus(
      mojom::CorsError::kUnexpectedPrivateNetworkAccess,
      mojom::IPAddressSpace::kUnknown, mojom::IPAddressSpace::kPrivate));

  RunUntilCreateLoaderAndStartCalled();
  NotifyLoaderClientOnComplete(net::ERR_INVALID_ARGUMENT);

  RunUntilCreateLoaderAndStartCalled();
  NotifyLoaderClientOnReceiveResponse();
  NotifyLoaderClientOnComplete(net::OK);
  RunUntilComplete();

  EXPECT_EQ(client().response_head()->private_network_access_preflight_result,
            mojom::PrivateNetworkAccessPreflightResult::kWarning);

  EXPECT_EQ(client().completion_status().error_code, net::OK);
  EXPECT_EQ(
      client().completion_status().private_network_access_preflight_result,
      mojom::PrivateNetworkAccessPreflightResult::kNone);

  EXPECT_THAT(histogram_tester.GetAllSamples(kPreflightErrorHistogramName),
              IsEmpty());
  EXPECT_THAT(histogram_tester.GetAllSamples(kPreflightWarningHistogramName),
              ElementsAre(MakeBucket(mojom::CorsError::kInvalidResponse, 1)));

  devtools_observer.WaitUntilCorsError();

  const MockDevToolsObserver::OnCorsErrorParams& error_params =
      *devtools_observer.cors_error_params();
  EXPECT_EQ(error_params.status,
            CorsErrorStatus(mojom::CorsError::kInvalidResponse,
                            mojom::IPAddressSpace::kPrivate,
                            mojom::IPAddressSpace::kPrivate));
  EXPECT_TRUE(error_params.is_warning);
  ASSERT_TRUE(error_params.client_security_state);
  EXPECT_TRUE(error_params.client_security_state->is_web_secure_context);
  EXPECT_EQ(error_params.client_security_state->private_network_request_policy,
            mojom::PrivateNetworkRequestPolicy::kPreflightWarn);
  EXPECT_EQ(error_params.client_security_state->ip_address_space,
            mojom::IPAddressSpace::kPublic);
}

// This test verifies that when:
//
//  - the private network request policy is set to `kPreflightWarn`
//  - a simple request detects a private network request
//  - the following PNA preflight response takes forever to arrive
//
// ... a short timeout is applied, the error ignored and the request proceeds.
TEST_F(CorsURLLoaderPrivateNetworkAccessTest, PolicyWarnSimpleTimeout) {
  auto initiator_origin = url::Origin::Create(GURL("https://example.com"));

  ResetFactoryParams factory_params;
  factory_params.is_trusted = true;
  ResetFactory(initiator_origin, kRendererProcessId, factory_params);

  MockDevToolsObserver devtools_observer;
  ResourceRequest request;
  request.method = "GET";
  request.mode = mojom::RequestMode::kCors;
  request.url = GURL("https://example.com/");
  request.request_initiator = initiator_origin;
  request.trusted_params =
      RequestTrustedParamsBuilder()
          .WithClientSecurityState(
              ClientSecurityStateBuilder()
                  .WithPrivateNetworkRequestPolicy(
                      mojom::PrivateNetworkRequestPolicy::kPreflightWarn)
                  .WithIsSecureContext(true)
                  .WithIPAddressSpace(mojom::IPAddressSpace::kPublic)
                  .Build())
          .WithDevToolsObserver(devtools_observer.Bind())
          .Build();
  request.devtools_request_id = "devtools";

  base::HistogramTester histogram_tester;

  CreateLoaderAndStart(request);
  RunUntilCreateLoaderAndStartCalled();
  NotifyLoaderClientOnComplete(CorsErrorStatus(
      mojom::CorsError::kUnexpectedPrivateNetworkAccess,
      mojom::IPAddressSpace::kUnknown, mojom::IPAddressSpace::kPrivate));

  RunUntilCreateLoaderAndStartCalled();
  // Here we intentionally wait for PreflightLoader to be timed out instead
  // of calling OnComplete.

  RunUntilCreateLoaderAndStartCalled();
  NotifyLoaderClientOnReceiveResponse();
  NotifyLoaderClientOnComplete(net::OK);
  RunUntilComplete();

  EXPECT_EQ(client().response_head()->private_network_access_preflight_result,
            mojom::PrivateNetworkAccessPreflightResult::kWarning);

  EXPECT_EQ(client().completion_status().error_code, net::OK);
  EXPECT_EQ(
      client().completion_status().private_network_access_preflight_result,
      mojom::PrivateNetworkAccessPreflightResult::kNone);

  EXPECT_THAT(histogram_tester.GetAllSamples(kPreflightErrorHistogramName),
              IsEmpty());
  EXPECT_THAT(histogram_tester.GetAllSamples(kPreflightWarningHistogramName),
              ElementsAre(MakeBucket(mojom::CorsError::kInvalidResponse, 1)));

  devtools_observer.WaitUntilCorsError();

  const MockDevToolsObserver::OnCorsErrorParams& error_params =
      *devtools_observer.cors_error_params();
  EXPECT_EQ(error_params.status,
            CorsErrorStatus(mojom::CorsError::kInvalidResponse,
                            mojom::IPAddressSpace::kPrivate,
                            mojom::IPAddressSpace::kPrivate));
  EXPECT_TRUE(error_params.is_warning);
  ASSERT_TRUE(error_params.client_security_state);
  EXPECT_TRUE(error_params.client_security_state->is_web_secure_context);
  EXPECT_EQ(error_params.client_security_state->private_network_request_policy,
            mojom::PrivateNetworkRequestPolicy::kPreflightWarn);
  EXPECT_EQ(error_params.client_security_state->ip_address_space,
            mojom::IPAddressSpace::kPublic);
}

// This test verifies that when:
//
//  - the private network request policy is set to `kPreflightWarn`
//  - a CORS preflight request detects a private network request
//  - the following PNA preflight response takes forever to arrive
//
// ... we wait as long as it takes for the response to arrive.
TEST_F(CorsURLLoaderPrivateNetworkAccessTest, PolicyWarnPreflightNoTimeout) {
  auto initiator_origin = url::Origin::Create(GURL("https://example.com"));

  ResetFactoryParams factory_params;
  factory_params.is_trusted = true;
  ResetFactory(initiator_origin, kRendererProcessId, factory_params);

  MockDevToolsObserver devtools_observer;
  ResourceRequest request;
  request.method = "PUT";
  request.mode = mojom::RequestMode::kCors;
  request.url = GURL("https://other.example/");
  request.request_initiator = initiator_origin;
  request.trusted_params =
      RequestTrustedParamsBuilder()
          .WithClientSecurityState(
              ClientSecurityStateBuilder()
                  .WithPrivateNetworkRequestPolicy(
                      mojom::PrivateNetworkRequestPolicy::kPreflightWarn)
                  .WithIsSecureContext(true)
                  .WithIPAddressSpace(mojom::IPAddressSpace::kPublic)
                  .Build())
          .WithDevToolsObserver(devtools_observer.Bind())
          .Build();

  base::HistogramTester histogram_tester;

  CreateLoaderAndStart(request);
  RunUntilCreateLoaderAndStartCalled();
  NotifyLoaderClientOnComplete(CorsErrorStatus(
      mojom::CorsError::kUnexpectedPrivateNetworkAccess,
      mojom::IPAddressSpace::kUnknown, mojom::IPAddressSpace::kPrivate));

  RunUntilCreateLoaderAndStartCalled();

  // Simulate a response that takes longer to arrive than the preflight timeout.
  // This should still work, because the timeout should not be applied.
  base::OneShotTimer timer;
  timer.Start(FROM_HERE, base::Milliseconds(500),
              base::BindLambdaForTesting([this] {
                NotifyLoaderClientOnReceiveResponse({
                    {"Access-Control-Allow-Methods", "PUT"},
                    {"Access-Control-Allow-Origin", "https://example.com"},
                    {"Access-Control-Allow-Credentials", "true"},
                    {"Access-Control-Allow-Private-Network", "true"},
                });
              }));

  RunUntilCreateLoaderAndStartCalled();
  NotifyLoaderClientOnReceiveResponse({
      {"Access-Control-Allow-Methods", "PUT"},
      {"Access-Control-Allow-Origin", "https://example.com"},
      {"Access-Control-Allow-Credentials", "true"},
  });
  NotifyLoaderClientOnComplete(net::OK);
  RunUntilComplete();

  EXPECT_EQ(client().response_head()->private_network_access_preflight_result,
            mojom::PrivateNetworkAccessPreflightResult::kSuccess);

  EXPECT_EQ(client().completion_status().error_code, net::OK);
  EXPECT_EQ(
      client().completion_status().private_network_access_preflight_result,
      mojom::PrivateNetworkAccessPreflightResult::kNone);

  EXPECT_THAT(histogram_tester.GetAllSamples(kPreflightWarningHistogramName),
              IsEmpty());
}

// This test verifies that when:
//
//  - the private network request policy is set to `kPreflightBlock`
//  - a simple request detects a private network request
//  - the following PNA preflight response takes forever to arrive
//
// ... we wait as long as it takes for the response to arrive.
TEST_F(CorsURLLoaderPrivateNetworkAccessTest, PolicyBlockPreflightNoTimeout) {
  auto initiator_origin = url::Origin::Create(GURL("https://example.com"));

  ResetFactoryParams factory_params;
  factory_params.is_trusted = true;
  ResetFactory(initiator_origin, kRendererProcessId, factory_params);

  ResourceRequest request;
  request.method = "GET";
  request.mode = mojom::RequestMode::kCors;
  request.url = GURL("https://example.com/");
  request.request_initiator = initiator_origin;
  request.trusted_params =
      RequestTrustedParamsBuilder()
          .WithClientSecurityState(
              ClientSecurityStateBuilder()
                  .WithPrivateNetworkRequestPolicy(
                      mojom::PrivateNetworkRequestPolicy::kPreflightBlock)
                  .WithIsSecureContext(true)
                  .WithIPAddressSpace(mojom::IPAddressSpace::kPublic)
                  .Build())
          .Build();
  request.devtools_request_id = "devtools";

  base::HistogramTester histogram_tester;

  CreateLoaderAndStart(request);
  RunUntilCreateLoaderAndStartCalled();
  NotifyLoaderClientOnComplete(CorsErrorStatus(
      mojom::CorsError::kUnexpectedPrivateNetworkAccess,
      mojom::IPAddressSpace::kUnknown, mojom::IPAddressSpace::kPrivate));

  RunUntilCreateLoaderAndStartCalled();

  // Simulate a response that takes longer to arrive than the preflight timeout.
  // This should still work, because the timeout should not be applied.
  base::OneShotTimer timer;
  timer.Start(FROM_HERE, base::Milliseconds(500),
              base::BindLambdaForTesting([this] {
                NotifyLoaderClientOnReceiveResponse({
                    {"Access-Control-Allow-Methods", "PUT"},
                    {"Access-Control-Allow-Origin", "https://example.com"},
                    {"Access-Control-Allow-Credentials", "true"},
                    {"Access-Control-Allow-Private-Network", "true"},
                });
              }));

  RunUntilCreateLoaderAndStartCalled();
  NotifyLoaderClientOnReceiveResponse({
      {"Access-Control-Allow-Methods", "PUT"},
      {"Access-Control-Allow-Origin", "https://example.com"},
      {"Access-Control-Allow-Credentials", "true"},
  });
  NotifyLoaderClientOnComplete(net::OK);
  RunUntilComplete();

  EXPECT_EQ(client().response_head()->private_network_access_preflight_result,
            mojom::PrivateNetworkAccessPreflightResult::kSuccess);

  EXPECT_EQ(client().completion_status().error_code, net::OK);
  EXPECT_EQ(
      client().completion_status().private_network_access_preflight_result,
      mojom::PrivateNetworkAccessPreflightResult::kNone);

  EXPECT_THAT(histogram_tester.GetAllSamples(kPreflightWarningHistogramName),
              IsEmpty());
}

// This test verifies that when:
//
//  - the private network request policy is set to `kPreflightWarn`
//  - a simple request detects a private network request
//  - the following PNA preflight fails due to a non-PNA CORS error
//
// ... the error is ignored and the request proceeds.
TEST_F(CorsURLLoaderPrivateNetworkAccessTest, PolicyWarnSimpleCorsError) {
  auto initiator_origin = url::Origin::Create(GURL("https://example.com"));

  ResetFactoryParams factory_params;
  factory_params.is_trusted = true;
  ResetFactory(initiator_origin, kRendererProcessId, factory_params);

  ResourceRequest request;
  request.method = "GET";
  request.mode = mojom::RequestMode::kCors;
  request.url = GURL("https://example.com/");
  request.request_initiator = initiator_origin;
  request.trusted_params =
      RequestTrustedParamsBuilder()
          .WithPrivateNetworkRequestPolicy(
              mojom::PrivateNetworkRequestPolicy::kPreflightWarn)
          .Build();

  base::HistogramTester histogram_tester;

  CreateLoaderAndStart(request);
  RunUntilCreateLoaderAndStartCalled();
  NotifyLoaderClientOnComplete(CorsErrorStatus(
      mojom::CorsError::kUnexpectedPrivateNetworkAccess,
      mojom::IPAddressSpace::kUnknown, mojom::IPAddressSpace::kPrivate));

  RunUntilCreateLoaderAndStartCalled();
  NotifyLoaderClientOnReceiveResponse();

  RunUntilCreateLoaderAndStartCalled();
  NotifyLoaderClientOnReceiveResponse();
  NotifyLoaderClientOnComplete(net::OK);
  RunUntilComplete();

  EXPECT_EQ(client().response_head()->private_network_access_preflight_result,
            mojom::PrivateNetworkAccessPreflightResult::kWarning);

  EXPECT_EQ(client().completion_status().error_code, net::OK);
  EXPECT_EQ(
      client().completion_status().private_network_access_preflight_result,
      mojom::PrivateNetworkAccessPreflightResult::kNone);

  EXPECT_THAT(histogram_tester.GetAllSamples(kPreflightErrorHistogramName),
              IsEmpty());
  EXPECT_THAT(histogram_tester.GetAllSamples(kPreflightWarningHistogramName),
              ElementsAre(MakeBucket(
                  mojom::CorsError::kPreflightMissingAllowOriginHeader, 1)));
}

// This test verifies that when:
//
//  - the private network request policy is set to `kPreflightWarn`
//  - a simple request detects a private network request
//  - the following PNA preflight fails due to a missing PNA header
//
// ... the error is ignored and the request proceeds.
TEST_F(CorsURLLoaderPrivateNetworkAccessTest,
       PolicyWarnSimpleMissingAllowPrivateNetwork) {
  auto initiator_origin = url::Origin::Create(GURL("https://example.com"));

  ResetFactoryParams factory_params;
  factory_params.is_trusted = true;
  ResetFactory(initiator_origin, kRendererProcessId, factory_params);

  ResourceRequest request;
  request.method = "GET";
  request.mode = mojom::RequestMode::kCors;
  request.url = GURL("https://example.com/");
  request.request_initiator = initiator_origin;
  request.trusted_params =
      RequestTrustedParamsBuilder()
          .WithPrivateNetworkRequestPolicy(
              mojom::PrivateNetworkRequestPolicy::kPreflightWarn)
          .Build();

  base::HistogramTester histogram_tester;

  CreateLoaderAndStart(request);
  RunUntilCreateLoaderAndStartCalled();
  NotifyLoaderClientOnComplete(CorsErrorStatus(
      mojom::CorsError::kUnexpectedPrivateNetworkAccess,
      mojom::IPAddressSpace::kUnknown, mojom::IPAddressSpace::kPrivate));

  RunUntilCreateLoaderAndStartCalled();
  NotifyLoaderClientOnReceiveResponse({
      {"Access-Control-Allow-Methods", "GET"},
      {"Access-Control-Allow-Origin", "https://example.com"},
      {"Access-Control-Allow-Credentials", "true"},
  });

  RunUntilCreateLoaderAndStartCalled();
  NotifyLoaderClientOnReceiveResponse();
  NotifyLoaderClientOnComplete(net::OK);
  RunUntilComplete();

  EXPECT_EQ(client().response_head()->private_network_access_preflight_result,
            mojom::PrivateNetworkAccessPreflightResult::kWarning);

  EXPECT_EQ(client().completion_status().error_code, net::OK);
  EXPECT_EQ(
      client().completion_status().private_network_access_preflight_result,
      mojom::PrivateNetworkAccessPreflightResult::kNone);

  EXPECT_THAT(histogram_tester.GetAllSamples(kPreflightErrorHistogramName),
              IsEmpty());
  EXPECT_THAT(histogram_tester.GetAllSamples(kPreflightWarningHistogramName),
              ElementsAre(MakeBucket(
                  mojom::CorsError::kPreflightMissingAllowPrivateNetwork, 1)));
}

// This test verifies that when:
//
//  - the private network request policy is set to `kPreflightWarn`
//  - a simple request detects a private network request
//  - the following PNA preflight fails due to an invalid PNA header
//
// ... the error is ignored and the request proceeds.
TEST_F(CorsURLLoaderPrivateNetworkAccessTest,
       PolicyWarnSimpleInvalidAllowPrivateNetwork) {
  auto initiator_origin = url::Origin::Create(GURL("https://example.com"));

  ResetFactoryParams factory_params;
  factory_params.is_trusted = true;
  ResetFactory(initiator_origin, kRendererProcessId, factory_params);

  ResourceRequest request;
  request.method = "GET";
  request.mode = mojom::RequestMode::kCors;
  request.url = GURL("https://example.com/");
  request.request_initiator = initiator_origin;
  request.trusted_params =
      RequestTrustedParamsBuilder()
          .WithPrivateNetworkRequestPolicy(
              mojom::PrivateNetworkRequestPolicy::kPreflightWarn)
          .Build();

  base::HistogramTester histogram_tester;

  CreateLoaderAndStart(request);
  RunUntilCreateLoaderAndStartCalled();
  NotifyLoaderClientOnComplete(CorsErrorStatus(
      mojom::CorsError::kUnexpectedPrivateNetworkAccess,
      mojom::IPAddressSpace::kUnknown, mojom::IPAddressSpace::kPrivate));

  RunUntilCreateLoaderAndStartCalled();
  NotifyLoaderClientOnReceiveResponse({
      {"Access-Control-Allow-Methods", "GET"},
      {"Access-Control-Allow-Origin", "https://example.com"},
      {"Access-Control-Allow-Credentials", "true"},
      {"Access-Control-Allow-Private-Network", "invalid-value"},
  });

  RunUntilCreateLoaderAndStartCalled();
  NotifyLoaderClientOnReceiveResponse();
  NotifyLoaderClientOnComplete(net::OK);
  RunUntilComplete();

  EXPECT_EQ(client().response_head()->private_network_access_preflight_result,
            mojom::PrivateNetworkAccessPreflightResult::kWarning);

  EXPECT_EQ(client().completion_status().error_code, net::OK);
  EXPECT_EQ(
      client().completion_status().private_network_access_preflight_result,
      mojom::PrivateNetworkAccessPreflightResult::kNone);

  EXPECT_THAT(histogram_tester.GetAllSamples(kPreflightErrorHistogramName),
              IsEmpty());
  EXPECT_THAT(histogram_tester.GetAllSamples(kPreflightWarningHistogramName),
              ElementsAre(MakeBucket(
                  mojom::CorsError::kPreflightInvalidAllowPrivateNetwork, 1)));
}

// This test verifies that when:
//
//  - the private network request policy is set to `kPreflightWarn`
//  - a CORS preflight request detects a private network request
//  - the following PNA preflight fails due to a network error
//
// ... the error is not ignored and the request is failed.
TEST_F(CorsURLLoaderPrivateNetworkAccessTest, PolicyWarnPreflightNetError) {
  auto initiator_origin = url::Origin::Create(GURL("https://example.com"));

  ResetFactoryParams factory_params;
  factory_params.is_trusted = true;
  ResetFactory(initiator_origin, kRendererProcessId, factory_params);

  ResourceRequest request;
  request.method = "PUT";
  request.mode = mojom::RequestMode::kCors;
  request.url = GURL("https://other.example/");
  request.request_initiator = initiator_origin;
  request.trusted_params =
      RequestTrustedParamsBuilder()
          .WithPrivateNetworkRequestPolicy(
              mojom::PrivateNetworkRequestPolicy::kPreflightWarn)
          .Build();

  base::HistogramTester histogram_tester;

  CreateLoaderAndStart(request);
  RunUntilCreateLoaderAndStartCalled();
  NotifyLoaderClientOnComplete(CorsErrorStatus(
      mojom::CorsError::kUnexpectedPrivateNetworkAccess,
      mojom::IPAddressSpace::kUnknown, mojom::IPAddressSpace::kPrivate));

  RunUntilCreateLoaderAndStartCalled();
  NotifyLoaderClientOnComplete(net::ERR_INVALID_ARGUMENT);
  RunUntilComplete();

  EXPECT_EQ(client().completion_status().error_code,
            net::ERR_BLOCKED_BY_PRIVATE_NETWORK_ACCESS_CHECKS);
  EXPECT_EQ(
      client().completion_status().private_network_access_preflight_result,
      mojom::PrivateNetworkAccessPreflightResult::kError);

  EXPECT_THAT(
      histogram_tester.GetAllSamples(kPreflightErrorHistogramName),
      ElementsAre(
          MakeBucket(mojom::CorsError::kInvalidResponse, 1),
          // TODO(crbug.com/40212338): This should not be logged.
          MakeBucket(mojom::CorsError::kUnexpectedPrivateNetworkAccess, 1)));
  EXPECT_THAT(histogram_tester.GetAllSamples(kPreflightWarningHistogramName),
              IsEmpty());
}

// This test verifies that when:
//
//  - the private network request policy is set to `kPreflightWarn`
//  - a CORS preflight request detects a private network request
//  - the following PNA preflight fails due to a non-PNA CORS error
//
// ... the error is not ignored and the request is failed.
TEST_F(CorsURLLoaderPrivateNetworkAccessTest, PolicyWarnPreflightCorsError) {
  auto initiator_origin = url::Origin::Create(GURL("https://example.com"));

  ResetFactoryParams factory_params;
  factory_params.is_trusted = true;
  ResetFactory(initiator_origin, kRendererProcessId, factory_params);

  MockDevToolsObserver devtools_observer;

  ResourceRequest request;
  request.method = "PUT";
  request.mode = mojom::RequestMode::kCors;
  request.url = GURL("https://other.example/");
  request.request_initiator = initiator_origin;
  request.trusted_params =
      RequestTrustedParamsBuilder()
          .WithClientSecurityState(
              ClientSecurityStateBuilder()
                  .WithPrivateNetworkRequestPolicy(
                      mojom::PrivateNetworkRequestPolicy::kPreflightWarn)
                  .WithIsSecureContext(true)
                  .WithIPAddressSpace(mojom::IPAddressSpace::kPublic)
                  .Build())
          .WithDevToolsObserver(devtools_observer.Bind())
          .Build();
  request.devtools_request_id = "devtools";

  base::HistogramTester histogram_tester;

  CreateLoaderAndStart(request);
  RunUntilCreateLoaderAndStartCalled();
  NotifyLoaderClientOnComplete(CorsErrorStatus(
      mojom::CorsError::kUnexpectedPrivateNetworkAccess,
      mojom::IPAddressSpace::kUnknown, mojom::IPAddressSpace::kPrivate));

  RunUntilCreateLoaderAndStartCalled();
  NotifyLoaderClientOnReceiveResponse();
  RunUntilComplete();

  EXPECT_EQ(client().completion_status().error_code,
            net::ERR_BLOCKED_BY_PRIVATE_NETWORK_ACCESS_CHECKS);
  EXPECT_EQ(
      client().completion_status().private_network_access_preflight_result,
      mojom::PrivateNetworkAccessPreflightResult::kError);
  EXPECT_THAT(client().completion_status().cors_error_status,
              Optional(CorsErrorStatus(
                  mojom::CorsError::kPreflightMissingAllowOriginHeader,
                  network::mojom::IPAddressSpace::kPrivate,
                  network::mojom::IPAddressSpace::kUnknown)));

  EXPECT_THAT(
      histogram_tester.GetAllSamples(kPreflightErrorHistogramName),
      ElementsAre(
          MakeBucket(mojom::CorsError::kPreflightMissingAllowOriginHeader, 1),
          // TODO(crbug.com/40212338): This should not be logged.
          MakeBucket(mojom::CorsError::kUnexpectedPrivateNetworkAccess, 1)));
  EXPECT_THAT(histogram_tester.GetAllSamples(kPreflightWarningHistogramName),
              IsEmpty());
  const network::URLLoaderCompletionStatus& preflight_status =
      *devtools_observer.preflight_status();
  EXPECT_EQ(preflight_status.cors_error_status,
            CorsErrorStatus(mojom::CorsError::kUnexpectedPrivateNetworkAccess,
                            mojom::IPAddressSpace::kUnknown,
                            mojom::IPAddressSpace::kPrivate));

  devtools_observer.WaitUntilCorsError();

  const MockDevToolsObserver::OnCorsErrorParams& error_params =
      *devtools_observer.cors_error_params();
  EXPECT_EQ(
      error_params.status,
      CorsErrorStatus(mojom::CorsError::kPreflightMissingAllowOriginHeader,
                      network::mojom::IPAddressSpace::kPrivate,
                      network::mojom::IPAddressSpace::kUnknown));
  EXPECT_FALSE(error_params.is_warning);
  ASSERT_TRUE(error_params.client_security_state);
  EXPECT_TRUE(error_params.client_security_state->is_web_secure_context);
  EXPECT_EQ(error_params.client_security_state->private_network_request_policy,
            mojom::PrivateNetworkRequestPolicy::kPreflightWarn);
  EXPECT_EQ(error_params.client_security_state->ip_address_space,
            mojom::IPAddressSpace::kPublic);
}

// This test verifies that when:
//
//  - the private network request policy is set to `kPreflightWarn`
//  - a CORS preflight request detects a private network request
//  - the following PNA preflight fails due to a missing PNA header
//
// ... the error is ignored and the request proceeds.
TEST_F(CorsURLLoaderPrivateNetworkAccessTest,
       PolicyWarnPreflightMissingAllowPrivateNetwork) {
  auto initiator_origin = url::Origin::Create(GURL("https://example.com"));

  ResetFactoryParams factory_params;
  factory_params.is_trusted = true;
  ResetFactory(initiator_origin, kRendererProcessId, factory_params);

  MockDevToolsObserver devtools_observer;

  ResourceRequest request;
  request.method = "PUT";
  request.mode = mojom::RequestMode::kCors;
  request.url = GURL("https://example.com/");
  request.request_initiator = initiator_origin;
  request.trusted_params =
      RequestTrustedParamsBuilder()
          .WithClientSecurityState(
              ClientSecurityStateBuilder()
                  .WithPrivateNetworkRequestPolicy(
                      mojom::PrivateNetworkRequestPolicy::kPreflightWarn)
                  .WithIsSecureContext(true)
                  .WithIPAddressSpace(mojom::IPAddressSpace::kPublic)
                  .Build())
          .WithDevToolsObserver(devtools_observer.Bind())
          .Build();
  // Without this, the devtools observer is not passed to `PreflightController`
  // and warnings suppressed inside `PreflightController` are not observed.
  request.devtools_request_id = "devtools";

  base::HistogramTester histogram_tester;

  CreateLoaderAndStart(request);
  RunUntilCreateLoaderAndStartCalled();
  NotifyLoaderClientOnComplete(CorsErrorStatus(
      mojom::CorsError::kUnexpectedPrivateNetworkAccess,
      mojom::IPAddressSpace::kUnknown, mojom::IPAddressSpace::kPrivate));

  RunUntilCreateLoaderAndStartCalled();
  NotifyLoaderClientOnReceiveResponse({
      {"Access-Control-Allow-Methods", "PUT"},
      {"Access-Control-Allow-Origin", "https://example.com"},
      {"Access-Control-Allow-Credentials", "true"},
  });

  RunUntilCreateLoaderAndStartCalled();
  NotifyLoaderClientOnReceiveResponse();
  NotifyLoaderClientOnComplete(net::OK);
  RunUntilComplete();

  EXPECT_EQ(client().response_head()->private_network_access_preflight_result,
            mojom::PrivateNetworkAccessPreflightResult::kWarning);

  EXPECT_EQ(client().completion_status().error_code, net::OK);
  EXPECT_EQ(
      client().completion_status().private_network_access_preflight_result,
      mojom::PrivateNetworkAccessPreflightResult::kNone);

  EXPECT_THAT(histogram_tester.GetAllSamples(kPreflightErrorHistogramName),
              IsEmpty());
  EXPECT_THAT(histogram_tester.GetAllSamples(kPreflightWarningHistogramName),
              ElementsAre(MakeBucket(
                  mojom::CorsError::kPreflightMissingAllowPrivateNetwork, 1)));

  devtools_observer.WaitUntilCorsError();

  CorsErrorStatus expected_status(
      mojom::CorsError::kPreflightMissingAllowPrivateNetwork);
  expected_status.target_address_space = mojom::IPAddressSpace::kPrivate;

  const MockDevToolsObserver::OnCorsErrorParams& error_params =
      *devtools_observer.cors_error_params();
  EXPECT_EQ(error_params.devtools_request_id, "devtools");
  EXPECT_EQ(error_params.status, expected_status);
  EXPECT_TRUE(error_params.is_warning);
  ASSERT_TRUE(error_params.client_security_state);
  EXPECT_TRUE(error_params.client_security_state->is_web_secure_context);
  EXPECT_EQ(error_params.client_security_state->private_network_request_policy,
            mojom::PrivateNetworkRequestPolicy::kPreflightWarn);
  EXPECT_EQ(error_params.client_security_state->ip_address_space,
            mojom::IPAddressSpace::kPublic);
}

// Returns a PUT request for a resource from `initiator_origin`.
//
// `devtools_observer` will receive mojo messages about events concerning the
// request.
ResourceRequest MakeSameOriginPutRequest(
    const url::Origin& initiator_origin,
    MockDevToolsObserver& devtools_observer) {
  ResourceRequest request;
  request.method = "PUT";
  request.mode = mojom::RequestMode::kCors;
  request.url = initiator_origin.GetURL();
  request.request_initiator = initiator_origin;

  request.trusted_params =
      RequestTrustedParamsBuilder()
          .WithClientSecurityState(
              ClientSecurityStateBuilder()
                  .WithPrivateNetworkRequestPolicy(
                      mojom::PrivateNetworkRequestPolicy::kPreflightWarn)
                  .WithIsSecureContext(true)
                  .WithIPAddressSpace(mojom::IPAddressSpace::kPublic)
                  .Build())
          .WithDevToolsObserver(devtools_observer.Bind())
          .Build();
  // Without this, the devtools observer is not passed to `PreflightController`
  // and warnings suppressed inside `PreflightController` are not observed.
  request.devtools_request_id = "devtools";

  return request;
}

// This test verifies that when:
//
//  - the private network request policy is set to `kPreflightWarn`
//  - the request mode is set to `kCors`
//  - the request is not "simple"
//  - the request is same-origin with the initiator (so no CORS preflight)
//  - the initial request detects a private network request
//  - the following PNA preflight fails due to a net error
//
// ... the error is ignored and the request proceeds.
TEST_F(CorsURLLoaderPrivateNetworkAccessTest, PolicyWarnSameOriginNetError) {
  auto initiator_origin = url::Origin::Create(GURL("https://example.com"));

  ResetFactoryParams factory_params;
  factory_params.is_trusted = true;
  ResetFactory(initiator_origin, kRendererProcessId, factory_params);

  MockDevToolsObserver devtools_observer;
  ResourceRequest request =
      MakeSameOriginPutRequest(initiator_origin, devtools_observer);

  base::HistogramTester histogram_tester;

  CreateLoaderAndStart(request);
  RunUntilCreateLoaderAndStartCalled();
  NotifyLoaderClientOnComplete(CorsErrorStatus(
      mojom::CorsError::kUnexpectedPrivateNetworkAccess,
      mojom::IPAddressSpace::kUnknown, mojom::IPAddressSpace::kPrivate));

  RunUntilCreateLoaderAndStartCalled();
  NotifyLoaderClientOnComplete(net::ERR_INVALID_ARGUMENT);

  RunUntilCreateLoaderAndStartCalled();
  NotifyLoaderClientOnReceiveResponse();
  NotifyLoaderClientOnComplete(net::OK);
  RunUntilComplete();

  EXPECT_EQ(client().response_head()->private_network_access_preflight_result,
            mojom::PrivateNetworkAccessPreflightResult::kWarning);

  EXPECT_EQ(client().completion_status().error_code, net::OK);
  EXPECT_EQ(client().completion_status().cors_error_status, std::nullopt);
  EXPECT_EQ(
      client().completion_status().private_network_access_preflight_result,
      mojom::PrivateNetworkAccessPreflightResult::kNone);

  EXPECT_THAT(histogram_tester.GetAllSamples(kPreflightErrorHistogramName),
              IsEmpty());
  EXPECT_THAT(histogram_tester.GetAllSamples(kPreflightWarningHistogramName),
              ElementsAre(MakeBucket(mojom::CorsError::kInvalidResponse, 1)));

  devtools_observer.WaitUntilCorsError();

  const MockDevToolsObserver::OnCorsErrorParams& error_params =
      *devtools_observer.cors_error_params();
  EXPECT_EQ(error_params.status,
            CorsErrorStatus(mojom::CorsError::kInvalidResponse,
                            network::mojom::IPAddressSpace::kPrivate,
                            network::mojom::IPAddressSpace::kPrivate));
  EXPECT_TRUE(error_params.is_warning);
  ASSERT_TRUE(error_params.client_security_state);
  EXPECT_TRUE(error_params.client_security_state->is_web_secure_context);
  EXPECT_EQ(error_params.client_security_state->private_network_request_policy,
            mojom::PrivateNetworkRequestPolicy::kPreflightWarn);
  EXPECT_EQ(error_params.client_security_state->ip_address_space,
            mojom::IPAddressSpace::kPublic);
}

// This test verifies that when:
//
//  - the private network request policy is set to `kPreflightWarn`
//  - the request mode is set to `kCors`
//  - the request is not "simple"
//  - the request is same-origin with the initiator (so no CORS preflight)
//  - the initial request detects a private network request
//  - the following PNA preflight fails due to a non-PNA CORS error
//
// ... the error is ignored and the request proceeds.
TEST_F(CorsURLLoaderPrivateNetworkAccessTest, PolicyWarnSameOriginCorsError) {
  auto initiator_origin = url::Origin::Create(GURL("https://example.com"));

  ResetFactoryParams factory_params;
  factory_params.is_trusted = true;
  ResetFactory(initiator_origin, kRendererProcessId, factory_params);

  MockDevToolsObserver devtools_observer;

  ResourceRequest request =
      MakeSameOriginPutRequest(initiator_origin, devtools_observer);

  base::HistogramTester histogram_tester;

  CreateLoaderAndStart(request);
  RunUntilCreateLoaderAndStartCalled();
  NotifyLoaderClientOnComplete(CorsErrorStatus(
      mojom::CorsError::kUnexpectedPrivateNetworkAccess,
      mojom::IPAddressSpace::kUnknown, mojom::IPAddressSpace::kPrivate));

  RunUntilCreateLoaderAndStartCalled();
  NotifyLoaderClientOnReceiveResponse();

  RunUntilCreateLoaderAndStartCalled();
  NotifyLoaderClientOnReceiveResponse();
  NotifyLoaderClientOnComplete(net::OK);
  RunUntilComplete();

  EXPECT_EQ(client().response_head()->private_network_access_preflight_result,
            mojom::PrivateNetworkAccessPreflightResult::kWarning);

  EXPECT_EQ(client().completion_status().error_code, net::OK);
  EXPECT_EQ(client().completion_status().cors_error_status, std::nullopt);
  EXPECT_EQ(
      client().completion_status().private_network_access_preflight_result,
      mojom::PrivateNetworkAccessPreflightResult::kNone);

  EXPECT_THAT(histogram_tester.GetAllSamples(kPreflightErrorHistogramName),
              IsEmpty());
  EXPECT_THAT(histogram_tester.GetAllSamples(kPreflightWarningHistogramName),
              ElementsAre(MakeBucket(
                  mojom::CorsError::kPreflightMissingAllowOriginHeader, 1)));

  devtools_observer.WaitUntilCorsError();

  const MockDevToolsObserver::OnCorsErrorParams& error_params =
      *devtools_observer.cors_error_params();
  EXPECT_EQ(
      error_params.status,
      CorsErrorStatus(mojom::CorsError::kPreflightMissingAllowOriginHeader,
                      network::mojom::IPAddressSpace::kPrivate,
                      network::mojom::IPAddressSpace::kUnknown));
  EXPECT_TRUE(error_params.is_warning);
  ASSERT_TRUE(error_params.client_security_state);
  EXPECT_TRUE(error_params.client_security_state->is_web_secure_context);
  EXPECT_EQ(error_params.client_security_state->private_network_request_policy,
            mojom::PrivateNetworkRequestPolicy::kPreflightWarn);
  EXPECT_EQ(error_params.client_security_state->ip_address_space,
            mojom::IPAddressSpace::kPublic);
}

// This test verifies that when:
//
//  - the private network request policy is set to `kPreflightWarn`
//  - the request mode is set to `kCors`
//  - the request is not "simple"
//  - the request is same-origin with the initiator (so no CORS preflight)
//  - the initial request detects a private network request
//  - the following PNA preflight fails due to missing PNA header
//
// ... the error is ignored and the request proceeds.
TEST_F(CorsURLLoaderPrivateNetworkAccessTest,
       PolicyWarnSameOriginMissingAllowPrivateNetwork) {
  auto initiator_origin = url::Origin::Create(GURL("https://example.com"));

  ResetFactoryParams factory_params;
  factory_params.is_trusted = true;
  ResetFactory(initiator_origin, kRendererProcessId, factory_params);

  MockDevToolsObserver devtools_observer;

  ResourceRequest request =
      MakeSameOriginPutRequest(initiator_origin, devtools_observer);

  base::HistogramTester histogram_tester;

  CreateLoaderAndStart(request);
  RunUntilCreateLoaderAndStartCalled();
  NotifyLoaderClientOnComplete(CorsErrorStatus(
      mojom::CorsError::kUnexpectedPrivateNetworkAccess,
      mojom::IPAddressSpace::kUnknown, mojom::IPAddressSpace::kPrivate));

  RunUntilCreateLoaderAndStartCalled();
  NotifyLoaderClientOnReceiveResponse({
      {"Access-Control-Allow-Methods", "PUT"},
      {"Access-Control-Allow-Origin", "https://example.com"},
      {"Access-Control-Allow-Credentials", "true"},
  });

  RunUntilCreateLoaderAndStartCalled();
  NotifyLoaderClientOnReceiveResponse();
  NotifyLoaderClientOnComplete(net::OK);
  RunUntilComplete();

  EXPECT_EQ(client().response_head()->private_network_access_preflight_result,
            mojom::PrivateNetworkAccessPreflightResult::kWarning);

  EXPECT_EQ(client().completion_status().error_code, net::OK);
  EXPECT_EQ(
      client().completion_status().private_network_access_preflight_result,
      mojom::PrivateNetworkAccessPreflightResult::kNone);

  EXPECT_THAT(histogram_tester.GetAllSamples(kPreflightErrorHistogramName),
              IsEmpty());
  EXPECT_THAT(histogram_tester.GetAllSamples(kPreflightWarningHistogramName),
              ElementsAre(MakeBucket(
                  mojom::CorsError::kPreflightMissingAllowPrivateNetwork, 1)));

  devtools_observer.WaitUntilCorsError();

  CorsErrorStatus expected_status(
      mojom::CorsError::kPreflightMissingAllowPrivateNetwork);
  expected_status.target_address_space = mojom::IPAddressSpace::kPrivate;

  const MockDevToolsObserver::OnCorsErrorParams& error_params =
      *devtools_observer.cors_error_params();
  EXPECT_EQ(error_params.devtools_request_id, "devtools");
  EXPECT_EQ(error_params.status, expected_status);
  EXPECT_TRUE(error_params.is_warning);
  ASSERT_TRUE(error_params.client_security_state);
  EXPECT_TRUE(error_params.client_security_state->is_web_secure_context);
  EXPECT_EQ(error_params.client_security_state->private_network_request_policy,
            mojom::PrivateNetworkRequestPolicy::kPreflightWarn);
  EXPECT_EQ(error_params.client_security_state->ip_address_space,
            mojom::IPAddressSpace::kPublic);
}

// The following `PrivateNetworkAccessPolicyBlock*` tests verify that PNA
// preflights must succeed for the overall request to succeed when the private
// network request policy is set to `kPreflightBlock`.

// This test verifies that when:
//
//  - the private network request policy is set to `kPreflightBlock`
//  - a private network request is detected
//  - the following PNA preflight fails due to a network error
//
// ... the error is not ignored and the request is failed.
TEST_F(CorsURLLoaderPrivateNetworkAccessTest, PolicyBlockNetError) {
  auto initiator_origin = url::Origin::Create(GURL("https://example.com"));

  ResetFactoryParams factory_params;
  factory_params.is_trusted = true;
  ResetFactory(initiator_origin, kRendererProcessId, factory_params);

  ResourceRequest request;
  request.method = "GET";
  request.mode = mojom::RequestMode::kCors;
  request.url = GURL("https://example.com/");
  request.request_initiator = initiator_origin;
  request.trusted_params =
      RequestTrustedParamsBuilder()
          .WithPrivateNetworkRequestPolicy(
              mojom::PrivateNetworkRequestPolicy::kPreflightBlock)
          .Build();

  base::HistogramTester histogram_tester;

  CreateLoaderAndStart(request);
  RunUntilCreateLoaderAndStartCalled();
  NotifyLoaderClientOnComplete(CorsErrorStatus(
      mojom::CorsError::kUnexpectedPrivateNetworkAccess,
      mojom::IPAddressSpace::kUnknown, mojom::IPAddressSpace::kPrivate));

  RunUntilCreateLoaderAndStartCalled();
  NotifyLoaderClientOnComplete(net::ERR_INVALID_ARGUMENT);
  RunUntilComplete();

  EXPECT_EQ(client().completion_status().error_code,
            net::ERR_BLOCKED_BY_PRIVATE_NETWORK_ACCESS_CHECKS);
  EXPECT_EQ(
      client().completion_status().private_network_access_preflight_result,
      mojom::PrivateNetworkAccessPreflightResult::kError);

  EXPECT_THAT(histogram_tester.GetAllSamples(kPreflightErrorHistogramName),
              ElementsAre(MakeBucket(mojom::CorsError::kInvalidResponse, 1)));
  EXPECT_THAT(histogram_tester.GetAllSamples(kPreflightWarningHistogramName),
              IsEmpty());
}

// This test verifies that when:
//
//  - the private network request policy is set to `kPreflightBlock`
//  - a simple request detects a private network request
//  - the following PNA preflight fails due to a non-PNA CORS error
//
// ... the error is not ignored and the request is failed.
TEST_F(CorsURLLoaderPrivateNetworkAccessTest, PolicyBlockCorsError) {
  auto initiator_origin = url::Origin::Create(GURL("https://example.com"));

  ResetFactoryParams factory_params;
  factory_params.is_trusted = true;
  ResetFactory(initiator_origin, kRendererProcessId, factory_params);

  MockDevToolsObserver devtools_observer;

  ResourceRequest request;
  request.method = "GET";
  request.mode = mojom::RequestMode::kCors;
  request.url = GURL("https://example.com/");
  request.request_initiator = initiator_origin;
  request.trusted_params =
      RequestTrustedParamsBuilder()
          .WithClientSecurityState(
              ClientSecurityStateBuilder()
                  .WithPrivateNetworkRequestPolicy(
                      mojom::PrivateNetworkRequestPolicy::kPreflightBlock)
                  .WithIsSecureContext(true)
                  .WithIPAddressSpace(mojom::IPAddressSpace::kPublic)
                  .Build())
          .WithDevToolsObserver(devtools_observer.Bind())
          .Build();
  request.devtools_request_id = "devtools";

  base::HistogramTester histogram_tester;

  CreateLoaderAndStart(request);
  RunUntilCreateLoaderAndStartCalled();
  NotifyLoaderClientOnComplete(CorsErrorStatus(
      mojom::CorsError::kUnexpectedPrivateNetworkAccess,
      mojom::IPAddressSpace::kUnknown, mojom::IPAddressSpace::kPrivate));

  RunUntilCreateLoaderAndStartCalled();
  NotifyLoaderClientOnReceiveResponse();
  RunUntilComplete();

  EXPECT_EQ(client().completion_status().error_code,
            net::ERR_BLOCKED_BY_PRIVATE_NETWORK_ACCESS_CHECKS);
  EXPECT_THAT(client().completion_status().cors_error_status,
              Optional(CorsErrorStatus(
                  mojom::CorsError::kPreflightMissingAllowOriginHeader,
                  network::mojom::IPAddressSpace::kPrivate,
                  network::mojom::IPAddressSpace::kUnknown)));
  EXPECT_EQ(
      client().completion_status().private_network_access_preflight_result,
      mojom::PrivateNetworkAccessPreflightResult::kError);

  EXPECT_THAT(histogram_tester.GetAllSamples(kPreflightErrorHistogramName),
              ElementsAre(MakeBucket(
                  mojom::CorsError::kPreflightMissingAllowOriginHeader, 1)));
  EXPECT_THAT(histogram_tester.GetAllSamples(kPreflightWarningHistogramName),
              IsEmpty());

  devtools_observer.WaitUntilCorsError();

  const MockDevToolsObserver::OnCorsErrorParams& error_params =
      *devtools_observer.cors_error_params();
  EXPECT_EQ(
      error_params.status,
      CorsErrorStatus(mojom::CorsError::kPreflightMissingAllowOriginHeader,
                      network::mojom::IPAddressSpace::kPrivate,
                      network::mojom::IPAddressSpace::kUnknown));
  EXPECT_FALSE(error_params.is_warning);
  ASSERT_TRUE(error_params.client_security_state);
  EXPECT_TRUE(error_params.client_security_state->is_web_secure_context);
  EXPECT_EQ(error_params.client_security_state->private_network_request_policy,
            mojom::PrivateNetworkRequestPolicy::kPreflightBlock);
  EXPECT_EQ(error_params.client_security_state->ip_address_space,
            mojom::IPAddressSpace::kPublic);
}

// This test verifies that when:
//
//  - the private network request policy is set to `kPreflightBlock`
//  - a simple request detects a private network request
//  - the following PNA preflight fails due to a missing PNA header
//
// ... the error is ignored and the request proceeds.
TEST_F(CorsURLLoaderPrivateNetworkAccessTest,
       PolicyBlockMissingAllowPrivateNetwork) {
  auto initiator_origin = url::Origin::Create(GURL("https://example.com"));

  ResetFactoryParams factory_params;
  factory_params.is_trusted = true;
  ResetFactory(initiator_origin, kRendererProcessId, factory_params);

  MockDevToolsObserver devtools_observer;

  ResourceRequest request;
  request.method = "GET";
  request.mode = mojom::RequestMode::kCors;
  request.url = GURL("https://example.com/");
  request.request_initiator = initiator_origin;
  request.trusted_params =
      RequestTrustedParamsBuilder()
          .WithClientSecurityState(
              ClientSecurityStateBuilder()
                  .WithPrivateNetworkRequestPolicy(
                      mojom::PrivateNetworkRequestPolicy::kPreflightBlock)
                  .WithIsSecureContext(true)
                  .WithIPAddressSpace(mojom::IPAddressSpace::kPublic)
                  .Build())
          .WithDevToolsObserver(devtools_observer.Bind())
          .Build();
  request.devtools_request_id = "devtools";

  base::HistogramTester histogram_tester;

  CreateLoaderAndStart(request);
  RunUntilCreateLoaderAndStartCalled();
  NotifyLoaderClientOnComplete(CorsErrorStatus(
      mojom::CorsError::kUnexpectedPrivateNetworkAccess,
      mojom::IPAddressSpace::kUnknown, mojom::IPAddressSpace::kPrivate));

  RunUntilCreateLoaderAndStartCalled();
  NotifyLoaderClientOnReceiveResponse({
      {"Access-Control-Allow-Methods", "GET"},
      {"Access-Control-Allow-Origin", "https://example.com"},
      {"Access-Control-Allow-Credentials", "true"},
  });
  RunUntilComplete();

  EXPECT_EQ(client().completion_status().error_code,
            net::ERR_BLOCKED_BY_PRIVATE_NETWORK_ACCESS_CHECKS);
  EXPECT_EQ(
      client().completion_status().private_network_access_preflight_result,
      mojom::PrivateNetworkAccessPreflightResult::kError);

  CorsErrorStatus expected_status(
      mojom::CorsError::kPreflightMissingAllowPrivateNetwork);
  expected_status.target_address_space = mojom::IPAddressSpace::kPrivate;
  EXPECT_THAT(client().completion_status().cors_error_status,
              Optional(expected_status));

  EXPECT_THAT(histogram_tester.GetAllSamples(kPreflightErrorHistogramName),
              ElementsAre(MakeBucket(
                  mojom::CorsError::kPreflightMissingAllowPrivateNetwork, 1)));
  EXPECT_THAT(histogram_tester.GetAllSamples(kPreflightWarningHistogramName),
              IsEmpty());

  devtools_observer.WaitUntilCorsError();

  const MockDevToolsObserver::OnCorsErrorParams& error_params =
      *devtools_observer.cors_error_params();
  EXPECT_EQ(error_params.status, expected_status);
  EXPECT_FALSE(error_params.is_warning);
  ASSERT_TRUE(error_params.client_security_state);
  EXPECT_TRUE(error_params.client_security_state->is_web_secure_context);
  EXPECT_EQ(error_params.client_security_state->private_network_request_policy,
            mojom::PrivateNetworkRequestPolicy::kPreflightBlock);
  EXPECT_EQ(error_params.client_security_state->ip_address_space,
            mojom::IPAddressSpace::kPublic);
}

// This test verifies that when:
//
//  - the private network request policy is set to `kPreflightWarn`
//  - a simple request detects a private network request
//  - the following PNA preflight fails due to an invalid PNA header
//
// ... the error is ignored and the request proceeds.
TEST_F(CorsURLLoaderPrivateNetworkAccessTest,
       PolicyBlockInvalidAllowPrivateNetwork) {
  auto initiator_origin = url::Origin::Create(GURL("https://example.com"));

  ResetFactoryParams factory_params;
  factory_params.is_trusted = true;
  ResetFactory(initiator_origin, kRendererProcessId, factory_params);

  MockDevToolsObserver devtools_observer;

  ResourceRequest request;
  request.method = "GET";
  request.mode = mojom::RequestMode::kCors;
  request.url = GURL("https://example.com/");
  request.request_initiator = initiator_origin;
  request.trusted_params =
      RequestTrustedParamsBuilder()
          .WithClientSecurityState(
              ClientSecurityStateBuilder()
                  .WithPrivateNetworkRequestPolicy(
                      mojom::PrivateNetworkRequestPolicy::kPreflightBlock)
                  .WithIsSecureContext(true)
                  .WithIPAddressSpace(mojom::IPAddressSpace::kPublic)
                  .Build())
          .WithDevToolsObserver(devtools_observer.Bind())
          .Build();
  request.devtools_request_id = "devtools";

  base::HistogramTester histogram_tester;

  CreateLoaderAndStart(request);
  RunUntilCreateLoaderAndStartCalled();
  NotifyLoaderClientOnComplete(CorsErrorStatus(
      mojom::CorsError::kUnexpectedPrivateNetworkAccess,
      mojom::IPAddressSpace::kUnknown, mojom::IPAddressSpace::kPrivate));

  RunUntilCreateLoaderAndStartCalled();
  NotifyLoaderClientOnReceiveResponse({
      {"Access-Control-Allow-Methods", "GET"},
      {"Access-Control-Allow-Origin", "https://example.com"},
      {"Access-Control-Allow-Credentials", "true"},
      {"Access-Control-Allow-Private-Network", "invalid-value"},
  });
  RunUntilComplete();

  EXPECT_EQ(client().completion_status().error_code,
            net::ERR_BLOCKED_BY_PRIVATE_NETWORK_ACCESS_CHECKS);
  EXPECT_EQ(
      client().completion_status().private_network_access_preflight_result,
      mojom::PrivateNetworkAccessPreflightResult::kError);

  CorsErrorStatus expected_status(
      mojom::CorsError::kPreflightInvalidAllowPrivateNetwork, "invalid-value");
  expected_status.target_address_space = mojom::IPAddressSpace::kPrivate;
  EXPECT_THAT(client().completion_status().cors_error_status,
              Optional(expected_status));

  EXPECT_THAT(histogram_tester.GetAllSamples(kPreflightErrorHistogramName),
              ElementsAre(MakeBucket(
                  mojom::CorsError::kPreflightInvalidAllowPrivateNetwork, 1)));
  EXPECT_THAT(histogram_tester.GetAllSamples(kPreflightWarningHistogramName),
              IsEmpty());

  devtools_observer.WaitUntilCorsError();

  const MockDevToolsObserver::OnCorsErrorParams& error_params =
      *devtools_observer.cors_error_params();
  EXPECT_EQ(error_params.status, expected_status);
  EXPECT_FALSE(error_params.is_warning);
  ASSERT_TRUE(error_params.client_security_state);
  EXPECT_TRUE(error_params.client_security_state->is_web_secure_context);
  EXPECT_EQ(error_params.client_security_state->private_network_request_policy,
            mojom::PrivateNetworkRequestPolicy::kPreflightBlock);
  EXPECT_EQ(error_params.client_security_state->ip_address_space,
            mojom::IPAddressSpace::kPublic);
}

// The following `PrivateNetworkAccessPolicyOn*` tests verify that the private
// network request policy can be set on the loader factory params or the request
// itself, with preference given to the factory params.

// This test verifies that when the `ResourceRequest` carries a client security
// state and the loader factory params do not, the private network request
// policy is taken from the request.
//
// This is achieved by setting the request policy to `kPreflightBlock` and
// checking that preflight results are respected.
TEST_F(CorsURLLoaderPrivateNetworkAccessTest, PolicyOnRequestOnly) {
  auto initiator_origin = url::Origin::Create(GURL("https://example.com"));

  ResetFactoryParams factory_params;
  factory_params.is_trusted = true;
  ResetFactory(initiator_origin, kRendererProcessId, factory_params);

  ResourceRequest request;
  request.method = "GET";
  request.mode = mojom::RequestMode::kCors;
  request.url = GURL("https://example.com/");
  request.request_initiator = initiator_origin;
  request.trusted_params =
      RequestTrustedParamsBuilder()
          .WithPrivateNetworkRequestPolicy(
              mojom::PrivateNetworkRequestPolicy::kPreflightBlock)
          .Build();

  CreateLoaderAndStart(request);
  RunUntilCreateLoaderAndStartCalled();
  NotifyLoaderClientOnComplete(CorsErrorStatus(
      mojom::CorsError::kUnexpectedPrivateNetworkAccess,
      mojom::IPAddressSpace::kUnknown, mojom::IPAddressSpace::kPrivate));

  RunUntilCreateLoaderAndStartCalled();
  NotifyLoaderClientOnComplete(net::ERR_INVALID_ARGUMENT);
  RunUntilComplete();

  EXPECT_EQ(client().completion_status().error_code,
            net::ERR_BLOCKED_BY_PRIVATE_NETWORK_ACCESS_CHECKS);
}

// This test verifies that when the loader factory params carry a client
// security state and the `ResourceRequest` does not, the private network
// request policy is taken from the factory params.
//
// This is achieved by setting the factory policy to `kPreflightBlock` and
// checking that preflight results are respected.
TEST_F(CorsURLLoaderPrivateNetworkAccessTest, PolicyOnFactoryOnly) {
  auto initiator_origin = url::Origin::Create(GURL("https://example.com"));

  ResetFactoryParams factory_params;
  factory_params.client_security_state =
      ClientSecurityStateBuilder()
          .WithPrivateNetworkRequestPolicy(
              mojom::PrivateNetworkRequestPolicy::kPreflightBlock)
          .Build();
  ResetFactory(initiator_origin, kRendererProcessId, factory_params);

  ResourceRequest request;
  request.method = "GET";
  request.mode = mojom::RequestMode::kCors;
  request.url = GURL("https://example.com/");
  request.request_initiator = initiator_origin;

  CreateLoaderAndStart(request);
  RunUntilCreateLoaderAndStartCalled();
  NotifyLoaderClientOnComplete(CorsErrorStatus(
      mojom::CorsError::kUnexpectedPrivateNetworkAccess,
      mojom::IPAddressSpace::kUnknown, mojom::IPAddressSpace::kPrivate));

  RunUntilCreateLoaderAndStartCalled();
  NotifyLoaderClientOnComplete(net::ERR_INVALID_ARGUMENT);
  RunUntilComplete();

  EXPECT_EQ(client().completion_status().error_code,
            net::ERR_BLOCKED_BY_PRIVATE_NETWORK_ACCESS_CHECKS);
}

// This test verifies that when both the `ResourceRequest`  and the loader
// factory params carry a client security state, the private network request
// policy is taken from the factory.
//
// This is achieved by setting the factory policy to `kPreflightBlock`,
// the request policy to `kPreflightWarn, and checking that preflight results
// are respected.
TEST_F(CorsURLLoaderPrivateNetworkAccessTest, PolicyOnFactoryAndRequest) {
  auto initiator_origin = url::Origin::Create(GURL("https://example.com"));

  ResetFactoryParams factory_params;
  factory_params.is_trusted = true;
  factory_params.client_security_state =
      ClientSecurityStateBuilder()
          .WithPrivateNetworkRequestPolicy(
              mojom::PrivateNetworkRequestPolicy::kPreflightBlock)
          .Build();
  ResetFactory(initiator_origin, kRendererProcessId, factory_params);

  ResourceRequest request;
  request.method = "GET";
  request.mode = mojom::RequestMode::kCors;
  request.url = GURL("https://example.com/");
  request.request_initiator = initiator_origin;
  request.trusted_params =
      RequestTrustedParamsBuilder()
          .WithPrivateNetworkRequestPolicy(
              mojom::PrivateNetworkRequestPolicy::kPreflightWarn)
          .Build();

  CreateLoaderAndStart(request);
  RunUntilCreateLoaderAndStartCalled();
  NotifyLoaderClientOnComplete(CorsErrorStatus(
      mojom::CorsError::kUnexpectedPrivateNetworkAccess,
      mojom::IPAddressSpace::kUnknown, mojom::IPAddressSpace::kPrivate));

  RunUntilCreateLoaderAndStartCalled();
  NotifyLoaderClientOnComplete(net::ERR_INVALID_ARGUMENT);
  RunUntilComplete();

  EXPECT_EQ(client().completion_status().error_code,
            net::ERR_BLOCKED_BY_PRIVATE_NETWORK_ACCESS_CHECKS);
}

TEST_F(CorsURLLoaderPrivateNetworkAccessTest,
       NoPermissionPromptForNonPnaPreflights) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kPrivateNetworkAccessPermissionPrompt);

  auto initiator = url::Origin::Create(GURL("https://foo.example"));
  ResetFactoryParams factory_params;
  factory_params.is_trusted = true;
  factory_params.client_security_state = mojom::ClientSecurityState::New();
  factory_params.client_security_state->is_web_secure_context = true;
  factory_params.url_loader_network_observer =
      TestURLLoaderNetworkObserver().Bind();
  ResetFactory(initiator, kRendererProcessId, factory_params);

  ResourceRequest request;
  request.mode = mojom::RequestMode::kCorsWithForcedPreflight;
  request.required_ip_address_space = mojom::IPAddressSpace::kPrivate;
  request.target_ip_address_space = mojom::IPAddressSpace::kUnknown;
  request.url = GURL("http://example.com/");
  request.request_initiator = initiator;
  request.trusted_params =
      RequestTrustedParamsBuilder()
          .WithClientSecurityState(
              ClientSecurityStateBuilder()
                  .WithPrivateNetworkRequestPolicy(
                      mojom::PrivateNetworkRequestPolicy::kPreflightBlock)
                  .WithIsSecureContext(false)
                  .WithIPAddressSpace(mojom::IPAddressSpace::kUnknown)
                  .Build())
          .Build();

  CreateLoaderAndStart(request);
  RunUntilCreateLoaderAndStartCalled();

  EXPECT_EQ(client().completion_status().error_code, net::OK);
}

}  // namespace
}  // namespace network::cors
