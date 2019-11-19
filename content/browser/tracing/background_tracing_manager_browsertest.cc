// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/json/json_reader.h"
#include "base/macros.h"
#include "base/memory/ref_counted_memory.h"
#include "base/metrics/histogram_macros.h"
#include "base/process/process_handle.h"
#include "base/run_loop.h"
#include "base/strings/pattern.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/post_task.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_timeouts.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "content/browser/devtools/protocol/devtools_protocol_test_support.h"
#include "content/browser/tracing/background_startup_tracing_observer.h"
#include "content/browser/tracing/background_tracing_active_scenario.h"
#include "content/browser/tracing/background_tracing_manager_impl.h"
#include "content/browser/tracing/background_tracing_rule.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "services/tracing/perfetto/privacy_filtering_check.h"
#include "services/tracing/public/cpp/tracing_features.h"
#include "third_party/zlib/zlib.h"

#if defined(OS_ANDROID)
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
  // We need to let the AndroidSystemProducer know the MockSystemService socket
  // address and that if we're running on Android devices older then Pie to
  // still connect.
  tracing::PerfettoTracedProcess::GetTaskRunner()
      ->GetOrCreateTaskRunner()
      ->PostTask(
          FROM_HERE,
          base::BindOnce(
              [](const std::string& producer_socket) {
                // The only other type of system producer is
                // AndroidSystemProducer so this assert ensures that the
                // static_cast below is safe.
                ASSERT_FALSE(tracing::PerfettoTracedProcess::Get()
                                 ->SystemProducerForTesting()
                                 ->IsDummySystemProducerForTesting());
                auto* producer = static_cast<tracing::AndroidSystemProducer*>(
                    tracing::PerfettoTracedProcess::Get()
                        ->SystemProducerForTesting());
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
#endif  // defined(OS_ANDROID)

using base::trace_event::TraceLog;

namespace content {
namespace {

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

// Wait until |condition| returns true.
void WaitForCondition(base::RepeatingCallback<bool()> condition,
                      const std::string& description) {
  const base::TimeDelta kTimeout = base::TimeDelta::FromSeconds(30);
  const base::TimeTicks start_time = base::TimeTicks::Now();
  while (!condition.Run() && (base::TimeTicks::Now() - start_time < kTimeout)) {
    base::RunLoop run_loop;
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), TestTimeouts::tiny_timeout());
    run_loop.Run();
  }
  ASSERT_TRUE(condition.Run())
      << "Timeout waiting for condition: " << description;
}

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

// An helper class that observes background tracing states transition and
// allows synchronisation with tests. The class adds itself as a backgrond
// tracing enabled state observer. It provides methods to wait for a given
// state.
//
// Usage:
//   TestBackgroundTracingHelper background_tracing_helper;
//   [... set a background tracing scenario ...]
//   background_tracing_helper.WaitForScenarioActivated();
//   [... trigger an event ...]
//   background_tracing_helper->WaitForTracingEnabled();
//   [... abort ...]
//   background_tracing_helper->WaitForScenarioAborted();
class TestBackgroundTracingHelper
    : public BackgroundTracingManagerImpl::EnabledStateObserver {
 public:
  TestBackgroundTracingHelper() {
    BackgroundTracingManagerImpl::GetInstance()->AddEnabledStateObserver(this);
  }

  ~TestBackgroundTracingHelper() override {
    BackgroundTracingManagerImpl::GetInstance()->RemoveEnabledStateObserver(
        this);
    EXPECT_FALSE(is_scenario_active_);
  }

  void OnScenarioActivated(const BackgroundTracingConfigImpl* config) override {
    is_scenario_active_ = true;
    wait_for_scenario_activated_.Quit();
  }

  void OnScenarioAborted() override {
    is_scenario_active_ = false;
    wait_for_scenario_aborted_.Quit();
  }

  void OnTracingEnabled(
      BackgroundTracingConfigImpl::CategoryPreset preset) override {
    wait_for_tracing_enabled_.Quit();
  }

  void WaitForScenarioActivated() { wait_for_scenario_activated_.Run(); }
  void WaitForScenarioAborted() { wait_for_scenario_aborted_.Run(); }
  void WaitForTracingEnabled() { wait_for_tracing_enabled_.Run(); }

 private:
  bool is_scenario_active_ = false;
  base::RunLoop wait_for_scenario_activated_;
  base::RunLoop wait_for_scenario_aborted_;
  base::RunLoop wait_for_tracing_enabled_;
};

// An helper class that receives uploaded trace. It allows synchronisation with
// tests.
//
// Usage:
//   TestTraceReceiverHelper trace_receiver_helper;
//   [... do tracing stuff ...]
//   trace_receiver_helper.WaitForTraceReceived();
class TestTraceReceiverHelper {
 public:
  BackgroundTracingManager::ReceiveCallback get_receive_callback() {
    return base::BindRepeating(&TestTraceReceiverHelper::Upload,
                               base::Unretained(this));
  }

  void WaitForTraceReceived() { wait_for_trace_received_.Run(); }
  bool trace_received() const { return trace_received_; }
  const std::string& file_contents() const { return file_contents_; }

  bool TraceHasMatchingString(const char* text) const {
    return file_contents_.find(text) != std::string::npos;
  }

  void Upload(
      std::unique_ptr<std::string> file_contents,
      BackgroundTracingManager::FinishedProcessingCallback done_callback) {
    EXPECT_TRUE(file_contents);
    EXPECT_FALSE(trace_received_);
    trace_received_ = true;

    // Uncompress the trace content.
    size_t compressed_length = file_contents->length();
    const size_t kOutputBufferLength = 10 * 1024 * 1024;
    std::vector<char> output_str(kOutputBufferLength);

    z_stream stream = {nullptr};
    stream.avail_in = compressed_length;
    stream.avail_out = kOutputBufferLength;
    stream.next_in =
        reinterpret_cast<Bytef*>(const_cast<char*>(file_contents->data()));
    stream.next_out = reinterpret_cast<Bytef*>(output_str.data());

    // 16 + MAX_WBITS means only decoding gzip encoded streams, and using
    // the biggest window size, according to zlib.h
    int result = inflateInit2(&stream, 16 + MAX_WBITS);
    EXPECT_EQ(Z_OK, result);
    result = inflate(&stream, Z_FINISH);
    int bytes_written = kOutputBufferLength - stream.avail_out;

    inflateEnd(&stream);
    EXPECT_EQ(Z_STREAM_END, result);

    file_contents_.assign(output_str.data(), bytes_written);

    // Post the callbacks.
    base::PostTask(FROM_HERE, {BrowserThread::UI},
                   base::BindOnce(std::move(done_callback), true));

    base::PostTask(FROM_HERE, {BrowserThread::UI},
                   wait_for_trace_received_.QuitWhenIdleClosure());
  }

 private:
  base::RunLoop wait_for_trace_received_;
  bool trace_received_ = false;
  std::string file_contents_;
};

// An helper class that receives multiple traces through the same callback.
class TestMultipleTraceReceiverHelper {
 public:
  BackgroundTracingManager::ReceiveCallback get_receive_callback() {
    return base::BindRepeating(&TestMultipleTraceReceiverHelper::Upload,
                               base::Unretained(this));
  }

  void WaitForTraceReceived(size_t offset) {
    trace_receivers_[offset].WaitForTraceReceived();
  }

  bool trace_received(size_t offset) {
    return trace_receivers_[offset].trace_received();
  }

  void Upload(
      std::unique_ptr<std::string> file_contents,
      BackgroundTracingManager::FinishedProcessingCallback done_callback) {
    trace_receivers_[current_receiver_offset_].Upload(std::move(file_contents),
                                                      std::move(done_callback));
    current_receiver_offset_++;
  }

 private:
  std::map<size_t, TestTraceReceiverHelper> trace_receivers_;
  int current_receiver_offset_ = 0;
};

// An helper class accepts a slow-report trigger callback.
//
// Usage:
//   TestTriggerHelper test_trigger_helper;
//    BackgroundTracingManager::GetInstance()->TriggerNamedEvent(
//        handle, trigger_helper.receive_closure(true));
//   test_trigger_helper.WaitForTriggerReceived();
class TestTriggerHelper {
 public:
  BackgroundTracingManager::StartedFinalizingCallback receive_closure(
      bool expected) {
    return base::BindRepeating(&TestTriggerHelper::OnTriggerReceive,
                               base::Unretained(this), expected);
  }

  void WaitForTriggerReceived() { wait_for_trigger_received_.Run(); }

 private:
  void OnTriggerReceive(bool expected, bool value) {
    EXPECT_EQ(expected, value);
    wait_for_trigger_received_.QuitWhenIdle();
  }

  base::RunLoop wait_for_trigger_received_;
};

}  // namespace

class BackgroundTracingManagerBrowserTest : public ContentBrowserTest {
 public:
  BackgroundTracingManagerBrowserTest() {
    feature_list_.InitWithFeatures(
        /* enabled_features = */ {features::kEnablePerfettoSystemTracing},
        /* disabled_features = */ {features::kBackgroundTracingProtoOutput});
    // CreateUniqueTempDir() makes a blocking call to create the directory and
    // wait on it. This isn't allowed in a normal browser context. Therefore we
    // do this in the test constructor before the browser prevents the blocking
    // call.
    CHECK(tmp_dir_.CreateUniqueTempDir());
  }

  void PreRunTestOnMainThread() override {
    BackgroundTracingManagerImpl::GetInstance()
        ->InvalidateTriggerHandlesForTesting();

    ContentBrowserTest::PreRunTestOnMainThread();
  }

  const base::ScopedTempDir& tmp_dir() const { return tmp_dir_; }

 private:
  base::ScopedTempDir tmp_dir_;
  base::test::ScopedFeatureList feature_list_;

  DISALLOW_COPY_AND_ASSIGN(BackgroundTracingManagerBrowserTest);
};

std::unique_ptr<BackgroundTracingConfig> CreatePreemptiveConfig() {
  base::DictionaryValue dict;

  dict.SetString("mode", "PREEMPTIVE_TRACING_MODE");
  dict.SetString("category", "BENCHMARK");

  std::unique_ptr<base::ListValue> rules_list(new base::ListValue());
  {
    std::unique_ptr<base::DictionaryValue> rules_dict(
        new base::DictionaryValue());
    rules_dict->SetString("rule", "MONITOR_AND_DUMP_WHEN_TRIGGER_NAMED");
    rules_dict->SetString("trigger_name", "preemptive_test");
    rules_list->Append(std::move(rules_dict));
  }
  dict.Set("configs", std::move(rules_list));

  std::unique_ptr<BackgroundTracingConfig> config(
      BackgroundTracingConfigImpl::FromDict(&dict));

  EXPECT_TRUE(config);
  return config;
}

std::unique_ptr<BackgroundTracingConfig> CreateReactiveConfig() {
  base::DictionaryValue dict;

  dict.SetString("mode", "REACTIVE_TRACING_MODE");

  std::unique_ptr<base::ListValue> rules_list(new base::ListValue());
  {
    std::unique_ptr<base::DictionaryValue> rules_dict(
        new base::DictionaryValue());
    rules_dict->SetString("rule", "TRACE_ON_NAVIGATION_UNTIL_TRIGGER_OR_FULL");
    rules_dict->SetString("trigger_name", "reactive_test");
    rules_dict->SetBoolean("stop_tracing_on_repeated_reactive", true);
    rules_dict->SetString("category", "BENCHMARK");
    rules_list->Append(std::move(rules_dict));
  }
  dict.Set("configs", std::move(rules_list));

  std::unique_ptr<BackgroundTracingConfig> config(
      BackgroundTracingConfigImpl::FromDict(&dict));

  EXPECT_TRUE(config);
  return config;
}

std::unique_ptr<BackgroundTracingConfig> CreateSystemConfig() {
  base::DictionaryValue dict;
  dict.SetString("mode", "SYSTEM_TRACING_MODE");
  dict.SetString("category", "BENCHMARK");

  std::unique_ptr<base::ListValue> rules_list =
      std::make_unique<base::ListValue>();
  {
    std::unique_ptr<base::DictionaryValue> rules_dict =
        std::make_unique<base::DictionaryValue>();
    rules_dict->SetString("rule", "MONITOR_AND_DUMP_WHEN_TRIGGER_NAMED");
    rules_dict->SetString("trigger_name", "system_test");
    rules_list->Append(std::move(rules_dict));
  }
  {
    std::unique_ptr<base::DictionaryValue> rules_dict =
        std::make_unique<base::DictionaryValue>();
    rules_dict->SetString("rule", "MONITOR_AND_DUMP_WHEN_TRIGGER_NAMED");
    rules_dict->SetString("trigger_name", "system_test_with_rule_id");
    rules_dict->SetString("rule_id", "rule_id_override");
    rules_list->Append(std::move(rules_dict));
  }
  dict.Set("configs", std::move(rules_list));
  std::unique_ptr<BackgroundTracingConfig> config(
      BackgroundTracingConfigImpl::FromDict(&dict));

  EXPECT_TRUE(config);
  return config;
}

// This tests that the endpoint receives the final trace data.
IN_PROC_BROWSER_TEST_F(BackgroundTracingManagerBrowserTest,
                       ReceiveTraceFinalContentsOnTrigger) {
  TestBackgroundTracingHelper background_tracing_helper;
  TestTraceReceiverHelper trace_receiver_helper;

  std::unique_ptr<BackgroundTracingConfig> config = CreatePreemptiveConfig();

  BackgroundTracingManager::TriggerHandle handle =
      BackgroundTracingManager::GetInstance()->RegisterTriggerType(
          "preemptive_test");

  EXPECT_TRUE(BackgroundTracingManager::GetInstance()->SetActiveScenario(
      std::move(config), trace_receiver_helper.get_receive_callback(),
      BackgroundTracingManager::NO_DATA_FILTERING));

  background_tracing_helper.WaitForTracingEnabled();

  TestTriggerHelper trigger_helper;
  BackgroundTracingManager::GetInstance()->TriggerNamedEvent(
      handle, trigger_helper.receive_closure(true));

  trace_receiver_helper.WaitForTraceReceived();
  BackgroundTracingManager::GetInstance()->AbortScenarioForTesting();
  background_tracing_helper.WaitForScenarioAborted();

  EXPECT_TRUE(trace_receiver_helper.trace_received());
}

// This tests triggering more than once still only gathers once.
IN_PROC_BROWSER_TEST_F(BackgroundTracingManagerBrowserTest,
                       CallTriggersMoreThanOnceOnlyGatherOnce) {
  TestBackgroundTracingHelper background_tracing_helper;
  TestTraceReceiverHelper trace_receiver_helper;

  std::unique_ptr<BackgroundTracingConfig> config = CreatePreemptiveConfig();

  content::BackgroundTracingManager::TriggerHandle handle =
      content::BackgroundTracingManager::GetInstance()->RegisterTriggerType(
          "preemptive_test");

  EXPECT_TRUE(BackgroundTracingManager::GetInstance()->SetActiveScenario(
      std::move(config), trace_receiver_helper.get_receive_callback(),
      BackgroundTracingManager::NO_DATA_FILTERING));

  background_tracing_helper.WaitForTracingEnabled();

  TestTriggerHelper trigger_helper;
  BackgroundTracingManager::GetInstance()->TriggerNamedEvent(
      handle, trigger_helper.receive_closure(true));
  BackgroundTracingManager::GetInstance()->TriggerNamedEvent(
      handle, trigger_helper.receive_closure(false));

  trace_receiver_helper.WaitForTraceReceived();
  BackgroundTracingManager::GetInstance()->AbortScenarioForTesting();
  background_tracing_helper.WaitForScenarioAborted();

  EXPECT_TRUE(trace_receiver_helper.trace_received());
}

// This tests that non-whitelisted args get stripped if required.
IN_PROC_BROWSER_TEST_F(BackgroundTracingManagerBrowserTest,
                       NotWhitelistedArgsStripped) {
  TestTraceReceiverHelper trace_receiver_helper;
  TestBackgroundTracingHelper background_tracing_helper;

  std::unique_ptr<BackgroundTracingConfig> config = CreatePreemptiveConfig();

  content::BackgroundTracingManager::TriggerHandle handle =
      content::BackgroundTracingManager::GetInstance()->RegisterTriggerType(
          "preemptive_test");

  EXPECT_TRUE(BackgroundTracingManager::GetInstance()->SetActiveScenario(
      std::move(config), trace_receiver_helper.get_receive_callback(),
      BackgroundTracingManager::ANONYMIZE_DATA));
  background_tracing_helper.WaitForTracingEnabled();

  {
    TRACE_EVENT1("benchmark", "TestWhitelist", "test_whitelist", "abc");
    TRACE_EVENT1("benchmark", "TestNotWhitelist", "test_not_whitelist", "abc");
  }

  TestTriggerHelper trigger_helper;
  BackgroundTracingManager::GetInstance()->TriggerNamedEvent(
      handle, trigger_helper.receive_closure(true));
  trigger_helper.WaitForTriggerReceived();

  trace_receiver_helper.WaitForTraceReceived();
  BackgroundTracingManager::GetInstance()->AbortScenarioForTesting();
  background_tracing_helper.WaitForScenarioAborted();

  EXPECT_TRUE(trace_receiver_helper.trace_received());
  EXPECT_TRUE(trace_receiver_helper.TraceHasMatchingString("{"));
  EXPECT_TRUE(trace_receiver_helper.TraceHasMatchingString("test_whitelist"));
  EXPECT_FALSE(
      trace_receiver_helper.TraceHasMatchingString("test_not_whitelist"));
}

// Tests that events emitted by the browser process immediately after the
// SetActiveScenario call does get included in the trace, without waiting for
// the full WaitForTracingEnabled() callback (background tracing will directly
// enable the TraceLog so we get events prior to waiting for the whole IPC
// sequence to enable tracing coming back from the tracing service).
// Temporarily disabled startup tracing on Android to be able to unblock
// Perfetto-based background tracing: https://crbug.com/941318
#if defined(OS_ANDROID)
#define MAYBE_EarlyTraceEventsInTrace DISABLED_EarlyTraceEventsInTrace
#else
#define MAYBE_EarlyTraceEventsInTrace EarlyTraceEventsInTrace
#endif
IN_PROC_BROWSER_TEST_F(BackgroundTracingManagerBrowserTest,
                       MAYBE_EarlyTraceEventsInTrace) {
  TestTraceReceiverHelper trace_receiver_helper;
  TestBackgroundTracingHelper background_tracing_helper;

  std::unique_ptr<BackgroundTracingConfig> config = CreatePreemptiveConfig();

  content::BackgroundTracingManager::TriggerHandle handle =
      content::BackgroundTracingManager::GetInstance()->RegisterTriggerType(
          "preemptive_test");

  EXPECT_TRUE(BackgroundTracingManager::GetInstance()->SetActiveScenario(
      std::move(config), trace_receiver_helper.get_receive_callback(),
      BackgroundTracingManager::ANONYMIZE_DATA));

  { TRACE_EVENT0("benchmark", "TestEarlyEvent"); }

  background_tracing_helper.WaitForTracingEnabled();

  TestTriggerHelper trigger_helper;
  BackgroundTracingManager::GetInstance()->TriggerNamedEvent(
      handle, trigger_helper.receive_closure(true));
  trigger_helper.WaitForTriggerReceived();

  trace_receiver_helper.WaitForTraceReceived();
  BackgroundTracingManager::GetInstance()->AbortScenarioForTesting();
  background_tracing_helper.WaitForScenarioAborted();

  EXPECT_TRUE(trace_receiver_helper.trace_received());
  EXPECT_TRUE(trace_receiver_helper.TraceHasMatchingString("{"));
  EXPECT_TRUE(trace_receiver_helper.TraceHasMatchingString("TestEarlyEvent"));
}

// This tests that browser metadata gets included in the trace.
IN_PROC_BROWSER_TEST_F(BackgroundTracingManagerBrowserTest,
                       TraceMetadataInTrace) {
  TestBackgroundTracingHelper background_tracing_helper;
  TestTraceReceiverHelper trace_receiver_helper;

  std::unique_ptr<BackgroundTracingConfig> config = CreatePreemptiveConfig();

  content::BackgroundTracingManager::TriggerHandle handle =
      content::BackgroundTracingManager::GetInstance()->RegisterTriggerType(
          "preemptive_test");

  EXPECT_TRUE(BackgroundTracingManager::GetInstance()->SetActiveScenario(
      std::move(config), trace_receiver_helper.get_receive_callback(),
      BackgroundTracingManager::ANONYMIZE_DATA));

  background_tracing_helper.WaitForTracingEnabled();

  TestTriggerHelper trigger_helper;
  BackgroundTracingManager::GetInstance()->TriggerNamedEvent(
      handle, trigger_helper.receive_closure(true));

  trace_receiver_helper.WaitForTraceReceived();
  BackgroundTracingManager::GetInstance()->AbortScenarioForTesting();
  background_tracing_helper.WaitForScenarioAborted();

  EXPECT_TRUE(trace_receiver_helper.trace_received());
  EXPECT_TRUE(trace_receiver_helper.TraceHasMatchingString("cpu-brand"));
  EXPECT_TRUE(trace_receiver_helper.TraceHasMatchingString("network-type"));
  EXPECT_TRUE(trace_receiver_helper.TraceHasMatchingString("user-agent"));
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
  TestTraceReceiverHelper trace_receiver_helper;

  std::unique_ptr<BackgroundTracingConfig> config = CreatePreemptiveConfig();

  content::BackgroundTracingManager::TriggerHandle handle =
      content::BackgroundTracingManager::GetInstance()->RegisterTriggerType(
          "preemptive_test");

  EXPECT_TRUE(BackgroundTracingManager::GetInstance()->SetActiveScenario(
      std::move(config), trace_receiver_helper.get_receive_callback(),
      BackgroundTracingManager::ANONYMIZE_DATA));

  background_tracing_helper.WaitForScenarioActivated();

  EXPECT_TRUE(NavigateToURL(shell(), GetTestUrl("", "about:blank")));

  TestTriggerHelper trigger_helper;
  BackgroundTracingManager::GetInstance()->TriggerNamedEvent(
      handle, trigger_helper.receive_closure(true));

  trace_receiver_helper.WaitForTraceReceived();
  BackgroundTracingManager::GetInstance()->AbortScenarioForTesting();
  background_tracing_helper.WaitForScenarioAborted();

  EXPECT_TRUE(trace_receiver_helper.trace_received());
  // We should *not* receive anything at all from the renderer,
  // the process should've crashed rather than letting that happen.
  EXPECT_FALSE(trace_receiver_helper.TraceHasMatchingString("CrRendererMain"));
}

// This tests multiple triggers still only gathers once.
IN_PROC_BROWSER_TEST_F(BackgroundTracingManagerBrowserTest,
                       CallMultipleTriggersOnlyGatherOnce) {
  TestBackgroundTracingHelper background_tracing_helper;
  TestTraceReceiverHelper trace_receiver_helper;

  base::DictionaryValue dict;
  dict.SetString("mode", "PREEMPTIVE_TRACING_MODE");
  dict.SetString("category", "BENCHMARK");

  std::unique_ptr<base::ListValue> rules_list(new base::ListValue());
  {
    std::unique_ptr<base::DictionaryValue> rules_dict(
        new base::DictionaryValue());
    rules_dict->SetString("rule", "MONITOR_AND_DUMP_WHEN_TRIGGER_NAMED");
    rules_dict->SetString("trigger_name", "test1");
    rules_list->Append(std::move(rules_dict));
  }
  {
    std::unique_ptr<base::DictionaryValue> rules_dict(
        new base::DictionaryValue());
    rules_dict->SetString("rule", "MONITOR_AND_DUMP_WHEN_TRIGGER_NAMED");
    rules_dict->SetString("trigger_name", "test2");
    rules_list->Append(std::move(rules_dict));
  }

  dict.Set("configs", std::move(rules_list));

  std::unique_ptr<BackgroundTracingConfig> config(
      BackgroundTracingConfigImpl::FromDict(&dict));
  EXPECT_TRUE(config);

  BackgroundTracingManager::TriggerHandle handle1 =
      BackgroundTracingManager::GetInstance()->RegisterTriggerType("test1");
  BackgroundTracingManager::TriggerHandle handle2 =
      BackgroundTracingManager::GetInstance()->RegisterTriggerType("test2");

  EXPECT_TRUE(BackgroundTracingManager::GetInstance()->SetActiveScenario(
      std::move(config), trace_receiver_helper.get_receive_callback(),
      BackgroundTracingManager::NO_DATA_FILTERING));

  background_tracing_helper.WaitForTracingEnabled();

  TestTriggerHelper trigger_helper;
  BackgroundTracingManager::GetInstance()->TriggerNamedEvent(
      handle1, trigger_helper.receive_closure(true));
  BackgroundTracingManager::GetInstance()->TriggerNamedEvent(
      handle2, trigger_helper.receive_closure(false));

  trace_receiver_helper.WaitForTraceReceived();
  BackgroundTracingManager::GetInstance()->AbortScenarioForTesting();
  background_tracing_helper.WaitForScenarioAborted();

  EXPECT_TRUE(trace_receiver_helper.trace_received());
}

// This tests that delayed histogram triggers work as expected
// with preemptive scenarios.
IN_PROC_BROWSER_TEST_F(BackgroundTracingManagerBrowserTest,
                       CallPreemptiveTriggerWithDelay) {
  TestBackgroundTracingHelper background_tracing_helper;
  TestTraceReceiverHelper trace_receiver_helper;

  base::DictionaryValue dict;
  dict.SetString("mode", "PREEMPTIVE_TRACING_MODE");
  dict.SetString("category", "BENCHMARK");

  std::unique_ptr<base::ListValue> rules_list(new base::ListValue());
  {
    std::unique_ptr<base::DictionaryValue> rules_dict(
        new base::DictionaryValue());
    rules_dict->SetString("rule",
                          "MONITOR_AND_DUMP_WHEN_SPECIFIC_HISTOGRAM_AND_VALUE");
    rules_dict->SetString("histogram_name", "fake");
    rules_dict->SetInteger("histogram_value", 1);
    rules_dict->SetInteger("trigger_delay", 10);
    rules_list->Append(std::move(rules_dict));
  }

  dict.Set("configs", std::move(rules_list));

  std::unique_ptr<BackgroundTracingConfig> config(
      BackgroundTracingConfigImpl::FromDict(&dict));
  EXPECT_TRUE(config);

  base::RunLoop rule_triggered_runloop;
  EXPECT_TRUE(BackgroundTracingManager::GetInstance()->SetActiveScenario(
      std::move(config), trace_receiver_helper.get_receive_callback(),
      BackgroundTracingManager::NO_DATA_FILTERING));

  background_tracing_helper.WaitForTracingEnabled();

  BackgroundTracingManagerImpl::GetInstance()
      ->GetActiveScenarioForTesting()
      ->SetRuleTriggeredCallbackForTesting(
          rule_triggered_runloop.QuitClosure());

  // Our reference value is "1", so a value of "2" should trigger a trace.
  LOCAL_HISTOGRAM_COUNTS("fake", 2);

  rule_triggered_runloop.Run();

  // Since we specified a delay in the scenario, we should still be tracing
  // at this point.
  EXPECT_TRUE(
      BackgroundTracingManagerImpl::GetInstance()->IsTracingForTesting());

  // Fake the timer firing.
  BackgroundTracingManagerImpl::GetInstance()
      ->GetActiveScenarioForTesting()
      ->FireTimerForTesting();

  trace_receiver_helper.WaitForTraceReceived();

  BackgroundTracingManager::GetInstance()->AbortScenarioForTesting();
  background_tracing_helper.WaitForScenarioAborted();

  EXPECT_TRUE(trace_receiver_helper.trace_received());
}

// This tests that you can't trigger without a scenario set.
IN_PROC_BROWSER_TEST_F(BackgroundTracingManagerBrowserTest,
                       CannotTriggerWithoutScenarioSet) {
  content::BackgroundTracingManager::TriggerHandle handle =
      content::BackgroundTracingManager::GetInstance()->RegisterTriggerType(
          "preemptive_test");

  TestTriggerHelper trigger_helper;
  BackgroundTracingManager::GetInstance()->TriggerNamedEvent(
      handle, trigger_helper.receive_closure(false));
  trigger_helper.WaitForTriggerReceived();
}

// This tests that no trace is triggered with a handle that isn't specified
// in the config.
IN_PROC_BROWSER_TEST_F(BackgroundTracingManagerBrowserTest,
                       DoesNotTriggerWithWrongHandle) {
  TestBackgroundTracingHelper background_tracing_helper;
  TestTraceReceiverHelper trace_receiver_helper;

  std::unique_ptr<BackgroundTracingConfig> config = CreatePreemptiveConfig();

  content::BackgroundTracingManager::TriggerHandle handle =
      content::BackgroundTracingManager::GetInstance()->RegisterTriggerType(
          "does_not_exist");

  EXPECT_TRUE(BackgroundTracingManager::GetInstance()->SetActiveScenario(
      std::move(config), trace_receiver_helper.get_receive_callback(),
      BackgroundTracingManager::NO_DATA_FILTERING));

  background_tracing_helper.WaitForTracingEnabled();

  TestTriggerHelper trigger_helper;
  BackgroundTracingManager::GetInstance()->TriggerNamedEvent(
      handle, trigger_helper.receive_closure(false));

  // Abort the scenario.
  BackgroundTracingManager::GetInstance()->AbortScenarioForTesting();
  background_tracing_helper.WaitForScenarioAborted();

  EXPECT_FALSE(trace_receiver_helper.trace_received());
}

// This tests that no trace is triggered with an invalid handle.
IN_PROC_BROWSER_TEST_F(BackgroundTracingManagerBrowserTest,
                       DoesNotTriggerWithInvalidHandle) {
  TestBackgroundTracingHelper background_tracing_helper;
  TestTraceReceiverHelper trace_receiver_helper;

  std::unique_ptr<BackgroundTracingConfig> config = CreatePreemptiveConfig();

  content::BackgroundTracingManager::TriggerHandle handle =
      BackgroundTracingManager::GetInstance()->RegisterTriggerType(
          "preemptive_test");

  BackgroundTracingManagerImpl::GetInstance()
      ->InvalidateTriggerHandlesForTesting();

  EXPECT_TRUE(BackgroundTracingManager::GetInstance()->SetActiveScenario(
      std::move(config), trace_receiver_helper.get_receive_callback(),
      BackgroundTracingManager::NO_DATA_FILTERING));

  background_tracing_helper.WaitForTracingEnabled();

  TestTriggerHelper trigger_helper;
  BackgroundTracingManager::GetInstance()->TriggerNamedEvent(
      handle, trigger_helper.receive_closure(false));

  // Abort the scenario.
  BackgroundTracingManager::GetInstance()->AbortScenarioForTesting();
  background_tracing_helper.WaitForScenarioAborted();

  EXPECT_FALSE(trace_receiver_helper.trace_received());
}

// This tests that no preemptive trace is triggered with 0 chance set.
IN_PROC_BROWSER_TEST_F(BackgroundTracingManagerBrowserTest,
                       PreemptiveNotTriggerWithZeroChance) {
  TestBackgroundTracingHelper background_tracing_helper;
  TestTraceReceiverHelper trace_receiver_helper;

  base::DictionaryValue dict;
  dict.SetString("mode", "PREEMPTIVE_TRACING_MODE");
  dict.SetString("category", "BENCHMARK");

  std::unique_ptr<base::ListValue> rules_list(new base::ListValue());
  {
    std::unique_ptr<base::DictionaryValue> rules_dict(
        new base::DictionaryValue());
    rules_dict->SetString("rule", "MONITOR_AND_DUMP_WHEN_TRIGGER_NAMED");
    rules_dict->SetString("trigger_name", "preemptive_test");
    rules_dict->SetDouble("trigger_chance", 0.0);
    rules_list->Append(std::move(rules_dict));
  }
  dict.Set("configs", std::move(rules_list));

  std::unique_ptr<BackgroundTracingConfig> config(
      BackgroundTracingConfigImpl::FromDict(&dict));
  EXPECT_TRUE(config);

  content::BackgroundTracingManager::TriggerHandle handle =
      content::BackgroundTracingManager::GetInstance()->RegisterTriggerType(
          "preemptive_test");

  EXPECT_TRUE(BackgroundTracingManager::GetInstance()->SetActiveScenario(
      std::move(config), trace_receiver_helper.get_receive_callback(),
      BackgroundTracingManager::NO_DATA_FILTERING));

  background_tracing_helper.WaitForTracingEnabled();

  TestTriggerHelper trigger_helper;
  BackgroundTracingManager::GetInstance()->TriggerNamedEvent(
      handle, trigger_helper.receive_closure(false));

  // Abort the scenario.
  BackgroundTracingManager::GetInstance()->AbortScenarioForTesting();
  background_tracing_helper.WaitForScenarioAborted();

  EXPECT_FALSE(trace_receiver_helper.trace_received());
}

// This tests that no reactive trace is triggered with 0 chance set.
IN_PROC_BROWSER_TEST_F(BackgroundTracingManagerBrowserTest,
                       ReactiveNotTriggerWithZeroChance) {
  TestBackgroundTracingHelper background_tracing_helper;
  TestTraceReceiverHelper trace_receiver_helper;

  base::DictionaryValue dict;
  dict.SetString("mode", "REACTIVE_TRACING_MODE");

  std::unique_ptr<base::ListValue> rules_list(new base::ListValue());
  {
    std::unique_ptr<base::DictionaryValue> rules_dict(
        new base::DictionaryValue());
    rules_dict->SetString("rule", "TRACE_ON_NAVIGATION_UNTIL_TRIGGER_OR_FULL");
    rules_dict->SetString("trigger_name", "reactive_test1");
    rules_dict->SetString("category", "BENCHMARK");
    rules_dict->SetDouble("trigger_chance", 0.0);

    rules_list->Append(std::move(rules_dict));
  }
  dict.Set("configs", std::move(rules_list));

  std::unique_ptr<BackgroundTracingConfig> config(
      BackgroundTracingConfigImpl::FromDict(&dict));
  EXPECT_TRUE(config);

  content::BackgroundTracingManager::TriggerHandle handle =
      content::BackgroundTracingManager::GetInstance()->RegisterTriggerType(
          "preemptive_test");

  EXPECT_TRUE(BackgroundTracingManager::GetInstance()->SetActiveScenario(
      std::move(config), trace_receiver_helper.get_receive_callback(),
      BackgroundTracingManager::NO_DATA_FILTERING));

  TestTriggerHelper trigger_helper;
  BackgroundTracingManager::GetInstance()->TriggerNamedEvent(
      handle, trigger_helper.receive_closure(false));
  trigger_helper.WaitForTriggerReceived();

  // Abort the scenario.
  BackgroundTracingManager::GetInstance()->AbortScenarioForTesting();
  background_tracing_helper.WaitForScenarioAborted();

  EXPECT_FALSE(trace_receiver_helper.trace_received());
}

// This tests that histogram triggers for preemptive mode configs.
IN_PROC_BROWSER_TEST_F(BackgroundTracingManagerBrowserTest,
                       ReceiveTraceSucceedsOnHigherHistogramSample) {
  TestBackgroundTracingHelper background_tracing_helper;
  TestTraceReceiverHelper trace_receiver_helper;

  base::DictionaryValue dict;
  dict.SetString("mode", "PREEMPTIVE_TRACING_MODE");
  dict.SetString("category", "BENCHMARK");

  std::unique_ptr<base::ListValue> rules_list(new base::ListValue());
  {
    std::unique_ptr<base::DictionaryValue> rules_dict(
        new base::DictionaryValue());
    rules_dict->SetString("rule",
                          "MONITOR_AND_DUMP_WHEN_SPECIFIC_HISTOGRAM_AND_VALUE");
    rules_dict->SetString("histogram_name", "fake");
    rules_dict->SetInteger("histogram_value", 1);
    rules_list->Append(std::move(rules_dict));
  }

  dict.Set("configs", std::move(rules_list));

  std::unique_ptr<BackgroundTracingConfig> config(
      BackgroundTracingConfigImpl::FromDict(&dict));
  EXPECT_TRUE(config);

  EXPECT_TRUE(BackgroundTracingManager::GetInstance()->SetActiveScenario(
      std::move(config), trace_receiver_helper.get_receive_callback(),
      BackgroundTracingManager::NO_DATA_FILTERING));

  background_tracing_helper.WaitForTracingEnabled();

  // Our reference value is "1", so a value of "2" should trigger a trace.
  LOCAL_HISTOGRAM_COUNTS("fake", 2);

  trace_receiver_helper.WaitForTraceReceived();

  EXPECT_TRUE(trace_receiver_helper.trace_received());

  base::Optional<base::Value> trace_json =
      base::JSONReader::Read(trace_receiver_helper.file_contents());
  ASSERT_TRUE(trace_json);
  auto* metadata_json = static_cast<base::DictionaryValue*>(
      trace_json->FindKeyOfType("metadata", base::Value::Type::DICTIONARY));
  ASSERT_TRUE(metadata_json);

  const std::string* trace_config =
      metadata_json->FindStringKey("trace-config");
  ASSERT_TRUE(trace_config);
  EXPECT_NE(trace_config->find("record-continuously"), trace_config->npos)
      << *trace_config;

  EXPECT_TRUE(BackgroundTracingManager::GetInstance()->HasActiveScenario());

  BackgroundTracingManager::GetInstance()->AbortScenarioForTesting();
  background_tracing_helper.WaitForScenarioAborted();
}

IN_PROC_BROWSER_TEST_F(BackgroundTracingManagerBrowserTest, CustomConfig) {
  TestBackgroundTracingHelper background_tracing_helper;
  TestTraceReceiverHelper trace_receiver_helper;

  base::DictionaryValue dict;
  dict.SetString("mode", "PREEMPTIVE_TRACING_MODE");
  dict.SetString("category", "BENCHMARK");
  dict.SetKey("trace_config", std::move(*base::JSONReader::Read(R"(
        {
          "included_categories": ["*"],
          "record_mode": "record-until-full"
        })")));

  std::unique_ptr<base::ListValue> rules_list(new base::ListValue());
  {
    std::unique_ptr<base::DictionaryValue> rules_dict(
        new base::DictionaryValue());
    rules_dict->SetString("rule",
                          "MONITOR_AND_DUMP_WHEN_SPECIFIC_HISTOGRAM_AND_VALUE");
    rules_dict->SetString("histogram_name", "fake");
    rules_dict->SetInteger("histogram_value", 1);
    rules_list->Append(std::move(rules_dict));
  }

  dict.Set("configs", std::move(rules_list));

  std::unique_ptr<BackgroundTracingConfig> config(
      BackgroundTracingConfigImpl::FromDict(&dict));
  EXPECT_TRUE(config);

  EXPECT_TRUE(BackgroundTracingManager::GetInstance()->SetActiveScenario(
      std::move(config), trace_receiver_helper.get_receive_callback(),
      BackgroundTracingManager::NO_DATA_FILTERING));

  background_tracing_helper.WaitForTracingEnabled();

  // Our reference value is "1", so a value of "2" should trigger a trace.
  LOCAL_HISTOGRAM_COUNTS("fake", 2);

  trace_receiver_helper.WaitForTraceReceived();
  BackgroundTracingManager::GetInstance()->AbortScenarioForTesting();
  background_tracing_helper.WaitForScenarioAborted();

  EXPECT_TRUE(trace_receiver_helper.trace_received());

  base::Optional<base::Value> trace_json =
      base::JSONReader::Read(trace_receiver_helper.file_contents());
  ASSERT_TRUE(trace_json);
  auto* metadata_json = static_cast<base::DictionaryValue*>(
      trace_json->FindKeyOfType("metadata", base::Value::Type::DICTIONARY));
  ASSERT_TRUE(metadata_json);

  const std::string* trace_config =
      metadata_json->FindStringKey("trace-config");
  ASSERT_TRUE(trace_config);
  EXPECT_NE(trace_config->find("record-until-full"), trace_config->npos)
      << *trace_config;
}

// This tests that histogram triggers for reactive mode configs.
IN_PROC_BROWSER_TEST_F(BackgroundTracingManagerBrowserTest,
                       ReceiveReactiveTraceSucceedsOnHigherHistogramSample) {
  TestBackgroundTracingHelper background_tracing_helper;
  TestTraceReceiverHelper trace_receiver_helper;

  base::DictionaryValue dict;
  dict.SetString("mode", "REACTIVE_TRACING_MODE");

  std::unique_ptr<base::ListValue> rules_list(new base::ListValue());
  {
    std::unique_ptr<base::DictionaryValue> rules_dict(
        new base::DictionaryValue());
    rules_dict->SetString("rule",
                          "MONITOR_AND_DUMP_WHEN_SPECIFIC_HISTOGRAM_AND_VALUE");
    rules_dict->SetString("histogram_name", "fake");
    rules_dict->SetInteger("histogram_value", 1);
    rules_dict->SetString("category", "BENCHMARK");
    rules_list->Append(std::move(rules_dict));
  }

  dict.Set("configs", std::move(rules_list));

  std::unique_ptr<BackgroundTracingConfig> config(
      BackgroundTracingConfigImpl::FromDict(&dict));
  EXPECT_TRUE(config);

  EXPECT_TRUE(BackgroundTracingManager::GetInstance()->SetActiveScenario(
      std::move(config), trace_receiver_helper.get_receive_callback(),
      BackgroundTracingManager::NO_DATA_FILTERING));

  background_tracing_helper.WaitForScenarioActivated();

  // Our reference value is "1", so a value of "2" should trigger a trace.
  LOCAL_HISTOGRAM_COUNTS("fake", 2);

  trace_receiver_helper.WaitForTraceReceived();

  // Abort the scenario.
  BackgroundTracingManager::GetInstance()->AbortScenarioForTesting();
  background_tracing_helper.WaitForScenarioAborted();

  EXPECT_TRUE(trace_receiver_helper.trace_received());
}

// This tests that histogram values < reference value don't trigger.
IN_PROC_BROWSER_TEST_F(BackgroundTracingManagerBrowserTest,
                       ReceiveTraceFailsOnLowerHistogramSample) {
  TestBackgroundTracingHelper background_tracing_helper;
  TestTraceReceiverHelper trace_receiver_helper;

  base::DictionaryValue dict;
  dict.SetString("mode", "PREEMPTIVE_TRACING_MODE");
  dict.SetString("category", "BENCHMARK");

  std::unique_ptr<base::ListValue> rules_list(new base::ListValue());
  {
    std::unique_ptr<base::DictionaryValue> rules_dict(
        new base::DictionaryValue());
    rules_dict->SetString("rule",
                          "MONITOR_AND_DUMP_WHEN_SPECIFIC_HISTOGRAM_AND_VALUE");
    rules_dict->SetString("histogram_name", "fake");
    rules_dict->SetInteger("histogram_value", 1);
    rules_list->Append(std::move(rules_dict));
  }

  dict.Set("configs", std::move(rules_list));

  std::unique_ptr<BackgroundTracingConfig> config(
      BackgroundTracingConfigImpl::FromDict(&dict));
  EXPECT_TRUE(config);

  EXPECT_TRUE(BackgroundTracingManager::GetInstance()->SetActiveScenario(
      std::move(config), trace_receiver_helper.get_receive_callback(),
      BackgroundTracingManager::NO_DATA_FILTERING));

  background_tracing_helper.WaitForTracingEnabled();

  // This should fail to trigger a trace since the sample value < the
  // the reference value above.
  LOCAL_HISTOGRAM_COUNTS("fake", 0);

  // Abort the scenario.
  BackgroundTracingManager::GetInstance()->AbortScenarioForTesting();
  background_tracing_helper.WaitForScenarioAborted();

  EXPECT_FALSE(trace_receiver_helper.trace_received());
}

// This tests that histogram values > upper reference value don't trigger.
IN_PROC_BROWSER_TEST_F(BackgroundTracingManagerBrowserTest,
                       ReceiveTraceFailsOnHigherHistogramSample) {
  TestBackgroundTracingHelper background_tracing_helper;
  TestTraceReceiverHelper trace_receiver_helper;

  base::DictionaryValue dict;
  dict.SetString("mode", "PREEMPTIVE_TRACING_MODE");
  dict.SetString("category", "BENCHMARK");

  std::unique_ptr<base::ListValue> rules_list(new base::ListValue());
  {
    std::unique_ptr<base::DictionaryValue> rules_dict(
        new base::DictionaryValue());
    rules_dict->SetString("rule",
                          "MONITOR_AND_DUMP_WHEN_SPECIFIC_HISTOGRAM_AND_VALUE");
    rules_dict->SetString("histogram_name", "fake");
    rules_dict->SetInteger("histogram_lower_value", 1);
    rules_dict->SetInteger("histogram_upper_value", 3);
    rules_list->Append(std::move(rules_dict));
  }

  dict.Set("configs", std::move(rules_list));

  std::unique_ptr<BackgroundTracingConfig> config(
      BackgroundTracingConfigImpl::FromDict(&dict));
  EXPECT_TRUE(config);

  EXPECT_TRUE(BackgroundTracingManager::GetInstance()->SetActiveScenario(
      std::move(config), trace_receiver_helper.get_receive_callback(),
      BackgroundTracingManager::NO_DATA_FILTERING));

  background_tracing_helper.WaitForTracingEnabled();

  // This should fail to trigger a trace since the sample value > the
  // the upper reference value above.
  LOCAL_HISTOGRAM_COUNTS("fake", 0);

  // Abort the scenario.
  BackgroundTracingManager::GetInstance()->AbortScenarioForTesting();
  background_tracing_helper.WaitForScenarioAborted();

  EXPECT_FALSE(trace_receiver_helper.trace_received());
}

// This tests that invalid preemptive mode configs will fail.
IN_PROC_BROWSER_TEST_F(BackgroundTracingManagerBrowserTest,
                       SetActiveScenarioFailsWithInvalidPreemptiveConfig) {
  base::DictionaryValue dict;
  dict.SetString("mode", "PREEMPTIVE_TRACING_MODE");
  dict.SetString("category", "BENCHMARK");

  std::unique_ptr<base::ListValue> rules_list(new base::ListValue());
  {
    std::unique_ptr<base::DictionaryValue> rules_dict(
        new base::DictionaryValue());
    rules_dict->SetString("rule", "INVALID_RULE");
    rules_list->Append(std::move(rules_dict));
  }

  dict.Set("configs", std::move(rules_list));

  std::unique_ptr<BackgroundTracingConfig> config(
      BackgroundTracingConfigImpl::FromDict(&dict));
  // An invalid config should always return a nullptr here.
  EXPECT_FALSE(config);
}

// This tests that reactive mode records and terminates with timeout.
IN_PROC_BROWSER_TEST_F(BackgroundTracingManagerBrowserTest,
                       ReactiveTimeoutTermination) {
  TestBackgroundTracingHelper background_tracing_helper;
  TestTraceReceiverHelper trace_receiver_helper;

  std::unique_ptr<BackgroundTracingConfig> config = CreateReactiveConfig();

  BackgroundTracingManager::TriggerHandle handle =
      BackgroundTracingManager::GetInstance()->RegisterTriggerType(
          "reactive_test");

  EXPECT_TRUE(BackgroundTracingManager::GetInstance()->SetActiveScenario(
      std::move(config), trace_receiver_helper.get_receive_callback(),
      BackgroundTracingManager::NO_DATA_FILTERING));

  TestTriggerHelper trigger_helper;
  BackgroundTracingManager::GetInstance()->TriggerNamedEvent(
      handle, trigger_helper.receive_closure(true));

  BackgroundTracingManagerImpl::GetInstance()
      ->GetActiveScenarioForTesting()
      ->FireTimerForTesting();

  trace_receiver_helper.WaitForTraceReceived();
  BackgroundTracingManager::GetInstance()->AbortScenarioForTesting();
  background_tracing_helper.WaitForScenarioAborted();

  EXPECT_TRUE(trace_receiver_helper.trace_received());
}

// This tests that reactive mode records and terminates with a second trigger.
IN_PROC_BROWSER_TEST_F(BackgroundTracingManagerBrowserTest,
                       ReactiveSecondTriggerTermination) {
  TestBackgroundTracingHelper background_tracing_helper;
  TestTraceReceiverHelper trace_receiver_helper;

  std::unique_ptr<BackgroundTracingConfig> config = CreateReactiveConfig();

  BackgroundTracingManager::TriggerHandle handle =
      BackgroundTracingManager::GetInstance()->RegisterTriggerType(
          "reactive_test");

  EXPECT_TRUE(BackgroundTracingManager::GetInstance()->SetActiveScenario(
      std::move(config), trace_receiver_helper.get_receive_callback(),
      BackgroundTracingManager::NO_DATA_FILTERING));

  TestTriggerHelper trigger_helper;
  BackgroundTracingManager::GetInstance()->TriggerNamedEvent(
      handle, trigger_helper.receive_closure(true));
  // second trigger to terminate.
  BackgroundTracingManager::GetInstance()->TriggerNamedEvent(
      handle, trigger_helper.receive_closure(true));

  trace_receiver_helper.WaitForTraceReceived();
  BackgroundTracingManager::GetInstance()->AbortScenarioForTesting();
  background_tracing_helper.WaitForScenarioAborted();

  EXPECT_TRUE(trace_receiver_helper.trace_received());
}

// This tests that reactive mode uploads on a second set of triggers.
IN_PROC_BROWSER_TEST_F(BackgroundTracingManagerBrowserTest,
                       ReactiveSecondUpload) {
  TestBackgroundTracingHelper background_tracing_helper;
  TestMultipleTraceReceiverHelper trace_receiver_helper;

  std::unique_ptr<BackgroundTracingConfig> config = CreateReactiveConfig();

  BackgroundTracingManager::TriggerHandle handle =
      BackgroundTracingManager::GetInstance()->RegisterTriggerType(
          "reactive_test");

  EXPECT_TRUE(BackgroundTracingManager::GetInstance()->SetActiveScenario(
      std::move(config), trace_receiver_helper.get_receive_callback(),
      BackgroundTracingManager::NO_DATA_FILTERING));

  background_tracing_helper.WaitForScenarioActivated();

  TestTriggerHelper trigger_helper;
  BackgroundTracingManager::GetInstance()->TriggerNamedEvent(
      handle, trigger_helper.receive_closure(true));

  // second trigger to terminate.
  BackgroundTracingManager::GetInstance()->TriggerNamedEvent(
      handle, trigger_helper.receive_closure(true));

  trace_receiver_helper.WaitForTraceReceived(0);
  EXPECT_TRUE(trace_receiver_helper.trace_received(0));

  BackgroundTracingManager::GetInstance()->TriggerNamedEvent(
      handle, trigger_helper.receive_closure(true));
  // second trigger to terminate.
  BackgroundTracingManager::GetInstance()->TriggerNamedEvent(
      handle, trigger_helper.receive_closure(true));

  trace_receiver_helper.WaitForTraceReceived(1);
  EXPECT_TRUE(trace_receiver_helper.trace_received(1));

  // Abort the scenario.
  BackgroundTracingManager::GetInstance()->AbortScenarioForTesting();
  background_tracing_helper.WaitForScenarioAborted();
}

// This tests that reactive mode only terminates with the same trigger.
IN_PROC_BROWSER_TEST_F(BackgroundTracingManagerBrowserTest,
                       ReactiveSecondTriggerMustMatchForTermination) {
  TestBackgroundTracingHelper background_tracing_helper;
  TestTraceReceiverHelper trace_receiver_helper;

  base::DictionaryValue dict;
  dict.SetString("mode", "REACTIVE_TRACING_MODE");

  std::unique_ptr<base::ListValue> rules_list(new base::ListValue());
  {
    std::unique_ptr<base::DictionaryValue> rules_dict(
        new base::DictionaryValue());
    rules_dict->SetString("rule", "TRACE_ON_NAVIGATION_UNTIL_TRIGGER_OR_FULL");
    rules_dict->SetString("trigger_name", "reactive_test1");
    rules_dict->SetBoolean("stop_tracing_on_repeated_reactive", true);
    rules_dict->SetInteger("trigger_delay", 10);
    rules_dict->SetString("category", "BENCHMARK");
    rules_list->Append(std::move(rules_dict));
  }
  {
    std::unique_ptr<base::DictionaryValue> rules_dict(
        new base::DictionaryValue());
    rules_dict->SetString("rule", "TRACE_ON_NAVIGATION_UNTIL_TRIGGER_OR_FULL");
    rules_dict->SetString("trigger_name", "reactive_test2");
    rules_dict->SetBoolean("stop_tracing_on_repeated_reactive", true);
    rules_dict->SetInteger("trigger_delay", 10);
    rules_dict->SetString("category", "BENCHMARK");
    rules_list->Append(std::move(rules_dict));
  }
  dict.Set("configs", std::move(rules_list));

  std::unique_ptr<BackgroundTracingConfig> config(
      BackgroundTracingConfigImpl::FromDict(&dict));

  BackgroundTracingManager::TriggerHandle handle1 =
      BackgroundTracingManager::GetInstance()->RegisterTriggerType(
          "reactive_test1");
  BackgroundTracingManager::TriggerHandle handle2 =
      BackgroundTracingManager::GetInstance()->RegisterTriggerType(
          "reactive_test2");

  EXPECT_TRUE(BackgroundTracingManager::GetInstance()->SetActiveScenario(
      std::move(config), trace_receiver_helper.get_receive_callback(),
      BackgroundTracingManager::NO_DATA_FILTERING));

  background_tracing_helper.WaitForScenarioActivated();

  TestTriggerHelper trigger_helper;
  BackgroundTracingManager::GetInstance()->TriggerNamedEvent(
      handle1, trigger_helper.receive_closure(true));

  // This is expected to fail since we triggered with handle1.
  BackgroundTracingManager::GetInstance()->TriggerNamedEvent(
      handle2, trigger_helper.receive_closure(false));

  // second trigger to terminate.
  BackgroundTracingManager::GetInstance()->TriggerNamedEvent(
      handle1, trigger_helper.receive_closure(true));

  trace_receiver_helper.WaitForTraceReceived();
  BackgroundTracingManager::GetInstance()->AbortScenarioForTesting();
  background_tracing_helper.WaitForScenarioAborted();

  EXPECT_TRUE(trace_receiver_helper.trace_received());
}

// This tests a third trigger in reactive more does not start another trace.
IN_PROC_BROWSER_TEST_F(BackgroundTracingManagerBrowserTest,
                       ReactiveThirdTriggerTimeout) {
  TestBackgroundTracingHelper background_tracing_helper;
  TestTraceReceiverHelper trace_receiver_helper;

  std::unique_ptr<BackgroundTracingConfig> config = CreateReactiveConfig();

  BackgroundTracingManager::TriggerHandle handle =
      BackgroundTracingManager::GetInstance()->RegisterTriggerType(
          "reactive_test");

  EXPECT_TRUE(BackgroundTracingManager::GetInstance()->SetActiveScenario(
      std::move(config), trace_receiver_helper.get_receive_callback(),
      BackgroundTracingManager::NO_DATA_FILTERING));

  background_tracing_helper.WaitForScenarioActivated();

  TestTriggerHelper trigger_helper;
  BackgroundTracingManager::GetInstance()->TriggerNamedEvent(
      handle, trigger_helper.receive_closure(true));
  // second trigger to terminate.
  BackgroundTracingManager::GetInstance()->TriggerNamedEvent(
      handle, trigger_helper.receive_closure(true));
  // third trigger to trigger again, fails as it is still gathering.
  BackgroundTracingManager::GetInstance()->TriggerNamedEvent(
      handle, trigger_helper.receive_closure(false));

  trace_receiver_helper.WaitForTraceReceived();
  BackgroundTracingManager::GetInstance()->AbortScenarioForTesting();
  background_tracing_helper.WaitForScenarioAborted();

  EXPECT_TRUE(trace_receiver_helper.trace_received());
}

// This tests that reactive mode only terminates with a repeated trigger
// if the config specifies that it should.
IN_PROC_BROWSER_TEST_F(BackgroundTracingManagerBrowserTest,
                       ReactiveSecondTriggerIgnored) {
  TestBackgroundTracingHelper background_tracing_helper;
  TestTraceReceiverHelper trace_receiver_helper;

  base::DictionaryValue dict;
  dict.SetString("mode", "REACTIVE_TRACING_MODE");

  std::unique_ptr<base::ListValue> rules_list(new base::ListValue());
  {
    std::unique_ptr<base::DictionaryValue> rules_dict(
        new base::DictionaryValue());
    rules_dict->SetString("rule", "TRACE_ON_NAVIGATION_UNTIL_TRIGGER_OR_FULL");
    rules_dict->SetString("trigger_name", "reactive_test");
    rules_dict->SetBoolean("stop_tracing_on_repeated_reactive", false);
    rules_dict->SetInteger("trigger_delay", 10);
    rules_dict->SetString("category", "BENCHMARK");
    rules_list->Append(std::move(rules_dict));
  }
  dict.Set("configs", std::move(rules_list));

  std::unique_ptr<BackgroundTracingConfig> config(
      BackgroundTracingConfigImpl::FromDict(&dict));

  BackgroundTracingManager::TriggerHandle trigger_handle =
      BackgroundTracingManager::GetInstance()->RegisterTriggerType(
          "reactive_test");

  EXPECT_TRUE(BackgroundTracingManager::GetInstance()->SetActiveScenario(
      std::move(config), trace_receiver_helper.get_receive_callback(),
      BackgroundTracingManager::NO_DATA_FILTERING));

  TestTriggerHelper trigger_helper;
  BackgroundTracingManager::GetInstance()->TriggerNamedEvent(
      trigger_handle, trigger_helper.receive_closure(true));

  background_tracing_helper.WaitForTracingEnabled();

  // This is expected to fail since we already triggered.
  BackgroundTracingManager::GetInstance()->TriggerNamedEvent(
      trigger_handle, trigger_helper.receive_closure(false));

  // Since we specified a delay in the scenario, we should still be tracing
  // at this point.
  EXPECT_TRUE(
      BackgroundTracingManagerImpl::GetInstance()->IsTracingForTesting());

  BackgroundTracingManagerImpl::GetInstance()
      ->GetActiveScenarioForTesting()
      ->FireTimerForTesting();

  trace_receiver_helper.WaitForTraceReceived();
  BackgroundTracingManager::GetInstance()->AbortScenarioForTesting();
  background_tracing_helper.WaitForScenarioAborted();

  EXPECT_TRUE(trace_receiver_helper.trace_received());
}

IN_PROC_BROWSER_TEST_F(BackgroundTracingManagerBrowserTest,
                       SetupStartupTracing) {
  TestBackgroundTracingHelper background_tracing_helper;
  TestTraceReceiverHelper trace_receiver_helper;

  std::unique_ptr<TestStartupPreferenceManagerImpl> preferences_moved(
      new TestStartupPreferenceManagerImpl);
  TestStartupPreferenceManagerImpl* preferences = preferences_moved.get();
  BackgroundStartupTracingObserver::GetInstance()
      ->SetPreferenceManagerForTesting(std::move(preferences_moved));
  preferences->SetBackgroundStartupTracingEnabled(false);

  base::DictionaryValue dict;
  std::unique_ptr<base::ListValue> rules_list(new base::ListValue());
  {
    std::unique_ptr<base::DictionaryValue> rules_dict(
        new base::DictionaryValue());
    rules_dict->SetString("rule", "MONITOR_AND_DUMP_WHEN_TRIGGER_NAMED");
    rules_dict->SetString("trigger_name", "startup-config");
    rules_dict->SetBoolean("stop_tracing_on_repeated_reactive", false);
    rules_dict->SetInteger("trigger_delay", 600);
    rules_dict->SetString("category", "BENCHMARK_STARTUP");
    rules_list->Append(std::move(rules_dict));
  }
  dict.Set("configs", std::move(rules_list));

  std::unique_ptr<BackgroundTracingConfig> config(
      BackgroundTracingConfigImpl::ReactiveFromDict(&dict));

  EXPECT_TRUE(BackgroundTracingManager::GetInstance()->SetActiveScenario(
      std::move(config), trace_receiver_helper.get_receive_callback(),
      BackgroundTracingManager::NO_DATA_FILTERING));

  background_tracing_helper.WaitForScenarioActivated();

  // Since we specified a delay in the scenario, we should still be tracing
  // at this point.
  EXPECT_FALSE(
      BackgroundTracingManagerImpl::GetInstance()->IsTracingForTesting());
  EXPECT_TRUE(preferences->GetBackgroundStartupTracingEnabled());

  // Abort the scenario.
  BackgroundTracingManager::GetInstance()->AbortScenarioForTesting();
  background_tracing_helper.WaitForScenarioAborted();

  EXPECT_FALSE(trace_receiver_helper.trace_received());
}

IN_PROC_BROWSER_TEST_F(BackgroundTracingManagerBrowserTest, RunStartupTracing) {
  TestTraceLogHelper tracelog_helper;
  TestBackgroundTracingHelper background_tracing_helper;
  TestTraceReceiverHelper trace_receiver_helper;

  std::unique_ptr<TestStartupPreferenceManagerImpl> preferences_moved(
      new TestStartupPreferenceManagerImpl);
  TestStartupPreferenceManagerImpl* preferences = preferences_moved.get();
  BackgroundStartupTracingObserver::GetInstance()
      ->SetPreferenceManagerForTesting(std::move(preferences_moved));
  preferences->SetBackgroundStartupTracingEnabled(true);

  base::DictionaryValue dict;
  std::unique_ptr<base::ListValue> rules_list(new base::ListValue());
  {
    std::unique_ptr<base::DictionaryValue> rules_dict(
        new base::DictionaryValue());
    rules_dict->SetString("rule", "MONITOR_AND_DUMP_WHEN_TRIGGER_NAMED");
    rules_dict->SetString("trigger_name", "gpu-config");
    rules_dict->SetBoolean("stop_tracing_on_repeated_reactive", false);
    rules_dict->SetInteger("trigger_delay", 10);
    rules_dict->SetString("category", "BENCHMARK_GPU");
    rules_list->Append(std::move(rules_dict));
  }
  dict.Set("configs", std::move(rules_list));

  std::unique_ptr<BackgroundTracingConfig> config(
      BackgroundTracingConfigImpl::ReactiveFromDict(&dict));

  EXPECT_TRUE(BackgroundTracingManager::GetInstance()->SetActiveScenario(
      std::move(config), trace_receiver_helper.get_receive_callback(),
      BackgroundTracingManager::ANONYMIZE_DATA));

  tracelog_helper.WaitForStartTracing();
  background_tracing_helper.WaitForTracingEnabled();

  EXPECT_TRUE(BackgroundTracingManagerImpl::GetInstance()
                  ->GetActiveScenarioForTesting()
                  ->GetConfig()
                  ->requires_anonymized_data());

  // Since we specified a delay in the scenario, we should still be tracing
  // at this point.
  EXPECT_TRUE(
      BackgroundTracingManagerImpl::GetInstance()->IsTracingForTesting());

  BackgroundTracingManagerImpl::GetInstance()
      ->GetActiveScenarioForTesting()
      ->FireTimerForTesting();

  trace_receiver_helper.WaitForTraceReceived();
  BackgroundTracingManager::GetInstance()->AbortScenarioForTesting();
  background_tracing_helper.WaitForScenarioAborted();

  EXPECT_TRUE(trace_receiver_helper.trace_received());
  EXPECT_FALSE(preferences->GetBackgroundStartupTracingEnabled());
}

namespace {

class ProtoBackgroundTracingTest : public DevToolsProtocolTest {
 public:
  ProtoBackgroundTracingTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kBackgroundTracingProtoOutput},
        /*disabled_features=*/{});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

}  // namespace

IN_PROC_BROWSER_TEST_F(ProtoBackgroundTracingTest,
                       DevtoolsInterruptsBackgroundTracing) {
  TestBackgroundTracingHelper background_tracing_helper;
  TestTraceReceiverHelper trace_receiver_helper;

  std::unique_ptr<BackgroundTracingConfig> config = CreatePreemptiveConfig();

  EXPECT_TRUE(BackgroundTracingManager::GetInstance()->SetActiveScenario(
      std::move(config), trace_receiver_helper.get_receive_callback(),
      BackgroundTracingManager::NO_DATA_FILTERING));

  background_tracing_helper.WaitForTracingEnabled();

  NavigateToURLBlockUntilNavigationsComplete(shell(), GURL("about:blank"), 1);
  Attach();

  base::Value* start_tracing_result =
      SendCommand("Tracing.start", nullptr, true);
  ASSERT_TRUE(start_tracing_result);
  BackgroundTracingManager::GetInstance()->AbortScenarioForTesting();
  background_tracing_helper.WaitForScenarioAborted();
}

IN_PROC_BROWSER_TEST_F(ProtoBackgroundTracingTest, ProtoTraceReceived) {
  TestBackgroundTracingHelper background_tracing_helper;

  std::unique_ptr<BackgroundTracingConfig> config = CreatePreemptiveConfig();

  BackgroundTracingManager::TriggerHandle handle =
      BackgroundTracingManager::GetInstance()->RegisterTriggerType(
          "preemptive_test");

  EXPECT_TRUE(BackgroundTracingManager::GetInstance()->SetActiveScenario(
      std::move(config), base::DoNothing(),
      BackgroundTracingManager::ANONYMIZE_DATA));

  background_tracing_helper.WaitForTracingEnabled();

  NavigateToURLBlockUntilNavigationsComplete(shell(), GURL("about:blank"), 1);

  TestTriggerHelper trigger_helper;
  BackgroundTracingManager::GetInstance()->TriggerNamedEvent(
      handle, trigger_helper.receive_closure(true));

  WaitForCondition(
      base::BindRepeating([]() {
        return BackgroundTracingManager::GetInstance()->HasTraceToUpload();
      }),
      "trace received");

  std::string trace_data =
      BackgroundTracingManager::GetInstance()->GetLatestTraceToUpload();

  BackgroundTracingManager::GetInstance()->AbortScenarioForTesting();
  background_tracing_helper.WaitForScenarioAborted();

  tracing::PrivacyFilteringCheck checker;
  checker.CheckProtoForUnexpectedFields(trace_data);
  EXPECT_GT(checker.stats().track_event, 0u);
  EXPECT_GT(checker.stats().process_desc, 0u);
  EXPECT_GT(checker.stats().thread_desc, 0u);
  EXPECT_TRUE(checker.stats().has_interned_names);
  EXPECT_TRUE(checker.stats().has_interned_categories);
  EXPECT_TRUE(checker.stats().has_interned_source_locations);
}

#if defined(OS_ANDROID)
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

  // "system_test" is a NamedTriggerRule in CreateSystemConfig().
  BackgroundTracingManager::TriggerHandle handle =
      BackgroundTracingManager::GetInstance()->RegisterTriggerType(
          "system_test");

  // trace_receiver_helper's function will not be called for SYSTEM background
  // trace.
  TestTraceReceiverHelper trace_receiver_helper;
  std::unique_ptr<BackgroundTracingConfig> config = CreateSystemConfig();
  ASSERT_TRUE(config);
  ASSERT_TRUE(BackgroundTracingManager::GetInstance()->SetActiveScenario(
      std::move(config), trace_receiver_helper.get_receive_callback(),
      BackgroundTracingManager::NO_DATA_FILTERING));

  // Actually send the trigger into the system.
  base::RunLoop rule_triggered_runloop;
  BackgroundTracingManagerImpl::GetInstance()
      ->GetActiveScenarioForTesting()
      ->SetRuleTriggeredCallbackForTesting(
          rule_triggered_runloop.QuitClosure());
  TestTriggerHelper trigger_helper;
  BackgroundTracingManager::GetInstance()->TriggerNamedEvent(
      handle, trigger_helper.receive_closure(true));
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

  // "system_test" is a NamedTriggerRule in CreateSystemConfig().
  BackgroundTracingManager::TriggerHandle handle =
      BackgroundTracingManager::GetInstance()->RegisterTriggerType(
          "system_test_with_rule_id");
  // trace_receiver_helper's function will not be called for SYSTEM background
  // trace.
  TestTraceReceiverHelper trace_receiver_helper;
  std::unique_ptr<BackgroundTracingConfig> config = CreateSystemConfig();
  ASSERT_TRUE(config);
  ASSERT_TRUE(BackgroundTracingManager::GetInstance()->SetActiveScenario(
      std::move(config), trace_receiver_helper.get_receive_callback(),
      BackgroundTracingManager::NO_DATA_FILTERING));
  // Actually send the trigger into the system.
  base::RunLoop rule_triggered_runloop;
  BackgroundTracingManagerImpl::GetInstance()
      ->GetActiveScenarioForTesting()
      ->SetRuleTriggeredCallbackForTesting(
          rule_triggered_runloop.QuitClosure());
  TestTriggerHelper trigger_helper;
  BackgroundTracingManager::GetInstance()->TriggerNamedEvent(
      handle, trigger_helper.receive_closure(true));
  rule_triggered_runloop.Run();

  // ************ Wait and verify packets received & clean up ************
  system_consumer->WaitForAllDataSourcesStopped();
  system_consumer->ReadBuffers();
  system_no_more_packets_runloop.Run();
  // We should at the very least receive the system packets if the trigger was
  // properly received by the trace. However if the background trigger was not
  // received we won't see any packets and |received_packets()| will be 0.
  EXPECT_LT(0u, system_consumer->received_packets());
}
#endif  // defined(OS_ANDROID)

}  // namespace content
