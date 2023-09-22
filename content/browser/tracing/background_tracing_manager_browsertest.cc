// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_reader.h"
#include "base/memory/ref_counted_memory.h"
#include "base/metrics/histogram_macros.h"
#include "base/path_service.h"
#include "base/process/process_handle.h"
#include "base/profiler/module_cache.h"
#include "base/run_loop.h"
#include "base/strings/pattern.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_tokenizer.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_proto_loader.h"
#include "base/test/trace_event_analyzer.h"
#include "base/threading/thread_restrictions.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "components/tracing/common/trace_startup_config.h"
#include "content/browser/devtools/protocol/devtools_protocol_test_support.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/tracing/background_startup_tracing_observer.h"
#include "content/browser/tracing/background_tracing_active_scenario.h"
#include "content/browser/tracing/background_tracing_manager_impl.h"
#include "content/browser/tracing/background_tracing_rule.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/background_tracing_test_support.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/dns/mock_host_resolver.h"
#include "services/tracing/perfetto/privacy_filtering_check.h"
#include "services/tracing/public/cpp/stack_sampling/tracing_sampler_profiler.h"
#include "services/tracing/public/cpp/tracing_features.h"
#include "third_party/perfetto/include/perfetto/ext/trace_processor/export_json.h"
#include "third_party/perfetto/include/perfetto/trace_processor/trace_processor_storage.h"
#include "third_party/re2/src/re2/re2.h"
#include "third_party/zlib/google/compression_utils.h"
#include "third_party/zlib/zlib.h"

#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
// Used by PerfettoSystemBackgroundScenario, nogncheck is required because this
// is only included and used on Android.
#include "services/tracing/perfetto/system_test_utils.h"  // nogncheck
#include "services/tracing/perfetto/test_utils.h"         // nogncheck

namespace content {
namespace {
perfetto::TraceConfig StopTracingTriggerConfig(
    const std::string& trigger_name) {
  perfetto::TraceConfig trace_config;
  trace_config.add_buffers()->set_size_kb(1024);
  auto* ds_config = trace_config.add_data_sources()->mutable_config();
  ds_config->set_name("org.chromium.trace_event");
  ds_config = trace_config.add_data_sources()->mutable_config();
  ds_config->set_name("org.chromium.trace_metadata");
  auto* trigger_cfg = trace_config.mutable_trigger_config();
  trigger_cfg->set_trigger_mode(
      perfetto::TraceConfig::TriggerConfig::STOP_TRACING);
  trigger_cfg->set_trigger_timeout_ms(15000);
  auto* trigger = trigger_cfg->add_triggers();
  trigger->set_name(trigger_name);
  trigger->set_stop_delay_ms(1);
  return trace_config;
}

void SetSystemProducerSocketAndChecksAsync(const std::string& producer_socket) {
  // We need to let the PosixSystemProducer know the MockSystemService socket
  // address and that if we're running on Android devices older then Pie to
  // still connect.
  tracing::PerfettoTracedProcess::GetTaskRunner()
      ->GetOrCreateTaskRunner()
      ->PostTask(
          FROM_HERE,
          base::BindOnce(
              [](const std::string& producer_socket) {
                // The only other type of system producer is
                // PosixSystemProducer so this assert ensures that the
                // static_cast below is safe.
                ASSERT_FALSE(tracing::PerfettoTracedProcess::Get()
                                 ->system_producer()
                                 ->IsDummySystemProducerForTesting());
                auto* producer = static_cast<tracing::PosixSystemProducer*>(
                    tracing::PerfettoTracedProcess::Get()->system_producer());
                producer->SetNewSocketForTesting(producer_socket.c_str());
                producer->SetDisallowPreAndroidPieForTesting(false);
              },
              producer_socket));
}

std::unique_ptr<tracing::MockConsumer> CreateDefaultConsumer(
    perfetto::TraceConfig trace_config,
    perfetto::TracingService* service,
    base::RunLoop* no_more_packets) {
  return std::unique_ptr<tracing::MockConsumer>(new tracing::MockConsumer(
      {"org.chromium.trace_event", "org.chromium.trace_metadata"}, service,
      [no_more_packets](bool has_more) {
        if (!has_more) {
          no_more_packets->Quit();
        }
      },
      std::move(trace_config)));
}
}  // namespace
}  // namespace content
#endif  // BUILDFLAG(IS_POSIX) && !BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)

using base::trace_event::TraceLog;

namespace content {
namespace {

using testing::_;

class TestStartupPreferenceManagerImpl
    : public BackgroundStartupTracingObserver::PreferenceManager {
 public:
  void SetBackgroundStartupTracingEnabled(bool enabled) override {
    enabled_ = enabled;
  }

  bool GetBackgroundStartupTracingEnabled() const override { return enabled_; }

 private:
  bool enabled_ = false;
};

// An helper class that observes tracing states transition and allows
// synchronisation with tests. The class adds itself as a tracelog
// enable state observer and provides methods to wait for a given state.
//
// Usage:
//   TestTraceLogHelper tracelog_helper;
//   [... start tracing ...]
//   tracelog_helper.WaitForStartTracing();
//   [... stop tracing ...]
//   tracing_controller->StopTracing();
//   tracelog_helper.WaitForStopTracing();
class TestTraceLogHelper : public TraceLog::EnabledStateObserver {
 public:
  TestTraceLogHelper() {
    EXPECT_FALSE(TraceLog::GetInstance()->IsEnabled());
    TraceLog::GetInstance()->AddEnabledStateObserver(this);
  }

  ~TestTraceLogHelper() override {
    EXPECT_FALSE(TraceLog::GetInstance()->IsEnabled());
    TraceLog::GetInstance()->RemoveEnabledStateObserver(this);

    // Ensures tracing got enabled/disabled only once.
    EXPECT_EQ(1, enable_count);
    EXPECT_EQ(1, disable_count);
  }

  void OnTraceLogEnabled() override {
    wait_for_start_tracing_.QuitWhenIdle();
    enable_count++;
  }

  void OnTraceLogDisabled() override {
    wait_for_stop_tracing_.QuitWhenIdle();
    disable_count++;
  }

  void WaitForStartTracing() { wait_for_start_tracing_.Run(); }
  void WaitForStopTracing() { wait_for_stop_tracing_.Run(); }

 private:
  int enable_count = 0;
  int disable_count = 0;
  base::RunLoop wait_for_start_tracing_;
  base::RunLoop wait_for_stop_tracing_;
};

// A helper class that observes background tracing states transition, receives
// uploaded trace and allows synchronisation with tests. The class adds itself
// as a background tracing enabled state observer. It provides methods to wait
// for a given state.
//
// Usage:
//   TestBackgroundTracingHelper background_tracing_helper;
//   [... set a background tracing scenario ...]
//   [... trigger an event ...]
//   background_tracing_helper->WaitForTraceStarted();
//   background_tracing_helper->ExpectOnScenarioIdle("scenario_name");
//   [... abort ...]
//   background_tracing_helper->WaitForScenarioIdle();
class TestBackgroundTracingHelper
    : public BackgroundTracingManager::EnabledStateTestObserver,
      public perfetto::trace_processor::json::OutputWriter {
 public:
  TestBackgroundTracingHelper() {
    BackgroundTracingManagerImpl::GetInstance()
        .AddEnabledStateObserverForTesting(this);
  }

  ~TestBackgroundTracingHelper() override {
    BackgroundTracingManagerImpl::GetInstance()
        .RemoveEnabledStateObserverForTesting(this);
  }

  MOCK_METHOD(void,
              OnScenarioActive,
              (const std::string& scenario_name),
              (override));
  MOCK_METHOD(void,
              OnScenarioIdle,
              (const std::string& scenario_name),
              (override));
  void OnTraceStarted() override { wait_for_trace_started_.Quit(); }
  void OnTraceReceived(const std::string& proto_content) override {
    ProcessTraceContent(proto_content);
  }
  void OnTraceSaved() override { wait_for_trace_saved_.Quit(); }

  void ExpectOnScenarioActive(const std::string& scenario_name) {
    EXPECT_CALL(*this, OnScenarioActive(scenario_name)).Times(1);
  }
  void ExpectOnScenarioIdle(const std::string& scenario_name) {
    EXPECT_CALL(*this, OnScenarioIdle(scenario_name))
        .WillOnce(
            base::test::RunOnceClosure(wait_for_scenario_idle_.QuitClosure()));
  }
  void WaitForScenarioIdle() { wait_for_scenario_idle_.Run(); }
  void WaitForTraceStarted() { wait_for_trace_started_.Run(); }
  void WaitForTraceReceived() { wait_for_trace_received_.Run(); }
  void WaitForTraceSaved() { wait_for_trace_saved_.Run(); }

  bool trace_received() const { return trace_received_; }
  const std::string& json_file_contents() const { return json_file_contents_; }
  const std::string& proto_file_contents() const {
    return proto_file_contents_;
  }
  bool TraceHasMatchingString(const char* text) const {
    return json_file_contents_.find(text) != std::string::npos;
  }

 private:
  void ProcessTraceContent(const std::string& file_contents) {
    EXPECT_FALSE(trace_received_);
    trace_received_ = true;
    proto_file_contents_ = file_contents;

    std::unique_ptr<perfetto::trace_processor::TraceProcessorStorage>
        trace_processor =
            perfetto::trace_processor::TraceProcessorStorage::CreateInstance(
                perfetto::trace_processor::Config());

    size_t data_length = proto_file_contents_.length();
    std::unique_ptr<uint8_t[]> data(new uint8_t[data_length]);
    memcpy(data.get(), proto_file_contents_.data(), data_length);

    auto parse_status = trace_processor->Parse(std::move(data), data_length);
    ASSERT_TRUE(parse_status.ok()) << parse_status.message();

    trace_processor->NotifyEndOfFile();

    auto export_status = perfetto::trace_processor::json::ExportJson(
        trace_processor.get(), this,
        perfetto::trace_processor::json::ArgumentFilterPredicate(),
        perfetto::trace_processor::json::MetadataFilterPredicate(),
        perfetto::trace_processor::json::LabelFilterPredicate());
    ASSERT_TRUE(export_status.ok()) << export_status.message();

    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, wait_for_trace_received_.QuitWhenIdleClosure());
  }

  // perfetto::trace_processor::json::OutputWriter
  perfetto::trace_processor::util::Status AppendString(
      const std::string& json) override {
    json_file_contents_ += json;
    return perfetto::trace_processor::util::OkStatus();
  }

  base::RunLoop wait_for_scenario_idle_;
  base::RunLoop wait_for_trace_started_;
  base::RunLoop wait_for_trace_received_;
  base::RunLoop wait_for_trace_saved_;

  bool trace_received_ = false;
  std::string proto_file_contents_;
  std::string json_file_contents_;
};

perfetto::protos::gen::ChromeFieldTracingConfig ParseFieldTracingConfigFromText(
    const std::string& proto_text) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::TestProtoLoader config_loader(
      base::PathService::CheckedGet(base::DIR_GEN_TEST_DATA_ROOT)
          .Append(
              FILE_PATH_LITERAL("third_party/perfetto/protos/perfetto/"
                                "config/chrome/scenario_config.descriptor")),
      "perfetto.protos.ChromeFieldTracingConfig");
  std::string serialized_message;
  config_loader.ParseFromText(proto_text, serialized_message);
  perfetto::protos::gen::ChromeFieldTracingConfig destination;
  destination.ParseFromString(serialized_message);
  return destination;
}

}  // namespace

class BackgroundTracingManagerBrowserTest : public ContentBrowserTest {
 public:
  BackgroundTracingManagerBrowserTest() {
    feature_list_.InitWithFeatures(
        /* enabled_features = */ {features::kEnablePerfettoSystemTracing},
        /* disabled_features = */ {});
    // CreateUniqueTempDir() makes a blocking call to create the directory and
    // wait on it. This isn't allowed in a normal browser context. Therefore we
    // do this in the test constructor before the browser prevents the blocking
    // call.
    CHECK(tmp_dir_.CreateUniqueTempDir());
    // browser_tests disables system tracing by default. This test needs to
    // override the setting to exercise the feature.
    tracing::PerfettoTracedProcess::SetSystemProducerEnabledForTesting(true);
  }

  BackgroundTracingManagerBrowserTest(
      const BackgroundTracingManagerBrowserTest&) = delete;
  BackgroundTracingManagerBrowserTest& operator=(
      const BackgroundTracingManagerBrowserTest&) = delete;

  void PreRunTestOnMainThread() override {
    BackgroundTracingManagerImpl::GetInstance()
        .InvalidateTriggersCallbackForTesting();

    ContentBrowserTest::PreRunTestOnMainThread();
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ContentBrowserTest::SetUpOnMainThread();
  }

  const base::ScopedTempDir& tmp_dir() const { return tmp_dir_; }

 private:
  base::ScopedTempDir tmp_dir_;
  base::test::ScopedFeatureList feature_list_;
};

std::unique_ptr<BackgroundTracingConfig> CreatePreemptiveConfig() {
  base::Value::Dict dict;

  dict.Set("mode", "PREEMPTIVE_TRACING_MODE");
  dict.Set("custom_categories",
           base::StrCat({tracing::TraceStartupConfig::kDefaultStartupCategories,
                         ",log"}));

  base::Value::List rules_list;
  {
    base::Value::Dict rules_dict;
    rules_dict.Set("rule", "MONITOR_AND_DUMP_WHEN_TRIGGER_NAMED");
    rules_dict.Set("trigger_name", "preemptive_test");
    rules_list.Append(std::move(rules_dict));
  }
  dict.Set("configs", std::move(rules_list));

  std::unique_ptr<BackgroundTracingConfig> config(
      BackgroundTracingConfigImpl::FromDict(std::move(dict)));

  EXPECT_TRUE(config);
  return config;
}

std::unique_ptr<BackgroundTracingConfig> CreateReactiveConfig() {
  base::Value::Dict dict;

  dict.Set("mode", "REACTIVE_TRACING_MODE");
  dict.Set("custom_categories",
           tracing::TraceStartupConfig::kDefaultStartupCategories);

  base::Value::List rules_list;
  {
    base::Value::Dict rules_dict;
    rules_dict.Set("rule", "MONITOR_AND_DUMP_WHEN_TRIGGER_NAMED");
    rules_dict.Set("trigger_name", "reactive_test");
    rules_dict.Set("trigger_delay", 15);
    rules_list.Append(std::move(rules_dict));
  }
  dict.Set("configs", std::move(rules_list));

  std::unique_ptr<BackgroundTracingConfig> config(
      BackgroundTracingConfigImpl::FromDict(std::move(dict)));

  EXPECT_TRUE(config);
  return config;
}

std::unique_ptr<BackgroundTracingConfig> CreateSystemConfig() {
  base::Value::Dict dict;
  dict.Set("mode", "SYSTEM_TRACING_MODE");
  dict.Set("custom_categories",
           tracing::TraceStartupConfig::kDefaultStartupCategories);

  base::Value::List rules_list;
  {
    base::Value::Dict rules_dict;
    rules_dict.Set("rule", "MONITOR_AND_DUMP_WHEN_TRIGGER_NAMED");
    rules_dict.Set("trigger_name", "system_test");
    rules_list.Append(std::move(rules_dict));
  }
  {
    base::Value::Dict rules_dict;
    rules_dict.Set("rule", "MONITOR_AND_DUMP_WHEN_TRIGGER_NAMED");
    rules_dict.Set("trigger_name", "system_test_with_rule_id");
    rules_dict.Set("rule_id", "rule_id_override");
    rules_list.Append(std::move(rules_dict));
  }
  dict.Set("configs", std::move(rules_list));
  std::unique_ptr<BackgroundTracingConfig> config(
      BackgroundTracingConfigImpl::FromDict(std::move(dict)));

  EXPECT_TRUE(config);
  return config;
}

IN_PROC_BROWSER_TEST_F(BackgroundTracingManagerBrowserTest,
                       StartUploadScenario) {
  TestBackgroundTracingHelper background_tracing_helper;
  constexpr const char kScenarioConfig[] = R"pb(
    scenarios: {
      scenario_name: "test_scenario"
      start_rules: {
        name: "start_trigger"
        manual_trigger_name: "start_trigger"
      }
      upload_rules: {
        name: "upload_trigger"
        manual_trigger_name: "upload_trigger"
      }
      trace_config: {
        data_sources: { config: { name: "org.chromium.trace_metadata" } }
      }
    }
  )pb";
  BackgroundTracingManager::GetInstance().InitializeScenarios(
      ParseFieldTracingConfigFromText(kScenarioConfig),
      BackgroundTracingManager::NO_DATA_FILTERING);

  background_tracing_helper.ExpectOnScenarioActive("test_scenario");
  EXPECT_TRUE(BackgroundTracingManager::EmitNamedTrigger("start_trigger"));
  background_tracing_helper.WaitForTraceStarted();

  background_tracing_helper.ExpectOnScenarioIdle("test_scenario");
  EXPECT_TRUE(BackgroundTracingManager::EmitNamedTrigger("upload_trigger"));
  background_tracing_helper.WaitForScenarioIdle();

  background_tracing_helper.WaitForTraceReceived();
  EXPECT_TRUE(background_tracing_helper.trace_received());
}

IN_PROC_BROWSER_TEST_F(BackgroundTracingManagerBrowserTest,
                       StartInvalidScenario) {
  TestBackgroundTracingHelper background_tracing_helper;
  constexpr const char kScenarioConfig[] = R"pb(
    scenarios: {
      scenario_name: "test_scenario"
      start_rules: {
        name: "start_trigger"
        manual_trigger_name: "start_trigger"
      }
      trace_config: {
        data_sources: { config: { name: "Invalid" target_buffer: 1 } }
      }
    }
  )pb";
  BackgroundTracingManager::GetInstance().InitializeScenarios(
      ParseFieldTracingConfigFromText(kScenarioConfig),
      BackgroundTracingManager::NO_DATA_FILTERING);

  background_tracing_helper.ExpectOnScenarioActive("test_scenario");
  background_tracing_helper.ExpectOnScenarioIdle("test_scenario");
  EXPECT_TRUE(BackgroundTracingManager::EmitNamedTrigger("start_trigger"));

  background_tracing_helper.WaitForScenarioIdle();
}

// This tests that non-allowlisted args get stripped if required.
IN_PROC_BROWSER_TEST_F(BackgroundTracingManagerBrowserTest,
                       NotAllowlistedArgsStripped) {
  TestBackgroundTracingHelper background_tracing_helper;

  constexpr const char kScenarioConfig[] = R"pb(
    scenarios: {
      scenario_name: "test_scenario"
      start_rules: {
        name: "start_trigger"
        manual_trigger_name: "start_trigger"
      }
      upload_rules: {
        name: "upload_trigger"
        manual_trigger_name: "upload_trigger"
      }
      trace_config: {
        data_sources: {
          config: {
            name: "track_event"
            track_event_config: {
              disabled_categories: [ "*" ],
              enabled_categories: [ "toplevel", "startup" ]
            }
          }
        }
      }
    }
  )pb";
  BackgroundTracingManager::GetInstance().InitializeScenarios(
      ParseFieldTracingConfigFromText(kScenarioConfig),
      BackgroundTracingManager::ANONYMIZE_DATA);
  background_tracing_helper.ExpectOnScenarioActive("test_scenario");
  EXPECT_TRUE(BackgroundTracingManager::EmitNamedTrigger("start_trigger"));
  background_tracing_helper.WaitForTraceStarted();

  {
    TRACE_EVENT1("toplevel", "ThreadPool_RunTask", "src_file", "abc");
    TRACE_EVENT1("startup", "TestNotAllowlist", "test_not_allowlist", "abc");
  }

  background_tracing_helper.ExpectOnScenarioIdle("test_scenario");
  EXPECT_TRUE(BackgroundTracingManager::EmitNamedTrigger("upload_trigger"));
  background_tracing_helper.WaitForScenarioIdle();

  background_tracing_helper.WaitForTraceReceived();
  EXPECT_TRUE(background_tracing_helper.trace_received());
  EXPECT_TRUE(background_tracing_helper.TraceHasMatchingString("{"));
  EXPECT_TRUE(background_tracing_helper.TraceHasMatchingString("src_file"));
  EXPECT_FALSE(
      background_tracing_helper.TraceHasMatchingString("test_not_allowlist"));
}

IN_PROC_BROWSER_TEST_F(BackgroundTracingManagerBrowserTest,
                       StartOtherScenario) {
  TestBackgroundTracingHelper observer;
  constexpr const char kScenarioConfig[] = R"pb(
    scenarios: {
      scenario_name: "test_scenario"
      start_rules: {
        name: "start_trigger"
        manual_trigger_name: "start_trigger"
      }
      stop_rules: { name: "stop_trigger" manual_trigger_name: "stop_trigger" }
      trace_config: {
        data_sources: { config: { name: "org.chromium.trace_metadata" } }
      }
    }
    scenarios: {
      scenario_name: "other_scenario"
      start_rules: {
        name: "start_trigger"
        manual_trigger_name: "other_start_trigger"
      }
      trace_config: {
        data_sources: { config: { name: "org.chromium.trace_metadata" } }
      }
    }
  )pb";
  BackgroundTracingManager::GetInstance().InitializeScenarios(
      ParseFieldTracingConfigFromText(kScenarioConfig),
      BackgroundTracingManager::NO_DATA_FILTERING);

  observer.ExpectOnScenarioActive("test_scenario");
  EXPECT_TRUE(BackgroundTracingManager::EmitNamedTrigger("start_trigger"));

  EXPECT_FALSE(BackgroundTracingManager::EmitNamedTrigger("other_scenario"));

  observer.ExpectOnScenarioIdle("test_scenario");
  EXPECT_TRUE(BackgroundTracingManager::EmitNamedTrigger("stop_trigger"));
  observer.WaitForScenarioIdle();

  observer.ExpectOnScenarioActive("other_scenario");
  EXPECT_TRUE(
      BackgroundTracingManager::EmitNamedTrigger("other_start_trigger"));
}

// This tests that the endpoint receives the final trace data.
IN_PROC_BROWSER_TEST_F(BackgroundTracingManagerBrowserTest,
                       ReceiveTraceFinalContentsOnTrigger) {
  TestBackgroundTracingHelper background_tracing_helper;

  std::unique_ptr<BackgroundTracingConfig> config = CreatePreemptiveConfig();

  EXPECT_TRUE(BackgroundTracingManager::GetInstance().SetActiveScenario(
      std::move(config), BackgroundTracingManager::NO_DATA_FILTERING));
  background_tracing_helper.WaitForTraceStarted();

  EXPECT_TRUE(BackgroundTracingManager::EmitNamedTrigger("preemptive_test"));
  background_tracing_helper.WaitForTraceReceived();

  background_tracing_helper.ExpectOnScenarioIdle("");
  BackgroundTracingManager::GetInstance().AbortScenarioForTesting();
  background_tracing_helper.WaitForScenarioIdle();

  EXPECT_TRUE(background_tracing_helper.trace_received());
}

// This tests triggering more than once still only gathers once.
IN_PROC_BROWSER_TEST_F(BackgroundTracingManagerBrowserTest,
                       CallTriggersMoreThanOnceOnlyGatherOnce) {
  TestBackgroundTracingHelper background_tracing_helper;

  std::unique_ptr<BackgroundTracingConfig> config = CreatePreemptiveConfig();

  EXPECT_TRUE(BackgroundTracingManager::GetInstance().SetActiveScenario(
      std::move(config), BackgroundTracingManager::NO_DATA_FILTERING));

  background_tracing_helper.WaitForTraceStarted();

  EXPECT_TRUE(BackgroundTracingManager::EmitNamedTrigger("preemptive_test"));
  EXPECT_FALSE(BackgroundTracingManager::EmitNamedTrigger("preemptive_test"));

  background_tracing_helper.WaitForTraceReceived();
  background_tracing_helper.ExpectOnScenarioIdle("");
  BackgroundTracingManager::GetInstance().AbortScenarioForTesting();
  background_tracing_helper.WaitForScenarioIdle();

  EXPECT_TRUE(background_tracing_helper.trace_received());
}

// This tests that non-allowlisted args get stripped if required.
IN_PROC_BROWSER_TEST_F(BackgroundTracingManagerBrowserTest,
                       LegacyNotAllowlistedArgsStripped) {
  TestBackgroundTracingHelper background_tracing_helper;

  std::unique_ptr<BackgroundTracingConfig> config = CreatePreemptiveConfig();

  EXPECT_TRUE(BackgroundTracingManager::GetInstance().SetActiveScenario(
      std::move(config), BackgroundTracingManager::ANONYMIZE_DATA));
  background_tracing_helper.WaitForTraceStarted();

  {
    TRACE_EVENT1("toplevel", "ThreadPool_RunTask", "src_file", "abc");
    TRACE_EVENT1("startup", "TestNotAllowlist", "test_not_allowlist", "abc");
  }

  EXPECT_TRUE(BackgroundTracingManager::EmitNamedTrigger("preemptive_test"));

  background_tracing_helper.WaitForTraceReceived();
  background_tracing_helper.ExpectOnScenarioIdle("");
  BackgroundTracingManager::GetInstance().AbortScenarioForTesting();
  background_tracing_helper.WaitForScenarioIdle();

  EXPECT_TRUE(background_tracing_helper.trace_received());
  EXPECT_TRUE(background_tracing_helper.TraceHasMatchingString("{"));
  EXPECT_TRUE(background_tracing_helper.TraceHasMatchingString("src_file"));
  EXPECT_FALSE(
      background_tracing_helper.TraceHasMatchingString("test_not_allowlist"));
}

// Regression test for https://crbug.com/1405341.
// Tests that RenderFrameHostImpl destruction is finished without crashing when
// tracing is enabled.
IN_PROC_BROWSER_TEST_F(BackgroundTracingManagerBrowserTest,
                       TracingRenderFrameHostImplDtor) {
  ASSERT_TRUE(embedded_test_server()->Start());

  TestBackgroundTracingHelper background_tracing_helper;

  std::unique_ptr<BackgroundTracingConfig> config =
      BackgroundTracingConfigImpl::FromDict(base::JSONReader::Read(R"JSON(
        {
          "mode": "PREEMPTIVE_TRACING_MODE",
          "custom_categories": "content",
          "configs": [
            {
              "rule": "MONITOR_AND_DUMP_WHEN_TRIGGER_NAMED",
              "trigger_name": "content_test"
            }
          ]
        }
      )JSON")
                                                .value()
                                                .TakeDict());

  EXPECT_TRUE(BackgroundTracingManager::GetInstance().SetActiveScenario(
      std::move(config), BackgroundTracingManager::NO_DATA_FILTERING));

  background_tracing_helper.WaitForTraceStarted();

  EXPECT_TRUE(BackgroundTracingManager::EmitNamedTrigger("content_test"));

  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("a.com", "/title1.html")));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  auto* rfhi = static_cast<RenderFrameHostImpl*>(
      shell()->web_contents()->GetPrimaryMainFrame());

  // Audible audio output should cause the media stream count to increment.
  rfhi->OnAudibleStateChanged(true);

  RenderFrameDeletedObserver delete_frame(rfhi);

  // The old RenderFrameHost might have entered the BackForwardCache. Disable
  // back-forward cache to ensure that the RenderFrameHost gets deleted.
  DisableBackForwardCacheForTesting(shell()->web_contents(),
                                    BackForwardCache::TEST_REQUIRES_NO_CACHING);

  GURL cross_site_url = embedded_test_server()->GetURL("b.com", "/title2.html");
  EXPECT_TRUE(NavigateToURL(shell(), cross_site_url));
  delete_frame.WaitUntilDeleted();

  background_tracing_helper.WaitForTraceReceived();
  background_tracing_helper.ExpectOnScenarioIdle("");
  BackgroundTracingManager::GetInstance().AbortScenarioForTesting();
  background_tracing_helper.WaitForScenarioIdle();

  EXPECT_TRUE(background_tracing_helper.trace_received());
}

// Tests that events emitted by the browser process immediately after the
// SetActiveScenarioWithReceiveCallback call does get included in the trace,
// without waiting for the full WaitForTraceStarted() callback (background
// tracing will directly enable the TraceLog so we get events prior to waiting
// for the whole IPC sequence to enable tracing coming back from the tracing
// service). Temporarily disabled startup tracing on Android to be able to
// unblock Perfetto-based background tracing: https://crbug.com/941318
// Also disabled in Perfetto SDK build because startup tracing is started
// asynchronously.
// TODO(khokhlov): Re-enable when background tracing is switched to synchronous
// start.
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
#define MAYBE_EarlyTraceEventsInTrace DISABLED_EarlyTraceEventsInTrace
#else
#define MAYBE_EarlyTraceEventsInTrace EarlyTraceEventsInTrace
#endif
IN_PROC_BROWSER_TEST_F(BackgroundTracingManagerBrowserTest,
                       MAYBE_EarlyTraceEventsInTrace) {
  TestBackgroundTracingHelper background_tracing_helper;

  std::unique_ptr<BackgroundTracingConfig> config = CreatePreemptiveConfig();

  EXPECT_TRUE(BackgroundTracingManager::GetInstance().SetActiveScenario(
      std::move(config), BackgroundTracingManager::ANONYMIZE_DATA));

  { TRACE_EVENT0("benchmark", "TestEarlyEvent"); }

  background_tracing_helper.WaitForTraceStarted();

  EXPECT_TRUE(BackgroundTracingManager::EmitNamedTrigger("preemptive_test"));

  background_tracing_helper.WaitForTraceReceived();
  background_tracing_helper.ExpectOnScenarioIdle("");
  BackgroundTracingManager::GetInstance().AbortScenarioForTesting();
  background_tracing_helper.WaitForScenarioIdle();

  EXPECT_TRUE(background_tracing_helper.trace_received());
  EXPECT_TRUE(background_tracing_helper.TraceHasMatchingString("{"));
  EXPECT_TRUE(
      background_tracing_helper.TraceHasMatchingString("TestEarlyEvent"));
}

// This tests that browser metadata gets included in the trace.
// TODO(crbug.com/1413115): Re-enable this test on TSAN builds.
#if BUILDFLAG(IS_LINUX) && defined(THREAD_SANITIZER)
#define MAYBE_TraceMetadataInTrace DISABLED_TraceMetadataInTrace
#else
#define MAYBE_TraceMetadataInTrace TraceMetadataInTrace
#endif
IN_PROC_BROWSER_TEST_F(BackgroundTracingManagerBrowserTest,
                       MAYBE_TraceMetadataInTrace) {
  TestBackgroundTracingHelper background_tracing_helper;

  std::unique_ptr<BackgroundTracingConfig> config = CreatePreemptiveConfig();

  EXPECT_TRUE(BackgroundTracingManager::GetInstance().SetActiveScenario(
      std::move(config), BackgroundTracingManager::NO_DATA_FILTERING));

  background_tracing_helper.WaitForTraceStarted();

  EXPECT_TRUE(BackgroundTracingManager::EmitNamedTrigger("preemptive_test"));

  background_tracing_helper.WaitForTraceReceived();
  background_tracing_helper.ExpectOnScenarioIdle("");
  BackgroundTracingManager::GetInstance().AbortScenarioForTesting();
  background_tracing_helper.WaitForScenarioIdle();

  EXPECT_TRUE(background_tracing_helper.trace_received());
  EXPECT_TRUE(background_tracing_helper.TraceHasMatchingString("cpu-brand"));
  EXPECT_TRUE(background_tracing_helper.TraceHasMatchingString("network-type"));
  EXPECT_TRUE(background_tracing_helper.TraceHasMatchingString("user-agent"));
}

// Flaky on android, linux, and windows: https://crbug.com/639706 and
// https://crbug.com/643415.
// This tests subprocesses (like a navigating renderer) which gets told to
// provide a argument-filtered trace and has no predicate in place to do the
// filtering (in this case, only the browser process gets it set), will crash
// rather than return potential PII.
IN_PROC_BROWSER_TEST_F(BackgroundTracingManagerBrowserTest,
                       DISABLED_CrashWhenSubprocessWithoutArgumentFilter) {
  TestBackgroundTracingHelper background_tracing_helper;

  std::unique_ptr<BackgroundTracingConfig> config = CreatePreemptiveConfig();

  EXPECT_TRUE(BackgroundTracingManager::GetInstance().SetActiveScenario(
      std::move(config), BackgroundTracingManager::ANONYMIZE_DATA));

  EXPECT_TRUE(NavigateToURL(shell(), GetTestUrl("", "about:blank")));

  EXPECT_TRUE(BackgroundTracingManager::EmitNamedTrigger("preemptive_test"));

  background_tracing_helper.WaitForTraceReceived();
  background_tracing_helper.ExpectOnScenarioIdle("");
  BackgroundTracingManager::GetInstance().AbortScenarioForTesting();
  background_tracing_helper.WaitForScenarioIdle();

  EXPECT_TRUE(background_tracing_helper.trace_received());
  // We should *not* receive anything at all from the renderer,
  // the process should've crashed rather than letting that happen.
  EXPECT_FALSE(
      background_tracing_helper.TraceHasMatchingString("CrRendererMain"));
}

// This tests multiple triggers still only gathers once.
IN_PROC_BROWSER_TEST_F(BackgroundTracingManagerBrowserTest,
                       CallMultipleTriggersOnlyGatherOnce) {
  TestBackgroundTracingHelper background_tracing_helper;

  base::Value::Dict dict;
  dict.Set("mode", "PREEMPTIVE_TRACING_MODE");
  dict.Set("custom_categories",
           tracing::TraceStartupConfig::kDefaultStartupCategories);

  base::Value::List rules_list;
  {
    base::Value::Dict rules_dict;
    rules_dict.Set("rule", "MONITOR_AND_DUMP_WHEN_TRIGGER_NAMED");
    rules_dict.Set("trigger_name", "test1");
    rules_list.Append(std::move(rules_dict));
  }
  {
    base::Value::Dict rules_dict;
    rules_dict.Set("rule", "MONITOR_AND_DUMP_WHEN_TRIGGER_NAMED");
    rules_dict.Set("trigger_name", "test2");
    rules_list.Append(std::move(rules_dict));
  }

  dict.Set("configs", std::move(rules_list));

  std::unique_ptr<BackgroundTracingConfig> config(
      BackgroundTracingConfigImpl::FromDict(std::move(dict)));
  EXPECT_TRUE(config);

  EXPECT_TRUE(BackgroundTracingManager::GetInstance().SetActiveScenario(
      std::move(config), BackgroundTracingManager::NO_DATA_FILTERING));

  background_tracing_helper.WaitForTraceStarted();

  EXPECT_TRUE(BackgroundTracingManager::EmitNamedTrigger("test1"));
  EXPECT_FALSE(BackgroundTracingManager::EmitNamedTrigger("test2"));

  background_tracing_helper.WaitForTraceReceived();
  background_tracing_helper.ExpectOnScenarioIdle("");
  BackgroundTracingManager::GetInstance().AbortScenarioForTesting();
  background_tracing_helper.WaitForScenarioIdle();

  EXPECT_TRUE(background_tracing_helper.trace_received());
}

// This tests that delayed histogram triggers work as expected
// with preemptive scenarios.
IN_PROC_BROWSER_TEST_F(BackgroundTracingManagerBrowserTest,
                       CallPreemptiveTriggerWithDelay) {
  TestBackgroundTracingHelper background_tracing_helper;

  base::Value::Dict dict;
  dict.Set("mode", "PREEMPTIVE_TRACING_MODE");
  dict.Set("custom_categories",
           tracing::TraceStartupConfig::kDefaultStartupCategories);

  base::Value::List rules_list;
  {
    base::Value::Dict rules_dict;
    rules_dict.Set("rule",
                   "MONITOR_AND_DUMP_WHEN_SPECIFIC_HISTOGRAM_AND_VALUE");
    rules_dict.Set("histogram_name", "fake");
    rules_dict.Set("histogram_value", 1);
    rules_dict.Set("trigger_delay", 10);
    rules_list.Append(std::move(rules_dict));
  }

  dict.Set("configs", std::move(rules_list));

  std::unique_ptr<BackgroundTracingConfig> config(
      BackgroundTracingConfigImpl::FromDict(std::move(dict)));
  EXPECT_TRUE(config);

  base::RunLoop rule_triggered_runloop;
  EXPECT_TRUE(BackgroundTracingManager::GetInstance().SetActiveScenario(
      std::move(config), BackgroundTracingManager::NO_DATA_FILTERING));

  background_tracing_helper.WaitForTraceStarted();

  BackgroundTracingManagerImpl::GetInstance()
      .GetActiveScenarioForTesting()
      ->SetRuleTriggeredCallbackForTesting(
          rule_triggered_runloop.QuitClosure());

  // Our reference value is "1", so a value of "2" should trigger a trace.
  LOCAL_HISTOGRAM_COUNTS("fake", 2);

  rule_triggered_runloop.Run();

  // Since we specified a delay in the scenario, we should still be tracing
  // at this point.
  EXPECT_TRUE(
      BackgroundTracingManagerImpl::GetInstance().IsTracingForTesting());

  // Fake the timer firing.
  BackgroundTracingManagerImpl::GetInstance()
      .GetActiveScenarioForTesting()
      ->FireTimerForTesting();

  background_tracing_helper.WaitForTraceReceived();

  background_tracing_helper.ExpectOnScenarioIdle("");
  BackgroundTracingManager::GetInstance().AbortScenarioForTesting();
  background_tracing_helper.WaitForScenarioIdle();

  EXPECT_TRUE(background_tracing_helper.trace_received());
}

// This tests that you can't trigger without a scenario set.
IN_PROC_BROWSER_TEST_F(BackgroundTracingManagerBrowserTest,
                       CannotTriggerWithoutScenarioSet) {
  EXPECT_FALSE(BackgroundTracingManager::EmitNamedTrigger("preemptive_test"));
}

// This tests that no trace is triggered with a handle that isn't specified
// in the config.
IN_PROC_BROWSER_TEST_F(BackgroundTracingManagerBrowserTest,
                       DoesNotTriggerWithWrongHandle) {
  TestBackgroundTracingHelper background_tracing_helper;

  std::unique_ptr<BackgroundTracingConfig> config = CreatePreemptiveConfig();

  EXPECT_TRUE(BackgroundTracingManager::GetInstance().SetActiveScenario(
      std::move(config), BackgroundTracingManager::NO_DATA_FILTERING));

  background_tracing_helper.WaitForTraceStarted();

  EXPECT_FALSE(BackgroundTracingManager::EmitNamedTrigger("does_not_exist"));

  // Abort the scenario.
  background_tracing_helper.ExpectOnScenarioIdle("");
  BackgroundTracingManager::GetInstance().AbortScenarioForTesting();
  background_tracing_helper.WaitForScenarioIdle();

  EXPECT_FALSE(background_tracing_helper.trace_received());
}

// This tests that no trace is triggered with an invalid handle.
IN_PROC_BROWSER_TEST_F(BackgroundTracingManagerBrowserTest,
                       DoesNotTriggerWithInvalidHandle) {
  TestBackgroundTracingHelper background_tracing_helper;

  std::unique_ptr<BackgroundTracingConfig> config = CreatePreemptiveConfig();

  EXPECT_TRUE(BackgroundTracingManager::GetInstance().SetActiveScenario(
      std::move(config), BackgroundTracingManager::NO_DATA_FILTERING));

  BackgroundTracingManagerImpl::GetInstance()
      .InvalidateTriggersCallbackForTesting();

  background_tracing_helper.WaitForTraceStarted();

  EXPECT_FALSE(BackgroundTracingManager::EmitNamedTrigger("preemptive_test"));

  // Abort the scenario.
  background_tracing_helper.ExpectOnScenarioIdle("");
  BackgroundTracingManager::GetInstance().AbortScenarioForTesting();
  background_tracing_helper.WaitForScenarioIdle();

  EXPECT_FALSE(background_tracing_helper.trace_received());
}

// This tests that no preemptive trace is triggered with 0 chance set.
IN_PROC_BROWSER_TEST_F(BackgroundTracingManagerBrowserTest,
                       PreemptiveNotTriggerWithZeroChance) {
  TestBackgroundTracingHelper background_tracing_helper;

  base::Value::Dict dict;
  dict.Set("mode", "PREEMPTIVE_TRACING_MODE");
  dict.Set("custom_categories",
           tracing::TraceStartupConfig::kDefaultStartupCategories);

  base::Value::List rules_list;
  {
    base::Value::Dict rules_dict;
    rules_dict.Set("rule", "MONITOR_AND_DUMP_WHEN_TRIGGER_NAMED");
    rules_dict.Set("trigger_name", "preemptive_test");
    rules_dict.Set("trigger_chance", 0.0);
    rules_list.Append(std::move(rules_dict));
  }
  dict.Set("configs", std::move(rules_list));

  std::unique_ptr<BackgroundTracingConfig> config(
      BackgroundTracingConfigImpl::FromDict(std::move(dict)));
  EXPECT_TRUE(config);

  EXPECT_TRUE(BackgroundTracingManager::GetInstance().SetActiveScenario(
      std::move(config), BackgroundTracingManager::NO_DATA_FILTERING));

  background_tracing_helper.WaitForTraceStarted();

  EXPECT_FALSE(BackgroundTracingManager::EmitNamedTrigger("preemptive_test"));

  // Abort the scenario.
  background_tracing_helper.ExpectOnScenarioIdle("");
  BackgroundTracingManager::GetInstance().AbortScenarioForTesting();
  background_tracing_helper.WaitForScenarioIdle();

  EXPECT_FALSE(background_tracing_helper.trace_received());
}

// This tests that no reactive trace is triggered with 0 chance set.
IN_PROC_BROWSER_TEST_F(BackgroundTracingManagerBrowserTest,
                       ReactiveNotTriggerWithZeroChance) {
  TestBackgroundTracingHelper background_tracing_helper;

  base::Value::Dict dict;
  dict.Set("mode", "REACTIVE_TRACING_MODE");
  dict.Set("custom_categories",
           tracing::TraceStartupConfig::kDefaultStartupCategories);

  base::Value::List rules_list;
  {
    base::Value::Dict rules_dict;
    rules_dict.Set("rule", "MONITOR_AND_DUMP_WHEN_TRIGGER_NAMED");
    rules_dict.Set("trigger_name", "reactive_test1");
    rules_dict.Set("trigger_chance", 0.0);

    rules_list.Append(std::move(rules_dict));
  }
  dict.Set("configs", std::move(rules_list));

  std::unique_ptr<BackgroundTracingConfig> config(
      BackgroundTracingConfigImpl::FromDict(std::move(dict)));
  EXPECT_TRUE(config);

  EXPECT_TRUE(BackgroundTracingManager::GetInstance().SetActiveScenario(
      std::move(config), BackgroundTracingManager::NO_DATA_FILTERING));

  EXPECT_FALSE(BackgroundTracingManager::EmitNamedTrigger("preemptive_test"));

  // Abort the scenario.
  background_tracing_helper.ExpectOnScenarioIdle("");
  BackgroundTracingManager::GetInstance().AbortScenarioForTesting();
  background_tracing_helper.WaitForScenarioIdle();

  EXPECT_FALSE(background_tracing_helper.trace_received());
}

// This tests that histogram triggers for preemptive mode configs.
// TODO(crbug.com/1429286): Flaky on Linux TSan.
#if (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)) && defined(THREAD_SANITIZER)
#define MAYBE_ReceiveTraceSucceedsOnHigherHistogramSample \
  DISABLED_ReceiveTraceSucceedsOnHigherHistogramSample
#else
#define MAYBE_ReceiveTraceSucceedsOnHigherHistogramSample \
  ReceiveTraceSucceedsOnHigherHistogramSample
#endif
IN_PROC_BROWSER_TEST_F(BackgroundTracingManagerBrowserTest,
                       MAYBE_ReceiveTraceSucceedsOnHigherHistogramSample) {
  TestBackgroundTracingHelper background_tracing_helper;

  base::Value::Dict dict;
  dict.Set("mode", "PREEMPTIVE_TRACING_MODE");
  dict.Set("custom_categories",
           tracing::TraceStartupConfig::kDefaultStartupCategories);

  base::Value::List rules_list;
  {
    base::Value::Dict rules_dict;
    rules_dict.Set("rule",
                   "MONITOR_AND_DUMP_WHEN_SPECIFIC_HISTOGRAM_AND_VALUE");
    rules_dict.Set("histogram_name", "fake");
    rules_dict.Set("histogram_value", 1);
    rules_list.Append(std::move(rules_dict));
  }

  dict.Set("configs", std::move(rules_list));

  std::unique_ptr<BackgroundTracingConfig> config(
      BackgroundTracingConfigImpl::FromDict(std::move(dict)));
  EXPECT_TRUE(config);

  EXPECT_TRUE(BackgroundTracingManager::GetInstance().SetActiveScenario(
      std::move(config), BackgroundTracingManager::NO_DATA_FILTERING));

  background_tracing_helper.WaitForTraceStarted();

  // Our reference value is "1", so a value of "2" should trigger a trace.
  LOCAL_HISTOGRAM_COUNTS("fake", 2);

  background_tracing_helper.WaitForTraceReceived();

  EXPECT_TRUE(background_tracing_helper.trace_received());

  absl::optional<base::Value> trace_json =
      base::JSONReader::Read(background_tracing_helper.json_file_contents());
  ASSERT_TRUE(trace_json);
  ASSERT_TRUE(trace_json->is_dict());
  auto* metadata_json = trace_json->GetDict().FindDict("metadata");
  ASSERT_TRUE(metadata_json);

  const std::string* trace_config = metadata_json->FindString("trace-config");
  ASSERT_TRUE(trace_config);
  EXPECT_NE(trace_config->find("record-continuously"), trace_config->npos)
      << *trace_config;

  EXPECT_TRUE(BackgroundTracingManager::GetInstance().HasActiveScenario());

  background_tracing_helper.ExpectOnScenarioIdle("");
  BackgroundTracingManager::GetInstance().AbortScenarioForTesting();
  background_tracing_helper.WaitForScenarioIdle();
}

// TODO(crbug.com/1227164): Test is flaky on Linux and Windows.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN)
#define MAYBE_CustomConfig DISABLED_CustomConfig
#else
#define MAYBE_CustomConfig CustomConfig
#endif
IN_PROC_BROWSER_TEST_F(BackgroundTracingManagerBrowserTest,
                       MAYBE_CustomConfig) {
  TestBackgroundTracingHelper background_tracing_helper;

  base::Value::Dict dict;
  dict.Set("mode", "PREEMPTIVE_TRACING_MODE");
  dict.Set("custom_categories",
           tracing::TraceStartupConfig::kDefaultStartupCategories);
  dict.Set("trace_config", std::move(*base::JSONReader::Read(R"(
        {
          "included_categories": ["*"],
          "record_mode": "record-until-full"
        })")));

  base::Value::List rules_list;
  {
    base::Value::Dict rules_dict;
    rules_dict.Set("rule",
                   "MONITOR_AND_DUMP_WHEN_SPECIFIC_HISTOGRAM_AND_VALUE");
    rules_dict.Set("histogram_name", "fake");
    rules_dict.Set("histogram_value", 1);
    rules_list.Append(std::move(rules_dict));
  }

  dict.Set("configs", std::move(rules_list));

  std::unique_ptr<BackgroundTracingConfig> config(
      BackgroundTracingConfigImpl::FromDict(std::move(dict)));
  EXPECT_TRUE(config);

  EXPECT_TRUE(BackgroundTracingManager::GetInstance().SetActiveScenario(
      std::move(config), BackgroundTracingManager::NO_DATA_FILTERING));

  background_tracing_helper.WaitForTraceStarted();

  // Our reference value is "1", so a value of "2" should trigger a trace.
  LOCAL_HISTOGRAM_COUNTS("fake", 2);

  background_tracing_helper.WaitForTraceReceived();
  background_tracing_helper.ExpectOnScenarioIdle("");
  BackgroundTracingManager::GetInstance().AbortScenarioForTesting();
  background_tracing_helper.WaitForScenarioIdle();

  EXPECT_TRUE(background_tracing_helper.trace_received());

  absl::optional<base::Value> trace_json =
      base::JSONReader::Read(background_tracing_helper.json_file_contents());
  ASSERT_TRUE(trace_json);
  ASSERT_TRUE(trace_json->is_dict());
  auto* metadata_json = trace_json->GetDict().FindDict("metadata");
  ASSERT_TRUE(metadata_json);

  const std::string* trace_config = metadata_json->FindString("trace-config");
  ASSERT_TRUE(trace_config);
  EXPECT_NE(trace_config->find("record-until-full"), trace_config->npos)
      << *trace_config;
}

// Used as a known symbol to look up the current module.
void DummyFunc() {}

// Test that the tracing sampler profiler running in background tracing mode,
// produces stack frames in the expected JSON format.
// TODO(https://crbug.com/1062581) Disabled for being flaky.
IN_PROC_BROWSER_TEST_F(BackgroundTracingManagerBrowserTest,
                       DISABLED_EndToEndStackSampling) {
  // In the browser process, the tracing sampler profiler gets constructed by
  // the chrome/ layer, so we need to do the same manually for testing purposes.
  auto tracing_sampler_profiler =
      tracing::TracingSamplerProfiler::CreateOnMainThread();

  // There won't be any samples if stack unwinding isn't supported.
  if (!tracing::TracingSamplerProfiler::IsStackUnwindingSupportedForTesting()) {
    return;
  }

  base::RunLoop wait_for_sample;
  tracing_sampler_profiler->SetSampleCallbackForTesting(
      wait_for_sample.QuitClosure());

  TestBackgroundTracingHelper background_tracing_helper;

  base::Value::Dict dict;
  dict.Set("mode", "PREEMPTIVE_TRACING_MODE");
  dict.Set("category", "CUSTOM");
  dict.Set("custom_categories", "disabled-by-default-cpu_profiler,-*");

  base::Value::List rules_list;
  {
    base::Value::Dict rules_dict;
    rules_dict.Set("rule", "MONITOR_AND_DUMP_WHEN_TRIGGER_NAMED");
    rules_dict.Set("trigger_name", "preemptive_test");
    rules_list.Append(std::move(rules_dict));
  }

  dict.Set("configs", std::move(rules_list));

  std::unique_ptr<BackgroundTracingConfig> config(
      BackgroundTracingConfigImpl::FromDict(std::move(dict)));
  EXPECT_TRUE(config);

  EXPECT_TRUE(BackgroundTracingManager::GetInstance().SetActiveScenario(
      std::move(config), BackgroundTracingManager::ANONYMIZE_DATA));

  background_tracing_helper.WaitForTraceStarted();

  wait_for_sample.Run();

  EXPECT_TRUE(BackgroundTracingManager::EmitNamedTrigger("preemptive_test"));

  background_tracing_helper.WaitForTraceReceived();
  background_tracing_helper.ExpectOnScenarioIdle("");
  BackgroundTracingManager::GetInstance().AbortScenarioForTesting();
  background_tracing_helper.WaitForScenarioIdle();

  EXPECT_TRUE(background_tracing_helper.trace_received());

  trace_analyzer::TraceEventVector events;
  std::unique_ptr<trace_analyzer::TraceAnalyzer> analyzer(
      trace_analyzer::TraceAnalyzer::Create(
          background_tracing_helper.json_file_contents()));
  ASSERT_TRUE(analyzer);

  base::ModuleCache module_cache;
  const base::ModuleCache::Module* this_module =
      module_cache.GetModuleForAddress(reinterpret_cast<uintptr_t>(&DummyFunc));
  ASSERT_TRUE(this_module);

  std::string module_id =
      base::TransformModuleIDToSymbolServerFormat(this_module->GetId());

  std::string desired_frame_pattern = base::StrCat(
      {"0x[[:xdigit:]]+ - /?", this_module->GetDebugBasename().MaybeAsASCII(),
       " \\[", module_id, "\\]"});

  analyzer->FindEvents(trace_analyzer::Query::EventName() ==
                           trace_analyzer::Query::String("StackCpuSampling"),
                       &events);
  EXPECT_GT(events.size(), 0u);

  bool found_match = false;
  for (const trace_analyzer::TraceEvent* event : events) {
    if (found_match) {
      break;
    }

    std::string frames = event->GetKnownArgAsString("frames");
    EXPECT_FALSE(frames.empty());
    base::StringTokenizer values_tokenizer(frames, "\n");
    while (values_tokenizer.GetNext()) {
      if (values_tokenizer.token_is_delim()) {
        continue;
      }

      if (RE2::FullMatch(values_tokenizer.token(), desired_frame_pattern)) {
        found_match = true;
        break;
      }
    }
  }

  EXPECT_TRUE(found_match);
}

// This tests that histogram triggers for reactive mode configs.
IN_PROC_BROWSER_TEST_F(BackgroundTracingManagerBrowserTest,
                       ReceiveReactiveTraceSucceedsOnHigherHistogramSample) {
  TestBackgroundTracingHelper background_tracing_helper;

  base::Value::Dict dict;
  dict.Set("mode", "REACTIVE_TRACING_MODE");
  dict.Set("custom_categories",
           tracing::TraceStartupConfig::kDefaultStartupCategories);

  base::Value::List rules_list;
  {
    base::Value::Dict rules_dict;
    rules_dict.Set("rule",
                   "MONITOR_AND_DUMP_WHEN_SPECIFIC_HISTOGRAM_AND_VALUE");
    rules_dict.Set("histogram_name", "fake");
    rules_dict.Set("histogram_value", 1);
    rules_list.Append(std::move(rules_dict));
  }

  dict.Set("configs", std::move(rules_list));

  std::unique_ptr<BackgroundTracingConfig> config(
      BackgroundTracingConfigImpl::FromDict(std::move(dict)));
  EXPECT_TRUE(config);

  EXPECT_TRUE(BackgroundTracingManager::GetInstance().SetActiveScenario(
      std::move(config), BackgroundTracingManager::NO_DATA_FILTERING));

  // Our reference value is "1", so a value of "2" should trigger a trace.
  LOCAL_HISTOGRAM_COUNTS("fake", 2);

  background_tracing_helper.WaitForTraceReceived();

  // Abort the scenario.
  background_tracing_helper.ExpectOnScenarioIdle("");
  BackgroundTracingManager::GetInstance().AbortScenarioForTesting();
  background_tracing_helper.WaitForScenarioIdle();

  EXPECT_TRUE(background_tracing_helper.trace_received());
}

// This tests that histogram values < reference value don't trigger.
IN_PROC_BROWSER_TEST_F(BackgroundTracingManagerBrowserTest,
                       ReceiveTraceFailsOnLowerHistogramSample) {
  TestBackgroundTracingHelper background_tracing_helper;

  base::Value::Dict dict;
  dict.Set("mode", "PREEMPTIVE_TRACING_MODE");
  dict.Set("custom_categories",
           tracing::TraceStartupConfig::kDefaultStartupCategories);

  base::Value::List rules_list;
  {
    base::Value::Dict rules_dict;
    rules_dict.Set("rule",
                   "MONITOR_AND_DUMP_WHEN_SPECIFIC_HISTOGRAM_AND_VALUE");
    rules_dict.Set("histogram_name", "fake");
    rules_dict.Set("histogram_value", 1);
    rules_list.Append(std::move(rules_dict));
  }

  dict.Set("configs", std::move(rules_list));

  std::unique_ptr<BackgroundTracingConfig> config(
      BackgroundTracingConfigImpl::FromDict(std::move(dict)));
  EXPECT_TRUE(config);

  EXPECT_TRUE(BackgroundTracingManager::GetInstance().SetActiveScenario(
      std::move(config), BackgroundTracingManager::NO_DATA_FILTERING));

  background_tracing_helper.WaitForTraceStarted();

  // This should fail to trigger a trace since the sample value < the
  // the reference value above.
  LOCAL_HISTOGRAM_COUNTS("fake", 0);

  // Abort the scenario.
  background_tracing_helper.ExpectOnScenarioIdle("");
  BackgroundTracingManager::GetInstance().AbortScenarioForTesting();
  background_tracing_helper.WaitForScenarioIdle();

  EXPECT_FALSE(background_tracing_helper.trace_received());
}

// This tests that histogram values > upper reference value don't trigger.
IN_PROC_BROWSER_TEST_F(BackgroundTracingManagerBrowserTest,
                       ReceiveTraceFailsOnHigherHistogramSample) {
  TestBackgroundTracingHelper background_tracing_helper;

  base::Value::Dict dict;
  dict.Set("mode", "PREEMPTIVE_TRACING_MODE");
  dict.Set("custom_categories",
           tracing::TraceStartupConfig::kDefaultStartupCategories);

  base::Value::List rules_list;
  {
    base::Value::Dict rules_dict;
    rules_dict.Set("rule",
                   "MONITOR_AND_DUMP_WHEN_SPECIFIC_HISTOGRAM_AND_VALUE");
    rules_dict.Set("histogram_name", "fake");
    rules_dict.Set("histogram_lower_value", 1);
    rules_dict.Set("histogram_upper_value", 3);
    rules_list.Append(std::move(rules_dict));
  }

  dict.Set("configs", std::move(rules_list));

  std::unique_ptr<BackgroundTracingConfig> config(
      BackgroundTracingConfigImpl::FromDict(std::move(dict)));
  EXPECT_TRUE(config);

  EXPECT_TRUE(BackgroundTracingManager::GetInstance().SetActiveScenario(
      std::move(config), BackgroundTracingManager::NO_DATA_FILTERING));

  background_tracing_helper.WaitForTraceStarted();

  // This should fail to trigger a trace since the sample value > the
  // the upper reference value above.
  LOCAL_HISTOGRAM_COUNTS("fake", 4);

  // Abort the scenario.
  background_tracing_helper.ExpectOnScenarioIdle("");
  BackgroundTracingManager::GetInstance().AbortScenarioForTesting();
  background_tracing_helper.WaitForScenarioIdle();

  EXPECT_FALSE(background_tracing_helper.trace_received());
}

// This tests that histogram values = upper reference value will trigger.
IN_PROC_BROWSER_TEST_F(BackgroundTracingManagerBrowserTest,
                       ReceiveTraceSucceedsOnUpperReferenceValue) {
  TestBackgroundTracingHelper background_tracing_helper;

  base::Value::Dict dict;
  dict.Set("mode", "PREEMPTIVE_TRACING_MODE");
  dict.Set("custom_categories",
           tracing::TraceStartupConfig::kDefaultStartupCategories);

  base::Value::List rules_list;
  {
    base::Value::Dict rules_dict;
    rules_dict.Set("rule",
                   "MONITOR_AND_DUMP_WHEN_SPECIFIC_HISTOGRAM_AND_VALUE");
    rules_dict.Set("histogram_name", "fake");
    rules_dict.Set("histogram_lower_value", 1);
    rules_dict.Set("histogram_upper_value", 3);
    rules_list.Append(std::move(rules_dict));
  }

  dict.Set("configs", std::move(rules_list));

  std::unique_ptr<BackgroundTracingConfig> config(
      BackgroundTracingConfigImpl::FromDict(std::move(dict)));
  EXPECT_TRUE(config);

  EXPECT_TRUE(BackgroundTracingManager::GetInstance().SetActiveScenario(
      std::move(config), BackgroundTracingManager::NO_DATA_FILTERING));

  background_tracing_helper.WaitForTraceStarted();

  LOCAL_HISTOGRAM_COUNTS("fake", 3);

  background_tracing_helper.WaitForTraceReceived();
  // Abort the scenario.
  background_tracing_helper.ExpectOnScenarioIdle("");
  BackgroundTracingManager::GetInstance().AbortScenarioForTesting();
  background_tracing_helper.WaitForScenarioIdle();

  EXPECT_TRUE(background_tracing_helper.trace_received());
}

// This tests that histogram values = lower reference value will trigger.
IN_PROC_BROWSER_TEST_F(BackgroundTracingManagerBrowserTest,
                       ReceiveTraceSucceedsOnLowerReferenceValue) {
  TestBackgroundTracingHelper background_tracing_helper;

  base::Value::Dict dict;
  dict.Set("mode", "PREEMPTIVE_TRACING_MODE");
  dict.Set("custom_categories",
           tracing::TraceStartupConfig::kDefaultStartupCategories);

  base::Value::List rules_list;
  {
    base::Value::Dict rules_dict;
    rules_dict.Set("rule",
                   "MONITOR_AND_DUMP_WHEN_SPECIFIC_HISTOGRAM_AND_VALUE");
    rules_dict.Set("histogram_name", "fake");
    rules_dict.Set("histogram_lower_value", 1);
    rules_dict.Set("histogram_upper_value", 3);
    rules_list.Append(std::move(rules_dict));
  }

  dict.Set("configs", std::move(rules_list));

  std::unique_ptr<BackgroundTracingConfig> config(
      BackgroundTracingConfigImpl::FromDict(std::move(dict)));
  EXPECT_TRUE(config);

  EXPECT_TRUE(BackgroundTracingManager::GetInstance().SetActiveScenario(
      std::move(config), BackgroundTracingManager::NO_DATA_FILTERING));

  background_tracing_helper.WaitForTraceStarted();

  LOCAL_HISTOGRAM_COUNTS("fake", 1);

  background_tracing_helper.WaitForTraceReceived();
  // Abort the scenario.
  background_tracing_helper.ExpectOnScenarioIdle("");
  BackgroundTracingManager::GetInstance().AbortScenarioForTesting();
  background_tracing_helper.WaitForScenarioIdle();

  EXPECT_TRUE(background_tracing_helper.trace_received());
}

// This tests that we can trigger for a single enum value.
IN_PROC_BROWSER_TEST_F(BackgroundTracingManagerBrowserTest,
                       ReceiveReactiveTraceSucceedsOnSingleEnumValue) {
  TestBackgroundTracingHelper background_tracing_helper;

  base::Value::Dict dict;
  dict.Set("mode", "PREEMPTIVE_TRACING_MODE");
  dict.Set("custom_categories",
           tracing::TraceStartupConfig::kDefaultStartupCategories);

  base::Value::List rules_list;
  {
    base::Value::Dict rules_dict;
    rules_dict.Set("rule",
                   "MONITOR_AND_DUMP_WHEN_SPECIFIC_HISTOGRAM_AND_VALUE");
    rules_dict.Set("histogram_name", "fake");
    rules_dict.Set("histogram_lower_value", 1);
    rules_dict.Set("histogram_upper_value", 1);
    rules_list.Append(std::move(rules_dict));
  }

  dict.Set("configs", std::move(rules_list));

  std::unique_ptr<BackgroundTracingConfig> config(
      BackgroundTracingConfigImpl::FromDict(std::move(dict)));
  EXPECT_TRUE(config);

  EXPECT_TRUE(BackgroundTracingManager::GetInstance().SetActiveScenario(
      std::move(config), BackgroundTracingManager::NO_DATA_FILTERING));

  background_tracing_helper.WaitForTraceStarted();

  LOCAL_HISTOGRAM_COUNTS("fake", 1);

  background_tracing_helper.WaitForTraceReceived();
  // Abort the scenario.
  background_tracing_helper.ExpectOnScenarioIdle("");
  BackgroundTracingManager::GetInstance().AbortScenarioForTesting();
  background_tracing_helper.WaitForScenarioIdle();

  EXPECT_TRUE(background_tracing_helper.trace_received());
}

// This tests that invalid preemptive mode configs will fail.
IN_PROC_BROWSER_TEST_F(
    BackgroundTracingManagerBrowserTest,
    SetActiveScenarioWithReceiveCallbackFailsWithInvalidPreemptiveConfig) {
  base::Value::Dict dict;
  dict.Set("mode", "PREEMPTIVE_TRACING_MODE");
  dict.Set("custom_categories",
           tracing::TraceStartupConfig::kDefaultStartupCategories);

  base::Value::List rules_list;
  {
    base::Value::Dict rules_dict;
    rules_dict.Set("rule", "INVALID_RULE");
    rules_list.Append(std::move(rules_dict));
  }

  dict.Set("configs", std::move(rules_list));

  std::unique_ptr<BackgroundTracingConfig> config(
      BackgroundTracingConfigImpl::FromDict(std::move(dict)));
  // An invalid config should always return a nullptr here.
  EXPECT_FALSE(config);
}

// This tests that reactive mode records and terminates with timeout.
IN_PROC_BROWSER_TEST_F(BackgroundTracingManagerBrowserTest,
                       ReactiveTimeoutTermination) {
  TestBackgroundTracingHelper background_tracing_helper;

  std::unique_ptr<BackgroundTracingConfig> config = CreateReactiveConfig();

  EXPECT_TRUE(BackgroundTracingManager::GetInstance().SetActiveScenario(
      std::move(config), BackgroundTracingManager::NO_DATA_FILTERING));

  EXPECT_TRUE(BackgroundTracingManager::EmitNamedTrigger("reactive_test"));

  BackgroundTracingManagerImpl::GetInstance()
      .GetActiveScenarioForTesting()
      ->FireTimerForTesting();

  background_tracing_helper.WaitForTraceReceived();
  background_tracing_helper.ExpectOnScenarioIdle("");
  BackgroundTracingManager::GetInstance().AbortScenarioForTesting();
  background_tracing_helper.WaitForScenarioIdle();

  EXPECT_TRUE(background_tracing_helper.trace_received());
}

IN_PROC_BROWSER_TEST_F(BackgroundTracingManagerBrowserTest,
                       SetupStartupTracing) {
  TestBackgroundTracingHelper background_tracing_helper;

  std::unique_ptr<TestStartupPreferenceManagerImpl> preferences_moved(
      new TestStartupPreferenceManagerImpl);
  TestStartupPreferenceManagerImpl* preferences = preferences_moved.get();
  BackgroundStartupTracingObserver::GetInstance()
      .SetPreferenceManagerForTesting(std::move(preferences_moved));
  preferences->SetBackgroundStartupTracingEnabled(false);

  base::Value::Dict dict;
  base::Value::List rules_list;
  {
    base::Value::Dict rules_dict;
    rules_dict.Set("rule", "MONITOR_AND_DUMP_WHEN_TRIGGER_NAMED");
    rules_dict.Set("trigger_name", "startup");
    rules_dict.Set("trigger_delay", 600);
    rules_list.Append(std::move(rules_dict));
  }
  dict.Set("configs", std::move(rules_list));

  std::unique_ptr<BackgroundTracingConfig> config(
      BackgroundTracingConfigImpl::ReactiveFromDict(dict));

  EXPECT_TRUE(BackgroundTracingManager::GetInstance().SetActiveScenario(
      std::move(config), BackgroundTracingManager::NO_DATA_FILTERING));

  // Since we specified a delay in the scenario, we should still be tracing
  // at this point.
  EXPECT_FALSE(
      BackgroundTracingManagerImpl::GetInstance().IsTracingForTesting());
  EXPECT_TRUE(preferences->GetBackgroundStartupTracingEnabled());

  // Abort the scenario.
  background_tracing_helper.ExpectOnScenarioIdle("");
  BackgroundTracingManager::GetInstance().AbortScenarioForTesting();
  background_tracing_helper.WaitForScenarioIdle();

  EXPECT_FALSE(background_tracing_helper.trace_received());
}

// TODO(crbug.com/1444519): Re-enable this test once fixed
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_CASTOS)
#define MAYBE_RunStartupTracing DISABLED_RunStartupTracing
#else
#define MAYBE_RunStartupTracing RunStartupTracing
#endif
IN_PROC_BROWSER_TEST_F(BackgroundTracingManagerBrowserTest,
                       MAYBE_RunStartupTracing) {
  TestTraceLogHelper tracelog_helper;
  TestBackgroundTracingHelper background_tracing_helper;

  std::unique_ptr<TestStartupPreferenceManagerImpl> preferences_moved(
      new TestStartupPreferenceManagerImpl);
  TestStartupPreferenceManagerImpl* preferences = preferences_moved.get();
  BackgroundStartupTracingObserver::GetInstance()
      .SetPreferenceManagerForTesting(std::move(preferences_moved));
  preferences->SetBackgroundStartupTracingEnabled(true);

  base::Value::Dict dict;
  base::Value::List rules_list;
  {
    base::Value::Dict rules_dict;
    rules_dict.Set("rule", "MONITOR_AND_DUMP_WHEN_TRIGGER_NAMED");
    rules_dict.Set("trigger_name", "foo");
    rules_dict.Set("trigger_delay", 10);
    rules_list.Append(std::move(rules_dict));
  }
  dict.Set("configs", std::move(rules_list));
  dict.Set("custom_categories",
           tracing::TraceStartupConfig::kDefaultStartupCategories);

  std::unique_ptr<BackgroundTracingConfig> config(
      BackgroundTracingConfigImpl::ReactiveFromDict(dict));

  EXPECT_TRUE(BackgroundTracingManager::GetInstance().SetActiveScenario(
      std::move(config), BackgroundTracingManager::ANONYMIZE_DATA));

  tracelog_helper.WaitForStartTracing();
  background_tracing_helper.WaitForTraceStarted();

  EXPECT_TRUE(BackgroundTracingManagerImpl::GetInstance()
                  .GetActiveScenarioForTesting()
                  ->GetConfig()
                  ->requires_anonymized_data());

  // Since we specified a delay in the scenario, we should still be tracing
  // at this point.
  EXPECT_TRUE(
      BackgroundTracingManagerImpl::GetInstance().IsTracingForTesting());

  BackgroundTracingManagerImpl::GetInstance()
      .GetActiveScenarioForTesting()
      ->FireTimerForTesting();

  background_tracing_helper.WaitForTraceReceived();
  background_tracing_helper.ExpectOnScenarioIdle("");
  BackgroundTracingManager::GetInstance().AbortScenarioForTesting();
  background_tracing_helper.WaitForScenarioIdle();

  EXPECT_TRUE(background_tracing_helper.trace_received());
  EXPECT_FALSE(preferences->GetBackgroundStartupTracingEnabled());
}

namespace {

class ProtoBackgroundTracingTest : public DevToolsProtocolTest {};

}  // namespace

IN_PROC_BROWSER_TEST_F(ProtoBackgroundTracingTest,
                       DevtoolsInterruptsBackgroundTracing) {
  TestBackgroundTracingHelper background_tracing_helper;

  std::unique_ptr<BackgroundTracingConfig> config = CreatePreemptiveConfig();

  EXPECT_TRUE(BackgroundTracingManager::GetInstance().SetActiveScenario(
      std::move(config), BackgroundTracingManager::NO_DATA_FILTERING));

  background_tracing_helper.WaitForTraceStarted();

  NavigateToURLBlockUntilNavigationsComplete(shell(), GURL("about:blank"), 1);
  Attach();

  const base::Value::Dict* start_tracing_result =
      SendCommandSync("Tracing.start");
  ASSERT_TRUE(start_tracing_result);
  background_tracing_helper.ExpectOnScenarioIdle("");
  BackgroundTracingManager::GetInstance().AbortScenarioForTesting();
  background_tracing_helper.WaitForScenarioIdle();
}

IN_PROC_BROWSER_TEST_F(ProtoBackgroundTracingTest, ProtoTraceReceived) {
  TestBackgroundTracingHelper background_tracing_helper;

  std::unique_ptr<BackgroundTracingConfig> config = CreatePreemptiveConfig();

  EXPECT_TRUE(BackgroundTracingManager::GetInstance().SetActiveScenario(
      std::move(config), BackgroundTracingManager::ANONYMIZE_DATA));

  background_tracing_helper.WaitForTraceStarted();

  // Add track event with blocked args.
  TRACE_EVENT_INSTANT("log", "LogMessage", [&](perfetto::EventContext ctx) {
    ctx.event()->set_log_message()->set_body_iid(
        base::trace_event::InternedLogMessage::Get(&ctx, std::string("test")));
  });

  NavigateToURLBlockUntilNavigationsComplete(shell(), GURL("about:blank"), 1);

  EXPECT_TRUE(BackgroundTracingManager::EmitNamedTrigger("preemptive_test"));
  background_tracing_helper.WaitForTraceSaved();
  EXPECT_TRUE(BackgroundTracingManager::GetInstance().HasTraceToUpload());

  std::string compressed_trace;
  base::RunLoop run_loop;
  BackgroundTracingManager::GetInstance().GetTraceToUpload(
      base::BindLambdaForTesting(
          [&](absl::optional<std::string> trace_content,
              absl::optional<std::string> system_profile) {
            ASSERT_TRUE(trace_content);
            compressed_trace = std::move(*trace_content);
            run_loop.Quit();
          }));
  run_loop.Run();

  background_tracing_helper.ExpectOnScenarioIdle("");
  BackgroundTracingManager::GetInstance().AbortScenarioForTesting();
  background_tracing_helper.WaitForScenarioIdle();

  std::string serialized_trace;
  ASSERT_TRUE(compression::GzipUncompress(compressed_trace, &serialized_trace));
  tracing::PrivacyFilteringCheck checker;
  checker.CheckProtoForUnexpectedFields(serialized_trace);
  EXPECT_GT(checker.stats().track_event, 0u);
  EXPECT_GT(checker.stats().process_desc, 0u);
  EXPECT_GT(checker.stats().thread_desc, 0u);
  EXPECT_TRUE(checker.stats().has_interned_names);
  EXPECT_TRUE(checker.stats().has_interned_categories);
  EXPECT_TRUE(checker.stats().has_interned_source_locations);
  EXPECT_FALSE(checker.stats().has_interned_log_messages);
}

IN_PROC_BROWSER_TEST_F(ProtoBackgroundTracingTest, ReceiveCallback) {
  TestBackgroundTracingHelper background_tracing_helper;

  std::unique_ptr<BackgroundTracingConfig> config = CreatePreemptiveConfig();

  // If a ReceiveCallback is given, it should be triggered instead of
  // SetTraceToUpload. (In production this is used to implement the
  // kBackgroundTracingOutputFile parameter, not to upload traces.)
  std::string received_trace_data;
  BackgroundTracingManager::GetInstance().SetReceiveCallback(
      base::BindLambdaForTesting(
          [&](std::string proto_content,
              BackgroundTracingManager::FinishedProcessingCallback callback) {
            received_trace_data = std::move(proto_content);
            std::move(callback).Run(true);
          }));
  EXPECT_TRUE(BackgroundTracingManager::GetInstance().SetActiveScenario(
      std::move(config), BackgroundTracingManager::ANONYMIZE_DATA));

  background_tracing_helper.WaitForTraceStarted();

  // Add track event with blocked args.
  TRACE_EVENT_INSTANT("log", "LogMessage", [&](perfetto::EventContext ctx) {
    ctx.event()->set_log_message()->set_body_iid(
        base::trace_event::InternedLogMessage::Get(&ctx, std::string("test")));
  });

  NavigateToURLBlockUntilNavigationsComplete(shell(), GURL("about:blank"), 1);

  EXPECT_TRUE(BackgroundTracingManager::EmitNamedTrigger("preemptive_test"));

  background_tracing_helper.WaitForTraceReceived();
  EXPECT_FALSE(BackgroundTracingManager::GetInstance().HasTraceToUpload());
  ASSERT_TRUE(background_tracing_helper.trace_received());
  std::string trace_data = background_tracing_helper.proto_file_contents();
  EXPECT_EQ(received_trace_data, trace_data);

  background_tracing_helper.ExpectOnScenarioIdle("");
  BackgroundTracingManager::GetInstance().AbortScenarioForTesting();
  background_tracing_helper.WaitForScenarioIdle();

  tracing::PrivacyFilteringCheck checker;
  checker.CheckProtoForUnexpectedFields(trace_data);
  EXPECT_GT(checker.stats().track_event, 0u);
  EXPECT_GT(checker.stats().process_desc, 0u);
  EXPECT_GT(checker.stats().thread_desc, 0u);
  EXPECT_TRUE(checker.stats().has_interned_names);
  EXPECT_TRUE(checker.stats().has_interned_categories);
  EXPECT_TRUE(checker.stats().has_interned_source_locations);
  EXPECT_FALSE(checker.stats().has_interned_log_messages);
}

// In SDK build, SystemProducer is not used. System tracing is done via the
// tracing muxer, so this test's setup is irrelevant.
// TODO(khokhlov): Figure out a way to test system tracing in SDK build.
#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
IN_PROC_BROWSER_TEST_F(BackgroundTracingManagerBrowserTest,
                       PerfettoSystemBackgroundScenarioDefaultName) {
  // This test will ensure that a BackgroundTracing scenario set to SYSTEM mode
  // can issue a SystemTrigger using the default name to let the Android
  // Perfetto service know the trace is interesting.
  //
  // This requires setting up a Perfetto Service which runs two unix sockets on
  // the android device. Chrome will be configured to connect to the producer
  // socket and treat it like the System tracing service. The test will connect
  // to the consumer to start a system trace and also to read back the results.
  //
  // This test is broken up into sections for readability:
  //
  // 1) Setup the sideloaded Perfetto System service
  // 2) Start System trace
  // 3) Setup & Run SYSTEM background scenario
  // 4) Wait and verify packets received & clean up

  // *********** Setup the sideloaded Perfetto System service **********
  auto system_service = std::make_unique<tracing::MockSystemService>(tmp_dir());
  SetSystemProducerSocketAndChecksAsync(system_service->producer());

  //  ******************** Start System trace **********************
  perfetto::TraceConfig trace_config =
      StopTracingTriggerConfig("org.chromium.background_tracing.system_test");
  base::RunLoop system_no_more_packets_runloop;
  auto system_consumer = CreateDefaultConsumer(std::move(trace_config),
                                               system_service->GetService(),
                                               &system_no_more_packets_runloop);
  system_consumer->WaitForAllDataSourcesStarted();

  // ************* Setup & Run SYSTEM background scenario ******************

  std::unique_ptr<BackgroundTracingConfig> config = CreateSystemConfig();
  ASSERT_TRUE(config);
  ASSERT_TRUE(BackgroundTracingManager::GetInstance().SetActiveScenario(
      std::move(config), BackgroundTracingManager::NO_DATA_FILTERING));

  // Actually send the trigger into the system.
  base::RunLoop rule_triggered_runloop;
  BackgroundTracingManagerImpl::GetInstance()
      .GetActiveScenarioForTesting()
      ->SetRuleTriggeredCallbackForTesting(
          rule_triggered_runloop.QuitClosure());
  // "system_test" is a NamedTriggerRule in CreateSystemConfig().
  EXPECT_TRUE(BackgroundTracingManager::EmitNamedTrigger("system_test"));
  rule_triggered_runloop.Run();

  // ************ Wait and verify packets received & clean up ************
  system_consumer->WaitForAllDataSourcesStopped();
  system_consumer->ReadBuffers();
  system_no_more_packets_runloop.Run();
  // We should at the very least receive the system packets if the trigger was
  // properly received by the trace. However if the background trigger was not
  // received we won't see any packets and |received_packets()| will be 0.
  EXPECT_GT(system_consumer->received_packets(), 0u);
}

IN_PROC_BROWSER_TEST_F(BackgroundTracingManagerBrowserTest,
                       PerfettoSystemBackgroundScenarioRuleId) {
  // This test will ensure that a BackgroundTracing scenario set to SYSTEM mode
  // can issue a SystemTrigger that users the |rule_id| json field to let the
  // Android Perfetto service know the trace is interesting.
  //
  // This requires setting up a Perfetto Service which runs two unix sockets on
  // the android device. Chrome will be configured to connect to the producer
  // socket and treat it like the System tracing service. The test will connect
  // to the consumer to start a system trace and also to read back the results.
  //
  // This test is broken up into sections for readability:
  //
  // 1) Setup the sideloaded Perfetto System service
  // 2) Start System trace
  // 3) Setup & Run SYSTEM background scenario
  // 4) Wait and verify packets received & clean up

  // *********** Setup the sideloaded Perfetto System service **********
  auto system_service = std::make_unique<tracing::MockSystemService>(tmp_dir());
  SetSystemProducerSocketAndChecksAsync(system_service->producer());

  //  ******************** Start System trace **********************
  perfetto::TraceConfig trace_config =
      StopTracingTriggerConfig("rule_id_override");
  base::RunLoop system_no_more_packets_runloop;
  auto system_consumer = CreateDefaultConsumer(std::move(trace_config),
                                               system_service->GetService(),
                                               &system_no_more_packets_runloop);
  system_consumer->WaitForAllDataSourcesStarted();

  // ************* Setup & Run SYSTEM background scenario ******************

  std::unique_ptr<BackgroundTracingConfig> config = CreateSystemConfig();
  ASSERT_TRUE(config);
  ASSERT_TRUE(BackgroundTracingManager::GetInstance().SetActiveScenario(
      std::move(config), BackgroundTracingManager::NO_DATA_FILTERING));
  // "system_test" is a NamedTriggerRule in CreateSystemConfig().
  EXPECT_TRUE(
      BackgroundTracingManager::EmitNamedTrigger("system_test_with_rule_id"));

  // ************ Wait and verify packets received & clean up ************
  system_consumer->WaitForAllDataSourcesStopped();
  system_consumer->ReadBuffers();
  system_no_more_packets_runloop.Run();
  // We should at the very least receive the system packets if the trigger was
  // properly received by the trace. However if the background trigger was not
  // received we won't see any packets and |received_packets()| will be 0.
  EXPECT_LT(0u, system_consumer->received_packets());
}
#endif  // BUILDFLAG(IS_POSIX) && !BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)

}  // namespace content
