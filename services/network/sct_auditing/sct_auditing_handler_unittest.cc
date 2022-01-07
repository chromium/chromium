// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/sct_auditing/sct_auditing_handler.h"

#include <memory>

#include "base/base64.h"
#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/hash_value.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/network_context.h"
#include "services/network/network_service.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/proto/sct_audit_report.pb.h"
#include "services/network/sct_auditing/sct_auditing_cache.h"
#include "services/network/sct_auditing/sct_auditing_reporter.h"
#include "services/network/test/fake_test_cert_verifier_params_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "services/network/url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {

namespace {

class SCTAuditingHandlerTest : public testing::Test {
 public:
  SCTAuditingHandlerTest()
      : network_service_(NetworkService::CreateForTesting()) {}
  ~SCTAuditingHandlerTest() override = default;

  SCTAuditingHandlerTest(const SCTAuditingHandlerTest&) = delete;
  SCTAuditingHandlerTest& operator=(const SCTAuditingHandlerTest&) = delete;

  void SetUp() override {
    ASSERT_TRUE(persistence_dir_.CreateUniqueTempDir());
    persistence_path_ = persistence_dir_.GetPath().AppendASCII("SCT Auditing");

    // Set up a NetworkContext.
    mojom::NetworkContextParamsPtr context_params =
        CreateNetworkContextParamsForTesting();
    context_params->cert_verifier_params =
        FakeTestCertVerifierParamsFactory::GetCertVerifierParams();
    context_params->enable_sct_auditing = true;
    network_context_ = std::make_unique<NetworkContext>(
        network_service_.get(),
        network_context_remote_.BindNewPipeAndPassReceiver(),
        std::move(context_params));

    // Set up SCT auditing configuration.
    auto* cache = network_service_->sct_auditing_cache();
    cache->set_enabled(true);
    cache->set_sampling_rate(1.0);
    cache->set_report_uri(GURL("https://example.test"));
    cache->set_traffic_annotation(
        net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));
  }

  // Get the contents of `persistence_path_`. Pumps the message loop before
  // returning the result.
  std::string GetTestFileContents() {
    task_environment_.RunUntilIdle();
    std::string file_contents;
    base::ReadFileToString(persistence_path_, &file_contents);
    return file_contents;
  }

  // Check whether `substring` appears in the file contents at
  // `persistence_path_`.
  bool FileContentsHasString(const std::string& substring) {
    auto contents = GetTestFileContents();
    auto position = contents.find(substring);
    return position != std::string::npos;
  }

  // Waits for `expected_requests` to be seen by the TestURLLoaderFactory. Note
  // that this only counts HTTP requests, so network errors (e.g., cert errors)
  // won't count.
  void WaitForRequests(size_t expected_requests) {
    // Initialize a new RunLoop, so that tests can call WaitForRequests()
    // multiple times, if needed.
    run_loop_ = std::make_unique<base::RunLoop>();

    if (num_requests_seen_ >= expected_requests) {
      return;
    }

    // Add a TestURLLoaderFactory interceptor to count requests seen.
    url_loader_factory_.SetInterceptor(base::BindLambdaForTesting(
        [&](const network::ResourceRequest& request) {
          ++num_requests_seen_;
          if (run_loop_->running() && num_requests_seen_ >= expected_requests) {
            run_loop_->QuitWhenIdle();
          }
        }));

    run_loop_->Run();
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::IO,
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::ScopedTempDir persistence_dir_;
  base::FilePath persistence_path_;
  std::unique_ptr<NetworkService> network_service_;
  std::unique_ptr<NetworkContext> network_context_;

  TestURLLoaderFactory url_loader_factory_;

  std::unique_ptr<base::RunLoop> run_loop_;
  size_t num_requests_seen_ = 0;

  // Stores the mojo::Remote<mojom::NetworkContext> of the most recently created
  // NetworkContext.
  mojo::Remote<mojom::NetworkContext> network_context_remote_;
};

// Test that when the retry+persistence feature is disabled no reports will be
// persisted on disk.
TEST_F(SCTAuditingHandlerTest, PersistenceFeatureDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      features::kSCTAuditingRetryAndPersistReports);

  mojo::PendingRemote<network::mojom::URLLoaderFactory> factory_remote;
  url_loader_factory_.Clone(factory_remote.InitWithNewPipeAndPassReceiver());

  SCTAuditingHandler handler(network_context_.get(), persistence_path_);
  handler.SetEnabled(true);
  handler.SetURLLoaderFactoryForTesting(std::move(factory_remote));

  // `file_writer` should not be created for this handler.
  auto* file_writer = handler.GetFileWriterForTesting();
  EXPECT_EQ(file_writer, nullptr);
}

// Test that when the SCTAuditingHandler is created without a persistence path
// (e.g., as happens for ephemeral profiles), no file writer is created.
TEST_F(SCTAuditingHandlerTest, HandlerWithoutPersistencePath) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kSCTAuditingRetryAndPersistReports);

  mojo::PendingRemote<network::mojom::URLLoaderFactory> factory_remote;
  url_loader_factory_.Clone(factory_remote.InitWithNewPipeAndPassReceiver());

  // Set up a Handler with an empty `persistence_path`.
  SCTAuditingHandler handler(network_context_.get(), base::FilePath());
  handler.SetEnabled(true);
  handler.SetURLLoaderFactoryForTesting(std::move(factory_remote));

  // `file_writer` should not be created for this handler.
  auto* file_writer = handler.GetFileWriterForTesting();
  ASSERT_EQ(file_writer, nullptr);
}

// Test that when the SCTAuditingHandler is created with a valid persistence
// path, then pending reports get stored to disk.
TEST_F(SCTAuditingHandlerTest, HandlerWithPersistencePath) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kSCTAuditingRetryAndPersistReports);

  mojo::PendingRemote<network::mojom::URLLoaderFactory> factory_remote;
  url_loader_factory_.Clone(factory_remote.InitWithNewPipeAndPassReceiver());

  SCTAuditingHandler handler(network_context_.get(), persistence_path_);
  handler.SetEnabled(true);
  handler.SetURLLoaderFactoryForTesting(std::move(factory_remote));

  auto* file_writer = handler.GetFileWriterForTesting();
  ASSERT_TRUE(file_writer);

  // Add a Reporter to the Handler and check that it gets scheduled to be
  // persisted to disk.
  auto report = std::make_unique<sct_auditing::SCTClientReport>();
  auto* tls_report = report->add_certificate_report();
  auto* connection_context = tls_report->mutable_context();
  auto* origin = connection_context->mutable_origin();
  origin->set_hostname("example.test");
  origin->set_port(443);

  // Fake a HashValue to use as the key.
  net::HashValue reporter_key(net::HASH_VALUE_SHA256);

  handler.AddReporter(reporter_key, std::move(report));
  ASSERT_EQ(handler.GetPendingReportersForTesting()->size(), 1u);
  ASSERT_TRUE(file_writer->HasPendingWrite());

  // Check that file got written with the expected content.
  file_writer->DoScheduledWrite();
  ASSERT_FALSE(file_writer->HasPendingWrite());
  EXPECT_TRUE(FileContentsHasString(reporter_key.ToString()));

  WaitForRequests(1u);

  EXPECT_EQ(1, url_loader_factory_.NumPending());

  // Simulate the server returning 200 OK to the report request.
  url_loader_factory_.SimulateResponseForPendingRequest(
      "https://example.test",
      /*content=*/"",
      /*status=*/net::HTTP_OK);

  // Check that there are no pending requests anymore.
  EXPECT_EQ(0, url_loader_factory_.NumPending());

  // Check that the pending reporter was deleted on successful completion.
  EXPECT_TRUE(handler.GetPendingReportersForTesting()->empty());

  // Check that the Reporter is no longer in the file.
  file_writer->DoScheduledWrite();
  ASSERT_FALSE(file_writer->HasPendingWrite());
  EXPECT_FALSE(FileContentsHasString(reporter_key.ToString()));
}

// Tests that serializing reports and then deserializing them results in the
// same data.
TEST_F(SCTAuditingHandlerTest, DataRoundTrip) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kSCTAuditingRetryAndPersistReports);

  // Create a Handler, add a reporter, and wait for it to get persisted.
  {
    SCTAuditingHandler handler(network_context_.get(), persistence_path_);
    handler.SetEnabled(true);
    mojo::PendingRemote<network::mojom::URLLoaderFactory> factory_remote;
    url_loader_factory_.Clone(factory_remote.InitWithNewPipeAndPassReceiver());
    handler.SetURLLoaderFactoryForTesting(std::move(factory_remote));

    auto* file_writer = handler.GetFileWriterForTesting();
    ASSERT_TRUE(file_writer);

    ASSERT_TRUE(handler.is_enabled());
    ASSERT_FALSE(file_writer->HasPendingWrite());

    // Add a Reporter to the Handler and check that it gets scheduled to be
    // persisted to disk.
    auto report = std::make_unique<sct_auditing::SCTClientReport>();
    auto* tls_report = report->add_certificate_report();
    auto* connection_context = tls_report->mutable_context();
    auto* origin = connection_context->mutable_origin();
    origin->set_hostname("example.test");
    origin->set_port(443);

    // Fake a HashValue to use as the key.
    net::HashValue reporter_key(net::HASH_VALUE_SHA256);

    handler.AddReporter(reporter_key, std::move(report));
    ASSERT_EQ(handler.GetPendingReportersForTesting()->size(), 1u);
    ASSERT_TRUE(file_writer->HasPendingWrite());

    // Check that file got written with the expected content.
    file_writer->DoScheduledWrite();
    ASSERT_FALSE(file_writer->HasPendingWrite());
    EXPECT_TRUE(FileContentsHasString(reporter_key.ToString()));
  }

  // Create a second Handler using the same persistence path. It should load
  // the same data.
  {
    SCTAuditingHandler handler(network_context_.get(), persistence_path_);
    handler.SetEnabled(true);
    mojo::PendingRemote<network::mojom::URLLoaderFactory> factory_remote;
    url_loader_factory_.Clone(factory_remote.InitWithNewPipeAndPassReceiver());
    handler.SetURLLoaderFactoryForTesting(std::move(factory_remote));

    auto* file_writer = handler.GetFileWriterForTesting();
    ASSERT_TRUE(file_writer);

    WaitForRequests(1u);

    auto* pending_reporters = handler.GetPendingReportersForTesting();
    ASSERT_EQ(1u, pending_reporters->size());

    // Reporter should be for "example.test:443" as added in the first Handler.
    for (const auto& reporter : *pending_reporters) {
      auto origin =
          reporter.second->report()->certificate_report(0).context().origin();
      EXPECT_EQ(origin.hostname(), "example.test");
      EXPECT_EQ(origin.port(), 443);
    }
  }
}

// Test that deserializing bad data shouldn't result in any reporters being
// created.
TEST_F(SCTAuditingHandlerTest, DeserializeBadData) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kSCTAuditingRetryAndPersistReports);

  mojo::PendingRemote<network::mojom::URLLoaderFactory> factory_remote;
  url_loader_factory_.Clone(factory_remote.InitWithNewPipeAndPassReceiver());

  // Set an empty persistence path so no file IO is performed.
  SCTAuditingHandler handler(network_context_.get(), base::FilePath());
  handler.SetURLLoaderFactoryForTesting(std::move(factory_remote));

  // Non-JSON data.
  handler.DeserializeData("Blorp");
  EXPECT_EQ(handler.GetPendingReportersForTesting()->size(), 0u);

  // JSON data but non-sensical.
  handler.DeserializeData("[15]");
  EXPECT_EQ(handler.GetPendingReportersForTesting()->size(), 0u);

  // JSON data in the right format, but with invalid keys.
  handler.DeserializeData(R"([{"blorp": "a", "bloop": "b", "bleep": "c"}])");
  EXPECT_EQ(handler.GetPendingReportersForTesting()->size(), 0u);

  // JSON data with the right format and keys, but data is invalid.
  handler.DeserializeData(
      R"([{"reporter_key": "a", "report": "b", "backoff_entry": ["c"]}])");
  EXPECT_EQ(handler.GetPendingReportersForTesting()->size(), 0u);

  // Check that no file got written to the persistence path.
  EXPECT_EQ(GetTestFileContents(), std::string());
}

// Test that a handler loads valid persisted data from disk and creates pending
// reporters for each entry.
TEST_F(SCTAuditingHandlerTest, HandlerWithExistingPersistedData) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kSCTAuditingRetryAndPersistReports);

  // Set up previously persisted data on disk:
  // - Default-initialized net::HashValue(net::HASH_VALUE_SHA256)
  // - Empty SCTClientReport for origin "example.test:443".
  // - A simple BackoffEntry.
  std::string persisted_report =
      R"(
        [{
          "reporter_key":
            "sha256/qqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqo=",
          "report": "EhUKExIRCgxleGFtcGxlLnRlc3QQuwM=",
          "backoff_entry": [2,0,"30000000","11644578625551798"]
        }]
      )";
  ASSERT_TRUE(base::WriteFile(persistence_path_, persisted_report));

  mojo::PendingRemote<network::mojom::URLLoaderFactory> factory_remote;
  url_loader_factory_.Clone(factory_remote.InitWithNewPipeAndPassReceiver());

  SCTAuditingHandler handler(network_context_.get(), persistence_path_);
  handler.SetEnabled(true);
  handler.SetURLLoaderFactoryForTesting(std::move(factory_remote));

  auto* file_writer = handler.GetFileWriterForTesting();
  ASSERT_TRUE(file_writer);

  WaitForRequests(1u);

  EXPECT_EQ(handler.GetPendingReportersForTesting()->size(), 1u);
  EXPECT_EQ(1, url_loader_factory_.NumPending());

  // Simulate the server returning 200 OK to the report request.
  url_loader_factory_.SimulateResponseForPendingRequest(
      "https://example.test",
      /*content=*/"",
      /*status=*/net::HTTP_OK);

  // Check that there is no pending requests anymore.
  EXPECT_EQ(0, url_loader_factory_.NumPending());

  // Check that the pending reporter was deleted on successful completion.
  EXPECT_TRUE(handler.GetPendingReportersForTesting()->empty());

  // Check that the Reporter is no longer in the file.
  file_writer->DoScheduledWrite();
  ASSERT_FALSE(file_writer->HasPendingWrite());
  EXPECT_FALSE(FileContentsHasString(
      "sha256/qqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqo="));
}

// Test that scheduling a retry causes the failure count to increment in
// persisted storage.
TEST_F(SCTAuditingHandlerTest, RetryUpdatesPersistedBackoffEntry) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kSCTAuditingRetryAndPersistReports);

  // Set up previously persisted data on disk:
  // - Default-initialized net::HashValue(net::HASH_VALUE_SHA256)
  // - Empty SCTClientReport for origin "example.test:443".
  // - A simple BackoffEntry with a failure count of "1".
  std::string persisted_report =
      R"(
        [{
          "reporter_key":
            "sha256/qqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqo=",
          "report": "EhUKExIRCgxleGFtcGxlLnRlc3QQuwM=",
          "backoff_entry": [2,1,"30000000","11644578625551798"]
        }]
      )";
  ASSERT_TRUE(base::WriteFile(persistence_path_, persisted_report));

  mojo::PendingRemote<network::mojom::URLLoaderFactory> factory_remote;
  url_loader_factory_.Clone(factory_remote.InitWithNewPipeAndPassReceiver());

  SCTAuditingHandler handler(network_context_.get(), persistence_path_);
  handler.SetEnabled(true);
  handler.SetURLLoaderFactoryForTesting(std::move(factory_remote));

  auto* file_writer = handler.GetFileWriterForTesting();
  ASSERT_TRUE(file_writer);

  WaitForRequests(1u);

  EXPECT_EQ(handler.GetPendingReportersForTesting()->size(), 1u);
  EXPECT_EQ(url_loader_factory_.NumPending(), 1);

  // Simulate the server returning error to the report request. The Reporter
  // should schedule a retry and trigger updating the persisted storage.
  url_loader_factory_.SimulateResponseForPendingRequest(
      "https://example.test",
      /*content=*/"",
      /*status=*/net::HTTP_TOO_MANY_REQUESTS);
  EXPECT_EQ(url_loader_factory_.NumPending(), 0);
  EXPECT_EQ(handler.GetPendingReportersForTesting()->size(), 1u);
  ASSERT_TRUE(file_writer->HasPendingWrite());

  // Check that the Reporter is updated in the persisted storage file.
  file_writer->DoScheduledWrite();
  ASSERT_FALSE(file_writer->HasPendingWrite());
  EXPECT_TRUE(FileContentsHasString(
      "sha256/qqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqo="));
  // Persisted backoff entry should have the failure count incrememted to 2.
  // (The first value of a serialized BackoffEntry is the version, the second is
  // the failure count.)
  EXPECT_TRUE(FileContentsHasString(R"("backoff_entry":[2,2,)"));
}

// Test that retries carry over correctly. Specifically, a persisted entry with
// 14 retries already (one less than kMaxRetries), if after being loaded from
// persisted storage tries and fails once more, should get deleted.
TEST_F(SCTAuditingHandlerTest, RestoringMaxRetries) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kSCTAuditingRetryAndPersistReports);

  // Set up previously persisted data on disk:
  // - Default-initialized net::HashValue(net::HASH_VALUE_SHA256)
  // - Empty SCTClientReport for origin "example.test:443".
  // - A simple BackoffEntry with a failure count of "15" (so it is scheduled to
  //   retry for the 15th and final time).
  std::string persisted_report =
      R"(
        [{
          "reporter_key":
            "sha256/qqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqo=",
          "report": "EhUKExIRCgxleGFtcGxlLnRlc3QQuwM=",
          "backoff_entry": [2,15,"30000000","11644578625551798"]
        }]
      )";
  ASSERT_TRUE(base::WriteFile(persistence_path_, persisted_report));

  mojo::PendingRemote<network::mojom::URLLoaderFactory> factory_remote;
  url_loader_factory_.Clone(factory_remote.InitWithNewPipeAndPassReceiver());

  SCTAuditingHandler handler(network_context_.get(), persistence_path_);
  handler.SetEnabled(true);
  handler.SetURLLoaderFactoryForTesting(std::move(factory_remote));

  auto* file_writer = handler.GetFileWriterForTesting();
  ASSERT_TRUE(file_writer);

  WaitForRequests(1u);

  EXPECT_EQ(handler.GetPendingReportersForTesting()->size(), 1u);
  EXPECT_EQ(url_loader_factory_.NumPending(), 1);

  // Simulate the server returning error to the report request. The Reporter
  // should schedule a retry and trigger updating the persisted storage.
  url_loader_factory_.SimulateResponseForPendingRequest(
      "https://example.test",
      /*content=*/"",
      /*status=*/net::HTTP_TOO_MANY_REQUESTS);
  EXPECT_EQ(url_loader_factory_.NumPending(), 0);

  // Pending reporter should get deleted as it has reached max retries.
  EXPECT_EQ(handler.GetPendingReportersForTesting()->size(), 0u);

  // Reporter state on disk should get deleted as well.
  file_writer->DoScheduledWrite();
  ASSERT_FALSE(file_writer->HasPendingWrite());
  EXPECT_FALSE(FileContentsHasString(
      "sha256/qqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqo="));
}

}  // namespace

}  // namespace network
