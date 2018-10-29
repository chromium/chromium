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
#include "base/macros.h"
#include "base/memory/ref_counted_memory.h"
#include "base/metrics/histogram_macros.h"
#include "base/run_loop.h"
#include "base/strings/pattern.h"
#include "base/task/post_task.h"
#include "base/trace_event/trace_event.h"
#include "content/browser/tracing/background_startup_tracing_observer.h"
#include "content/browser/tracing/background_tracing_manager_impl.h"
#include "content/browser/tracing/background_tracing_rule.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "third_party/zlib/zlib.h"

using base::trace_event::TraceLog;

namespace content {
namespace {

class TestEnabledStateObserver : public TraceLog::EnabledStateObserver {
 public:
  TestEnabledStateObserver(base::Closure tracing_enabled_callback)
      : tracing_enabled_callback_(tracing_enabled_callback) {
    TraceLog::GetInstance()->AddEnabledStateObserver(this);
  }

  ~TestEnabledStateObserver() override {
    EXPECT_TRUE(TraceLog::GetInstance()->IsEnabled());
    TraceLog::GetInstance()->RemoveEnabledStateObserver(this);
  }

  void OnTraceLogEnabled() override { tracing_enabled_callback_.Run(); }

  void OnTraceLogDisabled() override { EXPECT_TRUE(false); }

 private:
  base::Closure tracing_enabled_callback_;
};

class TestBackgroundTracingObserver
    : public BackgroundTracingManagerImpl::EnabledStateObserver {
 public:
  explicit TestBackgroundTracingObserver(
      base::Closure tracing_enabled_callback);
  ~TestBackgroundTracingObserver() override;

  void OnScenarioActivated(const BackgroundTracingConfigImpl* config) override;
  void OnScenarioAborted() override;
  void OnTracingEnabled(
      BackgroundTracingConfigImpl::CategoryPreset preset) override;

 private:
  bool is_scenario_active_;
  base::Closure tracing_enabled_callback_;
};

TestBackgroundTracingObserver::TestBackgroundTracingObserver(
    base::Closure tracing_enabled_callback)
    : is_scenario_active_(false),
      tracing_enabled_callback_(tracing_enabled_callback) {
  BackgroundTracingManagerImpl::GetInstance()->AddEnabledStateObserver(this);
}

TestBackgroundTracingObserver::~TestBackgroundTracingObserver() {
  static_cast<BackgroundTracingManagerImpl*>(
      BackgroundTracingManager::GetInstance())
      ->RemoveEnabledStateObserver(this);
  EXPECT_TRUE(is_scenario_active_);
}

void TestBackgroundTracingObserver::OnScenarioActivated(
    const BackgroundTracingConfigImpl* config) {
  is_scenario_active_ = true;
}

void TestBackgroundTracingObserver::OnScenarioAborted() {
  is_scenario_active_ = false;
}

void TestBackgroundTracingObserver::OnTracingEnabled(
    BackgroundTracingConfigImpl::CategoryPreset preset) {
  tracing_enabled_callback_.Run();
}

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

}  // namespace

class BackgroundTracingManagerBrowserTest : public ContentBrowserTest {
 public:
  BackgroundTracingManagerBrowserTest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(BackgroundTracingManagerBrowserTest);
};

class BackgroundTracingManagerUploadConfigWrapper {
 public:
  BackgroundTracingManagerUploadConfigWrapper(base::OnceClosure callback)
      : callback_(std::move(callback)), receive_count_(0) {}

  void Upload(
      const scoped_refptr<base::RefCountedString>& file_contents,
      std::unique_ptr<const base::DictionaryValue> metadata,
      BackgroundTracingManager::FinishedProcessingCallback done_callback) {
    receive_count_ += 1;
    EXPECT_TRUE(file_contents);

    size_t compressed_length = file_contents->data().length();
    const size_t kOutputBufferLength = 10 * 1024 * 1024;
    std::vector<char> output_str(kOutputBufferLength);

    z_stream stream = {nullptr};
    stream.avail_in = compressed_length;
    stream.avail_out = kOutputBufferLength;
    stream.next_in = (Bytef*)&file_contents->data()[0];
    stream.next_out = (Bytef*)output_str.data();

    // 16 + MAX_WBITS means only decoding gzip encoded streams, and using
    // the biggest window size, according to zlib.h
    int result = inflateInit2(&stream, 16 + MAX_WBITS);
    EXPECT_EQ(Z_OK, result);
    result = inflate(&stream, Z_FINISH);
    int bytes_written = kOutputBufferLength - stream.avail_out;

    inflateEnd(&stream);
    EXPECT_EQ(Z_STREAM_END, result);

    last_file_contents_.assign(output_str.data(), bytes_written);
    base::PostTaskWithTraits(FROM_HERE, {BrowserThread::UI},
                             base::BindOnce(std::move(done_callback), true));
    CHECK(callback_);
    base::PostTaskWithTraits(FROM_HERE, {BrowserThread::UI},
                             std::move(callback_));
  }

  void SetUploadCallback(base::OnceClosure callback) {
    callback_ = std::move(callback);
  }

  bool TraceHasMatchingString(const char* str) {
    return last_file_contents_.find(str) != std::string::npos;
  }

  int get_receive_count() const { return receive_count_; }

  BackgroundTracingManager::ReceiveCallback get_receive_callback() {
    return base::BindRepeating(
        &BackgroundTracingManagerUploadConfigWrapper::Upload,
        base::Unretained(this));
  }

 private:
  base::OnceClosure callback_;
  int receive_count_;
  std::string last_file_contents_;
};

void StartedFinalizingCallback(base::Closure callback,
                               bool expected,
                               bool value) {
  EXPECT_EQ(expected, value);
  if (!callback.is_null())
    std::move(callback).Run();
}

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

void SetupBackgroundTracingManager() {
  content::BackgroundTracingManager::GetInstance()
      ->InvalidateTriggerHandlesForTesting();
}

void DisableScenarioWhenIdle() {
  BackgroundTracingManager::GetInstance()->SetActiveScenario(
      nullptr, BackgroundTracingManager::ReceiveCallback(),
      BackgroundTracingManager::NO_DATA_FILTERING);
}

#if defined(OS_ANDROID)
// Flaky on android: https://crbug.com/639706
#define MAYBE_ReceiveTraceFinalContentsOnTrigger \
        DISABLED_ReceiveTraceFinalContentsOnTrigger
#else
#define MAYBE_ReceiveTraceFinalContentsOnTrigger \
        ReceiveTraceFinalContentsOnTrigger
#endif

// This tests that the endpoint receives the final trace data.
IN_PROC_BROWSER_TEST_F(BackgroundTracingManagerBrowserTest,
                       MAYBE_ReceiveTraceFinalContentsOnTrigger) {
  {
    SetupBackgroundTracingManager();

    base::RunLoop run_loop;
    BackgroundTracingManagerUploadConfigWrapper upload_config_wrapper(
        run_loop.QuitClosure());

    std::unique_ptr<BackgroundTracingConfig> config = CreatePreemptiveConfig();

    BackgroundTracingManager::TriggerHandle handle =
        BackgroundTracingManager::
            GetInstance()->RegisterTriggerType("preemptive_test");

    EXPECT_TRUE(BackgroundTracingManager::GetInstance()->SetActiveScenario(
        std::move(config), upload_config_wrapper.get_receive_callback(),
        BackgroundTracingManager::NO_DATA_FILTERING));

    BackgroundTracingManager::GetInstance()->WhenIdle(
        base::Bind(&DisableScenarioWhenIdle));

    BackgroundTracingManager::GetInstance()->TriggerNamedEvent(
        handle, base::Bind(&StartedFinalizingCallback, base::Closure(), true));

    run_loop.Run();

    EXPECT_TRUE(upload_config_wrapper.get_receive_count() == 1);
  }
}

#if defined(OS_ANDROID)
// Flaky on android: https://crbug.com/639706
#define MAYBE_CallTriggersMoreThanOnceOnlyGatherOnce \
        DISABLED_CallTriggersMoreThanOnceOnlyGatherOnce
#else
#define MAYBE_CallTriggersMoreThanOnceOnlyGatherOnce \
        CallTriggersMoreThanOnceOnlyGatherOnce
#endif

// This tests triggering more than once still only gathers once.
IN_PROC_BROWSER_TEST_F(BackgroundTracingManagerBrowserTest,
                       MAYBE_CallTriggersMoreThanOnceOnlyGatherOnce) {
  {
    SetupBackgroundTracingManager();

    base::RunLoop run_loop;
    BackgroundTracingManagerUploadConfigWrapper upload_config_wrapper(
        run_loop.QuitClosure());

    std::unique_ptr<BackgroundTracingConfig> config = CreatePreemptiveConfig();

    content::BackgroundTracingManager::TriggerHandle handle =
        content::BackgroundTracingManager::GetInstance()->RegisterTriggerType(
            "preemptive_test");

    EXPECT_TRUE(BackgroundTracingManager::GetInstance()->SetActiveScenario(
        std::move(config), upload_config_wrapper.get_receive_callback(),
        BackgroundTracingManager::NO_DATA_FILTERING));

    BackgroundTracingManager::GetInstance()->WhenIdle(
        base::Bind(&DisableScenarioWhenIdle));

    BackgroundTracingManager::GetInstance()->TriggerNamedEvent(
        handle, base::Bind(&StartedFinalizingCallback, base::Closure(), true));
    BackgroundTracingManager::GetInstance()->TriggerNamedEvent(
        handle, base::Bind(&StartedFinalizingCallback, base::Closure(), false));

    run_loop.Run();

    EXPECT_TRUE(upload_config_wrapper.get_receive_count() == 1);
  }
}

namespace {

bool IsTraceEventArgsWhitelisted(
    const char* category_group_name,
    const char* event_name,
    base::trace_event::ArgumentNameFilterPredicate* arg_filter) {
  if (base::MatchPattern(category_group_name, "benchmark") &&
      base::MatchPattern(event_name, "whitelisted")) {
    return true;
  }

  return false;
}

}  // namespace

#if defined(OS_ANDROID) || defined(OS_CHROMEOS) || defined(OS_LINUX)
// Flaky on android, chromeos: https://crbug.com/639706
// Flaky on linux: https://crbug.com/795803
#define MAYBE_NoWhitelistedArgsStripped DISABLED_NoWhitelistedArgsStripped
#else
#define MAYBE_NoWhitelistedArgsStripped NoWhitelistedArgsStripped
#endif

// This tests that non-whitelisted args get stripped if required.
IN_PROC_BROWSER_TEST_F(BackgroundTracingManagerBrowserTest,
                       MAYBE_NoWhitelistedArgsStripped) {
  SetupBackgroundTracingManager();

  TraceLog::GetInstance()->SetArgumentFilterPredicate(
      base::Bind(&IsTraceEventArgsWhitelisted));

  base::RunLoop wait_for_upload;
  BackgroundTracingManagerUploadConfigWrapper upload_config_wrapper(
      wait_for_upload.QuitClosure());

  std::unique_ptr<BackgroundTracingConfig> config = CreatePreemptiveConfig();

  content::BackgroundTracingManager::TriggerHandle handle =
      content::BackgroundTracingManager::GetInstance()->RegisterTriggerType(
          "preemptive_test");

  base::RunLoop wait_for_activated;
  TestBackgroundTracingObserver observer(wait_for_activated.QuitClosure());
  EXPECT_TRUE(BackgroundTracingManager::GetInstance()->SetActiveScenario(
      std::move(config), upload_config_wrapper.get_receive_callback(),
      BackgroundTracingManager::ANONYMIZE_DATA));

  wait_for_activated.Run();

  if (!TraceLog::GetInstance()->IsEnabled()) {
    // Sometimes the chrome tracing agent of the current process is registered
    // after start tracing is already sent. In those cases, the scenario will be
    // started before tracing is enabled on the current process. We need to wait
    // for tracing to be enabled on the current process before we try to add
    // trace events.
    base::RunLoop wait_for_tracing;
    TestEnabledStateObserver tracing_observer(wait_for_tracing.QuitClosure());
    wait_for_tracing.Run();
  }

  TRACE_EVENT1("benchmark", "whitelisted", "find_this", 1);
  TRACE_EVENT1("benchmark", "not_whitelisted", "this_not_found", 1);

  BackgroundTracingManager::GetInstance()->WhenIdle(
      base::Bind(&DisableScenarioWhenIdle));

  BackgroundTracingManager::GetInstance()->TriggerNamedEvent(
      handle, base::Bind(&StartedFinalizingCallback, base::Closure(), true));

  wait_for_upload.Run();

  EXPECT_TRUE(upload_config_wrapper.get_receive_count() == 1);
  EXPECT_TRUE(upload_config_wrapper.TraceHasMatchingString("{"));
  EXPECT_TRUE(upload_config_wrapper.TraceHasMatchingString("find_this"));
  EXPECT_FALSE(upload_config_wrapper.TraceHasMatchingString("this_not_found"));
}

#if defined(OS_ANDROID) || defined(OS_CHROMEOS)
// Flaky on android, chromeos: https://crbug.com/639706
#define MAYBE_TraceMetadataInTrace DISABLED_TraceMetadataInTrace
#else
#define MAYBE_TraceMetadataInTrace TraceMetadataInTrace
#endif

// This tests that browser metadata gets included in the trace.
IN_PROC_BROWSER_TEST_F(BackgroundTracingManagerBrowserTest,
                       MAYBE_TraceMetadataInTrace) {
  SetupBackgroundTracingManager();

  TraceLog::GetInstance()->SetArgumentFilterPredicate(
      base::Bind(&IsTraceEventArgsWhitelisted));

  base::RunLoop wait_for_upload;
  BackgroundTracingManagerUploadConfigWrapper upload_config_wrapper(
      wait_for_upload.QuitClosure());

  std::unique_ptr<BackgroundTracingConfig> config = CreatePreemptiveConfig();

  content::BackgroundTracingManager::TriggerHandle handle =
      content::BackgroundTracingManager::GetInstance()->RegisterTriggerType(
          "preemptive_test");

  base::RunLoop wait_for_activated;
  TestBackgroundTracingObserver observer(wait_for_activated.QuitClosure());
  EXPECT_TRUE(BackgroundTracingManager::GetInstance()->SetActiveScenario(
      std::move(config), upload_config_wrapper.get_receive_callback(),
      BackgroundTracingManager::ANONYMIZE_DATA));

  wait_for_activated.Run();

  BackgroundTracingManager::GetInstance()->WhenIdle(
      base::Bind(&DisableScenarioWhenIdle));

  BackgroundTracingManager::GetInstance()->TriggerNamedEvent(
      handle, base::Bind(&StartedFinalizingCallback, base::Closure(), true));

  wait_for_upload.Run();

  EXPECT_TRUE(upload_config_wrapper.get_receive_count() == 1);
  EXPECT_TRUE(upload_config_wrapper.TraceHasMatchingString("cpu-brand"));
  EXPECT_TRUE(upload_config_wrapper.TraceHasMatchingString("network-type"));
  EXPECT_TRUE(upload_config_wrapper.TraceHasMatchingString("user-agent"));
}

// Flaky on android, linux, and windows: https://crbug.com/639706 and
// https://crbug.com/643415.
// This tests subprocesses (like a navigating renderer) which gets told to
// provide a argument-filtered trace and has no predicate in place to do the
// filtering (in this case, only the browser process gets it set), will crash
// rather than return potential PII.
IN_PROC_BROWSER_TEST_F(BackgroundTracingManagerBrowserTest,
                       DISABLED_CrashWhenSubprocessWithoutArgumentFilter) {
  SetupBackgroundTracingManager();

  TraceLog::GetInstance()->SetArgumentFilterPredicate(
      base::Bind(&IsTraceEventArgsWhitelisted));

  base::RunLoop wait_for_upload;
  BackgroundTracingManagerUploadConfigWrapper upload_config_wrapper(
      wait_for_upload.QuitClosure());

  std::unique_ptr<BackgroundTracingConfig> config = CreatePreemptiveConfig();

  content::BackgroundTracingManager::TriggerHandle handle =
      content::BackgroundTracingManager::GetInstance()->RegisterTriggerType(
          "preemptive_test");

  base::RunLoop wait_for_activated;
  TestBackgroundTracingObserver observer(wait_for_activated.QuitClosure());
  EXPECT_TRUE(BackgroundTracingManager::GetInstance()->SetActiveScenario(
      std::move(config), upload_config_wrapper.get_receive_callback(),
      BackgroundTracingManager::ANONYMIZE_DATA));

  wait_for_activated.Run();

  NavigateToURL(shell(), GetTestUrl("", "about:blank"));

  BackgroundTracingManager::GetInstance()->WhenIdle(
      base::Bind(&DisableScenarioWhenIdle));

  BackgroundTracingManager::GetInstance()->TriggerNamedEvent(
      handle, base::Bind(&StartedFinalizingCallback, base::Closure(), true));

  wait_for_upload.Run();

  EXPECT_TRUE(upload_config_wrapper.get_receive_count() == 1);
  // We should *not* receive anything at all from the renderer,
  // the process should've crashed rather than letting that happen.
  EXPECT_TRUE(!upload_config_wrapper.TraceHasMatchingString("CrRendererMain"));
}

#if defined(OS_ANDROID)
// Flaky on android: https://crbug.com/639706
#define MAYBE_CallMultipleTriggersOnlyGatherOnce \
        DISABLED_CallMultipleTriggersOnlyGatherOnce
#else
#define MAYBE_CallMultipleTriggersOnlyGatherOnce \
        CallMultipleTriggersOnlyGatherOnce
#endif

// This tests multiple triggers still only gathers once.
IN_PROC_BROWSER_TEST_F(BackgroundTracingManagerBrowserTest,
                       MAYBE_CallMultipleTriggersOnlyGatherOnce) {
  {
    SetupBackgroundTracingManager();

    base::RunLoop run_loop;
    BackgroundTracingManagerUploadConfigWrapper upload_config_wrapper(
        run_loop.QuitClosure());

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
        std::move(config), upload_config_wrapper.get_receive_callback(),
        BackgroundTracingManager::NO_DATA_FILTERING));

    BackgroundTracingManager::GetInstance()->WhenIdle(
        base::Bind(&DisableScenarioWhenIdle));

    BackgroundTracingManager::GetInstance()->TriggerNamedEvent(
        handle1, base::Bind(&StartedFinalizingCallback, base::Closure(), true));
    BackgroundTracingManager::GetInstance()->TriggerNamedEvent(
        handle2,
        base::Bind(&StartedFinalizingCallback, base::Closure(), false));

    run_loop.Run();

    EXPECT_TRUE(upload_config_wrapper.get_receive_count() == 1);
  }
}

#if defined(OS_ANDROID)
// Flaky on android: https://crbug.com/639706
#define MAYBE_ToggleBlinkScenarios DISABLED_ToggleBlinkScenarios
#else
#define MAYBE_ToggleBlinkScenarios ToggleBlinkScenarios
#endif

// This tests that toggling Blink scenarios in the config alters the
// command-line.
IN_PROC_BROWSER_TEST_F(BackgroundTracingManagerBrowserTest,
                       MAYBE_ToggleBlinkScenarios) {
  {
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    ASSERT_TRUE(command_line);

    // Early bailout in the case command line arguments have been explicitly set
    // for the runner.
    if (!command_line->GetSwitchValueASCII(switches::kEnableBlinkFeatures)
             .empty() ||
        !command_line->GetSwitchValueASCII(switches::kDisableBlinkFeatures)
             .empty()) {
      return;
    }

    SetupBackgroundTracingManager();

    base::RunLoop run_loop;
    TestBackgroundTracingObserver observer(run_loop.QuitClosure());
    BackgroundTracingManagerUploadConfigWrapper upload_config_wrapper(
        (base::Closure()));

    base::DictionaryValue dict;
    dict.SetString("mode", "PREEMPTIVE_TRACING_MODE");
    dict.SetString("category", "BENCHMARK");

    std::unique_ptr<base::ListValue> rules_list(new base::ListValue());
    {
      std::unique_ptr<base::DictionaryValue> rules_dict(
          new base::DictionaryValue());
      rules_dict->SetString("rule", "MONITOR_AND_DUMP_WHEN_TRIGGER_NAMED");
      rules_dict->SetString("trigger_name", "test2");
      rules_list->Append(std::move(rules_dict));
    }

    dict.Set("configs", std::move(rules_list));
    dict.SetString("enable_blink_features", "FasterWeb1,FasterWeb2");
    dict.SetString("disable_blink_features", "SlowerWeb1,SlowerWeb2");
    std::unique_ptr<BackgroundTracingConfig> config(
        BackgroundTracingConfigImpl::FromDict(&dict));
    EXPECT_TRUE(config);

    bool scenario_activated =
        BackgroundTracingManager::GetInstance()->SetActiveScenario(
            std::move(config), upload_config_wrapper.get_receive_callback(),
            BackgroundTracingManager::NO_DATA_FILTERING);

    EXPECT_TRUE(scenario_activated);
    EXPECT_EQ(command_line->GetSwitchValueASCII(switches::kEnableBlinkFeatures),
              "FasterWeb1,FasterWeb2");
    EXPECT_EQ(
        command_line->GetSwitchValueASCII(switches::kDisableBlinkFeatures),
        "SlowerWeb1,SlowerWeb2");
    run_loop.Run();
  }
}

#if defined(OS_ANDROID)
// Flaky on android: https://crbug.com/639706
#define MAYBE_ToggleBlinkScenariosNotOverridingSwitches \
        DISABLED_ToggleBlinkScenariosNotOverridingSwitches
#else
#define MAYBE_ToggleBlinkScenariosNotOverridingSwitches \
        ToggleBlinkScenariosNotOverridingSwitches
#endif

// This tests that toggling Blink scenarios in a scenario won't activate
// if there's already Blink features toggled by something else (about://flags)
IN_PROC_BROWSER_TEST_F(BackgroundTracingManagerBrowserTest,
                       MAYBE_ToggleBlinkScenariosNotOverridingSwitches) {
  SetupBackgroundTracingManager();

  base::RunLoop run_loop;
  BackgroundTracingManagerUploadConfigWrapper upload_config_wrapper(
      run_loop.QuitClosure());

  base::DictionaryValue dict;
  dict.SetString("mode", "PREEMPTIVE_TRACING_MODE");
  dict.SetString("category", "BENCHMARK");

  std::unique_ptr<base::ListValue> rules_list(new base::ListValue());
  {
    std::unique_ptr<base::DictionaryValue> rules_dict(
        new base::DictionaryValue());
    rules_dict->SetString("rule", "MONITOR_AND_DUMP_WHEN_TRIGGER_NAMED");
    rules_dict->SetString("trigger_name", "test2");
    rules_list->Append(std::move(rules_dict));
  }

  dict.Set("configs", std::move(rules_list));
  dict.SetString("enable_blink_features", "FasterWeb1,FasterWeb2");
  dict.SetString("disable_blink_features", "SlowerWeb1,SlowerWeb2");
  std::unique_ptr<BackgroundTracingConfig> config(
      BackgroundTracingConfigImpl::FromDict(&dict));
  EXPECT_TRUE(config);

  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kEnableBlinkFeatures, "FooFeature");

  bool scenario_activated =
      BackgroundTracingManager::GetInstance()->SetActiveScenario(
          std::move(config), upload_config_wrapper.get_receive_callback(),
          BackgroundTracingManager::NO_DATA_FILTERING);

  EXPECT_FALSE(scenario_activated);
}

#if defined(OS_ANDROID)
// Flaky on android: https://crbug.com/639706
#define MAYBE_CallPreemptiveTriggerWithDelay \
        DISABLED_CallPreemptiveTriggerWithDelay
#else
#define MAYBE_CallPreemptiveTriggerWithDelay CallPreemptiveTriggerWithDelay
#endif

// This tests that delayed histogram triggers triggers work as expected
// with preemptive scenarios.
IN_PROC_BROWSER_TEST_F(BackgroundTracingManagerBrowserTest,
                       MAYBE_CallPreemptiveTriggerWithDelay) {
  {
    SetupBackgroundTracingManager();

    base::RunLoop run_loop;
    BackgroundTracingManagerUploadConfigWrapper upload_config_wrapper(
        run_loop.QuitClosure());

    base::DictionaryValue dict;
    dict.SetString("mode", "PREEMPTIVE_TRACING_MODE");
    dict.SetString("category", "BENCHMARK");

    std::unique_ptr<base::ListValue> rules_list(new base::ListValue());
    {
      std::unique_ptr<base::DictionaryValue> rules_dict(
          new base::DictionaryValue());
      rules_dict->SetString(
          "rule", "MONITOR_AND_DUMP_WHEN_SPECIFIC_HISTOGRAM_AND_VALUE");
      rules_dict->SetString("histogram_name", "fake");
      rules_dict->SetInteger("histogram_value", 1);
      rules_dict->SetInteger("trigger_delay", 10);
      rules_list->Append(std::move(rules_dict));
    }

    dict.Set("configs", std::move(rules_list));

    std::unique_ptr<BackgroundTracingConfig> config(
        BackgroundTracingConfigImpl::FromDict(&dict));
    EXPECT_TRUE(config);

    EXPECT_TRUE(BackgroundTracingManager::GetInstance()->SetActiveScenario(
        std::move(config), upload_config_wrapper.get_receive_callback(),
        BackgroundTracingManager::NO_DATA_FILTERING));

    BackgroundTracingManager::GetInstance()->WhenIdle(
        base::Bind(&DisableScenarioWhenIdle));

    base::RunLoop rule_triggered_runloop;
    BackgroundTracingManagerImpl::GetInstance()
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
    BackgroundTracingManagerImpl::GetInstance()->FireTimerForTesting();
    EXPECT_FALSE(
        BackgroundTracingManagerImpl::GetInstance()->IsTracingForTesting());

    run_loop.Run();

    EXPECT_TRUE(upload_config_wrapper.get_receive_count() == 1);
  }
}

#if defined(OS_ANDROID)
// Flaky on android: https://crbug.com/639706
#define MAYBE_CannotTriggerWithoutScenarioSet \
        DISABLED_CannotTriggerWithoutScenarioSet
#else
#define MAYBE_CannotTriggerWithoutScenarioSet CannotTriggerWithoutScenarioSet
#endif

// This tests that you can't trigger without a scenario set.
IN_PROC_BROWSER_TEST_F(BackgroundTracingManagerBrowserTest,
                       MAYBE_CannotTriggerWithoutScenarioSet) {
  {
    SetupBackgroundTracingManager();

    base::RunLoop run_loop;
    BackgroundTracingManagerUploadConfigWrapper upload_config_wrapper(
        (base::Closure()));

    std::unique_ptr<BackgroundTracingConfig> config = CreatePreemptiveConfig();

    content::BackgroundTracingManager::TriggerHandle handle =
        content::BackgroundTracingManager::GetInstance()->RegisterTriggerType(
            "preemptive_test");

    BackgroundTracingManager::GetInstance()->TriggerNamedEvent(
        handle,
        base::Bind(&StartedFinalizingCallback, run_loop.QuitClosure(), false));

    run_loop.Run();

    EXPECT_TRUE(upload_config_wrapper.get_receive_count() == 0);
  }
}

#if defined(OS_ANDROID)
// Flaky on android: https://crbug.com/639706
#define MAYBE_DoesNotTriggerWithWrongHandle \
        DISABLED_DoesNotTriggerWithWrongHandle
#else
#define MAYBE_DoesNotTriggerWithWrongHandle DoesNotTriggerWithWrongHandle
#endif

// This tests that no trace is triggered with a handle that isn't specified
// in the config.
IN_PROC_BROWSER_TEST_F(BackgroundTracingManagerBrowserTest,
                       MAYBE_DoesNotTriggerWithWrongHandle) {
  {
    SetupBackgroundTracingManager();

    base::RunLoop run_loop;
    TestBackgroundTracingObserver observer(run_loop.QuitClosure());
    BackgroundTracingManagerUploadConfigWrapper upload_config_wrapper(
        (base::Closure()));

    std::unique_ptr<BackgroundTracingConfig> config = CreatePreemptiveConfig();

    content::BackgroundTracingManager::TriggerHandle handle =
        content::BackgroundTracingManager::GetInstance()->RegisterTriggerType(
            "does_not_exist");

    EXPECT_TRUE(BackgroundTracingManager::GetInstance()->SetActiveScenario(
        std::move(config), upload_config_wrapper.get_receive_callback(),
        BackgroundTracingManager::NO_DATA_FILTERING));

    BackgroundTracingManager::GetInstance()->WhenIdle(
        base::Bind(&DisableScenarioWhenIdle));

    BackgroundTracingManager::GetInstance()->TriggerNamedEvent(
        handle, base::Bind(&StartedFinalizingCallback, base::Closure(), false));

    run_loop.Run();

    EXPECT_TRUE(upload_config_wrapper.get_receive_count() == 0);
  }
}

#if defined(OS_ANDROID)
// Flaky on android: https://crbug.com/639706
#define MAYBE_DoesNotTriggerWithInvalidHandle \
        DISABLED_DoesNotTriggerWithInvalidHandle
#else
#define MAYBE_DoesNotTriggerWithInvalidHandle DoesNotTriggerWithInvalidHandle
#endif

// This tests that no trace is triggered with an invalid handle.
IN_PROC_BROWSER_TEST_F(BackgroundTracingManagerBrowserTest,
                       MAYBE_DoesNotTriggerWithInvalidHandle) {
  {
    SetupBackgroundTracingManager();

    base::RunLoop run_loop;
    TestBackgroundTracingObserver observer(run_loop.QuitClosure());
    BackgroundTracingManagerUploadConfigWrapper upload_config_wrapper(
        (base::Closure()));

    std::unique_ptr<BackgroundTracingConfig> config = CreatePreemptiveConfig();

    content::BackgroundTracingManager::TriggerHandle handle =
        content::BackgroundTracingManager::GetInstance()->RegisterTriggerType(
            "preemptive_test");

    content::BackgroundTracingManager::GetInstance()
        ->InvalidateTriggerHandlesForTesting();

    EXPECT_TRUE(BackgroundTracingManager::GetInstance()->SetActiveScenario(
        std::move(config), upload_config_wrapper.get_receive_callback(),
        BackgroundTracingManager::NO_DATA_FILTERING));

    BackgroundTracingManager::GetInstance()->WhenIdle(
        base::Bind(&DisableScenarioWhenIdle));

    BackgroundTracingManager::GetInstance()->TriggerNamedEvent(
        handle, base::Bind(&StartedFinalizingCallback, base::Closure(), false));

    run_loop.Run();

    EXPECT_TRUE(upload_config_wrapper.get_receive_count() == 0);
  }
}

#if defined(OS_ANDROID)
// Flaky on android: https://crbug.com/639706
#define MAYBE_PreemptiveNotTriggerWithZeroChance \
        DISABLED_PreemptiveNotTriggerWithZeroChance
#else
#define MAYBE_PreemptiveNotTriggerWithZeroChance \
        PreemptiveNotTriggerWithZeroChance
#endif

// This tests that no preemptive trace is triggered with 0 chance set.
IN_PROC_BROWSER_TEST_F(BackgroundTracingManagerBrowserTest,
                       MAYBE_PreemptiveNotTriggerWithZeroChance) {
  {
    SetupBackgroundTracingManager();

    base::RunLoop run_loop;
    TestBackgroundTracingObserver observer(run_loop.QuitWhenIdleClosure());
    BackgroundTracingManagerUploadConfigWrapper upload_config_wrapper(
        (base::Closure()));

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
        std::move(config), upload_config_wrapper.get_receive_callback(),
        BackgroundTracingManager::NO_DATA_FILTERING));

    BackgroundTracingManager::GetInstance()->WhenIdle(
        base::Bind(&DisableScenarioWhenIdle));

    BackgroundTracingManager::GetInstance()->TriggerNamedEvent(
        handle, base::Bind(&StartedFinalizingCallback, base::Closure(), false));

    run_loop.Run();

    EXPECT_TRUE(upload_config_wrapper.get_receive_count() == 0);
  }
}

#if defined(OS_ANDROID)
// Flaky on android: https://crbug.com/639706
#define MAYBE_ReactiveNotTriggerWithZeroChance \
        DISABLED_ReactiveNotTriggerWithZeroChance
#else
#define MAYBE_ReactiveNotTriggerWithZeroChance ReactiveNotTriggerWithZeroChance
#endif

// This tests that no reactive trace is triggered with 0 chance set.
IN_PROC_BROWSER_TEST_F(BackgroundTracingManagerBrowserTest,
                       MAYBE_ReactiveNotTriggerWithZeroChance) {
  {
    SetupBackgroundTracingManager();

    base::RunLoop run_loop;
    BackgroundTracingManagerUploadConfigWrapper upload_config_wrapper(
        (base::Closure()));

    base::DictionaryValue dict;

    dict.SetString("mode", "REACTIVE_TRACING_MODE");

    std::unique_ptr<base::ListValue> rules_list(new base::ListValue());
    {
      std::unique_ptr<base::DictionaryValue> rules_dict(
          new base::DictionaryValue());
      rules_dict->SetString("rule",
                            "TRACE_ON_NAVIGATION_UNTIL_TRIGGER_OR_FULL");
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
        std::move(config), upload_config_wrapper.get_receive_callback(),
        BackgroundTracingManager::NO_DATA_FILTERING));

    BackgroundTracingManager::GetInstance()->WhenIdle(
        base::Bind(&DisableScenarioWhenIdle));

    BackgroundTracingManager::GetInstance()->TriggerNamedEvent(
        handle,
        base::Bind(&StartedFinalizingCallback, run_loop.QuitClosure(), false));

    run_loop.Run();

    EXPECT_TRUE(upload_config_wrapper.get_receive_count() == 0);
  }
}

#if defined(OS_ANDROID)
// Flaky on android: https://crbug.com/639706
#define MAYBE_ReceiveTraceSucceedsOnHigherHistogramSample \
        DISABLED_ReceiveTraceSucceedsOnHigherHistogramSample
#else
#define MAYBE_ReceiveTraceSucceedsOnHigherHistogramSample \
        ReceiveTraceSucceedsOnHigherHistogramSample
#endif

// This tests that histogram triggers for preemptive mode configs.
IN_PROC_BROWSER_TEST_F(BackgroundTracingManagerBrowserTest,
                       MAYBE_ReceiveTraceSucceedsOnHigherHistogramSample) {
  {
    SetupBackgroundTracingManager();

    base::RunLoop run_loop;

    BackgroundTracingManagerUploadConfigWrapper upload_config_wrapper(
        run_loop.QuitClosure());

    base::DictionaryValue dict;
    dict.SetString("mode", "PREEMPTIVE_TRACING_MODE");
    dict.SetString("category", "BENCHMARK");

    std::unique_ptr<base::ListValue> rules_list(new base::ListValue());
    {
      std::unique_ptr<base::DictionaryValue> rules_dict(
          new base::DictionaryValue());
      rules_dict->SetString(
          "rule", "MONITOR_AND_DUMP_WHEN_SPECIFIC_HISTOGRAM_AND_VALUE");
      rules_dict->SetString("histogram_name", "fake");
      rules_dict->SetInteger("histogram_value", 1);
      rules_list->Append(std::move(rules_dict));
    }

    dict.Set("configs", std::move(rules_list));

    std::unique_ptr<BackgroundTracingConfig> config(
        BackgroundTracingConfigImpl::FromDict(&dict));
    EXPECT_TRUE(config);

    EXPECT_TRUE(BackgroundTracingManager::GetInstance()->SetActiveScenario(
        std::move(config), upload_config_wrapper.get_receive_callback(),
        BackgroundTracingManager::NO_DATA_FILTERING));

    BackgroundTracingManager::GetInstance()->WhenIdle(
        base::Bind(&DisableScenarioWhenIdle));

    // Our reference value is "1", so a value of "2" should trigger a trace.
    LOCAL_HISTOGRAM_COUNTS("fake", 2);

    run_loop.Run();

    EXPECT_TRUE(upload_config_wrapper.get_receive_count() == 1);
  }
}

#if defined(OS_ANDROID)
// Flaky on android: https://crbug.com/639706
#define MAYBE_ReceiveReactiveTraceSucceedsOnHigherHistogramSample \
        DISABLED_ReceiveReactiveTraceSucceedsOnHigherHistogramSample
#else
#define MAYBE_ReceiveReactiveTraceSucceedsOnHigherHistogramSample \
        ReceiveReactiveTraceSucceedsOnHigherHistogramSample
#endif

// This tests that histogram triggers for reactive mode configs.
IN_PROC_BROWSER_TEST_F(
    BackgroundTracingManagerBrowserTest,
    MAYBE_ReceiveReactiveTraceSucceedsOnHigherHistogramSample) {
  {
    SetupBackgroundTracingManager();

    base::RunLoop run_loop;

    BackgroundTracingManagerUploadConfigWrapper upload_config_wrapper(
        run_loop.QuitClosure());

    base::DictionaryValue dict;
    dict.SetString("mode", "REACTIVE_TRACING_MODE");

    std::unique_ptr<base::ListValue> rules_list(new base::ListValue());
    {
      std::unique_ptr<base::DictionaryValue> rules_dict(
          new base::DictionaryValue());
      rules_dict->SetString(
          "rule", "MONITOR_AND_DUMP_WHEN_SPECIFIC_HISTOGRAM_AND_VALUE");
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
        std::move(config), upload_config_wrapper.get_receive_callback(),
        BackgroundTracingManager::NO_DATA_FILTERING));

    BackgroundTracingManager::GetInstance()->WhenIdle(
        base::Bind(&DisableScenarioWhenIdle));

    // Our reference value is "1", so a value of "2" should trigger a trace.
    LOCAL_HISTOGRAM_COUNTS("fake", 2);

    run_loop.Run();

    EXPECT_TRUE(upload_config_wrapper.get_receive_count() == 1);
  }
}

#if defined(OS_ANDROID)
// Flaky on android: https://crbug.com/639706
#define MAYBE_ReceiveTraceFailsOnLowerHistogramSample \
        DISABLED_ReceiveTraceFailsOnLowerHistogramSample
#else
#define MAYBE_ReceiveTraceFailsOnLowerHistogramSample \
        ReceiveTraceFailsOnLowerHistogramSample
#endif

// This tests that histogram values < reference value don't trigger.
IN_PROC_BROWSER_TEST_F(BackgroundTracingManagerBrowserTest,
                       MAYBE_ReceiveTraceFailsOnLowerHistogramSample) {
  {
    SetupBackgroundTracingManager();

    base::RunLoop run_loop;
    TestBackgroundTracingObserver observer(run_loop.QuitWhenIdleClosure());
    BackgroundTracingManagerUploadConfigWrapper upload_config_wrapper(
        (base::Closure()));

    base::DictionaryValue dict;
    dict.SetString("mode", "PREEMPTIVE_TRACING_MODE");
    dict.SetString("category", "BENCHMARK");

    std::unique_ptr<base::ListValue> rules_list(new base::ListValue());
    {
      std::unique_ptr<base::DictionaryValue> rules_dict(
          new base::DictionaryValue());
      rules_dict->SetString(
          "rule", "MONITOR_AND_DUMP_WHEN_SPECIFIC_HISTOGRAM_AND_VALUE");
      rules_dict->SetString("histogram_name", "fake");
      rules_dict->SetInteger("histogram_value", 1);
      rules_list->Append(std::move(rules_dict));
    }

    dict.Set("configs", std::move(rules_list));

    std::unique_ptr<BackgroundTracingConfig> config(
        BackgroundTracingConfigImpl::FromDict(&dict));
    EXPECT_TRUE(config);

    EXPECT_TRUE(BackgroundTracingManager::GetInstance()->SetActiveScenario(
        std::move(config), upload_config_wrapper.get_receive_callback(),
        BackgroundTracingManager::NO_DATA_FILTERING));

    BackgroundTracingManager::GetInstance()->WhenIdle(
        base::Bind(&DisableScenarioWhenIdle));

    // This should fail to trigger a trace since the sample value < the
    // the reference value above.
    LOCAL_HISTOGRAM_COUNTS("fake", 0);

    run_loop.Run();

    EXPECT_TRUE(upload_config_wrapper.get_receive_count() == 0);
  }
}

#if defined(OS_ANDROID)
// Flaky on android: https://crbug.com/639706
#define MAYBE_ReceiveTraceFailsOnHigherHistogramSample \
        DISABLED_ReceiveTraceFailsOnHigherHistogramSample
#else
#define MAYBE_ReceiveTraceFailsOnHigherHistogramSample \
        ReceiveTraceFailsOnHigherHistogramSample
#endif

// This tests that histogram values > upper reference value don't trigger.
IN_PROC_BROWSER_TEST_F(BackgroundTracingManagerBrowserTest,
                       MAYBE_ReceiveTraceFailsOnHigherHistogramSample) {
  {
    SetupBackgroundTracingManager();

    base::RunLoop run_loop;
    TestBackgroundTracingObserver observer(run_loop.QuitWhenIdleClosure());
    BackgroundTracingManagerUploadConfigWrapper upload_config_wrapper(
        (base::Closure()));

    base::DictionaryValue dict;
    dict.SetString("mode", "PREEMPTIVE_TRACING_MODE");
    dict.SetString("category", "BENCHMARK");

    std::unique_ptr<base::ListValue> rules_list(new base::ListValue());
    {
      std::unique_ptr<base::DictionaryValue> rules_dict(
          new base::DictionaryValue());
      rules_dict->SetString(
          "rule", "MONITOR_AND_DUMP_WHEN_SPECIFIC_HISTOGRAM_AND_VALUE");
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
        std::move(config), upload_config_wrapper.get_receive_callback(),
        BackgroundTracingManager::NO_DATA_FILTERING));

    BackgroundTracingManager::GetInstance()->WhenIdle(
        base::Bind(&DisableScenarioWhenIdle));

    // This should fail to trigger a trace since the sample value > the
    // the upper reference value above.
    LOCAL_HISTOGRAM_COUNTS("fake", 0);

    run_loop.Run();

    EXPECT_TRUE(upload_config_wrapper.get_receive_count() == 0);
  }
}

#if defined(OS_ANDROID)
// Flaky on android: https://crbug.com/639706
#define MAYBE_SetActiveScenarioFailsWithInvalidPreemptiveConfig \
        DISABLED_SetActiveScenarioFailsWithInvalidPreemptiveConfig
#else
#define MAYBE_SetActiveScenarioFailsWithInvalidPreemptiveConfig \
        SetActiveScenarioFailsWithInvalidPreemptiveConfig
#endif

// This tests that invalid preemptive mode configs will fail.
IN_PROC_BROWSER_TEST_F(
    BackgroundTracingManagerBrowserTest,
    MAYBE_SetActiveScenarioFailsWithInvalidPreemptiveConfig) {
  {
    SetupBackgroundTracingManager();

    BackgroundTracingManagerUploadConfigWrapper upload_config_wrapper(
        (base::Closure()));

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
}

#if defined(OS_ANDROID)
// Flaky on android: https://crbug.com/639706
#define MAYBE_ReactiveTimeoutTermination DISABLED_ReactiveTimeoutTermination
#else
#define MAYBE_ReactiveTimeoutTermination ReactiveTimeoutTermination
#endif

// This tests that reactive mode records and terminates with timeout.
IN_PROC_BROWSER_TEST_F(BackgroundTracingManagerBrowserTest,
                       MAYBE_ReactiveTimeoutTermination) {
  {
    SetupBackgroundTracingManager();

    base::RunLoop run_loop;
    BackgroundTracingManagerUploadConfigWrapper upload_config_wrapper(
        run_loop.QuitClosure());

    std::unique_ptr<BackgroundTracingConfig> config = CreateReactiveConfig();

    BackgroundTracingManager::TriggerHandle handle =
        BackgroundTracingManager::
            GetInstance()->RegisterTriggerType("reactive_test");

    EXPECT_TRUE(BackgroundTracingManager::GetInstance()->SetActiveScenario(
        std::move(config), upload_config_wrapper.get_receive_callback(),
        BackgroundTracingManager::NO_DATA_FILTERING));

    BackgroundTracingManager::GetInstance()->WhenIdle(
        base::Bind(&DisableScenarioWhenIdle));

    BackgroundTracingManager::GetInstance()->TriggerNamedEvent(
        handle, base::Bind(&StartedFinalizingCallback, base::Closure(), true));

    BackgroundTracingManager::GetInstance()->FireTimerForTesting();

    run_loop.Run();

    EXPECT_TRUE(upload_config_wrapper.get_receive_count() == 1);
  }
}

#if defined(OS_ANDROID)
// Flaky on android: https://crbug.com/639706
#define MAYBE_ReactiveSecondTriggerTermination \
        DISABLED_ReactiveSecondTriggerTermination
#else
#define MAYBE_ReactiveSecondTriggerTermination ReactiveSecondTriggerTermination
#endif

// This tests that reactive mode records and terminates with a second trigger.
IN_PROC_BROWSER_TEST_F(BackgroundTracingManagerBrowserTest,
                       MAYBE_ReactiveSecondTriggerTermination) {
  {
    SetupBackgroundTracingManager();

    base::RunLoop run_loop;
    BackgroundTracingManagerUploadConfigWrapper upload_config_wrapper(
        run_loop.QuitClosure());

    std::unique_ptr<BackgroundTracingConfig> config = CreateReactiveConfig();

    BackgroundTracingManager::TriggerHandle handle =
        BackgroundTracingManager::
            GetInstance()->RegisterTriggerType("reactive_test");

    EXPECT_TRUE(BackgroundTracingManager::GetInstance()->SetActiveScenario(
        std::move(config), upload_config_wrapper.get_receive_callback(),
        BackgroundTracingManager::NO_DATA_FILTERING));

    BackgroundTracingManager::GetInstance()->WhenIdle(
        base::Bind(&DisableScenarioWhenIdle));

    BackgroundTracingManager::GetInstance()->TriggerNamedEvent(
        handle, base::Bind(&StartedFinalizingCallback, base::Closure(), true));
    // second trigger to terminate.
    BackgroundTracingManager::GetInstance()->TriggerNamedEvent(
        handle, base::Bind(&StartedFinalizingCallback, base::Closure(), true));

    run_loop.Run();

    EXPECT_TRUE(upload_config_wrapper.get_receive_count() == 1);
  }
}

#if defined(OS_ANDROID)
// Flaky on android: https://crbug.com/639706
#define MAYBE_ReactiveSecondUpload DISABLED_ReactiveSecondUpload
#else
#define MAYBE_ReactiveSecondUpload ReactiveSecondUpload
#endif

// This tests that reactive mode uploads on a second set of triggers.
IN_PROC_BROWSER_TEST_F(BackgroundTracingManagerBrowserTest,
                       MAYBE_ReactiveSecondUpload) {
  {
    SetupBackgroundTracingManager();

    base::RunLoop run_loop;
    BackgroundTracingManagerUploadConfigWrapper upload_config_wrapper(
        run_loop.QuitClosure());

    std::unique_ptr<BackgroundTracingConfig> config = CreateReactiveConfig();

    BackgroundTracingManager::TriggerHandle handle =
        BackgroundTracingManager::GetInstance()->RegisterTriggerType(
            "reactive_test");

    EXPECT_TRUE(BackgroundTracingManager::GetInstance()->SetActiveScenario(
        std::move(config), upload_config_wrapper.get_receive_callback(),
        BackgroundTracingManager::NO_DATA_FILTERING));

    BackgroundTracingManager::GetInstance()->TriggerNamedEvent(
        handle, base::Bind(&StartedFinalizingCallback, base::Closure(), true));
    // second trigger to terminate.
    BackgroundTracingManager::GetInstance()->TriggerNamedEvent(
        handle, base::Bind(&StartedFinalizingCallback, base::Closure(), true));

    run_loop.Run();

    base::RunLoop second_upload_run_loop;
    upload_config_wrapper.SetUploadCallback(
        second_upload_run_loop.QuitClosure());

    BackgroundTracingManager::GetInstance()->TriggerNamedEvent(
        handle, base::Bind(&StartedFinalizingCallback, base::Closure(), true));
    // second trigger to terminate.
    BackgroundTracingManager::GetInstance()->TriggerNamedEvent(
        handle, base::Bind(&StartedFinalizingCallback, base::Closure(), true));

    second_upload_run_loop.Run();

    EXPECT_TRUE(upload_config_wrapper.get_receive_count() == 2);
  }
}

#if defined(OS_ANDROID)
// Flaky on android: https://crbug.com/639706
#define MAYBE_ReactiveSecondTriggerMustMatchForTermination \
        DISABLED_ReactiveSecondTriggerMustMatchForTermination
#else
#define MAYBE_ReactiveSecondTriggerMustMatchForTermination \
        ReactiveSecondTriggerMustMatchForTermination
#endif

// This tests that reactive mode only terminates with the same trigger.
IN_PROC_BROWSER_TEST_F(BackgroundTracingManagerBrowserTest,
                       MAYBE_ReactiveSecondTriggerMustMatchForTermination) {
  {
    SetupBackgroundTracingManager();

    base::RunLoop run_loop;
    BackgroundTracingManagerUploadConfigWrapper upload_config_wrapper(
        run_loop.QuitClosure());

    base::DictionaryValue dict;
    dict.SetString("mode", "REACTIVE_TRACING_MODE");

    std::unique_ptr<base::ListValue> rules_list(new base::ListValue());
    {
      std::unique_ptr<base::DictionaryValue> rules_dict(
          new base::DictionaryValue());
      rules_dict->SetString("rule",
                            "TRACE_ON_NAVIGATION_UNTIL_TRIGGER_OR_FULL");
      rules_dict->SetString("trigger_name", "reactive_test1");
      rules_dict->SetBoolean("stop_tracing_on_repeated_reactive", true);
      rules_dict->SetInteger("trigger_delay", 10);
      rules_dict->SetString("category", "BENCHMARK");
      rules_list->Append(std::move(rules_dict));
    }
    {
      std::unique_ptr<base::DictionaryValue> rules_dict(
          new base::DictionaryValue());
      rules_dict->SetString("rule",
                            "TRACE_ON_NAVIGATION_UNTIL_TRIGGER_OR_FULL");
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
        std::move(config), upload_config_wrapper.get_receive_callback(),
        BackgroundTracingManager::NO_DATA_FILTERING));

    BackgroundTracingManager::GetInstance()->WhenIdle(
        base::Bind(&DisableScenarioWhenIdle));

    BackgroundTracingManager::GetInstance()->TriggerNamedEvent(
        handle1, base::Bind(&StartedFinalizingCallback, base::Closure(), true));

    // This is expected to fail since we triggered with handle1.
    BackgroundTracingManager::GetInstance()->TriggerNamedEvent(
        handle2,
        base::Bind(&StartedFinalizingCallback, base::Closure(), false));

    // second trigger to terminate.
    BackgroundTracingManager::GetInstance()->TriggerNamedEvent(
        handle1, base::Bind(&StartedFinalizingCallback, base::Closure(), true));

    run_loop.Run();

    EXPECT_TRUE(upload_config_wrapper.get_receive_count() == 1);
  }
}

#if defined(OS_ANDROID)
// Flaky on android: https://crbug.com/639706
#define MAYBE_ReactiveThirdTriggerTimeout DISABLED_ReactiveThirdTriggerTimeout
#else
#define MAYBE_ReactiveThirdTriggerTimeout ReactiveThirdTriggerTimeout
#endif

// This tests a third trigger in reactive more does not start another trace.
IN_PROC_BROWSER_TEST_F(BackgroundTracingManagerBrowserTest,
                       MAYBE_ReactiveThirdTriggerTimeout) {
  {
    SetupBackgroundTracingManager();

    base::RunLoop run_loop;
    BackgroundTracingManagerUploadConfigWrapper upload_config_wrapper(
        run_loop.QuitClosure());

    std::unique_ptr<BackgroundTracingConfig> config = CreateReactiveConfig();

    BackgroundTracingManager::TriggerHandle handle =
        BackgroundTracingManager::
            GetInstance()->RegisterTriggerType("reactive_test");

    EXPECT_TRUE(BackgroundTracingManager::GetInstance()->SetActiveScenario(
        std::move(config), upload_config_wrapper.get_receive_callback(),
        BackgroundTracingManager::NO_DATA_FILTERING));

    BackgroundTracingManager::GetInstance()->WhenIdle(
        base::Bind(&DisableScenarioWhenIdle));

    BackgroundTracingManager::GetInstance()->TriggerNamedEvent(
        handle, base::Bind(&StartedFinalizingCallback, base::Closure(), true));
    // second trigger to terminate.
    BackgroundTracingManager::GetInstance()->TriggerNamedEvent(
        handle, base::Bind(&StartedFinalizingCallback, base::Closure(), true));
    // third trigger to trigger again, fails as it is still gathering.
    BackgroundTracingManager::GetInstance()->TriggerNamedEvent(
        handle, base::Bind(&StartedFinalizingCallback, base::Closure(), false));

    run_loop.Run();

    EXPECT_TRUE(upload_config_wrapper.get_receive_count() == 1);
  }
}

// This tests that reactive mode only terminates with a repeated trigger
// if the config specifies that it should.
IN_PROC_BROWSER_TEST_F(BackgroundTracingManagerBrowserTest,
                       ReactiveSecondTriggerIgnored) {
  {
    SetupBackgroundTracingManager();

    base::RunLoop run_loop;
    BackgroundTracingManagerUploadConfigWrapper upload_config_wrapper(
        run_loop.QuitClosure());

    base::DictionaryValue dict;
    dict.SetString("mode", "REACTIVE_TRACING_MODE");

    std::unique_ptr<base::ListValue> rules_list(new base::ListValue());
    {
      std::unique_ptr<base::DictionaryValue> rules_dict(
          new base::DictionaryValue());
      rules_dict->SetString("rule",
                            "TRACE_ON_NAVIGATION_UNTIL_TRIGGER_OR_FULL");
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

    base::RunLoop wait_for_tracing_enabled;
    TestBackgroundTracingObserver observer(
        wait_for_tracing_enabled.QuitClosure());
    EXPECT_TRUE(BackgroundTracingManager::GetInstance()->SetActiveScenario(
        std::move(config), upload_config_wrapper.get_receive_callback(),
        BackgroundTracingManager::NO_DATA_FILTERING));

    BackgroundTracingManager::GetInstance()->WhenIdle(
        base::Bind(&DisableScenarioWhenIdle));

    BackgroundTracingManager::GetInstance()->TriggerNamedEvent(
        trigger_handle,
        base::Bind(&StartedFinalizingCallback, base::Closure(), true));

    wait_for_tracing_enabled.Run();

    // This is expected to fail since we already triggered.
    BackgroundTracingManager::GetInstance()->TriggerNamedEvent(
        trigger_handle,
        base::Bind(&StartedFinalizingCallback, base::Closure(), false));

    // Since we specified a delay in the scenario, we should still be tracing
    // at this point.
    EXPECT_TRUE(
        BackgroundTracingManagerImpl::GetInstance()->IsTracingForTesting());

    BackgroundTracingManager::GetInstance()->FireTimerForTesting();

    EXPECT_FALSE(
        BackgroundTracingManagerImpl::GetInstance()->IsTracingForTesting());

    run_loop.Run();

    EXPECT_TRUE(upload_config_wrapper.get_receive_count() == 1);
  }
}

IN_PROC_BROWSER_TEST_F(BackgroundTracingManagerBrowserTest,
                       SetupStartupTracing) {
  SetupBackgroundTracingManager();
  std::unique_ptr<TestStartupPreferenceManagerImpl> preferences_moved(
      new TestStartupPreferenceManagerImpl);
  TestStartupPreferenceManagerImpl* preferences = preferences_moved.get();
  BackgroundStartupTracingObserver::GetInstance()
      ->SetPreferenceManagerForTesting(std::move(preferences_moved));
  preferences->SetBackgroundStartupTracingEnabled(false);
  BackgroundTracingManagerUploadConfigWrapper upload_config_wrapper(
      (base::OnceClosure()));

  base::DictionaryValue dict;
  std::unique_ptr<base::ListValue> rules_list(new base::ListValue());
  {
    std::unique_ptr<base::DictionaryValue> rules_dict(
        new base::DictionaryValue());
    rules_dict->SetString("rule", "MONITOR_AND_DUMP_WHEN_TRIGGER_NAMED");
    rules_dict->SetString("trigger_name", "startup-config");
    rules_dict->SetBoolean("stop_tracing_on_repeated_reactive", false);
    rules_dict->SetInteger("trigger_delay", 10);
    rules_dict->SetString("category", "BENCHMARK_STARTUP");
    rules_list->Append(std::move(rules_dict));
  }
  dict.Set("configs", std::move(rules_list));

  std::unique_ptr<BackgroundTracingConfig> config(
      BackgroundTracingConfigImpl::ReactiveFromDict(&dict));

  base::RunLoop wait_for_tracing_enabled;
  TestBackgroundTracingObserver observer(
      wait_for_tracing_enabled.QuitClosure());
  EXPECT_TRUE(BackgroundTracingManager::GetInstance()->SetActiveScenario(
      std::move(config), upload_config_wrapper.get_receive_callback(),
      BackgroundTracingManager::NO_DATA_FILTERING));

  BackgroundTracingManager::GetInstance()->WhenIdle(
      base::BindRepeating(&DisableScenarioWhenIdle));

  base::RunLoop().RunUntilIdle();

  // Since we specified a delay in the scenario, we should still be tracing
  // at this point.
  EXPECT_FALSE(
      BackgroundTracingManagerImpl::GetInstance()->IsTracingForTesting());
  EXPECT_TRUE(preferences->GetBackgroundStartupTracingEnabled());
}

IN_PROC_BROWSER_TEST_F(BackgroundTracingManagerBrowserTest, RunStartupTracing) {
  SetupBackgroundTracingManager();
  std::unique_ptr<TestStartupPreferenceManagerImpl> preferences_moved(
      new TestStartupPreferenceManagerImpl);
  TestStartupPreferenceManagerImpl* preferences = preferences_moved.get();
  BackgroundStartupTracingObserver::GetInstance()
      ->SetPreferenceManagerForTesting(std::move(preferences_moved));
  preferences->SetBackgroundStartupTracingEnabled(true);
  TraceLog::GetInstance()->SetArgumentFilterPredicate(
      base::BindRepeating(&IsTraceEventArgsWhitelisted));
  base::RunLoop wait_for_trace_log_enabled;
  std::unique_ptr<TestEnabledStateObserver> trace_log_observer(
      new TestEnabledStateObserver(wait_for_trace_log_enabled.QuitClosure()));

  base::RunLoop run_loop;
  BackgroundTracingManagerUploadConfigWrapper upload_config_wrapper(
      run_loop.QuitClosure());

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

  base::RunLoop wait_for_tracing_enabled;
  TestBackgroundTracingObserver observer(
      wait_for_tracing_enabled.QuitClosure());
  EXPECT_TRUE(BackgroundTracingManager::GetInstance()->SetActiveScenario(
      std::move(config), upload_config_wrapper.get_receive_callback(),
      BackgroundTracingManager::NO_DATA_FILTERING));

  BackgroundTracingManager::GetInstance()->WhenIdle(
      base::BindRepeating(&DisableScenarioWhenIdle));

  wait_for_tracing_enabled.Run();
  wait_for_trace_log_enabled.Run();
  trace_log_observer.reset();

  EXPECT_TRUE(BackgroundTracingManagerImpl::GetInstance()
                  ->requires_anonymized_data_for_testing());
  EXPECT_TRUE(base::trace_event::TraceLog::GetInstance()
                  ->GetCurrentTraceConfig()
                  .IsArgumentFilterEnabled());

  // Since we specified a delay in the scenario, we should still be tracing
  // at this point.
  EXPECT_TRUE(
      BackgroundTracingManagerImpl::GetInstance()->IsTracingForTesting());

  BackgroundTracingManager::GetInstance()->FireTimerForTesting();

  EXPECT_FALSE(
      BackgroundTracingManagerImpl::GetInstance()->IsTracingForTesting());

  run_loop.Run();

  EXPECT_TRUE(upload_config_wrapper.get_receive_count() == 1);
  EXPECT_FALSE(preferences->GetBackgroundStartupTracingEnabled());
}

}  // namespace content
