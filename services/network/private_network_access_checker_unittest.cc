// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/private_network_access_checker.h"

#include "base/strings/string_piece.h"
#include "base/test/metrics/histogram_tester.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/base/transport_info.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/client_security_state.mojom.h"
#include "services/network/public/mojom/cors.mojom.h"
#include "services/network/public/mojom/ip_address_space.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Optional;

namespace network {
namespace {

using Result = PrivateNetworkAccessCheckResult;

constexpr base::StringPiece kCheckResultHistogramName =
    "Security.PrivateNetworkAccess.CheckResult";

constexpr base::StringPiece kMismatchedAddressSpacesHistogramName =
    "Security.PrivateNetworkAccess.MismatchedAddressSpacesDuringRequest";

constexpr char kNoAcceptChFrame[] = "";

net::IPEndPoint LocalEndpoint() {
  return net::IPEndPoint(net::IPAddress::IPv4Localhost(), 80);
}

net::IPEndPoint PrivateEndpoint() {
  return net::IPEndPoint(net::IPAddress(10, 0, 0, 1), 80);
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

mojom::URLLoaderFactoryParamsPtr FactoryParamsWithClientAddressSpace(
    mojom::IPAddressSpace space) {
  auto params = mojom::URLLoaderFactoryParams::New();
  params->client_security_state = mojom::ClientSecurityState::New();
  params->client_security_state->ip_address_space = space;
  return params;
}

TEST(PrivateNetworkAccessCheckerTest, ClientSecurityStateNull) {
  mojom::URLLoaderFactoryParams factory_params;

  PrivateNetworkAccessChecker checker(
      ResourceRequest(), factory_params.client_security_state.get(),
      mojom::kURLLoadOptionNone);

  EXPECT_EQ(checker.client_security_state(), nullptr);
  EXPECT_EQ(checker.ClientAddressSpace(), mojom::IPAddressSpace::kUnknown);
}

TEST(PrivateNetworkAccessCheckerTest, ClientSecurityStateFromFactory) {
  auto factory_params =
      FactoryParamsWithClientAddressSpace(mojom::IPAddressSpace::kPublic);

  PrivateNetworkAccessChecker checker(
      ResourceRequest(), factory_params->client_security_state.get(),
      mojom::kURLLoadOptionNone);

  EXPECT_EQ(checker.client_security_state(),
            factory_params->client_security_state.get());
  EXPECT_EQ(checker.ClientAddressSpace(), mojom::IPAddressSpace::kPublic);
}

TEST(PrivateNetworkAccessCheckerTest, ClientSecurityStateFromRequest) {
  mojom::URLLoaderFactoryParams factory_params;

  ResourceRequest request;
  request.trusted_params.emplace();
  request.trusted_params->client_security_state =
      mojom::ClientSecurityState::New();
  request.trusted_params->client_security_state->ip_address_space =
      mojom::IPAddressSpace::kPrivate;

  PrivateNetworkAccessChecker checker(
      request, factory_params.client_security_state.get(),
      mojom::kURLLoadOptionNone);

  EXPECT_NE(checker.client_security_state(), nullptr);
  EXPECT_EQ(checker.ClientAddressSpace(), mojom::IPAddressSpace::kPrivate);
}

TEST(PrivateNetworkAccessCheckerTest, CheckLoadOptionUnknown) {
  base::HistogramTester histogram_tester;

  mojom::URLLoaderFactoryParams factory_params;
  PrivateNetworkAccessChecker checker(
      ResourceRequest(), factory_params.client_security_state.get(),
      mojom::kURLLoadOptionBlockLocalRequest);

  EXPECT_EQ(checker.Check(DirectTransport(net::IPEndPoint())),
            Result::kAllowedMissingClientSecurityState);
}

TEST(PrivateNetworkAccessCheckerTest, CheckLoadOptionPublic) {
  base::HistogramTester histogram_tester;

  mojom::URLLoaderFactoryParams factory_params;
  PrivateNetworkAccessChecker checker(
      ResourceRequest(), factory_params.client_security_state.get(),
      mojom::kURLLoadOptionBlockLocalRequest);

  EXPECT_EQ(checker.Check(DirectTransport(PublicEndpoint())),
            Result::kAllowedMissingClientSecurityState);
}

TEST(PrivateNetworkAccessCheckerTest, CheckLoadOptionPrivate) {
  base::HistogramTester histogram_tester;

  mojom::URLLoaderFactoryParams factory_params;
  PrivateNetworkAccessChecker checker(
      ResourceRequest(), factory_params.client_security_state.get(),
      mojom::kURLLoadOptionBlockLocalRequest);

  EXPECT_EQ(checker.Check(DirectTransport(PrivateEndpoint())),
            Result::kBlockedByLoadOption);

  histogram_tester.ExpectUniqueSample(kCheckResultHistogramName,
                                      Result::kBlockedByLoadOption, 1);
}

TEST(PrivateNetworkAccessCheckerTest, CheckLoadOptionLocal) {
  base::HistogramTester histogram_tester;

  mojom::URLLoaderFactoryParams factory_params;
  PrivateNetworkAccessChecker checker(
      ResourceRequest(), factory_params.client_security_state.get(),
      mojom::kURLLoadOptionBlockLocalRequest);

  EXPECT_EQ(checker.Check(DirectTransport(LocalEndpoint())),
            Result::kBlockedByLoadOption);

  histogram_tester.ExpectUniqueSample(kCheckResultHistogramName,
                                      Result::kBlockedByLoadOption, 1);
}

TEST(PrivateNetworkAccessCheckerTest, CheckAllowedMissingClientSecurityState) {
  base::HistogramTester histogram_tester;

  mojom::URLLoaderFactoryParams factory_params;
  PrivateNetworkAccessChecker checker(
      ResourceRequest(), factory_params.client_security_state.get(),
      mojom::kURLLoadOptionNone);

  EXPECT_EQ(checker.Check(DirectTransport(LocalEndpoint())),
            Result::kAllowedMissingClientSecurityState);

  histogram_tester.ExpectUniqueSample(
      kCheckResultHistogramName, Result::kAllowedMissingClientSecurityState, 1);
}

TEST(PrivateNetworkAccessCheckerTest,
     CheckAllowedMissingClientSecurityStateInconsistentIpAddressSpace) {
  base::HistogramTester histogram_tester;

  mojom::URLLoaderFactoryParams factory_params;
  PrivateNetworkAccessChecker checker(
      ResourceRequest(), factory_params.client_security_state.get(),
      mojom::kURLLoadOptionNone);

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

  mojom::URLLoaderFactoryParams factory_params;
  factory_params.client_security_state = mojom::ClientSecurityState::New();
  factory_params.client_security_state->ip_address_space =
      mojom::IPAddressSpace::kPrivate;
  factory_params.client_security_state->private_network_request_policy =
      mojom::PrivateNetworkRequestPolicy::kBlock;

  PrivateNetworkAccessChecker checker(
      ResourceRequest(), factory_params.client_security_state.get(),
      mojom::kURLLoadOptionNone);

  EXPECT_EQ(checker.Check(DirectTransport(PrivateEndpoint())),
            Result::kAllowedNoLessPublic);

  histogram_tester.ExpectUniqueSample(kCheckResultHistogramName,
                                      Result::kAllowedNoLessPublic, 1);
}

TEST(PrivateNetworkAccessCheckerTest, CheckAllowedByPolicyAllow) {
  base::HistogramTester histogram_tester;

  auto factory_params = mojom::URLLoaderFactoryParams::New();
  factory_params->client_security_state = mojom::ClientSecurityState::New();
  factory_params->client_security_state->ip_address_space =
      mojom::IPAddressSpace::kPublic;
  factory_params->client_security_state->private_network_request_policy =
      mojom::PrivateNetworkRequestPolicy::kAllow;

  PrivateNetworkAccessChecker checker(
      ResourceRequest(), factory_params->client_security_state.get(),
      mojom::kURLLoadOptionNone);

  EXPECT_EQ(checker.Check(DirectTransport(PrivateEndpoint())),
            Result::kAllowedByPolicyAllow);

  histogram_tester.ExpectUniqueSample(kCheckResultHistogramName,
                                      Result::kAllowedByPolicyAllow, 1);
}

TEST(PrivateNetworkAccessCheckerTest,
     CheckAllowedByPolicyWarnInconsistentIpAddressSpace) {
  base::HistogramTester histogram_tester;

  auto factory_params = mojom::URLLoaderFactoryParams::New();
  factory_params->client_security_state = mojom::ClientSecurityState::New();
  factory_params->client_security_state->ip_address_space =
      mojom::IPAddressSpace::kPublic;
  factory_params->client_security_state->private_network_request_policy =
      mojom::PrivateNetworkRequestPolicy::kWarn;

  PrivateNetworkAccessChecker checker(
      ResourceRequest(), factory_params->client_security_state.get(),
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

  auto factory_params = mojom::URLLoaderFactoryParams::New();
  factory_params->client_security_state = mojom::ClientSecurityState::New();
  factory_params->client_security_state->ip_address_space =
      mojom::IPAddressSpace::kPublic;
  factory_params->client_security_state->private_network_request_policy =
      mojom::PrivateNetworkRequestPolicy::kAllow;

  PrivateNetworkAccessChecker checker(
      ResourceRequest(), factory_params->client_security_state.get(),
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

  auto factory_params = mojom::URLLoaderFactoryParams::New();
  factory_params->client_security_state = mojom::ClientSecurityState::New();
  factory_params->client_security_state->ip_address_space =
      mojom::IPAddressSpace::kPublic;
  factory_params->client_security_state->private_network_request_policy =
      mojom::PrivateNetworkRequestPolicy::kWarn;

  PrivateNetworkAccessChecker checker(
      ResourceRequest(), factory_params->client_security_state.get(),
      mojom::kURLLoadOptionNone);

  EXPECT_EQ(checker.Check(DirectTransport(PrivateEndpoint())),
            Result::kAllowedByPolicyWarn);

  histogram_tester.ExpectUniqueSample(kCheckResultHistogramName,
                                      Result::kAllowedByPolicyWarn, 1);
}

TEST(PrivateNetworkAccessCheckerTest, CheckBlockedByPolicyBlock) {
  base::HistogramTester histogram_tester;

  auto factory_params = mojom::URLLoaderFactoryParams::New();
  factory_params->client_security_state = mojom::ClientSecurityState::New();
  factory_params->client_security_state->ip_address_space =
      mojom::IPAddressSpace::kPublic;
  factory_params->client_security_state->private_network_request_policy =
      mojom::PrivateNetworkRequestPolicy::kBlock;

  PrivateNetworkAccessChecker checker(
      ResourceRequest(), factory_params->client_security_state.get(),
      mojom::kURLLoadOptionNone);

  EXPECT_EQ(checker.Check(DirectTransport(PrivateEndpoint())),
            Result::kBlockedByPolicyBlock);

  histogram_tester.ExpectUniqueSample(kCheckResultHistogramName,
                                      Result::kBlockedByPolicyBlock, 1);
}

TEST(PrivateNetworkAccessCheckerTest, CheckBlockedByPolicyPreflightWarn) {
  base::HistogramTester histogram_tester;

  auto factory_params = mojom::URLLoaderFactoryParams::New();
  factory_params->client_security_state = mojom::ClientSecurityState::New();
  factory_params->client_security_state->ip_address_space =
      mojom::IPAddressSpace::kPublic;
  factory_params->client_security_state->private_network_request_policy =
      mojom::PrivateNetworkRequestPolicy::kPreflightWarn;

  PrivateNetworkAccessChecker checker(
      ResourceRequest(), factory_params->client_security_state.get(),
      mojom::kURLLoadOptionNone);

  EXPECT_EQ(checker.Check(DirectTransport(PrivateEndpoint())),
            Result::kBlockedByPolicyPreflightWarn);

  histogram_tester.ExpectUniqueSample(kCheckResultHistogramName,
                                      Result::kBlockedByPolicyPreflightWarn, 1);
}

TEST(PrivateNetworkAccessCheckerTest, CheckBlockedByPolicyPreflightBlock) {
  base::HistogramTester histogram_tester;

  auto factory_params = mojom::URLLoaderFactoryParams::New();
  factory_params->client_security_state = mojom::ClientSecurityState::New();
  factory_params->client_security_state->ip_address_space =
      mojom::IPAddressSpace::kPublic;
  factory_params->client_security_state->private_network_request_policy =
      mojom::PrivateNetworkRequestPolicy::kPreflightBlock;

  PrivateNetworkAccessChecker checker(
      ResourceRequest(), factory_params->client_security_state.get(),
      mojom::kURLLoadOptionNone);

  EXPECT_EQ(checker.Check(DirectTransport(PrivateEndpoint())),
            Result::kBlockedByPolicyPreflightBlock);

  histogram_tester.ExpectUniqueSample(
      kCheckResultHistogramName, Result::kBlockedByPolicyPreflightBlock, 1);
}

TEST(PrivateNetworkAccessCheckerTest, CheckBlockedByTargetIpAddressSpace) {
  base::HistogramTester histogram_tester;

  mojom::URLLoaderFactoryParams factory_params;
  factory_params.client_security_state = mojom::ClientSecurityState::New();
  factory_params.client_security_state->ip_address_space =
      mojom::IPAddressSpace::kPublic;
  factory_params.client_security_state->private_network_request_policy =
      mojom::PrivateNetworkRequestPolicy::kPreflightBlock;

  ResourceRequest request;
  request.target_ip_address_space = mojom::IPAddressSpace::kPublic;

  PrivateNetworkAccessChecker checker(
      request, factory_params.client_security_state.get(),
      mojom::kURLLoadOptionNone);

  EXPECT_EQ(checker.Check(DirectTransport(PrivateEndpoint())),
            Result::kBlockedByTargetIpAddressSpace);

  histogram_tester.ExpectUniqueSample(
      kCheckResultHistogramName, Result::kBlockedByTargetIpAddressSpace, 1);
}

TEST(PrivateNetworkAccessCheckerTest, CheckAllowedByPolicyPreflightWarn) {
  base::HistogramTester histogram_tester;

  auto factory_params = mojom::URLLoaderFactoryParams::New();
  factory_params->client_security_state = mojom::ClientSecurityState::New();
  factory_params->client_security_state->ip_address_space =
      mojom::IPAddressSpace::kPublic;
  factory_params->client_security_state->private_network_request_policy =
      mojom::PrivateNetworkRequestPolicy::kPreflightWarn;

  ResourceRequest request;
  request.target_ip_address_space = mojom::IPAddressSpace::kLocal;

  PrivateNetworkAccessChecker checker(
      request, factory_params->client_security_state.get(),
      mojom::kURLLoadOptionNone);

  EXPECT_EQ(checker.Check(DirectTransport(PrivateEndpoint())),
            Result::kAllowedByPolicyPreflightWarn);

  histogram_tester.ExpectUniqueSample(kCheckResultHistogramName,
                                      Result::kAllowedByPolicyPreflightWarn, 1);
}

TEST(PrivateNetworkAccessCheckerTest, CheckAllowedByTargetIpAddressSpace) {
  base::HistogramTester histogram_tester;

  mojom::URLLoaderFactoryParams factory_params;
  factory_params.client_security_state = mojom::ClientSecurityState::New();
  factory_params.client_security_state->ip_address_space =
      mojom::IPAddressSpace::kPublic;
  factory_params.client_security_state->private_network_request_policy =
      mojom::PrivateNetworkRequestPolicy::kPreflightBlock;

  ResourceRequest request;
  request.target_ip_address_space = mojom::IPAddressSpace::kPrivate;

  PrivateNetworkAccessChecker checker(
      request, factory_params.client_security_state.get(),
      mojom::kURLLoadOptionNone);

  EXPECT_EQ(checker.Check(DirectTransport(PrivateEndpoint())),
            Result::kAllowedByTargetIpAddressSpace);

  histogram_tester.ExpectUniqueSample(
      kCheckResultHistogramName, Result::kAllowedByTargetIpAddressSpace, 1);
}

TEST(PrivateNetworkAccessCheckerTest,
     CheckAllowedByPolicyPreflightWarnInconsistent) {
  auto factory_params = mojom::URLLoaderFactoryParams::New();
  factory_params->client_security_state = mojom::ClientSecurityState::New();
  factory_params->client_security_state->ip_address_space =
      mojom::IPAddressSpace::kPublic;
  factory_params->client_security_state->private_network_request_policy =
      mojom::PrivateNetworkRequestPolicy::kPreflightWarn;

  PrivateNetworkAccessChecker checker(
      ResourceRequest(), factory_params->client_security_state.get(),
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
  mojom::URLLoaderFactoryParams factory_params;
  factory_params.client_security_state = mojom::ClientSecurityState::New();
  factory_params.client_security_state->ip_address_space =
      mojom::IPAddressSpace::kPrivate;
  factory_params.client_security_state->private_network_request_policy =
      mojom::PrivateNetworkRequestPolicy::kPreflightBlock;

  PrivateNetworkAccessChecker checker(
      ResourceRequest(), factory_params.client_security_state.get(),
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

TEST(PrivateNetworkAccessCheckerTest, ResponseAddressSpace) {
  mojom::URLLoaderFactoryParams factory_params;

  PrivateNetworkAccessChecker checker(
      ResourceRequest(), factory_params.client_security_state.get(),
      mojom::kURLLoadOptionNone);

  EXPECT_EQ(checker.ResponseAddressSpace(), absl::nullopt);

  checker.Check(DirectTransport(PublicEndpoint()));

  EXPECT_THAT(checker.ResponseAddressSpace(),
              Optional(mojom::IPAddressSpace::kPublic));

  checker.Check(DirectTransport(PrivateEndpoint()));

  EXPECT_THAT(checker.ResponseAddressSpace(),
              Optional(mojom::IPAddressSpace::kPrivate));
}

TEST(PrivateNetworkAccessCheckerTest, ProxiedTransportAddressSpaceIsUnknown) {
  mojom::URLLoaderFactoryParams factory_params;

  PrivateNetworkAccessChecker checker(
      ResourceRequest(), factory_params.client_security_state.get(),
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
  mojom::URLLoaderFactoryParams factory_params;

  PrivateNetworkAccessChecker checker(
      ResourceRequest(), factory_params.client_security_state.get(),
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
  mojom::URLLoaderFactoryParams factory_params;

  PrivateNetworkAccessChecker checker(
      ResourceRequest(), factory_params.client_security_state.get(),
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

TEST(PrivateNetworkAccessCheckerTest, ResetForRedirectTargetAddressSpace) {
  mojom::URLLoaderFactoryParams factory_params;
  factory_params.client_security_state = mojom::ClientSecurityState::New();
  factory_params.client_security_state->ip_address_space =
      mojom::IPAddressSpace::kPrivate;
  factory_params.client_security_state->private_network_request_policy =
      mojom::PrivateNetworkRequestPolicy::kPreflightBlock;

  ResourceRequest request;
  request.target_ip_address_space = mojom::IPAddressSpace::kPublic;

  PrivateNetworkAccessChecker checker(
      request, factory_params.client_security_state.get(),
      mojom::kURLLoadOptionNone);

  checker.Reset();

  // The target address space has been cleared.
  EXPECT_EQ(checker.TargetAddressSpace(), mojom::IPAddressSpace::kUnknown);

  // This succeeds even though the IP address space does not match the target
  // passed at construction time, thanks to `Reset()`.
  EXPECT_EQ(checker.Check(DirectTransport(PrivateEndpoint())),
            Result::kAllowedNoLessPublic);
}

TEST(PrivateNetworkAccessCheckerTest, ResetForRedirectResponseAddressSpace) {
  mojom::URLLoaderFactoryParams factory_params;

  PrivateNetworkAccessChecker checker(
      ResourceRequest(), factory_params.client_security_state.get(),
      mojom::kURLLoadOptionNone);

  checker.Check(DirectTransport(PrivateEndpoint()));

  checker.Reset();

  EXPECT_EQ(checker.ResponseAddressSpace(), absl::nullopt);

  // This succeeds even though the IP address space does not match that of the
  // previous endpoint passed to `Check()`, thanks to `ResetForRedirect()`.
  EXPECT_EQ(checker.Check(DirectTransport(LocalEndpoint())),
            Result::kAllowedMissingClientSecurityState);
}

TEST(PrivateNetworkAccessCheckerTest,
     RecordsMismatchedAddressSpaceHistogramFalse) {
  mojom::URLLoaderFactoryParams factory_params;

  base::HistogramTester histogram_tester;

  {
    PrivateNetworkAccessChecker checker(
        ResourceRequest(), factory_params.client_security_state.get(),
        mojom::kURLLoadOptionNone);

    checker.Check(DirectTransport(PublicEndpoint()));
  }

  histogram_tester.ExpectUniqueSample(kMismatchedAddressSpacesHistogramName,
                                      false, 1);
}

TEST(PrivateNetworkAccessCheckerTest,
     RecordsMismatchedAddressSpaceHistogramTrue) {
  mojom::URLLoaderFactoryParams factory_params;

  base::HistogramTester histogram_tester;

  {
    PrivateNetworkAccessChecker checker(
        ResourceRequest(), factory_params.client_security_state.get(),
        mojom::kURLLoadOptionNone);

    checker.Check(DirectTransport(PublicEndpoint()));
    checker.Check(DirectTransport(PrivateEndpoint()));
    checker.Check(DirectTransport(PublicEndpoint()));
  }

  histogram_tester.ExpectUniqueSample(kMismatchedAddressSpacesHistogramName,
                                      true, 1);
}

TEST(PrivateNetworkAccessCheckerTest,
     RecordsMismatchedAddressSpaceHistogramResetForRedirect) {
  mojom::URLLoaderFactoryParams factory_params;

  base::HistogramTester histogram_tester;

  {
    PrivateNetworkAccessChecker checker(
        ResourceRequest(), factory_params.client_security_state.get(),
        mojom::kURLLoadOptionNone);

    checker.Check(DirectTransport(PublicEndpoint()));
    checker.Reset();
    checker.Check(DirectTransport(PrivateEndpoint()));
  }

  histogram_tester.ExpectUniqueSample(kMismatchedAddressSpacesHistogramName,
                                      false, 1);
}

}  // namespace
}  // namespace network
