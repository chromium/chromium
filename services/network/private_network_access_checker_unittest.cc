// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/private_network_access_checker.h"

#include <string_view>

#include "base/strings/strcat.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/base/transport_info.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/client_security_state.mojom-shared.h"
#include "services/network/public/mojom/client_security_state.mojom.h"
#include "services/network/public/mojom/cors.mojom.h"
#include "services/network/public/mojom/ip_address_space.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using ::testing::Optional;

namespace network {
namespace {

using Result = PrivateNetworkAccessCheckResult;

constexpr std::string_view kCheckResultHistogramName =
    "Security.PrivateNetworkAccess.CheckResult";

constexpr char kNoAcceptChFrame[] = "";

// For better readability than literal `nullptr`.
constexpr mojom::ClientSecurityState* kNullClientSecurityState = nullptr;

net::IPEndPoint LocalEndpoint() {
  return net::IPEndPoint(net::IPAddress::IPv4Localhost(), 80);
}

net::IPEndPoint PrivateEndpoint() {
  return net::IPEndPoint(net::IPAddress(10, 0, 0, 1), 80);
}

GURL PrivateEndpointUrl() {
  return GURL("http://10.0.0.1");
}

net::IPEndPoint PublicEndpoint() {
  return net::IPEndPoint(net::IPAddress(1, 2, 3, 4), 80);
}

net::TransportInfo DirectTransport(const net::IPEndPoint& endpoint) {
  return net::TransportInfo(
      net::TransportType::kDirect, endpoint, kNoAcceptChFrame,
      /*cert_is_issued_by_known_root=*/false, net::kProtoUnknown);
}

net::TransportInfo ProxiedTransport(const net::IPEndPoint& endpoint) {
  return net::TransportInfo(
      net::TransportType::kProxied, endpoint, kNoAcceptChFrame,
      /*cert_is_issued_by_known_root=*/false, net::kProtoUnknown);
}

net::TransportInfo CachedTransport(const net::IPEndPoint& endpoint) {
  return net::TransportInfo(
      net::TransportType::kCached, endpoint, kNoAcceptChFrame,
      /*cert_is_issued_by_known_root=*/false, net::kProtoUnknown);
}

net::TransportInfo MakeTransport(net::TransportType type,
                                 const net::IPEndPoint& endpoint) {
  return net::TransportInfo(type, endpoint, kNoAcceptChFrame,
                            /*cert_is_issued_by_known_root=*/false,
                            net::kProtoUnknown);
}

TEST(PrivateNetworkAccessCheckerTest, ClientSecurityStateNull) {
  PrivateNetworkAccessChecker checker(
      ResourceRequest(), kNullClientSecurityState, mojom::kURLLoadOptionNone);

  EXPECT_EQ(checker.client_security_state(), nullptr);
  EXPECT_EQ(checker.ClientAddressSpace(), mojom::IPAddressSpace::kUnknown);
}

TEST(PrivateNetworkAccessCheckerTest, ClientSecurityStateFromFactory) {
  mojom::ClientSecurityState client_security_state;
  client_security_state.ip_address_space = mojom::IPAddressSpace::kPublic;

  PrivateNetworkAccessChecker checker(ResourceRequest(), &client_security_state,
                                      mojom::kURLLoadOptionNone);

  EXPECT_EQ(checker.client_security_state(), &client_security_state);
  EXPECT_EQ(checker.ClientAddressSpace(), mojom::IPAddressSpace::kPublic);
}

TEST(PrivateNetworkAccessCheckerTest, ClientSecurityStateFromRequest) {
  ResourceRequest request;
  request.trusted_params.emplace();
  request.trusted_params->client_security_state =
      mojom::ClientSecurityState::New();
  request.trusted_params->client_security_state->ip_address_space =
      mojom::IPAddressSpace::kPrivate;

  PrivateNetworkAccessChecker checker(request, kNullClientSecurityState,
                                      mojom::kURLLoadOptionNone);

  EXPECT_NE(checker.client_security_state(), nullptr);
  EXPECT_EQ(checker.ClientAddressSpace(), mojom::IPAddressSpace::kPrivate);
}

TEST(PrivateNetworkAccessCheckerTest, CheckLoadOptionUnknown) {
  base::HistogramTester histogram_tester;

  PrivateNetworkAccessChecker checker(ResourceRequest(),
                                      kNullClientSecurityState,
                                      mojom::kURLLoadOptionBlockLocalRequest);

  EXPECT_EQ(checker.Check(DirectTransport(net::IPEndPoint())),
            Result::kAllowedMissingClientSecurityState);
}

TEST(PrivateNetworkAccessCheckerTest, CheckLoadOptionPublic) {
  base::HistogramTester histogram_tester;

  PrivateNetworkAccessChecker checker(ResourceRequest(),
                                      kNullClientSecurityState,
                                      mojom::kURLLoadOptionBlockLocalRequest);

  EXPECT_EQ(checker.Check(DirectTransport(PublicEndpoint())),
            Result::kAllowedMissingClientSecurityState);
}

TEST(PrivateNetworkAccessCheckerTest, CheckLoadOptionPrivate) {
  base::HistogramTester histogram_tester;

  PrivateNetworkAccessChecker checker(ResourceRequest(),
                                      kNullClientSecurityState,
                                      mojom::kURLLoadOptionBlockLocalRequest);

  EXPECT_EQ(checker.Check(DirectTransport(PrivateEndpoint())),
            Result::kBlockedByLoadOption);

  histogram_tester.ExpectUniqueSample(kCheckResultHistogramName,
                                      Result::kBlockedByLoadOption, 1);
}

TEST(PrivateNetworkAccessCheckerTest, CheckLoadOptionLocal) {
  base::HistogramTester histogram_tester;

  PrivateNetworkAccessChecker checker(ResourceRequest(),
                                      kNullClientSecurityState,
                                      mojom::kURLLoadOptionBlockLocalRequest);

  EXPECT_EQ(checker.Check(DirectTransport(LocalEndpoint())),
            Result::kBlockedByLoadOption);

  histogram_tester.ExpectUniqueSample(kCheckResultHistogramName,
                                      Result::kBlockedByLoadOption, 1);
}

TEST(PrivateNetworkAccessCheckerTest,
     CheckAllowedPotentiallyTrustworthySameOrigin) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kLocalNetworkAccessAllowPotentiallyTrustworthySameOrigin);

  base::HistogramTester histogram_tester;

  ResourceRequest request;
  request.url = GURL("https://a.com/subresource");
  request.request_initiator = url::Origin::Create(request.url);

  mojom::ClientSecurityState client_security_state;
  client_security_state.ip_address_space = mojom::IPAddressSpace::kPublic;
  client_security_state.private_network_request_policy =
      mojom::PrivateNetworkRequestPolicy::kPreflightBlock;

  PrivateNetworkAccessChecker checker(request, &client_security_state,
                                      mojom::kURLLoadOptionNone);

  EXPECT_EQ(checker.Check(DirectTransport(LocalEndpoint())),
            Result::kAllowedPotentiallyTrustworthySameOrigin);

  histogram_tester.ExpectUniqueSample(
      kCheckResultHistogramName,
      Result::kAllowedPotentiallyTrustworthySameOrigin, 1);
}

TEST(PrivateNetworkAccessCheckerTest,
     CheckDisallowedPotentiallyTrustworthyCrossOrigin) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kLocalNetworkAccessAllowPotentiallyTrustworthySameOrigin);

  base::HistogramTester histogram_tester;

  ResourceRequest request;
  request.url = GURL("https://example.com/subresource");
  request.request_initiator =
      url::Origin::Create(GURL("https://subdomain.example.com"));

  mojom::ClientSecurityState client_security_state;
  client_security_state.ip_address_space = mojom::IPAddressSpace::kPublic;
  client_security_state.private_network_request_policy =
      mojom::PrivateNetworkRequestPolicy::kPreflightBlock;

  PrivateNetworkAccessChecker checker(request, &client_security_state,
                                      mojom::kURLLoadOptionNone);

  EXPECT_EQ(checker.Check(DirectTransport(LocalEndpoint())),
            Result::kBlockedByPolicyPreflightBlock);

  histogram_tester.ExpectUniqueSample(
      kCheckResultHistogramName, Result::kBlockedByPolicyPreflightBlock, 1);
}

TEST(PrivateNetworkAccessCheckerTest, CheckDisallowedUntrustworthySameOrigin) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kLocalNetworkAccessAllowPotentiallyTrustworthySameOrigin);

  base::HistogramTester histogram_tester;

  ResourceRequest request;
  request.url = GURL("http://example.com/subresource");
  request.request_initiator = url::Origin::Create(request.url);

  mojom::ClientSecurityState client_security_state;
  client_security_state.ip_address_space = mojom::IPAddressSpace::kPublic;
  client_security_state.private_network_request_policy =
      mojom::PrivateNetworkRequestPolicy::kBlock;

  PrivateNetworkAccessChecker checker(request, &client_security_state,
                                      mojom::kURLLoadOptionNone);

  EXPECT_EQ(checker.Check(DirectTransport(LocalEndpoint())),
            Result::kBlockedByPolicyBlock);

  histogram_tester.ExpectUniqueSample(kCheckResultHistogramName,
                                      Result::kBlockedByPolicyBlock, 1);
}

TEST(PrivateNetworkAccessCheckerTest,
     CheckDisallowedPotentiallyTrustworthyCrossOriginAfterResetForRedirect) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kLocalNetworkAccessAllowPotentiallyTrustworthySameOrigin);

  base::HistogramTester histogram_tester;

  ResourceRequest request;
  request.url = GURL("https://example.com/subresource");
  request.request_initiator = url::Origin::Create(request.url);

  mojom::ClientSecurityState client_security_state;
  client_security_state.ip_address_space = mojom::IPAddressSpace::kPublic;
  client_security_state.private_network_request_policy =
      mojom::PrivateNetworkRequestPolicy::kPreflightBlock;

  PrivateNetworkAccessChecker checker(request, &client_security_state,
                                      mojom::kURLLoadOptionNone);
  checker.ResetForRedirect(GURL("https://subdomain.example.com/subresource"));

  EXPECT_EQ(checker.Check(DirectTransport(LocalEndpoint())),
            Result::kBlockedByPolicyPreflightBlock);

  histogram_tester.ExpectUniqueSample(
      kCheckResultHistogramName, Result::kBlockedByPolicyPreflightBlock, 1);
}

TEST(PrivateNetworkAccessCheckerTest,
     CheckAllowedPotentiallyTrustworthySameOriginAfterResetForRedirect) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kLocalNetworkAccessAllowPotentiallyTrustworthySameOrigin);

  base::HistogramTester histogram_tester;

  ResourceRequest request;
  request.url = GURL("https://example.com/subresource");
  request.request_initiator =
      url::Origin::Create(GURL("https://subdomain.example.com"));

  mojom::ClientSecurityState client_security_state;
  client_security_state.ip_address_space = mojom::IPAddressSpace::kPublic;
  client_security_state.private_network_request_policy =
      mojom::PrivateNetworkRequestPolicy::kPreflightBlock;

  PrivateNetworkAccessChecker checker(request, &client_security_state,
                                      mojom::kURLLoadOptionNone);
  checker.ResetForRedirect(GURL("https://subdomain.example.com/subresource"));

  EXPECT_EQ(checker.Check(DirectTransport(LocalEndpoint())),
            Result::kAllowedPotentiallyTrustworthySameOrigin);

  histogram_tester.ExpectUniqueSample(
      kCheckResultHistogramName,
      Result::kAllowedPotentiallyTrustworthySameOrigin, 1);
}

TEST(PrivateNetworkAccessCheckerTest, CheckAllowedMissingClientSecurityState) {
  base::HistogramTester histogram_tester;

  PrivateNetworkAccessChecker checker(
      ResourceRequest(), kNullClientSecurityState, mojom::kURLLoadOptionNone);

  EXPECT_EQ(checker.Check(DirectTransport(LocalEndpoint())),
            Result::kAllowedMissingClientSecurityState);

  histogram_tester.ExpectUniqueSample(
      kCheckResultHistogramName, Result::kAllowedMissingClientSecurityState, 1);
}

TEST(PrivateNetworkAccessCheckerTest,
     CheckAllowedMissingClientSecurityStateInconsistentIpAddressSpace) {
  base::HistogramTester histogram_tester;

  PrivateNetworkAccessChecker checker(
      ResourceRequest(), kNullClientSecurityState, mojom::kURLLoadOptionNone);

  EXPECT_EQ(checker.Check(DirectTransport(LocalEndpoint())),
            Result::kAllowedMissingClientSecurityState);

  // Even though this is inconsistent with the previous IP address space, the
  // inconsistency is ignored because of the missing client security state.
  //
  // This does not risk triggering https://crbug.com/1279376, because no check
  // with this checker will ever return `kBlocked*`.
  EXPECT_EQ(checker.Check(DirectTransport(PublicEndpoint())),
            Result::kAllowedMissingClientSecurityState);
}

TEST(PrivateNetworkAccessCheckerTest, CheckAllowedNoLessPublic) {
  base::HistogramTester histogram_tester;

  mojom::ClientSecurityState client_security_state;
  client_security_state.ip_address_space = mojom::IPAddressSpace::kPrivate;
  client_security_state.private_network_request_policy =
      mojom::PrivateNetworkRequestPolicy::kBlock;

  PrivateNetworkAccessChecker checker(ResourceRequest(), &client_security_state,
                                      mojom::kURLLoadOptionNone);

  EXPECT_EQ(checker.Check(DirectTransport(PrivateEndpoint())),
            Result::kAllowedNoLessPublic);

  histogram_tester.ExpectUniqueSample(kCheckResultHistogramName,
                                      Result::kAllowedNoLessPublic, 1);
}

TEST(PrivateNetworkAccessCheckerTest, CheckAllowedByPolicyAllow) {
  base::HistogramTester histogram_tester;

  mojom::ClientSecurityState client_security_state;
  client_security_state.ip_address_space = mojom::IPAddressSpace::kPublic;
  client_security_state.private_network_request_policy =
      mojom::PrivateNetworkRequestPolicy::kAllow;

  PrivateNetworkAccessChecker checker(ResourceRequest(), &client_security_state,
                                      mojom::kURLLoadOptionNone);

  EXPECT_EQ(checker.Check(DirectTransport(PrivateEndpoint())),
            Result::kAllowedByPolicyAllow);

  histogram_tester.ExpectUniqueSample(kCheckResultHistogramName,
                                      Result::kAllowedByPolicyAllow, 1);
}

TEST(PrivateNetworkAccessCheckerTest,
     CheckAllowedByPolicyWarnInconsistentIpAddressSpace) {
  base::HistogramTester histogram_tester;

  mojom::ClientSecurityState client_security_state;
  client_security_state.ip_address_space = mojom::IPAddressSpace::kPublic;
  client_security_state.private_network_request_policy =
      mojom::PrivateNetworkRequestPolicy::kWarn;

  PrivateNetworkAccessChecker checker(ResourceRequest(), &client_security_state,
                                      mojom::kURLLoadOptionNone);

  EXPECT_EQ(checker.Check(DirectTransport(PrivateEndpoint())),
            Result::kAllowedByPolicyWarn);

  // Even though this is inconsistent with the previous IP address space, the
  // inconsistency is ignored because of the `kWarn` policy.
  EXPECT_EQ(checker.Check(DirectTransport(PublicEndpoint())),
            Result::kAllowedByPolicyWarn);
}

TEST(PrivateNetworkAccessCheckerTest,
     CheckAllowedByPolicyAllowInconsistentIpAddressSpace) {
  base::HistogramTester histogram_tester;

  mojom::ClientSecurityState client_security_state;
  client_security_state.ip_address_space = mojom::IPAddressSpace::kPublic;
  client_security_state.private_network_request_policy =
      mojom::PrivateNetworkRequestPolicy::kAllow;

  PrivateNetworkAccessChecker checker(ResourceRequest(), &client_security_state,
                                      mojom::kURLLoadOptionNone);

  EXPECT_EQ(checker.Check(DirectTransport(PrivateEndpoint())),
            Result::kAllowedByPolicyAllow);

  // Even though this is inconsistent with the previous IP address space, the
  // inconsistency is ignored because of the `kAllow` policy.
  //
  // This does not risk triggering https://crbug.com/1279376, because no check
  // with this checker will ever return `kBlocked*`.
  EXPECT_EQ(checker.Check(DirectTransport(PublicEndpoint())),
            Result::kAllowedByPolicyAllow);
}

TEST(PrivateNetworkAccessCheckerTest, CheckAllowedByPolicyWarn) {
  base::HistogramTester histogram_tester;

  mojom::ClientSecurityState client_security_state;
  client_security_state.ip_address_space = mojom::IPAddressSpace::kPublic;
  client_security_state.private_network_request_policy =
      mojom::PrivateNetworkRequestPolicy::kWarn;

  PrivateNetworkAccessChecker checker(ResourceRequest(), &client_security_state,
                                      mojom::kURLLoadOptionNone);

  EXPECT_EQ(checker.Check(DirectTransport(PrivateEndpoint())),
            Result::kAllowedByPolicyWarn);

  histogram_tester.ExpectUniqueSample(kCheckResultHistogramName,
                                      Result::kAllowedByPolicyWarn, 1);
}

TEST(PrivateNetworkAccessCheckerTest, CheckBlockedByPolicyBlock) {
  base::HistogramTester histogram_tester;

  mojom::ClientSecurityState client_security_state;
  client_security_state.ip_address_space = mojom::IPAddressSpace::kPublic;
  client_security_state.private_network_request_policy =
      mojom::PrivateNetworkRequestPolicy::kBlock;

  PrivateNetworkAccessChecker checker(ResourceRequest(), &client_security_state,
                                      mojom::kURLLoadOptionNone);

  EXPECT_EQ(checker.Check(DirectTransport(PrivateEndpoint())),
            Result::kBlockedByPolicyBlock);

  histogram_tester.ExpectUniqueSample(kCheckResultHistogramName,
                                      Result::kBlockedByPolicyBlock, 1);
}

TEST(PrivateNetworkAccessCheckerTest, CheckBlockedByPolicyPreflightWarn) {
  base::HistogramTester histogram_tester;

  mojom::ClientSecurityState client_security_state;
  client_security_state.ip_address_space = mojom::IPAddressSpace::kPublic;
  client_security_state.private_network_request_policy =
      mojom::PrivateNetworkRequestPolicy::kPreflightWarn;

  PrivateNetworkAccessChecker checker(ResourceRequest(), &client_security_state,
                                      mojom::kURLLoadOptionNone);

  EXPECT_EQ(checker.Check(DirectTransport(PrivateEndpoint())),
            Result::kBlockedByPolicyPreflightWarn);

  histogram_tester.ExpectUniqueSample(kCheckResultHistogramName,
                                      Result::kBlockedByPolicyPreflightWarn, 1);
}

TEST(PrivateNetworkAccessCheckerTest, CheckBlockedByPolicyPreflightBlock) {
  base::HistogramTester histogram_tester;

  mojom::ClientSecurityState client_security_state;
  client_security_state.ip_address_space = mojom::IPAddressSpace::kPublic;
  client_security_state.private_network_request_policy =
      mojom::PrivateNetworkRequestPolicy::kPreflightBlock;

  PrivateNetworkAccessChecker checker(ResourceRequest(), &client_security_state,
                                      mojom::kURLLoadOptionNone);

  EXPECT_EQ(checker.Check(DirectTransport(PrivateEndpoint())),
            Result::kBlockedByPolicyPreflightBlock);

  histogram_tester.ExpectUniqueSample(
      kCheckResultHistogramName, Result::kBlockedByPolicyPreflightBlock, 1);
}

TEST(PrivateNetworkAccessCheckerTest, CheckBlockedByTargetIpAddressSpace) {
  base::HistogramTester histogram_tester;

  mojom::ClientSecurityState client_security_state;
  client_security_state.ip_address_space = mojom::IPAddressSpace::kPublic;
  client_security_state.private_network_request_policy =
      mojom::PrivateNetworkRequestPolicy::kPreflightBlock;

  ResourceRequest request;
  request.target_ip_address_space = mojom::IPAddressSpace::kPublic;

  PrivateNetworkAccessChecker checker(request, &client_security_state,
                                      mojom::kURLLoadOptionNone);

  EXPECT_EQ(checker.Check(DirectTransport(PrivateEndpoint())),
            Result::kBlockedByTargetIpAddressSpace);

  histogram_tester.ExpectUniqueSample(
      kCheckResultHistogramName, Result::kBlockedByTargetIpAddressSpace, 1);
}

TEST(PrivateNetworkAccessCheckerTest, CheckAllowedByPolicyPreflightWarn) {
  base::HistogramTester histogram_tester;

  mojom::ClientSecurityState client_security_state;
  client_security_state.ip_address_space = mojom::IPAddressSpace::kPublic;
  client_security_state.private_network_request_policy =
      mojom::PrivateNetworkRequestPolicy::kPreflightWarn;

  ResourceRequest request;
  request.target_ip_address_space = mojom::IPAddressSpace::kLocal;

  PrivateNetworkAccessChecker checker(request, &client_security_state,
                                      mojom::kURLLoadOptionNone);

  EXPECT_EQ(checker.Check(DirectTransport(PrivateEndpoint())),
            Result::kAllowedByPolicyPreflightWarn);

  histogram_tester.ExpectUniqueSample(kCheckResultHistogramName,
                                      Result::kAllowedByPolicyPreflightWarn, 1);
}

TEST(PrivateNetworkAccessCheckerTest, CheckAllowedByTargetIpAddressSpace) {
  base::HistogramTester histogram_tester;

  mojom::ClientSecurityState client_security_state;
  client_security_state.ip_address_space = mojom::IPAddressSpace::kPublic;
  client_security_state.private_network_request_policy =
      mojom::PrivateNetworkRequestPolicy::kPreflightBlock;

  ResourceRequest request;
  request.target_ip_address_space = mojom::IPAddressSpace::kPrivate;

  PrivateNetworkAccessChecker checker(request, &client_security_state,
                                      mojom::kURLLoadOptionNone);

  EXPECT_EQ(checker.Check(DirectTransport(PrivateEndpoint())),
            Result::kAllowedByTargetIpAddressSpace);

  histogram_tester.ExpectUniqueSample(
      kCheckResultHistogramName, Result::kAllowedByTargetIpAddressSpace, 1);
}

TEST(PrivateNetworkAccessCheckerTest,
     CheckAllowedByPolicyPreflightWarnInconsistent) {
  mojom::ClientSecurityState client_security_state;
  client_security_state.ip_address_space = mojom::IPAddressSpace::kPublic;
  client_security_state.private_network_request_policy =
      mojom::PrivateNetworkRequestPolicy::kPreflightWarn;

  PrivateNetworkAccessChecker checker(ResourceRequest(), &client_security_state,
                                      mojom::kURLLoadOptionNone);

  checker.Check(DirectTransport(PublicEndpoint()));

  base::HistogramTester histogram_tester;

  EXPECT_EQ(checker.Check(DirectTransport(PrivateEndpoint())),
            Result::kAllowedByPolicyPreflightWarn);

  histogram_tester.ExpectUniqueSample(kCheckResultHistogramName,
                                      Result::kAllowedByPolicyPreflightWarn, 1);
}

TEST(PrivateNetworkAccessCheckerTest,
     CheckBlockedByInconsistentIpAddressSpace) {
  mojom::ClientSecurityState client_security_state;
  client_security_state.ip_address_space = mojom::IPAddressSpace::kPrivate;
  client_security_state.private_network_request_policy =
      mojom::PrivateNetworkRequestPolicy::kPreflightBlock;

  PrivateNetworkAccessChecker checker(ResourceRequest(), &client_security_state,
                                      mojom::kURLLoadOptionNone);

  EXPECT_EQ(checker.Check(DirectTransport(PublicEndpoint())),
            Result::kAllowedNoLessPublic);

  base::HistogramTester histogram_tester;

  EXPECT_EQ(checker.Check(DirectTransport(PrivateEndpoint())),
            Result::kBlockedByInconsistentIpAddressSpace);

  histogram_tester.ExpectUniqueSample(
      kCheckResultHistogramName, Result::kBlockedByInconsistentIpAddressSpace,
      1);
}

TEST(PrivateNetworkAccessCheckerTest,
     CheckBlockedByUnmatchedRequiredAddressSpaceAndResourceAddressSpace) {
  mojom::ClientSecurityState client_security_state;
  client_security_state.ip_address_space = mojom::IPAddressSpace::kPublic;
  client_security_state.private_network_request_policy =
      mojom::PrivateNetworkRequestPolicy::kPreflightBlock;

  ResourceRequest request;
  request.target_ip_address_space = mojom::IPAddressSpace::kUnknown;
  request.required_ip_address_space = mojom::IPAddressSpace::kPrivate;
  PrivateNetworkAccessChecker checker(request, &client_security_state,
                                      mojom::kURLLoadOptionNone);

  EXPECT_EQ(checker.Check(DirectTransport(PublicEndpoint())),
            Result::kBlockedByTargetIpAddressSpace);
}

TEST(PrivateNetworkAccessCheckerTest, ResponseAddressSpace) {
  PrivateNetworkAccessChecker checker(
      ResourceRequest(), kNullClientSecurityState, mojom::kURLLoadOptionNone);

  EXPECT_EQ(checker.ResponseAddressSpace(), std::nullopt);

  checker.Check(DirectTransport(PublicEndpoint()));

  EXPECT_THAT(checker.ResponseAddressSpace(),
              Optional(mojom::IPAddressSpace::kPublic));

  checker.Check(DirectTransport(PrivateEndpoint()));

  EXPECT_THAT(checker.ResponseAddressSpace(),
              Optional(mojom::IPAddressSpace::kPrivate));
}

TEST(PrivateNetworkAccessCheckerTest, ProxiedTransportAddressSpaceIsUnknown) {
  PrivateNetworkAccessChecker checker(ResourceRequest(),
                                      kNullClientSecurityState,
                                      mojom::kURLLoadOptionBlockLocalRequest);

  // This succeeds in spite of the load option, because the proxied transport
  // is not considered any less public than `kPublic`.
  EXPECT_EQ(checker.Check(ProxiedTransport(LocalEndpoint())),
            Result::kAllowedMissingClientSecurityState);

  // In fact, it is considered unknown.
  EXPECT_EQ(checker.ResponseAddressSpace(), mojom::IPAddressSpace::kUnknown);
}

TEST(PrivateNetworkAccessCheckerTest,
     CachedFromProxyTransportAddressSpaceIsUnknown) {
  PrivateNetworkAccessChecker checker(ResourceRequest(),
                                      kNullClientSecurityState,
                                      mojom::kURLLoadOptionBlockLocalRequest);

  // This succeeds in spite of the load option, because the cached-from-proxy
  // transport is not considered any less public than `kPublic`.
  EXPECT_EQ(checker.Check(MakeTransport(net::TransportType::kCachedFromProxy,
                                        LocalEndpoint())),
            Result::kAllowedMissingClientSecurityState);

  // In fact, it is considered unknown.
  EXPECT_EQ(checker.ResponseAddressSpace(), mojom::IPAddressSpace::kUnknown);
}

TEST(PrivateNetworkAccessCheckerTest, CachedTransportAddressSpace) {
  PrivateNetworkAccessChecker checker(ResourceRequest(),
                                      kNullClientSecurityState,
                                      mojom::kURLLoadOptionBlockLocalRequest);

  // The cached transport is treated like a direct transport to the same
  // endpoint, so the load option does not fail the check.
  EXPECT_EQ(checker.Check(CachedTransport(PublicEndpoint())),
            Result::kAllowedMissingClientSecurityState);

  EXPECT_EQ(checker.ResponseAddressSpace(), mojom::IPAddressSpace::kPublic);

  // When the endpoint is local, the check fails as for a direct transport.
  EXPECT_EQ(checker.Check(CachedTransport(LocalEndpoint())),
            Result::kBlockedByLoadOption);

  EXPECT_EQ(checker.ResponseAddressSpace(), mojom::IPAddressSpace::kLocal);
}

TEST(PrivateNetworkAccessCheckerTest, ResetTargetAddressSpace) {
  mojom::ClientSecurityState client_security_state;
  client_security_state.ip_address_space = mojom::IPAddressSpace::kPrivate;
  client_security_state.private_network_request_policy =
      mojom::PrivateNetworkRequestPolicy::kPreflightBlock;

  ResourceRequest request;
  request.target_ip_address_space = mojom::IPAddressSpace::kPublic;

  PrivateNetworkAccessChecker checker(request, &client_security_state,
                                      mojom::kURLLoadOptionNone);

  checker.ResetForRedirect(GURL("http://foo.com"));

  // The target address space has been cleared.
  EXPECT_EQ(checker.TargetAddressSpace(), mojom::IPAddressSpace::kUnknown);

  // This succeeds even though the IP address space does not match the target
  // passed at construction time, thanks to `ResetForRedirect()`.
  EXPECT_EQ(checker.Check(DirectTransport(PrivateEndpoint())),
            Result::kAllowedNoLessPublic);
}

TEST(PrivateNetworkAccessCheckerTest, ResetResponseAddressSpace) {
  PrivateNetworkAccessChecker checker(
      ResourceRequest(), kNullClientSecurityState, mojom::kURLLoadOptionNone);

  checker.Check(DirectTransport(PrivateEndpoint()));

  checker.ResetForRedirect(GURL("http://foo.com"));

  EXPECT_EQ(checker.ResponseAddressSpace(), std::nullopt);

  // This succeeds even though the IP address space does not match that of the
  // previous endpoint passed to `Check()`, thanks to `ResetForRedirect()`.
  EXPECT_EQ(checker.Check(DirectTransport(LocalEndpoint())),
            Result::kAllowedMissingClientSecurityState);
}

TEST(PrivateNetworkAccessCheckerTest,
     DoesNotRecordPrivateIpInferrableHistogramForPublicEndpoint) {
  base::HistogramTester histogram_tester;

  ResourceRequest request;
  request.url = GURL("http://192.168.1.1");
  PrivateNetworkAccessChecker checker(request, kNullClientSecurityState,
                                      mojom::kURLLoadOptionNone);

  checker.Check(DirectTransport(PublicEndpoint()));

  histogram_tester.ExpectTotalCount(
      "Security.PrivateNetworkAccess.PrivateIpInferrable", 0);
}

TEST(PrivateNetworkAccessCheckerTest,
     DoesNotRecordPrivateIpInferrableHistogramForHttpsScheme) {
  base::HistogramTester histogram_tester;

  ResourceRequest request;
  request.url = GURL("https://192.168.1.1");
  PrivateNetworkAccessChecker checker(request, kNullClientSecurityState,
                                      mojom::kURLLoadOptionNone);

  checker.Check(DirectTransport(PrivateEndpoint()));

  histogram_tester.ExpectTotalCount(
      "Security.PrivateNetworkAccess.PrivateIpInferrable", 0);
}

TEST(PrivateNetworkAccessCheckerTest,
     DoesNotRecordPrivateIpInferrableHistogramWithTargetIpAddressSpace) {
  mojom::ClientSecurityState client_security_state;
  client_security_state.ip_address_space = mojom::IPAddressSpace::kPublic;
  client_security_state.private_network_request_policy =
      mojom::PrivateNetworkRequestPolicy::kPreflightWarn;

  base::HistogramTester histogram_tester;

  ResourceRequest request;
  request.url = GURL("http://192.168.1.1");
  request.target_ip_address_space = mojom::IPAddressSpace::kPrivate;

  PrivateNetworkAccessChecker checker(request, &client_security_state,
                                      mojom::kURLLoadOptionNone);

  checker.Check(DirectTransport(PrivateEndpoint()));

  histogram_tester.ExpectTotalCount(
      "Security.PrivateNetworkAccess.PrivateIpInferrable", 0);
}

TEST(PrivateNetworkAccessCheckerTest,
     RecordsPrivateIpInferrableHistogramPolicyAllow) {
  mojom::ClientSecurityState client_security_state;
  client_security_state.ip_address_space = mojom::IPAddressSpace::kPublic;
  client_security_state.private_network_request_policy =
      mojom::PrivateNetworkRequestPolicy::kAllow;

  base::HistogramTester histogram_tester;

  ResourceRequest request;
  request.url = GURL("http://192.168.1.1");
  PrivateNetworkAccessChecker checker(request, &client_security_state,
                                      mojom::kURLLoadOptionNone);

  checker.Check(DirectTransport(PrivateEndpoint()));

  histogram_tester.ExpectUniqueSample(
      "Security.PrivateNetworkAccess.PrivateIpInferrable", true, 1);
}

TEST(PrivateNetworkAccessCheckerTest,
     RecordsPrivateIpInferrableHistogramNoLessPublic) {
  mojom::ClientSecurityState client_security_state;
  client_security_state.ip_address_space = mojom::IPAddressSpace::kPrivate;
  client_security_state.private_network_request_policy =
      mojom::PrivateNetworkRequestPolicy::kBlock;

  base::HistogramTester histogram_tester;

  ResourceRequest request;
  request.url = GURL("http://192.168.1.1");
  PrivateNetworkAccessChecker checker(request, &client_security_state,
                                      mojom::kURLLoadOptionNone);

  checker.Check(DirectTransport(PrivateEndpoint()));

  histogram_tester.ExpectUniqueSample(
      "Security.PrivateNetworkAccess.PrivateIpInferrable", true, 1);
}

TEST(PrivateNetworkAccessCheckerTest, RecordsPrivateIpInferrableHistogramTrue) {
  mojom::ClientSecurityState client_security_state;
  client_security_state.ip_address_space = mojom::IPAddressSpace::kPublic;
  client_security_state.private_network_request_policy =
      mojom::PrivateNetworkRequestPolicy::kBlock;

  base::HistogramTester histogram_tester;

  ResourceRequest request;
  request.url = GURL("http://192.168.1.1");
  PrivateNetworkAccessChecker checker(request, &client_security_state,
                                      mojom::kURLLoadOptionNone);

  checker.Check(DirectTransport(PrivateEndpoint()));

  histogram_tester.ExpectUniqueSample(
      "Security.PrivateNetworkAccess.PrivateIpInferrable", true, 1);
}

TEST(PrivateNetworkAccessCheckerTest,
     RecordsPrivateIpInferrableHistogramTrueIpv6) {
  mojom::ClientSecurityState client_security_state;
  client_security_state.ip_address_space = mojom::IPAddressSpace::kPublic;
  client_security_state.private_network_request_policy =
      mojom::PrivateNetworkRequestPolicy::kBlock;

  base::HistogramTester histogram_tester;

  ResourceRequest request;
  request.url = GURL("http://[fc00::]");
  PrivateNetworkAccessChecker checker(request, &client_security_state,
                                      mojom::kURLLoadOptionNone);

  checker.Check(DirectTransport(net::IPEndPoint(
      net::IPAddress(0xfc, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0), 80)));

  histogram_tester.ExpectUniqueSample(
      "Security.PrivateNetworkAccess.PrivateIpInferrable", true, 1);
}

TEST(PrivateNetworkAccessCheckerTest,
     RecordsPrivateIpInferrableHistogramFalse) {
  mojom::ClientSecurityState client_security_state;
  client_security_state.ip_address_space = mojom::IPAddressSpace::kPublic;
  client_security_state.private_network_request_policy =
      mojom::PrivateNetworkRequestPolicy::kBlock;

  base::HistogramTester histogram_tester;

  ResourceRequest request;
  request.url = GURL("http://foo");
  PrivateNetworkAccessChecker checker(request, &client_security_state,
                                      mojom::kURLLoadOptionNone);

  checker.Check(DirectTransport(PrivateEndpoint()));

  histogram_tester.ExpectUniqueSample(
      "Security.PrivateNetworkAccess.PrivateIpInferrable", false, 1);
}

TEST(PrivateNetworkAccessCheckerTest,
     RecordsPrivateIpInferrableHistogramAfterResetForRetry) {
  base::HistogramTester histogram_tester;

  ResourceRequest request;
  request.url = PrivateEndpointUrl();
  PrivateNetworkAccessChecker checker(request, kNullClientSecurityState,
                                      mojom::kURLLoadOptionNone);

  checker.ResetForRetry();
  checker.Check(DirectTransport(PrivateEndpoint()));

  histogram_tester.ExpectUniqueSample(
      "Security.PrivateNetworkAccess.PrivateIpResolveMatch", true, 1);
}

TEST(PrivateNetworkAccessCheckerTest,
     RecordsPrivateIpInferrableHistogramAfterResetForRedirect) {
  base::HistogramTester histogram_tester;

  ResourceRequest request;
  request.url = GURL("http://1.2.3.4");
  PrivateNetworkAccessChecker checker(request, kNullClientSecurityState,
                                      mojom::kURLLoadOptionNone);

  checker.ResetForRedirect(PrivateEndpointUrl());
  checker.Check(DirectTransport(PrivateEndpoint()));

  histogram_tester.ExpectUniqueSample(
      "Security.PrivateNetworkAccess.PrivateIpResolveMatch", true, 1);
}

TEST(PrivateNetworkAccessCheckerTest,
     DoesNotRecordPrivateIpResolveMatchHistogramLocalhost) {
  base::HistogramTester histogram_tester;

  ResourceRequest request;
  request.url = GURL("http://127.0.0.1");
  PrivateNetworkAccessChecker checker(request, kNullClientSecurityState,
                                      mojom::kURLLoadOptionNone);

  checker.Check(DirectTransport(PrivateEndpoint()));

  histogram_tester.ExpectTotalCount(
      "Security.PrivateNetworkAccess.PrivateIpResolveMatch", 0);
}

TEST(PrivateNetworkAccessCheckerTest,
     DoesNotRecordPrivateIpResolveMatchHistogramDomain) {
  base::HistogramTester histogram_tester;

  ResourceRequest request;
  request.url = GURL("http://foo");
  PrivateNetworkAccessChecker checker(request, kNullClientSecurityState,
                                      mojom::kURLLoadOptionNone);

  checker.Check(DirectTransport(PrivateEndpoint()));

  histogram_tester.ExpectTotalCount(
      "Security.PrivateNetworkAccess.PrivateIpResolveMatch", 0);
}

TEST(PrivateNetworkAccessCheckerTest,
     DoesNotRecordPrivateIpResolveMatchHistogramProxy) {
  base::HistogramTester histogram_tester;

  ResourceRequest request;
  request.url = PrivateEndpointUrl();
  PrivateNetworkAccessChecker checker(request, kNullClientSecurityState,
                                      mojom::kURLLoadOptionNone);

  checker.Check(ProxiedTransport(LocalEndpoint()));

  histogram_tester.ExpectTotalCount(
      "Security.PrivateNetworkAccess.PrivateIpResolveMatch", 0);
}

TEST(PrivateNetworkAccessCheckerTest,
     RecordsPrivateIpResolveMatchHistogramFalse) {
  base::HistogramTester histogram_tester;

  ResourceRequest request;
  request.url = GURL("http://192.168.1.1");
  PrivateNetworkAccessChecker checker(request, kNullClientSecurityState,
                                      mojom::kURLLoadOptionNone);

  checker.Check(DirectTransport(PrivateEndpoint()));

  histogram_tester.ExpectUniqueSample(
      "Security.PrivateNetworkAccess.PrivateIpResolveMatch", false, 1);
}

TEST(PrivateNetworkAccessCheckerTest,
     RecordsPrivateIpResolveMatchHistogramTrue) {
  base::HistogramTester histogram_tester;

  ResourceRequest request;
  request.url = PrivateEndpointUrl();
  PrivateNetworkAccessChecker checker(request, kNullClientSecurityState,
                                      mojom::kURLLoadOptionNone);

  checker.Check(DirectTransport(PrivateEndpoint()));

  histogram_tester.ExpectUniqueSample(
      "Security.PrivateNetworkAccess.PrivateIpResolveMatch", true, 1);
}

TEST(PrivateNetworkAccessCheckerTest,
     RecordsPrivateIpResolveMatchHistogramTrueIpv6) {
  base::HistogramTester histogram_tester;

  ResourceRequest request;
  request.url = GURL("http://[fc00::]");
  PrivateNetworkAccessChecker checker(request, kNullClientSecurityState,
                                      mojom::kURLLoadOptionNone);

  checker.Check(DirectTransport(net::IPEndPoint(
      net::IPAddress(0xfc, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0), 80)));

  histogram_tester.ExpectUniqueSample(
      "Security.PrivateNetworkAccess.PrivateIpResolveMatch", true, 1);
}

TEST(PrivateNetworkAccessCheckerTest,
     RecordsPrivateIpResolveMatchHistogramAfterResetForRetry) {
  base::HistogramTester histogram_tester;

  ResourceRequest request;
  request.url = GURL("http://192.168.1.1");
  PrivateNetworkAccessChecker checker(request, kNullClientSecurityState,
                                      mojom::kURLLoadOptionNone);

  checker.ResetForRetry();
  checker.Check(DirectTransport(PrivateEndpoint()));

  histogram_tester.ExpectUniqueSample(
      "Security.PrivateNetworkAccess.PrivateIpResolveMatch", false, 1);
}

TEST(PrivateNetworkAccessCheckerTest,
     RecordsPrivateIpResolveMatchHistogramAfterResetForRedirect) {
  base::HistogramTester histogram_tester;

  ResourceRequest request;
  request.url = GURL("http://192.168.1.1");
  PrivateNetworkAccessChecker checker(request, kNullClientSecurityState,
                                      mojom::kURLLoadOptionNone);

  checker.ResetForRedirect(PrivateEndpointUrl());
  checker.Check(DirectTransport(PrivateEndpoint()));

  histogram_tester.ExpectUniqueSample(
      "Security.PrivateNetworkAccess.PrivateIpResolveMatch", true, 1);
}

}  // namespace
}  // namespace network
