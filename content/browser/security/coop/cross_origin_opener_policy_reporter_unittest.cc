// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/security/coop/cross_origin_opener_policy_reporter.h"

#include <memory>
#include <optional>
#include <vector>

#include "base/test/task_environment.h"
#include "base/unguessable_token.h"
#include "base/values.h"
#include "content/public/test/test_storage_partition.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/network_anonymization_key.h"
#include "services/network/test/test_network_context.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

class TestNetworkContext : public network::TestNetworkContext {
 public:
  struct Report {
    Report(const std::string& type,
           const std::string& group,
           const GURL& url,
           const net::NetworkAnonymizationKey& network_anonymization_key,
           base::Value::Dict body)
        : type(type),
          group(group),
          url(url),
          network_anonymization_key(network_anonymization_key),
          body(std::move(body)) {}

    std::string type;
    std::string group;
    GURL url;
    net::NetworkAnonymizationKey network_anonymization_key;
    base::Value::Dict body;
  };

  void QueueReport(
      const std::string& type,
      const std::string& group,
      const GURL& url,
      const std::optional<base::UnguessableToken>& reporting_source,
      const net::NetworkAnonymizationKey& network_anonymization_key,
      base::Value::Dict body) override {
    reports_.emplace_back(type, group, url, network_anonymization_key,
                          std::move(body));
  }

  const std::vector<Report>& reports() const { return reports_; }

 private:
  std::vector<Report> reports_;
};

}  // namespace

class CrossOriginOpenerPolicyReporterTest : public testing::Test {
 public:
  using Report = TestNetworkContext::Report;
  CrossOriginOpenerPolicyReporterTest() {
    storage_partition_.set_network_context(&network_context_);
    coop_.value =
        network::mojom::CrossOriginOpenerPolicyValue::kSameOriginPlusCoep;
    coop_.reporting_endpoint = "e1";
    context_url_ = GURL("https://www1.example.com/x");
  }

  StoragePartition* storage_partition() { return &storage_partition_; }
  const TestNetworkContext& network_context() const { return network_context_; }
  const GURL& context_url() const { return context_url_; }
  const network::CrossOriginOpenerPolicy& coop() const { return coop_; }
  const base::UnguessableToken& reporting_source() const {
    return reporting_source_;
  }
  const net::NetworkAnonymizationKey& network_anonymization_key() const {
    return network_anonymization_key_;
  }

 protected:
  std::unique_ptr<CrossOriginOpenerPolicyReporter> GetReporter() {
    return std::make_unique<CrossOriginOpenerPolicyReporter>(
        storage_partition(), context_url(), GURL("https://referrer.com/?a#b"),
        coop(), reporting_source(), network_anonymization_key_);
  }

 private:
  base::test::TaskEnvironment task_environment_;
  TestNetworkContext network_context_;
  TestStoragePartition storage_partition_;
  GURL context_url_;
  network::CrossOriginOpenerPolicy coop_;
  const base::UnguessableToken reporting_source_ =
      base::UnguessableToken::Create();
  const net::NetworkAnonymizationKey network_anonymization_key_ =
      net::NetworkAnonymizationKey::CreateTransient();
};

TEST_F(CrossOriginOpenerPolicyReporterTest, Basic) {
  auto reporter = GetReporter();
  std::string url1 = "https://www1.example.com/y?bar=baz#foo";
  std::string url1_report = "https://www1.example.com/y?bar=baz";
  std::string url2 = "https://www1.example.com/";
  std::string url3 = "http://www2.example.com:41/z";

  reporter->QueueNavigationToCOOPReport(GURL(url1), true, false);
  reporter->QueueNavigationAwayFromCOOPReport(GURL(url3), true, true, false);

  ASSERT_EQ(2u, network_context().reports().size());
  const Report& r1 = network_context().reports()[0];
  const Report& r2 = network_context().reports()[1];

  EXPECT_EQ(r1.type, "coop");
  EXPECT_EQ(r1.url, context_url());
  EXPECT_EQ(r1.network_anonymization_key, network_anonymization_key());
  EXPECT_EQ(*r1.body.FindString("disposition"), "enforce");
  EXPECT_EQ(*r1.body.FindString("previousResponseURL"), url1_report);
  EXPECT_EQ(*r1.body.FindString("referrer"), "https://referrer.com/?a");
  EXPECT_EQ(*r1.body.FindString("type"), "navigation-to-response");
  EXPECT_EQ(*r1.body.FindString("effectivePolicy"), "same-origin-plus-coep");

  EXPECT_EQ(r2.type, "coop");
  EXPECT_EQ(r2.url, context_url());
  EXPECT_EQ(r2.network_anonymization_key, network_anonymization_key());
  EXPECT_EQ(*r2.body.FindString("disposition"), "enforce");
  EXPECT_EQ(*r2.body.FindString("nextResponseURL"), url3);
  EXPECT_EQ(*r2.body.FindString("type"), "navigation-from-response");
  EXPECT_EQ(*r2.body.FindString("effectivePolicy"), "same-origin-plus-coep");
}

TEST_F(CrossOriginOpenerPolicyReporterTest, UserAndPassSanitization) {
  auto reporter = GetReporter();
  std::string url = "https://u:p@www2.example.com/x";

  reporter->QueueNavigationToCOOPReport(GURL(url), true, false);
  reporter->QueueNavigationAwayFromCOOPReport(GURL(url), true, true, false);

  ASSERT_EQ(2u, network_context().reports().size());
  const Report& r1 = network_context().reports()[0];
  const Report& r2 = network_context().reports()[1];

  EXPECT_EQ(r1.type, "coop");
  EXPECT_EQ(r1.url, GURL("https://www1.example.com/x"));
  EXPECT_EQ(*r1.body.FindString("previousResponseURL"),
            "https://www2.example.com/x");
  EXPECT_EQ(*r1.body.FindString("referrer"), "https://referrer.com/?a");

  EXPECT_EQ(r2.type, "coop");
  EXPECT_EQ(r2.url, GURL("https://www1.example.com/x"));
  EXPECT_EQ(*r2.body.FindString("nextResponseURL"),
            "https://www2.example.com/x");
}

}  // namespace content
