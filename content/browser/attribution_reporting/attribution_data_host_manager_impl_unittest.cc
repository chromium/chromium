// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_data_host_manager_impl.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/barrier_closure.h"
#include "base/containers/flat_map.h"
#include "base/metrics/histogram_base.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "content/browser/attribution_reporting/attribution_aggregation_keys.h"
#include "content/browser/attribution_reporting/attribution_manager.h"
#include "content/browser/attribution_reporting/attribution_source_type.h"
#include "content/browser/attribution_reporting/attribution_test_utils.h"
#include "content/browser/attribution_reporting/attribution_trigger.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/numeric/int128.h"
#include "third_party/blink/public/common/attribution_reporting/constants.h"
#include "third_party/blink/public/mojom/conversions/attribution_data_host.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

namespace {

using ::testing::_;
using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::InSequence;
using ::testing::Mock;
using ::testing::Optional;

using Checkpoint = ::testing::MockFunction<void(int step)>;

constexpr char kSourceDataHandleStatusMetric[] =
    "Conversions.SourceDataHandleStatus";
constexpr char kTriggerDataHandleStatusMetric[] =
    "Conversions.TriggerDataHandleStatus";

constexpr char kRegisterSourceJson[] =
    R"json({"source_event_id":"5","destination":"https://destination.example"})json";

struct ExpectedTriggerQueueEventCounts {
  base::HistogramBase::Count skipped_queue = 0;
  base::HistogramBase::Count dropped = 0;
  base::HistogramBase::Count enqueued = 0;
  base::HistogramBase::Count processed_with_delay = 0;
  base::HistogramBase::Count flushed = 0;

  base::flat_map<base::TimeDelta, base::HistogramBase::Count> delays;
};

void CheckTriggerQueueHistograms(const base::HistogramTester& histograms,
                                 ExpectedTriggerQueueEventCounts expected) {
  static constexpr char kEventsMetric[] = "Conversions.TriggerQueueEvents";
  static constexpr char kDelayMetric[] = "Conversions.TriggerQueueDelay";

  histograms.ExpectBucketCount(kEventsMetric, 0, expected.skipped_queue);
  histograms.ExpectBucketCount(kEventsMetric, 1, expected.dropped);
  histograms.ExpectBucketCount(kEventsMetric, 2, expected.enqueued);
  histograms.ExpectBucketCount(kEventsMetric, 3, expected.processed_with_delay);
  histograms.ExpectBucketCount(kEventsMetric, 4, expected.flushed);

  base::HistogramBase::Count total = 0;

  for (const auto& [delay, count] : expected.delays) {
    histograms.ExpectTimeBucketCount(kDelayMetric, delay, count);
    total += count;
  }

  histograms.ExpectTotalCount(kDelayMetric, total);
}

struct RemoteDataHost {
  BrowserTaskEnvironment& task_environment;
  mojo::Remote<blink::mojom::AttributionDataHost> data_host;

  ~RemoteDataHost() {
    // Disconnect the data host.
    data_host.reset();
    task_environment.RunUntilIdle();
  }
};

class AttributionDataHostManagerImplTest : public testing::Test {
 public:
  AttributionDataHostManagerImplTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        data_host_manager_(&mock_manager_) {}

 protected:
  BrowserTaskEnvironment task_environment_;
  MockAttributionManager mock_manager_;
  AttributionDataHostManagerImpl data_host_manager_;
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
};

TEST_F(AttributionDataHostManagerImplTest, SourceDataHost_SourceRegistered) {
  base::HistogramTester histograms;

  auto page_origin = url::Origin::Create(GURL("https://page.example"));
  auto destination_origin =
      url::Origin::Create(GURL("https://trigger.example"));
  auto reporting_origin = url::Origin::Create(GURL("https://reporter.example"));
  EXPECT_CALL(
      mock_manager_,
      HandleSource(
          AllOf(SourceTypeIs(AttributionSourceType::kEvent),
                SourceEventIdIs(10), DestinationOriginIs(destination_origin),
                ImpressionOriginIs(page_origin), SourcePriorityIs(20),
                SourceDebugKeyIs(789),
                AggregationKeysAre(*AttributionAggregationKeys::FromKeys(
                    {{"key", absl::MakeUint128(/*high=*/5, /*low=*/345)}})))));
  {
    RemoteDataHost data_host_remote{.task_environment = task_environment_};
    data_host_manager_.RegisterDataHost(
        data_host_remote.data_host.BindNewPipeAndPassReceiver(), page_origin);

    task_environment_.FastForwardBy(base::Milliseconds(1));

    auto source_data = blink::mojom::AttributionSourceData::New();
    source_data->source_event_id = 10;
    source_data->destination = destination_origin;
    source_data->reporting_origin = reporting_origin;
    source_data->priority = 20;
    source_data->debug_key = blink::mojom::AttributionDebugKey::New(789);
    source_data->filter_data = blink::mojom::AttributionFilterData::New();
    source_data->aggregation_keys = {
        {"key", absl::MakeUint128(/*high=*/5, /*low=*/345)},
    };
    data_host_remote.data_host->SourceDataAvailable(std::move(source_data));
    data_host_remote.data_host.FlushForTesting();
  }

  histograms.ExpectUniqueSample("Conversions.RegisteredSourcesPerDataHost", 1,
                                1);
  histograms.ExpectTimeBucketCount("Conversions.SourceEligibleDataHostLifeTime",
                                   base::Milliseconds(1), 1);
  // kSuccess = 0.
  histograms.ExpectUniqueSample(kSourceDataHandleStatusMetric, 0, 1);
}

TEST_F(AttributionDataHostManagerImplTest,
       SourceDataHost_OriginTrustworthyChecksPerformed) {
  base::HistogramTester histograms;

  const char kLocalHost[] = "http://localhost";

  struct {
    const char* source_origin;
    const char* destination_origin;
    const char* reporting_origin;
    bool source_expected;
    const char* bad_message = nullptr;
  } kTestCases[] = {
      {.source_origin = kLocalHost,
       .destination_origin = kLocalHost,
       .reporting_origin = kLocalHost,
       .source_expected = true},
      {.source_origin = "http://127.0.0.1",
       .destination_origin = "http://127.0.0.1",
       .reporting_origin = "http://127.0.0.1",
       .source_expected = true},
      {.source_origin = kLocalHost,
       .destination_origin = kLocalHost,
       .reporting_origin = "http://insecure.com",
       .source_expected = false,
       .bad_message = "AttributionDataHost: Reporting origin must be secure."},
      {.source_origin = kLocalHost,
       .destination_origin = "http://insecure.com",
       .reporting_origin = kLocalHost,
       .source_expected = false,
       .bad_message =
           "AttributionDataHost: Destination origin must be secure."},
      {.source_origin = "https://secure.com",
       .destination_origin = "https://secure.com",
       .reporting_origin = "https://secure.com",
       .source_expected = true},
  };

  for (const auto& test_case : kTestCases) {
    EXPECT_CALL(mock_manager_, HandleSource).Times(test_case.source_expected);

    mojo::test::BadMessageObserver bad_message_observer;

    RemoteDataHost data_host_remote{.task_environment = task_environment_};
    data_host_manager_.RegisterDataHost(
        data_host_remote.data_host.BindNewPipeAndPassReceiver(),
        url::Origin::Create(GURL(test_case.source_origin)));

    auto source_data = blink::mojom::AttributionSourceData::New();
    source_data->destination =
        url::Origin::Create(GURL(test_case.destination_origin));
    source_data->reporting_origin =
        url::Origin::Create(GURL(test_case.reporting_origin));
    source_data->filter_data = blink::mojom::AttributionFilterData::New();
    data_host_remote.data_host->SourceDataAvailable(std::move(source_data));
    data_host_remote.data_host.FlushForTesting();

    Mock::VerifyAndClear(&mock_manager_);

    EXPECT_EQ(bad_message_observer.got_bad_message(), !!test_case.bad_message);

    if (test_case.bad_message) {
      EXPECT_EQ(bad_message_observer.WaitForBadMessage(),
                test_case.bad_message);
    }
  }

  histograms.ExpectUniqueSample("Conversions.RegisteredSourcesPerDataHost", 1,
                                3);
  // kSuccess = 0.
  histograms.ExpectBucketCount(kSourceDataHandleStatusMetric, 0, 3);
  // kUntrustworthyOrigin = 1.
  histograms.ExpectBucketCount(kSourceDataHandleStatusMetric, 1, 2);
}

TEST_F(AttributionDataHostManagerImplTest,
       SourceDataHost_FilterSizeCheckPerformed) {
  for (const auto& test_case : kAttributionFilterSizeTestCases) {
    base::HistogramTester histograms;

    SCOPED_TRACE(test_case.description);  // EXPECT_CALL doesn't support <<
    EXPECT_CALL(mock_manager_, HandleSource).Times(test_case.valid);

    mojo::Remote<blink::mojom::AttributionDataHost> data_host_remote;
    data_host_manager_.RegisterDataHost(
        data_host_remote.BindNewPipeAndPassReceiver(),
        url::Origin::Create(GURL("https://page.example")));

    mojo::test::BadMessageObserver bad_message_observer;

    auto source_data = blink::mojom::AttributionSourceData::New();
    source_data->destination =
        url::Origin::Create(GURL("https://trigger.example"));
    source_data->reporting_origin =
        url::Origin::Create(GURL("https://reporter.example"));
    source_data->filter_data =
        blink::mojom::AttributionFilterData::New(test_case.AsMap());
    data_host_remote->SourceDataAvailable(std::move(source_data));
    data_host_remote.FlushForTesting();

    Mock::VerifyAndClear(&mock_manager_);

    EXPECT_NE(bad_message_observer.got_bad_message(), test_case.valid);

    if (!test_case.valid) {
      EXPECT_EQ(bad_message_observer.WaitForBadMessage(),
                "AttributionDataHost: Invalid filter data.");
    }

    // kSuccess = 0, kInvalidData = 3.
    histograms.ExpectUniqueSample(kSourceDataHandleStatusMetric,
                                  test_case.valid ? 0 : 3, 1);
  }
}

TEST_F(AttributionDataHostManagerImplTest,
       SourceDataHost_FilterSourceTypeCheckPerformed) {
  const struct {
    std::string description;
    base::flat_map<std::string, std::vector<std::string>> filter_data;
    bool valid;
  } kTestCases[]{
      {
          "valid",
          {{"SOURCE_TYPE", {}}},
          true,
      },
      {
          "invalid",
          {{"source_type", {}}},
          false,
      },
  };

  for (const auto& test_case : kTestCases) {
    base::HistogramTester histograms;

    SCOPED_TRACE(test_case.description);  // EXPECT_CALL doesn't support <<
    EXPECT_CALL(mock_manager_, HandleSource).Times(test_case.valid);

    mojo::Remote<blink::mojom::AttributionDataHost> data_host_remote;
    data_host_manager_.RegisterDataHost(
        data_host_remote.BindNewPipeAndPassReceiver(),
        url::Origin::Create(GURL("https://page.example")));

    mojo::test::BadMessageObserver bad_message_observer;

    auto source_data = blink::mojom::AttributionSourceData::New();
    source_data->destination =
        url::Origin::Create(GURL("https://trigger.example"));
    source_data->reporting_origin =
        url::Origin::Create(GURL("https://reporter.example"));
    source_data->filter_data =
        blink::mojom::AttributionFilterData::New(test_case.filter_data);
    data_host_remote->SourceDataAvailable(std::move(source_data));
    data_host_remote.FlushForTesting();

    Mock::VerifyAndClear(&mock_manager_);

    EXPECT_NE(bad_message_observer.got_bad_message(), test_case.valid);

    if (!test_case.valid) {
      EXPECT_EQ(bad_message_observer.WaitForBadMessage(),
                "AttributionDataHost: Invalid filter data.");
    }

    // kSuccess = 0, kInvalidData = 3.
    histograms.ExpectUniqueSample(kSourceDataHandleStatusMetric,
                                  test_case.valid ? 0 : 3, 1);
  }
}

TEST_F(AttributionDataHostManagerImplTest,
       SourceDataHost_ReceiverDestinationsMayDiffer) {
  base::HistogramTester histograms;

  Checkpoint checkpoint;
  {
    InSequence seq;

    EXPECT_CALL(mock_manager_, HandleSource);
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(mock_manager_, HandleSource);
    EXPECT_CALL(checkpoint, Call(2));
    EXPECT_CALL(mock_manager_, HandleSource);
    EXPECT_CALL(checkpoint, Call(3));
    EXPECT_CALL(mock_manager_, HandleSource);
  }

  auto page_origin = url::Origin::Create(GURL("https://page.example"));
  auto destination_origin =
      url::Origin::Create(GURL("https://trigger.example"));
  auto reporting_origin = url::Origin::Create(GURL("https://reporter.example"));

  {
    RemoteDataHost data_host_remote{.task_environment = task_environment_};
    data_host_manager_.RegisterDataHost(
        data_host_remote.data_host.BindNewPipeAndPassReceiver(), page_origin);

    auto source_data = blink::mojom::AttributionSourceData::New();
    source_data->destination = destination_origin;
    source_data->reporting_origin = reporting_origin;
    source_data->filter_data = blink::mojom::AttributionFilterData::New();
    data_host_remote.data_host->SourceDataAvailable(source_data.Clone());
    data_host_remote.data_host.FlushForTesting();

    checkpoint.Call(1);

    data_host_remote.data_host->SourceDataAvailable(source_data.Clone());
    data_host_remote.data_host.FlushForTesting();

    checkpoint.Call(2);

    source_data->destination =
        url::Origin::Create(GURL("https://other-trigger.example"));
    data_host_remote.data_host->SourceDataAvailable(source_data.Clone());
    data_host_remote.data_host.FlushForTesting();

    checkpoint.Call(3);
    data_host_remote.data_host->SourceDataAvailable(std::move(source_data));
    data_host_remote.data_host.FlushForTesting();
  }

  histograms.ExpectUniqueSample("Conversions.RegisteredSourcesPerDataHost", 4,
                                1);
  // kSuccess = 0.
  histograms.ExpectBucketCount(kSourceDataHandleStatusMetric, 0, 4);
}

TEST_F(AttributionDataHostManagerImplTest,
       SourceDataHost_AggregatableSourceizeCheckPerformed) {
  struct AggregatableSourceizeTestCase {
    const char* description;
    bool valid;
    size_t key_count;
    size_t key_size;

    base::flat_map<std::string, absl::uint128> GetAggregationKeys() const {
      base::flat_map<std::string, absl::uint128> keys;
      for (size_t i = 0u; i < key_count; ++i) {
        std::string key(key_size, 'A' + i);
        keys.emplace(std::move(key), absl::MakeUint128(/*high=*/i, /*low=*/i));
      }
      return keys;
    }
  };

  const AggregatableSourceizeTestCase kTestCases[] = {
      {"empty", true, 0, 0},
      {"max_keys", true,
       blink::kMaxAttributionAggregationKeysPerSourceOrTrigger, 1},
      {"too_many_keys", false,
       blink::kMaxAttributionAggregationKeysPerSourceOrTrigger + 1, 1},
      {"max_key_size", true, 1, blink::kMaxBytesPerAttributionAggregationKeyId},
      {"excessive_key_size", false, 1,
       blink::kMaxBytesPerAttributionAggregationKeyId + 1},
  };

  for (auto& test_case : kTestCases) {
    base::HistogramTester histograms;

    SCOPED_TRACE(
        test_case.description);  // Since EXPECT_CALL doesn't support <<
    EXPECT_CALL(mock_manager_, HandleSource).Times(test_case.valid);

    mojo::test::BadMessageObserver bad_message_observer;

    mojo::Remote<blink::mojom::AttributionDataHost> data_host_remote;
    data_host_manager_.RegisterDataHost(
        data_host_remote.BindNewPipeAndPassReceiver(),
        url::Origin::Create(GURL("https://page.example")));

    auto source_data = blink::mojom::AttributionSourceData::New();
    source_data->destination =
        url::Origin::Create(GURL("https://trigger.example"));
    source_data->reporting_origin =
        url::Origin::Create(GURL("https://reporter.example"));
    source_data->filter_data = blink::mojom::AttributionFilterData::New();
    source_data->aggregation_keys = test_case.GetAggregationKeys();
    data_host_remote->SourceDataAvailable(std::move(source_data));
    data_host_remote.FlushForTesting();

    Mock::VerifyAndClear(&mock_manager_);

    EXPECT_NE(bad_message_observer.got_bad_message(), test_case.valid);

    if (!test_case.valid) {
      EXPECT_EQ(bad_message_observer.WaitForBadMessage(),
                "AttributionDataHost: Invalid aggregatable source.");
    }

    // kSuccess = 0, kInvalidData = 3.
    histograms.ExpectUniqueSample(kSourceDataHandleStatusMetric,
                                  test_case.valid ? 0 : 3, 1);
  }
}

TEST_F(AttributionDataHostManagerImplTest, TriggerDataHost_TriggerRegistered) {
  base::HistogramTester histograms;

  auto destination_origin =
      url::Origin::Create(GURL("https://trigger.example"));
  auto reporting_origin = url::Origin::Create(GURL("https://reporter.example"));
  EXPECT_CALL(
      mock_manager_,
      HandleTrigger(AttributionTriggerMatches(AttributionTriggerMatcherConfig(
          destination_origin, reporting_origin,
          *AttributionFilterData::FromTriggerFilterValues({
              {"a", {"b"}},
          }),
          Optional(789),
          ElementsAre(EventTriggerDataMatches(EventTriggerDataMatcherConfig(
                          1, 2, Optional(3),
                          *AttributionFilterData::FromTriggerFilterValues({
                              {"c", {"d"}},
                          }),
                          *AttributionFilterData::FromTriggerFilterValues({
                              {"e", {"f"}},
                          }))),
                      EventTriggerDataMatches(EventTriggerDataMatcherConfig(
                          4, 5, Eq(absl::nullopt), AttributionFilterData(),
                          AttributionFilterData())))))));

  {
    RemoteDataHost data_host_remote{.task_environment = task_environment_};
    data_host_manager_.RegisterDataHost(
        data_host_remote.data_host.BindNewPipeAndPassReceiver(),
        destination_origin);

    auto trigger_data = blink::mojom::AttributionTriggerData::New();
    trigger_data->reporting_origin = reporting_origin;
    trigger_data->debug_key = blink::mojom::AttributionDebugKey::New(789);

    trigger_data->filters = blink::mojom::AttributionFilterData::New(
        AttributionFilterData::FilterValues({{"a", {"b"}}}));
    trigger_data->not_filters = blink::mojom::AttributionFilterData::New();

    trigger_data->event_triggers.push_back(blink::mojom::EventTriggerData::New(
        /*data=*/1,
        /*priority=*/2, blink::mojom::AttributionTriggerDedupKey::New(3),
        /*filters=*/
        blink::mojom::AttributionFilterData::New(
            AttributionFilterData::FilterValues({{"c", {"d"}}})),
        /*not_filters=*/
        blink::mojom::AttributionFilterData::New(
            AttributionFilterData::FilterValues({{"e", {"f"}}}))));

    trigger_data->event_triggers.push_back(blink::mojom::EventTriggerData::New(
        /*data=*/4,
        /*priority=*/5,
        /*dedup_key=*/nullptr,
        /*filters=*/blink::mojom::AttributionFilterData::New(),
        /*not_filters=*/blink::mojom::AttributionFilterData::New()));

    data_host_remote.data_host->TriggerDataAvailable(std::move(trigger_data));
    data_host_remote.data_host.FlushForTesting();
  }

  histograms.ExpectBucketCount("Conversions.RegisteredTriggersPerDataHost", 1,
                               1);
  // kSuccess = 0.
  histograms.ExpectUniqueSample(kTriggerDataHandleStatusMetric, 0, 1);
}

TEST_F(AttributionDataHostManagerImplTest,
       TriggerDataHost_OriginTrustworthyChecksPerformed) {
  base::HistogramTester histograms;

  const char kLocalHost[] = "http://localhost";

  struct {
    const char* destination_origin;
    const char* reporting_origin;
    bool trigger_expected;
    const char* bad_message = nullptr;
  } kTestCases[] = {
      {.destination_origin = kLocalHost,
       .reporting_origin = kLocalHost,
       .trigger_expected = true},
      {.destination_origin = "http://127.0.0.1",
       .reporting_origin = "http://127.0.0.1",
       .trigger_expected = true},
      {.destination_origin = kLocalHost,
       .reporting_origin = "http://insecure.com",
       .trigger_expected = false,
       .bad_message = "AttributionDataHost: Reporting origin must be secure."},
      {.destination_origin = "https://secure.com",
       .reporting_origin = "https://secure.com",
       .trigger_expected = true},
  };

  for (const auto& test_case : kTestCases) {
    EXPECT_CALL(mock_manager_, HandleTrigger).Times(test_case.trigger_expected);

    mojo::test::BadMessageObserver bad_message_observer;

    RemoteDataHost data_host_remote{.task_environment = task_environment_};
    data_host_manager_.RegisterDataHost(
        data_host_remote.data_host.BindNewPipeAndPassReceiver(),
        url::Origin::Create(GURL(test_case.destination_origin)));

    auto trigger_data = blink::mojom::AttributionTriggerData::New();
    trigger_data->reporting_origin =
        url::Origin::Create(GURL(test_case.reporting_origin));

    trigger_data->filters = blink::mojom::AttributionFilterData::New();
    trigger_data->not_filters = blink::mojom::AttributionFilterData::New();

    data_host_remote.data_host->TriggerDataAvailable(std::move(trigger_data));
    data_host_remote.data_host.FlushForTesting();

    EXPECT_EQ(bad_message_observer.got_bad_message(), !!test_case.bad_message);

    if (test_case.bad_message) {
      EXPECT_EQ(bad_message_observer.WaitForBadMessage(),
                test_case.bad_message);
    }

    Mock::VerifyAndClear(&mock_manager_);
  }

  histograms.ExpectUniqueSample("Conversions.RegisteredTriggersPerDataHost", 1,
                                3);
  // kSuccess = 0.
  histograms.ExpectBucketCount(kTriggerDataHandleStatusMetric, 0, 3);
  // kUntrustworthyOrigin = 1.
  histograms.ExpectBucketCount(kTriggerDataHandleStatusMetric, 1, 1);
}

TEST_F(AttributionDataHostManagerImplTest,
       TriggerDataHost_TopLevelFilterSizeCheckPerformed) {
  for (const auto& test_case : kAttributionFilterSizeTestCases) {
    base::HistogramTester histograms;

    SCOPED_TRACE(test_case.description);  // EXPECT_CALL doesn't support <<
    EXPECT_CALL(mock_manager_, HandleTrigger).Times(test_case.valid);

    mojo::test::BadMessageObserver bad_message_observer;

    mojo::Remote<blink::mojom::AttributionDataHost> data_host_remote;
    data_host_manager_.RegisterDataHost(
        data_host_remote.BindNewPipeAndPassReceiver(),
        url::Origin::Create(GURL("https://trigger.example")));

    auto trigger_data = blink::mojom::AttributionTriggerData::New();
    trigger_data->reporting_origin =
        url::Origin::Create(GURL("https://reporter.example"));

    trigger_data->filters =
        blink::mojom::AttributionFilterData::New(test_case.AsMap());
    trigger_data->not_filters = blink::mojom::AttributionFilterData::New();

    data_host_remote->TriggerDataAvailable(std::move(trigger_data));
    data_host_remote.FlushForTesting();

    Mock::VerifyAndClear(&mock_manager_);

    EXPECT_NE(bad_message_observer.got_bad_message(), test_case.valid);

    if (!test_case.valid) {
      EXPECT_EQ(bad_message_observer.WaitForBadMessage(),
                "AttributionDataHost: Invalid top-level filters.");
    }

    // kSuccess = 0, kInvalidData = 3.
    histograms.ExpectUniqueSample(kTriggerDataHandleStatusMetric,
                                  test_case.valid ? 0 : 3, 1);
  }
}

TEST_F(AttributionDataHostManagerImplTest,
       TriggerDataHost_EventTriggerDataFilterSizeCheckPerformed) {
  for (const auto& test_case : kAttributionFilterSizeTestCases) {
    base::HistogramTester histograms;

    SCOPED_TRACE(test_case.description);  // EXPECT_CALL doesn't support <<
    EXPECT_CALL(mock_manager_, HandleTrigger).Times(test_case.valid);

    mojo::test::BadMessageObserver bad_message_observer;

    mojo::Remote<blink::mojom::AttributionDataHost> data_host_remote;
    data_host_manager_.RegisterDataHost(
        data_host_remote.BindNewPipeAndPassReceiver(),
        url::Origin::Create(GURL("https://trigger.example")));

    auto trigger_data = blink::mojom::AttributionTriggerData::New();
    trigger_data->reporting_origin =
        url::Origin::Create(GURL("https://reporter.example"));

    trigger_data->filters = blink::mojom::AttributionFilterData::New();
    trigger_data->not_filters = blink::mojom::AttributionFilterData::New();

    trigger_data->event_triggers.push_back(blink::mojom::EventTriggerData::New(
        /*data=*/0,
        /*priority=*/0,
        /*dedup_key=*/nullptr,
        /*filters=*/blink::mojom::AttributionFilterData::New(test_case.AsMap()),
        /*not_filters=*/blink::mojom::AttributionFilterData::New()));

    data_host_remote->TriggerDataAvailable(std::move(trigger_data));
    data_host_remote.FlushForTesting();

    Mock::VerifyAndClear(&mock_manager_);

    EXPECT_NE(bad_message_observer.got_bad_message(), test_case.valid);

    if (!test_case.valid) {
      EXPECT_EQ(bad_message_observer.WaitForBadMessage(),
                "AttributionDataHost: Invalid event-trigger filters.");
    }

    // kSuccess = 0, kInvalidData = 3.
    histograms.ExpectUniqueSample(kTriggerDataHandleStatusMetric,
                                  test_case.valid ? 0 : 3, 1);
  }
}

TEST_F(AttributionDataHostManagerImplTest,
       TriggerDataHost_EventTriggerDataNotFilterSizeCheckPerformed) {
  for (const auto& test_case : kAttributionFilterSizeTestCases) {
    base::HistogramTester histograms;

    SCOPED_TRACE(test_case.description);  // EXPECT_CALL doesn't support <<
    EXPECT_CALL(mock_manager_, HandleTrigger).Times(test_case.valid);

    mojo::test::BadMessageObserver bad_message_observer;

    mojo::Remote<blink::mojom::AttributionDataHost> data_host_remote;
    data_host_manager_.RegisterDataHost(
        data_host_remote.BindNewPipeAndPassReceiver(),
        url::Origin::Create(GURL("https://trigger.example")));

    auto trigger_data = blink::mojom::AttributionTriggerData::New();
    trigger_data->reporting_origin =
        url::Origin::Create(GURL("https://reporter.example"));

    trigger_data->filters = blink::mojom::AttributionFilterData::New();
    trigger_data->not_filters = blink::mojom::AttributionFilterData::New();

    trigger_data->event_triggers.push_back(blink::mojom::EventTriggerData::New(
        /*data=*/0,
        /*priority=*/0,
        /*dedup_key=*/nullptr,
        /*filters=*/blink::mojom::AttributionFilterData::New(),
        /*not_filters=*/
        blink::mojom::AttributionFilterData::New(test_case.AsMap())));

    data_host_remote->TriggerDataAvailable(std::move(trigger_data));
    data_host_remote.FlushForTesting();

    Mock::VerifyAndClear(&mock_manager_);

    EXPECT_NE(bad_message_observer.got_bad_message(), test_case.valid);

    if (!test_case.valid) {
      EXPECT_EQ(bad_message_observer.WaitForBadMessage(),
                "AttributionDataHost: Invalid event-trigger not_filters.");
    }

    // kSuccess = 0, kInvalidData = 3.
    histograms.ExpectUniqueSample(kTriggerDataHandleStatusMetric,
                                  test_case.valid ? 0 : 3, 1);
  }
}

TEST_F(AttributionDataHostManagerImplTest,
       TriggerDataHost_EventTriggerDataSizeCheckPerformed) {
  const struct {
    size_t size;
    bool expected;
  } kTestCases[] = {
      {blink::kMaxAttributionEventTriggerData, true},
      {blink::kMaxAttributionEventTriggerData + 1, false},
  };

  for (const auto& test_case : kTestCases) {
    base::HistogramTester histograms;

    EXPECT_CALL(mock_manager_, HandleTrigger).Times(test_case.expected);

    mojo::test::BadMessageObserver bad_message_observer;

    mojo::Remote<blink::mojom::AttributionDataHost> data_host_remote;
    data_host_manager_.RegisterDataHost(
        data_host_remote.BindNewPipeAndPassReceiver(),
        url::Origin::Create(GURL("https://trigger.example")));

    auto trigger_data = blink::mojom::AttributionTriggerData::New();
    trigger_data->reporting_origin =
        url::Origin::Create(GURL("https://reporter.example"));

    for (size_t i = 0; i < test_case.size; ++i) {
      trigger_data->event_triggers.push_back(
          blink::mojom::EventTriggerData::New(
              /*data=*/0,
              /*priority=*/0,
              /*dedup_key=*/nullptr,
              /*filters=*/blink::mojom::AttributionFilterData::New(),
              /*not_filters=*/blink::mojom::AttributionFilterData::New()));
    }

    trigger_data->filters = blink::mojom::AttributionFilterData::New();
    trigger_data->not_filters = blink::mojom::AttributionFilterData::New();

    data_host_remote->TriggerDataAvailable(std::move(trigger_data));
    data_host_remote.FlushForTesting();

    Mock::VerifyAndClear(&mock_manager_);

    EXPECT_NE(bad_message_observer.got_bad_message(), test_case.expected);

    if (!test_case.expected) {
      EXPECT_EQ(bad_message_observer.WaitForBadMessage(),
                "AttributionDataHost: Too many event triggers.");
    }

    // kSuccess = 0, kInvalidData = 3.
    histograms.ExpectUniqueSample(kTriggerDataHandleStatusMetric,
                                  test_case.expected ? 0 : 3, 1);
  }
}

TEST_F(AttributionDataHostManagerImplTest,
       TriggerDataHost_AggregatableTriggerDataCheckPerformed) {
  const struct {
    size_t size;
    bool expected;
  } kTestCases[] = {
      {blink::kMaxAttributionAggregatableTriggerDataPerTrigger, true},
      {blink::kMaxAttributionAggregatableTriggerDataPerTrigger + 1, false},
  };

  for (const auto& test_case : kTestCases) {
    base::HistogramTester histograms;

    EXPECT_CALL(mock_manager_, HandleTrigger).Times(test_case.expected);

    mojo::Remote<blink::mojom::AttributionDataHost> data_host_remote;
    data_host_manager_.RegisterDataHost(
        data_host_remote.BindNewPipeAndPassReceiver(),
        url::Origin::Create(GURL("https://trigger.example")));

    mojo::test::BadMessageObserver bad_message_observer;

    auto trigger_data = blink::mojom::AttributionTriggerData::New();
    trigger_data->reporting_origin =
        url::Origin::Create(GURL("https://reporter.example"));

    trigger_data->filters = blink::mojom::AttributionFilterData::New();
    trigger_data->not_filters = blink::mojom::AttributionFilterData::New();

    for (size_t i = 0; i < test_case.size; ++i) {
      trigger_data->aggregatable_trigger_data.push_back(
          blink::mojom::AttributionAggregatableTriggerData::New(
              /*key_piece=*/345, /*source_keys=*/std::vector<std::string>{"a"},
              /*filters=*/blink::mojom::AttributionFilterData::New(),
              /*not_filters=*/blink::mojom::AttributionFilterData::New()));
    }

    data_host_remote->TriggerDataAvailable(std::move(trigger_data));
    data_host_remote.FlushForTesting();

    Mock::VerifyAndClear(&mock_manager_);

    EXPECT_NE(bad_message_observer.got_bad_message(), test_case.expected);

    if (!test_case.expected) {
      EXPECT_EQ(bad_message_observer.WaitForBadMessage(),
                "AttributionDataHost: Invalid aggregatable trigger data.");
    }

    // kSuccess = 0, kInvalidData = 3.
    histograms.ExpectUniqueSample(kTriggerDataHandleStatusMetric,
                                  test_case.expected ? 0 : 3, 1);
  }
}

TEST_F(AttributionDataHostManagerImplTest,
       TriggerDataHost_AggregatableValuesSizeCheckPerformed) {
  const struct {
    size_t size;
    bool expected;
  } kTestCases[] = {
      {blink::kMaxAttributionAggregationKeysPerSourceOrTrigger, true},
      {blink::kMaxAttributionAggregationKeysPerSourceOrTrigger + 1, false},
  };

  for (const auto& test_case : kTestCases) {
    base::HistogramTester histograms;

    EXPECT_CALL(mock_manager_, HandleTrigger).Times(test_case.expected);

    mojo::Remote<blink::mojom::AttributionDataHost> data_host_remote;
    data_host_manager_.RegisterDataHost(
        data_host_remote.BindNewPipeAndPassReceiver(),
        url::Origin::Create(GURL("https://trigger.example")));

    mojo::test::BadMessageObserver bad_message_observer;

    auto trigger_data = blink::mojom::AttributionTriggerData::New();
    trigger_data->reporting_origin =
        url::Origin::Create(GURL("https://reporter.example"));

    trigger_data->filters = blink::mojom::AttributionFilterData::New();
    trigger_data->not_filters = blink::mojom::AttributionFilterData::New();

    for (size_t i = 0; i < test_case.size; ++i) {
      trigger_data->aggregatable_values.emplace(base::NumberToString(i), 1);
    }

    data_host_remote->TriggerDataAvailable(std::move(trigger_data));
    data_host_remote.FlushForTesting();

    Mock::VerifyAndClear(&mock_manager_);

    EXPECT_NE(bad_message_observer.got_bad_message(), test_case.expected);

    if (!test_case.expected) {
      EXPECT_EQ(bad_message_observer.WaitForBadMessage(),
                "AttributionDataHost: Invalid aggregatable values.");
    }

    // kSuccess = 0, kInvalidData = 3.
    histograms.ExpectUniqueSample(kTriggerDataHandleStatusMetric,
                                  test_case.expected ? 0 : 3, 1);
  }
}

TEST_F(AttributionDataHostManagerImplTest,
       TriggerDataHost_ReceiverModeCheckPerformed) {
  base::HistogramTester histograms;

  Checkpoint checkpoint;
  {
    InSequence seq;

    EXPECT_CALL(mock_manager_, HandleTrigger);
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(mock_manager_, HandleTrigger);
    EXPECT_CALL(checkpoint, Call(2));
    EXPECT_CALL(mock_manager_, HandleSource).Times(0);
    EXPECT_CALL(checkpoint, Call(3));
    EXPECT_CALL(mock_manager_, HandleTrigger);
  }

  auto destination_origin =
      url::Origin::Create(GURL("https://trigger.example"));
  auto reporting_origin = url::Origin::Create(GURL("https://reporter.example"));

  {
    RemoteDataHost data_host_remote{.task_environment = task_environment_};
    data_host_manager_.RegisterDataHost(
        data_host_remote.data_host.BindNewPipeAndPassReceiver(),
        destination_origin);

    auto trigger_data = blink::mojom::AttributionTriggerData::New();
    trigger_data->reporting_origin = reporting_origin;
    trigger_data->filters = blink::mojom::AttributionFilterData::New();
    trigger_data->not_filters = blink::mojom::AttributionFilterData::New();

    data_host_remote.data_host->TriggerDataAvailable(trigger_data.Clone());
    data_host_remote.data_host.FlushForTesting();

    checkpoint.Call(1);

    data_host_remote.data_host->TriggerDataAvailable(trigger_data.Clone());
    data_host_remote.data_host.FlushForTesting();

    checkpoint.Call(2);

    {
      mojo::test::BadMessageObserver bad_message_observer;

      auto source_data = blink::mojom::AttributionSourceData::New();
      source_data->destination = destination_origin;
      source_data->reporting_origin = reporting_origin;
      source_data->filter_data = blink::mojom::AttributionFilterData::New();

      data_host_remote.data_host->SourceDataAvailable(std::move(source_data));
      data_host_remote.data_host.FlushForTesting();

      EXPECT_EQ(bad_message_observer.WaitForBadMessage(),
                "AttributionDataHost: Cannot register sources after "
                "registering a trigger.");
    }

    checkpoint.Call(3);

    data_host_remote.data_host->TriggerDataAvailable(std::move(trigger_data));
    data_host_remote.data_host.FlushForTesting();
  }

  histograms.ExpectTotalCount("Conversions.RegisteredSourcesPerDataHost", 0);
  histograms.ExpectUniqueSample("Conversions.RegisteredTriggersPerDataHost", 3,
                                1);
  // kSuccess = 0.
  histograms.ExpectUniqueSample(kTriggerDataHandleStatusMetric, 0, 3);
  // kContextError = 2.
  histograms.ExpectUniqueSample(kSourceDataHandleStatusMetric, 2, 1);
}

TEST_F(AttributionDataHostManagerImplTest,
       SourceDataHost_ReceiverModeCheckPerformed) {
  base::HistogramTester histograms;

  Checkpoint checkpoint;
  {
    InSequence seq;

    EXPECT_CALL(mock_manager_, HandleSource);
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(mock_manager_, HandleSource);
    EXPECT_CALL(checkpoint, Call(2));
    EXPECT_CALL(mock_manager_, HandleTrigger).Times(0);
    EXPECT_CALL(checkpoint, Call(3));
    EXPECT_CALL(mock_manager_, HandleSource);
  }

  auto page_origin = url::Origin::Create(GURL("https://page.example"));
  auto destination_origin =
      url::Origin::Create(GURL("https://trigger.example"));
  auto reporting_origin = url::Origin::Create(GURL("https://reporter.example"));

  {
    RemoteDataHost data_host_remote{.task_environment = task_environment_};
    data_host_manager_.RegisterDataHost(
        data_host_remote.data_host.BindNewPipeAndPassReceiver(), page_origin);

    auto source_data = blink::mojom::AttributionSourceData::New();
    source_data->destination = destination_origin;
    source_data->reporting_origin = reporting_origin;
    source_data->filter_data = blink::mojom::AttributionFilterData::New();

    data_host_remote.data_host->SourceDataAvailable(source_data.Clone());
    data_host_remote.data_host.FlushForTesting();

    checkpoint.Call(1);

    data_host_remote.data_host->SourceDataAvailable(source_data.Clone());
    data_host_remote.data_host.FlushForTesting();

    checkpoint.Call(2);

    {
      mojo::test::BadMessageObserver bad_message_observer;

      auto trigger_data = blink::mojom::AttributionTriggerData::New();
      trigger_data->reporting_origin = reporting_origin;
      trigger_data->filters = blink::mojom::AttributionFilterData::New();
      trigger_data->not_filters = blink::mojom::AttributionFilterData::New();

      data_host_remote.data_host->TriggerDataAvailable(std::move(trigger_data));
      data_host_remote.data_host.FlushForTesting();

      EXPECT_EQ(bad_message_observer.WaitForBadMessage(),
                "AttributionDataHost: Cannot register triggers after "
                "registering a source.");
    }

    checkpoint.Call(3);

    data_host_remote.data_host->SourceDataAvailable(std::move(source_data));
    data_host_remote.data_host.FlushForTesting();
  }

  histograms.ExpectUniqueSample("Conversions.RegisteredSourcesPerDataHost", 3,
                                1);
  histograms.ExpectTotalCount("Conversions.RegisteredTriggersPerDataHost", 0);
  // kSuccess = 0.
  histograms.ExpectUniqueSample(kSourceDataHandleStatusMetric, 0, 3);
  // kContextError = 2.
  histograms.ExpectUniqueSample(kTriggerDataHandleStatusMetric, 2, 1);
}

TEST_F(AttributionDataHostManagerImplTest,
       SourceDataHost_NavigationSourceRegistered) {
  base::HistogramTester histograms;

  const auto page_origin = url::Origin::Create(GURL("https://page.example"));
  const auto destination_origin =
      url::Origin::Create(GURL("https://trigger.example"));
  const auto reporting_origin =
      url::Origin::Create(GURL("https://reporter.example"));

  Checkpoint checkpoint;
  {
    InSequence seq;

    EXPECT_CALL(
        mock_manager_,
        HandleSource(AllOf(
            SourceTypeIs(AttributionSourceType::kNavigation),
            SourceEventIdIs(10), DestinationOriginIs(destination_origin),
            ImpressionOriginIs(page_origin), SourcePriorityIs(20),
            SourceDebugKeyIs(789),
            AggregationKeysAre(*AttributionAggregationKeys::FromKeys(
                {{"key", absl::MakeUint128(/*high=*/5, /*low=*/345)}})))));
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(mock_manager_, HandleSource).Times(0);
  }

  const blink::AttributionSrcToken attribution_src_token;

  {
    RemoteDataHost data_host_remote{.task_environment = task_environment_};
    data_host_manager_.RegisterNavigationDataHost(
        data_host_remote.data_host.BindNewPipeAndPassReceiver(),
        attribution_src_token);

    task_environment_.FastForwardBy(base::Milliseconds(1));

    data_host_manager_.NotifyNavigationForDataHost(
        attribution_src_token, page_origin, destination_origin);

    auto source_data = blink::mojom::AttributionSourceData::New();
    source_data->source_event_id = 10;
    source_data->destination = destination_origin;
    source_data->reporting_origin = reporting_origin;
    source_data->priority = 20;
    source_data->debug_key = blink::mojom::AttributionDebugKey::New(789);
    source_data->filter_data = blink::mojom::AttributionFilterData::New();
    source_data->aggregation_keys = {
        {"key", absl::MakeUint128(/*high=*/5, /*low=*/345)},
    };
    data_host_remote.data_host->SourceDataAvailable(source_data.Clone());
    data_host_remote.data_host.FlushForTesting();

    checkpoint.Call(1);

    // This should fail because the destination site doesn't match the final
    // navigation site.
    source_data->destination =
        url::Origin::Create(GURL("https://trigger2.example"));
    data_host_remote.data_host->SourceDataAvailable(std::move(source_data));
    data_host_remote.data_host.FlushForTesting();
  }

  histograms.ExpectTimeBucketCount("Conversions.SourceEligibleDataHostLifeTime",
                                   base::Milliseconds(1), 1);

  // kRegistered = 0, kProcessed = 3.
  histograms.ExpectBucketCount("Conversions.NavigationDataHostStatus", 0, 1);
  histograms.ExpectBucketCount("Conversions.NavigationDataHostStatus", 3, 1);

  // kSuccess = 0, kContextError = 2
  histograms.ExpectBucketCount(kSourceDataHandleStatusMetric, 0, 1);
  histograms.ExpectBucketCount(kSourceDataHandleStatusMetric, 2, 1);
}

// Ensures correct behavior in
// `AttributionDataHostManagerImpl::OnDataHostDisconnected()` when a data host
// is registered but disconnects before registering a source or trigger.
TEST_F(AttributionDataHostManagerImplTest, NoSourceOrTrigger) {
  base::HistogramTester histograms;

  auto page_origin = url::Origin::Create(GURL("https://page.example"));

  {
    RemoteDataHost data_host_remote{.task_environment = task_environment_};
    data_host_manager_.RegisterDataHost(
        data_host_remote.data_host.BindNewPipeAndPassReceiver(), page_origin);
  }

  histograms.ExpectTotalCount("Conversions.RegisteredSourcesPerDataHost", 0);
  histograms.ExpectTotalCount("Conversions.RegisteredTriggersPerDataHost", 0);
}

TEST_F(AttributionDataHostManagerImplTest,
       SourceModeReceiverConnected_TriggerDelayed) {
  base::HistogramTester histograms;

  Checkpoint checkpoint;
  {
    InSequence seq;

    EXPECT_CALL(mock_manager_, HandleTrigger).Times(0);
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(mock_manager_, HandleTrigger);
  }

  {
    RemoteDataHost source_data_host_remote{.task_environment =
                                               task_environment_};
    data_host_manager_.RegisterDataHost(
        source_data_host_remote.data_host.BindNewPipeAndPassReceiver(),
        url::Origin::Create(GURL("https://page1.example")));

    mojo::Remote<blink::mojom::AttributionDataHost> trigger_data_host_remote;
    data_host_manager_.RegisterDataHost(
        trigger_data_host_remote.BindNewPipeAndPassReceiver(),
        url::Origin::Create(GURL("https://page2.example")));

    task_environment_.FastForwardBy(base::Milliseconds(1));

    // Because there is a connected data host in source mode, this trigger
    // should be delayed.
    auto trigger_data = blink::mojom::AttributionTriggerData::New();
    trigger_data->reporting_origin =
        url::Origin::Create(GURL("https://report.test"));
    trigger_data->filters = blink::mojom::AttributionFilterData::New();
    trigger_data->not_filters = blink::mojom::AttributionFilterData::New();
    trigger_data_host_remote->TriggerDataAvailable(std::move(trigger_data));
    trigger_data_host_remote.FlushForTesting();

    task_environment_.FastForwardBy(base::Seconds(5) - base::Microseconds(1));
    checkpoint.Call(1);
    task_environment_.FastForwardBy(base::Microseconds(1));
  }

  CheckTriggerQueueHistograms(histograms, {
                                              .enqueued = 1,
                                              .processed_with_delay = 1,
                                              .delays =
                                                  {
                                                      {base::Seconds(5), 1},
                                                  },
                                          });

  // Recorded when source data host was disconnected.
  histograms.ExpectTimeBucketCount("Conversions.SourceEligibleDataHostLifeTime",
                                   base::Seconds(5), 1);
  // Recorded when trigger data was available.
  histograms.ExpectTimeBucketCount("Conversions.SourceEligibleDataHostLifeTime",
                                   base::Milliseconds(1), 1);
}

TEST_F(AttributionDataHostManagerImplTest,
       NavigationSourceReceiverConnected_TriggerDelayed) {
  base::HistogramTester histograms;

  Checkpoint checkpoint;
  {
    InSequence seq;

    EXPECT_CALL(mock_manager_, HandleTrigger).Times(0);
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(mock_manager_, HandleTrigger);
  }

  const blink::AttributionSrcToken attribution_src_token;
  mojo::Remote<blink::mojom::AttributionDataHost> source_data_host_remote;
  data_host_manager_.RegisterNavigationDataHost(
      source_data_host_remote.BindNewPipeAndPassReceiver(),
      attribution_src_token);

  mojo::Remote<blink::mojom::AttributionDataHost> trigger_data_host_remote;
  data_host_manager_.RegisterDataHost(
      trigger_data_host_remote.BindNewPipeAndPassReceiver(),
      url::Origin::Create(GURL("https://page2.example")));

  // Because there is a connected data host in source mode, this trigger should
  // be delayed.
  auto trigger_data = blink::mojom::AttributionTriggerData::New();
  trigger_data->reporting_origin =
      url::Origin::Create(GURL("https://report.test"));
  trigger_data->filters = blink::mojom::AttributionFilterData::New();
  trigger_data->not_filters = blink::mojom::AttributionFilterData::New();
  trigger_data_host_remote->TriggerDataAvailable(std::move(trigger_data));
  trigger_data_host_remote.FlushForTesting();

  task_environment_.FastForwardBy(base::Seconds(5) - base::Microseconds(1));
  checkpoint.Call(1);
  task_environment_.FastForwardBy(base::Microseconds(1));

  CheckTriggerQueueHistograms(histograms, {
                                              .enqueued = 1,
                                              .processed_with_delay = 1,
                                              .delays =
                                                  {
                                                      {base::Seconds(5), 1},
                                                  },
                                          });
}

TEST_F(AttributionDataHostManagerImplTest,
       NavigationRedirectSource_SourceSiteTrustworthyChecksPerformed) {
  EXPECT_CALL(mock_manager_, HandleSource).Times(0);

  auto reporter = url::Origin::Create(GURL("https://report.test"));
  auto source_site = url::Origin::Create(GURL("http://source.test"));

  const blink::AttributionSrcToken attribution_src_token;
  data_host_manager_.NotifyNavigationRedirectRegistation(
      attribution_src_token, kRegisterSourceJson, reporter, source_site);
  // Wait for parsing to finish.
  task_environment_.FastForwardBy(base::TimeDelta());

  data_host_manager_.NotifyNavigationForDataHost(
      attribution_src_token, source_site,
      url::Origin::Create(GURL("https://destination.example")));
}

TEST_F(AttributionDataHostManagerImplTest,
       NavigationRedirectSource_ReportingTrustworthyChecksPerformed) {
  EXPECT_CALL(mock_manager_, HandleSource).Times(0);

  auto reporter = url::Origin::Create(GURL("http://report.test"));
  auto source_site = url::Origin::Create(GURL("https://source.test"));

  const blink::AttributionSrcToken attribution_src_token;
  data_host_manager_.NotifyNavigationRedirectRegistation(
      attribution_src_token, kRegisterSourceJson, reporter, source_site);
  // Wait for parsing to finish.
  task_environment_.FastForwardBy(base::TimeDelta());

  data_host_manager_.NotifyNavigationForDataHost(
      attribution_src_token, source_site,
      url::Origin::Create(GURL("https://destination.example")));
}

TEST_F(AttributionDataHostManagerImplTest,
       NavigationRedirectSource_RegistrationWithDestinationMismatchIgnored) {
  EXPECT_CALL(mock_manager_, HandleSource).Times(0);

  auto reporter = url::Origin::Create(GURL("https://report.test"));
  auto source_site = url::Origin::Create(GURL("https://source.test"));

  const blink::AttributionSrcToken attribution_src_token;
  data_host_manager_.NotifyNavigationRedirectRegistation(
      attribution_src_token, kRegisterSourceJson, reporter, source_site);
  // Wait for parsing to finish.
  task_environment_.FastForwardBy(base::TimeDelta());

  data_host_manager_.NotifyNavigationForDataHost(
      attribution_src_token, source_site,
      url::Origin::Create(GURL("https://destination2.example")));
}

TEST_F(AttributionDataHostManagerImplTest,
       NavigationRedirectSource_DestinationMismatchIgnoredAfterFinished) {
  EXPECT_CALL(mock_manager_, HandleSource).Times(0);

  auto reporter = url::Origin::Create(GURL("https://report.test"));
  auto source_site = url::Origin::Create(GURL("https://source.test"));

  const blink::AttributionSrcToken attribution_src_token;
  data_host_manager_.NotifyNavigationRedirectRegistation(
      attribution_src_token, kRegisterSourceJson, reporter, source_site);

  data_host_manager_.NotifyNavigationForDataHost(
      attribution_src_token, source_site,
      url::Origin::Create(GURL("https://destination2.example")));

  // Wait for parsing to finish. Note that this relies on the DataDecoder
  // callback not being invoked in the same callstack as the call to
  // `NotifyNavigationRedirectRegistation()` above. If flakes result, perhaps
  // due to a change in the DataDecoder implementation, consider replacing this
  // with a mock whose callback sequencing we can explicitly control.
  task_environment_.FastForwardBy(base::TimeDelta());
}

TEST_F(AttributionDataHostManagerImplTest,
       NavigationRedirectSource_NavigationFailed) {
  EXPECT_CALL(mock_manager_, HandleSource).Times(0);

  auto reporter = url::Origin::Create(GURL("https://report.test"));
  auto source_site = url::Origin::Create(GURL("https://source.test"));

  const blink::AttributionSrcToken attribution_src_token;
  data_host_manager_.NotifyNavigationRedirectRegistation(
      attribution_src_token, kRegisterSourceJson, reporter, source_site);
  // Wait for parsing to finish.
  task_environment_.FastForwardBy(base::TimeDelta());

  data_host_manager_.NotifyNavigationFailure(attribution_src_token);

  data_host_manager_.NotifyNavigationForDataHost(
      attribution_src_token, source_site,
      url::Origin::Create(GURL("https://destination.example")));
}

TEST_F(AttributionDataHostManagerImplTest,
       NavigationRedirectSource_NavigationFailedBeforeParsing) {
  EXPECT_CALL(mock_manager_, HandleSource).Times(0);

  auto reporter = url::Origin::Create(GURL("https://report.test"));
  auto source_site = url::Origin::Create(GURL("https://source.test"));

  const blink::AttributionSrcToken attribution_src_token;
  data_host_manager_.NotifyNavigationRedirectRegistation(
      attribution_src_token, kRegisterSourceJson, reporter, source_site);
  data_host_manager_.NotifyNavigationFailure(attribution_src_token);

  // Wait for parsing to finish.
  task_environment_.FastForwardBy(base::TimeDelta());
}

TEST_F(AttributionDataHostManagerImplTest,
       NavigationRedirectSource_ParsingFinishesBeforeAndAfterNav) {
  EXPECT_CALL(mock_manager_, HandleSource).Times(2);

  auto reporter = url::Origin::Create(GURL("https://report.test"));
  auto source_site = url::Origin::Create(GURL("https://source.test"));

  const blink::AttributionSrcToken attribution_src_token;
  data_host_manager_.NotifyNavigationRedirectRegistation(
      attribution_src_token, kRegisterSourceJson, reporter, source_site);
  // Wait for parsing to finish.
  task_environment_.FastForwardBy(base::TimeDelta());

  data_host_manager_.NotifyNavigationRedirectRegistation(
      attribution_src_token, kRegisterSourceJson, reporter, source_site);

  data_host_manager_.NotifyNavigationForDataHost(
      attribution_src_token, source_site,
      url::Origin::Create(GURL("https://destination.example")));
  // Wait for parsing to finish.
  task_environment_.FastForwardBy(base::TimeDelta());
}

TEST_F(AttributionDataHostManagerImplTest,
       NavigationRedirectSource_ParsingFailsBeforeAndSucceedsAfterNav) {
  EXPECT_CALL(mock_manager_, HandleSource).Times(1);

  auto reporter = url::Origin::Create(GURL("https://report.test"));
  auto source_site = url::Origin::Create(GURL("https://source.test"));

  const blink::AttributionSrcToken attribution_src_token;
  data_host_manager_.NotifyNavigationRedirectRegistation(
      attribution_src_token, "!!!invalid json", reporter, source_site);
  // Wait for parsing to finish.
  task_environment_.FastForwardBy(base::TimeDelta());

  data_host_manager_.NotifyNavigationRedirectRegistation(
      attribution_src_token, kRegisterSourceJson, reporter, source_site);

  data_host_manager_.NotifyNavigationForDataHost(
      attribution_src_token, source_site,
      url::Origin::Create(GURL("https://destination.example")));
  // Wait for parsing to finish.
  task_environment_.FastForwardBy(base::TimeDelta());
}

TEST_F(AttributionDataHostManagerImplTest,
       NavigationRedirectSource_TriggerDelayed) {
  base::HistogramTester histograms;

  Checkpoint checkpoint;
  {
    InSequence seq;

    EXPECT_CALL(mock_manager_, HandleTrigger).Times(0);
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(mock_manager_, HandleTrigger);
  }

  auto reporter = url::Origin::Create(GURL("https://report.test"));
  auto source_site = url::Origin::Create(GURL("https://source.test"));

  const blink::AttributionSrcToken attribution_src_token;
  data_host_manager_.NotifyNavigationRedirectRegistation(
      attribution_src_token, kRegisterSourceJson, reporter, source_site);

  mojo::Remote<blink::mojom::AttributionDataHost> trigger_data_host_remote;
  data_host_manager_.RegisterDataHost(
      trigger_data_host_remote.BindNewPipeAndPassReceiver(),
      url::Origin::Create(GURL("https://page2.example")));

  // Because there is a connected data host in source mode, this trigger should
  // be delayed.
  auto trigger_data = blink::mojom::AttributionTriggerData::New();
  trigger_data->reporting_origin = reporter;
  trigger_data->filters = blink::mojom::AttributionFilterData::New();
  trigger_data->not_filters = blink::mojom::AttributionFilterData::New();
  trigger_data_host_remote->TriggerDataAvailable(std::move(trigger_data));
  trigger_data_host_remote.FlushForTesting();

  task_environment_.FastForwardBy(base::Seconds(5) - base::Microseconds(1));
  checkpoint.Call(1);
  task_environment_.FastForwardBy(base::Microseconds(1));

  CheckTriggerQueueHistograms(histograms, {
                                              .enqueued = 1,
                                              .processed_with_delay = 1,
                                              .delays =
                                                  {
                                                      {base::Seconds(5), 1},
                                                  },
                                          });
}

TEST_F(AttributionDataHostManagerImplTest,
       NavigationRedirectSource_NavigationFinishedQueueSkipped) {
  base::HistogramTester histograms;

  Checkpoint checkpoint;
  {
    InSequence seq;

    EXPECT_CALL(mock_manager_, HandleSource).Times(2);
    EXPECT_CALL(mock_manager_, HandleTrigger).Times(0);
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(mock_manager_, HandleTrigger);
  }

  auto reporter = url::Origin::Create(GURL("https://report.test"));
  auto source_site = url::Origin::Create(GURL("https://source.test"));

  const blink::AttributionSrcToken attribution_src_token;
  data_host_manager_.NotifyNavigationRedirectRegistation(
      attribution_src_token, kRegisterSourceJson, reporter, source_site);
  data_host_manager_.NotifyNavigationRedirectRegistation(
      attribution_src_token, kRegisterSourceJson, reporter, source_site);

  // Wait for parsing.
  task_environment_.FastForwardBy(base::TimeDelta());
  data_host_manager_.NotifyNavigationForDataHost(
      attribution_src_token, source_site,
      url::Origin::Create(GURL("https://destination.example")));

  checkpoint.Call(1);

  mojo::Remote<blink::mojom::AttributionDataHost> trigger_data_host_remote;
  data_host_manager_.RegisterDataHost(
      trigger_data_host_remote.BindNewPipeAndPassReceiver(),
      url::Origin::Create(GURL("https://page2.example")));

  auto trigger_data = blink::mojom::AttributionTriggerData::New();
  trigger_data->reporting_origin =
      url::Origin::Create(GURL("https://report.test"));
  trigger_data->filters = blink::mojom::AttributionFilterData::New();
  trigger_data->not_filters = blink::mojom::AttributionFilterData::New();
  trigger_data_host_remote->TriggerDataAvailable(std::move(trigger_data));
  trigger_data_host_remote.FlushForTesting();

  CheckTriggerQueueHistograms(histograms, {.skipped_queue = 1});
}

TEST_F(AttributionDataHostManagerImplTest,
       NavigationRedirectSource_ParsingAfterNavigationFinishedQueueSkipped) {
  base::HistogramTester histograms;

  Checkpoint checkpoint;
  {
    InSequence seq;

    EXPECT_CALL(mock_manager_, HandleSource).Times(2);
    EXPECT_CALL(mock_manager_, HandleTrigger).Times(0);
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(mock_manager_, HandleTrigger);
  }

  auto reporter = url::Origin::Create(GURL("https://report.test"));
  auto source_site = url::Origin::Create(GURL("https://source.test"));

  const blink::AttributionSrcToken attribution_src_token;
  data_host_manager_.NotifyNavigationRedirectRegistation(
      attribution_src_token, kRegisterSourceJson, reporter, source_site);
  data_host_manager_.NotifyNavigationRedirectRegistation(
      attribution_src_token, kRegisterSourceJson, reporter, source_site);

  // Wait for parsing.
  data_host_manager_.NotifyNavigationForDataHost(
      attribution_src_token, source_site,
      url::Origin::Create(GURL("https://destination.example")));
  task_environment_.FastForwardBy(base::TimeDelta());
  checkpoint.Call(1);

  mojo::Remote<blink::mojom::AttributionDataHost> trigger_data_host_remote;
  data_host_manager_.RegisterDataHost(
      trigger_data_host_remote.BindNewPipeAndPassReceiver(),
      url::Origin::Create(GURL("https://page2.example")));

  auto trigger_data = blink::mojom::AttributionTriggerData::New();
  trigger_data->reporting_origin =
      url::Origin::Create(GURL("https://report.test"));
  trigger_data->filters = blink::mojom::AttributionFilterData::New();
  trigger_data->not_filters = blink::mojom::AttributionFilterData::New();
  trigger_data_host_remote->TriggerDataAvailable(std::move(trigger_data));
  trigger_data_host_remote.FlushForTesting();

  CheckTriggerQueueHistograms(histograms, {.skipped_queue = 1});
}

TEST_F(AttributionDataHostManagerImplTest,
       SourceModeReceiverConnectsDisconnects_TriggerNotDelayed) {
  base::HistogramTester histograms;

  EXPECT_CALL(mock_manager_, HandleTrigger);

  mojo::Remote<blink::mojom::AttributionDataHost> source_data_host_remote;
  data_host_manager_.RegisterDataHost(
      source_data_host_remote.BindNewPipeAndPassReceiver(),
      url::Origin::Create(GURL("https://page1.example")));

  mojo::Remote<blink::mojom::AttributionDataHost> trigger_data_host_remote;
  data_host_manager_.RegisterDataHost(
      trigger_data_host_remote.BindNewPipeAndPassReceiver(),
      url::Origin::Create(GURL("https://page2.example")));

  source_data_host_remote.reset();

  auto trigger_data = blink::mojom::AttributionTriggerData::New();
  trigger_data->reporting_origin =
      url::Origin::Create(GURL("https://report.test"));
  trigger_data->filters = blink::mojom::AttributionFilterData::New();
  trigger_data->not_filters = blink::mojom::AttributionFilterData::New();
  trigger_data_host_remote->TriggerDataAvailable(std::move(trigger_data));
  trigger_data_host_remote.FlushForTesting();

  CheckTriggerQueueHistograms(histograms, {.skipped_queue = 1});
}

TEST_F(AttributionDataHostManagerImplTest, TwoTriggerReceivers) {
  base::HistogramTester histograms;

  EXPECT_CALL(mock_manager_, HandleTrigger).Times(2);

  mojo::Remote<blink::mojom::AttributionDataHost> trigger_data_host_remote1;
  data_host_manager_.RegisterDataHost(
      trigger_data_host_remote1.BindNewPipeAndPassReceiver(),
      url::Origin::Create(GURL("https://page1.example")));

  mojo::Remote<blink::mojom::AttributionDataHost> trigger_data_host_remote2;
  data_host_manager_.RegisterDataHost(
      trigger_data_host_remote2.BindNewPipeAndPassReceiver(),
      url::Origin::Create(GURL("https://page2.example")));

  auto trigger_data = blink::mojom::AttributionTriggerData::New();
  trigger_data->reporting_origin =
      url::Origin::Create(GURL("https://report.test"));
  trigger_data->filters = blink::mojom::AttributionFilterData::New();
  trigger_data->not_filters = blink::mojom::AttributionFilterData::New();

  trigger_data_host_remote1->TriggerDataAvailable(trigger_data.Clone());
  trigger_data_host_remote2->TriggerDataAvailable(std::move(trigger_data));

  trigger_data_host_remote1.FlushForTesting();
  trigger_data_host_remote2.FlushForTesting();

  // 1. Trigger 1 is enqueued because the other data host is connected in source
  //    mode.
  // 2. Trigger 2 resets the source mode receiver count to 0, which flushes
  //    trigger 1.
  // 3. Trigger 2 skips the queue.
  CheckTriggerQueueHistograms(histograms, {
                                              .skipped_queue = 1,
                                              .enqueued = 1,
                                              .flushed = 1,
                                              .delays =
                                                  {
                                                      {base::Seconds(0), 1},
                                                  },
                                          });
}

TEST_F(AttributionDataHostManagerImplTest,
       NavigationSourceReceiverConnectsFails_TriggerNotDelayed) {
  base::HistogramTester histograms;

  EXPECT_CALL(mock_manager_, HandleTrigger);

  const blink::AttributionSrcToken attribution_src_token;
  mojo::Remote<blink::mojom::AttributionDataHost> source_data_host_remote;
  data_host_manager_.RegisterNavigationDataHost(
      source_data_host_remote.BindNewPipeAndPassReceiver(),
      attribution_src_token);

  mojo::Remote<blink::mojom::AttributionDataHost> trigger_data_host_remote;
  data_host_manager_.RegisterDataHost(
      trigger_data_host_remote.BindNewPipeAndPassReceiver(),
      url::Origin::Create(GURL("https://page2.example")));

  task_environment_.FastForwardBy(base::Milliseconds(1));

  data_host_manager_.NotifyNavigationFailure(attribution_src_token);

  auto trigger_data = blink::mojom::AttributionTriggerData::New();
  trigger_data->reporting_origin =
      url::Origin::Create(GURL("https://report.test"));
  trigger_data->filters = blink::mojom::AttributionFilterData::New();
  trigger_data->not_filters = blink::mojom::AttributionFilterData::New();
  trigger_data_host_remote->TriggerDataAvailable(std::move(trigger_data));
  trigger_data_host_remote.FlushForTesting();

  CheckTriggerQueueHistograms(histograms, {.skipped_queue = 1});

  histograms.ExpectTotalCount("Conversions.TriggerQueueDelay", 0);
  histograms.ExpectTimeBucketCount("Conversions.SourceEligibleDataHostLifeTime",
                                   base::Milliseconds(1), 2);

  // kRegistered = 0, kNavigationFailed = 2.
  histograms.ExpectBucketCount("Conversions.NavigationDataHostStatus", 0, 1);
  histograms.ExpectBucketCount("Conversions.NavigationDataHostStatus", 2, 1);
}

TEST_F(AttributionDataHostManagerImplTest,
       SourceModeReceiverConnected_DelayedTriggersHandledInOrder) {
  base::HistogramTester histograms;

  const auto reporting_origin1 =
      url::Origin::Create(GURL("https://report1.test"));
  const auto reporting_origin2 =
      url::Origin::Create(GURL("https://report2.test"));

  Checkpoint checkpoint;
  {
    InSequence seq;

    EXPECT_CALL(mock_manager_, HandleTrigger).Times(0);
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(mock_manager_,
                HandleTrigger(AttributionTriggerMatches(
                    AttributionTriggerMatcherConfig(_, reporting_origin1))));
    EXPECT_CALL(checkpoint, Call(2));
    EXPECT_CALL(mock_manager_,
                HandleTrigger(AttributionTriggerMatches(
                    AttributionTriggerMatcherConfig(_, reporting_origin2))));
  }

  mojo::Remote<blink::mojom::AttributionDataHost> source_data_host_remote;
  data_host_manager_.RegisterDataHost(
      source_data_host_remote.BindNewPipeAndPassReceiver(),
      url::Origin::Create(GURL("https://page1.example")));

  mojo::Remote<blink::mojom::AttributionDataHost> trigger_data_host_remote;
  data_host_manager_.RegisterDataHost(
      trigger_data_host_remote.BindNewPipeAndPassReceiver(),
      url::Origin::Create(GURL("https://page2.example")));

  auto send_trigger = [&](url::Origin reporting_origin) {
    auto trigger_data = blink::mojom::AttributionTriggerData::New();
    trigger_data->reporting_origin = std::move(reporting_origin);
    trigger_data->filters = blink::mojom::AttributionFilterData::New();
    trigger_data->not_filters = blink::mojom::AttributionFilterData::New();
    trigger_data_host_remote->TriggerDataAvailable(std::move(trigger_data));
  };

  send_trigger(reporting_origin1);
  task_environment_.FastForwardBy(base::Seconds(1));
  send_trigger(reporting_origin2);
  trigger_data_host_remote.FlushForTesting();

  checkpoint.Call(1);
  task_environment_.FastForwardBy(base::Seconds(4));
  checkpoint.Call(2);
  task_environment_.FastForwardBy(base::Seconds(1));

  CheckTriggerQueueHistograms(histograms, {
                                              .enqueued = 2,
                                              .processed_with_delay = 2,
                                              .delays =
                                                  {
                                                      {base::Seconds(5), 2},
                                                  },
                                          });
}

TEST_F(AttributionDataHostManagerImplTest,
       SourceModeReceiverConnectsDisconnects_DelayedTriggersFlushed) {
  base::HistogramTester histograms;

  base::RunLoop loop;
  EXPECT_CALL(mock_manager_, HandleTrigger)
      .WillOnce([&](AttributionTrigger trigger) { loop.Quit(); });

  mojo::Remote<blink::mojom::AttributionDataHost> source_data_host_remote;
  data_host_manager_.RegisterDataHost(
      source_data_host_remote.BindNewPipeAndPassReceiver(),
      url::Origin::Create(GURL("https://page1.example")));

  mojo::Remote<blink::mojom::AttributionDataHost> trigger_data_host_remote;
  data_host_manager_.RegisterDataHost(
      trigger_data_host_remote.BindNewPipeAndPassReceiver(),
      url::Origin::Create(GURL("https://page2.example")));

  auto trigger_data = blink::mojom::AttributionTriggerData::New();
  trigger_data->reporting_origin =
      url::Origin::Create(GURL("https://report.test"));
  trigger_data->filters = blink::mojom::AttributionFilterData::New();
  trigger_data->not_filters = blink::mojom::AttributionFilterData::New();
  trigger_data_host_remote->TriggerDataAvailable(std::move(trigger_data));
  trigger_data_host_remote.FlushForTesting();

  task_environment_.FastForwardBy(base::Seconds(2));
  source_data_host_remote.reset();
  loop.Run();

  CheckTriggerQueueHistograms(histograms, {
                                              .enqueued = 1,
                                              .flushed = 1,
                                              .delays =
                                                  {
                                                      {base::Seconds(2), 1},
                                                  },
                                          });
}

TEST_F(AttributionDataHostManagerImplTest,
       SourceModeReceiverConnected_ExcessiveDelayedTriggersDropped) {
  constexpr size_t kMaxDelayedTriggers = 30;

  base::HistogramTester histograms;

  base::RunLoop loop;
  auto barrier = base::BarrierClosure(kMaxDelayedTriggers, loop.QuitClosure());

  mojo::Remote<blink::mojom::AttributionDataHost> source_data_host_remote;
  data_host_manager_.RegisterDataHost(
      source_data_host_remote.BindNewPipeAndPassReceiver(),
      url::Origin::Create(GURL("https://page1.example")));

  mojo::Remote<blink::mojom::AttributionDataHost> trigger_data_host_remote;
  data_host_manager_.RegisterDataHost(
      trigger_data_host_remote.BindNewPipeAndPassReceiver(),
      url::Origin::Create(GURL("https://page2.example")));

  auto send_trigger = [&](url::Origin reporting_origin) {
    auto trigger_data = blink::mojom::AttributionTriggerData::New();
    trigger_data->reporting_origin = std::move(reporting_origin);
    trigger_data->filters = blink::mojom::AttributionFilterData::New();
    trigger_data->not_filters = blink::mojom::AttributionFilterData::New();
    trigger_data_host_remote->TriggerDataAvailable(std::move(trigger_data));
  };

  for (size_t i = 0; i < kMaxDelayedTriggers; i++) {
    url::Origin reporting_origin = url::Origin::Create(GURL(
        base::StrCat({"https://report", base::NumberToString(i), ".test"})));

    EXPECT_CALL(mock_manager_,
                HandleTrigger(AttributionTriggerMatches(
                    AttributionTriggerMatcherConfig(_, reporting_origin))))
        .WillOnce([&](AttributionTrigger trigger) { barrier.Run(); });

    send_trigger(std::move(reporting_origin));
  }

  // This one should be dropped.
  send_trigger(url::Origin::Create(GURL("https://excessive.test")));

  trigger_data_host_remote.FlushForTesting();
  source_data_host_remote.reset();
  loop.Run();

  CheckTriggerQueueHistograms(
      histograms, {
                      .dropped = 1,
                      .enqueued = kMaxDelayedTriggers,
                      .flushed = kMaxDelayedTriggers,
                      .delays =
                          {
                              {base::Seconds(0), kMaxDelayedTriggers},
                          },
                  });
}

TEST_F(AttributionDataHostManagerImplTest, SourceThenTrigger_TriggerDelayed) {
  base::HistogramTester histograms;

  Checkpoint checkpoint;
  {
    InSequence seq;

    EXPECT_CALL(mock_manager_, HandleTrigger).Times(0);
    EXPECT_CALL(mock_manager_, HandleSource);
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(mock_manager_, HandleTrigger);
  }

  mojo::Remote<blink::mojom::AttributionDataHost> source_data_host_remote;
  data_host_manager_.RegisterDataHost(
      source_data_host_remote.BindNewPipeAndPassReceiver(),
      url::Origin::Create(GURL("https://page1.example")));

  mojo::Remote<blink::mojom::AttributionDataHost> trigger_data_host_remote;
  data_host_manager_.RegisterDataHost(
      trigger_data_host_remote.BindNewPipeAndPassReceiver(),
      url::Origin::Create(GURL("https://page2.example")));

  auto source_data = blink::mojom::AttributionSourceData::New();
  source_data->destination = url::Origin::Create(GURL("https://dest.test"));
  source_data->reporting_origin =
      url::Origin::Create(GURL("https://report1.test"));
  source_data->filter_data = blink::mojom::AttributionFilterData::New();
  source_data_host_remote->SourceDataAvailable(std::move(source_data));
  source_data_host_remote.FlushForTesting();

  // Because there is still a connected data host in source mode, this trigger
  // should be delayed.
  auto trigger_data = blink::mojom::AttributionTriggerData::New();
  trigger_data->reporting_origin =
      url::Origin::Create(GURL("https://report2.test"));
  trigger_data->filters = blink::mojom::AttributionFilterData::New();
  trigger_data->not_filters = blink::mojom::AttributionFilterData::New();
  trigger_data_host_remote->TriggerDataAvailable(std::move(trigger_data));
  trigger_data_host_remote.FlushForTesting();

  task_environment_.FastForwardBy(base::Seconds(5) - base::Microseconds(1));
  checkpoint.Call(1);
  task_environment_.FastForwardBy(base::Microseconds(1));

  CheckTriggerQueueHistograms(histograms, {
                                              .enqueued = 1,
                                              .processed_with_delay = 1,
                                              .delays =
                                                  {
                                                      {base::Seconds(5), 1},
                                                  },
                                          });
}

// Tests that an insecure context origin or destination origin in
// `AttributionDataHostManagerImpl::NotifyNavigationForDataHost()` disconnects
// the data host and flushes triggers.
TEST_F(AttributionDataHostManagerImplTest, InsecureNavigationOrigin_Dropped) {
  const struct {
    url::Origin page_origin;
    url::Origin destination_origin;
  } kTestCases[] = {
      {
          url::Origin::Create(GURL("http://page.example")),
          url::Origin::Create(GURL("https://trigger.example")),
      },
      {
          url::Origin::Create(GURL("https://page.example")),
          url::Origin::Create(GURL("http://trigger.example")),
      },
      {
          url::Origin::Create(GURL("http://page.example")),
          url::Origin::Create(GURL("http://trigger.example")),
      },
  };

  for (const auto& test_case : kTestCases) {
    base::HistogramTester histograms;

    EXPECT_CALL(mock_manager_, HandleSource).Times(0);
    EXPECT_CALL(mock_manager_, HandleTrigger);

    const blink::AttributionSrcToken attribution_src_token;

    mojo::Remote<blink::mojom::AttributionDataHost> source_data_host_remote;
    data_host_manager_.RegisterNavigationDataHost(
        source_data_host_remote.BindNewPipeAndPassReceiver(),
        attribution_src_token);

    mojo::Remote<blink::mojom::AttributionDataHost> trigger_data_host_remote;
    data_host_manager_.RegisterDataHost(
        trigger_data_host_remote.BindNewPipeAndPassReceiver(),
        url::Origin::Create(GURL("https://page2.example")));

    auto trigger_data = blink::mojom::AttributionTriggerData::New();
    trigger_data->reporting_origin =
        url::Origin::Create(GURL("https://report2.test"));
    trigger_data->filters = blink::mojom::AttributionFilterData::New();
    trigger_data->not_filters = blink::mojom::AttributionFilterData::New();
    trigger_data_host_remote->TriggerDataAvailable(std::move(trigger_data));
    trigger_data_host_remote.FlushForTesting();

    data_host_manager_.NotifyNavigationForDataHost(
        attribution_src_token, test_case.page_origin,
        test_case.destination_origin);

    auto source_data = blink::mojom::AttributionSourceData::New();
    source_data->source_event_id = 10;
    source_data->destination = test_case.destination_origin;
    source_data->reporting_origin =
        url::Origin::Create(GURL("https://reporter.example"));
    source_data->filter_data = blink::mojom::AttributionFilterData::New();
    source_data_host_remote->SourceDataAvailable(std::move(source_data));
    source_data_host_remote.FlushForTesting();

    Mock::VerifyAndClear(&mock_manager_);

    CheckTriggerQueueHistograms(histograms, {
                                                .enqueued = 1,
                                                .flushed = 1,
                                                .delays =
                                                    {
                                                        {base::Seconds(0), 1},
                                                    },
                                            });
  }
}

TEST_F(AttributionDataHostManagerImplTest, NavigationDataHostNotRegistered) {
  base::HistogramTester histograms;

  const blink::AttributionSrcToken attribution_src_token;
  data_host_manager_.NotifyNavigationForDataHost(
      attribution_src_token, url::Origin::Create(GURL("https://page.example")),
      url::Origin::Create(GURL("https://trigger.example")));

  // kNotFound = 1.
  histograms.ExpectUniqueSample("Conversions.NavigationDataHostStatus", 1, 1);
}

TEST_F(AttributionDataHostManagerImplTest,
       TriggerDataHost_AggregatableValuesCheckPerformed) {
  const struct {
    const char* description;
    bool valid;
    base::flat_map<std::string, uint32_t> values;
  } kTestCases[] = {
      {"negative", false, {{"key", -1}}},
      {"zero", false, {{"key", 0}}},
      {"max limit", true, {{"key", 65536}}},
      {"out of range", false, {{"key", 65537}}},
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.description);  // EXPECT_CALL doesn't support <<
    EXPECT_CALL(mock_manager_, HandleTrigger).Times(test_case.valid);

    mojo::Remote<blink::mojom::AttributionDataHost> data_host_remote;
    data_host_manager_.RegisterDataHost(
        data_host_remote.BindNewPipeAndPassReceiver(),
        url::Origin::Create(GURL("https://trigger.example")));

    auto trigger_data = blink::mojom::AttributionTriggerData::New();
    trigger_data->reporting_origin =
        url::Origin::Create(GURL("https://reporter.example"));

    trigger_data->filters = blink::mojom::AttributionFilterData::New();
    trigger_data->not_filters = blink::mojom::AttributionFilterData::New();

    trigger_data->aggregatable_values = test_case.values;

    data_host_remote->TriggerDataAvailable(std::move(trigger_data));
    data_host_remote.FlushForTesting();

    Mock::VerifyAndClear(&mock_manager_);
  }
}

TEST_F(AttributionDataHostManagerImplTest,
       NavigationDataHost_CannotRegisterTrigger) {
  base::HistogramTester histograms;

  EXPECT_CALL(mock_manager_, HandleTrigger).Times(0);

  const blink::AttributionSrcToken attribution_src_token;
  mojo::Remote<blink::mojom::AttributionDataHost> data_host_remote;
  data_host_manager_.RegisterNavigationDataHost(
      data_host_remote.BindNewPipeAndPassReceiver(), attribution_src_token);

  data_host_manager_.NotifyNavigationForDataHost(
      attribution_src_token, url::Origin::Create(GURL("https://s.test")),
      url::Origin::Create(GURL("https://d.test")));

  mojo::test::BadMessageObserver bad_message_observer;

  auto trigger_data = blink::mojom::AttributionTriggerData::New();
  trigger_data->reporting_origin = url::Origin::Create(GURL("https://r.test"));
  trigger_data->filters = blink::mojom::AttributionFilterData::New();
  trigger_data->not_filters = blink::mojom::AttributionFilterData::New();

  data_host_remote->TriggerDataAvailable(std::move(trigger_data));
  data_host_remote.FlushForTesting();

  EXPECT_EQ(bad_message_observer.WaitForBadMessage(),
            "AttributionDataHost: Navigation-bound data hosts cannot register "
            "triggers.");

  // kContextError = 2.
  histograms.ExpectUniqueSample(kTriggerDataHandleStatusMetric, 2, 1);
}

TEST_F(AttributionDataHostManagerImplTest,
       DuplicateAttributionSrcToken_NotRegistered) {
  EXPECT_CALL(mock_manager_, HandleSource(SourceEventIdIs(1)));

  const blink::AttributionSrcToken attribution_src_token;

  mojo::Remote<blink::mojom::AttributionDataHost> data_host_remote1,
      data_host_remote2;

  {
    base::HistogramTester histograms;

    EXPECT_TRUE(data_host_manager_.RegisterNavigationDataHost(
        data_host_remote1.BindNewPipeAndPassReceiver(), attribution_src_token));

    // This one should not be registered, as `attribution_src_token` is already
    // associated with a receiver.
    EXPECT_FALSE(data_host_manager_.RegisterNavigationDataHost(
        data_host_remote2.BindNewPipeAndPassReceiver(), attribution_src_token));

    // kRegistered = 0.
    histograms.ExpectUniqueSample("Conversions.NavigationDataHostStatus", 0, 1);
  }

  const url::Origin destination_origin =
      url::Origin::Create(GURL("https://trigger.example"));

  data_host_manager_.NotifyNavigationForDataHost(
      attribution_src_token, url::Origin::Create(GURL("https://page.example")),
      destination_origin);

  auto source_data = blink::mojom::AttributionSourceData::New();
  source_data->source_event_id = 1;
  source_data->destination = destination_origin;
  source_data->reporting_origin =
      url::Origin::Create(GURL("https://reporter.example"));
  source_data->filter_data = blink::mojom::AttributionFilterData::New();
  data_host_remote1->SourceDataAvailable(source_data.Clone());
  data_host_remote1.FlushForTesting();

  source_data->source_event_id = 2;
  data_host_remote2->SourceDataAvailable(std::move(source_data));
  data_host_remote2.FlushForTesting();
}

}  // namespace
}  // namespace content
