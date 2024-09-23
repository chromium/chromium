// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/reporting/reporting_header_parser.h"

#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/time/time.h"
#include "base/values.h"
#include "net/base/features.h"
#include "net/base/isolation_info.h"
#include "net/base/network_anonymization_key.h"
#include "net/base/schemeful_site.h"
#include "net/reporting/mock_persistent_reporting_store.h"
#include "net/reporting/reporting_cache.h"
#include "net/reporting/reporting_endpoint.h"
#include "net/reporting/reporting_target_type.h"
#include "net/reporting/reporting_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace net {
namespace {

using CommandType = MockPersistentReportingStore::Command::Type;
using Dictionary = structured_headers::Dictionary;

constexpr char kReportingHeaderTypeHistogram[] = "Net.Reporting.HeaderType";

class ReportingHeaderParserTestBase
    : public ReportingTestBase,
      public ::testing::WithParamInterface<bool> {
 protected:
  ReportingHeaderParserTestBase() {
    ReportingPolicy policy;
    policy.max_endpoints_per_origin = 10;
    policy.max_endpoint_count = 20;
    UsePolicy(policy);

    std::unique_ptr<MockPersistentReportingStore> store;
    if (GetParam()) {
      store = std::make_unique<MockPersistentReportingStore>();
    }
    store_ = store.get();
    UseStore(std::move(store));
  }
  ~ReportingHeaderParserTestBase() override = default;

  void SetUp() override {
    // All ReportingCache methods assume that the store has been initialized.
    if (mock_store()) {
      mock_store()->LoadReportingClients(
          base::BindOnce(&ReportingCache::AddClientsLoadedFromStore,
                         base::Unretained(cache())));
      mock_store()->FinishLoading(true);
    }
  }

  MockPersistentReportingStore* mock_store() { return store_; }

  base::test::ScopedFeatureList feature_list_;
  const GURL kUrl1_ = GURL("https://origin1.test/path");
  const url::Origin kOrigin1_ = url::Origin::Create(kUrl1_);
  const GURL kUrl2_ = GURL("https://origin2.test/path");
  const url::Origin kOrigin2_ = url::Origin::Create(kUrl2_);
  const NetworkAnonymizationKey kNak_ =
      NetworkAnonymizationKey::CreateSameSite(SchemefulSite(kOrigin1_));
  const NetworkAnonymizationKey kOtherNak_ =
      NetworkAnonymizationKey::CreateSameSite(SchemefulSite(kOrigin2_));
  const IsolationInfo kIsolationInfo_ =
      IsolationInfo::Create(IsolationInfo::RequestType::kOther,
                            kOrigin1_,
                            kOrigin1_,
                            SiteForCookies::FromOrigin(kOrigin1_));
  const GURL kUrlEtld_ = GURL("https://co.uk/foo.html/");
  const url::Origin kOriginEtld_ = url::Origin::Create(kUrlEtld_);
  const GURL kEndpoint1_ = GURL("https://endpoint1.test/");
  const GURL kEndpoint2_ = GURL("https://endpoint2.test/");
  const GURL kEndpoint3_ = GURL("https://endpoint3.test/");
  const GURL kEndpointPathAbsolute_ =
      GURL("https://origin1.test/path-absolute-url");
  const std::string kGroup1_ = "group1";
  const std::string kGroup2_ = "group2";
  // There are 2^3 = 8 of these to test the different combinations of matching
  // vs mismatching NAK, origin, and group.
  const ReportingEndpointGroupKey kGroupKey11_ =
      ReportingEndpointGroupKey(kNak_,
                                kOrigin1_,
                                kGroup1_,
                                ReportingTargetType::kDeveloper);
  const ReportingEndpointGroupKey kGroupKey21_ =
      ReportingEndpointGroupKey(kNak_,
                                kOrigin2_,
                                kGroup1_,
                                ReportingTargetType::kDeveloper);
  const ReportingEndpointGroupKey kGroupKey12_ =
      ReportingEndpointGroupKey(kNak_,
                                kOrigin1_,
                                kGroup2_,
                                ReportingTargetType::kDeveloper);
  const ReportingEndpointGroupKey kGroupKey22_ =
      ReportingEndpointGroupKey(kNak_,
                                kOrigin2_,
                                kGroup2_,
                                ReportingTargetType::kDeveloper);

 private:
  raw_ptr<MockPersistentReportingStore> store_;
};

// This test is parametrized on a boolean that represents whether to use a
// MockPersistentReportingStore.
class ReportingHeaderParserTest : public ReportingHeaderParserTestBase {
 protected:
  ReportingHeaderParserTest() {
    // This is a private API of the reporting service, so no need to test the
    // case kPartitionConnectionsByNetworkIsolationKey is disabled - the
    // feature is only applied at the entry points of the service.
    feature_list_.InitAndEnableFeature(
        features::kPartitionConnectionsByNetworkIsolationKey);
  }

  ReportingEndpointGroup MakeEndpointGroup(
      const std::string& name,
      const std::vector<ReportingEndpoint::EndpointInfo>& endpoints,
      OriginSubdomains include_subdomains = OriginSubdomains::DEFAULT,
      base::TimeDelta ttl = base::Days(1),
      url::Origin origin = url::Origin()) {
    ReportingEndpointGroupKey group_key(kNak_ /* unused */,
                                        url::Origin() /* unused */, name,
                                        ReportingTargetType::kDeveloper);
    ReportingEndpointGroup group;
    group.group_key = group_key;
    group.include_subdomains = include_subdomains;
    group.ttl = ttl;
    group.endpoints = std::move(endpoints);
    return group;
  }

  // Constructs a string which would represent a single group in a Report-To
  // header. If |group_name| is an empty string, the group name will be omitted
  // (and thus default to "default" when parsed). Setting |omit_defaults| omits
  // the priority, weight, and include_subdomains fields if they are default,
  // otherwise they are spelled out fully.
  std::string ConstructHeaderGroupString(const ReportingEndpointGroup& group,
                                         bool omit_defaults = true) {
    std::ostringstream s;
    s << "{ ";

    if (!group.group_key.group_name.empty()) {
      s << "\"group\": \"" << group.group_key.group_name << "\", ";
    }

    s << "\"max_age\": " << group.ttl.InSeconds() << ", ";

    if (group.include_subdomains != OriginSubdomains::DEFAULT) {
      s << "\"include_subdomains\": true, ";
    } else if (!omit_defaults) {
      s << "\"include_subdomains\": false, ";
    }

    s << "\"endpoints\": [";
    for (const ReportingEndpoint::EndpointInfo& endpoint_info :
         group.endpoints) {
      s << "{ ";
      s << "\"url\": \"" << endpoint_info.url.spec() << "\"";

      if (!omit_defaults ||
          endpoint_info.priority !=
              ReportingEndpoint::EndpointInfo::kDefaultPriority) {
        s << ", \"priority\": " << endpoint_info.priority;
      }

      if (!omit_defaults ||
          endpoint_info.weight !=
              ReportingEndpoint::EndpointInfo::kDefaultWeight) {
        s << ", \"weight\": " << endpoint_info.weight;
      }

      s << " }, ";
    }
    if (!group.endpoints.empty())
      s.seekp(-2, s.cur);  // Overwrite trailing comma and space.
    s << "]";

    s << " }";

    return s.str();
  }

  void ParseHeader(const NetworkAnonymizationKey& network_anonymization_key,
                   const url::Origin& origin,
                   const std::string& json) {
    std::optional<base::Value> value = base::JSONReader::Read("[" + json + "]");
    if (value) {
      ReportingHeaderParser::ParseReportToHeader(
          context(), network_anonymization_key, origin, value->GetList());
    }
  }
};

// TODO(juliatuttle): Ideally these tests should be expecting that JSON parsing
// (and therefore header parsing) may happen asynchronously, but the entire
// pipeline is also tested by NetworkErrorLoggingEndToEndTest.

TEST_P(ReportingHeaderParserTest, Invalid) {
  static const struct {
    const char* header_value;
    const char* description;
  } kInvalidHeaderTestCases[] = {
      {"{\"max_age\":1, \"endpoints\": [{}]}", "missing url"},
      {"{\"max_age\":1, \"endpoints\": [{\"url\":0}]}", "non-string url"},
      {"{\"max_age\":1, \"endpoints\": [{\"url\":\"//scheme/relative\"}]}",
       "scheme-relative url"},
      {"{\"max_age\":1, \"endpoints\": [{\"url\":\"relative/path\"}]}",
       "path relative url"},
      {"{\"max_age\":1, \"endpoints\": [{\"url\":\"http://insecure/\"}]}",
       "insecure url"},
      {"{\"endpoints\": [{\"url\":\"https://endpoint/\"}]}", "missing max_age"},
      {"{\"max_age\":\"\", \"endpoints\": [{\"url\":\"https://endpoint/\"}]}",
       "non-integer max_age"},
      {"{\"max_age\":-1, \"endpoints\": [{\"url\":\"https://endpoint/\"}]}",
       "negative max_age"},
      {"{\"max_age\":1, \"group\":0, "
       "\"endpoints\": [{\"url\":\"https://endpoint/\"}]}",
       "non-string group"},

      // Note that a non-boolean include_subdomains field is *not* invalid, per
      // the spec.

      // Priority should be a nonnegative integer.
      {"{\"max_age\":1, "
       "\"endpoints\": [{\"url\":\"https://endpoint/\",\"priority\":\"\"}]}",
       "non-integer priority"},
      {"{\"max_age\":1, "
       "\"endpoints\": [{\"url\":\"https://endpoint/\",\"priority\":-1}]}",
       "negative priority"},

      // Weight should be a non-negative integer.
      {"{\"max_age\":1, "
       "\"endpoints\": [{\"url\":\"https://endpoint/\",\"weight\":\"\"}]}",
       "non-integer weight"},
      {"{\"max_age\":1, "
       "\"endpoints\": [{\"url\":\"https://endpoint/\",\"weight\":-1}]}",
       "negative weight"},

      {"[{\"max_age\":1, \"endpoints\": [{\"url\":\"https://a/\"}]},"
       "{\"max_age\":1, \"endpoints\": [{\"url\":\"https://b/\"}]}]",
       "wrapped in list"}};

  base::HistogramTester histograms;
  int invalid_case_count = 0;

  for (const auto& test_case : kInvalidHeaderTestCases) {
    ParseHeader(kNak_, kOrigin1_, test_case.header_value);
    invalid_case_count++;

    EXPECT_EQ(0u, cache()->GetEndpointCount())
        << "Invalid Report-To header (" << test_case.description << ": \""
        << test_case.header_value << "\") parsed as valid.";
    histograms.ExpectBucketCount(
        kReportingHeaderTypeHistogram,
        ReportingHeaderParser::ReportingHeaderType::kReportToInvalid,
        invalid_case_count);
    if (mock_store()) {
      mock_store()->Flush();
      EXPECT_EQ(0, mock_store()->StoredEndpointsCount());
      EXPECT_EQ(0, mock_store()->StoredEndpointGroupsCount());
    }
  }
  histograms.ExpectBucketCount(
      kReportingHeaderTypeHistogram,
      ReportingHeaderParser::ReportingHeaderType::kReportTo, 0);
}

TEST_P(ReportingHeaderParserTest, Basic) {
  std::vector<ReportingEndpoint::EndpointInfo> endpoints = {{kEndpoint1_}};
  base::HistogramTester histograms;

  std::string header =
      ConstructHeaderGroupString(MakeEndpointGroup(kGroup1_, endpoints));

  ParseHeader(kNak_, kOrigin1_, header);
  EXPECT_EQ(1u, cache()->GetEndpointGroupCountForTesting());
  histograms.ExpectBucketCount(
      kReportingHeaderTypeHistogram,
      ReportingHeaderParser::ReportingHeaderType::kReportTo, 1);
  EXPECT_TRUE(
      EndpointGroupExistsInCache(kGroupKey11_, OriginSubdomains::DEFAULT));
  EXPECT_TRUE(ClientExistsInCacheForOrigin(kOrigin1_));
  EXPECT_EQ(1u, cache()->GetEndpointCount());
  ReportingEndpoint endpoint = FindEndpointInCache(kGroupKey11_, kEndpoint1_);
  ASSERT_TRUE(endpoint);
  EXPECT_EQ(kOrigin1_, endpoint.group_key.origin);
  EXPECT_EQ(kGroup1_, endpoint.group_key.group_name);
  EXPECT_EQ(kEndpoint1_, endpoint.info.url);
  EXPECT_EQ(ReportingEndpoint::EndpointInfo::kDefaultPriority,
            endpoint.info.priority);
  EXPECT_EQ(ReportingEndpoint::EndpointInfo::kDefaultWeight,
            endpoint.info.weight);

  if (mock_store()) {
    mock_store()->Flush();
    EXPECT_EQ(1, mock_store()->StoredEndpointsCount());
    EXPECT_EQ(1, mock_store()->StoredEndpointGroupsCount());
    MockPersistentReportingStore::CommandList expected_commands;
    expected_commands.emplace_back(CommandType::ADD_REPORTING_ENDPOINT,
                                   kGroupKey11_, kEndpoint1_);
    expected_commands.emplace_back(CommandType::ADD_REPORTING_ENDPOINT_GROUP,
                                   kGroupKey11_);
    EXPECT_THAT(mock_store()->GetAllCommands(),
                testing::IsSupersetOf(expected_commands));
  }
}

TEST_P(ReportingHeaderParserTest, PathAbsoluteURLEndpoint) {
  std::string header =
      "{\"group\": \"group1\", \"max_age\":1, \"endpoints\": "
      "[{\"url\":\"/path-absolute-url\"}]}";
  base::HistogramTester histograms;

  ParseHeader(kNak_, kOrigin1_, header);
  EXPECT_EQ(1u, cache()->GetEndpointGroupCountForTesting());
  histograms.ExpectBucketCount(
      kReportingHeaderTypeHistogram,
      ReportingHeaderParser::ReportingHeaderType::kReportTo, 1);
  EXPECT_TRUE(
      EndpointGroupExistsInCache(kGroupKey11_, OriginSubdomains::DEFAULT));
  EXPECT_TRUE(ClientExistsInCacheForOrigin(kOrigin1_));
  EXPECT_EQ(1u, cache()->GetEndpointCount());
  ReportingEndpoint endpoint =
      FindEndpointInCache(kGroupKey11_, kEndpointPathAbsolute_);
  ASSERT_TRUE(endpoint);
  EXPECT_EQ(kOrigin1_, endpoint.group_key.origin);
  EXPECT_EQ(kGroup1_, endpoint.group_key.group_name);
  EXPECT_EQ(kEndpointPathAbsolute_, endpoint.info.url);
  EXPECT_EQ(ReportingEndpoint::EndpointInfo::kDefaultPriority,
            endpoint.info.priority);
  EXPECT_EQ(ReportingEndpoint::EndpointInfo::kDefaultWeight,
            endpoint.info.weight);

  if (mock_store()) {
    mock_store()->Flush();
    EXPECT_EQ(1, mock_store()->StoredEndpointsCount());
    EXPECT_EQ(1, mock_store()->StoredEndpointGroupsCount());
    MockPersistentReportingStore::CommandList expected_commands;
    expected_commands.emplace_back(
        CommandType::ADD_REPORTING_ENDPOINT,
        ReportingEndpoint(kGroupKey11_, ReportingEndpoint::EndpointInfo{
                                            kEndpointPathAbsolute_}));
    expected_commands.emplace_back(
        CommandType::ADD_REPORTING_ENDPOINT_GROUP,
        CachedReportingEndpointGroup(
            kGroupKey11_, OriginSubdomains::DEFAULT /* irrelevant */,
            base::Time() /* irrelevant */, base::Time() /* irrelevant */));
    EXPECT_THAT(mock_store()->GetAllCommands(),
                testing::IsSupersetOf(expected_commands));
  }
}

TEST_P(ReportingHeaderParserTest, OmittedGroupName) {
  ReportingEndpointGroupKey kGroupKey(kNak_, kOrigin1_, "default",
                                      ReportingTargetType::kDeveloper);
  std::vector<ReportingEndpoint::EndpointInfo> endpoints = {{kEndpoint1_}};
  std::string header =
      ConstructHeaderGroupString(MakeEndpointGroup(std::string(), endpoints));

  ParseHeader(kNak_, kOrigin1_, header);
  EXPECT_EQ(1u, cache()->GetEndpointGroupCountForTesting());
  EXPECT_TRUE(EndpointGroupExistsInCache(kGroupKey, OriginSubdomains::DEFAULT));
  EXPECT_TRUE(ClientExistsInCacheForOrigin(kOrigin1_));
  EXPECT_EQ(1u, cache()->GetEndpointCount());
  ReportingEndpoint endpoint = FindEndpointInCache(kGroupKey, kEndpoint1_);
  ASSERT_TRUE(endpoint);
  EXPECT_EQ(kOrigin1_, endpoint.group_key.origin);
  EXPECT_EQ("default", endpoint.group_key.group_name);
  EXPECT_EQ(kEndpoint1_, endpoint.info.url);
  EXPECT_EQ(ReportingEndpoint::EndpointInfo::kDefaultPriority,
            endpoint.info.priority);
  EXPECT_EQ(ReportingEndpoint::EndpointInfo::kDefaultWeight,
            endpoint.info.weight);

  if (mock_store()) {
    mock_store()->Flush();
    EXPECT_EQ(1, mock_store()->StoredEndpointsCount());
    EXPECT_EQ(1, mock_store()->StoredEndpointGroupsCount());
    MockPersistentReportingStore::CommandList expected_commands;
    expected_commands.emplace_back(CommandType::ADD_REPORTING_ENDPOINT,
                                   kGroupKey, kEndpoint1_);
    expected_commands.emplace_back(CommandType::ADD_REPORTING_ENDPOINT_GROUP,
                                   kGroupKey);
    EXPECT_THAT(mock_store()->GetAllCommands(),
                testing::IsSupersetOf(expected_commands));
  }
}

TEST_P(ReportingHeaderParserTest, IncludeSubdomainsTrue) {
  std::vector<ReportingEndpoint::EndpointInfo> endpoints = {{kEndpoint1_}};

  std::string header = ConstructHeaderGroupString(
      MakeEndpointGroup(kGroup1_, endpoints, OriginSubdomains::INCLUDE));
  ParseHeader(kNak_, kOrigin1_, header);

  EXPECT_EQ(1u, cache()->GetEndpointGroupCountForTesting());
  EXPECT_TRUE(
      EndpointGroupExistsInCache(kGroupKey11_, OriginSubdomains::INCLUDE));
  EXPECT_EQ(1u, cache()->GetEndpointCount());
  EXPECT_TRUE(EndpointExistsInCache(kGroupKey11_, kEndpoint1_));

  if (mock_store()) {
    mock_store()->Flush();
    EXPECT_EQ(1, mock_store()->StoredEndpointsCount());
    EXPECT_EQ(1, mock_store()->StoredEndpointGroupsCount());
    MockPersistentReportingStore::CommandList expected_commands;
    expected_commands.emplace_back(CommandType::ADD_REPORTING_ENDPOINT,
                                   kGroupKey11_, kEndpoint1_);
    expected_commands.emplace_back(CommandType::ADD_REPORTING_ENDPOINT_GROUP,
                                   kGroupKey11_);
    EXPECT_THAT(mock_store()->GetAllCommands(),
                testing::IsSupersetOf(expected_commands));
  }
}

TEST_P(ReportingHeaderParserTest, IncludeSubdomainsFalse) {
  std::vector<ReportingEndpoint::EndpointInfo> endpoints = {{kEndpoint1_}};

  std::string header = ConstructHeaderGroupString(
      MakeEndpointGroup(kGroup1_, endpoints, OriginSubdomains::EXCLUDE),
      false /* omit_defaults */);
  ParseHeader(kNak_, kOrigin1_, header);

  EXPECT_EQ(1u, cache()->GetEndpointGroupCountForTesting());
  EXPECT_TRUE(
      EndpointGroupExistsInCache(kGroupKey11_, OriginSubdomains::EXCLUDE));
  EXPECT_EQ(1u, cache()->GetEndpointCount());
  EXPECT_TRUE(EndpointExistsInCache(kGroupKey11_, kEndpoint1_));

  if (mock_store()) {
    mock_store()->Flush();
    EXPECT_EQ(1, mock_store()->StoredEndpointsCount());
    EXPECT_EQ(1, mock_store()->StoredEndpointGroupsCount());
    MockPersistentReportingStore::CommandList expected_commands;
    expected_commands.emplace_back(CommandType::ADD_REPORTING_ENDPOINT,
                                   kGroupKey11_, kEndpoint1_);
    expected_commands.emplace_back(CommandType::ADD_REPORTING_ENDPOINT_GROUP,
                                   kGroupKey11_);
    EXPECT_THAT(mock_store()->GetAllCommands(),
                testing::IsSupersetOf(expected_commands));
  }
}

TEST_P(ReportingHeaderParserTest, IncludeSubdomainsEtldRejected) {
  ReportingEndpointGroupKey kGroupKey(kNak_, kOriginEtld_, kGroup1_,
                                      ReportingTargetType::kDeveloper);
  std::vector<ReportingEndpoint::EndpointInfo> endpoints = {{kEndpoint1_}};

  std::string header = ConstructHeaderGroupString(
      MakeEndpointGroup(kGroup1_, endpoints, OriginSubdomains::INCLUDE));
  ParseHeader(kNak_, kOriginEtld_, header);

  EXPECT_EQ(0u, cache()->GetEndpointGroupCountForTesting());
  EXPECT_FALSE(
      EndpointGroupExistsInCache(kGroupKey, OriginSubdomains::INCLUDE));
  EXPECT_EQ(0u, cache()->GetEndpointCount());
  EXPECT_FALSE(EndpointExistsInCache(kGroupKey, kEndpoint1_));
}

TEST_P(ReportingHeaderParserTest, NonIncludeSubdomainsEtldAccepted) {
  ReportingEndpointGroupKey kGroupKey(kNak_, kOriginEtld_, kGroup1_,
                                      ReportingTargetType::kDeveloper);
  std::vector<ReportingEndpoint::EndpointInfo> endpoints = {{kEndpoint1_}};

  std::string header = ConstructHeaderGroupString(
      MakeEndpointGroup(kGroup1_, endpoints, OriginSubdomains::EXCLUDE));
  ParseHeader(kNak_, kOriginEtld_, header);

  EXPECT_EQ(1u, cache()->GetEndpointGroupCountForTesting());
  EXPECT_TRUE(EndpointGroupExistsInCache(kGroupKey, OriginSubdomains::EXCLUDE));
  EXPECT_EQ(1u, cache()->GetEndpointCount());
  EXPECT_TRUE(EndpointExistsInCache(kGroupKey, kEndpoint1_));
}

TEST_P(ReportingHeaderParserTest, IncludeSubdomainsNotBoolean) {
  std::string header =
      "{\"group\": \"" + kGroup1_ +
      "\", "
      "\"max_age\":86400, \"include_subdomains\": \"NotABoolean\", "
      "\"endpoints\": [{\"url\":\"" +
      kEndpoint1_.spec() + "\"}]}";
  ParseHeader(kNak_, kOrigin1_, header);

  EXPECT_EQ(1u, cache()->GetEndpointGroupCountForTesting());
  EXPECT_TRUE(
      EndpointGroupExistsInCache(kGroupKey11_, OriginSubdomains::DEFAULT));
  EXPECT_EQ(1u, cache()->GetEndpointCount());
  EXPECT_TRUE(EndpointExistsInCache(kGroupKey11_, kEndpoint1_));

  if (mock_store()) {
    mock_store()->Flush();
    EXPECT_EQ(1, mock_store()->StoredEndpointsCount());
    EXPECT_EQ(1, mock_store()->StoredEndpointGroupsCount());
    MockPersistentReportingStore::CommandList expected_commands;
    expected_commands.emplace_back(CommandType::ADD_REPORTING_ENDPOINT,
                                   kGroupKey11_, kEndpoint1_);
    expected_commands.emplace_back(CommandType::ADD_REPORTING_ENDPOINT_GROUP,
                                   kGroupKey11_);
    EXPECT_THAT(mock_store()->GetAllCommands(),
                testing::IsSupersetOf(expected_commands));
  }
}

TEST_P(ReportingHeaderParserTest, NonDefaultPriority) {
  const int kNonDefaultPriority = 10;
  std::vector<ReportingEndpoint::EndpointInfo> endpoints = {
      {kEndpoint1_, kNonDefaultPriority}};

  std::string header =
      ConstructHeaderGroupString(MakeEndpointGroup(kGroup1_, endpoints));
  ParseHeader(kNak_, kOrigin1_, header);

  EXPECT_EQ(1u, cache()->GetEndpointGroupCountForTesting());
  EXPECT_TRUE(
      EndpointGroupExistsInCache(kGroupKey11_, OriginSubdomains::DEFAULT));
  EXPECT_EQ(1u, cache()->GetEndpointCount());
  ReportingEndpoint endpoint = FindEndpointInCache(kGroupKey11_, kEndpoint1_);
  ASSERT_TRUE(endpoint);
  EXPECT_EQ(kNonDefaultPriority, endpoint.info.priority);
  EXPECT_EQ(ReportingEndpoint::EndpointInfo::kDefaultWeight,
            endpoint.info.weight);

  if (mock_store()) {
    mock_store()->Flush();
    EXPECT_EQ(1, mock_store()->StoredEndpointsCount());
    EXPECT_EQ(1, mock_store()->StoredEndpointGroupsCount());
    MockPersistentReportingStore::CommandList expected_commands;
    expected_commands.emplace_back(CommandType::ADD_REPORTING_ENDPOINT,
                                   kGroupKey11_, kEndpoint1_);
    expected_commands.emplace_back(CommandType::ADD_REPORTING_ENDPOINT_GROUP,
                                   kGroupKey11_);
    EXPECT_THAT(mock_store()->GetAllCommands(),
                testing::IsSupersetOf(expected_commands));
  }
}

TEST_P(ReportingHeaderParserTest, NonDefaultWeight) {
  const int kNonDefaultWeight = 10;
  std::vector<ReportingEndpoint::EndpointInfo> endpoints = {
      {kEndpoint1_, ReportingEndpoint::EndpointInfo::kDefaultPriority,
       kNonDefaultWeight}};

  std::string header =
      ConstructHeaderGroupString(MakeEndpointGroup(kGroup1_, endpoints));
  ParseHeader(kNak_, kOrigin1_, header);

  EXPECT_EQ(1u, cache()->GetEndpointGroupCountForTesting());
  EXPECT_TRUE(
      EndpointGroupExistsInCache(kGroupKey11_, OriginSubdomains::DEFAULT));
  EXPECT_EQ(1u, cache()->GetEndpointCount());
  ReportingEndpoint endpoint = FindEndpointInCache(kGroupKey11_, kEndpoint1_);
  ASSERT_TRUE(endpoint);
  EXPECT_EQ(ReportingEndpoint::EndpointInfo::kDefaultPriority,
            endpoint.info.priority);
  EXPECT_EQ(kNonDefaultWeight, endpoint.info.weight);

  if (mock_store()) {
    mock_store()->Flush();
    EXPECT_EQ(1, mock_store()->StoredEndpointsCount());
    EXPECT_EQ(1, mock_store()->StoredEndpointGroupsCount());
    MockPersistentReportingStore::CommandList expected_commands;
    expected_commands.emplace_back(CommandType::ADD_REPORTING_ENDPOINT,
                                   kGroupKey11_, kEndpoint1_);
    expected_commands.emplace_back(CommandType::ADD_REPORTING_ENDPOINT_GROUP,
                                   kGroupKey11_);
    EXPECT_THAT(mock_store()->GetAllCommands(),
                testing::IsSupersetOf(expected_commands));
  }
}

TEST_P(ReportingHeaderParserTest, MaxAge) {
  const int kMaxAgeSecs = 100;
  base::TimeDelta ttl = base::Seconds(kMaxAgeSecs);
  base::Time expires = clock()->Now() + ttl;

  std::vector<ReportingEndpoint::EndpointInfo> endpoints = {{kEndpoint1_}};

  std::string header = ConstructHeaderGroupString(
      MakeEndpointGroup(kGroup1_, endpoints, OriginSubdomains::DEFAULT, ttl));

  ParseHeader(kNak_, kOrigin1_, header);
  EXPECT_EQ(1u, cache()->GetEndpointGroupCountForTesting());
  EXPECT_TRUE(EndpointGroupExistsInCache(kGroupKey11_,
                                         OriginSubdomains::DEFAULT, expires));

  if (mock_store()) {
    mock_store()->Flush();
    EXPECT_EQ(1, mock_store()->StoredEndpointsCount());
    EXPECT_EQ(1, mock_store()->StoredEndpointGroupsCount());
    MockPersistentReportingStore::CommandList expected_commands;
    expected_commands.emplace_back(CommandType::ADD_REPORTING_ENDPOINT,
                                   kGroupKey11_, kEndpoint1_);
    expected_commands.emplace_back(CommandType::ADD_REPORTING_ENDPOINT_GROUP,
                                   kGroupKey11_);
    EXPECT_THAT(mock_store()->GetAllCommands(),
                testing::IsSupersetOf(expected_commands));
  }
}

TEST_P(ReportingHeaderParserTest, MultipleEndpointsSameGroup) {
  std::vector<ReportingEndpoint::EndpointInfo> endpoints = {{kEndpoint1_},
                                                            {kEndpoint2_}};
  std::string header =
      ConstructHeaderGroupString(MakeEndpointGroup(kGroup1_, endpoints));

  ParseHeader(kNak_, kOrigin1_, header);
  EXPECT_EQ(1u, cache()->GetEndpointGroupCountForTesting());
  EXPECT_TRUE(
      EndpointGroupExistsInCache(kGroupKey11_, OriginSubdomains::DEFAULT));
  EXPECT_TRUE(ClientExistsInCacheForOrigin(kOrigin1_));
  EXPECT_EQ(2u, cache()->GetEndpointCount());
  ReportingEndpoint endpoint = FindEndpointInCache(kGroupKey11_, kEndpoint1_);
  ASSERT_TRUE(endpoint);
  EXPECT_EQ(kOrigin1_, endpoint.group_key.origin);
  EXPECT_EQ(kGroup1_, endpoint.group_key.group_name);
  EXPECT_EQ(kEndpoint1_, endpoint.info.url);
  EXPECT_EQ(ReportingEndpoint::EndpointInfo::kDefaultPriority,
            endpoint.info.priority);
  EXPECT_EQ(ReportingEndpoint::EndpointInfo::kDefaultWeight,
            endpoint.info.weight);

  ReportingEndpoint endpoint2 = FindEndpointInCache(kGroupKey11_, kEndpoint2_);
  ASSERT_TRUE(endpoint2);
  EXPECT_EQ(kOrigin1_, endpoint2.group_key.origin);
  EXPECT_EQ(kGroup1_, endpoint2.group_key.group_name);
  EXPECT_EQ(kEndpoint2_, endpoint2.info.url);
  EXPECT_EQ(ReportingEndpoint::EndpointInfo::kDefaultPriority,
            endpoint2.info.priority);
  EXPECT_EQ(ReportingEndpoint::EndpointInfo::kDefaultWeight,
            endpoint2.info.weight);

  if (mock_store()) {
    mock_store()->Flush();
    EXPECT_EQ(2, mock_store()->StoredEndpointsCount());
    EXPECT_EQ(1, mock_store()->StoredEndpointGroupsCount());
    MockPersistentReportingStore::CommandList expected_commands;
    expected_commands.emplace_back(CommandType::ADD_REPORTING_ENDPOINT,
                                   kGroupKey11_, kEndpoint1_);
    expected_commands.emplace_back(CommandType::ADD_REPORTING_ENDPOINT,
                                   kGroupKey11_, kEndpoint2_);
    expected_commands.emplace_back(CommandType::ADD_REPORTING_ENDPOINT_GROUP,
                                   kGroupKey11_);
    EXPECT_THAT(mock_store()->GetAllCommands(),
                testing::IsSupersetOf(expected_commands));
  }
}

TEST_P(ReportingHeaderParserTest, MultipleEndpointsDifferentGroups) {
  std::vector<ReportingEndpoint::EndpointInfo> endpoints1 = {{kEndpoint1_}};
  std::vector<ReportingEndpoint::EndpointInfo> endpoints2 = {{kEndpoint1_}};
  std::string header =
      ConstructHeaderGroupString(MakeEndpointGroup(kGroup1_, endpoints1)) +
      ", " +
      ConstructHeaderGroupString(MakeEndpointGroup(kGroup2_, endpoints2));

  ParseHeader(kNak_, kOrigin1_, header);
  EXPECT_EQ(2u, cache()->GetEndpointGroupCountForTesting());
  EXPECT_TRUE(
      EndpointGroupExistsInCache(kGroupKey11_, OriginSubdomains::DEFAULT));
  EXPECT_TRUE(
      EndpointGroupExistsInCache(kGroupKey12_, OriginSubdomains::DEFAULT));
  EXPECT_TRUE(ClientExistsInCacheForOrigin(kOrigin1_));

  EXPECT_EQ(2u, cache()->GetEndpointCount());
  ReportingEndpoint endpoint = FindEndpointInCache(kGroupKey11_, kEndpoint1_);
  ASSERT_TRUE(endpoint);
  EXPECT_EQ(kOrigin1_, endpoint.group_key.origin);
  EXPECT_EQ(kGroup1_, endpoint.group_key.group_name);
  EXPECT_EQ(ReportingEndpoint::EndpointInfo::kDefaultPriority,
            endpoint.info.priority);
  EXPECT_EQ(ReportingEndpoint::EndpointInfo::kDefaultWeight,
            endpoint.info.weight);

  ReportingEndpoint endpoint2 = FindEndpointInCache(kGroupKey12_, kEndpoint1_);
  ASSERT_TRUE(endpoint2);
  EXPECT_EQ(kOrigin1_, endpoint2.group_key.origin);
  EXPECT_EQ(kGroup2_, endpoint2.group_key.group_name);
  EXPECT_EQ(ReportingEndpoint::EndpointInfo::kDefaultPriority,
            endpoint2.info.priority);
  EXPECT_EQ(ReportingEndpoint::EndpointInfo::kDefaultWeight,
            endpoint2.info.weight);

  if (mock_store()) {
    mock_store()->Flush();
    EXPECT_EQ(2, mock_store()->StoredEndpointsCount());
    EXPECT_EQ(2, mock_store()->StoredEndpointGroupsCount());
    MockPersistentReportingStore::CommandList expected_commands;
    expected_commands.emplace_back(CommandType::ADD_REPORTING_ENDPOINT,
                                   kGroupKey11_, kEndpoint1_);
    expected_commands.emplace_back(CommandType::ADD_REPORTING_ENDPOINT,
                                   kGroupKey12_, kEndpoint1_);
    expected_commands.emplace_back(CommandType::ADD_REPORTING_ENDPOINT_GROUP,
                                   kGroupKey11_);
    expected_commands.emplace_back(CommandType::ADD_REPORTING_ENDPOINT_GROUP,
                                   kGroupKey12_);
    EXPECT_THAT(mock_store()->GetAllCommands(),
                testing::IsSupersetOf(expected_commands));
  }
}

TEST_P(ReportingHeaderParserTest, MultipleHeadersFromDifferentOrigins) {
  // First origin sets a header with two endpoints in the same group.
  std::vector<ReportingEndpoint::EndpointInfo> endpoints1 = {{kEndpoint1_},
                                                             {kEndpoint2_}};
  std::string header1 =
      ConstructHeaderGroupString(MakeEndpointGroup(kGroup1_, endpoints1));
  ParseHeader(kNak_, kOrigin1_, header1);

  // Second origin has two endpoint groups.
  std::vector<ReportingEndpoint::EndpointInfo> endpoints2 = {{kEndpoint1_}};
  std::vector<ReportingEndpoint::EndpointInfo> endpoints3 = {{kEndpoint2_}};
  std::string header2 =
      ConstructHeaderGroupString(MakeEndpointGroup(kGroup1_, endpoints2)) +
      ", " +
      ConstructHeaderGroupString(MakeEndpointGroup(kGroup2_, endpoints3));
  ParseHeader(kNak_, kOrigin2_, header2);

  EXPECT_TRUE(ClientExistsInCacheForOrigin(kOrigin1_));
  EXPECT_TRUE(ClientExistsInCacheForOrigin(kOrigin2_));

  EXPECT_EQ(3u, cache()->GetEndpointGroupCountForTesting());
  EXPECT_TRUE(
      EndpointGroupExistsInCache(kGroupKey11_, OriginSubdomains::DEFAULT));
  EXPECT_TRUE(
      EndpointGroupExistsInCache(kGroupKey21_, OriginSubdomains::DEFAULT));
  EXPECT_TRUE(
      EndpointGroupExistsInCache(kGroupKey22_, OriginSubdomains::DEFAULT));

  EXPECT_EQ(4u, cache()->GetEndpointCount());
  EXPECT_TRUE(FindEndpointInCache(kGroupKey11_, kEndpoint1_));
  EXPECT_TRUE(FindEndpointInCache(kGroupKey11_, kEndpoint2_));
  EXPECT_TRUE(FindEndpointInCache(kGroupKey21_, kEndpoint1_));
  EXPECT_TRUE(FindEndpointInCache(kGroupKey22_, kEndpoint2_));

  if (mock_store()) {
    mock_store()->Flush();
    EXPECT_EQ(4, mock_store()->StoredEndpointsCount());
    EXPECT_EQ(3, mock_store()->StoredEndpointGroupsCount());
    MockPersistentReportingStore::CommandList expected_commands;
    expected_commands.emplace_back(CommandType::ADD_REPORTING_ENDPOINT,
                                   kGroupKey11_, kEndpoint1_);
    expected_commands.emplace_back(CommandType::ADD_REPORTING_ENDPOINT,
                                   kGroupKey11_, kEndpoint2_);
    expected_commands.emplace_back(CommandType::ADD_REPORTING_ENDPOINT,
                                   kGroupKey21_, kEndpoint1_);
    expected_commands.emplace_back(CommandType::ADD_REPORTING_ENDPOINT,
                                   kGroupKey22_, kEndpoint2_);
    expected_commands.emplace_back(CommandType::ADD_REPORTING_ENDPOINT_GROUP,
                                   kGroupKey11_);
    expected_commands.emplace_back(CommandType::ADD_REPORTING_ENDPOINT_GROUP,
                                   kGroupKey21_);
    expected_commands.emplace_back(CommandType::ADD_REPORTING_ENDPOINT_GROUP,
                                   kGroupKey22_);
    EXPECT_THAT(mock_store()->GetAllCommands(),
                testing::IsSupersetOf(expected_commands));
  }
}

// Test that each combination of NAK, origin, and group name is considered
// distinct.
// See also: ReportingCacheTest.ClientsKeyedByEndpointGroupKey
TEST_P(ReportingHeaderParserTest, EndpointGroupKey) {
  // Raise the endpoint limits for this test.
  ReportingPolicy policy;
  policy.max_endpoints_per_origin = 5;  // This test should use 4.
  policy.max_endpoint_count = 20;       // This test should use 16.
  UsePolicy(policy);

  std::vector<ReportingEndpoint::EndpointInfo> endpoints1 = {{kEndpoint1_},
                                                             {kEndpoint2_}};
  std::string header1 =
      ConstructHeaderGroupString(MakeEndpointGroup(kGroup1_, endpoints1)) +
      ", " +
      ConstructHeaderGroupString(MakeEndpointGroup(kGroup2_, endpoints1));

  const ReportingEndpointGroupKey kOtherGroupKey11 = ReportingEndpointGroupKey(
      kOtherNak_, kOrigin1_, kGroup1_, ReportingTargetType::kDeveloper);
  const ReportingEndpointGroupKey kOtherGroupKey21 = ReportingEndpointGroupKey(
      kOtherNak_, kOrigin2_, kGroup1_, ReportingTargetType::kDeveloper);
  const ReportingEndpointGroupKey kOtherGroupKey12 = ReportingEndpointGroupKey(
      kOtherNak_, kOrigin1_, kGroup2_, ReportingTargetType::kDeveloper);
  const ReportingEndpointGroupKey kOtherGroupKey22 = ReportingEndpointGroupKey(
      kOtherNak_, kOrigin2_, kGroup2_, ReportingTargetType::kDeveloper);

  const struct {
    NetworkAnonymizationKey network_anonymization_key;
    GURL url;
    ReportingEndpointGroupKey group1_key;
    ReportingEndpointGroupKey group2_key;
  } kHeaderSources[] = {
      {kNak_, kUrl1_, kGroupKey11_, kGroupKey12_},
      {kNak_, kUrl2_, kGroupKey21_, kGroupKey22_},
      {kOtherNak_, kUrl1_, kOtherGroupKey11, kOtherGroupKey12},
      {kOtherNak_, kUrl2_, kOtherGroupKey21, kOtherGroupKey22},
  };

  size_t endpoint_group_count = 0u;
  size_t endpoint_count = 0u;
  MockPersistentReportingStore::CommandList expected_commands;

  // Set 2 endpoints in each of 2 groups for each of 2x2 combinations of
  // (NAK, origin).
  for (const auto& source : kHeaderSources) {
    // Verify pre-parsing state
    EXPECT_FALSE(FindEndpointInCache(source.group1_key, kEndpoint1_));
    EXPECT_FALSE(FindEndpointInCache(source.group1_key, kEndpoint2_));
    EXPECT_FALSE(FindEndpointInCache(source.group2_key, kEndpoint1_));
    EXPECT_FALSE(FindEndpointInCache(source.group2_key, kEndpoint2_));
    EXPECT_FALSE(EndpointGroupExistsInCache(source.group1_key,
                                            OriginSubdomains::DEFAULT));
    EXPECT_FALSE(EndpointGroupExistsInCache(source.group2_key,
                                            OriginSubdomains::DEFAULT));

    ParseHeader(source.network_anonymization_key,
                url::Origin::Create(source.url), header1);
    endpoint_group_count += 2u;
    endpoint_count += 4u;
    EXPECT_EQ(endpoint_group_count, cache()->GetEndpointGroupCountForTesting());
    EXPECT_EQ(endpoint_count, cache()->GetEndpointCount());

    // Verify post-parsing state
    EXPECT_TRUE(FindEndpointInCache(source.group1_key, kEndpoint1_));
    EXPECT_TRUE(FindEndpointInCache(source.group1_key, kEndpoint2_));
    EXPECT_TRUE(FindEndpointInCache(source.group2_key, kEndpoint1_));
    EXPECT_TRUE(FindEndpointInCache(source.group2_key, kEndpoint2_));
    EXPECT_TRUE(EndpointGroupExistsInCache(source.group1_key,
                                           OriginSubdomains::DEFAULT));
    EXPECT_TRUE(EndpointGroupExistsInCache(source.group2_key,
                                           OriginSubdomains::DEFAULT));

    if (mock_store()) {
      mock_store()->Flush();
      EXPECT_EQ(static_cast<int>(endpoint_count),
                mock_store()->StoredEndpointsCount());
      EXPECT_EQ(static_cast<int>(endpoint_group_count),
                mock_store()->StoredEndpointGroupsCount());
      expected_commands.emplace_back(CommandType::ADD_REPORTING_ENDPOINT,
                                     source.group1_key, kEndpoint1_);
      expected_commands.emplace_back(CommandType::ADD_REPORTING_ENDPOINT,
                                     source.group1_key, kEndpoint2_);
      expected_commands.emplace_back(CommandType::ADD_REPORTING_ENDPOINT_GROUP,
                                     source.group1_key);
      expected_commands.emplace_back(CommandType::ADD_REPORTING_ENDPOINT,
                                     source.group2_key, kEndpoint1_);
      expected_commands.emplace_back(CommandType::ADD_REPORTING_ENDPOINT,
                                     source.group2_key, kEndpoint2_);
      expected_commands.emplace_back(CommandType::ADD_REPORTING_ENDPOINT_GROUP,
                                     source.group2_key);
      EXPECT_THAT(mock_store()->GetAllCommands(),
                  testing::IsSupersetOf(expected_commands));
    }
  }

  // Check that expected data is present in the ReportingCache at the end.
  for (const auto& source : kHeaderSources) {
    EXPECT_TRUE(FindEndpointInCache(source.group1_key, kEndpoint1_));
    EXPECT_TRUE(FindEndpointInCache(source.group1_key, kEndpoint2_));
    EXPECT_TRUE(FindEndpointInCache(source.group2_key, kEndpoint1_));
    EXPECT_TRUE(FindEndpointInCache(source.group2_key, kEndpoint2_));
    EXPECT_TRUE(EndpointGroupExistsInCache(source.group1_key,
                                           OriginSubdomains::DEFAULT));
    EXPECT_TRUE(EndpointGroupExistsInCache(source.group2_key,
                                           OriginSubdomains::DEFAULT));
    EXPECT_TRUE(cache()->ClientExistsForTesting(
        source.network_anonymization_key, url::Origin::Create(source.url)));
  }

  // Test updating existing configurations

  // This removes endpoint 1, updates the priority of endpoint 2, and adds
  // endpoint 3.
  std::vector<ReportingEndpoint::EndpointInfo> endpoints2 = {{kEndpoint2_, 2},
                                                             {kEndpoint3_}};
  // Removes group 1, updates include_subdomains for group 2.
  std::string header2 = ConstructHeaderGroupString(
      MakeEndpointGroup(kGroup2_, endpoints2, OriginSubdomains::INCLUDE));

  for (const auto& source : kHeaderSources) {
    // Verify pre-update state
    EXPECT_TRUE(EndpointGroupExistsInCache(source.group1_key,
                                           OriginSubdomains::DEFAULT));
    EXPECT_TRUE(EndpointGroupExistsInCache(source.group2_key,
                                           OriginSubdomains::DEFAULT));
    EXPECT_TRUE(FindEndpointInCache(source.group2_key, kEndpoint1_));
    ReportingEndpoint endpoint =
        FindEndpointInCache(source.group2_key, kEndpoint2_);
    EXPECT_TRUE(endpoint);
    EXPECT_EQ(ReportingEndpoint::EndpointInfo::kDefaultPriority,
              endpoint.info.priority);
    EXPECT_FALSE(FindEndpointInCache(source.group2_key, kEndpoint3_));

    ParseHeader(source.network_anonymization_key,
                url::Origin::Create(source.url), header2);
    endpoint_group_count--;
    endpoint_count -= 2;
    EXPECT_EQ(endpoint_group_count, cache()->GetEndpointGroupCountForTesting());
    EXPECT_EQ(endpoint_count, cache()->GetEndpointCount());

    // Verify post-update state
    EXPECT_FALSE(EndpointGroupExistsInCache(source.group1_key,
                                            OriginSubdomains::DEFAULT));
    EXPECT_TRUE(EndpointGroupExistsInCache(source.group2_key,
                                           OriginSubdomains::INCLUDE));
    EXPECT_FALSE(FindEndpointInCache(source.group2_key, kEndpoint1_));
    endpoint = FindEndpointInCache(source.group2_key, kEndpoint2_);
    EXPECT_TRUE(endpoint);
    EXPECT_EQ(2, endpoint.info.priority);
    EXPECT_TRUE(FindEndpointInCache(source.group2_key, kEndpoint3_));

    if (mock_store()) {
      mock_store()->Flush();
      EXPECT_EQ(static_cast<int>(endpoint_count),
                mock_store()->StoredEndpointsCount());
      EXPECT_EQ(static_cast<int>(endpoint_group_count),
                mock_store()->StoredEndpointGroupsCount());
      expected_commands.emplace_back(CommandType::DELETE_REPORTING_ENDPOINT,
                                     source.group1_key, kEndpoint1_);
      expected_commands.emplace_back(CommandType::DELETE_REPORTING_ENDPOINT,
                                     source.group1_key, kEndpoint2_);
      expected_commands.emplace_back(
          CommandType::DELETE_REPORTING_ENDPOINT_GROUP, source.group1_key);
      expected_commands.emplace_back(CommandType::DELETE_REPORTING_ENDPOINT,
                                     source.group2_key, kEndpoint1_);
      expected_commands.emplace_back(
          CommandType::UPDATE_REPORTING_ENDPOINT_DETAILS, source.group2_key,
          kEndpoint2_);
      expected_commands.emplace_back(CommandType::ADD_REPORTING_ENDPOINT,
                                     source.group2_key, kEndpoint3_);
      expected_commands.emplace_back(
          CommandType::UPDATE_REPORTING_ENDPOINT_GROUP_DETAILS,
          source.group2_key);
      EXPECT_THAT(mock_store()->GetAllCommands(),
                  testing::IsSupersetOf(expected_commands));
    }
  }

  // Check that expected data is present in the ReportingCache at the end.
  for (const auto& source : kHeaderSources) {
    EXPECT_FALSE(FindEndpointInCache(source.group1_key, kEndpoint1_));
    EXPECT_FALSE(FindEndpointInCache(source.group1_key, kEndpoint2_));
    EXPECT_FALSE(FindEndpointInCache(source.group2_key, kEndpoint1_));
    EXPECT_TRUE(FindEndpointInCache(source.group2_key, kEndpoint2_));
    EXPECT_TRUE(FindEndpointInCache(source.group2_key, kEndpoint3_));
    EXPECT_FALSE(EndpointGroupExistsInCache(source.group1_key,
                                            OriginSubdomains::DEFAULT));
    EXPECT_TRUE(EndpointGroupExistsInCache(source.group2_key,
                                           OriginSubdomains::INCLUDE));
    EXPECT_TRUE(cache()->ClientExistsForTesting(
        source.network_anonymization_key, url::Origin::Create(source.url)));
  }
}

TEST_P(ReportingHeaderParserTest,
       HeaderErroneouslyContainsMultipleGroupsOfSameName) {
  // Add a preexisting header to test that a header with multiple groups of the
  // same name is treated as if it specified a single group with the combined
  // set of specified endpoints. In particular, it must overwrite/update any
  // preexisting group all at once. See https://crbug.com/1116529.
  std::vector<ReportingEndpoint::EndpointInfo> preexisting = {{kEndpoint1_}};
  std::string preexisting_header =
      ConstructHeaderGroupString(MakeEndpointGroup(kGroup1_, preexisting));

  ParseHeader(kNak_, kOrigin1_, preexisting_header);
  EXPECT_TRUE(
      EndpointGroupExistsInCache(kGroupKey11_, OriginSubdomains::DEFAULT));
  EXPECT_EQ(1u, cache()->GetEndpointGroupCountForTesting());
  EXPECT_TRUE(ClientExistsInCacheForOrigin(kOrigin1_));
  EXPECT_EQ(1u, cache()->GetEndpointCount());
  ReportingEndpoint endpoint = FindEndpointInCache(kGroupKey11_, kEndpoint1_);
  ASSERT_TRUE(endpoint);

  if (mock_store()) {
    mock_store()->Flush();
    EXPECT_EQ(1, mock_store()->StoredEndpointsCount());
    EXPECT_EQ(1, mock_store()->StoredEndpointGroupsCount());
    MockPersistentReportingStore::CommandList expected_commands;
    expected_commands.emplace_back(CommandType::ADD_REPORTING_ENDPOINT,
                                   kGroupKey11_, kEndpoint1_);
    expected_commands.emplace_back(CommandType::ADD_REPORTING_ENDPOINT_GROUP,
                                   kGroupKey11_);
    EXPECT_THAT(mock_store()->GetAllCommands(),
                testing::IsSupersetOf(expected_commands));
    // Reset commands so we can check that the next part, adding the header with
    // duplicate groups, does not cause clearing of preexisting endpoints twice.
    mock_store()->ClearCommands();
  }

  std::vector<ReportingEndpoint::EndpointInfo> endpoints1 = {{kEndpoint1_}};
  std::vector<ReportingEndpoint::EndpointInfo> endpoints2 = {{kEndpoint2_}};
  std::string duplicate_groups_header =
      ConstructHeaderGroupString(MakeEndpointGroup(kGroup1_, endpoints1)) +
      ", " +
      ConstructHeaderGroupString(MakeEndpointGroup(kGroup1_, endpoints2));

  ParseHeader(kNak_, kOrigin1_, duplicate_groups_header);
  // Result is as if they set the two groups with the same name as one group.
  EXPECT_TRUE(
      EndpointGroupExistsInCache(kGroupKey11_, OriginSubdomains::DEFAULT));
  EXPECT_EQ(1u, cache()->GetEndpointGroupCountForTesting());

  EXPECT_TRUE(ClientExistsInCacheForOrigin(kOrigin1_));

  EXPECT_EQ(2u, cache()->GetEndpointCount());
  ReportingEndpoint endpoint1 = FindEndpointInCache(kGroupKey11_, kEndpoint1_);
  ASSERT_TRUE(endpoint);
  EXPECT_EQ(kOrigin1_, endpoint.group_key.origin);
  EXPECT_EQ(kGroup1_, endpoint.group_key.group_name);
  EXPECT_EQ(ReportingEndpoint::EndpointInfo::kDefaultPriority,
            endpoint.info.priority);
  EXPECT_EQ(ReportingEndpoint::EndpointInfo::kDefaultWeight,
            endpoint.info.weight);

  ReportingEndpoint endpoint2 = FindEndpointInCache(kGroupKey11_, kEndpoint2_);
  ASSERT_TRUE(endpoint2);
  EXPECT_EQ(kOrigin1_, endpoint2.group_key.origin);
  EXPECT_EQ(kGroup1_, endpoint2.group_key.group_name);
  EXPECT_EQ(ReportingEndpoint::EndpointInfo::kDefaultPriority,
            endpoint2.info.priority);
  EXPECT_EQ(ReportingEndpoint::EndpointInfo::kDefaultWeight,
            endpoint2.info.weight);

  if (mock_store()) {
    mock_store()->Flush();
    EXPECT_EQ(2, mock_store()->StoredEndpointsCount());
    EXPECT_EQ(1, mock_store()->StoredEndpointGroupsCount());
    MockPersistentReportingStore::CommandList expected_commands;
    expected_commands.emplace_back(
        CommandType::UPDATE_REPORTING_ENDPOINT_DETAILS, kGroupKey11_,
        kEndpoint1_);
    expected_commands.emplace_back(CommandType::ADD_REPORTING_ENDPOINT,
                                   kGroupKey11_, kEndpoint2_);
    expected_commands.emplace_back(
        CommandType::UPDATE_REPORTING_ENDPOINT_GROUP_DETAILS, kGroupKey11_);
    MockPersistentReportingStore::CommandList actual_commands =
        mock_store()->GetAllCommands();
    EXPECT_THAT(actual_commands, testing::IsSupersetOf(expected_commands));
    for (const auto& command : actual_commands) {
      EXPECT_NE(CommandType::DELETE_REPORTING_ENDPOINT, command.type);
      EXPECT_NE(CommandType::DELETE_REPORTING_ENDPOINT_GROUP, command.type);

      // The endpoint with URL kEndpoint1_ is only ever updated, not added anew.
      EXPECT_NE(
          MockPersistentReportingStore::Command(
              CommandType::ADD_REPORTING_ENDPOINT, kGroupKey11_, kEndpoint1_),
          command);
      // The group is only ever updated, not added anew.
      EXPECT_NE(MockPersistentReportingStore::Command(
                    CommandType::ADD_REPORTING_ENDPOINT_GROUP, kGroupKey11_),
                command);
    }
  }
}

TEST_P(ReportingHeaderParserTest,
       HeaderErroneouslyContainsGroupsWithRedundantEndpoints) {
  std::vector<ReportingEndpoint::EndpointInfo> endpoints = {{kEndpoint1_},
                                                            {kEndpoint1_}};
  std::string header =
      ConstructHeaderGroupString(MakeEndpointGroup(kGroup1_, endpoints));
  ParseHeader(kNak_, kOrigin1_, header);

  // We should dedupe the identical endpoint URLs.
  EXPECT_EQ(1u, cache()->GetEndpointCount());
  ASSERT_TRUE(FindEndpointInCache(kGroupKey11_, kEndpoint1_));

  EXPECT_TRUE(
      EndpointGroupExistsInCache(kGroupKey11_, OriginSubdomains::DEFAULT));
  EXPECT_EQ(1u, cache()->GetEndpointGroupCountForTesting());

  EXPECT_TRUE(ClientExistsInCacheForOrigin(kOrigin1_));
}

TEST_P(ReportingHeaderParserTest,
       HeaderErroneouslyContainsMultipleGroupsOfSameNameAndEndpoints) {
  std::vector<ReportingEndpoint::EndpointInfo> endpoints = {{kEndpoint1_}};
  std::string header =
      ConstructHeaderGroupString(MakeEndpointGroup(kGroup1_, endpoints)) +
      ", " + ConstructHeaderGroupString(MakeEndpointGroup(kGroup1_, endpoints));
  ParseHeader(kNak_, kOrigin1_, header);

  // We should dedupe the identical endpoint URLs, even when they're in
  // different group.
  EXPECT_EQ(1u, cache()->GetEndpointCount());
  ASSERT_TRUE(FindEndpointInCache(kGroupKey11_, kEndpoint1_));

  EXPECT_TRUE(
      EndpointGroupExistsInCache(kGroupKey11_, OriginSubdomains::DEFAULT));
  EXPECT_EQ(1u, cache()->GetEndpointGroupCountForTesting());

  EXPECT_TRUE(ClientExistsInCacheForOrigin(kOrigin1_));
}

TEST_P(ReportingHeaderParserTest,
       HeaderErroneouslyContainsGroupsOfSameNameAndOverlappingEndpoints) {
  std::vector<ReportingEndpoint::EndpointInfo> endpoints1 = {{kEndpoint1_},
                                                             {kEndpoint2_}};
  std::vector<ReportingEndpoint::EndpointInfo> endpoints2 = {{kEndpoint1_},
                                                             {kEndpoint3_}};
  std::string header =
      ConstructHeaderGroupString(MakeEndpointGroup(kGroup1_, endpoints1)) +
      ", " +
      ConstructHeaderGroupString(MakeEndpointGroup(kGroup1_, endpoints2));
  ParseHeader(kNak_, kOrigin1_, header);

  // We should dedupe the identical endpoint URLs, even when they're in
  // different group.
  EXPECT_EQ(3u, cache()->GetEndpointCount());
  ASSERT_TRUE(FindEndpointInCache(kGroupKey11_, kEndpoint1_));
  ASSERT_TRUE(FindEndpointInCache(kGroupKey11_, kEndpoint2_));
  ASSERT_TRUE(FindEndpointInCache(kGroupKey11_, kEndpoint3_));

  EXPECT_TRUE(
      EndpointGroupExistsInCache(kGroupKey11_, OriginSubdomains::DEFAULT));
  EXPECT_EQ(1u, cache()->GetEndpointGroupCountForTesting());

  EXPECT_TRUE(ClientExistsInCacheForOrigin(kOrigin1_));
}

TEST_P(ReportingHeaderParserTest, OverwriteOldHeader) {
  // First, the origin sets a header with two endpoints in the same group.
  std::vector<ReportingEndpoint::EndpointInfo> endpoints1 = {
      {kEndpoint1_, 10 /* priority */}, {kEndpoint2_}};
  std::string header1 =
      ConstructHeaderGroupString(MakeEndpointGroup(kGroup1_, endpoints1));
  ParseHeader(kNak_, kOrigin1_, header1);

  EXPECT_TRUE(ClientExistsInCacheForOrigin(kOrigin1_));
  EXPECT_EQ(1u, cache()->GetEndpointGroupCountForTesting());
  EXPECT_TRUE(
      EndpointGroupExistsInCache(kGroupKey11_, OriginSubdomains::DEFAULT));
  EXPECT_EQ(2u, cache()->GetEndpointCount());
  EXPECT_TRUE(FindEndpointInCache(kGroupKey11_, kEndpoint1_));
  EXPECT_TRUE(FindEndpointInCache(kGroupKey11_, kEndpoint2_));
  if (mock_store()) {
    mock_store()->Flush();
    EXPECT_EQ(2,
              mock_store()->CountCommands(CommandType::ADD_REPORTING_ENDPOINT));
    EXPECT_EQ(1, mock_store()->CountCommands(
                     CommandType::ADD_REPORTING_ENDPOINT_GROUP));
    MockPersistentReportingStore::CommandList expected_commands;
    expected_commands.emplace_back(CommandType::ADD_REPORTING_ENDPOINT,
                                   kGroupKey11_, kEndpoint1_);
    expected_commands.emplace_back(CommandType::ADD_REPORTING_ENDPOINT,
                                   kGroupKey11_, kEndpoint2_);
    expected_commands.emplace_back(CommandType::ADD_REPORTING_ENDPOINT_GROUP,
                                   kGroupKey11_);
    EXPECT_THAT(mock_store()->GetAllCommands(),
                testing::IsSupersetOf(expected_commands));
  }

  // Second header from the same origin should overwrite the previous one.
  std::vector<ReportingEndpoint::EndpointInfo> endpoints2 = {
      // This endpoint should update the priority of the existing one.
      {kEndpoint1_, 20 /* priority */}};
  // The second endpoint in this group will be deleted.
  // This group is new.
  std::vector<ReportingEndpoint::EndpointInfo> endpoints3 = {{kEndpoint2_}};
  std::string header2 =
      ConstructHeaderGroupString(MakeEndpointGroup(kGroup1_, endpoints2)) +
      ", " +
      ConstructHeaderGroupString(MakeEndpointGroup(kGroup2_, endpoints3));
  ParseHeader(kNak_, kOrigin1_, header2);

  EXPECT_TRUE(ClientExistsInCacheForOrigin(kOrigin1_));

  EXPECT_TRUE(
      EndpointGroupExistsInCache(kGroupKey11_, OriginSubdomains::DEFAULT));
  EXPECT_TRUE(
      EndpointGroupExistsInCache(kGroupKey12_, OriginSubdomains::DEFAULT));

  EXPECT_EQ(2u, cache()->GetEndpointCount());
  EXPECT_TRUE(FindEndpointInCache(kGroupKey11_, kEndpoint1_));
  EXPECT_EQ(20, FindEndpointInCache(kGroupKey11_, kEndpoint1_).info.priority);
  EXPECT_FALSE(FindEndpointInCache(kGroupKey11_, kEndpoint2_));
  EXPECT_TRUE(FindEndpointInCache(kGroupKey12_, kEndpoint2_));
  if (mock_store()) {
    mock_store()->Flush();
    EXPECT_EQ(2 + 1,
              mock_store()->CountCommands(CommandType::ADD_REPORTING_ENDPOINT));
    EXPECT_EQ(1 + 1, mock_store()->CountCommands(
                         CommandType::ADD_REPORTING_ENDPOINT_GROUP));
    EXPECT_EQ(
        1, mock_store()->CountCommands(CommandType::DELETE_REPORTING_ENDPOINT));
    MockPersistentReportingStore::CommandList expected_commands;
    expected_commands.emplace_back(CommandType::ADD_REPORTING_ENDPOINT,
                                   kGroupKey12_, kEndpoint2_);
    expected_commands.emplace_back(CommandType::ADD_REPORTING_ENDPOINT_GROUP,
                                   kGroupKey12_);
    expected_commands.emplace_back(CommandType::DELETE_REPORTING_ENDPOINT,
                                   kGroupKey11_, kEndpoint2_);
    EXPECT_THAT(mock_store()->GetAllCommands(),
                testing::IsSupersetOf(expected_commands));
  }
}

TEST_P(ReportingHeaderParserTest, OverwriteOldHeaderWithCompletelyNew) {
  ReportingEndpointGroupKey kGroupKey1(kNak_, kOrigin1_, "1",
                                       ReportingTargetType::kDeveloper);
  ReportingEndpointGroupKey kGroupKey2(kNak_, kOrigin1_, "2",
                                       ReportingTargetType::kDeveloper);
  ReportingEndpointGroupKey kGroupKey3(kNak_, kOrigin1_, "3",
                                       ReportingTargetType::kDeveloper);
  ReportingEndpointGroupKey kGroupKey4(kNak_, kOrigin1_, "4",
                                       ReportingTargetType::kDeveloper);
  ReportingEndpointGroupKey kGroupKey5(kNak_, kOrigin1_, "5",
                                       ReportingTargetType::kDeveloper);
  std::vector<ReportingEndpoint::EndpointInfo> endpoints1_1 = {{MakeURL(10)},
                                                               {MakeURL(11)}};
  std::vector<ReportingEndpoint::EndpointInfo> endpoints2_1 = {{MakeURL(20)},
                                                               {MakeURL(21)}};
  std::vector<ReportingEndpoint::EndpointInfo> endpoints3_1 = {{MakeURL(30)},
                                                               {MakeURL(31)}};
  std::string header1 =
      ConstructHeaderGroupString(MakeEndpointGroup("1", endpoints1_1)) + ", " +
      ConstructHeaderGroupString(MakeEndpointGroup("2", endpoints2_1)) + ", " +
      ConstructHeaderGroupString(MakeEndpointGroup("3", endpoints3_1));
  ParseHeader(kNak_, kOrigin1_, header1);
  EXPECT_TRUE(ClientExistsInCacheForOrigin(kOrigin1_));
  EXPECT_EQ(3u, cache()->GetEndpointGroupCountForTesting());
  EXPECT_TRUE(
      EndpointGroupExistsInCache(kGroupKey1, OriginSubdomains::DEFAULT));
  EXPECT_TRUE(
      EndpointGroupExistsInCache(kGroupKey2, OriginSubdomains::DEFAULT));
  EXPECT_TRUE(
      EndpointGroupExistsInCache(kGroupKey3, OriginSubdomains::DEFAULT));
  EXPECT_EQ(6u, cache()->GetEndpointCount());
  if (mock_store()) {
    mock_store()->Flush();
    EXPECT_EQ(6,
              mock_store()->CountCommands(CommandType::ADD_REPORTING_ENDPOINT));
    EXPECT_EQ(3, mock_store()->CountCommands(
                     CommandType::ADD_REPORTING_ENDPOINT_GROUP));
    MockPersistentReportingStore::CommandList expected_commands;
    expected_commands.emplace_back(CommandType::ADD_REPORTING_ENDPOINT,
                                   kGroupKey1, endpoints1_1[0].url);
    expected_commands.emplace_back(CommandType::ADD_REPORTING_ENDPOINT,
                                   kGroupKey1, endpoints1_1[1].url);
    expected_commands.emplace_back(CommandType::ADD_REPORTING_ENDPOINT,
                                   kGroupKey2, endpoints2_1[0].url);
    expected_commands.emplace_back(CommandType::ADD_REPORTING_ENDPOINT,
                                   kGroupKey2, endpoints2_1[1].url);
    expected_commands.emplace_back(CommandType::ADD_REPORTING_ENDPOINT,
                                   kGroupKey3, endpoints3_1[0].url);
    expected_commands.emplace_back(CommandType::ADD_REPORTING_ENDPOINT,
                                   kGroupKey3, endpoints3_1[1].url);
    expected_commands.emplace_back(CommandType::ADD_REPORTING_ENDPOINT_GROUP,
                                   kGroupKey1);
    expected_commands.emplace_back(CommandType::ADD_REPORTING_ENDPOINT_GROUP,
                                   kGroupKey2);
    expected_commands.emplace_back(CommandType::ADD_REPORTING_ENDPOINT_GROUP,
                                   kGroupKey3);
    EXPECT_THAT(mock_store()->GetAllCommands(),
                testing::IsSupersetOf(expected_commands));
  }

  // Replace endpoints in each group with completely new endpoints.
  std::vector<ReportingEndpoint::EndpointInfo> endpoints1_2 = {{MakeURL(12)}};
  std::vector<ReportingEndpoint::EndpointInfo> endpoints2_2 = {{MakeURL(22)}};
  std::vector<ReportingEndpoint::EndpointInfo> endpoints3_2 = {{MakeURL(32)}};
  std::string header2 =
      ConstructHeaderGroupString(MakeEndpointGroup("1", endpoints1_2)) + ", " +
      ConstructHeaderGroupString(MakeEndpointGroup("2", endpoints2_2)) + ", " +
      ConstructHeaderGroupString(MakeEndpointGroup("3", endpoints3_2));
  ParseHeader(kNak_, kOrigin1_, header2);
  EXPECT_TRUE(ClientExistsInCacheForOrigin(kOrigin1_));
  EXPECT_EQ(3u, cache()->GetEndpointGroupCountForTesting());
  EXPECT_TRUE(
      EndpointGroupExistsInCache(kGroupKey1, OriginSubdomains::DEFAULT));
  EXPECT_TRUE(
      EndpointGroupExistsInCache(kGroupKey2, OriginSubdomains::DEFAULT));
  EXPECT_TRUE(
      EndpointGroupExistsInCache(kGroupKey3, OriginSubdomains::DEFAULT));
  EXPECT_EQ(3u, cache()->GetEndpointCount());
  EXPECT_TRUE(FindEndpointInCache(kGroupKey1, MakeURL(12)));
  EXPECT_FALSE(FindEndpointInCache(kGroupKey1, MakeURL(10)));
  EXPECT_FALSE(FindEndpointInCache(kGroupKey1, MakeURL(11)));
  EXPECT_TRUE(FindEndpointInCache(kGroupKey2, MakeURL(22)));
  EXPECT_FALSE(FindEndpointInCache(kGroupKey2, MakeURL(20)));
  EXPECT_FALSE(FindEndpointInCache(kGroupKey2, MakeURL(21)));
  EXPECT_TRUE(FindEndpointInCache(kGroupKey3, MakeURL(32)));
  EXPECT_FALSE(FindEndpointInCache(kGroupKey3, MakeURL(30)));
  EXPECT_FALSE(FindEndpointInCache(kGroupKey3, MakeURL(31)));
  if (mock_store()) {
    mock_store()->Flush();
    EXPECT_EQ(6 + 3,
              mock_store()->CountCommands(CommandType::ADD_REPORTING_ENDPOINT));
    EXPECT_EQ(3, mock_store()->CountCommands(
                     CommandType::ADD_REPORTING_ENDPOINT_GROUP));
    EXPECT_EQ(
        6, mock_store()->CountCommands(CommandType::DELETE_REPORTING_ENDPOINT));
    EXPECT_EQ(0, mock_store()->CountCommands(
                     CommandType::DELETE_REPORTING_ENDPOINT_GROUP));
    MockPersistentReportingStore::CommandList expected_commands;
    expected_commands.emplace_back(CommandType::ADD_REPORTING_ENDPOINT,
                                   kGroupKey1, endpoints1_2[0].url);
    expected_commands.emplace_back(CommandType::ADD_REPORTING_ENDPOINT,
                                   kGroupKey2, endpoints2_2[0].url);
    expected_commands.emplace_back(CommandType::ADD_REPORTING_ENDPOINT,
                                   kGroupKey3, endpoints3_2[0].url);
    expected_commands.emplace_back(CommandType::DELETE_REPORTING_ENDPOINT,
                                   kGroupKey1, endpoints1_1[0].url);
    expected_commands.emplace_back(CommandType::DELETE_REPORTING_ENDPOINT,
                                   kGroupKey1, endpoints1_1[1].url);
    expected_commands.emplace_back(CommandType::DELETE_REPORTING_ENDPOINT,
                                   kGroupKey2, endpoints2_1[0].url);
    expected_commands.emplace_back(CommandType::DELETE_REPORTING_ENDPOINT,
                                   kGroupKey2, endpoints2_1[1].url);
    expected_commands.emplace_back(CommandType::DELETE_REPORTING_ENDPOINT,
                                   kGroupKey3, endpoints3_1[0].url);
    expected_commands.emplace_back(CommandType::DELETE_REPORTING_ENDPOINT,
                                   kGroupKey3, endpoints3_1[1].url);
    EXPECT_THAT(mock_store()->GetAllCommands(),
                testing::IsSupersetOf(expected_commands));
  }

  // Replace all the groups with completely new groups.
  std::vector<ReportingEndpoint::EndpointInfo> endpoints4_3 = {{MakeURL(40)}};
  std::vector<ReportingEndpoint::EndpointInfo> endpoints5_3 = {{MakeURL(50)}};
  std::string header3 =
      ConstructHeaderGroupString(MakeEndpointGroup("4", endpoints4_3)) + ", " +
      ConstructHeaderGroupString(MakeEndpointGroup("5", endpoints5_3));
  ParseHeader(kNak_, kOrigin1_, header3);
  EXPECT_TRUE(ClientExistsInCacheForOrigin(kOrigin1_));
  EXPECT_EQ(2u, cache()->GetEndpointGroupCountForTesting());
  EXPECT_TRUE(
      EndpointGroupExistsInCache(kGroupKey4, OriginSubdomains::DEFAULT));
  EXPECT_TRUE(
      EndpointGroupExistsInCache(kGroupKey4, OriginSubdomains::DEFAULT));
  EXPECT_FALSE(
      EndpointGroupExistsInCache(kGroupKey1, OriginSubdomains::DEFAULT));
  EXPECT_FALSE(
      EndpointGroupExistsInCache(kGroupKey2, OriginSubdomains::DEFAULT));
  EXPECT_FALSE(
      EndpointGroupExistsInCache(kGroupKey3, OriginSubdomains::DEFAULT));
  EXPECT_EQ(2u, cache()->GetEndpointCount());
  if (mock_store()) {
    mock_store()->Flush();
    EXPECT_EQ(6 + 3 + 2,
              mock_store()->CountCommands(CommandType::ADD_REPORTING_ENDPOINT));
    EXPECT_EQ(3 + 2, mock_store()->CountCommands(
                         CommandType::ADD_REPORTING_ENDPOINT_GROUP));
    EXPECT_EQ(6 + 3, mock_store()->CountCommands(
                         CommandType::DELETE_REPORTING_ENDPOINT));
    EXPECT_EQ(3, mock_store()->CountCommands(
                     CommandType::DELETE_REPORTING_ENDPOINT_GROUP));
    MockPersistentReportingStore::CommandList expected_commands;
    expected_commands.emplace_back(CommandType::ADD_REPORTING_ENDPOINT,
                                   kGroupKey4, endpoints4_3[0].url);
    expected_commands.emplace_back(CommandType::ADD_REPORTING_ENDPOINT,
                                   kGroupKey5, endpoints5_3[0].url);
    expected_commands.emplace_back(CommandType::ADD_REPORTING_ENDPOINT_GROUP,
                                   kGroupKey4);
    expected_commands.emplace_back(CommandType::ADD_REPORTING_ENDPOINT_GROUP,
                                   kGroupKey5);
    expected_commands.emplace_back(CommandType::DELETE_REPORTING_ENDPOINT,
                                   kGroupKey1, endpoints1_2[0].url);
    expected_commands.emplace_back(CommandType::DELETE_REPORTING_ENDPOINT,
                                   kGroupKey2, endpoints2_2[0].url);
    expected_commands.emplace_back(CommandType::DELETE_REPORTING_ENDPOINT,
                                   kGroupKey3, endpoints3_2[0].url);
    expected_commands.emplace_back(CommandType::DELETE_REPORTING_ENDPOINT_GROUP,
                                   kGroupKey1);
    expected_commands.emplace_back(CommandType::DELETE_REPORTING_ENDPOINT_GROUP,
                                   kGroupKey2);
    expected_commands.emplace_back(CommandType::DELETE_REPORTING_ENDPOINT_GROUP,
                                   kGroupKey3);
    EXPECT_THAT(mock_store()->GetAllCommands(),
                testing::IsSupersetOf(expected_commands));
  }
}

TEST_P(ReportingHeaderParserTest, ZeroMaxAgeRemovesEndpointGroup) {
  // Without a pre-existing client, max_age: 0 should do nothing.
  ASSERT_EQ(0u, cache()->GetEndpointCount());
  ParseHeader(kNak_, kOrigin1_,
              "{\"endpoints\":[{\"url\":\"" + kEndpoint1_.spec() +
                  "\"}],\"max_age\":0}");
  EXPECT_EQ(0u, cache()->GetEndpointCount());
  if (mock_store()) {
    mock_store()->Flush();
    EXPECT_EQ(0,
              mock_store()->CountCommands(CommandType::ADD_REPORTING_ENDPOINT));
    EXPECT_EQ(0, mock_store()->CountCommands(
                     CommandType::ADD_REPORTING_ENDPOINT_GROUP));
  }

  // Set a header with two endpoint groups.
  std::vector<ReportingEndpoint::EndpointInfo> endpoints1 = {{kEndpoint1_}};
  std::vector<ReportingEndpoint::EndpointInfo> endpoints2 = {{kEndpoint2_}};
  std::string header1 =
      ConstructHeaderGroupString(MakeEndpointGroup(kGroup1_, endpoints1)) +
      ", " +
      ConstructHeaderGroupString(MakeEndpointGroup(kGroup2_, endpoints2));
  ParseHeader(kNak_, kOrigin1_, header1);

  EXPECT_TRUE(ClientExistsInCacheForOrigin(kOrigin1_));
  EXPECT_EQ(2u, cache()->GetEndpointGroupCountForTesting());
  EXPECT_TRUE(
      EndpointGroupExistsInCache(kGroupKey11_, OriginSubdomains::DEFAULT));
  EXPECT_TRUE(
      EndpointGroupExistsInCache(kGroupKey12_, OriginSubdomains::DEFAULT));
  EXPECT_EQ(2u, cache()->GetEndpointCount());
  if (mock_store()) {
    mock_store()->Flush();
    EXPECT_EQ(2,
              mock_store()->CountCommands(CommandType::ADD_REPORTING_ENDPOINT));
    EXPECT_EQ(2, mock_store()->CountCommands(
                     CommandType::ADD_REPORTING_ENDPOINT_GROUP));
    MockPersistentReportingStore::CommandList expected_commands;
    expected_commands.emplace_back(CommandType::ADD_REPORTING_ENDPOINT,
                                   kGroupKey11_, kEndpoint1_);
    expected_commands.emplace_back(CommandType::ADD_REPORTING_ENDPOINT,
                                   kGroupKey12_, kEndpoint2_);
    expected_commands.emplace_back(CommandType::ADD_REPORTING_ENDPOINT_GROUP,
                                   kGroupKey11_);
    expected_commands.emplace_back(CommandType::ADD_REPORTING_ENDPOINT_GROUP,
                                   kGroupKey12_);
    EXPECT_THAT(mock_store()->GetAllCommands(),
                testing::IsSupersetOf(expected_commands));
  }

  // Set another header with max_age: 0 to delete one of the groups.
  std::string header2 =
      ConstructHeaderGroupString(MakeEndpointGroup(
          kGroup1_, endpoints1, OriginSubdomains::DEFAULT, base::Seconds(0))) +
      ", " +
      ConstructHeaderGroupString(
          MakeEndpointGroup(kGroup2_, endpoints2));  // Other group stays.
  ParseHeader(kNak_, kOrigin1_, header2);

  EXPECT_TRUE(ClientExistsInCacheForOrigin(kOrigin1_));
  EXPECT_EQ(1u, cache()->GetEndpointGroupCountForTesting());

  // Group was deleted.
  EXPECT_FALSE(
      EndpointGroupExistsInCache(kGroupKey11_, OriginSubdomains::DEFAULT));
  // Other group remains in the cache.
  EXPECT_TRUE(
      EndpointGroupExistsInCache(kGroupKey12_, OriginSubdomains::DEFAULT));
  EXPECT_EQ(1u, cache()->GetEndpointCount());
  if (mock_store()) {
    mock_store()->Flush();
    EXPECT_EQ(2,
              mock_store()->CountCommands(CommandType::ADD_REPORTING_ENDPOINT));
    EXPECT_EQ(2, mock_store()->CountCommands(
                     CommandType::ADD_REPORTING_ENDPOINT_GROUP));
    EXPECT_EQ(
        1, mock_store()->CountCommands(CommandType::DELETE_REPORTING_ENDPOINT));
    EXPECT_EQ(1, mock_store()->CountCommands(
                     CommandType::DELETE_REPORTING_ENDPOINT_GROUP));
    MockPersistentReportingStore::CommandList expected_commands;
    expected_commands.emplace_back(CommandType::DELETE_REPORTING_ENDPOINT,
                                   kGroupKey11_, kEndpoint1_);
    expected_commands.emplace_back(CommandType::DELETE_REPORTING_ENDPOINT_GROUP,
                                   kGroupKey11_);
    EXPECT_THAT(mock_store()->GetAllCommands(),
                testing::IsSupersetOf(expected_commands));
  }

  // Set another header with max_age: 0 to delete the other group. (Should work
  // even if the endpoints field is an empty list.)
  std::string header3 = ConstructHeaderGroupString(MakeEndpointGroup(
      kGroup2_, std::vector<ReportingEndpoint::EndpointInfo>(),
      OriginSubdomains::DEFAULT, base::Seconds(0)));
  ParseHeader(kNak_, kOrigin1_, header3);

  // Deletion of the last remaining group also deletes the client for this
  // origin.
  EXPECT_FALSE(ClientExistsInCacheForOrigin(kOrigin1_));
  EXPECT_EQ(0u, cache()->GetEndpointGroupCountForTesting());
  EXPECT_EQ(0u, cache()->GetEndpointCount());
  if (mock_store()) {
    mock_store()->Flush();
    EXPECT_EQ(2,
              mock_store()->CountCommands(CommandType::ADD_REPORTING_ENDPOINT));
    EXPECT_EQ(2, mock_store()->CountCommands(
                     CommandType::ADD_REPORTING_ENDPOINT_GROUP));
    EXPECT_EQ(1 + 1, mock_store()->CountCommands(
                         CommandType::DELETE_REPORTING_ENDPOINT));
    EXPECT_EQ(1 + 1, mock_store()->CountCommands(
                         CommandType::DELETE_REPORTING_ENDPOINT_GROUP));
    MockPersistentReportingStore::CommandList expected_commands;
    expected_commands.emplace_back(CommandType::DELETE_REPORTING_ENDPOINT,
                                   kGroupKey12_, kEndpoint2_);
    expected_commands.emplace_back(CommandType::DELETE_REPORTING_ENDPOINT_GROUP,
                                   kGroupKey12_);
    EXPECT_THAT(mock_store()->GetAllCommands(),
                testing::IsSupersetOf(expected_commands));
  }
}

// Invalid advertisements that parse as JSON should remove an endpoint group,
// while those that don't are ignored.
TEST_P(ReportingHeaderParserTest, InvalidAdvertisementRemovesEndpointGroup) {
  std::string invalid_non_json_header = "Goats should wear hats.";
  std::string invalid_json_header = "\"Goats should wear hats.\"";

  // Without a pre-existing client, neither invalid header does anything.

  ASSERT_EQ(0u, cache()->GetEndpointCount());
  ParseHeader(kNak_, kOrigin1_, invalid_non_json_header);
  EXPECT_EQ(0u, cache()->GetEndpointCount());
  if (mock_store()) {
    mock_store()->Flush();
    EXPECT_EQ(0,
              mock_store()->CountCommands(CommandType::ADD_REPORTING_ENDPOINT));
    EXPECT_EQ(0, mock_store()->CountCommands(
                     CommandType::ADD_REPORTING_ENDPOINT_GROUP));
  }

  ASSERT_EQ(0u, cache()->GetEndpointCount());
  ParseHeader(kNak_, kOrigin1_, invalid_json_header);
  EXPECT_EQ(0u, cache()->GetEndpointCount());
  if (mock_store()) {
    mock_store()->Flush();
    EXPECT_EQ(0,
              mock_store()->CountCommands(CommandType::ADD_REPORTING_ENDPOINT));
    EXPECT_EQ(0, mock_store()->CountCommands(
                     CommandType::ADD_REPORTING_ENDPOINT_GROUP));
  }

  // Set a header with two endpoint groups.
  std::vector<ReportingEndpoint::EndpointInfo> endpoints1 = {{kEndpoint1_}};
  std::vector<ReportingEndpoint::EndpointInfo> endpoints2 = {{kEndpoint2_}};
  std::string header1 =
      ConstructHeaderGroupString(MakeEndpointGroup(kGroup1_, endpoints1)) +
      ", " +
      ConstructHeaderGroupString(MakeEndpointGroup(kGroup2_, endpoints2));
  ParseHeader(kNak_, kOrigin1_, header1);

  EXPECT_TRUE(ClientExistsInCacheForOrigin(kOrigin1_));
  EXPECT_EQ(2u, cache()->GetEndpointGroupCountForTesting());
  EXPECT_TRUE(
      EndpointGroupExistsInCache(kGroupKey11_, OriginSubdomains::DEFAULT));
  EXPECT_TRUE(
      EndpointGroupExistsInCache(kGroupKey12_, OriginSubdomains::DEFAULT));
  EXPECT_EQ(2u, cache()->GetEndpointCount());
  if (mock_store()) {
    mock_store()->Flush();
    EXPECT_EQ(2,
              mock_store()->CountCommands(CommandType::ADD_REPORTING_ENDPOINT));
    EXPECT_EQ(2, mock_store()->CountCommands(
                     CommandType::ADD_REPORTING_ENDPOINT_GROUP));
    MockPersistentReportingStore::CommandList expected_commands;
    expected_commands.emplace_back(CommandType::ADD_REPORTING_ENDPOINT,
                                   kGroupKey11_, kEndpoint1_);
    expected_commands.emplace_back(CommandType::ADD_REPORTING_ENDPOINT,
                                   kGroupKey12_, kEndpoint2_);
    expected_commands.emplace_back(CommandType::ADD_REPORTING_ENDPOINT_GROUP,
                                   kGroupKey11_);
    expected_commands.emplace_back(CommandType::ADD_REPORTING_ENDPOINT_GROUP,
                                   kGroupKey12_);
    EXPECT_THAT(mock_store()->GetAllCommands(),
                testing::IsSupersetOf(expected_commands));
  }

  // Set another header with max_age: 0 to delete one of the groups.
  std::string header2 =
      ConstructHeaderGroupString(MakeEndpointGroup(
          kGroup1_, endpoints1, OriginSubdomains::DEFAULT, base::Seconds(0))) +
      ", " +
      ConstructHeaderGroupString(
          MakeEndpointGroup(kGroup2_, endpoints2));  // Other group stays.
  ParseHeader(kNak_, kOrigin1_, header2);

  EXPECT_TRUE(ClientExistsInCacheForOrigin(kOrigin1_));
  EXPECT_EQ(1u, cache()->GetEndpointGroupCountForTesting());

  // Group was deleted.
  EXPECT_FALSE(
      EndpointGroupExistsInCache(kGroupKey11_, OriginSubdomains::DEFAULT));
  // Other group remains in the cache.
  EXPECT_TRUE(
      EndpointGroupExistsInCache(kGroupKey12_, OriginSubdomains::DEFAULT));
  EXPECT_EQ(1u, cache()->GetEndpointCount());
  if (mock_store()) {
    mock_store()->Flush();
    EXPECT_EQ(2,
              mock_store()->CountCommands(CommandType::ADD_REPORTING_ENDPOINT));
    EXPECT_EQ(2, mock_store()->CountCommands(
                     CommandType::ADD_REPORTING_ENDPOINT_GROUP));
    EXPECT_EQ(
        1, mock_store()->CountCommands(CommandType::DELETE_REPORTING_ENDPOINT));
    EXPECT_EQ(1, mock_store()->CountCommands(
                     CommandType::DELETE_REPORTING_ENDPOINT_GROUP));
    MockPersistentReportingStore::CommandList expected_commands;
    expected_commands.emplace_back(CommandType::DELETE_REPORTING_ENDPOINT,
                                   kGroupKey11_, kEndpoint1_);
    expected_commands.emplace_back(CommandType::DELETE_REPORTING_ENDPOINT_GROUP,
                                   kGroupKey11_);
    EXPECT_THAT(mock_store()->GetAllCommands(),
                testing::IsSupersetOf(expected_commands));
  }

  // Invalid header values that are not JSON lists (without the outer brackets)
  // are ignored.
  ParseHeader(kNak_, kOrigin1_, invalid_non_json_header);
  EXPECT_TRUE(ClientExistsInCacheForOrigin(kOrigin1_));
  EXPECT_TRUE(
      EndpointGroupExistsInCache(kGroupKey12_, OriginSubdomains::DEFAULT));
  EXPECT_EQ(1u, cache()->GetEndpointCount());
  if (mock_store()) {
    mock_store()->Flush();
    EXPECT_EQ(2,
              mock_store()->CountCommands(CommandType::ADD_REPORTING_ENDPOINT));
    EXPECT_EQ(2, mock_store()->CountCommands(
                     CommandType::ADD_REPORTING_ENDPOINT_GROUP));
    EXPECT_EQ(
        1, mock_store()->CountCommands(CommandType::DELETE_REPORTING_ENDPOINT));
    EXPECT_EQ(1, mock_store()->CountCommands(
                     CommandType::DELETE_REPORTING_ENDPOINT_GROUP));
    MockPersistentReportingStore::CommandList expected_commands;
    expected_commands.emplace_back(CommandType::DELETE_REPORTING_ENDPOINT,
                                   kGroupKey11_, kEndpoint1_);
    expected_commands.emplace_back(CommandType::DELETE_REPORTING_ENDPOINT_GROUP,
                                   kGroupKey11_);
    EXPECT_THAT(mock_store()->GetAllCommands(),
                testing::IsSupersetOf(expected_commands));
  }

  // Invalid headers that do parse as JSON should delete the corresponding
  // client.
  ParseHeader(kNak_, kOrigin1_, invalid_json_header);

  // Deletion of the last remaining group also deletes the client for this
  // origin.
  EXPECT_FALSE(ClientExistsInCacheForOrigin(kOrigin1_));
  EXPECT_EQ(0u, cache()->GetEndpointGroupCountForTesting());
  EXPECT_EQ(0u, cache()->GetEndpointCount());
  if (mock_store()) {
    mock_store()->Flush();
    EXPECT_EQ(2,
              mock_store()->CountCommands(CommandType::ADD_REPORTING_ENDPOINT));
    EXPECT_EQ(2, mock_store()->CountCommands(
                     CommandType::ADD_REPORTING_ENDPOINT_GROUP));
    EXPECT_EQ(1 + 1, mock_store()->CountCommands(
                         CommandType::DELETE_REPORTING_ENDPOINT));
    EXPECT_EQ(1 + 1, mock_store()->CountCommands(
                         CommandType::DELETE_REPORTING_ENDPOINT_GROUP));
    MockPersistentReportingStore::CommandList expected_commands;
    expected_commands.emplace_back(CommandType::DELETE_REPORTING_ENDPOINT,
                                   kGroupKey12_, kEndpoint2_);
    expected_commands.emplace_back(CommandType::DELETE_REPORTING_ENDPOINT_GROUP,
                                   kGroupKey12_);
    EXPECT_THAT(mock_store()->GetAllCommands(),
                testing::IsSupersetOf(expected_commands));
  }
}

TEST_P(ReportingHeaderParserTest, EvictEndpointsOverPerOriginLimit1) {
  // Set a header with too many endpoints, all in the same group.
  std::vector<ReportingEndpoint::EndpointInfo> endpoints;
  for (size_t i = 0; i < policy().max_endpoints_per_origin + 1; ++i) {
    endpoints.push_back({MakeURL(i)});
  }
  std::string header =
      ConstructHeaderGroupString(MakeEndpointGroup(kGroup1_, endpoints));
  ParseHeader(kNak_, kOrigin1_, header);

  // Endpoint count should be at most the limit.
  EXPECT_GE(policy().max_endpoints_per_origin, cache()->GetEndpointCount());

  if (mock_store()) {
    mock_store()->Flush();
    EXPECT_EQ(policy().max_endpoints_per_origin + 1,
              static_cast<unsigned long>(mock_store()->CountCommands(
                  CommandType::ADD_REPORTING_ENDPOINT)));
    EXPECT_EQ(1, mock_store()->CountCommands(
                     CommandType::ADD_REPORTING_ENDPOINT_GROUP));
    EXPECT_EQ(
        1, mock_store()->CountCommands(CommandType::DELETE_REPORTING_ENDPOINT));
  }
}

TEST_P(ReportingHeaderParserTest, EvictEndpointsOverPerOriginLimit2) {
  // Set a header with too many endpoints, in different groups.
  std::string header;
  for (size_t i = 0; i < policy().max_endpoints_per_origin + 1; ++i) {
    std::vector<ReportingEndpoint::EndpointInfo> endpoints = {{MakeURL(i)}};
    header = header + ConstructHeaderGroupString(MakeEndpointGroup(
                          base::NumberToString(i), endpoints));
    if (i != policy().max_endpoints_per_origin)
      header = header + ", ";
  }
  ParseHeader(kNak_, kOrigin1_, header);

  // Endpoint count should be at most the limit.
  EXPECT_GE(policy().max_endpoints_per_origin, cache()->GetEndpointCount());

  if (mock_store()) {
    mock_store()->Flush();
    EXPECT_EQ(policy().max_endpoints_per_origin + 1,
              static_cast<unsigned long>(mock_store()->CountCommands(
                  CommandType::ADD_REPORTING_ENDPOINT)));
    EXPECT_EQ(policy().max_endpoints_per_origin + 1,
              static_cast<unsigned long>(mock_store()->CountCommands(
                  CommandType::ADD_REPORTING_ENDPOINT_GROUP)));
    EXPECT_EQ(
        1, mock_store()->CountCommands(CommandType::DELETE_REPORTING_ENDPOINT));
    EXPECT_EQ(1, mock_store()->CountCommands(
                     CommandType::DELETE_REPORTING_ENDPOINT_GROUP));
  }
}

TEST_P(ReportingHeaderParserTest, EvictEndpointsOverGlobalLimit) {
  // Set headers from different origins up to the global limit.
  for (size_t i = 0; i < policy().max_endpoint_count; ++i) {
    std::vector<ReportingEndpoint::EndpointInfo> endpoints = {{MakeURL(i)}};
    std::string header =
        ConstructHeaderGroupString(MakeEndpointGroup(kGroup1_, endpoints));
    ParseHeader(kNak_, url::Origin::Create(MakeURL(i)), header);
  }
  EXPECT_EQ(policy().max_endpoint_count, cache()->GetEndpointCount());

  // Parse one more header to trigger eviction.
  ParseHeader(kNak_, kOrigin1_,
              "{\"endpoints\":[{\"url\":\"" + kEndpoint1_.spec() +
                  "\"}],\"max_age\":1}");

  // Endpoint count should be at most the limit.
  EXPECT_GE(policy().max_endpoint_count, cache()->GetEndpointCount());

  if (mock_store()) {
    mock_store()->Flush();
    EXPECT_EQ(policy().max_endpoint_count + 1,
              static_cast<unsigned long>(mock_store()->CountCommands(
                  CommandType::ADD_REPORTING_ENDPOINT)));
    EXPECT_EQ(policy().max_endpoint_count + 1,
              static_cast<unsigned long>(mock_store()->CountCommands(
                  CommandType::ADD_REPORTING_ENDPOINT_GROUP)));
    EXPECT_EQ(
        1, mock_store()->CountCommands(CommandType::DELETE_REPORTING_ENDPOINT));
    EXPECT_EQ(1, mock_store()->CountCommands(
                     CommandType::DELETE_REPORTING_ENDPOINT_GROUP));
  }
}

INSTANTIATE_TEST_SUITE_P(ReportingHeaderParserStoreTest,
                         ReportingHeaderParserTest,
                         testing::Bool());

// This test is parametrized on a boolean that represents whether to use a
// MockPersistentReportingStore.
class ReportingHeaderParserStructuredHeaderTest
    : public ReportingHeaderParserTestBase {
 protected:
  ReportingHeaderParserStructuredHeaderTest() {
    // Enable kDocumentReporting to support new StructuredHeader-based
    // Reporting-Endpoints header.
    feature_list_.InitWithFeatures(
        {features::kPartitionConnectionsByNetworkIsolationKey,
         features::kDocumentReporting},
        {});
  }

  ~ReportingHeaderParserStructuredHeaderTest() override = default;

  ReportingEndpointGroup MakeEndpointGroup(
      const std::string& name,
      const std::vector<ReportingEndpoint::EndpointInfo>& endpoints,
      url::Origin origin = url::Origin()) {
    ReportingEndpointGroupKey group_key(kNak_ /* unused */,
                                        url::Origin() /* unused */, name,
                                        ReportingTargetType::kDeveloper);
    ReportingEndpointGroup group;
    group.group_key = group_key;
    group.include_subdomains = OriginSubdomains::EXCLUDE;
    group.ttl = base::Days(30);
    group.endpoints = std::move(endpoints);
    return group;
  }

  // Constructs a string which would represent a single endpoint in a
  // Reporting-Endpoints header.
  std::string ConstructHeaderGroupString(const ReportingEndpointGroup& group) {
    std::string header = group.group_key.group_name;
    if (header.empty())
      return header;
    base::StrAppend(&header, {"="});
    if (group.endpoints.empty())
      return header;
    base::StrAppend(&header, {"\"", group.endpoints.front().url.spec(), "\""});
    return header;
  }

  void ParseHeader(const base::UnguessableToken& reporting_source,
                   const IsolationInfo& isolation_info,
                   const url::Origin& origin,
                   const std::string& header_string) {
    std::optional<base::flat_map<std::string, std::string>> header_map =
        ParseReportingEndpoints(header_string);

    if (header_map) {
      ReportingHeaderParser::ProcessParsedReportingEndpointsHeader(
          context(), reporting_source, isolation_info,
          isolation_info.network_anonymization_key(), origin, *header_map);
    }
  }
  void ProcessParsedHeader(
      const base::UnguessableToken& reporting_source,
      const IsolationInfo& isolation_info,
      const url::Origin& origin,
      const std::optional<base::flat_map<std::string, std::string>>&
          header_map) {
    ReportingHeaderParser::ProcessParsedReportingEndpointsHeader(
        context(), reporting_source, isolation_info,
        isolation_info.network_anonymization_key(), origin, *header_map);
  }

  const base::UnguessableToken kReportingSource_ =
      base::UnguessableToken::Create();
};

TEST_P(ReportingHeaderParserStructuredHeaderTest, ParseInvalid) {
  static const struct {
    const char* header_value;
    const char* description;
  } kInvalidHeaderTestCases[] = {
      {"default=", "missing url"},
      {"default=1", "non-string url"},
  };

  for (auto& test_case : kInvalidHeaderTestCases) {
    auto parsed_result = ParseReportingEndpoints(test_case.header_value);

    EXPECT_FALSE(parsed_result.has_value())
        << "Invalid Reporting-Endpoints header (" << test_case.description
        << ": \"" << test_case.header_value << "\") parsed as valid.";
  }
}

TEST_P(ReportingHeaderParserStructuredHeaderTest, ProcessInvalid) {
  static const struct {
    const char* header_value;
    const char* description;
  } kInvalidHeaderTestCases[] = {
      {"default=\"//scheme/relative\"", "scheme-relative url"},
      {"default=\"relative/path\"", "path relative url"},
      {"default=\"http://insecure/\"", "insecure url"}};

  base::HistogramTester histograms;
  int invalid_case_count = 0;

  for (auto& test_case : kInvalidHeaderTestCases) {
    auto parsed_result = ParseReportingEndpoints(test_case.header_value);

    EXPECT_TRUE(parsed_result.has_value())
        << "Syntactically valid Reporting-Endpoints header (\""
        << test_case.description << ": \"" << test_case.header_value
        << "\") parsed as invalid.";
    ProcessParsedHeader(kReportingSource_, kIsolationInfo_, kOrigin1_,
                        parsed_result);

    invalid_case_count++;
    histograms.ExpectBucketCount(
        kReportingHeaderTypeHistogram,
        ReportingHeaderParser::ReportingHeaderType::kReportingEndpointsInvalid,
        invalid_case_count);

    // The endpoint should not have been set up in the cache.
    ReportingEndpoint endpoint =
        cache()->GetV1EndpointForTesting(kReportingSource_, "default");
    EXPECT_FALSE(endpoint);
  }
  histograms.ExpectBucketCount(
      kReportingHeaderTypeHistogram,
      ReportingHeaderParser::ReportingHeaderType::kReportingEndpoints, 0);
}

TEST_P(ReportingHeaderParserStructuredHeaderTest, ParseBasic) {
  std::vector<ReportingEndpoint::EndpointInfo> endpoints = {{kEndpoint1_}};

  std::string header =
      ConstructHeaderGroupString(MakeEndpointGroup(kGroup1_, endpoints));
  auto parsed_result = ParseReportingEndpoints(header);

  EXPECT_TRUE(parsed_result.has_value())
      << "Valid Reporting-Endpoints header (\"" << header
      << "\") parsed as invalid.";
  EXPECT_EQ(1u, parsed_result->size());
  EXPECT_EQ(parsed_result->at(kGroup1_), kEndpoint1_.spec());
}

TEST_P(ReportingHeaderParserStructuredHeaderTest, Basic) {
  base::HistogramTester histograms;
  std::vector<ReportingEndpoint::EndpointInfo> endpoints = {{kEndpoint1_}};

  std::string header =
      ConstructHeaderGroupString(MakeEndpointGroup(kGroup1_, endpoints));
  auto parsed_result = ParseReportingEndpoints(header);
  ProcessParsedHeader(kReportingSource_, kIsolationInfo_, kOrigin1_,
                      parsed_result);

  // Ensure that the endpoint was not inserted into the persistent endpoint
  // groups used for v0 reporting.
  EXPECT_EQ(0u, cache()->GetEndpointGroupCountForTesting());

  ReportingEndpoint endpoint =
      cache()->GetV1EndpointForTesting(kReportingSource_, kGroup1_);
  EXPECT_TRUE(endpoint);

  IsolationInfo isolation_info = cache()->GetIsolationInfoForEndpoint(endpoint);
  EXPECT_TRUE(isolation_info.IsEqualForTesting(kIsolationInfo_));
  EXPECT_EQ(kOrigin1_, endpoint.group_key.origin);
  EXPECT_EQ(kGroup1_, endpoint.group_key.group_name);
  EXPECT_EQ(kEndpoint1_, endpoint.info.url);
  EXPECT_EQ(ReportingEndpoint::EndpointInfo::kDefaultPriority,
            endpoint.info.priority);
  EXPECT_EQ(ReportingEndpoint::EndpointInfo::kDefaultWeight,
            endpoint.info.weight);
  histograms.ExpectBucketCount(
      kReportingHeaderTypeHistogram,
      ReportingHeaderParser::ReportingHeaderType::kReportingEndpoints, 1);

  // Ephemeral endpoints should not be persisted in the store
  if (mock_store()) {
    mock_store()->Flush();
    EXPECT_EQ(0, mock_store()->StoredEndpointsCount());
    EXPECT_EQ(0, mock_store()->StoredEndpointGroupsCount());
  }
}

TEST_P(ReportingHeaderParserStructuredHeaderTest, PathAbsoluteURLEndpoint) {
  base::HistogramTester histograms;
  std::string header = "group1=\"/path-absolute-url\"";
  auto parsed_result = ParseReportingEndpoints(header);
  ProcessParsedHeader(kReportingSource_, kIsolationInfo_, kOrigin1_,
                      parsed_result);

  // Ensure that the endpoint was not inserted into the persistent endpoint
  // groups used for v0 reporting.
  EXPECT_EQ(0u, cache()->GetEndpointGroupCountForTesting());

  ReportingEndpoint endpoint =
      cache()->GetV1EndpointForTesting(kReportingSource_, kGroup1_);
  EXPECT_TRUE(endpoint);
  EXPECT_EQ(kOrigin1_, endpoint.group_key.origin);
  EXPECT_EQ(kGroup1_, endpoint.group_key.group_name);
  EXPECT_EQ(kEndpointPathAbsolute_, endpoint.info.url);
  EXPECT_EQ(ReportingEndpoint::EndpointInfo::kDefaultPriority,
            endpoint.info.priority);
  EXPECT_EQ(ReportingEndpoint::EndpointInfo::kDefaultWeight,
            endpoint.info.weight);
  histograms.ExpectBucketCount(
      kReportingHeaderTypeHistogram,
      ReportingHeaderParser::ReportingHeaderType::kReportingEndpoints, 1);

  // Ephemeral endpoints should not be persisted in the store
  if (mock_store()) {
    mock_store()->Flush();
    EXPECT_EQ(0, mock_store()->StoredEndpointsCount());
    EXPECT_EQ(0, mock_store()->StoredEndpointGroupsCount());
  }
}

INSTANTIATE_TEST_SUITE_P(ReportingHeaderParserStoreTest,
                         ReportingHeaderParserStructuredHeaderTest,
                         testing::Bool());

}  // namespace
}  // namespace net
