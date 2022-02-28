// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_data_host_manager_impl.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <utility>

#include "base/strings/string_number_conversions.h"
#include "content/browser/attribution_reporting/attribution_aggregatable_sources.h"
#include "content/browser/attribution_reporting/attribution_manager.h"
#include "content/browser/attribution_reporting/attribution_test_utils.h"
#include "content/public/browser/browser_context.h"
#include "content/public/common/content_client.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
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

using ConversionMeasurementOperation =
    ::content::ContentBrowserClient::ConversionMeasurementOperation;

using ::testing::_;
using ::testing::AllOf;
using ::testing::InSequence;
using ::testing::IsNull;
using ::testing::Mock;
using ::testing::Pointee;
using ::testing::Return;

using Checkpoint = ::testing::MockFunction<void(int step)>;

}  // namespace

class AttributionDataHostManagerImplTest : public testing::Test {
 public:
  AttributionDataHostManagerImplTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        browser_context_(std::make_unique<TestBrowserContext>()),
        data_host_manager_(browser_context_.get(), &mock_manager_) {}

 protected:
  BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestBrowserContext> browser_context_;
  MockAttributionManager mock_manager_;
  AttributionDataHostManagerImpl data_host_manager_;
};

TEST_F(AttributionDataHostManagerImplTest, SourceDataHost_SourceRegistered) {
  auto page_origin = url::Origin::Create(GURL("https://page.example"));
  auto destination_origin =
      url::Origin::Create(GURL("https://trigger.example"));
  auto reporting_origin = url::Origin::Create(GURL("https://reporter.example"));
  EXPECT_CALL(mock_manager_,
              HandleSource(AllOf(
                  SourceTypeIs(CommonSourceInfo::SourceType::kEvent),
                  SourceEventIdIs(10), ConversionOriginIs(destination_origin),
                  ImpressionOriginIs(page_origin), SourcePriorityIs(20),
                  SourceDebugKeyIs(789),
                  AggregatableSourcesAre(AttributionAggregatableSources::Create(
                      AggregatableSourcesProtoBuilder()
                          .AddKey("key", AggregatableKeyProtoBuilder()
                                             .SetHighBits(5)
                                             .SetLowBits(345)
                                             .Build())
                          .Build())))));

  mojo::Remote<blink::mojom::AttributionDataHost> data_host_remote;
  data_host_manager_.RegisterDataHost(
      data_host_remote.BindNewPipeAndPassReceiver(), page_origin);

  auto source_data = blink::mojom::AttributionSourceData::New();
  source_data->source_event_id = 10;
  source_data->destination = destination_origin;
  source_data->reporting_origin = reporting_origin;
  source_data->priority = 20;
  source_data->debug_key = blink::mojom::AttributionDebugKey::New(789);
  source_data->filter_data = blink::mojom::AttributionFilterData::New();
  source_data->aggregatable_sources =
      AggregatableSourcesMojoBuilder()
          .AddKey(/*key_id=*/"key",
                  blink::mojom::AttributionAggregatableKey::New(
                      /*high_bits=*/5, /*low_bits=*/345))
          .Build();
  data_host_remote->SourceDataAvailable(std::move(source_data));
  data_host_remote.FlushForTesting();
}

TEST_F(AttributionDataHostManagerImplTest,
       SourceDataHost_OriginTrustworthyChecksPerformed) {
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
    data_host_manager_.RegisterDataHost(
        data_host_remote.BindNewPipeAndPassReceiver(),
        url::Origin::Create(GURL(test_case.source_origin)));

    auto source_data = blink::mojom::AttributionSourceData::New();
    source_data->destination =
        url::Origin::Create(GURL(test_case.destination_origin));
    source_data->reporting_origin =
        url::Origin::Create(GURL(test_case.reporting_origin));
    source_data->filter_data = blink::mojom::AttributionFilterData::New();
    source_data->aggregatable_sources =
        blink::mojom::AttributionAggregatableSources::New();
    data_host_remote->SourceDataAvailable(std::move(source_data));
    data_host_remote.FlushForTesting();

    Mock::VerifyAndClear(&mock_manager_);
  }
}

TEST_F(AttributionDataHostManagerImplTest,
       SourceDataHost_FilterSizeCheckPerformed) {
  for (const auto& test_case : kAttributionFilterSizeTestCases) {
    SCOPED_TRACE(test_case.description);  // EXPECT_CALL doesn't support <<
    EXPECT_CALL(mock_manager_, HandleSource).Times(test_case.valid);

    mojo::Remote<blink::mojom::AttributionDataHost> data_host_remote;
    data_host_manager_.RegisterDataHost(
        data_host_remote.BindNewPipeAndPassReceiver(),
        url::Origin::Create(GURL("https://page.example")));

    auto source_data = blink::mojom::AttributionSourceData::New();
    source_data->destination =
        url::Origin::Create(GURL("https://trigger.example"));
    source_data->reporting_origin =
        url::Origin::Create(GURL("https://reporter.example"));
    source_data->filter_data =
        blink::mojom::AttributionFilterData::New(test_case.AsMap());
    source_data->aggregatable_sources =
        blink::mojom::AttributionAggregatableSources::New();
    data_host_remote->SourceDataAvailable(std::move(source_data));
    data_host_remote.FlushForTesting();

    Mock::VerifyAndClear(&mock_manager_);
  }
}

TEST_F(AttributionDataHostManagerImplTest,
       SourceDataHostEmbedderDisallow_SourceDropped) {
  EXPECT_CALL(mock_manager_, HandleSource).Times(0);

  auto page_origin = url::Origin::Create(GURL("https://page.example"));
  auto destination_origin =
      url::Origin::Create(GURL("https://trigger.example"));
  auto reporting_origin = url::Origin::Create(GURL("https://reporter.example"));

  MockAttributionReportingContentBrowserClient browser_client;
  EXPECT_CALL(browser_client,
              IsConversionMeasurementOperationAllowed(
                  _, ConversionMeasurementOperation::kImpression,
                  Pointee(page_origin), IsNull(), Pointee(reporting_origin)))
      .Times(1)
      .WillRepeatedly(Return(false));
  ScopedContentBrowserClientSetting setting(&browser_client);
  mojo::Remote<blink::mojom::AttributionDataHost> data_host_remote;
  data_host_manager_.RegisterDataHost(
      data_host_remote.BindNewPipeAndPassReceiver(), page_origin);

  auto source_data = blink::mojom::AttributionSourceData::New();
  source_data->source_event_id = 10;
  source_data->destination = destination_origin;
  source_data->reporting_origin = reporting_origin;
  source_data->filter_data = blink::mojom::AttributionFilterData::New();
  source_data->aggregatable_sources =
      blink::mojom::AttributionAggregatableSources::New();
  data_host_remote->SourceDataAvailable(std::move(source_data));
  data_host_remote.FlushForTesting();
}

TEST_F(AttributionDataHostManagerImplTest,
       SourceDataHost_ReceiverDestinationCheckPerformed) {
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
  data_host_manager_.RegisterDataHost(
      data_host_remote.BindNewPipeAndPassReceiver(), page_origin);

  auto source_data = blink::mojom::AttributionSourceData::New();
  source_data->destination = destination_origin;
  source_data->reporting_origin = reporting_origin;
  source_data->filter_data = blink::mojom::AttributionFilterData::New();
  source_data->aggregatable_sources =
      blink::mojom::AttributionAggregatableSources::New();
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
}

TEST_F(AttributionDataHostManagerImplTest,
       SourceDataHost_AggregatableSourcesSizeCheckPerformed) {
  struct AggregatableSourcesSizeTestCase {
    const char* description;
    bool valid;
    size_t key_count;
    size_t key_size;

    blink::mojom::AttributionAggregatableSourcesPtr GetAggregatableSources()
        const {
      AggregatableSourcesMojoBuilder builder;
      for (size_t i = 0u; i < key_count; ++i) {
        std::string key(key_size, 'A' + i);
        builder.AddKey(std::move(key),
                       blink::mojom::AttributionAggregatableKey::New(
                           /*high_bits=*/i, /*low_bits=*/i));
      }
      return builder.Build();
    }
  };

  const AggregatableSourcesSizeTestCase kTestCases[] = {
      {"empty", true, 0, 0},
      {"max_keys", true, blink::kMaxAttributionAggregatableKeysPerSource, 1},
      {"too_many_keys", false,
       blink::kMaxAttributionAggregatableKeysPerSource + 1, 1},
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
    data_host_manager_.RegisterDataHost(
        data_host_remote.BindNewPipeAndPassReceiver(),
        url::Origin::Create(GURL("https://page.example")));

    auto source_data = blink::mojom::AttributionSourceData::New();
    source_data->destination =
        url::Origin::Create(GURL("https://trigger.example"));
    source_data->reporting_origin =
        url::Origin::Create(GURL("https://reporter.example"));
    source_data->filter_data = blink::mojom::AttributionFilterData::New();
    source_data->aggregatable_sources = test_case.GetAggregatableSources();
    data_host_remote->SourceDataAvailable(std::move(source_data));
    data_host_remote.FlushForTesting();

    Mock::VerifyAndClear(&mock_manager_);
  }
}

}  // namespace content
