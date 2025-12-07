// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/segmentation_platform/model/ukm_data_manager_test_utils.h"

#import "base/run_loop.h"
#import "components/history/core/browser/history_service.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/segmentation_platform/embedder/model_provider_factory_impl.h"
#import "components/segmentation_platform/internal/database/ukm_database.h"
#import "components/segmentation_platform/internal/execution/mock_model_provider.h"
#import "components/segmentation_platform/internal/metadata/metadata_writer.h"
#import "components/segmentation_platform/internal/segmentation_platform_service_impl.h"
#import "components/segmentation_platform/internal/signals/ukm_observer.h"
#import "components/segmentation_platform/internal/ukm_data_manager.h"
#import "components/segmentation_platform/public/proto/segmentation_platform.pb.h"
#import "ios/chrome/browser/history/model/history_service_factory.h"
#import "ios/chrome/browser/segmentation_platform/model/segmentation_platform_service_factory.h"
#import "ios/chrome/browser/segmentation_platform/model/ukm_database_client.h"
#import "services/metrics/public/cpp/ukm_builders.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "url/gurl.h"

namespace segmentation_platform {

namespace {

using ::segmentation_platform::proto::SegmentId;
using ::testing::Return;
using ::ukm::builders::PageLoad;

// Returns a sample UKM entry.
ukm::mojom::UkmEntryPtr GetSamplePageLoadEntry(ukm::SourceId source_id) {
  ukm::mojom::UkmEntryPtr entry = ukm::mojom::UkmEntry::New();
  entry->source_id = source_id;
  entry->event_hash = PageLoad::kEntryNameHash;
  entry->metrics[PageLoad::kCpuTimeNameHash] = 10;
  entry->metrics[PageLoad::kIsNewBookmarkNameHash] = 20;
  entry->metrics[PageLoad::kIsNTPCustomLinkNameHash] = 30;
  return entry;
}

// Runs the given query and returns the result as float value. See
// RunReadOnlyQueries() for more info.
std::optional<float> RunQueryAndGetResult(UkmDatabase* database,
                                          UkmDatabase::CustomSqlQuery&& query) {
  std::optional<float> output;
  UkmDatabase::QueryList queries;
  queries.emplace(0, std::move(query));
  base::RunLoop wait_for_query;
  database->RunReadOnlyQueries(
      std::move(queries),
      base::BindOnce(
          [](base::OnceClosure quit, std::optional<float>* output, bool success,
             processing::IndexedTensors tensor) {
            if (success) {
              EXPECT_EQ(1u, tensor.size());
              EXPECT_EQ(1u, tensor.at(0).size());
              *output = tensor.at(0)[0].float_val;
            }
            std::move(quit).Run();
          },
          wait_for_query.QuitClosure(), &output));
  wait_for_query.Run();
  return output;
}

}  // namespace

UkmDataManagerTestUtils::UkmDataManagerTestUtils(
    ukm::TestUkmRecorder* ukm_recorder,
    bool owned_db_client)
    : ukm_recorder_(ukm_recorder) {
  if (owned_db_client) {
    owned_db_client_ = std::make_unique<UkmDatabaseClient>();
    ukm_database_client_ = owned_db_client_.get();
  } else {
    ukm_database_client_ = &UkmDatabaseClientHolder::GetClientInstance(nullptr);
  }
}
UkmDataManagerTestUtils::~UkmDataManagerTestUtils() {
#if !BUILDFLAG(IS_ANDROID)
  // The client should be torn down after profile is destroyed. On Android
  // browser tests the profile is never destroyed, so do not tear down the
  // client.
  ukm_database_client_->TearDownForTesting();
#endif
  ukm_database_client_ = nullptr;
}

void UkmDataManagerTestUtils::PreProfileInit(
    const std::map<SegmentId, proto::SegmentationModelMetadata>&
        default_overrides) {
  // Set test recorder before UkmObserver is created.
  ukm_database_client_->set_ukm_recorder_for_testing(ukm_recorder_);

  for (const auto& segment : default_overrides) {
    auto provider = std::make_unique<MockDefaultModelProvider>(segment.first,
                                                               segment.second);

    default_overrides_[segment.first] = provider.get();
    // Default model must be overridden before the platform is created:
    TestDefaultModelOverride::GetInstance().SetModelForTesting(
        segment.first, std::move(provider));
  }

  if (owned_db_client_) {
    owned_db_client_->PreProfileInit(/*in_memory_database=*/true);
  }
}

void UkmDataManagerTestUtils::SetupForProfile(ProfileIOS* profile) {
  UkmDatabaseClientHolder::SetUkmClientForTesting(profile,
                                                  ukm_database_client_.get());
  CHECK_EQ(ukm_database_client_.get(),
           &UkmDatabaseClientHolder::GetClientInstance(profile));
  history_service_ = ios::HistoryServiceFactory::GetForProfile(
      profile, ServiceAccessType::EXPLICIT_ACCESS);
  // Create the platform to kick off initialization.
  segmentation_platform::SegmentationPlatformServiceFactory::GetForProfile(
      profile);
}

void UkmDataManagerTestUtils::WillDestroyProfile(ProfileIOS* profile) {
  UkmDatabaseClientHolder::SetUkmClientForTesting(profile, nullptr);
}

void UkmDataManagerTestUtils::WaitForUkmObserverRegistration() {
  UkmObserver* observer = ukm_database_client_->ukm_observer_for_testing();
  while (!observer->is_started_for_testing()) {
    base::RunLoop().RunUntilIdle();
  }
}

proto::SegmentationModelMetadata
UkmDataManagerTestUtils::GetSamplePageLoadMetadata(const std::string& query) {
  proto::SegmentationModelMetadata metadata;
  MetadataWriter writer(&metadata);
  writer.AddOutputConfigForBinaryClassifier(
      /*threshold=*/0.5f,
      /*positive_label=*/"Show",
      /*negative_label=*/"NotShow");
  metadata.set_time_unit(proto::TimeUnit::DAY);
  metadata.set_bucket_duration(42u);

  auto* feature = metadata.add_input_features();
  auto* sql_feature = feature->mutable_sql_feature();
  sql_feature->set_sql(query);

  auto* ukm_event = sql_feature->mutable_signal_filter()->add_ukm_events();
  ukm_event->set_event_hash(PageLoad::kEntryNameHash);
  ukm_event->add_metric_hash_filter(PageLoad::kCpuTimeNameHash);
  ukm_event->add_metric_hash_filter(PageLoad::kIsNewBookmarkNameHash);
  return metadata;
}

void UkmDataManagerTestUtils::RecordPageLoadUkm(const GURL& url,
                                                base::Time history_timestamp) {
  UkmObserver* observer = ukm_database_client_->ukm_observer_for_testing();
  // Ensure that the observer is started before recording metrics.
  ASSERT_TRUE(observer->is_started_for_testing());
  // Ensure that OTR profiles are not started in the test.
  ASSERT_FALSE(observer->is_paused_for_testing());

  ukm_recorder_->AddEntry(GetSamplePageLoadEntry(source_id_counter_));
  ukm_recorder_->UpdateSourceURL(source_id_counter_, url);
  source_id_counter_++;

  // Without a history service the recorded URLs will not be written to
  // database.
  ASSERT_TRUE(history_service_);
  history_service_->AddPage(url, history_timestamp,
                            history::VisitSource::SOURCE_BROWSED);
}

bool UkmDataManagerTestUtils::IsUrlInDatabase(const GURL& url) {
  UkmDatabase::CustomSqlQuery query("SELECT 1 FROM urls WHERE url=?",
                                    {processing::ProcessedValue(url.spec())});
  std::optional<float> result = RunQueryAndGetResult(
      ukm_database_client_->GetUkmDataManager()->GetUkmDatabase(),
      std::move(query));
  return !!result;
}

MockDefaultModelProvider* UkmDataManagerTestUtils::GetDefaultOverride(
    proto::SegmentId segment_id) {
  return default_overrides_[segment_id];
}

}  // namespace segmentation_platform
