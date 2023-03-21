// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/local_network_access_checker.h"

#include "base/strings/strcat.h"
#include "base/strings/string_piece.h"
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

using Result = LocalNetworkAccessCheckResult;

constexpr base::StringPiece kCheckResultHistogramName =
    "Security.PrivateNetworkAccess.CheckResult";

constexpr char kNoAcceptChFrame[] = "";

// For better readability than literal `nullptr`.
constexpr mojom::ClientSecurityState* kNullClientSecurityState = nullptr;

net::IPEndPoint LoopbackEndpoint() {
  return net::IPEndPoint(net::IPAddress::IPv4Localhost(), 80);
}

net::IPEndPoint LocalEndpoint() {
  return net::IPEndPoint(net::IPAddress(10, 0, 0, 1), 80);
}

GURL LocalEndpointUrl() {
  return GURL("http://10.0.0.1");
}

net::IPEndPoint PublicEndpoint() {
  return net::IPEndPoint(net::IPAddress(1, 2, 3, 4), 80);
}

net::TransportInfo DirectTransport(const net::IPEndPoint& endpoint) {
  return net::TransportInfo(net::TransportType::kDirect, endpoint,
                            kNoAcceptChFrame);
}

net::TransportInfo ProxiedTransport(const net::IPEndPoint& endpoint) {
  return net::TransportInfo(net::TransportType::kProxied, endpoint,
                            kNoAcceptChFrame);
}

net::TransportInfo CachedTransport(const net::IPEndPoint& endpoint) {
  return net::TransportInfo(net::TransportType::kCached, endpoint,
                            kNoAcceptChFrame);
}

net::TransportInfo MakeTransport(net::TransportType type,
                                 const net::IPEndPoint& endpoint) {
  return net::TransportInfo(type, endpoint, kNoAcceptChFrame);
}

TEST(LocalNetworkAccessCheckerTest, ClientSecurityStateNull) {
  LocalNetworkAccessChecker checker(ResourceRequest(), kNullClientSecurityState,
                                    mojom::kURLLoadOptionNone);

  EXPECT_EQ(checker.client_security_state(), nullptr);
  EXPECT_EQ(checker.ClientAddressSpace(), mojom::IPAddressSpace::kUnknown);
}

TEST(LocalNetworkAccessCheckerTest, ClientSecurityStateFromFactory) {
  mojom::ClientSecurityState client_security_state;
  client_security_state.ip_address_space = mojom::IPAddressSpace::kPublic;

  LocalNetworkAccessChecker checker(ResourceRequest(), &client_security_state,
                                    mojom::kURLLoadOptionNone);

  EXPECT_EQ(checker.client_security_state(), &client_security_state);
  EXPECT_EQ(checker.ClientAddressSpace(), mojom::IPAddressSpace::kPublic);
}

TEST(LocalNetworkAccessCheckerTest, ClientSecurityStateFromRequest) {
  ResourceRequest request;
  request.trusted_params.emplace();
  request.trusted_params->client_security_state =
      mojom::ClientSecurityState::New();
  request.trusted_params->client_security_state->ip_address_space =
      mojom::IPAddressSpace::kLocal;

  LocalNetworkAccessChecker checker(request, kNullClientSecurityState,
                                    mojom::kURLLoadOptionNone);

  EXPECT_NE(checker.client_security_state(), nullptr);
  EXPECT_EQ(checker.ClientAddressSpace(), mojom::IPAddressSpace::kLocal);
}

TEST(LocalNetworkAccessCheckerTest, CheckLoadOptionUnknown) {
  base::HistogramTester histogram_tester;

  LocalNetworkAccessChecker checker(ResourceRequest(), kNullClientSecurityState,
                                    mojom::kURLLoadOptionBlockLocalRequest);

  EXPECT_EQ(checker.Check(DirectTransport(net::IPEndPoint())),
            Result::kAllowedMissingClientSecurityState);
}

TEST(LocalNetworkAccessCheckerTest, CheckLoadOptionPublic) {
  base::HistogramTester histogram_tester;

  LocalNetworkAccessChecker checker(ResourceRequest(), kNullClientSecurityState,
                                    mojom::kURLLoadOptionBlockLocalRequest);

  EXPECT_EQ(checker.Check(DirectTransport(PublicEndpoint())),
            Result::kAllowedMissingClientSecurityState);
}

TEST(LocalNetworkAccessCheckerTest, CheckLoadOptionLocal) {
  base::HistogramTester histogram_tester;

  LocalNetworkAccessChecker checker(ResourceRequest(), kNullClientSecurityState,
                                    mojom::kURLLoadOptionBlockLocalRequest);

  EXPECT_EQ(checker.Check(DirectTransport(LocalEndpoint())),
            Result::kBlockedByLoadOption);

  histogram_tester.ExpectUniqueSample(kCheckResultHistogramName,
                                      Result::kBlockedByLoadOption, 1);
}

TEST(LocalNetworkAccessCheckerTest, CheckLoadOptionLoopback) {
  base::HistogramTester histogram_tester;

  LocalNetworkAccessChecker checker(ResourceRequest(), kNullClientSecurityState,
                                    mojom::kURLLoadOptionBlockLocalRequest);

  EXPECT_EQ(checker.Check(DirectTransport(LoopbackEndpoint())),
            Result::kBlockedByLoadOption);

  histogram_tester.ExpectUniqueSample(kCheckResultHistogramName,
                                      Result::kBlockedByLoadOption, 1);
}

TEST(LocalNetworkAccessCheckerTest,
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
  client_security_state.local_network_request_policy =
      mojom::LocalNetworkRequestPolicy::kPreflightBlock;

  LocalNetworkAccessChecker checker(request, &client_security_state,
                                    mojom::kURLLoadOptionNone);

  EXPECT_EQ(checker.Check(DirectTransport(LoopbackEndpoint())),
            Result::kAllowedPotentiallyTrustworthySameOrigin);

  histogram_tester.ExpectUniqueSample(
      kCheckResultHistogramName,
      Result::kAllowedPotentiallyTrustworthySameOrigin, 1);
}

TEST(LocalNetworkAccessCheckerTest,
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
  client_security_state.local_network_request_policy =
      mojom::LocalNetworkRequestPolicy::kPreflightBlock;

  LocalNetworkAccessChecker checker(request, &client_security_state,
                                    mojom::kURLLoadOptionNone);

  EXPECT_EQ(checker.Check(DirectTransport(LoopbackEndpoint())),
            Result::kBlockedByPolicyPreflightBlock);

  histogram_tester.ExpectUniqueSample(
      kCheckResultHistogramName, Result::kBlockedByPolicyPreflightBlock, 1);
}

TEST(LocalNetworkAccessCheckerTest, CheckDisallowedUntrustworthySameOrigin) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kLocalNetworkAccessAllowPotentiallyTrustworthySameOrigin);

  base::HistogramTester histogram_tester;

  ResourceRequest request;
  request.url = GURL("http://example.com/subresource");
  request.request_initiator = url::Origin::Create(request.url);

  mojom::ClientSecurityState client_security_state;
  client_security_state.ip_address_space = mojom::IPAddressSpace::kPublic;
  client_security_state.local_network_request_policy =
      mojom::LocalNetworkRequestPolicy::kBlock;

  LocalNetworkAccessChecker checker(request, &client_security_state,
                                    mojom::kURLLoadOptionNone);

  EXPECT_EQ(checker.Check(DirectTransport(LoopbackEndpoint())),
            Result::kBlockedByPolicyBlock);

  histogram_tester.ExpectUniqueSample(kCheckResultHistogramName,
                                      Result::kBlockedByPolicyBlock, 1);
}

TEST(LocalNetworkAccessCheckerTest,
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
  client_security_state.local_network_request_policy =
      mojom::LocalNetworkRequestPolicy::kPreflightBlock;

  LocalNetworkAccessChecker checker(request, &client_security_state,
                                    mojom::kURLLoadOptionNone);
  checker.ResetForRedirect(GURL("https://subdomain.example.com/subresource"));

  EXPECT_EQ(checker.Check(DirectTransport(LoopbackEndpoint())),
            Result::kBlockedByPolicyPreflightBlock);

  histogram_tester.ExpectUniqueSample(
      kCheckResultHistogramName, Result::kBlockedByPolicyPreflightBlock, 1);
}

TEST(LocalNetworkAccessCheckerTest,
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
  client_security_state.local_network_request_policy =
      mojom::LocalNetworkRequestPolicy::kPreflightBlock;

  LocalNetworkAccessChecker checker(request, &client_security_state,
                                    mojom::kURLLoadOptionNone);
  checker.ResetForRedirect(GURL("https://subdomain.example.com/subresource"));

  EXPECT_EQ(checker.Check(DirectTransport(LoopbackEndpoint())),
            Result::kAllowedPotentiallyTrustworthySameOrigin);

  histogram_tester.ExpectUniqueSample(
      kCheckResultHistogramName,
      Result::kAllowedPotentiallyTrustworthySameOrigin, 1);
}

TEST(LocalNetworkAccessCheckerTest, CheckAllowedMissingClientSecurityState) {
  base::HistogramTester histogram_tester;

  LocalNetworkAccessChecker checker(ResourceRequest(), kNullClientSecurityState,
                                    mojom::kURLLoadOptionNone);

  EXPECT_EQ(checker.Check(DirectTransport(LoopbackEndpoint())),
            Result::kAllowedMissingClientSecurityState);

  histogram_tester.ExpectUniqueSample(
      kCheckResultHistogramName, Result::kAllowedMissingClientSecurityState, 1);
}

TEST(LocalNetworkAccessCheckerTest,
     CheckAllowedMissingClientSecurityStateInconsistentIpAddressSpace) {
  base::HistogramTester histogram_tester;

  LocalNetworkAccessChecker checker(ResourceRequest(), kNullClientSecurityState,
                                    mojom::kURLLoadOptionNone);

  EXPECT_EQ(checker.Check(DirectTransport(LoopbackEndpoint())),
            Result::kAllowedMissingClientSecurityState);

  // Even though this is inconsistent with the previous IP address space, the
  // inconsistency is ignored because of the missing client security state.
  //
  // This does not risk triggering https://crbug.com/1279376, because no check
  // with this checker will ever return `kBlocked*`.
  EXPECT_EQ(checker.Check(DirectTransport(PublicEndpoint())),
            Result::kAllowedMissingClientSecurityState);
}

TEST(LocalNetworkAccessCheckerTest, CheckAllowedNoLessPublic) {
  base::HistogramTester histogram_tester;

  mojom::ClientSecurityState client_security_state;
  client_security_state.ip_address_space = mojom::IPAddressSpace::kLocal;
  client_security_state.local_network_request_policy =
      mojom::LocalNetworkRequestPolicy::kBlock;

  LocalNetworkAccessChecker checker(ResourceRequest(), &client_security_state,
                                    mojom::kURLLoadOptionNone);

  EXPECT_EQ(checker.Check(DirectTransport(LocalEndpoint())),
            Result::kAllowedNoLessPublic);

  histogram_tester.ExpectUniqueSample(kCheckResultHistogramName,
                                      Result::kAllowedNoLessPublic, 1);
}

TEST(LocalNetworkAccessCheckerTest, CheckAllowedByPolicyAllow) {
  base::HistogramTester histogram_tester;

  mojom::ClientSecurityState client_security_state;
  client_security_state.ip_address_space = mojom::IPAddressSpace::kPublic;
  client_security_state.local_network_request_policy =
      mojom::LocalNetworkRequestPolicy::kAllow;

  LocalNetworkAccessChecker checker(ResourceRequest(), &client_security_state,
                                    mojom::kURLLoadOptionNone);

  EXPECT_EQ(checker.Check(DirectTransport(LocalEndpoint())),
            Result::kAllowedByPolicyAllow);

  histogram_tester.ExpectUniqueSample(kCheckResultHistogramName,
                                      Result::kAllowedByPolicyAllow, 1);
}

TEST(LocalNetworkAccessCheckerTest,
     CheckAllowedByPolicyWarnInconsistentIpAddressSpace) {
  base::HistogramTester histogram_tester;

  mojom::ClientSecurityState client_security_state;
  client_security_state.ip_address_space = mojom::IPAddressSpace::kPublic;
  client_security_state.local_network_request_policy =
      mojom::LocalNetworkRequestPolicy::kWarn;

  LocalNetworkAccessChecker checker(ResourceRequest(), &client_security_state,
                                    mojom::kURLLoadOptionNone);

  EXPECT_EQ(checker.Check(DirectTransport(LocalEndpoint())),
            Result::kAllowedByPolicyWarn);

  // Even though this is inconsistent with the previous IP address space, the
  // inconsistency is ignored because of the `kWarn` policy.
  EXPECT_EQ(checker.Check(DirectTransport(PublicEndpoint())),
            Result::kAllowedByPolicyWarn);
}

TEST(LocalNetworkAccessCheckerTest,
     CheckAllowedByPolicyAllowInconsistentIpAddressSpace) {
  base::HistogramTester histogram_tester;

  mojom::ClientSecurityState client_security_state;
  client_security_state.ip_address_space = mojom::IPAddressSpace::kPublic;
  client_security_state.local_network_request_policy =
      mojom::LocalNetworkRequestPolicy::kAllow;

  LocalNetworkAccessChecker checker(ResourceRequest(), &client_security_state,
                                    mojom::kURLLoadOptionNone);

  EXPECT_EQ(checker.Check(DirectTransport(LocalEndpoint())),
            Result::kAllowedByPolicyAllow);

  // Even though this is inconsistent with the previous IP address space, the
  // inconsistency is ignored because of the `kAllow` policy.
  //
  // This does not risk triggering https://crbug.com/1279376, because no check
  // with this checker will ever return `kBlocked*`.
  EXPECT_EQ(checker.Check(DirectTransport(PublicEndpoint())),
            Result::kAllowedByPolicyAllow);
}

TEST(LocalNetworkAccessCheckerTest, CheckAllowedByPolicyWarn) {
  base::HistogramTester histogram_tester;

  mojom::ClientSecurityState client_security_state;
  client_security_state.ip_address_space = mojom::IPAddressSpace::kPublic;
  client_security_state.local_network_request_policy =
      mojom::LocalNetworkRequestPolicy::kWarn;

  LocalNetworkAccessChecker checker(ResourceRequest(), &client_security_state,
                                    mojom::kURLLoadOptionNone);

  EXPECT_EQ(checker.Check(DirectTransport(LocalEndpoint())),
            Result::kAllowedByPolicyWarn);

  histogram_tester.ExpectUniqueSample(kCheckResultHistogramName,
                                      Result::kAllowedByPolicyWarn, 1);
}

TEST(LocalNetworkAccessCheckerTest, CheckBlockedByPolicyBlock) {
  base::HistogramTester histogram_tester;

  mojom::ClientSecurityState client_security_state;
  client_security_state.ip_address_space = mojom::IPAddressSpace::kPublic;
  client_security_state.local_network_request_policy =
      mojom::LocalNetworkRequestPolicy::kBlock;

  LocalNetworkAccessChecker checker(ResourceRequest(), &client_security_state,
                                    mojom::kURLLoadOptionNone);

  EXPECT_EQ(checker.Check(DirectTransport(LocalEndpoint())),
            Result::kBlockedByPolicyBlock);

  histogram_tester.ExpectUniqueSample(kCheckResultHistogramName,
                                      Result::kBlockedByPolicyBlock, 1);
}

TEST(LocalNetworkAccessCheckerTest, CheckBlockedByPolicyPreflightWarn) {
  base::HistogramTester histogram_tester;

  mojom::ClientSecurityState client_security_state;
  client_security_state.ip_address_space = mojom::IPAddressSpace::kPublic;
  client_security_state.local_network_request_policy =
      mojom::LocalNetworkRequestPolicy::kPreflightWarn;

  LocalNetworkAccessChecker checker(ResourceRequest(), &client_security_state,
                                    mojom::kURLLoadOptionNone);

  EXPECT_EQ(checker.Check(DirectTransport(LocalEndpoint())),
            Result::kBlockedByPolicyPreflightWarn);

  histogram_tester.ExpectUniqueSample(kCheckResultHistogramName,
                                      Result::kBlockedByPolicyPreflightWarn, 1);
}

TEST(LocalNetworkAccessCheckerTest, CheckBlockedByPolicyPreflightBlock) {
  base::HistogramTester histogram_tester;

  mojom::ClientSecurityState client_security_state;
  client_security_state.ip_address_space = mojom::IPAddressSpace::kPublic;
  client_security_state.local_network_request_policy =
      mojom::LocalNetworkRequestPolicy::kPreflightBlock;

  LocalNetworkAccessChecker checker(ResourceRequest(), &client_security_state,
                                    mojom::kURLLoadOptionNone);

  EXPECT_EQ(checker.Check(DirectTransport(LocalEndpoint())),
            Result::kBlockedByPolicyPreflightBlock);

  histogram_tester.ExpectUniqueSample(
      kCheckResultHistogramName, Result::kBlockedByPolicyPreflightBlock, 1);
}

TEST(LocalNetworkAccessCheckerTest, CheckBlockedByTargetIpAddressSpace) {
  base::HistogramTester histogram_tester;

  mojom::ClientSecurityState client_security_state;
  client_security_state.ip_address_space = mojom::IPAddressSpace::kPublic;
  client_security_state.local_network_request_policy =
      mojom::LocalNetworkRequestPolicy::kPreflightBlock;

  ResourceRequest request;
  request.target_ip_address_space = mojom::IPAddressSpace::kPublic;

  LocalNetworkAccessChecker checker(request, &client_security_state,
                                    mojom::kURLLoadOptionNone);

  EXPECT_EQ(checker.Check(DirectTransport(LocalEndpoint())),
            Result::kBlockedByTargetIpAddressSpace);

  histogram_tester.ExpectUniqueSample(
      kCheckResultHistogramName, Result::kBlockedByTargetIpAddressSpace, 1);
}

TEST(LocalNetworkAccessCheckerTest, CheckAllowedByPolicyPreflightWarn) {
  base::HistogramTester histogram_tester;

  mojom::ClientSecurityState client_security_state;
  client_security_state.ip_address_space = mojom::IPAddressSpace::kPublic;
  client_security_state.local_network_request_policy =
      mojom::LocalNetworkRequestPolicy::kPreflightWarn;

  ResourceRequest request;
  request.target_ip_address_space = mojom::IPAddressSpace::kLoopback;

  LocalNetworkAccessChecker checker(request, &client_security_state,
                                    mojom::kURLLoadOptionNone);

  EXPECT_EQ(checker.Check(DirectTransport(LocalEndpoint())),
            Result::kAllowedByPolicyPreflightWarn);

  histogram_tester.ExpectUniqueSample(kCheckResultHistogramName,
                                      Result::kAllowedByPolicyPreflightWarn, 1);
}

TEST(LocalNetworkAccessCheckerTest, CheckAllowedByTargetIpAddressSpace) {
  base::HistogramTester histogram_tester;

  mojom::ClientSecurityState client_security_state;
  client_security_state.ip_address_space = mojom::IPAddressSpace::kPublic;
  client_security_state.local_network_request_policy =
      mojom::LocalNetworkRequestPolicy::kPreflightBlock;

  ResourceRequest request;
  request.target_ip_address_space = mojom::IPAddressSpace::kLocal;

  LocalNetworkAccessChecker checker(request, &client_security_state,
                                    mojom::kURLLoadOptionNone);

  EXPECT_EQ(checker.Check(DirectTransport(LocalEndpoint())),
            Result::kAllowedByTargetIpAddressSpace);

  histogram_tester.ExpectUniqueSample(
      kCheckResultHistogramName, Result::kAllowedByTargetIpAddressSpace, 1);
}

TEST(LocalNetworkAccessCheckerTest,
     CheckAllowedByPolicyPreflightWarnInconsistent) {
  mojom::ClientSecurityState client_security_state;
  client_security_state.ip_address_space = mojom::IPAddressSpace::kPublic;
  client_security_state.local_network_request_policy =
      mojom::LocalNetworkRequestPolicy::kPreflightWarn;

  LocalNetworkAccessChecker checker(ResourceRequest(), &client_security_state,
                                    mojom::kURLLoadOptionNone);

  checker.Check(DirectTransport(PublicEndpoint()));

  base::HistogramTester histogram_tester;

  EXPECT_EQ(checker.Check(DirectTransport(LocalEndpoint())),
            Result::kAllowedByPolicyPreflightWarn);

  histogram_tester.ExpectUniqueSample(kCheckResultHistogramName,
                                      Result::kAllowedByPolicyPreflightWarn, 1);
}

TEST(LocalNetworkAccessCheckerTest, CheckBlockedByInconsistentIpAddressSpace) {
  mojom::ClientSecurityState client_security_state;
  client_security_state.ip_address_space = mojom::IPAddressSpace::kLocal;
  client_security_state.local_network_request_policy =
      mojom::LocalNetworkRequestPolicy::kPreflightBlock;

  LocalNetworkAccessChecker checker(ResourceRequest(), &client_security_state,
                                    mojom::kURLLoadOptionNone);

  EXPECT_EQ(checker.Check(DirectTransport(PublicEndpoint())),
            Result::kAllowedNoLessPublic);

  base::HistogramTester histogram_tester;

  EXPECT_EQ(checker.Check(DirectTransport(LocalEndpoint())),
            Result::kBlockedByInconsistentIpAddressSpace);

  histogram_tester.ExpectUniqueSample(
      kCheckResultHistogramName, Result::kBlockedByInconsistentIpAddressSpace,
      1);
}

TEST(LocalNetworkAccessCheckerTest, ResponseAddressSpace) {
  LocalNetworkAccessChecker checker(ResourceRequest(), kNullClientSecurityState,
                                    mojom::kURLLoadOptionNone);

  EXPECT_EQ(checker.ResponseAddressSpace(), absl::nullopt);

  checker.Check(DirectTransport(PublicEndpoint()));

  EXPECT_THAT(checker.ResponseAddressSpace(),
              Optional(mojom::IPAddressSpace::kPublic));

  checker.Check(DirectTransport(LocalEndpoint()));

  EXPECT_THAT(checker.ResponseAddressSpace(),
              Optional(mojom::IPAddressSpace::kLocal));
}

TEST(LocalNetworkAccessCheckerTest, ProxiedTransportAddressSpaceIsUnknown) {
  LocalNetworkAccessChecker checker(ResourceRequest(), kNullClientSecurityState,
                                    mojom::kURLLoadOptionBlockLocalRequest);

  // This succeeds in spite of the load option, because the proxied transport
  // is not considered any less public than `kPublic`.
  EXPECT_EQ(checker.Check(ProxiedTransport(LoopbackEndpoint())),
            Result::kAllowedMissingClientSecurityState);

  // In fact, it is considered unknown.
  EXPECT_EQ(checker.ResponseAddressSpace(), mojom::IPAddressSpace::kUnknown);
}

TEST(LocalNetworkAccessCheckerTest,
     CachedFromProxyTransportAddressSpaceIsUnknown) {
  LocalNetworkAccessChecker checker(ResourceRequest(), kNullClientSecurityState,
                                    mojom::kURLLoadOptionBlockLocalRequest);

  // This succeeds in spite of the load option, because the cached-from-proxy
  // transport is not considered any less public than `kPublic`.
  EXPECT_EQ(checker.Check(MakeTransport(net::TransportType::kCachedFromProxy,
                                        LoopbackEndpoint())),
            Result::kAllowedMissingClientSecurityState);

  // In fact, it is considered unknown.
  EXPECT_EQ(checker.ResponseAddressSpace(), mojom::IPAddressSpace::kUnknown);
}

TEST(LocalNetworkAccessCheckerTest, CachedTransportAddressSpace) {
  LocalNetworkAccessChecker checker(ResourceRequest(), kNullClientSecurityState,
                                    mojom::kURLLoadOptionBlockLocalRequest);

  // The cached transport is treated like a direct transport to the same
  // endpoint, so the load option does not fail the check.
  EXPECT_EQ(checker.Check(CachedTransport(PublicEndpoint())),
            Result::kAllowedMissingClientSecurityState);

  EXPECT_EQ(checker.ResponseAddressSpace(), mojom::IPAddressSpace::kPublic);

  // When the endpoint is loopback, the check fails as for a direct transport.
  EXPECT_EQ(checker.Check(CachedTransport(LoopbackEndpoint())),
            Result::kBlockedByLoadOption);

  EXPECT_EQ(checker.ResponseAddressSpace(), mojom::IPAddressSpace::kLoopback);
}

TEST(LocalNetworkAccessCheckerTest, ResetTargetAddressSpace) {
  mojom::ClientSecurityState client_security_state;
  client_security_state.ip_address_space = mojom::IPAddressSpace::kLocal;
  client_security_state.local_network_request_policy =
      mojom::LocalNetworkRequestPolicy::kPreflightBlock;

  ResourceRequest request;
  request.target_ip_address_space = mojom::IPAddressSpace::kPublic;

  LocalNetworkAccessChecker checker(request, &client_security_state,
                                    mojom::kURLLoadOptionNone);

  checker.ResetForRedirect(GURL("http://foo.com"));

  // The target address space has been cleared.
  EXPECT_EQ(checker.TargetAddressSpace(), mojom::IPAddressSpace::kUnknown);

  // This succeeds even though the IP address space does not match the target
  // passed at construction time, thanks to `ResetForRedirect()`.
  EXPECT_EQ(checker.Check(DirectTransport(LocalEndpoint())),
            Result::kAllowedNoLessPublic);
}

TEST(LocalNetworkAccessCheckerTest, ResetResponseAddressSpace) {
  LocalNetworkAccessChecker checker(ResourceRequest(), kNullClientSecurityState,
                                    mojom::kURLLoadOptionNone);

  checker.Check(DirectTransport(LocalEndpoint()));

  checker.ResetForRedirect(GURL("http://foo.com"));

  EXPECT_EQ(checker.ResponseAddressSpace(), absl::nullopt);

  // This succeeds even though the IP address space does not match that of the
  // previous endpoint passed to `Check()`, thanks to `ResetForRedirect()`.
  EXPECT_EQ(checker.Check(DirectTransport(LoopbackEndpoint())),
            Result::kAllowedMissingClientSecurityState);
}

TEST(LocalNetworkAccessCheckerTest,
     DoesNotRecordLocalIpInferrableHistogramForPublicEndpoint) {
  base::HistogramTester histogram_tester;

  ResourceRequest request;
  request.url = GURL("http://192.168.1.1");
  LocalNetworkAccessChecker checker(request, kNullClientSecurityState,
                                    mojom::kURLLoadOptionNone);

  checker.Check(DirectTransport(PublicEndpoint()));

  histogram_tester.ExpectTotalCount(
      "Security.PrivateNetworkAccess.PrivateIpInferrable", 0);
}

TEST(LocalNetworkAccessCheckerTest,
     DoesNotRecordLocalIpInferrableHistogramForHttpsScheme) {
  base::HistogramTester histogram_tester;

  ResourceRequest request;
  request.url = GURL("https://192.168.1.1");
  LocalNetworkAccessChecker checker(request, kNullClientSecurityState,
                                    mojom::kURLLoadOptionNone);

  checker.Check(DirectTransport(LocalEndpoint()));

  histogram_tester.ExpectTotalCount(
      "Security.PrivateNetworkAccess.PrivateIpInferrable", 0);
}

TEST(LocalNetworkAccessCheckerTest,
     DoesNotRecordLocalIpInferrableHistogramWithTargetIpAddressSpace) {
  mojom::ClientSecurityState client_security_state;
  client_security_state.ip_address_space = mojom::IPAddressSpace::kPublic;
  client_security_state.local_network_request_policy =
      mojom::LocalNetworkRequestPolicy::kPreflightWarn;

  base::HistogramTester histogram_tester;

  ResourceRequest request;
  request.url = GURL("http://192.168.1.1");
  request.target_ip_address_space = mojom::IPAddressSpace::kLocal;

  LocalNetworkAccessChecker checker(request, &client_security_state,
                                    mojom::kURLLoadOptionNone);

  checker.Check(DirectTransport(LocalEndpoint()));

  histogram_tester.ExpectTotalCount(
      "Security.PrivateNetworkAccess.PrivateIpInferrable", 0);
}

TEST(LocalNetworkAccessCheckerTest,
     RecordsLocalIpInferrableHistogramPolicyAllow) {
  mojom::ClientSecurityState client_security_state;
  client_security_state.ip_address_space = mojom::IPAddressSpace::kPublic;
  client_security_state.local_network_request_policy =
      mojom::LocalNetworkRequestPolicy::kAllow;

  base::HistogramTester histogram_tester;

  ResourceRequest request;
  request.url = GURL("http://192.168.1.1");
  LocalNetworkAccessChecker checker(request, &client_security_state,
                                    mojom::kURLLoadOptionNone);

  checker.Check(DirectTransport(LocalEndpoint()));

  histogram_tester.ExpectUniqueSample(
      "Security.PrivateNetworkAccess.PrivateIpInferrable", true, 1);
}

TEST(LocalNetworkAccessCheckerTest,
     RecordsLocalIpInferrableHistogramNoLessPublic) {
  mojom::ClientSecurityState client_security_state;
  client_security_state.ip_address_space = mojom::IPAddressSpace::kLocal;
  client_security_state.local_network_request_policy =
      mojom::LocalNetworkRequestPolicy::kBlock;

  base::HistogramTester histogram_tester;

  ResourceRequest request;
  request.url = GURL("http://192.168.1.1");
  LocalNetworkAccessChecker checker(request, &client_security_state,
                                    mojom::kURLLoadOptionNone);

  checker.Check(DirectTransport(LocalEndpoint()));

  histogram_tester.ExpectUniqueSample(
      "Security.PrivateNetworkAccess.PrivateIpInferrable", true, 1);
}

TEST(LocalNetworkAccessCheckerTest, RecordsLocalIpInferrableHistogramTrue) {
  mojom::ClientSecurityState client_security_state;
  client_security_state.ip_address_space = mojom::IPAddressSpace::kPublic;
  client_security_state.local_network_request_policy =
      mojom::LocalNetworkRequestPolicy::kBlock;

  base::HistogramTester histogram_tester;

  ResourceRequest request;
  request.url = GURL("http://192.168.1.1");
  LocalNetworkAccessChecker checker(request, &client_security_state,
                                    mojom::kURLLoadOptionNone);

  checker.Check(DirectTransport(LocalEndpoint()));

  histogram_tester.ExpectUniqueSample(
      "Security.PrivateNetworkAccess.PrivateIpInferrable", true, 1);
}

TEST(LocalNetworkAccessCheckerTest, RecordsLocalIpInferrableHistogramTrueIpv6) {
  mojom::ClientSecurityState client_security_state;
  client_security_state.ip_address_space = mojom::IPAddressSpace::kPublic;
  client_security_state.local_network_request_policy =
      mojom::LocalNetworkRequestPolicy::kBlock;

  base::HistogramTester histogram_tester;

  ResourceRequest request;
  request.url = GURL("http://[fc00::]");
  LocalNetworkAccessChecker checker(request, &client_security_state,
                                    mojom::kURLLoadOptionNone);

  checker.Check(DirectTransport(net::IPEndPoint(
      net::IPAddress(0xfc, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0), 80)));

  histogram_tester.ExpectUniqueSample(
      "Security.PrivateNetworkAccess.PrivateIpInferrable", true, 1);
}

TEST(LocalNetworkAccessCheckerTest, RecordsLocalIpInferrableHistogramFalse) {
  mojom::ClientSecurityState client_security_state;
  client_security_state.ip_address_space = mojom::IPAddressSpace::kPublic;
  client_security_state.local_network_request_policy =
      mojom::LocalNetworkRequestPolicy::kBlock;

  base::HistogramTester histogram_tester;

  ResourceRequest request;
  request.url = GURL("http://foo");
  LocalNetworkAccessChecker checker(request, &client_security_state,
                                    mojom::kURLLoadOptionNone);

  checker.Check(DirectTransport(LocalEndpoint()));

  histogram_tester.ExpectUniqueSample(
      "Security.PrivateNetworkAccess.PrivateIpInferrable", false, 1);
}

TEST(LocalNetworkAccessCheckerTest,
     RecordsLocalIpInferrableHistogramAfterResetForRetry) {
  base::HistogramTester histogram_tester;

  ResourceRequest request;
  request.url = LocalEndpointUrl();
  LocalNetworkAccessChecker checker(request, kNullClientSecurityState,
                                    mojom::kURLLoadOptionNone);

  checker.ResetForRetry();
  checker.Check(DirectTransport(LocalEndpoint()));

  histogram_tester.ExpectUniqueSample(
      "Security.PrivateNetworkAccess.PrivateIpResolveMatch", true, 1);
}

TEST(LocalNetworkAccessCheckerTest,
     RecordsLocalIpInferrableHistogramAfterResetForRedirect) {
  base::HistogramTester histogram_tester;

  ResourceRequest request;
  request.url = GURL("http://1.2.3.4");
  LocalNetworkAccessChecker checker(request, kNullClientSecurityState,
                                    mojom::kURLLoadOptionNone);

  checker.ResetForRedirect(LocalEndpointUrl());
  checker.Check(DirectTransport(LocalEndpoint()));

  histogram_tester.ExpectUniqueSample(
      "Security.PrivateNetworkAccess.PrivateIpResolveMatch", true, 1);
}

TEST(LocalNetworkAccessCheckerTest,
     DoesNotRecordLocalIpResolveMatchHistogramLocalhost) {
  base::HistogramTester histogram_tester;

  ResourceRequest request;
  request.url = GURL("http://127.0.0.1");
  LocalNetworkAccessChecker checker(request, kNullClientSecurityState,
                                    mojom::kURLLoadOptionNone);

  checker.Check(DirectTransport(LocalEndpoint()));

  histogram_tester.ExpectTotalCount(
      "Security.PrivateNetworkAccess.PrivateIpResolveMatch", 0);
}

TEST(LocalNetworkAccessCheckerTest,
     DoesNotRecordLocalIpResolveMatchHistogramDomain) {
  base::HistogramTester histogram_tester;

  ResourceRequest request;
  request.url = GURL("http://foo");
  LocalNetworkAccessChecker checker(request, kNullClientSecurityState,
                                    mojom::kURLLoadOptionNone);

  checker.Check(DirectTransport(LocalEndpoint()));

  histogram_tester.ExpectTotalCount(
      "Security.PrivateNetworkAccess.PrivateIpResolveMatch", 0);
}

TEST(LocalNetworkAccessCheckerTest,
     DoesNotRecordLocalIpResolveMatchHistogramProxy) {
  base::HistogramTester histogram_tester;

  ResourceRequest request;
  request.url = LocalEndpointUrl();
  LocalNetworkAccessChecker checker(request, kNullClientSecurityState,
                                    mojom::kURLLoadOptionNone);

  checker.Check(ProxiedTransport(LocalEndpoint()));

  histogram_tester.ExpectTotalCount(
      "Security.PrivateNetworkAccess.PrivateIpResolveMatch", 0);
}

TEST(LocalNetworkAccessCheckerTest, RecordsLocalIpResolveMatchHistogramFalse) {
  base::HistogramTester histogram_tester;

  ResourceRequest request;
  request.url = GURL("http://192.168.1.1");
  LocalNetworkAccessChecker checker(request, kNullClientSecurityState,
                                    mojom::kURLLoadOptionNone);

  checker.Check(DirectTransport(LocalEndpoint()));

  histogram_tester.ExpectUniqueSample(
      "Security.PrivateNetworkAccess.PrivateIpResolveMatch", false, 1);
}

TEST(LocalNetworkAccessCheckerTest, RecordsLocalIpResolveMatchHistogramTrue) {
  base::HistogramTester histogram_tester;

  ResourceRequest request;
  request.url = LocalEndpointUrl();
  LocalNetworkAccessChecker checker(request, kNullClientSecurityState,
                                    mojom::kURLLoadOptionNone);

  checker.Check(DirectTransport(LocalEndpoint()));

  histogram_tester.ExpectUniqueSample(
      "Security.PrivateNetworkAccess.PrivateIpResolveMatch", true, 1);
}

TEST(LocalNetworkAccessCheckerTest,
     RecordsLocalIpResolveMatchHistogramTrueIpv6) {
  base::HistogramTester histogram_tester;

  ResourceRequest request;
  request.url = GURL("http://[fc00::]");
  LocalNetworkAccessChecker checker(request, kNullClientSecurityState,
                                    mojom::kURLLoadOptionNone);

  checker.Check(DirectTransport(net::IPEndPoint(
      net::IPAddress(0xfc, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0), 80)));

  histogram_tester.ExpectUniqueSample(
      "Security.PrivateNetworkAccess.PrivateIpResolveMatch", true, 1);
}

TEST(LocalNetworkAccessCheckerTest,
     RecordsLocalIpResolveMatchHistogramAfterResetForRetry) {
  base::HistogramTester histogram_tester;

  ResourceRequest request;
  request.url = GURL("http://192.168.1.1");
  LocalNetworkAccessChecker checker(request, kNullClientSecurityState,
                                    mojom::kURLLoadOptionNone);

  checker.ResetForRetry();
  checker.Check(DirectTransport(LocalEndpoint()));

  histogram_tester.ExpectUniqueSample(
      "Security.PrivateNetworkAccess.PrivateIpResolveMatch", false, 1);
}

TEST(LocalNetworkAccessCheckerTest,
     RecordsLocalIpResolveMatchHistogramAfterResetForRedirect) {
  base::HistogramTester histogram_tester;

  ResourceRequest request;
  request.url = GURL("http://192.168.1.1");
  LocalNetworkAccessChecker checker(request, kNullClientSecurityState,
                                    mojom::kURLLoadOptionNone);

  checker.ResetForRedirect(LocalEndpointUrl());
  checker.Check(DirectTransport(LocalEndpoint()));

  histogram_tester.ExpectUniqueSample(
      "Security.PrivateNetworkAccess.PrivateIpResolveMatch", true, 1);
}

}  // namespace
}  // namespace network
