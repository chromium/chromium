// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/security/dip/document_isolation_policy_reporter.h"

#include <optional>
#include <string_view>
#include <vector>

#include "base/test/task_environment.h"
#include "base/unguessable_token.h"
#include "base/values.h"
#include "content/public/test/test_storage_partition.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/cpp/cross_origin_embedder_policy.h"
#include "services/network/public/cpp/request_destination.h"
#include "services/network/test/test_network_context.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/frame/reporting_observer.mojom.h"

namespace content {
namespace {

using network::DocumentIsolationPolicy;
using network::mojom::RequestDestination;

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

class TestObserver final : public blink::mojom::ReportingObserver {
 public:
  explicit TestObserver(
      mojo::PendingReceiver<blink::mojom::ReportingObserver> receiver)
      : receiver_(this, std::move(receiver)) {}

  // blink::mojom::ReportingObserver implementation.
  void Notify(blink::mojom::ReportPtr report) override {
    reports_.push_back(std::move(report));
  }

  void FlushForTesting() { receiver_.FlushForTesting(); }

  const std::vector<blink::mojom::ReportPtr>& reports() const {
    return reports_;
  }

 private:
  mojo::Receiver<blink::mojom::ReportingObserver> receiver_;
  std::vector<blink::mojom::ReportPtr> reports_;
};

class DocumentIsolationPolicyReporterTest : public testing::Test {
 public:
  using Report = TestNetworkContext::Report;
  DocumentIsolationPolicyReporterTest() {
    storage_partition_.set_network_context(&network_context_);
  }

  base::WeakPtr<StoragePartition> GetStoragePartition() {
    return storage_partition_.GetWeakPtr();
  }
  void InvalidateWeakPtrs() { storage_partition_.InvalidateWeakPtrs(); }
  const TestNetworkContext& network_context() const { return network_context_; }
  base::Value::Dict CreateBodyForCorp(std::string_view blocked_url,
                                      RequestDestination destination,
                                      std::string_view disposition) const {
    base::Value::Dict dict;
    for (const auto& pair :
         CreateBodyForCorpInternal(blocked_url, destination, disposition)) {
      dict.Set(std::move(pair.first), std::move(pair.second));
    }
    return dict;
  }

  blink::mojom::ReportBodyPtr CreateMojomBodyForCorp(
      std::string_view blocked_url,
      RequestDestination destination,
      std::string_view disposition) {
    auto body = blink::mojom::ReportBody::New();
    for (const auto& pair :
         CreateBodyForCorpInternal(blocked_url, destination, disposition)) {
      body->body.push_back(blink::mojom::ReportBodyElement::New(
          std::move(pair.first), std::move(pair.second)));
    }
    return body;
  }

 private:
  std::vector<std::pair<std::string, std::string>> CreateBodyForCorpInternal(
      std::string_view blocked_url,
      RequestDestination destination,
      std::string_view disposition) const {
    return {{"type", "corp"},
            {"blockedURL", std::string(blocked_url)},
            {"destination", network::RequestDestinationToString(destination)},
            {"disposition", std::string(disposition)}};
  }

  base::test::TaskEnvironment task_environment_;
  TestNetworkContext network_context_;
  TestStoragePartition storage_partition_;
};

TEST_F(DocumentIsolationPolicyReporterTest, NullEndpointsForCorp) {
  const GURL kContextUrl("https://example.com/path");
  DocumentIsolationPolicyReporter reporter(
      GetStoragePartition(), kContextUrl, std::nullopt, std::nullopt,
      base::UnguessableToken::Create(), net::NetworkAnonymizationKey());

  reporter.QueueCorpViolationReport(GURL("https://www1.example.com/y"),
                                    RequestDestination::kEmpty,
                                    /*report_only=*/false);
  reporter.QueueCorpViolationReport(GURL("https://www2.example.com/x"),
                                    RequestDestination::kEmpty,
                                    /*report_only=*/true);

  EXPECT_TRUE(network_context().reports().empty());
}

TEST_F(DocumentIsolationPolicyReporterTest, BasicCorp) {
  const GURL kContextUrl("https://example.com/path");
  const auto kNetworkIsolationKey =
      net::NetworkAnonymizationKey::CreateTransient();
  const auto kReportingSource = base::UnguessableToken::Create();
  DocumentIsolationPolicyReporter reporter(GetStoragePartition(), kContextUrl,
                                           "e1", "e2", kReportingSource,
                                           kNetworkIsolationKey);

  reporter.QueueCorpViolationReport(
      GURL("https://www1.example.com/x#foo?bar=baz"),
      RequestDestination::kScript,
      /*report_only=*/false);
  reporter.QueueCorpViolationReport(GURL("http://www2.example.com:41/y"),
                                    RequestDestination::kEmpty,
                                    /*report_only=*/true);

  ASSERT_EQ(2u, network_context().reports().size());
  const Report& r1 = network_context().reports()[0];
  const Report& r2 = network_context().reports()[1];

  EXPECT_EQ(r1.type, "dip");
  EXPECT_EQ(r1.group, "e1");
  EXPECT_EQ(r1.url, kContextUrl);
  EXPECT_EQ(r1.network_anonymization_key, kNetworkIsolationKey);
  EXPECT_EQ(r1.body, CreateBodyForCorp("https://www1.example.com/x#foo?bar=baz",
                                       RequestDestination::kScript, "enforce"));
  EXPECT_EQ(r2.type, "dip");
  EXPECT_EQ(r2.group, "e2");
  EXPECT_EQ(r2.url, kContextUrl);
  EXPECT_EQ(r2.network_anonymization_key, kNetworkIsolationKey);
  EXPECT_EQ(r2.body,
            CreateBodyForCorp("http://www2.example.com:41/y",
                              RequestDestination::kEmpty, "reporting"));
}

TEST_F(DocumentIsolationPolicyReporterTest, UserAndPassForCorp) {
  const GURL kContextUrl("https://example.com/path");
  DocumentIsolationPolicyReporter reporter(
      GetStoragePartition(), kContextUrl, "e1", "e2",
      base::UnguessableToken::Create(), net::NetworkAnonymizationKey());

  reporter.QueueCorpViolationReport(GURL("https://u:p@www1.example.com/x"),
                                    RequestDestination::kImage,
                                    /*report_only=*/false);
  reporter.QueueCorpViolationReport(GURL("https://u:p@www2.example.com/y"),
                                    RequestDestination::kScript,
                                    /*report_only=*/true);

  ASSERT_EQ(2u, network_context().reports().size());
  const Report& r1 = network_context().reports()[0];
  const Report& r2 = network_context().reports()[1];

  EXPECT_EQ(r1.type, "dip");
  EXPECT_EQ(r1.group, "e1");
  EXPECT_EQ(r1.url, kContextUrl);
  EXPECT_EQ(r1.body, CreateBodyForCorp("https://www1.example.com/x",
                                       RequestDestination::kImage, "enforce"));
  EXPECT_EQ(r2.type, "dip");
  EXPECT_EQ(r2.group, "e2");
  EXPECT_EQ(r2.url, kContextUrl);
  EXPECT_EQ(r2.body,
            CreateBodyForCorp("https://www2.example.com/y",
                              RequestDestination::kScript, "reporting"));
}

TEST_F(DocumentIsolationPolicyReporterTest, ObserverForCorp) {
  const GURL kContextUrl("https://example.com/path");
  mojo::PendingRemote<blink::mojom::ReportingObserver> observer_remote;
  TestObserver observer(observer_remote.InitWithNewPipeAndPassReceiver());

  DocumentIsolationPolicyReporter reporter(
      GetStoragePartition(), kContextUrl, std::nullopt, std::nullopt,
      base::UnguessableToken::Create(), net::NetworkAnonymizationKey());
  reporter.BindObserver(std::move(observer_remote));
  reporter.QueueCorpViolationReport(GURL("https://u:p@www1.example.com/x"),
                                    RequestDestination::kImage,
                                    /*report_only=*/false);
  reporter.QueueCorpViolationReport(GURL("https://u:p@www2.example.com/y"),
                                    RequestDestination::kEmpty,
                                    /*report_only=*/true);

  observer.FlushForTesting();
  ASSERT_EQ(2u, observer.reports().size());
  const blink::mojom::Report& r1 = *(observer.reports()[0]);
  const blink::mojom::Report& r2 = *(observer.reports()[1]);

  EXPECT_EQ(r1.type, "dip");
  EXPECT_EQ(r1.url, kContextUrl);
  EXPECT_TRUE(mojo::Equals(
      r1.body, CreateMojomBodyForCorp("https://www1.example.com/x",
                                      RequestDestination::kImage, "enforce")));
  EXPECT_EQ(r2.type, "dip");
  EXPECT_EQ(r2.url, kContextUrl);
  EXPECT_TRUE(mojo::Equals(
      r2.body,
      CreateMojomBodyForCorp("https://www2.example.com/y",
                             RequestDestination::kEmpty, "reporting")));
}

TEST_F(DocumentIsolationPolicyReporterTest, Clone) {
  const GURL kContextUrl("https://example.com/path");
  DocumentIsolationPolicyReporter reporter(
      GetStoragePartition(), kContextUrl, "e1", "e2",
      base::UnguessableToken::Create(), net::NetworkAnonymizationKey());

  mojo::Remote<network::mojom::DocumentIsolationPolicyReporter> remote;
  reporter.Clone(remote.BindNewPipeAndPassReceiver());

  remote->QueueCorpViolationReport(GURL("https://www1.example.com/x"),
                                   RequestDestination::kIframe,
                                   /*report_only=*/false);
  remote->QueueCorpViolationReport(GURL("https://www2.example.com/y"),
                                   RequestDestination::kScript,
                                   /*report_only=*/true);

  remote.FlushForTesting();

  ASSERT_EQ(2u, network_context().reports().size());
  const Report& r1 = network_context().reports()[0];
  const Report& r2 = network_context().reports()[1];

  EXPECT_EQ(r1.type, "dip");
  EXPECT_EQ(r1.group, "e1");
  EXPECT_EQ(r1.url, kContextUrl);
  EXPECT_EQ(r1.body, CreateBodyForCorp("https://www1.example.com/x",
                                       RequestDestination::kIframe, "enforce"));
  EXPECT_EQ(r2.type, "dip");
  EXPECT_EQ(r2.group, "e2");
  EXPECT_EQ(r2.url, kContextUrl);
  EXPECT_EQ(r2.body,
            CreateBodyForCorp("https://www2.example.com/y",
                              RequestDestination::kScript, "reporting"));
}

TEST_F(DocumentIsolationPolicyReporterTest, StoragePartitionInvalidated) {
  const GURL kContextUrl("https://example.com/path");
  const auto kNetworkIsolationKey =
      net::NetworkAnonymizationKey::CreateTransient();
  const auto kReportingSource = base::UnguessableToken::Create();
  DocumentIsolationPolicyReporter reporter(GetStoragePartition(), kContextUrl,
                                           "e1", "e2", kReportingSource,
                                           kNetworkIsolationKey);

  InvalidateWeakPtrs();

  reporter.QueueCorpViolationReport(
      GURL("https://www1.example.com/x#foo?bar=baz"),
      RequestDestination::kScript,
      /*report_only=*/false);
  reporter.QueueCorpViolationReport(GURL("http://www2.example.com:41/y"),
                                    RequestDestination::kEmpty,
                                    /*report_only=*/true);

  ASSERT_EQ(0u, network_context().reports().size());
}

}  // namespace
}  // namespace content
