// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_data_host_manager_impl.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "content/browser/attribution_reporting/attribution_aggregatable_source.h"
#include "content/browser/attribution_reporting/attribution_manager.h"
#include "content/browser/attribution_reporting/attribution_source_type.h"
#include "content/browser/attribution_reporting/attribution_test_utils.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
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

}  // namespace

class AttributionDataHostManagerImplTest : public testing::Test {
 public:
  AttributionDataHostManagerImplTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        data_host_manager_(
            std::make_unique<AttributionDataHostManagerImpl>(&mock_manager_)) {}

 protected:
  BrowserTaskEnvironment task_environment_;
  MockAttributionManager mock_manager_;
  std::unique_ptr<AttributionDataHostManagerImpl> data_host_manager_;
};

TEST_F(AttributionDataHostManagerImplTest, SourceDataHost_SourceRegistered) {
  base::HistogramTester histograms;

  auto page_origin = url::Origin::Create(GURL("https://page.example"));
  auto destination_origin =
      url::Origin::Create(GURL("https://trigger.example"));
  auto reporting_origin = url::Origin::Create(GURL("https://reporter.example"));
  EXPECT_CALL(mock_manager_,
              HandleSource(AllOf(
                  SourceTypeIs(AttributionSourceType::kEvent),
                  SourceEventIdIs(10), ConversionOriginIs(destination_origin),
                  ImpressionOriginIs(page_origin), SourcePriorityIs(20),
                  SourceDebugKeyIs(789),
                  AggregatableSourceAre(AttributionAggregatableSource::Create(
                      AggregatableSourceProtoBuilder()
                          .AddKey("key", AggregatableKeyProtoBuilder()
                                             .SetHighBits(5)
                                             .SetLowBits(345)
                                             .Build())
                          .Build())))));

  mojo::Remote<blink::mojom::AttributionDataHost> data_host_remote;
  data_host_manager_->RegisterDataHost(
      data_host_remote.BindNewPipeAndPassReceiver(), page_origin);

  auto source_data = blink::mojom::AttributionSourceData::New();
  source_data->source_event_id = 10;
  source_data->destination = destination_origin;
  source_data->reporting_origin = reporting_origin;
  source_data->priority = 20;
  source_data->debug_key = blink::mojom::AttributionDebugKey::New(789);
  source_data->filter_data = blink::mojom::AttributionFilterData::New();
  source_data->aggregatable_source =
      AggregatableSourceMojoBuilder()
          .AddKey(/*key_id=*/"key",
                  blink::mojom::AttributionAggregatableKey::New(
                      /*high_bits=*/5, /*low_bits=*/345))
          .Build();
  data_host_remote->SourceDataAvailable(std::move(source_data));
  data_host_remote.FlushForTesting();

  data_host_manager_.reset();

  histograms.ExpectUniqueSample("Conversions.RegisteredSourcesPerDataHost", 1,
                                1);
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
       .source_expected = false},
      {.source_origin = kLocalHost,
       .destination_origin = "http://insecure.com",
       .reporting_origin = kLocalHost,
       .source_expected = false},
      {.source_origin = "http://insecure.com",
       .destination_origin = kLocalHost,
       .reporting_origin = kLocalHost,
       .source_expected = false},
      {.source_origin = "https://secure.com",
       .destination_origin = "https://secure.com",
       .reporting_origin = "https://secure.com",
       .source_expected = true},
  };

  for (const auto& test_case : kTestCases) {
    EXPECT_CALL(mock_manager_, HandleSource).Times(test_case.source_expected);

    mojo::Remote<blink::mojom::AttributionDataHost> data_host_remote;
    data_host_manager_->RegisterDataHost(
        data_host_remote.BindNewPipeAndPassReceiver(),
        url::Origin::Create(GURL(test_case.source_origin)));

    auto source_data = blink::mojom::AttributionSourceData::New();
    source_data->destination =
        url::Origin::Create(GURL(test_case.destination_origin));
    source_data->reporting_origin =
        url::Origin::Create(GURL(test_case.reporting_origin));
    source_data->filter_data = blink::mojom::AttributionFilterData::New();
    source_data->aggregatable_source =
        blink::mojom::AttributionAggregatableSource::New();
    data_host_remote->SourceDataAvailable(std::move(source_data));
    data_host_remote.FlushForTesting();

    Mock::VerifyAndClear(&mock_manager_);
  }

  data_host_manager_.reset();

  histograms.ExpectUniqueSample("Conversions.RegisteredSourcesPerDataHost", 1,
                                3);
}

TEST_F(AttributionDataHostManagerImplTest,
       SourceDataHost_FilterSizeCheckPerformed) {
  for (const auto& test_case : kAttributionFilterSizeTestCases) {
    SCOPED_TRACE(test_case.description);  // EXPECT_CALL doesn't support <<
    EXPECT_CALL(mock_manager_, HandleSource).Times(test_case.valid);

    mojo::Remote<blink::mojom::AttributionDataHost> data_host_remote;
    data_host_manager_->RegisterDataHost(
        data_host_remote.BindNewPipeAndPassReceiver(),
        url::Origin::Create(GURL("https://page.example")));

    auto source_data = blink::mojom::AttributionSourceData::New();
    source_data->destination =
        url::Origin::Create(GURL("https://trigger.example"));
    source_data->reporting_origin =
        url::Origin::Create(GURL("https://reporter.example"));
    source_data->filter_data =
        blink::mojom::AttributionFilterData::New(test_case.AsMap());
    source_data->aggregatable_source =
        blink::mojom::AttributionAggregatableSource::New();
    data_host_remote->SourceDataAvailable(std::move(source_data));
    data_host_remote.FlushForTesting();

    Mock::VerifyAndClear(&mock_manager_);
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
    SCOPED_TRACE(test_case.description);  // EXPECT_CALL doesn't support <<
    EXPECT_CALL(mock_manager_, HandleSource).Times(test_case.valid);

    mojo::Remote<blink::mojom::AttributionDataHost> data_host_remote;
    data_host_manager_->RegisterDataHost(
        data_host_remote.BindNewPipeAndPassReceiver(),
        url::Origin::Create(GURL("https://page.example")));

    auto source_data = blink::mojom::AttributionSourceData::New();
    source_data->destination =
        url::Origin::Create(GURL("https://trigger.example"));
    source_data->reporting_origin =
        url::Origin::Create(GURL("https://reporter.example"));
    source_data->filter_data =
        blink::mojom::AttributionFilterData::New(test_case.filter_data);
    source_data->aggregatable_source =
        blink::mojom::AttributionAggregatableSource::New();
    data_host_remote->SourceDataAvailable(std::move(source_data));
    data_host_remote.FlushForTesting();

    Mock::VerifyAndClear(&mock_manager_);
  }
}

TEST_F(AttributionDataHostManagerImplTest,
       SourceDataHost_ReceiverDestinationCheckPerformed) {
  base::HistogramTester histograms;

  Checkpoint checkpoint;
  {
    InSequence seq;

    EXPECT_CALL(mock_manager_, HandleSource);
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(mock_manager_, HandleSource);
    EXPECT_CALL(checkpoint, Call(2));
    EXPECT_CALL(mock_manager_, HandleSource).Times(0);
    EXPECT_CALL(checkpoint, Call(3));
    EXPECT_CALL(mock_manager_, HandleSource).Times(0);
  }

  auto page_origin = url::Origin::Create(GURL("https://page.example"));
  auto destination_origin =
      url::Origin::Create(GURL("https://trigger.example"));
  auto reporting_origin = url::Origin::Create(GURL("https://reporter.example"));

  mojo::Remote<blink::mojom::AttributionDataHost> data_host_remote;
  data_host_manager_->RegisterDataHost(
      data_host_remote.BindNewPipeAndPassReceiver(), page_origin);

  auto source_data = blink::mojom::AttributionSourceData::New();
  source_data->destination = destination_origin;
  source_data->reporting_origin = reporting_origin;
  source_data->filter_data = blink::mojom::AttributionFilterData::New();
  source_data->aggregatable_source =
      blink::mojom::AttributionAggregatableSource::New();
  data_host_remote->SourceDataAvailable(source_data.Clone());
  data_host_remote.FlushForTesting();

  checkpoint.Call(1);

  data_host_remote->SourceDataAvailable(source_data.Clone());
  data_host_remote.FlushForTesting();

  checkpoint.Call(2);

  source_data->destination =
      url::Origin::Create(GURL("https://other-trigger.example"));
  data_host_remote->SourceDataAvailable(source_data.Clone());
  data_host_remote.FlushForTesting();

  checkpoint.Call(3);
  data_host_remote->SourceDataAvailable(std::move(source_data));
  data_host_remote.FlushForTesting();

  data_host_manager_.reset();

  histograms.ExpectUniqueSample("Conversions.RegisteredSourcesPerDataHost", 2,
                                1);
}

TEST_F(AttributionDataHostManagerImplTest,
       SourceDataHost_AggregatableSourceizeCheckPerformed) {
  struct AggregatableSourceizeTestCase {
    const char* description;
    bool valid;
    size_t key_count;
    size_t key_size;

    blink::mojom::AttributionAggregatableSourcePtr GetAggregatableSource()
        const {
      AggregatableSourceMojoBuilder builder;
      for (size_t i = 0u; i < key_count; ++i) {
        std::string key(key_size, 'A' + i);
        builder.AddKey(std::move(key),
                       blink::mojom::AttributionAggregatableKey::New(
                           /*high_bits=*/i, /*low_bits=*/i));
      }
      return builder.Build();
    }
  };

  const AggregatableSourceizeTestCase kTestCases[] = {
      {"empty", true, 0, 0},
      {"max_keys", true,
       blink::kMaxAttributionAggregatableKeysPerSourceOrTrigger, 1},
      {"too_many_keys", false,
       blink::kMaxAttributionAggregatableKeysPerSourceOrTrigger + 1, 1},
      {"max_key_size", true, 1,
       blink::kMaxBytesPerAttributionAggregatableKeyId},
      {"excessive_key_size", false, 1,
       blink::kMaxBytesPerAttributionAggregatableKeyId + 1},
  };

  for (auto& test_case : kTestCases) {
    SCOPED_TRACE(
        test_case.description);  // Since EXPECT_CALL doesn't support <<
    EXPECT_CALL(mock_manager_, HandleSource).Times(test_case.valid);

    mojo::Remote<blink::mojom::AttributionDataHost> data_host_remote;
    data_host_manager_->RegisterDataHost(
        data_host_remote.BindNewPipeAndPassReceiver(),
        url::Origin::Create(GURL("https://page.example")));

    auto source_data = blink::mojom::AttributionSourceData::New();
    source_data->destination =
        url::Origin::Create(GURL("https://trigger.example"));
    source_data->reporting_origin =
        url::Origin::Create(GURL("https://reporter.example"));
    source_data->filter_data = blink::mojom::AttributionFilterData::New();
    source_data->aggregatable_source = test_case.GetAggregatableSource();
    data_host_remote->SourceDataAvailable(std::move(source_data));
    data_host_remote.FlushForTesting();

    Mock::VerifyAndClear(&mock_manager_);
  }
}

TEST_F(AttributionDataHostManagerImplTest, TriggerDataHost_TriggerRegistered) {
  base::HistogramTester histograms;

  auto destination_origin =
      url::Origin::Create(GURL("https://trigger.example"));
  auto reporting_origin = url::Origin::Create(GURL("https://reporter.example"));
  EXPECT_CALL(
      mock_manager_,
      HandleTrigger(AttributionTriggerMatches({
          .destination_origin = destination_origin,
          .reporting_origin = reporting_origin,
          .filters = *AttributionFilterData::FromTriggerFilterValues({
              {"a", {"b"}},
          }),
          .debug_key = Optional(789),
          .event_triggers = ElementsAre(
              EventTriggerDataMatches({
                  .data = 1,
                  .priority = 2,
                  .dedup_key = Optional(3),
                  .filters = *AttributionFilterData::FromTriggerFilterValues({
                      {"c", {"d"}},
                  }),
                  .not_filters =
                      *AttributionFilterData::FromTriggerFilterValues({
                          {"e", {"f"}},
                      }),
              }),
              EventTriggerDataMatches({
                  .data = 4,
                  .priority = 5,
                  .dedup_key = Eq(absl::nullopt),
                  .filters = AttributionFilterData(),
                  .not_filters = AttributionFilterData(),
              })),
      })));

  mojo::Remote<blink::mojom::AttributionDataHost> data_host_remote;
  data_host_manager_->RegisterDataHost(
      data_host_remote.BindNewPipeAndPassReceiver(), destination_origin);

  auto trigger_data = blink::mojom::AttributionTriggerData::New();
  trigger_data->reporting_origin = reporting_origin;
  trigger_data->debug_key = blink::mojom::AttributionDebugKey::New(789);

  trigger_data->filters = blink::mojom::AttributionFilterData::New(
      AttributionFilterData::FilterValues({{"a", {"b"}}}));

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

  trigger_data->aggregatable_trigger =
      blink::mojom::AttributionAggregatableTrigger::New();

  data_host_remote->TriggerDataAvailable(std::move(trigger_data));
  data_host_remote.FlushForTesting();

  data_host_manager_.reset();

  histograms.ExpectBucketCount("Conversions.RegisteredTriggersPerDataHost", 1,
                               1);
}

TEST_F(AttributionDataHostManagerImplTest,
       TriggerDataHost_OriginTrustworthyChecksPerformed) {
  base::HistogramTester histograms;

  const char kLocalHost[] = "http://localhost";

  struct {
    const char* destination_origin;
    const char* reporting_origin;
    bool trigger_expected;
  } kTestCases[] = {
      {.destination_origin = kLocalHost,
       .reporting_origin = kLocalHost,
       .trigger_expected = true},
      {.destination_origin = "http://127.0.0.1",
       .reporting_origin = "http://127.0.0.1",
       .trigger_expected = true},
      {.destination_origin = kLocalHost,
       .reporting_origin = "http://insecure.com",
       .trigger_expected = false},
      {.destination_origin = "http://insecure.com",
       .reporting_origin = kLocalHost,
       .trigger_expected = false},
      {.destination_origin = "https://secure.com",
       .reporting_origin = "https://secure.com",
       .trigger_expected = true},
  };

  for (const auto& test_case : kTestCases) {
    EXPECT_CALL(mock_manager_, HandleTrigger).Times(test_case.trigger_expected);

    mojo::Remote<blink::mojom::AttributionDataHost> data_host_remote;
    data_host_manager_->RegisterDataHost(
        data_host_remote.BindNewPipeAndPassReceiver(),
        url::Origin::Create(GURL(test_case.destination_origin)));

    auto trigger_data = blink::mojom::AttributionTriggerData::New();
    trigger_data->reporting_origin =
        url::Origin::Create(GURL(test_case.reporting_origin));

    trigger_data->filters = blink::mojom::AttributionFilterData::New();
    trigger_data->aggregatable_trigger =
        blink::mojom::AttributionAggregatableTrigger::New();

    data_host_remote->TriggerDataAvailable(std::move(trigger_data));
    data_host_remote.FlushForTesting();

    Mock::VerifyAndClear(&mock_manager_);
  }

  data_host_manager_.reset();

  histograms.ExpectUniqueSample("Conversions.RegisteredTriggersPerDataHost", 1,
                                3);
}

TEST_F(AttributionDataHostManagerImplTest,
       TriggerDataHost_TopLevelFilterSizeCheckPerformed) {
  for (const auto& test_case : kAttributionFilterSizeTestCases) {
    SCOPED_TRACE(test_case.description);  // EXPECT_CALL doesn't support <<
    EXPECT_CALL(mock_manager_, HandleTrigger).Times(test_case.valid);

    mojo::Remote<blink::mojom::AttributionDataHost> data_host_remote;
    data_host_manager_->RegisterDataHost(
        data_host_remote.BindNewPipeAndPassReceiver(),
        url::Origin::Create(GURL("https://trigger.example")));

    auto trigger_data = blink::mojom::AttributionTriggerData::New();
    trigger_data->reporting_origin =
        url::Origin::Create(GURL("https://reporter.example"));

    trigger_data->filters =
        blink::mojom::AttributionFilterData::New(test_case.AsMap());

    trigger_data->aggregatable_trigger =
        blink::mojom::AttributionAggregatableTrigger::New();

    data_host_remote->TriggerDataAvailable(std::move(trigger_data));
    data_host_remote.FlushForTesting();

    Mock::VerifyAndClear(&mock_manager_);
  }
}

TEST_F(AttributionDataHostManagerImplTest,
       TriggerDataHost_EventTriggerDataFilterSizeCheckPerformed) {
  for (const auto& test_case : kAttributionFilterSizeTestCases) {
    SCOPED_TRACE(test_case.description);  // EXPECT_CALL doesn't support <<
    EXPECT_CALL(mock_manager_, HandleTrigger).Times(test_case.valid);

    mojo::Remote<blink::mojom::AttributionDataHost> data_host_remote;
    data_host_manager_->RegisterDataHost(
        data_host_remote.BindNewPipeAndPassReceiver(),
        url::Origin::Create(GURL("https://trigger.example")));

    auto trigger_data = blink::mojom::AttributionTriggerData::New();
    trigger_data->reporting_origin =
        url::Origin::Create(GURL("https://reporter.example"));

    trigger_data->filters = blink::mojom::AttributionFilterData::New();

    trigger_data->aggregatable_trigger =
        blink::mojom::AttributionAggregatableTrigger::New();

    trigger_data->event_triggers.push_back(blink::mojom::EventTriggerData::New(
        /*data=*/0,
        /*priority=*/0,
        /*dedup_key=*/nullptr,
        /*filters=*/blink::mojom::AttributionFilterData::New(test_case.AsMap()),
        /*not_filters=*/blink::mojom::AttributionFilterData::New()));

    data_host_remote->TriggerDataAvailable(std::move(trigger_data));
    data_host_remote.FlushForTesting();

    Mock::VerifyAndClear(&mock_manager_);
  }
}

TEST_F(AttributionDataHostManagerImplTest,
       TriggerDataHost_EventTriggerDataNotFilterSizeCheckPerformed) {
  for (const auto& test_case : kAttributionFilterSizeTestCases) {
    SCOPED_TRACE(test_case.description);  // EXPECT_CALL doesn't support <<
    EXPECT_CALL(mock_manager_, HandleTrigger).Times(test_case.valid);

    mojo::Remote<blink::mojom::AttributionDataHost> data_host_remote;
    data_host_manager_->RegisterDataHost(
        data_host_remote.BindNewPipeAndPassReceiver(),
        url::Origin::Create(GURL("https://trigger.example")));

    auto trigger_data = blink::mojom::AttributionTriggerData::New();
    trigger_data->reporting_origin =
        url::Origin::Create(GURL("https://reporter.example"));

    trigger_data->filters = blink::mojom::AttributionFilterData::New();

    trigger_data->aggregatable_trigger =
        blink::mojom::AttributionAggregatableTrigger::New();

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
    EXPECT_CALL(mock_manager_, HandleTrigger).Times(test_case.expected);

    mojo::Remote<blink::mojom::AttributionDataHost> data_host_remote;
    data_host_manager_->RegisterDataHost(
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
    trigger_data->aggregatable_trigger =
        blink::mojom::AttributionAggregatableTrigger::New();

    data_host_remote->TriggerDataAvailable(std::move(trigger_data));
    data_host_remote.FlushForTesting();

    Mock::VerifyAndClear(&mock_manager_);
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

  mojo::Remote<blink::mojom::AttributionDataHost> data_host_remote;
  data_host_manager_->RegisterDataHost(
      data_host_remote.BindNewPipeAndPassReceiver(), destination_origin);

  auto trigger_data = blink::mojom::AttributionTriggerData::New();
  trigger_data->reporting_origin = reporting_origin;
  trigger_data->filters = blink::mojom::AttributionFilterData::New();
  trigger_data->aggregatable_trigger =
      blink::mojom::AttributionAggregatableTrigger::New();

  data_host_remote->TriggerDataAvailable(trigger_data.Clone());
  data_host_remote.FlushForTesting();

  checkpoint.Call(1);

  data_host_remote->TriggerDataAvailable(trigger_data.Clone());
  data_host_remote.FlushForTesting();

  checkpoint.Call(2);

  auto source_data = blink::mojom::AttributionSourceData::New();
  source_data->destination = destination_origin;
  source_data->reporting_origin = reporting_origin;
  source_data->filter_data = blink::mojom::AttributionFilterData::New();
  source_data->aggregatable_source =
      blink::mojom::AttributionAggregatableSource::New();

  data_host_remote->SourceDataAvailable(std::move(source_data));
  data_host_remote.FlushForTesting();

  checkpoint.Call(3);

  data_host_remote->TriggerDataAvailable(std::move(trigger_data));
  data_host_remote.FlushForTesting();

  data_host_manager_.reset();

  histograms.ExpectTotalCount("Conversions.RegisteredSourcesPerDataHost", 0);
  histograms.ExpectUniqueSample("Conversions.RegisteredTriggersPerDataHost", 3,
                                1);
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

  mojo::Remote<blink::mojom::AttributionDataHost> data_host_remote;
  data_host_manager_->RegisterDataHost(
      data_host_remote.BindNewPipeAndPassReceiver(), page_origin);

  auto source_data = blink::mojom::AttributionSourceData::New();
  source_data->destination = destination_origin;
  source_data->reporting_origin = reporting_origin;
  source_data->filter_data = blink::mojom::AttributionFilterData::New();
  source_data->aggregatable_source =
      blink::mojom::AttributionAggregatableSource::New();

  data_host_remote->SourceDataAvailable(source_data.Clone());
  data_host_remote.FlushForTesting();

  checkpoint.Call(1);

  data_host_remote->SourceDataAvailable(source_data.Clone());
  data_host_remote.FlushForTesting();

  checkpoint.Call(2);

  auto trigger_data = blink::mojom::AttributionTriggerData::New();
  trigger_data->reporting_origin = reporting_origin;
  trigger_data->filters = blink::mojom::AttributionFilterData::New();
  trigger_data->aggregatable_trigger =
      blink::mojom::AttributionAggregatableTrigger::New();

  data_host_remote->TriggerDataAvailable(std::move(trigger_data));
  data_host_remote.FlushForTesting();

  checkpoint.Call(3);

  data_host_remote->SourceDataAvailable(std::move(source_data));
  data_host_remote.FlushForTesting();

  data_host_manager_.reset();

  histograms.ExpectUniqueSample("Conversions.RegisteredSourcesPerDataHost", 3,
                                1);
  histograms.ExpectTotalCount("Conversions.RegisteredTriggersPerDataHost", 0);
}

TEST_F(AttributionDataHostManagerImplTest,
       SourceDataHost_NavigationSourceRegistered) {
  const auto page_origin = url::Origin::Create(GURL("https://page.example"));
  const auto destination_origin =
      url::Origin::Create(GURL("https://trigger.example"));
  const auto reporting_origin =
      url::Origin::Create(GURL("https://reporter.example"));
  EXPECT_CALL(mock_manager_,
              HandleSource(AllOf(
                  SourceTypeIs(AttributionSourceType::kNavigation),
                  SourceEventIdIs(10), ConversionOriginIs(destination_origin),
                  ImpressionOriginIs(page_origin), SourcePriorityIs(20),
                  SourceDebugKeyIs(789),
                  AggregatableSourceAre(AttributionAggregatableSource::Create(
                      AggregatableSourceProtoBuilder()
                          .AddKey("key", AggregatableKeyProtoBuilder()
                                             .SetHighBits(5)
                                             .SetLowBits(345)
                                             .Build())
                          .Build())))));

  const blink::AttributionSrcToken attribution_src_token;

  mojo::Remote<blink::mojom::AttributionDataHost> data_host_remote;
  data_host_manager_->RegisterNavigationDataHost(
      data_host_remote.BindNewPipeAndPassReceiver(), attribution_src_token);

  data_host_manager_->NotifyNavigationForDataHost(
      attribution_src_token, page_origin, destination_origin);

  auto source_data = blink::mojom::AttributionSourceData::New();
  source_data->source_event_id = 10;
  source_data->destination = destination_origin;
  source_data->reporting_origin = reporting_origin;
  source_data->priority = 20;
  source_data->debug_key = blink::mojom::AttributionDebugKey::New(789);
  source_data->filter_data = blink::mojom::AttributionFilterData::New();
  source_data->aggregatable_source =
      AggregatableSourceMojoBuilder()
          .AddKey(/*key_id=*/"key",
                  blink::mojom::AttributionAggregatableKey::New(
                      /*high_bits=*/5, /*low_bits=*/345))
          .Build();
  data_host_remote->SourceDataAvailable(std::move(source_data));
  data_host_remote.FlushForTesting();
}

// Ensures correct behavior in
// `AttributionDataHostManagerImpl::OnDataHostDisconnected()` when a data host
// is registered but disconnects before registering a source or trigger.
TEST_F(AttributionDataHostManagerImplTest, NoSourceOrTrigger) {
  base::HistogramTester histograms;

  auto page_origin = url::Origin::Create(GURL("https://page.example"));

  mojo::Remote<blink::mojom::AttributionDataHost> data_host_remote;
  data_host_manager_->RegisterDataHost(
      data_host_remote.BindNewPipeAndPassReceiver(), page_origin);
  data_host_remote.reset();
  task_environment_.RunUntilIdle();

  data_host_manager_.reset();

  histograms.ExpectTotalCount("Conversions.RegisteredSourcesPerDataHost", 0);
  histograms.ExpectTotalCount("Conversions.RegisteredTriggersPerDataHost", 0);
}

}  // namespace content
