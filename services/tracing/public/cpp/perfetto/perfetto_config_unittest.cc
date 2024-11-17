// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/public/cpp/perfetto/perfetto_config.h"

#include "base/base_paths.h"
#include "base/json/json_reader.h"
#include "base/path_service.h"
#include "base/test/test_proto_loader.h"
#include "base/trace_event/trace_config.h"
#include "build/build_config.h"
#include "build/chromecast_buildflags.h"
#include "build/chromeos_buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/perfetto/protos/perfetto/config/data_source_config.gen.h"

namespace tracing {

namespace {

base::FilePath GetTestDataRoot() {
  return base::PathService::CheckedGet(base::DIR_GEN_TEST_DATA_ROOT);
}

class AdaptPerfettoConfigForChromeTest : public ::testing::Test {
 public:
  ~AdaptPerfettoConfigForChromeTest() override = default;

  perfetto::TraceConfig ParsePerfettoConfigFromText(
      const std::string& proto_text) {
    std::string serialized_message =
        config_loader_.ParseFromText("perfetto.protos.TraceConfig", proto_text);
    perfetto::TraceConfig destination;
    destination.ParseFromString(serialized_message);
    return destination;
  }

  std::string PrintConfigToText(perfetto::TraceConfig message) {
    std::string serialized_message = message.SerializeAsString();
    std::string proto_text = config_loader_.PrintToText(
        "perfetto.protos.TraceConfig", serialized_message);
    return proto_text;
  }

  std::string PrintConfigToText(
      std::optional<perfetto::DataSourceConfig> message) {
    if (!message) {
      return "";
    }
    std::string serialized_message = message->SerializeAsString();
    std::string proto_text = config_loader_.PrintToText(
        "perfetto.protos.DataSourceConfig", serialized_message);
    return proto_text;
  }

  std::optional<perfetto::DataSourceConfig> GetDataSourceConfig(
      perfetto::TraceConfig config,
      std::string_view name) {
    for (const auto& data_source_config : config.data_sources()) {
      if (data_source_config.config().name() == name) {
        return data_source_config.config();
      }
    }
    return std::nullopt;
  }

 protected:
  base::TestProtoSetLoader config_loader_{GetTestDataRoot().Append(
      FILE_PATH_LITERAL("third_party/perfetto/protos/perfetto/"
                        "config/config.descriptor"))};
};

base::trace_event::TraceConfig ParseTraceConfigFromJson(
    const std::string& json_config) {
  auto dict = base::JSONReader::Read(json_config);
  return base::trace_event::TraceConfig(std::move(*dict).TakeDict());
}

}  // namespace

TEST_F(AdaptPerfettoConfigForChromeTest, Simple) {
  auto perfetto_config = ParsePerfettoConfigFromText(R"pb(
    data_sources: {
      config: {
        name: "track_event"
        track_event_config: {
          enabled_categories: [ "foo", "__metadata" ]
          disabled_categories: [ "*" ]
        }
      }
    }
    data_sources: { config: { name: "org.chromium.trace_metadata" } }
  )pb");
  auto trace_config = GetDefaultPerfettoConfig(ParseTraceConfigFromJson(R"json({
      "record_mode": "record-continuously",
      "included_categories": ["foo"]
    })json"));
  EXPECT_TRUE(AdaptPerfettoConfigForChrome(&perfetto_config));
  EXPECT_EQ(PrintConfigToText(trace_config),
            PrintConfigToText(perfetto_config));
}

TEST_F(AdaptPerfettoConfigForChromeTest, LegacyTraceEvent) {
  auto perfetto_config = ParsePerfettoConfigFromText(R"pb(
    data_sources: {
      config: {
        name: "org.chromium.trace_event"
        track_event_config: {
          enabled_categories: [ "foo", "__metadata" ]
          disabled_categories: [ "*" ]
        }
      }
    }
    data_sources: { config: { name: "org.chromium.trace_metadata" } }
  )pb");
  auto trace_config = GetDefaultPerfettoConfig(ParseTraceConfigFromJson(R"json({
      "record_mode": "record-continuously",
      "included_categories": ["foo"]
    })json"));
  EXPECT_TRUE(AdaptPerfettoConfigForChrome(&perfetto_config));
  EXPECT_EQ(PrintConfigToText(trace_config),
            PrintConfigToText(perfetto_config));
}

TEST_F(AdaptPerfettoConfigForChromeTest, UnsupportedTrackEvent) {
  auto perfetto_config = ParsePerfettoConfigFromText(R"pb(
    data_sources: {
      config: {
        name: "org.chromium.trace_event"
        track_event_config: {
          enabled_tags: [ "foo" ]
          disabled_tags: [ "*" ]
        }
      }
    }
  )pb");
  EXPECT_FALSE(AdaptPerfettoConfigForChrome(&perfetto_config));
}

TEST_F(AdaptPerfettoConfigForChromeTest, DisabledCategories) {
  auto perfetto_config = ParsePerfettoConfigFromText(R"pb(
    data_sources: {
      config: {
        name: "track_event"
        track_event_config: {
          enabled_categories: [ "*", "__metadata" ]
          disabled_categories: [ "bar" ]
        }
      }
    }
    data_sources: { config: { name: "org.chromium.trace_metadata" } }
  )pb");
  auto trace_config = GetDefaultPerfettoConfig(ParseTraceConfigFromJson(R"json({
      "record_mode": "record-continuously",
      "excluded_categories": ["bar"]
    })json"));
  EXPECT_TRUE(AdaptPerfettoConfigForChrome(&perfetto_config));
  EXPECT_EQ(PrintConfigToText(trace_config),
            PrintConfigToText(perfetto_config));
}

TEST_F(AdaptPerfettoConfigForChromeTest, PrivacyFiltering) {
  auto perfetto_config = ParsePerfettoConfigFromText(R"pb(
    data_sources: { config: { name: "org.chromium.trace_metadata" } }
  )pb");
  EXPECT_TRUE(AdaptPerfettoConfigForChrome(&perfetto_config,
                                           /*privacy_filtering_enabled*/ true));
  auto trace_config =
      GetDefaultPerfettoConfig(ParseTraceConfigFromJson(R"json({
      "record_mode": "record-continuously"
    })json"),
                               /*privacy_filtering_enabled*/ true);
  EXPECT_EQ(PrintConfigToText(GetDataSourceConfig(
                trace_config, "org.chromium.trace_metadata")),
            PrintConfigToText(GetDataSourceConfig(
                perfetto_config, "org.chromium.trace_metadata")));
}

TEST_F(AdaptPerfettoConfigForChromeTest, DiscardBuffer) {
  auto perfetto_config = ParsePerfettoConfigFromText(R"pb(
    buffers: { fill_policy: DISCARD size_kb: 42 }
    data_sources: { config: { name: "org.chromium.trace_metadata" } }
  )pb");
  EXPECT_TRUE(AdaptPerfettoConfigForChrome(&perfetto_config));
}

TEST_F(AdaptPerfettoConfigForChromeTest, MultipleBuffers) {
  auto perfetto_config = ParsePerfettoConfigFromText(R"pb(
    buffers: { fill_policy: RING_BUFFER size_kb: 42 }
    buffers: { fill_policy: DISCARD size_kb: 42 }
    data_sources: { config: { name: "org.chromium.trace_metadata" } }
  )pb");
  EXPECT_TRUE(AdaptPerfettoConfigForChrome(&perfetto_config));
}

TEST_F(AdaptPerfettoConfigForChromeTest, ProcessFilter) {
  auto perfetto_config = ParsePerfettoConfigFromText(R"pb(
    data_sources: {
      config: {
        name: "track_event"
        track_event_config: {
          enabled_categories: [ "foo", "__metadata" ]
          disabled_categories: [ "*" ]
        }
      }
      producer_name_filter: "org.chromium-3"
    }
    data_sources: { config: { name: "org.chromium.trace_metadata" } }
  )pb");
  auto trace_config = GetDefaultPerfettoConfig(ParseTraceConfigFromJson(R"json({
      "record_mode": "record-continuously",
      "included_categories": ["foo"],
      "included_process_ids": [3]
    })json"));
  EXPECT_TRUE(AdaptPerfettoConfigForChrome(&perfetto_config));
  EXPECT_EQ(PrintConfigToText(trace_config),
            PrintConfigToText(perfetto_config));
}

#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CASTOS)
TEST_F(AdaptPerfettoConfigForChromeTest, Systrace) {
  auto perfetto_config = ParsePerfettoConfigFromText(R"pb(
    data_sources: { config: { name: "org.chromium.trace_system" } }
  )pb");
  auto trace_config = GetDefaultPerfettoConfig(ParseTraceConfigFromJson(R"json({
      "record_mode": "record-continuously",
      "enable_systrace": true
    })json"));
  EXPECT_TRUE(AdaptPerfettoConfigForChrome(&perfetto_config));
  EXPECT_EQ(PrintConfigToText(
                GetDataSourceConfig(trace_config, "org.chromium.trace_system")),
            PrintConfigToText(GetDataSourceConfig(
                perfetto_config, "org.chromium.trace_system")));
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CASTOS)

}  // namespace tracing
