// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/files/scoped_temp_dir.h"
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
#include "base/trace_event/named_trigger.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "content/browser/devtools/protocol/devtools_protocol_test_support.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/tracing/background_tracing_manager_impl.h"
#include "content/browser/tracing/background_tracing_rule.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/background_tracing_test_support.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/dns/mock_host_resolver.h"
#include "services/tracing/perfetto/privacy_filtering_check.h"
#include "services/tracing/public/cpp/stack_sampling/tracing_sampler_profiler.h"
#include "services/tracing/public/cpp/trace_startup_config.h"
#include "services/tracing/public/cpp/tracing_features.h"
#include "third_party/perfetto/include/perfetto/ext/trace_processor/export_json.h"
#include "third_party/perfetto/include/perfetto/trace_processor/trace_processor_storage.h"
#include "third_party/re2/src/re2/re2.h"
#include "third_party/zlib/google/compression_utils.h"
#include "third_party/zlib/zlib.h"

using base::trace_event::TraceLog;

namespace content {
namespace {

using testing::_;

class TestStartupPreferenceManagerImpl
    : public BackgroundTracingManagerImpl::PreferenceManager {
 public:
  void SetBackgroundStartupTracingEnabled(bool enabled) { enabled_ = enabled; }

  bool GetBackgroundStartupTracingEnabled() const override { return enabled_; }

 private:
  bool enabled_ = false;
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

    perfetto::trace_processor::TraceBlob blob =
        perfetto::trace_processor::TraceBlob::CopyFrom(
            proto_file_contents_.data(), proto_file_contents_.length());

    auto parse_status = trace_processor->Parse(
        perfetto::trace_processor::TraceBlobView(std::move(blob)));
    ASSERT_TRUE(parse_status.ok()) << parse_status.message();

    auto end_status = trace_processor->NotifyEndOfFile();
    ASSERT_TRUE(end_status.ok()) << end_status.message();

    auto export_status = perfetto::trace_processor::json::ExportJson(
        trace_processor.get(), this,
        perfetto::trace_processor::json::ArgumentFilterPredicate(),
        perfetto::trace_processor::json::MetadataFilterPredicate(),
        perfetto::trace_processor::json::LabelFilterPredicate());
    ASSERT_TRUE(export_status.ok()) << export_status.message();

    GetUIThreadTaskRunner({})->PostTask(
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

perfetto::protos::gen::ChromeFieldTracingConfig CreateSimpleScenarioConfig() {
  constexpr const char kScenarioConfig[] = R"pb(
    scenarios: {
      scenario_name: "test_scenario"
      start_rules: { manual_trigger_name: "start_trigger" }
      upload_rules: { manual_trigger_name: "upload_trigger" }
      trace_config: {
        data_sources: {
          config: {
            name: "track_event"
            track_event_config: {
              disabled_categories: [ "*" ],
              enabled_categories: [ "toplevel", "benchmark", "startup" ]
            }
          }
        }
        data_sources: { config: { name: "org.chromium.trace_metadata" } }
      }
    }
  )pb";
  return ParseFieldTracingConfigFromText(kScenarioConfig);
}

IN_PROC_BROWSER_TEST_F(BackgroundTracingManagerBrowserTest,
                       AddPresetScenarios) {
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
  auto scenarios = BackgroundTracingManager::GetInstance().AddPresetScenarios(
      ParseFieldTracingConfigFromText(kScenarioConfig),
      BackgroundTracingManager::NO_DATA_FILTERING);
  EXPECT_EQ(std::vector<std::string>({"e345f523fcd98b60063256afa89905ca"}),
            scenarios);
  {
    auto all_scenarios =
        BackgroundTracingManagerImpl::GetInstance().GetAllPresetScenarios();
    std::vector<trace_report::mojom::ScenarioPtr> expected;
    expected.push_back(trace_report::mojom::Scenario::New(
        "e345f523fcd98b60063256afa89905ca", "test_scenario"));
    EXPECT_EQ(expected, all_scenarios);
  }

  BackgroundTracingManager::GetInstance().SetEnabledScenarios(scenarios);
  EXPECT_EQ(std::vector<std::string>({"e345f523fcd98b60063256afa89905ca"}),
            BackgroundTracingManagerImpl::GetInstance().GetEnabledScenarios());

  background_tracing_helper.ExpectOnScenarioActive("test_scenario");
  EXPECT_TRUE(base::trace_event::EmitNamedTrigger("start_trigger"));
  background_tracing_helper.WaitForTraceStarted();

  background_tracing_helper.ExpectOnScenarioIdle("test_scenario");
  EXPECT_TRUE(base::trace_event::EmitNamedTrigger("upload_trigger"));
  background_tracing_helper.WaitForScenarioIdle();

  background_tracing_helper.WaitForTraceReceived();
  EXPECT_TRUE(background_tracing_helper.trace_received());
}

IN_PROC_BROWSER_TEST_F(BackgroundTracingManagerBrowserTest,
                       EnablePresetScenariosWhileTracing) {
  TestBackgroundTracingHelper background_tracing_helper;
  constexpr const char kScenarioConfig[] = R"pb(
    scenarios: {
      scenario_name: "test_scenario"
      start_rules: {
        name: "start_trigger"
        manual_trigger_name: "start_trigger"
      }
      trace_config: {
        data_sources: { config: { name: "org.chromium.trace_metadata" } }
      }
    }
  )pb";
  auto scenarios = BackgroundTracingManager::GetInstance().AddPresetScenarios(
      ParseFieldTracingConfigFromText(kScenarioConfig),
      BackgroundTracingManager::NO_DATA_FILTERING);

  EXPECT_EQ(std::vector<std::string>({"5875325968aa9b724ccf25e4018a2907"}),
            scenarios);
  BackgroundTracingManager::GetInstance().SetEnabledScenarios(scenarios);
  EXPECT_EQ(std::vector<std::string>({"5875325968aa9b724ccf25e4018a2907"}),
            BackgroundTracingManagerImpl::GetInstance().GetEnabledScenarios());

  background_tracing_helper.ExpectOnScenarioActive("test_scenario");
  EXPECT_TRUE(base::trace_event::EmitNamedTrigger("start_trigger"));
  background_tracing_helper.WaitForTraceStarted();

  background_tracing_helper.ExpectOnScenarioIdle("test_scenario");
  BackgroundTracingManager::GetInstance().SetEnabledScenarios({});
  EXPECT_EQ(std::vector<std::string>(),
            BackgroundTracingManagerImpl::GetInstance().GetEnabledScenarios());
  background_tracing_helper.WaitForScenarioIdle();
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
  BackgroundTracingManager::GetInstance().InitializeFieldScenarios(
      ParseFieldTracingConfigFromText(kScenarioConfig),
      BackgroundTracingManager::NO_DATA_FILTERING, false, 0);

  background_tracing_helper.ExpectOnScenarioActive("test_scenario");
  EXPECT_TRUE(base::trace_event::EmitNamedTrigger("start_trigger"));
  background_tracing_helper.WaitForTraceStarted();

  background_tracing_helper.ExpectOnScenarioIdle("test_scenario");
  EXPECT_TRUE(base::trace_event::EmitNamedTrigger("upload_trigger"));
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
  BackgroundTracingManager::GetInstance().InitializeFieldScenarios(
      ParseFieldTracingConfigFromText(kScenarioConfig),
      BackgroundTracingManager::NO_DATA_FILTERING, false, 0);

  background_tracing_helper.ExpectOnScenarioActive("test_scenario");
  background_tracing_helper.ExpectOnScenarioIdle("test_scenario");
  EXPECT_TRUE(base::trace_event::EmitNamedTrigger("start_trigger"));

  background_tracing_helper.WaitForScenarioIdle();
}

// This tests that non-allowlisted args get stripped if required.
// TODO(https://crbug.com/332743783): Flakey on Linux TSan.
#if BUILDFLAG(IS_LINUX) && defined(THREAD_SANITIZER)
#define MAYBE_NotAllowlistedArgsStripped DISABLED_NotAllowlistedArgsStripped
#else
#define MAYBE_NotAllowlistedArgsStripped NotAllowlistedArgsStripped
#endif
IN_PROC_BROWSER_TEST_F(BackgroundTracingManagerBrowserTest,
                       MAYBE_NotAllowlistedArgsStripped) {
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
  BackgroundTracingManager::GetInstance().InitializeFieldScenarios(
      ParseFieldTracingConfigFromText(kScenarioConfig),
      BackgroundTracingManager::ANONYMIZE_DATA, false, 0);
  background_tracing_helper.ExpectOnScenarioActive("test_scenario");
  EXPECT_TRUE(base::trace_event::EmitNamedTrigger("start_trigger"));
  background_tracing_helper.WaitForTraceStarted();

  {
    TRACE_EVENT1("toplevel", "ThreadPool_RunTask", "src_file", "abc");
    TRACE_EVENT1("startup", "TestNotAllowlist", "test_not_allowlist", "abc");
  }

  background_tracing_helper.ExpectOnScenarioIdle("test_scenario");
  EXPECT_TRUE(base::trace_event::EmitNamedTrigger("upload_trigger"));
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
  BackgroundTracingManager::GetInstance().InitializeFieldScenarios(
      ParseFieldTracingConfigFromText(kScenarioConfig),
      BackgroundTracingManager::NO_DATA_FILTERING, false, 0);

  observer.ExpectOnScenarioActive("test_scenario");
  EXPECT_TRUE(base::trace_event::EmitNamedTrigger("start_trigger"));

  EXPECT_FALSE(base::trace_event::EmitNamedTrigger("other_scenario"));

  observer.ExpectOnScenarioIdle("test_scenario");
  EXPECT_TRUE(base::trace_event::EmitNamedTrigger("stop_trigger"));
  observer.WaitForScenarioIdle();

  observer.ExpectOnScenarioActive("other_scenario");
  EXPECT_TRUE(base::trace_event::EmitNamedTrigger("other_start_trigger"));
}

IN_PROC_BROWSER_TEST_F(BackgroundTracingManagerBrowserTest,
                       StartNestedScenario) {
  TestBackgroundTracingHelper observer;
  constexpr const char kScenarioConfig[] = R"pb(
    scenarios: {
      scenario_name: "test_scenario"
      start_rules: { manual_trigger_name: "start_trigger" }
      stop_rules: { manual_trigger_name: "stop_trigger" }
      trace_config: {
        data_sources: { config: { name: "org.chromium.trace_metadata" } }
      }
      nested_scenarios: {
        scenario_name: "nested_scenario"
        start_rules: { manual_trigger_name: "nested_start_trigger" }
        upload_rules: { manual_trigger_name: "nested_upload_trigger" }
      }
    }
  )pb";
  BackgroundTracingManager::GetInstance().InitializeFieldScenarios(
      ParseFieldTracingConfigFromText(kScenarioConfig),
      BackgroundTracingManager::NO_DATA_FILTERING, false, 0);

  observer.ExpectOnScenarioActive("test_scenario");
  EXPECT_TRUE(base::trace_event::EmitNamedTrigger("start_trigger"));

  EXPECT_TRUE(base::trace_event::EmitNamedTrigger("nested_start_trigger"));
  EXPECT_TRUE(base::trace_event::EmitNamedTrigger("nested_upload_trigger"));

  observer.WaitForTraceReceived();
  EXPECT_TRUE(observer.trace_received());

  observer.ExpectOnScenarioIdle("test_scenario");
  EXPECT_TRUE(base::trace_event::EmitNamedTrigger("stop_trigger"));
  observer.WaitForScenarioIdle();
}

// This tests that non-allowlisted args get stripped if required.
IN_PROC_BROWSER_TEST_F(BackgroundTracingManagerBrowserTest,
                       LegacyNotAllowlistedArgsStripped) {
  TestBackgroundTracingHelper background_tracing_helper;

  EXPECT_TRUE(BackgroundTracingManager::GetInstance().InitializeFieldScenarios(
      CreateSimpleScenarioConfig(), BackgroundTracingManager::ANONYMIZE_DATA,
      false, 0));

  background_tracing_helper.ExpectOnScenarioActive("test_scenario");
  EXPECT_TRUE(base::trace_event::EmitNamedTrigger("start_trigger"));
  background_tracing_helper.WaitForTraceStarted();

  {
    TRACE_EVENT1("toplevel", "ThreadPool_RunTask", "src_file", "abc");
    TRACE_EVENT1("startup", "TestNotAllowlist", "test_not_allowlist", "abc");
  }

  background_tracing_helper.ExpectOnScenarioIdle("test_scenario");
  EXPECT_TRUE(base::trace_event::EmitNamedTrigger("upload_trigger"));
  background_tracing_helper.WaitForScenarioIdle();

  background_tracing_helper.WaitForTraceReceived();

  EXPECT_TRUE(background_tracing_helper.trace_received());
  EXPECT_TRUE(background_tracing_helper.TraceHasMatchingString("{"));
  EXPECT_TRUE(background_tracing_helper.TraceHasMatchingString("src_file"));
  EXPECT_FALSE(
      background_tracing_helper.TraceHasMatchingString("test_not_allowlist"));
}

// Regression test for https://crbug.com/1405341.
// Tests that RenderFrameHostImpl destruction is finished without crashing when
// tracing is enabled.
// TODO(crbug.com/335334098): Flaky on Linux TSan
#if BUILDFLAG(IS_LINUX) && defined(THREAD_SANITIZER)
#define MAYBE_TracingRenderFrameHostImplDtor \
  DISABLED_TracingRenderFrameHostImplDtor
#else
#define MAYBE_TracingRenderFrameHostImplDtor TracingRenderFrameHostImplDtor
#endif
IN_PROC_BROWSER_TEST_F(BackgroundTracingManagerBrowserTest,
                       MAYBE_TracingRenderFrameHostImplDtor) {
  ASSERT_TRUE(embedded_test_server()->Start());

  TestBackgroundTracingHelper background_tracing_helper;

  constexpr const char kScenarioConfig[] = R"pb(
    scenarios: {
      scenario_name: "test_scenario"
      start_rules: { manual_trigger_name: "start_trigger" }
      upload_rules: { manual_trigger_name: "upload_trigger" }
      trace_config: {
        data_sources: {
          config: {
            name: "track_event"
            track_event_config: {
              disabled_categories: [ "*" ],
              enabled_categories: [ "content" ]
            }
          }
        }
      }
    }
  )pb";

  BackgroundTracingManager::GetInstance().InitializeFieldScenarios(
      ParseFieldTracingConfigFromText(kScenarioConfig),
      BackgroundTracingManager::NO_DATA_FILTERING, false, 0);

  background_tracing_helper.ExpectOnScenarioActive("test_scenario");
  EXPECT_TRUE(base::trace_event::EmitNamedTrigger("start_trigger"));
  background_tracing_helper.WaitForTraceStarted();

  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("a.com", "/title1.html")));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  auto* rfhi = static_cast<RenderFrameHostImpl*>(
      shell()->web_contents()->GetPrimaryMainFrame());

  // Audible audio output should cause the media stream count to increment.
  rfhi->OnMediaStreamAdded(
      RenderFrameHostImpl::MediaStreamType::kPlayingAudibleAudioStream);

  RenderFrameDeletedObserver delete_frame(rfhi);

  // The old RenderFrameHost might have entered the BackForwardCache. Disable
  // back-forward cache to ensure that the RenderFrameHost gets deleted.
  DisableBackForwardCacheForTesting(shell()->web_contents(),
                                    BackForwardCache::TEST_REQUIRES_NO_CACHING);

  GURL cross_site_url = embedded_test_server()->GetURL("b.com", "/title2.html");
  EXPECT_TRUE(NavigateToURL(shell(), cross_site_url));
  delete_frame.WaitUntilDeleted();

  background_tracing_helper.ExpectOnScenarioIdle("test_scenario");
  EXPECT_TRUE(base::trace_event::EmitNamedTrigger("upload_trigger"));
  background_tracing_helper.WaitForScenarioIdle();

  background_tracing_helper.WaitForTraceReceived();

  EXPECT_TRUE(background_tracing_helper.trace_received());
}

// Tests that events emitted by the browser process immediately after the
// SetActiveScenarioWithReceiveCallback call does get included in the trace,
// without waiting for the full WaitForTraceStarted() callback (background
// tracing will directly enable the TraceLog so we get events prior to waiting
// for the whole IPC sequence to enable tracing coming back from the tracing
// service).
// The test is disabled since Perfetto SDK migration because startup tracing is
// now started asynchronously.
// TODO(khokhlov): Re-enable when background tracing is switched to synchronous
// start.
IN_PROC_BROWSER_TEST_F(BackgroundTracingManagerBrowserTest,
                       EarlyTraceEventsInTrace) {
  TestBackgroundTracingHelper background_tracing_helper;

  EXPECT_TRUE(BackgroundTracingManager::GetInstance().InitializeFieldScenarios(
      CreateSimpleScenarioConfig(), BackgroundTracingManager::ANONYMIZE_DATA,
      false, 0));

  background_tracing_helper.ExpectOnScenarioActive("test_scenario");
  EXPECT_TRUE(base::trace_event::EmitNamedTrigger("start_trigger"));

  { TRACE_EVENT0("benchmark", "TestEarlyEvent"); }

  background_tracing_helper.WaitForTraceStarted();

  background_tracing_helper.ExpectOnScenarioIdle("test_scenario");
  EXPECT_TRUE(base::trace_event::EmitNamedTrigger("upload_trigger"));
  background_tracing_helper.WaitForScenarioIdle();

  background_tracing_helper.WaitForTraceReceived();

  EXPECT_TRUE(background_tracing_helper.trace_received());
  EXPECT_TRUE(background_tracing_helper.TraceHasMatchingString("{"));
  EXPECT_TRUE(
      background_tracing_helper.TraceHasMatchingString("TestEarlyEvent"));
}

// This tests that browser metadata gets included in the trace.
// TODO(crbug.com/40891272): Re-enable this test on TSAN builds.
#if BUILDFLAG(IS_LINUX) && defined(THREAD_SANITIZER)
#define MAYBE_TraceMetadataInTrace DISABLED_TraceMetadataInTrace
#else
#define MAYBE_TraceMetadataInTrace TraceMetadataInTrace
#endif
IN_PROC_BROWSER_TEST_F(BackgroundTracingManagerBrowserTest,
                       MAYBE_TraceMetadataInTrace) {
  TestBackgroundTracingHelper background_tracing_helper;

  EXPECT_TRUE(BackgroundTracingManager::GetInstance().InitializeFieldScenarios(
      CreateSimpleScenarioConfig(), BackgroundTracingManager::NO_DATA_FILTERING,
      false, 0));

  background_tracing_helper.ExpectOnScenarioActive("test_scenario");
  EXPECT_TRUE(base::trace_event::EmitNamedTrigger("start_trigger"));
  background_tracing_helper.WaitForTraceStarted();

  background_tracing_helper.ExpectOnScenarioIdle("test_scenario");
  EXPECT_TRUE(base::trace_event::EmitNamedTrigger("upload_trigger"));
  background_tracing_helper.WaitForScenarioIdle();

  background_tracing_helper.WaitForTraceReceived();

  EXPECT_TRUE(background_tracing_helper.trace_received());
  EXPECT_TRUE(background_tracing_helper.TraceHasMatchingString("cpu-brand"));
  EXPECT_TRUE(background_tracing_helper.TraceHasMatchingString("network-type"));
  EXPECT_TRUE(background_tracing_helper.TraceHasMatchingString("user-agent"));
}

// This tests that histogram triggers for preemptive mode configs.
// TODO(crbug.com/40900999): Flaky on Linux TSan.
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
  constexpr const char kScenarioConfig[] = R"pb(
    scenarios: {
      scenario_name: "test_scenario"
      start_rules: { manual_trigger_name: "start_trigger" }
      upload_rules: { histogram: { histogram_name: "fake" min_value: 1 } }
      trace_config: {
        data_sources: { config: { name: "org.chromium.trace_metadata" } }
      }
    }
  )pb";
  BackgroundTracingManager::GetInstance().InitializeFieldScenarios(
      ParseFieldTracingConfigFromText(kScenarioConfig),
      BackgroundTracingManager::NO_DATA_FILTERING, false, 0);

  background_tracing_helper.ExpectOnScenarioActive("test_scenario");
  EXPECT_TRUE(base::trace_event::EmitNamedTrigger("start_trigger"));
  background_tracing_helper.WaitForTraceStarted();

  background_tracing_helper.ExpectOnScenarioIdle("test_scenario");
  // Our reference value is "1", so a value of "2" should trigger a trace.
  LOCAL_HISTOGRAM_COUNTS("fake", 2);

  background_tracing_helper.WaitForScenarioIdle();
  background_tracing_helper.WaitForTraceReceived();

  EXPECT_TRUE(background_tracing_helper.trace_received());

  std::optional<base::Value> trace_json =
      base::JSONReader::Read(background_tracing_helper.json_file_contents());
  ASSERT_TRUE(trace_json);
  ASSERT_TRUE(trace_json->is_dict());
  auto* metadata_json = trace_json->GetDict().FindDict("metadata");
  ASSERT_TRUE(metadata_json);

  const std::string* trace_config = metadata_json->FindString("trace-config");
  ASSERT_TRUE(trace_config);
  EXPECT_NE(trace_config->find("record-continuously"), trace_config->npos)
      << *trace_config;
}

// Used as a known symbol to look up the current module.
void DummyFunc() {}

// Test that the tracing sampler profiler running in background tracing mode,
// produces stack frames in the expected JSON format.
// TODO(crbug.com/40680210) Disabled for being flaky.
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

  constexpr const char kScenarioConfig[] = R"pb(
    scenarios: {
      scenario_name: "test_scenario"
      start_rules: { manual_trigger_name: "start_trigger" }
      upload_rules: { manual_trigger_name: "upload_trigger" }
      trace_config: {
        data_sources: {
          config: {
            name: "track_event"
            track_event_config: {
              disabled_categories: [ "*" ],
              enabled_categories: [ "content" ]
            }
          }
        }
      }
    }
  )pb";

  BackgroundTracingManager::GetInstance().InitializeFieldScenarios(
      ParseFieldTracingConfigFromText(kScenarioConfig),
      BackgroundTracingManager::ANONYMIZE_DATA, false, 0);

  background_tracing_helper.ExpectOnScenarioActive("test_scenario");
  EXPECT_TRUE(base::trace_event::EmitNamedTrigger("start_trigger"));
  background_tracing_helper.WaitForTraceStarted();

  wait_for_sample.Run();

  background_tracing_helper.ExpectOnScenarioIdle("test_scenario");
  EXPECT_TRUE(base::trace_event::EmitNamedTrigger("upload_trigger"));
  background_tracing_helper.WaitForScenarioIdle();
  background_tracing_helper.WaitForTraceReceived();

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

IN_PROC_BROWSER_TEST_F(BackgroundTracingManagerBrowserTest,
                       SetupStartupTracing) {
  std::unique_ptr<TestStartupPreferenceManagerImpl> preferences_moved(
      new TestStartupPreferenceManagerImpl);
  TestStartupPreferenceManagerImpl* preferences = preferences_moved.get();
  BackgroundTracingManagerImpl::GetInstance().SetPreferenceManagerForTesting(
      std::move(preferences_moved));
  preferences->SetBackgroundStartupTracingEnabled(false);

  perfetto::protos::gen::ChromeFieldTracingConfig config;
  EXPECT_TRUE(BackgroundTracingManager::GetInstance().InitializeFieldScenarios(
      config, BackgroundTracingManager::ANONYMIZE_DATA, false, 0));

  EXPECT_FALSE(base::trace_event::EmitNamedTrigger(
      base::trace_event::kStartupTracingTriggerName));
}

// TODO(crbug.com/40267734): Re-enable this test once fixed
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID)
#define MAYBE_RunStartupTracing DISABLED_RunStartupTracing
#else
#define MAYBE_RunStartupTracing RunStartupTracing
#endif
IN_PROC_BROWSER_TEST_F(BackgroundTracingManagerBrowserTest,
                       MAYBE_RunStartupTracing) {
  TestBackgroundTracingHelper background_tracing_helper;

  std::unique_ptr<TestStartupPreferenceManagerImpl> preferences_moved(
      new TestStartupPreferenceManagerImpl);
  TestStartupPreferenceManagerImpl* preferences = preferences_moved.get();
  BackgroundTracingManagerImpl::GetInstance().SetPreferenceManagerForTesting(
      std::move(preferences_moved));
  preferences->SetBackgroundStartupTracingEnabled(true);

  perfetto::protos::gen::ChromeFieldTracingConfig config;
  EXPECT_TRUE(BackgroundTracingManager::GetInstance().InitializeFieldScenarios(
      config, BackgroundTracingManager::ANONYMIZE_DATA, false, 0));

  background_tracing_helper.ExpectOnScenarioActive("Startup");
  EXPECT_TRUE(base::trace_event::EmitNamedTrigger(
      base::trace_event::kStartupTracingTriggerName));

  background_tracing_helper.WaitForTraceStarted();

  background_tracing_helper.ExpectOnScenarioIdle("Startup");
  background_tracing_helper.WaitForScenarioIdle();

  background_tracing_helper.WaitForTraceReceived();
  EXPECT_TRUE(background_tracing_helper.trace_received());
}

namespace {

class ProtoBackgroundTracingTest : public DevToolsProtocolTest {};

}  // namespace

IN_PROC_BROWSER_TEST_F(ProtoBackgroundTracingTest,
                       DevtoolsInterruptsBackgroundTracing) {
  TestBackgroundTracingHelper background_tracing_helper;
  constexpr const char kScenarioConfig[] = R"pb(
    scenarios: {
      scenario_name: "test_scenario"
      start_rules: { manual_trigger_name: "start_trigger" }
      trace_config: {
        data_sources: { config: { name: "org.chromium.trace_metadata" } }
      }
    }
  )pb";

  EXPECT_TRUE(BackgroundTracingManager::GetInstance().InitializeFieldScenarios(
      ParseFieldTracingConfigFromText(kScenarioConfig),
      BackgroundTracingManager::NO_DATA_FILTERING, false, 0));

  background_tracing_helper.ExpectOnScenarioActive("test_scenario");
  EXPECT_TRUE(base::trace_event::EmitNamedTrigger("start_trigger"));
  background_tracing_helper.WaitForTraceStarted();

  NavigateToURLBlockUntilNavigationsComplete(shell(), GURL("about:blank"), 1);
  Attach();

  const base::Value::Dict* start_tracing_result =
      SendCommandSync("Tracing.start");
  ASSERT_TRUE(start_tracing_result);
  background_tracing_helper.ExpectOnScenarioIdle("test_scenario");
  BackgroundTracingManager::GetInstance().AbortScenarioForTesting();
  background_tracing_helper.WaitForScenarioIdle();
}

IN_PROC_BROWSER_TEST_F(ProtoBackgroundTracingTest, ProtoTraceReceived) {
  TestBackgroundTracingHelper background_tracing_helper;

  EXPECT_TRUE(BackgroundTracingManager::GetInstance().InitializeFieldScenarios(
      CreateSimpleScenarioConfig(), BackgroundTracingManager::ANONYMIZE_DATA,
      false, 0));

  background_tracing_helper.ExpectOnScenarioActive("test_scenario");
  EXPECT_TRUE(base::trace_event::EmitNamedTrigger("start_trigger"));
  background_tracing_helper.WaitForTraceStarted();

  // Add track event with blocked args.
  TRACE_EVENT_INSTANT("log", "LogMessage", [&](perfetto::EventContext ctx) {
    ctx.event()->set_log_message()->set_body_iid(
        base::trace_event::InternedLogMessage::Get(&ctx, std::string("test")));
  });

  NavigateToURLBlockUntilNavigationsComplete(shell(), GURL("about:blank"), 1);

  background_tracing_helper.ExpectOnScenarioIdle("test_scenario");
  EXPECT_TRUE(base::trace_event::EmitNamedTrigger("upload_trigger"));
  background_tracing_helper.WaitForScenarioIdle();

  background_tracing_helper.WaitForTraceSaved();
  EXPECT_TRUE(BackgroundTracingManager::GetInstance().HasTraceToUpload());

  std::string compressed_trace;
  base::RunLoop run_loop;
  BackgroundTracingManager::GetInstance().GetTraceToUpload(
      base::BindLambdaForTesting(
          [&](std::optional<std::string> trace_content,
              std::optional<std::string> system_profile) {
            ASSERT_TRUE(trace_content);
            compressed_trace = std::move(*trace_content);
            run_loop.Quit();
          }));
  run_loop.Run();

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

  EXPECT_TRUE(BackgroundTracingManager::GetInstance().InitializeFieldScenarios(
      CreateSimpleScenarioConfig(), BackgroundTracingManager::ANONYMIZE_DATA,
      false, 0));

  // If a ReceiveCallback is given, it should be triggered instead of
  // SetTraceToUpload. (In production this is used to implement the
  // kBackgroundTracingOutputFile parameter, not to upload traces.)
  std::string received_trace_data;
  BackgroundTracingManager::GetInstance().SetReceiveCallback(
      base::BindLambdaForTesting(
          [&](const std::string& file_name, std::string proto_content,
              BackgroundTracingManager::FinishedProcessingCallback callback) {
            received_trace_data = std::move(proto_content);
            std::move(callback).Run(true);
          }));

  background_tracing_helper.ExpectOnScenarioActive("test_scenario");
  EXPECT_TRUE(base::trace_event::EmitNamedTrigger("start_trigger"));
  background_tracing_helper.WaitForTraceStarted();

  // Add track event with blocked args.
  TRACE_EVENT_INSTANT("log", "LogMessage", [&](perfetto::EventContext ctx) {
    ctx.event()->set_log_message()->set_body_iid(
        base::trace_event::InternedLogMessage::Get(&ctx, std::string("test")));
  });

  NavigateToURLBlockUntilNavigationsComplete(shell(), GURL("about:blank"), 1);

  background_tracing_helper.ExpectOnScenarioIdle("test_scenario");
  EXPECT_TRUE(base::trace_event::EmitNamedTrigger("upload_trigger"));
  background_tracing_helper.WaitForScenarioIdle();
  background_tracing_helper.WaitForTraceReceived();
  EXPECT_FALSE(BackgroundTracingManager::GetInstance().HasTraceToUpload());
  ASSERT_TRUE(background_tracing_helper.trace_received());
  std::string trace_data = background_tracing_helper.proto_file_contents();
  EXPECT_EQ(received_trace_data, trace_data);

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

}  // namespace content
