// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/dns_task_results_manager.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <string_view>

#include "base/check.h"
#include "base/functional/callback_forward.h"
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

 private:
  void OnServiceEndpointsUpdated() override {
    // Do nothing for now.
  }
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
    std::multimap<HttpsRecordPriority, ConnectionEndpointMetadata> metadatas) {
  return std::make_unique<HostResolverInternalMetadataResult>(
      std::string(domain_name), DnsQueryType::HTTPS,
      /*expiration=*/base::TimeTicks(), /*timed_expiration=*/base::Time(),
      HostResolverInternalResult::Source::kDns, std::move(metadatas));
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
    std::string(kHostName));

static const ConnectionEndpointMetadata kMetadata2(
    /*supported_protocol_alpns=*/{"h2", "http/1.1"},
    /*ech_config_list=*/{},
    std::string(kHostName));

static const std::multimap<HttpsRecordPriority, ConnectionEndpointMetadata>
    kMetadatas{{1, kMetadata1}, {2, kMetadata2}};

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

  std::set<std::unique_ptr<HostResolverInternalResult>> results;
  results.insert(CreateDataResult(kHostName, {MakeIPEndPoint("192.0.2.1")},
                                  DnsQueryType::A));
  manager->ProcessDnsTransactionResults(DnsQueryType::A, results);

  EXPECT_THAT(manager->GetCurrentEndpoints(),
              ElementsAre(ExpectServiceEndpoint(
                  ElementsAre(MakeIPEndPoint("192.0.2.1", 443)))));
  EXPECT_THAT(manager->GetAliases(), UnorderedElementsAre(kHostName));
}

TEST_F(DnsTaskResultsManagerTest, IPv4First) {
  std::unique_ptr<DnsTaskResultsManager> manager = factory().Create();

  // A comes first. Service endpoints creation should be delayed.
  std::set<std::unique_ptr<HostResolverInternalResult>> results1;
  results1.insert(CreateDataResult(kHostName, {MakeIPEndPoint("192.0.2.1")},
                                   DnsQueryType::A));
  manager->ProcessDnsTransactionResults(DnsQueryType::A, results1);

  ASSERT_TRUE(manager->GetCurrentEndpoints().empty());

  // AAAA is responded. Service endpoints should be available.
  std::set<std::unique_ptr<HostResolverInternalResult>> results2;
  results2.insert(CreateDataResult(kHostName, {MakeIPEndPoint("2001:db8::1")},
                                   DnsQueryType::AAAA));
  manager->ProcessDnsTransactionResults(DnsQueryType::AAAA, results2);

  EXPECT_THAT(manager->GetCurrentEndpoints(),
              ElementsAre(ExpectServiceEndpoint(
                  ElementsAre(MakeIPEndPoint("192.0.2.1", 443)),
                  ElementsAre(MakeIPEndPoint("2001:db8::1", 443)))));
}

TEST_F(DnsTaskResultsManagerTest, IPv6First) {
  std::unique_ptr<DnsTaskResultsManager> manager = factory().Create();

  // AAAA comes first. Service endpoints should be available immediately.
  std::set<std::unique_ptr<HostResolverInternalResult>> results1;
  results1.insert(CreateDataResult(kHostName, {MakeIPEndPoint("2001:db8::1")},
                                   DnsQueryType::AAAA));
  manager->ProcessDnsTransactionResults(DnsQueryType::AAAA, results1);

  EXPECT_THAT(manager->GetCurrentEndpoints(),
              ElementsAre(ExpectServiceEndpoint(
                  IsEmpty(), ElementsAre(MakeIPEndPoint("2001:db8::1", 443)))));

  // A is responded. Service endpoints should be updated.
  std::set<std::unique_ptr<HostResolverInternalResult>> results;
  results.insert(CreateDataResult(
      kHostName, {MakeIPEndPoint("192.0.2.1"), MakeIPEndPoint("192.0.2.2")},
      DnsQueryType::A));
  manager->ProcessDnsTransactionResults(DnsQueryType::A, results);

  EXPECT_THAT(manager->GetCurrentEndpoints(),
              ElementsAre(ExpectServiceEndpoint(
                  ElementsAre(MakeIPEndPoint("192.0.2.1", 443),
                              MakeIPEndPoint("192.0.2.2", 443)),
                  ElementsAre(MakeIPEndPoint("2001:db8::1", 443)))));
}

TEST_F(DnsTaskResultsManagerTest, IPv6Timedout) {
  std::unique_ptr<DnsTaskResultsManager> manager = factory().Create();

  // A comes first. Service endpoints creation should be delayed.
  std::set<std::unique_ptr<HostResolverInternalResult>> results1;
  results1.insert(CreateDataResult(kHostName, {MakeIPEndPoint("192.0.2.1")},
                                   DnsQueryType::A));
  manager->ProcessDnsTransactionResults(DnsQueryType::A, results1);

  ASSERT_TRUE(manager->GetCurrentEndpoints().empty());

  // AAAA is timed out. Service endpoints should be available after timeout.
  FastForwardBy(DnsTaskResultsManager::kResolutionDelay +
                base::Milliseconds(1));

  EXPECT_THAT(manager->GetCurrentEndpoints(),
              ElementsAre(ExpectServiceEndpoint(
                  ElementsAre(MakeIPEndPoint("192.0.2.1", 443)))));

  // AAAA is responded after timeout. Service endpoints should be updated.
  std::set<std::unique_ptr<HostResolverInternalResult>> results2;
  results2.insert(CreateDataResult(kHostName, {MakeIPEndPoint("2001:db8::1")},
                                   DnsQueryType::AAAA));
  manager->ProcessDnsTransactionResults(DnsQueryType::AAAA, results2);

  EXPECT_THAT(manager->GetCurrentEndpoints(),
              ElementsAre(ExpectServiceEndpoint(
                  ElementsAre(MakeIPEndPoint("192.0.2.1", 443)),
                  ElementsAre(MakeIPEndPoint("2001:db8::1", 443)))));
}

TEST_F(DnsTaskResultsManagerTest, IPv6NoDataBeforeIPv4) {
  std::unique_ptr<DnsTaskResultsManager> manager = factory().Create();

  // AAAA is responded with no data. Service endpoints should not be available.
  std::set<std::unique_ptr<HostResolverInternalResult>> results1;
  results1.insert(CreateNoData(kHostName, DnsQueryType::AAAA));
  manager->ProcessDnsTransactionResults(DnsQueryType::AAAA, results1);

  ASSERT_TRUE(manager->GetCurrentEndpoints().empty());

  // A is responded. Service endpoints creation should happen without resolution
  // delay.
  std::set<std::unique_ptr<HostResolverInternalResult>> results2;
  results2.insert(CreateDataResult(kHostName, {MakeIPEndPoint("192.0.2.1")},
                                   DnsQueryType::A));
  manager->ProcessDnsTransactionResults(DnsQueryType::A, results2);

  EXPECT_THAT(manager->GetCurrentEndpoints(),
              ElementsAre(ExpectServiceEndpoint(
                  ElementsAre(MakeIPEndPoint("192.0.2.1", 443)))));
}

TEST_F(DnsTaskResultsManagerTest, IPv6NoDataAfterIPv4) {
  std::unique_ptr<DnsTaskResultsManager> manager = factory().Create();

  // A is responded. Service endpoints creation should be delayed.
  std::set<std::unique_ptr<HostResolverInternalResult>> results1;
  results1.insert(CreateDataResult(kHostName, {MakeIPEndPoint("192.0.2.1")},
                                   DnsQueryType::A));
  manager->ProcessDnsTransactionResults(DnsQueryType::A, results1);

  ASSERT_TRUE(manager->GetCurrentEndpoints().empty());

  // AAAA is responded with no data before the resolution delay timer. Service
  // endpoints should be available without waiting for the timeout.
  std::set<std::unique_ptr<HostResolverInternalResult>> results2;
  results2.insert(CreateNoData(kHostName, DnsQueryType::AAAA));
  manager->ProcessDnsTransactionResults(DnsQueryType::AAAA, results2);

  EXPECT_THAT(manager->GetCurrentEndpoints(),
              ElementsAre(ExpectServiceEndpoint(
                  ElementsAre(MakeIPEndPoint("192.0.2.1", 443)))));
}

TEST_F(DnsTaskResultsManagerTest, IPv6EmptyDataAfterIPv4) {
  std::unique_ptr<DnsTaskResultsManager> manager = factory().Create();

  // A is responded. Service endpoints creation should be delayed.
  std::set<std::unique_ptr<HostResolverInternalResult>> results1;
  results1.insert(CreateDataResult(kHostName, {MakeIPEndPoint("192.0.2.1")},
                                   DnsQueryType::A));
  manager->ProcessDnsTransactionResults(DnsQueryType::A, results1);

  ASSERT_TRUE(manager->GetCurrentEndpoints().empty());

  // AAAA is responded with a non-cacheable result (an empty result) before the
  // resolution delay timer. Service endpoints should be available without
  // waiting for the timeout.
  std::set<std::unique_ptr<HostResolverInternalResult>> results2;
  manager->ProcessDnsTransactionResults(DnsQueryType::AAAA, results2);

  EXPECT_THAT(manager->GetCurrentEndpoints(),
              ElementsAre(ExpectServiceEndpoint(
                  ElementsAre(MakeIPEndPoint("192.0.2.1", 443)))));
}

TEST_F(DnsTaskResultsManagerTest, IPv4AndIPv6NoData) {
  std::unique_ptr<DnsTaskResultsManager> manager = factory().Create();

  // AAAA is responded with no data. Service endpoints should not be available.
  std::set<std::unique_ptr<HostResolverInternalResult>> results1;
  results1.insert(CreateNoData(kHostName, DnsQueryType::AAAA));
  manager->ProcessDnsTransactionResults(DnsQueryType::AAAA, results1);

  ASSERT_TRUE(manager->GetCurrentEndpoints().empty());

  // A is responded with no data. Service endpoints should not be available.
  std::set<std::unique_ptr<HostResolverInternalResult>> results2;
  results2.insert(CreateNoData(kHostName, DnsQueryType::A));
  manager->ProcessDnsTransactionResults(DnsQueryType::A, results2);

  ASSERT_TRUE(manager->GetCurrentEndpoints().empty());
}

TEST_F(DnsTaskResultsManagerTest, IPv4NoDataIPv6AfterResolutionDelay) {
  std::unique_ptr<DnsTaskResultsManager> manager = factory().Create();

  // A comes first with no data. Service endpoints creation should be delayed
  // and the resolution delay timer should not start.
  std::set<std::unique_ptr<HostResolverInternalResult>> results1;
  results1.insert(CreateNoData(kHostName, DnsQueryType::A));
  manager->ProcessDnsTransactionResults(DnsQueryType::A, results1);

  ASSERT_FALSE(manager->IsResolutionDelayTimerRunningForTest());
  ASSERT_TRUE(manager->GetCurrentEndpoints().empty());

  // The resolution delay passed. Service endpoints should not be available yet.
  FastForwardBy(DnsTaskResultsManager::kResolutionDelay +
                base::Milliseconds(1));

  ASSERT_TRUE(manager->GetCurrentEndpoints().empty());

  // AAAA is responded. Service endpoints should be updated.
  std::set<std::unique_ptr<HostResolverInternalResult>> results2;
  results2.insert(CreateDataResult(kHostName, {MakeIPEndPoint("2001:db8::1")},
                                   DnsQueryType::AAAA));
  manager->ProcessDnsTransactionResults(DnsQueryType::AAAA, results2);

  EXPECT_THAT(manager->GetCurrentEndpoints(),
              ElementsAre(ExpectServiceEndpoint(
                  IsEmpty(), ElementsAre(MakeIPEndPoint("2001:db8::1", 443)))));
}

TEST_F(DnsTaskResultsManagerTest, MetadataFirst) {
  std::unique_ptr<DnsTaskResultsManager> manager = factory().Create();

  // HTTPS comes first. Service endpoints should not be available yet since
  // Chrome doesn't support ipv{4,6}hint yet.
  std::set<std::unique_ptr<HostResolverInternalResult>> results1;
  results1.insert(CreateMetadata(kHostName, kMetadatas));
  manager->ProcessDnsTransactionResults(DnsQueryType::HTTPS, results1);

  ASSERT_TRUE(manager->GetCurrentEndpoints().empty());
  ASSERT_TRUE(manager->IsMetadataReady());

  // A is responded. Service endpoints creation should be delayed.
  std::set<std::unique_ptr<HostResolverInternalResult>> results2;
  results2.insert(CreateDataResult(kHostName, {MakeIPEndPoint("192.0.2.1")},
                                   DnsQueryType::A));
  manager->ProcessDnsTransactionResults(DnsQueryType::A, results2);

  ASSERT_TRUE(manager->GetCurrentEndpoints().empty());

  // AAAA is responded. Service endpoints should be available with metadatas.
  std::set<std::unique_ptr<HostResolverInternalResult>> results3;
  results3.insert(CreateDataResult(kHostName, {MakeIPEndPoint("2001:db8::1")},
                                   DnsQueryType::AAAA));
  manager->ProcessDnsTransactionResults(DnsQueryType::AAAA, results3);

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
      /*target_name=*/"other.example.net.");
  std::set<std::unique_ptr<HostResolverInternalResult>> results1;
  results1.insert(
      CreateMetadata(kHostName, {{1, kMetadataDifferentTargetName}}));
  manager->ProcessDnsTransactionResults(DnsQueryType::HTTPS, results1);

  ASSERT_TRUE(manager->IsMetadataReady());

  // AAAA is responded. Service endpoints should be available without metadatas
  // since the target name is different.
  std::set<std::unique_ptr<HostResolverInternalResult>> results2;
  results2.insert(CreateDataResult(kHostName, {MakeIPEndPoint("2001:db8::1")},
                                   DnsQueryType::AAAA));
  manager->ProcessDnsTransactionResults(DnsQueryType::AAAA, results2);

  ASSERT_TRUE(manager->IsMetadataReady());
  EXPECT_THAT(manager->GetCurrentEndpoints(),
              ElementsAre(ExpectServiceEndpoint(
                  IsEmpty(), ElementsAre(MakeIPEndPoint("2001:db8::1", 443)))));
}

TEST_F(DnsTaskResultsManagerTest, MetadataAfterIPv6) {
  std::unique_ptr<DnsTaskResultsManager> manager = factory().Create();

  // AAAA comes first. Service endpoints should be available without metadatas.
  std::set<std::unique_ptr<HostResolverInternalResult>> results1;
  results1.insert(CreateDataResult(kHostName, {MakeIPEndPoint("2001:db8::1")},
                                   DnsQueryType::AAAA));
  manager->ProcessDnsTransactionResults(DnsQueryType::AAAA, results1);

  ASSERT_FALSE(manager->IsMetadataReady());
  EXPECT_THAT(manager->GetCurrentEndpoints(),
              ElementsAre(ExpectServiceEndpoint(
                  IsEmpty(), ElementsAre(MakeIPEndPoint("2001:db8::1", 443)))));

  // HTTPS is responded. Metadata should be available.
  std::set<std::unique_ptr<HostResolverInternalResult>> results2;
  results2.insert(CreateMetadata(kHostName, kMetadatas));
  manager->ProcessDnsTransactionResults(DnsQueryType::HTTPS, results2);

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
  std::set<std::unique_ptr<HostResolverInternalResult>> results1;
  results1.insert(CreateDataResult(kHostName, {MakeIPEndPoint("192.0.2.1")},
                                   DnsQueryType::A));
  manager->ProcessDnsTransactionResults(DnsQueryType::A, results1);

  ASSERT_FALSE(manager->IsMetadataReady());
  ASSERT_TRUE(manager->GetCurrentEndpoints().empty());

  // HTTPS is responded. Service endpoints should not be available because
  // the manager is waiting for the resolution delay and Chrome doesn't support
  // ipv6hint yet.
  std::set<std::unique_ptr<HostResolverInternalResult>> results2;
  results2.insert(CreateMetadata(kHostName, kMetadatas));
  manager->ProcessDnsTransactionResults(DnsQueryType::HTTPS, results2);

  ASSERT_TRUE(manager->IsMetadataReady());
  ASSERT_TRUE(manager->GetCurrentEndpoints().empty());

  // AAAA is timed out. Service endpoints should be available with metadatas.
  FastForwardBy(DnsTaskResultsManager::kResolutionDelay +
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

TEST_F(DnsTaskResultsManagerTest, IPv4NoDataIPv6TimedoutAfterMetadata) {
  std::unique_ptr<DnsTaskResultsManager> manager = factory().Create();

  // HTTPS is responded. Service endpoints should not be available because
  // the manager is waiting for the resolution delay and Chrome doesn't support
  // address hints yet.
  std::set<std::unique_ptr<HostResolverInternalResult>> results1;
  results1.insert(CreateMetadata(kHostName, kMetadatas));
  manager->ProcessDnsTransactionResults(DnsQueryType::HTTPS, results1);

  ASSERT_TRUE(manager->IsMetadataReady());
  ASSERT_TRUE(manager->GetCurrentEndpoints().empty());

  // A is responded with no address. Service endpoints should not be available
  // since there are no addresses.
  std::set<std::unique_ptr<HostResolverInternalResult>> results2;
  results2.insert(CreateNoData(kHostName, DnsQueryType::A));
  manager->ProcessDnsTransactionResults(DnsQueryType::A, results2);

  ASSERT_TRUE(manager->GetCurrentEndpoints().empty());

  // AAAA is timed out. Service endpoints should not be available since there
  // are no addresses.
  FastForwardBy(DnsTaskResultsManager::kResolutionDelay +
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
      /*target_name=*/std::string(kSvcbHost1));
  const ConnectionEndpointMetadata kSvcbHost1Metadata2(
      /*supported_protocol_alpns=*/{"h3"},
      /*ech_config_list=*/{},
      /*target_name=*/std::string(kSvcbHost1));

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
      std::set<std::unique_ptr<HostResolverInternalResult>> results;
      results.insert(CreateDataResult(testdata.host, testdata.ipv4_endpoints,
                                      DnsQueryType::A));
      manager->ProcessDnsTransactionResults(DnsQueryType::A, results);
    }
    if (!testdata.ipv6_endpoints.empty()) {
      std::set<std::unique_ptr<HostResolverInternalResult>> results;
      results.insert(CreateDataResult(testdata.host, testdata.ipv6_endpoints,
                                      DnsQueryType::AAAA));
      manager->ProcessDnsTransactionResults(DnsQueryType::AAAA, results);
    }
    if (!testdata.metadatas.empty()) {
      std::set<std::unique_ptr<HostResolverInternalResult>> results;
      results.insert(CreateMetadata(testdata.host, testdata.metadatas));
      manager->ProcessDnsTransactionResults(DnsQueryType::HTTPS, results);
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
  std::set<std::unique_ptr<HostResolverInternalResult>> results1;
  results1.insert(CreateAlias(kHostName, DnsQueryType::AAAA, kAliasTarget1));
  results1.insert(
      CreateAlias(kAliasTarget1, DnsQueryType::AAAA, kAliasTarget2));
  results1.insert(CreateDataResult(kHostName, {MakeIPEndPoint("2001:db8::1")},
                                   DnsQueryType::AAAA));
  manager->ProcessDnsTransactionResults(DnsQueryType::AAAA, results1);

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
  std::set<std::unique_ptr<HostResolverInternalResult>> results;
  results.insert(CreateDataResult(kHostName, {endpoint}, DnsQueryType::AAAA));
  manager->ProcessDnsTransactionResults(DnsQueryType::AAAA, results);
  EXPECT_THAT(manager->GetCurrentEndpoints(),
              ElementsAre(ExpectServiceEndpoint(
                  ElementsAre(MakeIPEndPoint("192.0.2.1", 443)), IsEmpty())));
}

}  // namespace net
