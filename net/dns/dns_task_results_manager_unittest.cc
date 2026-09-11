// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/dns_task_results_manager.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <string_view>

#include "base/check.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "net/base/connection_endpoint_metadata.h"
#include "net/base/net_errors.h"
#include "net/dns/host_resolver_internal_result.h"
#include "net/dns/host_resolver_results_test_util.h"
#include "net/dns/https_record_rdata.h"
#include "net/test/test_with_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/scheme_host_port.h"

using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::UnorderedElementsAre;

namespace net {

namespace {

class FakeDelegate : public DnsTaskResultsManager::Delegate {
 public:
  FakeDelegate() = default;
  ~FakeDelegate() override = default;

  size_t update_count() const { return update_count_; }

 private:
  void OnServiceEndpointsUpdated() override { ++update_count_; }

  size_t update_count_ = 0;
};

IPEndPoint MakeIPEndPoint(std::string_view ip_literal, uint16_t port = 0) {
  std::optional<IPAddress> ip = IPAddress::FromIPLiteral(std::move(ip_literal));
  return IPEndPoint(*ip, port);
}

std::unique_ptr<HostResolverInternalDataResult> CreateDataResult(
    std::string_view domain_name,
    std::vector<IPEndPoint> ip_endpoints,
    DnsQueryType query_type) {
  return std::make_unique<HostResolverInternalDataResult>(
      std::string(domain_name), query_type, /*expiration=*/base::TimeTicks(),
      /*timed_expiration=*/base::Time(),
      HostResolverInternalResult::Source::kDns, std::move(ip_endpoints),
      std::vector<std::string>(), std::vector<HostPortPair>());
}

std::unique_ptr<HostResolverInternalErrorResult> CreateNoData(
    std::string_view domain_name,
    DnsQueryType query_type) {
  return std::make_unique<HostResolverInternalErrorResult>(
      std::string(domain_name), query_type, /*expiration=*/base::TimeTicks(),
      /*timed_expiration=*/base::Time(),
      HostResolverInternalResult::Source::kDns, ERR_NAME_NOT_RESOLVED);
}

std::unique_ptr<HostResolverInternalMetadataResult> CreateMetadata(
    std::string_view domain_name,
    std::multimap<HttpsRecordPriority, ConnectionEndpointMetadata> metadatas,
    HostResolverInternalMetadataResult::AddressHintsMap address_hints = {}) {
  return std::make_unique<HostResolverInternalMetadataResult>(
      std::string(domain_name), DnsQueryType::HTTPS,
      /*expiration=*/base::TimeTicks(), /*timed_expiration=*/base::Time(),
      HostResolverInternalResult::Source::kDns, std::move(metadatas),
      std::move(address_hints));
}

HostResolverInternalMetadataResult::AddressHints MakeAddressHints(
    const std::vector<std::string_view>& ipv4_literals,
    const std::vector<std::string_view>& ipv6_literals) {
  HostResolverInternalMetadataResult::AddressHints hints;
  for (std::string_view literal : ipv4_literals) {
    hints.ipv4_hints.insert(*IPAddress::FromIPLiteral(literal));
  }
  for (std::string_view literal : ipv6_literals) {
    hints.ipv6_hints.insert(*IPAddress::FromIPLiteral(literal));
  }
  return hints;
}

std::unique_ptr<HostResolverInternalAliasResult> CreateAlias(
    std::string_view domain_name,
    DnsQueryType query_type,
    std::string_view alias_target) {
  return std::make_unique<HostResolverInternalAliasResult>(
      std::string(domain_name), query_type, /*expiration=*/base::TimeTicks(),
      /*timed_expiration=*/base::Time(),
      HostResolverInternalResult::Source::kDns, std::string(alias_target));
}

std::vector<IPEndPoint> WithPort(const std::vector<IPEndPoint>& endpoints,
                                 uint16_t port) {
  std::vector<IPEndPoint> out_endpoints;
  for (const auto& endpoint : endpoints) {
    out_endpoints.emplace_back(endpoint.address(), port);
  }
  return out_endpoints;
}

static constexpr std::string_view kHostName = "www.example.com";
static constexpr std::string_view kAliasTarget1 = "alias1.example.net";
static constexpr std::string_view kAliasTarget2 = "alias2.example.net";

static const ConnectionEndpointMetadata kMetadata1(
    /*supported_protocol_alpns=*/{"h3"},
    /*ech_config_list=*/{},
    std::string(kHostName),
    {});

static const ConnectionEndpointMetadata kMetadata2(
    /*supported_protocol_alpns=*/{"h2", "http/1.1"},
    /*ech_config_list=*/{},
    std::string(kHostName),
    {});

static const std::multimap<HttpsRecordPriority, ConnectionEndpointMetadata>
    kMetadatas{{1, kMetadata1}, {2, kMetadata2}, {}};

// A helper class to create a DnsTaskResultsManager.
class ManagerFactory {
 public:
  explicit ManagerFactory(DnsTaskResultsManager::Delegate* delegate)
      : delegate_(delegate),
        host_(
            HostResolver::Host(url::SchemeHostPort("https", kHostName, 443))) {}

  std::unique_ptr<DnsTaskResultsManager> Create() {
    return std::make_unique<DnsTaskResultsManager>(
        delegate_, host_, query_types_, NetLogWithSource());
  }

  ManagerFactory& query_types(DnsQueryTypeSet query_types) {
    query_types_ = query_types;
    return *this;
  }

 private:
  raw_ptr<DnsTaskResultsManager::Delegate> delegate_;
  HostResolver::Host host_;
  DnsQueryTypeSet query_types_ = {DnsQueryType::A, DnsQueryType::AAAA,
                                  DnsQueryType::HTTPS};
};

}  // namespace

class DnsTaskResultsManagerTest : public TestWithTaskEnvironment {
 public:
  DnsTaskResultsManagerTest()
      : TestWithTaskEnvironment(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override { delegate_ = std::make_unique<FakeDelegate>(); }

 protected:
  ManagerFactory factory() { return ManagerFactory(delegate_.get()); }

  FakeDelegate* delegate() { return delegate_.get(); }

 private:
  std::unique_ptr<FakeDelegate> delegate_;
};

TEST_F(DnsTaskResultsManagerTest, IsMetadataReady) {
  // HTTPS RR is not queried.
  std::unique_ptr<DnsTaskResultsManager> manager =
      factory().query_types({DnsQueryType::A, DnsQueryType::AAAA}).Create();
  ASSERT_TRUE(manager->IsMetadataReady());

  // HTTPS RR is queried.
  manager = factory()
                .query_types(
                    {DnsQueryType::A, DnsQueryType::AAAA, DnsQueryType::HTTPS})
                .Create();
  ASSERT_FALSE(manager->IsMetadataReady());

  manager->ProcessDnsTransactionResults(DnsQueryType::HTTPS, /*results=*/{});
  ASSERT_TRUE(manager->IsMetadataReady());
}

TEST_F(DnsTaskResultsManagerTest, IPv6NotQueried) {
  std::unique_ptr<DnsTaskResultsManager> manager =
      factory().query_types({DnsQueryType::A, DnsQueryType::HTTPS}).Create();

  std::unique_ptr<HostResolverInternalResult> result = CreateDataResult(
      kHostName, {MakeIPEndPoint("192.0.2.1")}, DnsQueryType::A);
  manager->ProcessDnsTransactionResults(DnsQueryType::A, {result.get()});

  EXPECT_THAT(manager->GetCurrentEndpoints(),
              ElementsAre(ExpectServiceEndpoint(
                  ElementsAre(MakeIPEndPoint("192.0.2.1", 443)))));
  EXPECT_THAT(manager->GetAliases(), UnorderedElementsAre(kHostName));
}

TEST_F(DnsTaskResultsManagerTest, IPv4First) {
  std::unique_ptr<DnsTaskResultsManager> manager = factory().Create();

  // A comes first. Service endpoints creation should be delayed.
  std::unique_ptr<HostResolverInternalResult> result1 = CreateDataResult(
      kHostName, {MakeIPEndPoint("192.0.2.1")}, DnsQueryType::A);
  manager->ProcessDnsTransactionResults(DnsQueryType::A, {result1.get()});

  ASSERT_TRUE(manager->GetCurrentEndpoints().empty());

  // AAAA is responded. Service endpoints should be available.
  std::unique_ptr<HostResolverInternalResult> result2 = CreateDataResult(
      kHostName, {MakeIPEndPoint("2001:db8::1")}, DnsQueryType::AAAA);
  manager->ProcessDnsTransactionResults(DnsQueryType::AAAA, {result2.get()});

  EXPECT_THAT(manager->GetCurrentEndpoints(),
              ElementsAre(ExpectServiceEndpoint(
                  ElementsAre(MakeIPEndPoint("192.0.2.1", 443)),
                  ElementsAre(MakeIPEndPoint("2001:db8::1", 443)))));
}

TEST_F(DnsTaskResultsManagerTest, IPv6First) {
  std::unique_ptr<DnsTaskResultsManager> manager = factory().Create();

  // AAAA comes first. Service endpoints should be available immediately.
  std::unique_ptr<HostResolverInternalResult> result1 = CreateDataResult(
      kHostName, {MakeIPEndPoint("2001:db8::1")}, DnsQueryType::AAAA);
  manager->ProcessDnsTransactionResults(DnsQueryType::AAAA, {result1.get()});

  EXPECT_THAT(manager->GetCurrentEndpoints(),
              ElementsAre(ExpectServiceEndpoint(
                  IsEmpty(), ElementsAre(MakeIPEndPoint("2001:db8::1", 443)))));

  // A is responded. Service endpoints should be updated.
  std::unique_ptr<HostResolverInternalResult> result2 = CreateDataResult(
      kHostName, {MakeIPEndPoint("192.0.2.1"), MakeIPEndPoint("192.0.2.2")},
      DnsQueryType::A);
  manager->ProcessDnsTransactionResults(DnsQueryType::A, {result2.get()});

  EXPECT_THAT(manager->GetCurrentEndpoints(),
              ElementsAre(ExpectServiceEndpoint(
                  ElementsAre(MakeIPEndPoint("192.0.2.1", 443),
                              MakeIPEndPoint("192.0.2.2", 443)),
                  ElementsAre(MakeIPEndPoint("2001:db8::1", 443)))));
}

TEST_F(DnsTaskResultsManagerTest, IPv6Timedout) {
  std::unique_ptr<DnsTaskResultsManager> manager = factory().Create();

  // A comes first. Service endpoints creation should be delayed.
  std::unique_ptr<HostResolverInternalResult> result1 = CreateDataResult(
      kHostName, {MakeIPEndPoint("192.0.2.1")}, DnsQueryType::A);
  manager->ProcessDnsTransactionResults(DnsQueryType::A, {result1.get()});

  ASSERT_TRUE(manager->GetCurrentEndpoints().empty());

  // AAAA is timed out. Service endpoints should be available after timeout.
  FastForwardBy(DnsTaskResultsManager::GetResolutionDelay() +
                base::Milliseconds(1));

  EXPECT_THAT(manager->GetCurrentEndpoints(),
              ElementsAre(ExpectServiceEndpoint(
                  ElementsAre(MakeIPEndPoint("192.0.2.1", 443)))));

  // AAAA is responded after timeout. Service endpoints should be updated.
  std::unique_ptr<HostResolverInternalResult> result2 = CreateDataResult(
      kHostName, {MakeIPEndPoint("2001:db8::1")}, DnsQueryType::AAAA);
  manager->ProcessDnsTransactionResults(DnsQueryType::AAAA, {result2.get()});

  EXPECT_THAT(manager->GetCurrentEndpoints(),
              ElementsAre(ExpectServiceEndpoint(
                  ElementsAre(MakeIPEndPoint("192.0.2.1", 443)),
                  ElementsAre(MakeIPEndPoint("2001:db8::1", 443)))));
}

TEST_F(DnsTaskResultsManagerTest, IPv6NoDataBeforeIPv4) {
  std::unique_ptr<DnsTaskResultsManager> manager = factory().Create();

  // AAAA is responded with no data. Service endpoints should not be available.
  std::unique_ptr<HostResolverInternalResult> result1 =
      CreateNoData(kHostName, DnsQueryType::AAAA);
  manager->ProcessDnsTransactionResults(DnsQueryType::AAAA, {result1.get()});

  ASSERT_TRUE(manager->GetCurrentEndpoints().empty());

  // A is responded. Service endpoints creation should happen without resolution
  // delay.
  std::unique_ptr<HostResolverInternalResult> result2 = CreateDataResult(
      kHostName, {MakeIPEndPoint("192.0.2.1")}, DnsQueryType::A);
  manager->ProcessDnsTransactionResults(DnsQueryType::A, {result2.get()});

  EXPECT_THAT(manager->GetCurrentEndpoints(),
              ElementsAre(ExpectServiceEndpoint(
                  ElementsAre(MakeIPEndPoint("192.0.2.1", 443)))));
}

TEST_F(DnsTaskResultsManagerTest, IPv6NoDataAfterIPv4) {
  std::unique_ptr<DnsTaskResultsManager> manager = factory().Create();

  // A is responded. Service endpoints creation should be delayed.
  std::unique_ptr<HostResolverInternalResult> result1 = CreateDataResult(
      kHostName, {MakeIPEndPoint("192.0.2.1")}, DnsQueryType::A);
  manager->ProcessDnsTransactionResults(DnsQueryType::A, {result1.get()});

  ASSERT_TRUE(manager->GetCurrentEndpoints().empty());

  // AAAA is responded with no data before the resolution delay timer. Service
  // endpoints should be available without waiting for the timeout.
  std::unique_ptr<HostResolverInternalResult> result2 =
      CreateNoData(kHostName, DnsQueryType::AAAA);
  manager->ProcessDnsTransactionResults(DnsQueryType::AAAA, {result2.get()});

  EXPECT_THAT(manager->GetCurrentEndpoints(),
              ElementsAre(ExpectServiceEndpoint(
                  ElementsAre(MakeIPEndPoint("192.0.2.1", 443)))));
}

TEST_F(DnsTaskResultsManagerTest, IPv6EmptyDataAfterIPv4) {
  std::unique_ptr<DnsTaskResultsManager> manager = factory().Create();

  // A is responded. Service endpoints creation should be delayed.
  std::unique_ptr<HostResolverInternalResult> result1 = CreateDataResult(
      kHostName, {MakeIPEndPoint("192.0.2.1")}, DnsQueryType::A);
  manager->ProcessDnsTransactionResults(DnsQueryType::A, {result1.get()});

  ASSERT_TRUE(manager->GetCurrentEndpoints().empty());

  // AAAA is responded with a non-cacheable result (an empty result) before the
  // resolution delay timer. Service endpoints should be available without
  // waiting for the timeout.
  manager->ProcessDnsTransactionResults(DnsQueryType::AAAA, {});

  EXPECT_THAT(manager->GetCurrentEndpoints(),
              ElementsAre(ExpectServiceEndpoint(
                  ElementsAre(MakeIPEndPoint("192.0.2.1", 443)))));
}

TEST_F(DnsTaskResultsManagerTest, IPv4AndIPv6NoData) {
  std::unique_ptr<DnsTaskResultsManager> manager = factory().Create();

  // AAAA is responded with no data. Service endpoints should not be available.
  std::unique_ptr<HostResolverInternalResult> result1 =
      CreateNoData(kHostName, DnsQueryType::AAAA);
  manager->ProcessDnsTransactionResults(DnsQueryType::AAAA, {result1.get()});

  ASSERT_TRUE(manager->GetCurrentEndpoints().empty());

  // A is responded with no data. Service endpoints should not be available.
  std::unique_ptr<HostResolverInternalResult> result2 =
      CreateNoData(kHostName, DnsQueryType::A);
  manager->ProcessDnsTransactionResults(DnsQueryType::A, {result2.get()});

  ASSERT_TRUE(manager->GetCurrentEndpoints().empty());
}

TEST_F(DnsTaskResultsManagerTest, IPv4NoDataIPv6AfterResolutionDelay) {
  std::unique_ptr<DnsTaskResultsManager> manager = factory().Create();

  // A comes first with no data. Service endpoints creation should be delayed
  // and the resolution delay timer should not start.
  std::unique_ptr<HostResolverInternalResult> result1 =
      CreateNoData(kHostName, DnsQueryType::A);
  manager->ProcessDnsTransactionResults(DnsQueryType::A, {result1.get()});

  ASSERT_FALSE(manager->IsResolutionDelayTimerRunningForTest());
  ASSERT_TRUE(manager->GetCurrentEndpoints().empty());

  // The resolution delay passed. Service endpoints should not be available yet.
  FastForwardBy(DnsTaskResultsManager::GetResolutionDelay() +
                base::Milliseconds(1));

  ASSERT_TRUE(manager->GetCurrentEndpoints().empty());

  // AAAA is responded. Service endpoints should be updated.
  std::unique_ptr<HostResolverInternalResult> result2 = CreateDataResult(
      kHostName, {MakeIPEndPoint("2001:db8::1")}, DnsQueryType::AAAA);
  manager->ProcessDnsTransactionResults(DnsQueryType::AAAA, {result2.get()});

  EXPECT_THAT(manager->GetCurrentEndpoints(),
              ElementsAre(ExpectServiceEndpoint(
                  IsEmpty(), ElementsAre(MakeIPEndPoint("2001:db8::1", 443)))));
}

TEST_F(DnsTaskResultsManagerTest, MetadataFirst) {
  std::unique_ptr<DnsTaskResultsManager> manager = factory().Create();

  // HTTPS comes first. Service endpoints should not be available yet since
  // the HTTPS response has no address hints.
  std::unique_ptr<HostResolverInternalResult> result1 =
      CreateMetadata(kHostName, kMetadatas);
  manager->ProcessDnsTransactionResults(DnsQueryType::HTTPS, {result1.get()});

  ASSERT_TRUE(manager->GetCurrentEndpoints().empty());
  ASSERT_TRUE(manager->IsMetadataReady());

  // A is responded. Service endpoints creation should be delayed.
  std::unique_ptr<HostResolverInternalResult> result2 = CreateDataResult(
      kHostName, {MakeIPEndPoint("192.0.2.1")}, DnsQueryType::A);
  manager->ProcessDnsTransactionResults(DnsQueryType::A, {result2.get()});

  ASSERT_TRUE(manager->GetCurrentEndpoints().empty());

  // AAAA is responded. Service endpoints should be available with metadatas.
  std::unique_ptr<HostResolverInternalResult> result3 = CreateDataResult(
      kHostName, {MakeIPEndPoint("2001:db8::1")}, DnsQueryType::AAAA);
  manager->ProcessDnsTransactionResults(DnsQueryType::AAAA, {result3.get()});

  EXPECT_THAT(
      manager->GetCurrentEndpoints(),
      ElementsAre(
          ExpectServiceEndpoint(ElementsAre(MakeIPEndPoint("192.0.2.1", 443)),
                                ElementsAre(MakeIPEndPoint("2001:db8::1", 443)),
                                kMetadata1),
          ExpectServiceEndpoint(ElementsAre(MakeIPEndPoint("192.0.2.1", 443)),
                                ElementsAre(MakeIPEndPoint("2001:db8::1", 443)),
                                kMetadata2)));
}

TEST_F(DnsTaskResultsManagerTest, MetadataDifferentTargetName) {
  std::unique_ptr<DnsTaskResultsManager> manager = factory().Create();

  // HTTPS is responded and the target name is different from QNAME.
  const ConnectionEndpointMetadata kMetadataDifferentTargetName(
      /*supported_protocol_alpns=*/{"h2", "http/1.1"},
      /*ech_config_list=*/{},
      /*target_name=*/"other.example.net.", {});
  std::unique_ptr<HostResolverInternalResult> result1 =
      CreateMetadata(kHostName, {{1, kMetadataDifferentTargetName}});
  manager->ProcessDnsTransactionResults(DnsQueryType::HTTPS, {result1.get()});

  ASSERT_TRUE(manager->IsMetadataReady());

  // AAAA is responded. Service endpoints should be available without metadatas
  // since the target name is different.
  std::unique_ptr<HostResolverInternalResult> result2 = CreateDataResult(
      kHostName, {MakeIPEndPoint("2001:db8::1")}, DnsQueryType::AAAA);
  manager->ProcessDnsTransactionResults(DnsQueryType::AAAA, {result2.get()});

  ASSERT_TRUE(manager->IsMetadataReady());
  EXPECT_THAT(manager->GetCurrentEndpoints(),
              ElementsAre(ExpectServiceEndpoint(
                  IsEmpty(), ElementsAre(MakeIPEndPoint("2001:db8::1", 443)))));
}

TEST_F(DnsTaskResultsManagerTest, MetadataAfterIPv6) {
  std::unique_ptr<DnsTaskResultsManager> manager = factory().Create();

  // AAAA comes first. Service endpoints should be available without metadatas.
  std::unique_ptr<HostResolverInternalResult> result1 = CreateDataResult(
      kHostName, {MakeIPEndPoint("2001:db8::1")}, DnsQueryType::AAAA);
  manager->ProcessDnsTransactionResults(DnsQueryType::AAAA, {result1.get()});

  ASSERT_FALSE(manager->IsMetadataReady());
  EXPECT_THAT(manager->GetCurrentEndpoints(),
              ElementsAre(ExpectServiceEndpoint(
                  IsEmpty(), ElementsAre(MakeIPEndPoint("2001:db8::1", 443)))));

  // HTTPS is responded. Metadata should be available.
  std::unique_ptr<HostResolverInternalResult> result2 =
      CreateMetadata(kHostName, kMetadatas);
  manager->ProcessDnsTransactionResults(DnsQueryType::HTTPS, {result2.get()});

  ASSERT_TRUE(manager->IsMetadataReady());
  EXPECT_THAT(
      manager->GetCurrentEndpoints(),
      ElementsAre(
          ExpectServiceEndpoint(IsEmpty(),
                                ElementsAre(MakeIPEndPoint("2001:db8::1", 443)),
                                kMetadata1),
          ExpectServiceEndpoint(IsEmpty(),
                                ElementsAre(MakeIPEndPoint("2001:db8::1", 443)),
                                kMetadata2)));
}

TEST_F(DnsTaskResultsManagerTest, IPv6TimedoutAfterMetadata) {
  std::unique_ptr<DnsTaskResultsManager> manager = factory().Create();

  // A comes first. Service endpoints creation should be delayed.
  std::unique_ptr<HostResolverInternalResult> result1 = CreateDataResult(
      kHostName, {MakeIPEndPoint("192.0.2.1")}, DnsQueryType::A);
  manager->ProcessDnsTransactionResults(DnsQueryType::A, {result1.get()});

  ASSERT_FALSE(manager->IsMetadataReady());
  ASSERT_TRUE(manager->GetCurrentEndpoints().empty());

  // HTTPS is responded without address hints. Service endpoints should not be
  // available because the manager is waiting for the resolution delay.
  std::unique_ptr<HostResolverInternalResult> result2 =
      CreateMetadata(kHostName, kMetadatas);
  manager->ProcessDnsTransactionResults(DnsQueryType::HTTPS, {result2.get()});

  ASSERT_TRUE(manager->IsMetadataReady());
  ASSERT_TRUE(manager->GetCurrentEndpoints().empty());

  // AAAA is timed out. Service endpoints should be available with metadatas.
  FastForwardBy(DnsTaskResultsManager::GetResolutionDelay() +
                base::Milliseconds(1));

  ASSERT_TRUE(manager->IsMetadataReady());
  EXPECT_THAT(
      manager->GetCurrentEndpoints(),
      ElementsAre(
          ExpectServiceEndpoint(ElementsAre(MakeIPEndPoint("192.0.2.1", 443)),
                                IsEmpty(), kMetadata1),
          ExpectServiceEndpoint(ElementsAre(MakeIPEndPoint("192.0.2.1", 443)),
                                IsEmpty(), kMetadata2)));
}

TEST_F(DnsTaskResultsManagerTest, MetadataAfterIpv6Timeout) {
  std::unique_ptr<DnsTaskResultsManager> manager = factory().Create();

  // A comes first. Service endpoints creation should be delayed.
  std::unique_ptr<HostResolverInternalResult> result1 = CreateDataResult(
      kHostName, {MakeIPEndPoint("192.0.2.1")}, DnsQueryType::A);
  manager->ProcessDnsTransactionResults(DnsQueryType::A, {result1.get()});

  ASSERT_FALSE(manager->IsMetadataReady());
  ASSERT_TRUE(manager->GetCurrentEndpoints().empty());

  // AAAA is timed out. Service endpoints should be available without metadatas.
  FastForwardBy(DnsTaskResultsManager::GetResolutionDelay() +
                base::Milliseconds(1));

  ASSERT_FALSE(manager->IsMetadataReady());
  EXPECT_THAT(manager->GetCurrentEndpoints(),
              ElementsAre(ExpectServiceEndpoint(
                  ElementsAre(MakeIPEndPoint("192.0.2.1", 443)))));

  // HTTPS is responded after timeout. Service endpoints should be updated.
  std::unique_ptr<HostResolverInternalResult> result2 =
      CreateMetadata(kHostName, kMetadatas);
  manager->ProcessDnsTransactionResults(DnsQueryType::HTTPS, {result2.get()});

  ASSERT_TRUE(manager->IsMetadataReady());
  EXPECT_THAT(
      manager->GetCurrentEndpoints(),
      ElementsAre(
          ExpectServiceEndpoint(ElementsAre(MakeIPEndPoint("192.0.2.1", 443)),
                                IsEmpty(), kMetadata1),
          ExpectServiceEndpoint(ElementsAre(MakeIPEndPoint("192.0.2.1", 443)),
                                IsEmpty(), kMetadata2)));
}

TEST_F(DnsTaskResultsManagerTest, IPv4NoDataIPv6TimedoutAfterMetadata) {
  std::unique_ptr<DnsTaskResultsManager> manager = factory().Create();

  // HTTPS is responded without address hints. Service endpoints should not be
  // available because the manager is waiting for the resolution delay.
  std::unique_ptr<HostResolverInternalResult> result1 =
      CreateMetadata(kHostName, kMetadatas);
  manager->ProcessDnsTransactionResults(DnsQueryType::HTTPS, {result1.get()});

  ASSERT_TRUE(manager->IsMetadataReady());
  ASSERT_TRUE(manager->GetCurrentEndpoints().empty());

  // A is responded with no address. Service endpoints should not be available
  // since there are no addresses.
  std::unique_ptr<HostResolverInternalResult> result2 =
      CreateNoData(kHostName, DnsQueryType::A);
  manager->ProcessDnsTransactionResults(DnsQueryType::A, {result2.get()});

  ASSERT_TRUE(manager->GetCurrentEndpoints().empty());

  // AAAA is timed out. Service endpoints should not be available since there
  // are no addresses.
  FastForwardBy(DnsTaskResultsManager::GetResolutionDelay() +
                base::Milliseconds(1));

  ASSERT_TRUE(manager->GetCurrentEndpoints().empty());
}

TEST_F(DnsTaskResultsManagerTest, EndpointOrdering) {
  // Has both IPv4/v6 addresses and metadata.
  constexpr static const std::string_view kSvcbHost1 = "svc1.example.com";
  // Has both IPv4/v6 addresses but no metadata.
  constexpr static const std::string_view kSvcbHost2 = "svc2.example.com";
  // Only has IPv4 addresses.
  constexpr static const std::string_view kSvcbHost3 = "svc3.example.com";

  const std::vector<IPEndPoint> kSvcbHost1IPv4s = {MakeIPEndPoint("192.0.2.1")};
  const std::vector<IPEndPoint> kSvcbHost2IPv4s = {MakeIPEndPoint("192.0.2.2")};
  const std::vector<IPEndPoint> kSvcbHost3IPv4s = {MakeIPEndPoint("192.0.2.3")};

  const std::vector<IPEndPoint> kSvcbHost1IPv6s = {
      MakeIPEndPoint("2001:db8::1")};
  const std::vector<IPEndPoint> kSvcbHost2IPv6s = {
      MakeIPEndPoint("2001:db8::2")};

  const ConnectionEndpointMetadata kSvcbHost1Metadata1(
      /*supported_protocol_alpns=*/{"h2", "http/1.1"},
      /*ech_config_list=*/{},
      /*target_name=*/std::string(kSvcbHost1), {});
  const ConnectionEndpointMetadata kSvcbHost1Metadata2(
      /*supported_protocol_alpns=*/{"h3"},
      /*ech_config_list=*/{},
      /*target_name=*/std::string(kSvcbHost1), {});

  const std::multimap<HttpsRecordPriority, ConnectionEndpointMetadata>
      kSvcbHost1Metadatas{{1, kSvcbHost1Metadata1}, {2, kSvcbHost1Metadata2}};

  struct TestData {
    std::string_view host;
    std::vector<IPEndPoint> ipv4_endpoints;
    std::vector<IPEndPoint> ipv6_endpoints;
    std::multimap<HttpsRecordPriority, ConnectionEndpointMetadata> metadatas;
  };
  const TestData kTestDatas[] = {
      {kSvcbHost1,
       /*ipv4_addresses=*/kSvcbHost1IPv4s,
       /*ipv6_addresses=*/kSvcbHost1IPv6s,
       /*metadatas=*/kSvcbHost1Metadatas},
      {kSvcbHost2,
       /*ipv4_addresses=*/kSvcbHost2IPv4s,
       /*ipv6_addresses=*/kSvcbHost2IPv6s,
       /*metadatas=*/{}},
      {kSvcbHost3, /*ipv4_addresses=*/kSvcbHost3IPv4s,
       /*ipv6_addresses=*/{}, /*metadatas=*/{}},
  };

  std::unique_ptr<DnsTaskResultsManager> manager = factory().Create();

  for (const auto& testdata : kTestDatas) {
    if (!testdata.ipv4_endpoints.empty()) {
      std::unique_ptr<HostResolverInternalResult> result = CreateDataResult(
          testdata.host, testdata.ipv4_endpoints, DnsQueryType::A);
      manager->ProcessDnsTransactionResults(DnsQueryType::A, {result.get()});
    }
    if (!testdata.ipv6_endpoints.empty()) {
      std::unique_ptr<HostResolverInternalResult> result = CreateDataResult(
          testdata.host, testdata.ipv6_endpoints, DnsQueryType::AAAA);
      manager->ProcessDnsTransactionResults(DnsQueryType::AAAA, {result.get()});
    }
    if (!testdata.metadatas.empty()) {
      std::unique_ptr<HostResolverInternalResult> result =
          CreateMetadata(testdata.host, testdata.metadatas);
      manager->ProcessDnsTransactionResults(DnsQueryType::HTTPS,
                                            {result.get()});
    }
  }

  const std::vector<ServiceEndpoint> kExpects = {
      ServiceEndpoint(WithPort(kSvcbHost1IPv4s, 443),
                      WithPort(kSvcbHost1IPv6s, 443), kSvcbHost1Metadata1),
      ServiceEndpoint(WithPort(kSvcbHost1IPv4s, 443),
                      WithPort(kSvcbHost1IPv6s, 443), kSvcbHost1Metadata2),
      ServiceEndpoint(WithPort(kSvcbHost2IPv4s, 443),
                      WithPort(kSvcbHost2IPv6s, 443),
                      ConnectionEndpointMetadata()),
      ServiceEndpoint(WithPort(kSvcbHost3IPv4s, 443), {},
                      ConnectionEndpointMetadata()),
  };

  ASSERT_EQ(manager->GetCurrentEndpoints().size(), kExpects.size());
  for (size_t i = 0; i < manager->GetCurrentEndpoints().size(); ++i) {
    SCOPED_TRACE(i);
    EXPECT_THAT(manager->GetCurrentEndpoints()[i], kExpects[i]);
  }
}

TEST_F(DnsTaskResultsManagerTest, Aliases) {
  std::unique_ptr<DnsTaskResultsManager> manager = factory().Create();

  // AAAA is responded with aliases.
  std::unique_ptr<HostResolverInternalResult> result1 =
      CreateAlias(kHostName, DnsQueryType::AAAA, kAliasTarget1);
  std::unique_ptr<HostResolverInternalResult> result2 =
      CreateAlias(kAliasTarget1, DnsQueryType::AAAA, kAliasTarget2);
  std::unique_ptr<HostResolverInternalResult> result3 = CreateDataResult(
      kHostName, {MakeIPEndPoint("2001:db8::1")}, DnsQueryType::AAAA);
  manager->ProcessDnsTransactionResults(
      DnsQueryType::AAAA, {result1.get(), result2.get(), result3.get()});

  EXPECT_THAT(manager->GetCurrentEndpoints(),
              ElementsAre(ExpectServiceEndpoint(
                  IsEmpty(), ElementsAre(MakeIPEndPoint("2001:db8::1", 443)))));
  EXPECT_THAT(manager->GetAliases(),
              UnorderedElementsAre(kHostName, kAliasTarget1, kAliasTarget2));
}

// Regression test for crbug.com/369232963. An IPv4 mapped IPv6 address should
// be handled without crashing.
TEST_F(DnsTaskResultsManagerTest, Ipv4MappedIpv6) {
  std::unique_ptr<DnsTaskResultsManager> manager = factory().Create();

  auto ip_address = *IPAddress::FromIPLiteral("::ffff:192.0.2.1");
  IPEndPoint endpoint(ConvertIPv4MappedIPv6ToIPv4(ip_address), /*port=*/0);
  std::unique_ptr<HostResolverInternalResult> result =
      CreateDataResult(kHostName, {endpoint}, DnsQueryType::AAAA);
  manager->ProcessDnsTransactionResults(DnsQueryType::AAAA, {result.get()});
  EXPECT_THAT(manager->GetCurrentEndpoints(),
              ElementsAre(ExpectServiceEndpoint(
                  ElementsAre(MakeIPEndPoint("192.0.2.1", 443)), IsEmpty())));
}

TEST_F(DnsTaskResultsManagerTest,
       AliasesAreFixedUpWhenProcessingDnsTransactionResults) {
  std::unique_ptr<DnsTaskResultsManager> manager = factory().Create();
  std::string_view bad_alias = "bad_alias.1";
  std::string_view good_alias = "good_alias";
  std::string_view funky_alias = "GOOGLE.TeSt";

  // AAAA is responded with aliases.
  std::unique_ptr<HostResolverInternalResult> result1 =
      CreateAlias(kHostName, DnsQueryType::AAAA, kAliasTarget1);
  std::unique_ptr<HostResolverInternalResult> result2 =
      CreateAlias(kAliasTarget1, DnsQueryType::AAAA, kAliasTarget2);
  std::unique_ptr<HostResolverInternalResult> result3 =
      CreateAlias(kAliasTarget2, DnsQueryType::AAAA, bad_alias);
  std::unique_ptr<HostResolverInternalResult> result4 =
      CreateAlias(bad_alias, DnsQueryType::AAAA, good_alias);
  std::unique_ptr<HostResolverInternalResult> result5 =
      CreateAlias(good_alias, DnsQueryType::AAAA, funky_alias);
  std::unique_ptr<HostResolverInternalResult> result6 = CreateDataResult(
      kHostName, {MakeIPEndPoint("2001:db8::1")}, DnsQueryType::AAAA);
  manager->ProcessDnsTransactionResults(
      DnsQueryType::AAAA, {result1.get(), result2.get(), result3.get(),
                           result4.get(), result5.get(), result6.get()});

  // Ensure bad_alias is removed, good_alias remains in the set, and funky_alias
  // capitalization changes due to URL canonicalization.
  EXPECT_THAT(manager->GetCurrentEndpoints(),
              ElementsAre(ExpectServiceEndpoint(
                  IsEmpty(), ElementsAre(MakeIPEndPoint("2001:db8::1", 443)))));
  EXPECT_THAT(manager->GetAliases(),
              UnorderedElementsAre(kHostName, kAliasTarget1, kAliasTarget2,
                                   good_alias, "google.test"));
  EXPECT_TRUE(manager->GetAliases().find(std::string(bad_alias)) ==
              manager->GetAliases().end());
}

TEST_F(DnsTaskResultsManagerTest, AddressHintsFirst) {
  std::unique_ptr<DnsTaskResultsManager> manager = factory().Create();

  // HTTPS comes first with address hints. Service endpoints should be
  // available immediately from the hints. Duplicated hints should be merged.
  HostResolverInternalMetadataResult::AddressHintsMap address_hints;
  address_hints[std::string(kHostName)] =
      MakeAddressHints({"192.0.2.10", "192.0.2.10"}, {"2001:db8::10"});
  std::unique_ptr<HostResolverInternalResult> result =
      CreateMetadata(kHostName, kMetadatas, std::move(address_hints));
  manager->ProcessDnsTransactionResults(DnsQueryType::HTTPS, {result.get()});

  ASSERT_TRUE(manager->IsMetadataReady());
  EXPECT_THAT(
      manager->GetCurrentEndpoints(),
      ElementsAre(
          ExpectServiceEndpoint(
              ElementsAre(MakeIPEndPoint("192.0.2.10", 443)),
              ElementsAre(MakeIPEndPoint("2001:db8::10", 443)), kMetadata1),
          ExpectServiceEndpoint(
              ElementsAre(MakeIPEndPoint("192.0.2.10", 443)),
              ElementsAre(MakeIPEndPoint("2001:db8::10", 443)), kMetadata2)));
  EXPECT_EQ(delegate()->update_count(), 1u);
}

TEST_F(DnsTaskResultsManagerTest, AddressHintsSupersededByRealResponses) {
  std::unique_ptr<DnsTaskResultsManager> manager = factory().Create();

  // HTTPS comes first with address hints.
  HostResolverInternalMetadataResult::AddressHintsMap address_hints;
  address_hints[std::string(kHostName)] =
      MakeAddressHints({"192.0.2.10"}, {"2001:db8::10"});
  std::unique_ptr<HostResolverInternalResult> result1 =
      CreateMetadata(kHostName, {{1, kMetadata1}}, std::move(address_hints));
  manager->ProcessDnsTransactionResults(DnsQueryType::HTTPS, {result1.get()});

  EXPECT_THAT(
      manager->GetCurrentEndpoints(),
      ElementsAre(ExpectServiceEndpoint(
          ElementsAre(MakeIPEndPoint("192.0.2.10", 443)),
          ElementsAre(MakeIPEndPoint("2001:db8::10", 443)), kMetadata1)));

  // AAAA is responded. IPv6 hints should be superseded by real addresses.
  std::unique_ptr<HostResolverInternalResult> result2 = CreateDataResult(
      kHostName, {MakeIPEndPoint("2001:db8::1")}, DnsQueryType::AAAA);
  manager->ProcessDnsTransactionResults(DnsQueryType::AAAA, {result2.get()});

  EXPECT_THAT(
      manager->GetCurrentEndpoints(),
      ElementsAre(ExpectServiceEndpoint(
          ElementsAre(MakeIPEndPoint("192.0.2.10", 443)),
          ElementsAre(MakeIPEndPoint("2001:db8::1", 443)), kMetadata1)));

  // A is responded. IPv4 hints should be superseded by real addresses.
  std::unique_ptr<HostResolverInternalResult> result3 = CreateDataResult(
      kHostName, {MakeIPEndPoint("192.0.2.1")}, DnsQueryType::A);
  manager->ProcessDnsTransactionResults(DnsQueryType::A, {result3.get()});

  EXPECT_THAT(
      manager->GetCurrentEndpoints(),
      ElementsAre(ExpectServiceEndpoint(
          ElementsAre(MakeIPEndPoint("192.0.2.1", 443)),
          ElementsAre(MakeIPEndPoint("2001:db8::1", 443)), kMetadata1)));
}

TEST_F(DnsTaskResultsManagerTest, AddressHintsIPv6NoData) {
  std::unique_ptr<DnsTaskResultsManager> manager = factory().Create();

  // HTTPS comes first with address hints.
  HostResolverInternalMetadataResult::AddressHintsMap address_hints;
  address_hints[std::string(kHostName)] =
      MakeAddressHints({"192.0.2.10"}, {"2001:db8::10"});
  std::unique_ptr<HostResolverInternalResult> result1 =
      CreateMetadata(kHostName, {{1, kMetadata1}}, std::move(address_hints));
  manager->ProcessDnsTransactionResults(DnsQueryType::HTTPS, {result1.get()});

  EXPECT_THAT(
      manager->GetCurrentEndpoints(),
      ElementsAre(ExpectServiceEndpoint(
          ElementsAre(MakeIPEndPoint("192.0.2.10", 443)),
          ElementsAre(MakeIPEndPoint("2001:db8::10", 443)), kMetadata1)));

  // AAAA is responded with no data. IPv6 hints should be dropped while IPv4
  // hints are still usable.
  std::unique_ptr<HostResolverInternalResult> result2 =
      CreateNoData(kHostName, DnsQueryType::AAAA);
  manager->ProcessDnsTransactionResults(DnsQueryType::AAAA, {result2.get()});

  EXPECT_THAT(manager->GetCurrentEndpoints(),
              ElementsAre(ExpectServiceEndpoint(
                  ElementsAre(MakeIPEndPoint("192.0.2.10", 443)), IsEmpty(),
                  kMetadata1)));
}

TEST_F(DnsTaskResultsManagerTest, AddressHintsIPv4OnlyResolutionDelay) {
  std::unique_ptr<DnsTaskResultsManager> manager = factory().Create();

  // HTTPS comes first with IPv4-only hints. Service endpoints creation should
  // be delayed since AAAA is still outstanding.
  HostResolverInternalMetadataResult::AddressHintsMap address_hints;
  address_hints[std::string(kHostName)] =
      MakeAddressHints({"192.0.2.10"}, /*ipv6_literals=*/{});
  std::unique_ptr<HostResolverInternalResult> result =
      CreateMetadata(kHostName, {{1, kMetadata1}}, std::move(address_hints));
  manager->ProcessDnsTransactionResults(DnsQueryType::HTTPS, {result.get()});

  ASSERT_TRUE(manager->IsResolutionDelayTimerRunningForTest());
  ASSERT_TRUE(manager->GetCurrentEndpoints().empty());

  // AAAA is timed out. IPv4 hints should be published.
  FastForwardBy(DnsTaskResultsManager::GetResolutionDelay() +
                base::Milliseconds(1));

  EXPECT_THAT(manager->GetCurrentEndpoints(),
              ElementsAre(ExpectServiceEndpoint(
                  ElementsAre(MakeIPEndPoint("192.0.2.10", 443)), IsEmpty(),
                  kMetadata1)));
}

TEST_F(DnsTaskResultsManagerTest, AddressHintsIPv4OnlyThenIPv4Response) {
  std::unique_ptr<DnsTaskResultsManager> manager = factory().Create();

  // HTTPS comes first with IPv4-only hints, starting the resolution delay
  // timer.
  HostResolverInternalMetadataResult::AddressHintsMap address_hints;
  address_hints[std::string(kHostName)] =
      MakeAddressHints({"192.0.2.10"}, /*ipv6_literals=*/{});
  std::unique_ptr<HostResolverInternalResult> result1 =
      CreateMetadata(kHostName, {{1, kMetadata1}}, std::move(address_hints));
  manager->ProcessDnsTransactionResults(DnsQueryType::HTTPS, {result1.get()});

  ASSERT_TRUE(manager->IsResolutionDelayTimerRunningForTest());
  ASSERT_TRUE(manager->GetCurrentEndpoints().empty());

  // A is responded while the timer is running. Service endpoints creation
  // should still be delayed and the timer should keep running.
  std::unique_ptr<HostResolverInternalResult> result2 = CreateDataResult(
      kHostName, {MakeIPEndPoint("192.0.2.1")}, DnsQueryType::A);
  manager->ProcessDnsTransactionResults(DnsQueryType::A, {result2.get()});

  ASSERT_TRUE(manager->IsResolutionDelayTimerRunningForTest());
  ASSERT_TRUE(manager->GetCurrentEndpoints().empty());

  // AAAA is timed out. Real IPv4 addresses should be published without IPv4
  // hints since the A response has arrived.
  FastForwardBy(DnsTaskResultsManager::GetResolutionDelay() +
                base::Milliseconds(1));

  EXPECT_THAT(manager->GetCurrentEndpoints(),
              ElementsAre(ExpectServiceEndpoint(
                  ElementsAre(MakeIPEndPoint("192.0.2.1", 443)), IsEmpty(),
                  kMetadata1)));
}

TEST_F(DnsTaskResultsManagerTest, AddressHintsIPv6WhileResolutionDelay) {
  std::unique_ptr<DnsTaskResultsManager> manager = factory().Create();

  // A comes first, starting the resolution delay timer.
  std::unique_ptr<HostResolverInternalResult> result1 = CreateDataResult(
      kHostName, {MakeIPEndPoint("192.0.2.1")}, DnsQueryType::A);
  manager->ProcessDnsTransactionResults(DnsQueryType::A, {result1.get()});

  ASSERT_TRUE(manager->IsResolutionDelayTimerRunningForTest());
  ASSERT_TRUE(manager->GetCurrentEndpoints().empty());

  // HTTPS is responded with IPv6 hints. Service endpoints should be published
  // without waiting for the AAAA response and the timer should stop.
  HostResolverInternalMetadataResult::AddressHintsMap address_hints;
  address_hints[std::string(kHostName)] =
      MakeAddressHints(/*ipv4_literals=*/{}, {"2001:db8::10"});
  std::unique_ptr<HostResolverInternalResult> result2 =
      CreateMetadata(kHostName, {{1, kMetadata1}}, std::move(address_hints));
  manager->ProcessDnsTransactionResults(DnsQueryType::HTTPS, {result2.get()});

  ASSERT_FALSE(manager->IsResolutionDelayTimerRunningForTest());
  EXPECT_THAT(
      manager->GetCurrentEndpoints(),
      ElementsAre(ExpectServiceEndpoint(
          ElementsAre(MakeIPEndPoint("192.0.2.1", 443)),
          ElementsAre(MakeIPEndPoint("2001:db8::10", 443)), kMetadata1)));
  EXPECT_EQ(delegate()->update_count(), 1u);

  // The resolution delay passed. No extra notification should happen.
  FastForwardBy(DnsTaskResultsManager::GetResolutionDelay() +
                base::Milliseconds(1));

  EXPECT_EQ(delegate()->update_count(), 1u);

  // AAAA is responded. IPv6 hints should be superseded.
  std::unique_ptr<HostResolverInternalResult> result3 = CreateDataResult(
      kHostName, {MakeIPEndPoint("2001:db8::1")}, DnsQueryType::AAAA);
  manager->ProcessDnsTransactionResults(DnsQueryType::AAAA, {result3.get()});

  ASSERT_FALSE(manager->IsResolutionDelayTimerRunningForTest());
  EXPECT_THAT(
      manager->GetCurrentEndpoints(),
      ElementsAre(ExpectServiceEndpoint(
          ElementsAre(MakeIPEndPoint("192.0.2.1", 443)),
          ElementsAre(MakeIPEndPoint("2001:db8::1", 443)), kMetadata1)));
}

TEST_F(DnsTaskResultsManagerTest, AddressHintsIPv4FlushedOnANoData) {
  std::unique_ptr<DnsTaskResultsManager> manager = factory().Create();

  // HTTPS comes first with IPv4-only hints, which are published after the
  // resolution delay.
  HostResolverInternalMetadataResult::AddressHintsMap address_hints;
  address_hints[std::string(kHostName)] =
      MakeAddressHints({"192.0.2.10"}, /*ipv6_literals=*/{});
  std::unique_ptr<HostResolverInternalResult> result1 =
      CreateMetadata(kHostName, {{1, kMetadata1}}, std::move(address_hints));
  manager->ProcessDnsTransactionResults(DnsQueryType::HTTPS, {result1.get()});

  FastForwardBy(DnsTaskResultsManager::GetResolutionDelay() +
                base::Milliseconds(1));

  EXPECT_THAT(manager->GetCurrentEndpoints(),
              ElementsAre(ExpectServiceEndpoint(
                  ElementsAre(MakeIPEndPoint("192.0.2.10", 443)), IsEmpty(),
                  kMetadata1)));
  EXPECT_EQ(delegate()->update_count(), 1u);

  // A is responded with no data. Superseded IPv4 hints should be flushed.
  std::unique_ptr<HostResolverInternalResult> result2 =
      CreateNoData(kHostName, DnsQueryType::A);
  manager->ProcessDnsTransactionResults(DnsQueryType::A, {result2.get()});

  EXPECT_TRUE(manager->GetCurrentEndpoints().empty());
  EXPECT_EQ(delegate()->update_count(), 2u);
}

TEST_F(DnsTaskResultsManagerTest,
       AddressHintsIPv4FlushedOnANoDataWhileResolutionDelay) {
  std::unique_ptr<DnsTaskResultsManager> manager = factory().Create();

  // HTTPS comes first with IPv4-only hints, starting the resolution delay
  // timer.
  HostResolverInternalMetadataResult::AddressHintsMap address_hints;
  address_hints[std::string(kHostName)] =
      MakeAddressHints({"192.0.2.10"}, /*ipv6_literals=*/{});
  std::unique_ptr<HostResolverInternalResult> result1 =
      CreateMetadata(kHostName, {{1, kMetadata1}}, std::move(address_hints));
  manager->ProcessDnsTransactionResults(DnsQueryType::HTTPS, {result1.get()});

  ASSERT_TRUE(manager->IsResolutionDelayTimerRunningForTest());
  ASSERT_TRUE(manager->GetCurrentEndpoints().empty());

  // A is responded with no data while the timer is running. The resolution
  // delay timer should stop immediately and superseded IPv4 hints should be
  // flushed.
  std::unique_ptr<HostResolverInternalResult> result2 =
      CreateNoData(kHostName, DnsQueryType::A);
  manager->ProcessDnsTransactionResults(DnsQueryType::A, {result2.get()});

  EXPECT_FALSE(manager->IsResolutionDelayTimerRunningForTest());
  EXPECT_TRUE(manager->GetCurrentEndpoints().empty());
}

TEST_F(DnsTaskResultsManagerTest, AddressHintsIPv6FlushedOnAaaaNoData) {
  std::unique_ptr<DnsTaskResultsManager> manager = factory().Create();

  // HTTPS comes first with IPv6-only hints, which are published immediately.
  HostResolverInternalMetadataResult::AddressHintsMap address_hints;
  address_hints[std::string(kHostName)] =
      MakeAddressHints(/*ipv4_literals=*/{}, {"2001:db8::10"});
  std::unique_ptr<HostResolverInternalResult> result1 =
      CreateMetadata(kHostName, {{1, kMetadata1}}, std::move(address_hints));
  manager->ProcessDnsTransactionResults(DnsQueryType::HTTPS, {result1.get()});

  EXPECT_THAT(manager->GetCurrentEndpoints(),
              ElementsAre(ExpectServiceEndpoint(
                  IsEmpty(), ElementsAre(MakeIPEndPoint("2001:db8::10", 443)),
                  kMetadata1)));
  EXPECT_EQ(delegate()->update_count(), 1u);

  // AAAA is responded with no data. Superseded IPv6 hints should be flushed.
  std::unique_ptr<HostResolverInternalResult> result2 =
      CreateNoData(kHostName, DnsQueryType::AAAA);
  manager->ProcessDnsTransactionResults(DnsQueryType::AAAA, {result2.get()});

  EXPECT_TRUE(manager->GetCurrentEndpoints().empty());
  EXPECT_EQ(delegate()->update_count(), 2u);
}

TEST_F(DnsTaskResultsManagerTest, AddressHintsDifferentTargetName) {
  std::unique_ptr<DnsTaskResultsManager> manager = factory().Create();

  // HTTPS is responded with hints keyed to a target name different from QNAME.
  // The hints should land on the target name's endpoint entry.
  const ConnectionEndpointMetadata kMetadataDifferentTargetName(
      /*supported_protocol_alpns=*/{"h3"},
      /*ech_config_list=*/{},
      /*target_name=*/"other.example.net.", {});
  HostResolverInternalMetadataResult::AddressHintsMap address_hints;
  address_hints["other.example.net."] =
      MakeAddressHints({"192.0.2.10"}, {"2001:db8::10"});
  std::unique_ptr<HostResolverInternalResult> result = CreateMetadata(
      kHostName, {{1, kMetadataDifferentTargetName}}, std::move(address_hints));
  manager->ProcessDnsTransactionResults(DnsQueryType::HTTPS, {result.get()});

  EXPECT_THAT(manager->GetCurrentEndpoints(),
              ElementsAre(ExpectServiceEndpoint(
                  ElementsAre(MakeIPEndPoint("192.0.2.10", 443)),
                  ElementsAre(MakeIPEndPoint("2001:db8::10", 443)),
                  kMetadataDifferentTargetName)));
}

TEST_F(DnsTaskResultsManagerTest, AddressHintsIPv6NotQueried) {
  std::unique_ptr<DnsTaskResultsManager> manager =
      factory().query_types({DnsQueryType::A, DnsQueryType::HTTPS}).Create();

  // HTTPS is responded with IPv4 and IPv6 hints. IPv6 hints should not be
  // published since AAAA is never queried.
  HostResolverInternalMetadataResult::AddressHintsMap address_hints;
  address_hints[std::string(kHostName)] =
      MakeAddressHints({"192.0.2.10"}, {"2001:db8::10"});
  std::unique_ptr<HostResolverInternalResult> result =
      CreateMetadata(kHostName, {{1, kMetadata1}}, std::move(address_hints));
  manager->ProcessDnsTransactionResults(DnsQueryType::HTTPS, {result.get()});

  EXPECT_THAT(manager->GetCurrentEndpoints(),
              ElementsAre(ExpectServiceEndpoint(
                  ElementsAre(MakeIPEndPoint("192.0.2.10", 443)), IsEmpty(),
                  kMetadata1)));
}

TEST_F(DnsTaskResultsManagerTest, AddressHintsIgnoredAfterFamilyComplete) {
  std::unique_ptr<DnsTaskResultsManager> manager = factory().Create();

  // A is responded first.
  std::unique_ptr<HostResolverInternalResult> result1 = CreateDataResult(
      kHostName, {MakeIPEndPoint("192.0.2.1")}, DnsQueryType::A);
  manager->ProcessDnsTransactionResults(DnsQueryType::A, {result1.get()});

  // AAAA is responded with no data.
  std::unique_ptr<HostResolverInternalResult> result2 =
      CreateNoData(kHostName, DnsQueryType::AAAA);
  manager->ProcessDnsTransactionResults(DnsQueryType::AAAA, {result2.get()});

  // HTTPS comes after both A and AAAA transactions have completed. Hints for
  // already-completed address families must be ignored per RFC 9460 §7.3.
  HostResolverInternalMetadataResult::AddressHintsMap address_hints;
  address_hints[std::string(kHostName)] =
      MakeAddressHints({"192.0.2.10"}, {"2001:db8::10"});
  std::unique_ptr<HostResolverInternalResult> result3 =
      CreateMetadata(kHostName, {{1, kMetadata1}}, std::move(address_hints));
  manager->ProcessDnsTransactionResults(DnsQueryType::HTTPS, {result3.get()});

  EXPECT_THAT(manager->GetCurrentEndpoints(),
              ElementsAre(ExpectServiceEndpoint(
                  ElementsAre(MakeIPEndPoint("192.0.2.1", 443)), IsEmpty(),
                  kMetadata1)));
}

}  // namespace net
