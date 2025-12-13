// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/tracing/tracing_scenario.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/notimplemented.h"
#include "base/path_service.h"
#include "base/task/thread_pool.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/test_proto_loader.h"
#include "base/token.h"
#include "base/trace_event/named_trigger.h"
#include "build/build_config.h"
#include "content/public/browser/background_tracing_manager.h"
#include "content/public/browser/tracing_delegate.h"
#include "content/public/test/browser_task_environment.h"
#include "services/tracing/perfetto/test_utils.h"
#include "services/tracing/public/cpp/perfetto/perfetto_traced_process.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_POSIX)
#include "base/files/scoped_temp_dir.h"
#include "base/task/current_thread.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "services/tracing/perfetto/system_test_utils.h"
#include "services/tracing/public/cpp/tracing_features.h"
#include "services/tracing/tracing_service.h"
#endif  // BUILDFLAG(IS_POSIX)

namespace content {
namespace {

const char* kDefaultNestedConfig = R"pb(
  scenario_name: "test_nested_scenario"
  start_rules: { manual_trigger_name: "start_trigger" }
  stop_rules: { manual_trigger_name: "stop_trigger" }
  upload_rules: { manual_trigger_name: "upload_trigger" }
)pb";

const char* kDefaultConfig = R"pb(
  scenario_name: "test_scenario"
  setup_rules: { manual_trigger_name: "setup_trigger" }
  start_rules: { manual_trigger_name: "start_trigger" }
  stop_rules: { manual_trigger_name: "stop_trigger" }
  upload_rules: { manual_trigger_name: "upload_trigger" }
  trace_config: {
    data_sources: { config: { name: "org.chromium.trace_metadata2" } }
  }
  nested_scenarios: {
    scenario_name: "nested_scenario"
    start_rules: { manual_trigger_name: "nested_start_trigger" }
    stop_rules: { manual_trigger_name: "nested_stop_trigger" }
    upload_rules: { manual_trigger_name: "nested_upload_trigger" }
  }
  nested_scenarios: {
    scenario_name: "other_nested_scenario"
    start_rules: { manual_trigger_name: "other_nested_start_trigger" }
    stop_rules: { manual_trigger_name: "other_nested_stop_trigger" }
    upload_rules: { manual_trigger_name: "other_nested_upload_trigger" }
  }
)pb";

using testing::_;

class TestTracingScenarioDelegate : public TracingScenario::Delegate {
 public:
  ~TestTracingScenarioDelegate() = default;

  MOCK_METHOD(bool, OnScenarioActive, (TracingScenario * scenario), (override));
  MOCK_METHOD(bool, OnScenarioIdle, (TracingScenario * scenario), (override));
  MOCK_METHOD(bool, OnScenarioCloned, (TracingScenario * scenario), (override));
  MOCK_METHOD(void,
              OnScenarioError,
              (TracingScenario * scenario, perfetto::TracingError),
              (override));
  MOCK_METHOD(void,
              OnScenarioRecording,
              (TracingScenario * scenario),
              (override));
  MOCK_METHOD(void,
              SaveTrace,
              (TracingScenario * scenario,
               base::Token trace_uuid,
               const BackgroundTracingRule* triggered_rule,
               std::string&& trace_data),
              (override));
};

class TestNestedTracingScenarioDelegate
    : public NestedTracingScenario::Delegate {
 public:
  ~TestNestedTracingScenarioDelegate() = default;

  MOCK_METHOD(void,
              OnNestedScenarioStart,
              (NestedTracingScenario * scenario),
              (override));
  MOCK_METHOD(void,
              OnNestedScenarioStop,
              (NestedTracingScenario * scenario),
              (override));
  MOCK_METHOD(void,
              OnNestedScenarioUpload,
              (NestedTracingScenario * scenario,
               const BackgroundTracingRule* triggered_rule),
              (override));
};

// Fake perfetto::TracingSession.
class TestTracingSession : public perfetto::TracingSession {
 public:
  static constexpr base::Token kClonedSessionId = base::Token(0xAB, 0xCD);

  TestTracingSession() = default;
  ~TestTracingSession() override = default;

  void Setup(const perfetto::TraceConfig& config, int fd = -1) override {
    if (!config.data_sources().empty()) {
      start_should_fail_ =
          config.data_sources()[0].config().name() == "Invalid";
      should_spuriously_stop =
          config.data_sources()[0].config().name() == "Stop";
    }
  }

  void Start() override {
    if (start_should_fail_) {
      base::ThreadPool::PostTask(
          FROM_HERE,
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
          base::BindOnce(
              [](std::function<void(perfetto::TracingError)>  // nocheck
                     on_error_callback) {
                on_error_callback(
                    {perfetto::TracingError::kTracingFailed, "error"});
              },
              on_error_callback_));
      return;
    }
    if (should_spuriously_stop) {
      Stop();
      return;
    }
    // perfetto::TracingSession runs callbacks from its own background thread.
    base::ThreadPool::PostTask(
        FROM_HERE,
        {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
         base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
        base::BindOnce(
            [](std::function<void()> on_start_callback) {  // nocheck
              on_start_callback();
            },
            on_start_callback_));
  }

  void StartBlocking() override { NOTIMPLEMENTED(); }

  void SetOnStartCallback(std::function<void()> on_start) override {  // nocheck
    on_start_callback_ = on_start;
  }

  void SetOnErrorCallback(
      std::function<void(perfetto::TracingError)> on_error)  // nocheck
      override {
    on_error_callback_ = on_error;
  }

  void Flush(std::function<void(bool)>, uint32_t timeout_ms = 0)  // nocheck
      override {
    NOTIMPLEMENTED();
  }

  void Stop() override {
    // perfetto::TracingSession runs callbacks from its own background thread.
    base::ThreadPool::PostTask(
        FROM_HERE,
        {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
         base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
        base::BindOnce(
            [](std::function<void()> on_stop_callback) {  // nocheck
              on_stop_callback();
            },
            on_stop_callback_));
  }

  void CloneTrace(CloneTraceArgs args,
                  CloneTraceCallback on_session_cloned) override {
    // perfetto::TracingSession runs callbacks from its own background thread.
    base::ThreadPool::PostTask(
        FROM_HERE,
        {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
         base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
        base::BindOnce(
            [](CloneTraceCallback on_session_cloned) {  // nocheck
              on_session_cloned(
                  {true, "", kClonedSessionId.low(), kClonedSessionId.high()});
            },
            on_session_cloned));
  }

  void StopBlocking() override { NOTIMPLEMENTED(); }

  void SetOnStopCallback(std::function<void()> on_stop) override {  // nocheck
    on_stop_callback_ = on_stop;
  }

  void ChangeTraceConfig(const perfetto::TraceConfig&) override {
    NOTIMPLEMENTED();
  }
  void ReadTrace(ReadTraceCallback read_callback) override {
    base::ThreadPool::PostTask(
        FROM_HERE,
        {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
         base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
        base::BindOnce(
            [](ReadTraceCallback read_callback) {  // nocheck
              std::string trace_content = "this is a trace";
              read_callback({trace_content.data(), trace_content.size(),
                             /*has_more=*/false});
            },
            read_callback));
  }
  void GetTraceStats(GetTraceStatsCallback) override { NOTIMPLEMENTED(); }
  void QueryServiceState(QueryServiceStateCallback) override {
    NOTIMPLEMENTED();
  }

 private:
  std::function<void()> on_start_callback_;                        // nocheck
  std::function<void()> on_stop_callback_;                         // nocheck
  std::function<void(perfetto::TracingError)> on_error_callback_;  // nocheck
  bool start_should_fail_ = false;
  bool should_spuriously_stop = false;
};

class TracingScenarioForTesting : public TracingScenario {
 public:
  TracingScenarioForTesting(const perfetto::protos::gen::ScenarioConfig& config,
                            TestTracingScenarioDelegate* delegate)
      : TracingScenario(config,
                        delegate,
                        /*enable_privacy_filter=*/false,
                        /*is_local_scenario=*/true,
                        /*request_startup_tracing=*/false) {
    EXPECT_TRUE(Initialize(config, false));
  }

 protected:
  std::unique_ptr<perfetto::TracingSession> CreateTracingSession() override {
    return std::make_unique<TestTracingSession>();
  }
};

class NestedTracingScenarioForTesting : public NestedTracingScenario {
 public:
  NestedTracingScenarioForTesting(
      const perfetto::protos::gen::NestedScenarioConfig& config,
      NestedTracingScenario::Delegate* delegate)
      : NestedTracingScenario(config, delegate) {
    EXPECT_TRUE(Initialize(config));
  }
};

perfetto::protos::gen::ScenarioConfig ParseScenarioConfigFromText(
    const std::string& proto_text) {
  base::TestProtoLoader config_loader(
      base::PathService::CheckedGet(base::DIR_GEN_TEST_DATA_ROOT)
          .Append(
              FILE_PATH_LITERAL("third_party/perfetto/protos/perfetto/"
                                "config/chrome/scenario_config.descriptor")),
      "perfetto.protos.ScenarioConfig");
  std::string serialized_message;
  config_loader.ParseFromText(proto_text, serialized_message);
  perfetto::protos::gen::ScenarioConfig destination;
  destination.ParseFromString(serialized_message);
  return destination;
}

perfetto::protos::gen::NestedScenarioConfig ParseNestedScenarioConfigFromText(
    const std::string& proto_text) {
  base::TestProtoLoader config_loader(
      base::PathService::CheckedGet(base::DIR_GEN_TEST_DATA_ROOT)
          .Append(
              FILE_PATH_LITERAL("third_party/perfetto/protos/perfetto/"
                                "config/chrome/scenario_config.descriptor")),
      "perfetto.protos.NestedScenarioConfig");
  std::string serialized_message;
  config_loader.ParseFromText(proto_text, serialized_message);
  perfetto::protos::gen::NestedScenarioConfig destination;
  destination.ParseFromString(serialized_message);
  return destination;
}

class TracingScenarioTest : public testing::Test {
 public:
  TracingScenarioTest()
      : background_tracing_manager_(
            content::BackgroundTracingManager::CreateInstance(
                &tracing_delegate_)) {}

 protected:
  BrowserTaskEnvironment task_environment;
  tracing::TracedProcessForTesting traced_process{
      base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()})};
  TestTracingScenarioDelegate delegate;
  TestNestedTracingScenarioDelegate nested_delegate;
  content::TracingDelegate tracing_delegate_;
  std::unique_ptr<content::BackgroundTracingManager>
      background_tracing_manager_;
};

class NestedTracingScenarioTest : public testing::Test {
 public:
  NestedTracingScenarioTest()
      : background_tracing_manager_(
            content::BackgroundTracingManager::CreateInstance(
                &tracing_delegate_)) {}

 protected:
  BrowserTaskEnvironment task_environment;
  TestNestedTracingScenarioDelegate delegate;
  content::TracingDelegate tracing_delegate_;
  std::unique_ptr<content::BackgroundTracingManager>
      background_tracing_manager_;
};

}  // namespace

TEST_F(TracingScenarioTest, Init) {
  TracingScenarioForTesting tracing_scenario(
      ParseScenarioConfigFromText(R"pb(
        scenario_name: "test_scenario"
        trace_config: {
          data_sources: { config: { name: "org.chromium.trace_metadata2" } }
        }
      )pb"),
      &delegate);
  EXPECT_EQ("test_scenario", tracing_scenario.scenario_name());
  EXPECT_EQ(TracingScenario::State::kDisabled,
            tracing_scenario.current_state());
}

TEST_F(TracingScenarioTest, Disabled) {
  TracingScenarioForTesting tracing_scenario(
      ParseScenarioConfigFromText(kDefaultConfig), &delegate);

  EXPECT_CALL(delegate, OnScenarioActive(&tracing_scenario)).Times(0);

  EXPECT_FALSE(base::trace_event::EmitNamedTrigger("setup_trigger"));
  EXPECT_FALSE(base::trace_event::EmitNamedTrigger("start_trigger"));
  EXPECT_FALSE(base::trace_event::EmitNamedTrigger("nested_start_trigger"));

  tracing_scenario.Enable();
  EXPECT_EQ(TracingScenario::State::kEnabled, tracing_scenario.current_state());
  tracing_scenario.Disable();
  EXPECT_EQ(TracingScenario::State::kDisabled,
            tracing_scenario.current_state());

  EXPECT_FALSE(base::trace_event::EmitNamedTrigger("setup_trigger"));
  EXPECT_FALSE(base::trace_event::EmitNamedTrigger("start_trigger"));
  EXPECT_FALSE(base::trace_event::EmitNamedTrigger("nested_start_trigger"));
}

TEST_F(TracingScenarioTest, StartStop) {
  TracingScenarioForTesting tracing_scenario(
      ParseScenarioConfigFromText(kDefaultConfig), &delegate);

  tracing_scenario.Enable();
  EXPECT_EQ(TracingScenario::State::kEnabled, tracing_scenario.current_state());
  EXPECT_CALL(delegate, OnScenarioActive(&tracing_scenario))
      .WillOnce(testing::Return(true));
  EXPECT_TRUE(base::trace_event::EmitNamedTrigger("start_trigger"));

  base::RunLoop run_loop;
  EXPECT_CALL(delegate, OnScenarioIdle(&tracing_scenario))
      .WillOnce([&run_loop]() {
        run_loop.Quit();
        return true;
      });

  EXPECT_TRUE(base::trace_event::EmitNamedTrigger("stop_trigger"));
  run_loop.Run();
  EXPECT_EQ(TracingScenario::State::kDisabled,
            tracing_scenario.current_state());
}

TEST_F(TracingScenarioTest, NestedStartStop) {
  TracingScenarioForTesting tracing_scenario(
      ParseScenarioConfigFromText(kDefaultConfig), &delegate);

  tracing_scenario.Enable();
  EXPECT_EQ(TracingScenario::State::kEnabled, tracing_scenario.current_state());
  EXPECT_CALL(delegate, OnScenarioActive(&tracing_scenario))
      .WillOnce(testing::Return(true));
  EXPECT_TRUE(base::trace_event::EmitNamedTrigger("start_trigger"));
  EXPECT_TRUE(base::trace_event::EmitNamedTrigger("nested_start_trigger"));

  EXPECT_TRUE(base::trace_event::EmitNamedTrigger("nested_stop_trigger"));

  base::RunLoop run_loop;
  EXPECT_CALL(delegate, OnScenarioIdle(&tracing_scenario))
      .WillOnce([&run_loop]() {
        run_loop.Quit();
        return true;
      });

  EXPECT_TRUE(base::trace_event::EmitNamedTrigger("stop_trigger"));
  run_loop.Run();
  EXPECT_EQ(TracingScenario::State::kDisabled,
            tracing_scenario.current_state());
}

TEST_F(TracingScenarioTest, UnnestedStop) {
  TracingScenarioForTesting tracing_scenario(
      ParseScenarioConfigFromText(kDefaultConfig), &delegate);

  tracing_scenario.Enable();
  EXPECT_EQ(TracingScenario::State::kEnabled, tracing_scenario.current_state());
  EXPECT_CALL(delegate, OnScenarioActive(&tracing_scenario))
      .WillOnce(testing::Return(true));
  EXPECT_TRUE(base::trace_event::EmitNamedTrigger("start_trigger"));
  EXPECT_TRUE(base::trace_event::EmitNamedTrigger("nested_start_trigger"));

  base::RunLoop run_loop;
  EXPECT_CALL(delegate, OnScenarioIdle(&tracing_scenario))
      .WillOnce([&run_loop]() {
        run_loop.Quit();
        return true;
      });

  EXPECT_TRUE(base::trace_event::EmitNamedTrigger("stop_trigger"));
  run_loop.Run();
  EXPECT_EQ(TracingScenario::State::kDisabled,
            tracing_scenario.current_state());
}

TEST_F(TracingScenarioTest, UnnestedStopUpload) {
  TracingScenarioForTesting tracing_scenario(
      ParseScenarioConfigFromText(kDefaultConfig), &delegate);

  tracing_scenario.Enable();
  EXPECT_EQ(TracingScenario::State::kEnabled, tracing_scenario.current_state());
  EXPECT_CALL(delegate, OnScenarioActive(&tracing_scenario))
      .WillOnce(testing::Return(true));
  EXPECT_TRUE(base::trace_event::EmitNamedTrigger("start_trigger"));
  EXPECT_TRUE(base::trace_event::EmitNamedTrigger("nested_start_trigger"));

  base::RunLoop run_loop;
  EXPECT_CALL(delegate, OnScenarioIdle(&tracing_scenario))
      .WillOnce([&run_loop]() {
        run_loop.Quit();
        return true;
      });

  EXPECT_TRUE(base::trace_event::EmitNamedTrigger("stop_trigger"));
  EXPECT_FALSE(base::trace_event::EmitNamedTrigger("nested_upload_trigger"));
  run_loop.Run();
  EXPECT_EQ(TracingScenario::State::kDisabled,
            tracing_scenario.current_state());
}

TEST_F(TracingScenarioTest, StartFail) {
  TracingScenarioForTesting tracing_scenario(
      ParseScenarioConfigFromText(R"pb(
        scenario_name: "test_scenario"
        start_rules: {
          name: "start_trigger"
          manual_trigger_name: "start_trigger"
        }
        stop_rules: { name: "stop_trigger" manual_trigger_name: "stop_trigger" }
        trace_config: { data_sources: { config: { name: "Invalid" } } }
      )pb"),
      &delegate);

  tracing_scenario.Enable();
  EXPECT_EQ(TracingScenario::State::kEnabled, tracing_scenario.current_state());
  EXPECT_CALL(delegate, OnScenarioActive(&tracing_scenario))
      .WillOnce(testing::Return(true));
  EXPECT_TRUE(base::trace_event::EmitNamedTrigger("start_trigger"));

  base::RunLoop run_loop;
  EXPECT_CALL(delegate, OnScenarioIdle(&tracing_scenario))
      .WillOnce([&run_loop]() {
        run_loop.Quit();
        return true;
      });

  run_loop.Run();
  EXPECT_EQ(TracingScenario::State::kDisabled,
            tracing_scenario.current_state());
  EXPECT_FALSE(base::trace_event::EmitNamedTrigger("stop_trigger"));
}

TEST_F(TracingScenarioTest, SpuriousStop) {
  TracingScenarioForTesting tracing_scenario(
      ParseScenarioConfigFromText(R"pb(
        scenario_name: "test_scenario"
        start_rules: {
          name: "start_trigger"
          manual_trigger_name: "start_trigger"
        }
        stop_rules: { name: "stop_trigger" manual_trigger_name: "stop_trigger" }
        trace_config: { data_sources: { config: { name: "Stop" } } }
      )pb"),
      &delegate);

  tracing_scenario.Enable();
  EXPECT_EQ(TracingScenario::State::kEnabled, tracing_scenario.current_state());
  EXPECT_CALL(delegate, OnScenarioActive(&tracing_scenario))
      .WillOnce(testing::Return(true));
  EXPECT_TRUE(base::trace_event::EmitNamedTrigger("start_trigger"));

  base::RunLoop run_loop;
  EXPECT_CALL(delegate, OnScenarioIdle(&tracing_scenario))
      .WillOnce([&run_loop]() {
        run_loop.Quit();
        return true;
      });

  run_loop.Run();
  EXPECT_EQ(TracingScenario::State::kDisabled,
            tracing_scenario.current_state());
  EXPECT_FALSE(base::trace_event::EmitNamedTrigger("stop_trigger"));
}

TEST_F(TracingScenarioTest, SetupStop) {
  TracingScenarioForTesting tracing_scenario(
      ParseScenarioConfigFromText(kDefaultConfig), &delegate);

  tracing_scenario.Enable();
  EXPECT_CALL(delegate, OnScenarioActive(&tracing_scenario))
      .WillOnce(testing::Return(true));
  EXPECT_TRUE(base::trace_event::EmitNamedTrigger("setup_trigger"));
  EXPECT_EQ(TracingScenario::State::kSetup, tracing_scenario.current_state());

  base::RunLoop run_loop;
  EXPECT_CALL(delegate, OnScenarioIdle(&tracing_scenario))
      .WillOnce([&run_loop]() {
        run_loop.Quit();
        return true;
      });

  EXPECT_TRUE(base::trace_event::EmitNamedTrigger("stop_trigger"));
  run_loop.Run();
  EXPECT_EQ(TracingScenario::State::kDisabled,
            tracing_scenario.current_state());
}

TEST_F(TracingScenarioTest, SetupUpload) {
  TracingScenarioForTesting tracing_scenario(
      ParseScenarioConfigFromText(kDefaultConfig), &delegate);

  tracing_scenario.Enable();
  EXPECT_CALL(delegate, OnScenarioActive(&tracing_scenario))
      .WillOnce(testing::Return(true));
  EXPECT_TRUE(base::trace_event::EmitNamedTrigger("setup_trigger"));
  EXPECT_EQ(TracingScenario::State::kSetup, tracing_scenario.current_state());

  base::Token trace_uuid = tracing_scenario.GetSessionID();
  base::RunLoop run_loop;
  EXPECT_CALL(delegate, SaveTrace(_, trace_uuid, _, _)).Times(0);
  EXPECT_CALL(delegate, OnScenarioIdle(&tracing_scenario))
      .WillOnce([&run_loop]() {
        run_loop.Quit();
        return true;
      });

  EXPECT_TRUE(base::trace_event::EmitNamedTrigger("upload_trigger"));
  run_loop.Run();
  EXPECT_EQ(TracingScenario::State::kDisabled,
            tracing_scenario.current_state());
}

TEST_F(TracingScenarioTest, SetupStartStop) {
  TracingScenarioForTesting tracing_scenario(
      ParseScenarioConfigFromText(kDefaultConfig), &delegate);

  tracing_scenario.Enable();
  EXPECT_CALL(delegate, OnScenarioActive(&tracing_scenario))
      .WillOnce(testing::Return(true));
  EXPECT_TRUE(base::trace_event::EmitNamedTrigger("setup_trigger"));
  EXPECT_EQ(TracingScenario::State::kSetup, tracing_scenario.current_state());

  EXPECT_CALL(delegate, OnScenarioActive(&tracing_scenario)).Times(0);
  EXPECT_TRUE(base::trace_event::EmitNamedTrigger("start_trigger"));
  EXPECT_EQ(TracingScenario::State::kStarting,
            tracing_scenario.current_state());

  base::RunLoop run_loop;
  EXPECT_CALL(delegate, OnScenarioIdle(&tracing_scenario))
      .WillOnce([&run_loop]() {
        run_loop.Quit();
        return true;
      });

  EXPECT_TRUE(base::trace_event::EmitNamedTrigger("stop_trigger"));
  run_loop.Run();
  EXPECT_EQ(TracingScenario::State::kDisabled,
            tracing_scenario.current_state());
}

TEST_F(TracingScenarioTest, SetupNestedStartStop) {
  TracingScenarioForTesting tracing_scenario(
      ParseScenarioConfigFromText(kDefaultConfig), &delegate);

  tracing_scenario.Enable();
  EXPECT_CALL(delegate, OnScenarioActive(&tracing_scenario))
      .WillOnce(testing::Return(true));
  EXPECT_TRUE(base::trace_event::EmitNamedTrigger("setup_trigger"));
  EXPECT_EQ(TracingScenario::State::kSetup, tracing_scenario.current_state());

  EXPECT_CALL(delegate, OnScenarioActive(&tracing_scenario)).Times(0);
  EXPECT_TRUE(base::trace_event::EmitNamedTrigger("nested_start_trigger"));
  EXPECT_EQ(TracingScenario::State::kStarting,
            tracing_scenario.current_state());
  EXPECT_FALSE(
      base::trace_event::EmitNamedTrigger("other_nested_start_trigger"));

  EXPECT_TRUE(base::trace_event::EmitNamedTrigger("nested_stop_trigger"));

  base::RunLoop run_loop;
  EXPECT_CALL(delegate, OnScenarioIdle(&tracing_scenario))
      .WillOnce([&run_loop]() {
        run_loop.Quit();
        return true;
      });
  EXPECT_TRUE(base::trace_event::EmitNamedTrigger("stop_trigger"));
  run_loop.Run();
  EXPECT_EQ(TracingScenario::State::kDisabled,
            tracing_scenario.current_state());
}

TEST_F(TracingScenarioTest, Abort) {
  TracingScenarioForTesting tracing_scenario(
      ParseScenarioConfigFromText(kDefaultConfig), &delegate);

  tracing_scenario.Enable();
  EXPECT_CALL(delegate, OnScenarioActive(&tracing_scenario))
      .WillOnce(testing::Return(true));
  EXPECT_TRUE(base::trace_event::EmitNamedTrigger("start_trigger"));
  EXPECT_EQ(TracingScenario::State::kStarting,
            tracing_scenario.current_state());

  base::RunLoop run_loop;
  EXPECT_CALL(delegate, OnScenarioIdle(&tracing_scenario))
      .WillOnce([&run_loop]() {
        run_loop.Quit();
        return true;
      });

  tracing_scenario.Abort();
  run_loop.Run();
  EXPECT_EQ(TracingScenario::State::kDisabled,
            tracing_scenario.current_state());
  EXPECT_FALSE(base::trace_event::EmitNamedTrigger("stop_trigger"));
}

TEST_F(TracingScenarioTest, Upload) {
  TracingScenarioForTesting tracing_scenario(
      ParseScenarioConfigFromText(kDefaultConfig), &delegate);

  tracing_scenario.Enable();
  EXPECT_CALL(delegate, OnScenarioActive(&tracing_scenario))
      .WillOnce(testing::Return(true));
  {
    base::RunLoop run_loop;
    EXPECT_CALL(delegate, OnScenarioRecording(&tracing_scenario))
        .WillOnce(base::test::RunOnceClosure(run_loop.QuitClosure()));
    EXPECT_TRUE(base::trace_event::EmitNamedTrigger("start_trigger"));
    EXPECT_EQ(TracingScenario::State::kStarting,
              tracing_scenario.current_state());
    run_loop.Run();
    EXPECT_EQ(TracingScenario::State::kRecording,
              tracing_scenario.current_state());
  }

  base::Token trace_uuid = tracing_scenario.GetSessionID();
  base::RunLoop run_loop;
  EXPECT_CALL(delegate, SaveTrace(&tracing_scenario, trace_uuid, _,
                                  std::string("this is a trace")))
      .WillOnce(base::test::RunOnceClosure(run_loop.QuitClosure()));
  EXPECT_CALL(delegate, OnScenarioIdle(&tracing_scenario))
      .WillOnce(testing::Return(true));

  EXPECT_TRUE(base::trace_event::EmitNamedTrigger("upload_trigger"));

  run_loop.Run();
  EXPECT_EQ(TracingScenario::State::kDisabled,
            tracing_scenario.current_state());
}

TEST_F(TracingScenarioTest, StopUpload) {
  TracingScenarioForTesting tracing_scenario(
      ParseScenarioConfigFromText(kDefaultConfig), &delegate);

  tracing_scenario.Enable();
  EXPECT_CALL(delegate, OnScenarioActive(&tracing_scenario))
      .WillOnce(testing::Return(true));
  {
    base::RunLoop run_loop;
    EXPECT_CALL(delegate, OnScenarioRecording(&tracing_scenario))
        .WillOnce(base::test::RunOnceClosure(run_loop.QuitClosure()));
    EXPECT_TRUE(base::trace_event::EmitNamedTrigger("start_trigger"));
    run_loop.Run();
  }

  base::Token trace_uuid = tracing_scenario.GetSessionID();
  base::RunLoop run_loop;
  EXPECT_CALL(delegate, SaveTrace(&tracing_scenario, trace_uuid, _,
                                  std::string("this is a trace")))
      .WillOnce(base::test::RunOnceClosure(run_loop.QuitClosure()));
  EXPECT_CALL(delegate, OnScenarioIdle(&tracing_scenario))
      .WillOnce(testing::Return(true));

  EXPECT_TRUE(base::trace_event::EmitNamedTrigger("stop_trigger"));
  EXPECT_TRUE(base::trace_event::EmitNamedTrigger("upload_trigger"));

  run_loop.Run();
  EXPECT_EQ(TracingScenario::State::kDisabled,
            tracing_scenario.current_state());
}

TEST_F(TracingScenarioTest, NestedUpload) {
  TracingScenarioForTesting tracing_scenario(
      ParseScenarioConfigFromText(kDefaultConfig), &delegate);

  tracing_scenario.Enable();
  EXPECT_CALL(delegate, OnScenarioActive(&tracing_scenario))
      .WillOnce(testing::Return(true));
  {
    base::RunLoop run_loop;
    EXPECT_CALL(delegate, OnScenarioRecording(&tracing_scenario))
        .WillOnce(base::test::RunOnceClosure(run_loop.QuitClosure()));
    EXPECT_TRUE(base::trace_event::EmitNamedTrigger("start_trigger"));
    EXPECT_TRUE(base::trace_event::EmitNamedTrigger("nested_start_trigger"));
    run_loop.Run();
  }
  EXPECT_EQ(TracingScenario::State::kRecording,
            tracing_scenario.current_state());

  {
    base::RunLoop run_loop;
    EXPECT_CALL(delegate, OnScenarioCloned(&tracing_scenario))
        .WillOnce(testing::Return(true));
    EXPECT_CALL(delegate, SaveTrace(&tracing_scenario,
                                    TestTracingSession::kClonedSessionId, _,
                                    std::string("this is a trace")))
        .WillOnce(base::test::RunOnceClosure(run_loop.QuitClosure()));
    EXPECT_TRUE(base::trace_event::EmitNamedTrigger("nested_upload_trigger"));
    run_loop.Run();
  }

  {
    base::RunLoop run_loop;
    EXPECT_EQ(TracingScenario::State::kRecording,
              tracing_scenario.current_state());

    EXPECT_CALL(delegate, OnScenarioIdle(&tracing_scenario))
        .WillOnce([&run_loop]() {
          run_loop.Quit();
          return true;
        });
    EXPECT_TRUE(base::trace_event::EmitNamedTrigger("stop_trigger"));
    run_loop.Run();
  }
}

TEST_F(TracingScenarioTest, NestedStopUpload) {
  TracingScenarioForTesting tracing_scenario(
      ParseScenarioConfigFromText(kDefaultConfig), &delegate);

  tracing_scenario.Enable();
  EXPECT_CALL(delegate, OnScenarioActive(&tracing_scenario))
      .WillOnce(testing::Return(true));
  {
    base::RunLoop run_loop;
    EXPECT_CALL(delegate, OnScenarioRecording(&tracing_scenario))
        .WillOnce(base::test::RunOnceClosure(run_loop.QuitClosure()));
    EXPECT_TRUE(base::trace_event::EmitNamedTrigger("start_trigger"));
    EXPECT_TRUE(base::trace_event::EmitNamedTrigger("nested_start_trigger"));
    run_loop.Run();
  }
  EXPECT_EQ(TracingScenario::State::kRecording,
            tracing_scenario.current_state());

  {
    EXPECT_TRUE(base::trace_event::EmitNamedTrigger("nested_stop_trigger"));
    base::RunLoop run_loop;
    EXPECT_CALL(delegate, OnScenarioCloned(&tracing_scenario))
        .WillOnce(testing::Return(true));
    EXPECT_CALL(delegate, SaveTrace(&tracing_scenario,
                                    TestTracingSession::kClonedSessionId, _,
                                    std::string("this is a trace")))
        .WillOnce(base::test::RunOnceClosure(run_loop.QuitClosure()));
    EXPECT_TRUE(base::trace_event::EmitNamedTrigger("nested_upload_trigger"));
    run_loop.Run();
  }

  {
    base::RunLoop run_loop;
    EXPECT_EQ(TracingScenario::State::kRecording,
              tracing_scenario.current_state());

    EXPECT_CALL(delegate, OnScenarioIdle(&tracing_scenario))
        .WillOnce([&run_loop]() {
          run_loop.Quit();
          return true;
        });
    EXPECT_TRUE(base::trace_event::EmitNamedTrigger("stop_trigger"));
    run_loop.Run();
  }
}

TEST_F(NestedTracingScenarioTest, Disabled) {
  NestedTracingScenarioForTesting tracing_scenario(
      ParseNestedScenarioConfigFromText(kDefaultNestedConfig), &delegate);

  EXPECT_CALL(delegate, OnNestedScenarioStart(&tracing_scenario)).Times(0);

  EXPECT_FALSE(base::trace_event::EmitNamedTrigger("start_trigger"));

  tracing_scenario.Enable();
  EXPECT_EQ(NestedTracingScenario::State::kEnabled,
            tracing_scenario.current_state());
  tracing_scenario.Disable();
  EXPECT_EQ(NestedTracingScenario::State::kDisabled,
            tracing_scenario.current_state());

  EXPECT_FALSE(base::trace_event::EmitNamedTrigger("start_trigger"));
}

TEST_F(NestedTracingScenarioTest, StartStop) {
  NestedTracingScenarioForTesting tracing_scenario(
      ParseNestedScenarioConfigFromText(kDefaultNestedConfig), &delegate);

  tracing_scenario.Enable();
  EXPECT_EQ(NestedTracingScenario::State::kEnabled,
            tracing_scenario.current_state());
  EXPECT_CALL(delegate, OnNestedScenarioStart(&tracing_scenario)).Times(1);
  EXPECT_TRUE(base::trace_event::EmitNamedTrigger("start_trigger"));

  EXPECT_CALL(delegate, OnNestedScenarioStop(&tracing_scenario)).Times(1);
  EXPECT_TRUE(base::trace_event::EmitNamedTrigger("stop_trigger"));
  EXPECT_EQ(NestedTracingScenario::State::kStopping,
            tracing_scenario.current_state());
  tracing_scenario.Disable();
  EXPECT_EQ(NestedTracingScenario::State::kDisabled,
            tracing_scenario.current_state());
}

TEST_F(NestedTracingScenarioTest, Upload) {
  NestedTracingScenarioForTesting tracing_scenario(
      ParseNestedScenarioConfigFromText(kDefaultNestedConfig), &delegate);

  tracing_scenario.Enable();
  EXPECT_CALL(delegate, OnNestedScenarioStart(&tracing_scenario)).Times(1);
  EXPECT_TRUE(base::trace_event::EmitNamedTrigger("start_trigger"));

  EXPECT_CALL(delegate, OnNestedScenarioUpload(&tracing_scenario, _)).Times(1);
  EXPECT_TRUE(base::trace_event::EmitNamedTrigger("upload_trigger"));

  EXPECT_EQ(NestedTracingScenario::State::kDisabled,
            tracing_scenario.current_state());
}

TEST_F(NestedTracingScenarioTest, StopUpload) {
  NestedTracingScenarioForTesting tracing_scenario(
      ParseNestedScenarioConfigFromText(kDefaultNestedConfig), &delegate);

  tracing_scenario.Enable();
  EXPECT_CALL(delegate, OnNestedScenarioStart(&tracing_scenario)).Times(1);
  EXPECT_TRUE(base::trace_event::EmitNamedTrigger("start_trigger"));

  base::RunLoop run_loop;
  EXPECT_CALL(delegate, OnNestedScenarioUpload(&tracing_scenario, _))
      .WillOnce(base::test::RunOnceClosure(run_loop.QuitClosure()));
  EXPECT_CALL(delegate, OnNestedScenarioStop(&tracing_scenario)).Times(1);

  EXPECT_TRUE(base::trace_event::EmitNamedTrigger("stop_trigger"));
  EXPECT_TRUE(base::trace_event::EmitNamedTrigger("upload_trigger"));

  run_loop.Run();
  EXPECT_EQ(NestedTracingScenario::State::kDisabled,
            tracing_scenario.current_state());
}

#if BUILDFLAG(IS_POSIX)

const char* kSystemProbeScenarioConfig = R"pb(
  scenario_name: "test_scenario"
  setup_rules: { manual_trigger_name: "setup_trigger" }
  start_rules: { manual_trigger_name: "start_trigger" }
  stop_rules: { manual_trigger_name: "stop_trigger" }
  trace_config: { data_sources: { config: { name: "mock_data_source" } } }
  use_system_backend: true
)pb";

class TracingScenarioSystemBackendTest : public testing::Test {
 public:
  static constexpr char kPerfettoConsumerSockName[] =
      "PERFETTO_CONSUMER_SOCK_NAME";
  static constexpr char kPerfettoProducerSockName[] =
      "PERFETTO_PRODUCER_SOCK_NAME";
  TracingScenarioSystemBackendTest()
      : task_environment_(content::BrowserTaskEnvironment::IO_MAINLOOP) {}

  void TearDown() override {
    system_producer_ = nullptr;
    system_service_ = nullptr;
    traced_process_ = nullptr;

    // Restore env variables after shutdown of the above to data races with the
    // muxer thread.
    if (saved_consumer_sock_name_) {
      ASSERT_EQ(0, setenv(kPerfettoConsumerSockName,
                          saved_consumer_sock_name_->c_str(), 1));
    } else {
      ASSERT_EQ(0, unsetenv(kPerfettoConsumerSockName));
    }
    if (saved_producer_sock_name_) {
      ASSERT_EQ(0, setenv(kPerfettoProducerSockName,
                          saved_producer_sock_name_->c_str(), 1));
    } else {
      ASSERT_EQ(0, unsetenv(kPerfettoConsumerSockName));
    }
  }

  // This is not implemented as Setup() because system tracing feature flags
  // affect the behavior of PerfettoTracedProcess::SetupClientLibrary(). The
  // tests will need to enable/disable features before |traced_process| is
  // created.
  void InitTracing() {
    // The test connects to the mock system service.
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    system_service_ = std::make_unique<tracing::MockSystemService>(temp_dir_);

    const auto* env_val = getenv(kPerfettoConsumerSockName);
    if (env_val) {
      saved_consumer_sock_name_ = env_val;
    }
    ASSERT_EQ(0, setenv(kPerfettoConsumerSockName,
                        system_service_->consumer().c_str(), 1));

    env_val = getenv(kPerfettoProducerSockName);
    if (env_val) {
      saved_producer_sock_name_ = env_val;
    }
    ASSERT_EQ(0, setenv(kPerfettoProducerSockName,
                        system_service_->producer().c_str(), 1));

    traced_process_ = std::make_unique<tracing::TracedProcessForTesting>(
        base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()}));
    background_tracing_manager_ =
        content::BackgroundTracingManager::CreateInstance(&tracing_delegate_);

    // Connect the producer to the tracing service.
    system_producer_ = std::make_unique<tracing::MockProducer>();
    system_producer_->Connect(system_service_->GetService(), "mock_producer");
    system_producer_->RegisterDataSource("mock_data_source");

    // Also enable the custom backend.
    static tracing::mojom::TracingService* s_service;
    s_service = &tracing_service_;
    // Check if the consumer backend is used.
    static bool* consumer_conn_created;
    consumer_conn_created = &custom_backend_consumer_conn_created_;
    auto factory = []() -> tracing::mojom::TracingService& {
      *consumer_conn_created = true;
      return *s_service;
    };
    tracing::PerfettoTracedProcess::Get().SetConsumerConnectionFactory(
        factory, base::SequencedTaskRunner::GetCurrentDefault());

    // Reset the callback that controls whether system consumer is allowed.
    tracing::PerfettoTracedProcess::Get().SetAllowSystemTracingConsumerCallback(
        base::RepeatingCallback<bool()>());
  }

 protected:
  // Not inheriting from TracingScenarioTest because initialization of
  // |traced_process_| depends on feature flags and environment variables.
  BrowserTaskEnvironment task_environment_;
  std::unique_ptr<tracing::TracedProcessForTesting> traced_process_;
  TestTracingScenarioDelegate delegate;
  content::TracingDelegate tracing_delegate_;
  std::unique_ptr<content::BackgroundTracingManager>
      background_tracing_manager_;

  base::ScopedTempDir temp_dir_;
  std::unique_ptr<tracing::MockSystemService> system_service_;
  std::optional<std::string> saved_consumer_sock_name_;
  std::optional<std::string> saved_producer_sock_name_;
  std::unique_ptr<tracing::MockProducer> system_producer_;

  tracing::PerfettoService perfetto_service_;
  tracing::TracingService tracing_service_;

  // Set to true when a consumer connection of the custom backend is created.
  bool custom_backend_consumer_conn_created_ = false;
};

// Test the system backend by creating a real Perfetto tracing service thread
// that listens to producer and consumer sockets. Config the tracing scenario to
// use the system backend and check from the producer that a trace session is
// started and stopped through the system backend.
TEST_F(TracingScenarioSystemBackendTest, StartStop) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kEnablePerfettoSystemTracing,
                            features::kEnablePerfettoSystemBackgroundTracing},
      /*disabled_features=*/{});
  InitTracing();

  bool allow_system_consumer_checked = false;
  auto callback =
      base::BindLambdaForTesting([&allow_system_consumer_checked]() {
        allow_system_consumer_checked = true;
        // Allow system consumer connections. This is typically checked in
        // ChromeTracingDelegate::IsSystemWideTracingEnabled().
        return true;
      });
  tracing::PerfettoTracedProcess::Get().SetAllowSystemTracingConsumerCallback(
      std::move(callback));

  // Create a real tracing scenario. Under the hood the system backend is used.
  auto tracing_scenario = TracingScenario::Create(
      ParseScenarioConfigFromText(kSystemProbeScenarioConfig), false, false,
      false, false, &delegate);

  tracing_scenario->Enable();
  EXPECT_EQ(TracingScenario::State::kEnabled,
            tracing_scenario->current_state());
  EXPECT_CALL(delegate, OnScenarioActive(tracing_scenario.get()))
      .WillOnce(testing::Return(true));
  EXPECT_TRUE(base::trace_event::EmitNamedTrigger("setup_trigger"));
  EXPECT_TRUE(base::trace_event::EmitNamedTrigger("start_trigger"));

  // Wait until the data source is started.
  base::RunLoop run_loop_start;
  EXPECT_CALL(*system_producer_.get(), OnStartDataSource(_, _))
      .WillOnce([&run_loop_start]() {
        run_loop_start.Quit();
        return true;
      });
  run_loop_start.Run();

  // Stop tracing. Wait until the data source is stopped.
  EXPECT_TRUE(base::trace_event::EmitNamedTrigger("stop_trigger"));
  base::RunLoop run_loop_stop;
  EXPECT_CALL(*system_producer_.get(), OnStopDataSource(_, _))
      .WillOnce([&run_loop_stop]() {
        run_loop_stop.Quit();
        return true;
      });
  run_loop_stop.Run();

  // Run until the scenario is idle.
  base::RunLoop run_loop_idle;
  EXPECT_CALL(delegate, OnScenarioIdle(tracing_scenario.get()))
      .WillOnce([&run_loop_idle]() {
        run_loop_idle.Quit();
        return true;
      });
  run_loop_idle.Run();

  EXPECT_EQ(TracingScenario::State::kDisabled,
            tracing_scenario->current_state());
  EXPECT_TRUE(allow_system_consumer_checked);
}

// Test that system consumer connections are denied.
TEST_F(TracingScenarioSystemBackendTest, SystemConsumerNotAllowedByCallback) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kEnablePerfettoSystemTracing,
                            features::kEnablePerfettoSystemBackgroundTracing},
      /*disabled_features=*/{});
  InitTracing();

  bool allow_system_consumer_checked = false;
  auto callback =
      base::BindLambdaForTesting([&allow_system_consumer_checked]() {
        allow_system_consumer_checked = true;
        return false;  // Deny system consumer connection.
      });

  tracing::PerfettoTracedProcess::Get().SetAllowSystemTracingConsumerCallback(
      std::move(callback));

  // Create a real tracing scenario.
  auto tracing_scenario = TracingScenario::Create(
      ParseScenarioConfigFromText(kSystemProbeScenarioConfig), false, false,
      false, false, &delegate);

  tracing_scenario->Enable();
  EXPECT_EQ(TracingScenario::State::kEnabled,
            tracing_scenario->current_state());
  EXPECT_CALL(delegate, OnScenarioActive(tracing_scenario.get()))
      .WillOnce(testing::Return(true));
  EXPECT_TRUE(base::trace_event::EmitNamedTrigger("setup_trigger"));
  EXPECT_TRUE(base::trace_event::EmitNamedTrigger("start_trigger"));

  // The system data producer isn't used.
  EXPECT_CALL(*system_producer_.get(), OnStartDataSource(_, _)).Times(0);
  // The callback doesn't allow system consumer. The scenario won't be started.

  tracing_scenario->Abort();
  base::RunLoop run_loop_idle;
  EXPECT_CALL(delegate, OnScenarioIdle(tracing_scenario.get()))
      .WillOnce([&run_loop_idle]() {
        run_loop_idle.Quit();
        return true;
      });
  run_loop_idle.Run();
  EXPECT_TRUE(allow_system_consumer_checked);
  // Not using the custom backend.
  EXPECT_FALSE(custom_backend_consumer_conn_created_);
}

// The scenario is ignored if it requests to use the system backend, but the
// system backend is unavailable (feature
// "EnablePerfettoSystemBackgroundTracing" isn't enabled).
TEST_F(TracingScenarioSystemBackendTest, FeatureNotEnabled_1) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kEnablePerfettoSystemTracing},
      /*disabled_features=*/{features::kEnablePerfettoSystemBackgroundTracing});
  InitTracing();

  // Create a real tracing scenario. The system backend isn't used.
  auto tracing_scenario = TracingScenario::Create(
      ParseScenarioConfigFromText(kSystemProbeScenarioConfig), false, false,
      false, false, &delegate);

  tracing_scenario->Enable();
  EXPECT_EQ(TracingScenario::State::kEnabled,
            tracing_scenario->current_state());
  EXPECT_CALL(delegate, OnScenarioActive(tracing_scenario.get())).Times(0);
  EXPECT_FALSE(base::trace_event::EmitNamedTrigger("setup_trigger"));
  EXPECT_FALSE(base::trace_event::EmitNamedTrigger("start_trigger"));

  tracing_scenario->Disable();
  EXPECT_EQ(TracingScenario::State::kDisabled,
            tracing_scenario->current_state());
  // Not using the custom backend.
  EXPECT_FALSE(custom_backend_consumer_conn_created_);
}

// EnablePerfettoSystemTracing doesn't have an effect on Android. In debugging
// (which is always true in content_unittests) system tracing is always enabled.
// Disable this test on Android.
#if !BUILDFLAG(IS_ANDROID)
// The scenario is ignored if it requests to use the system backend, but the
// system backend is unavailable (feature "EnablePerfettoSystemTracing" isn't
// enabled).
TEST_F(TracingScenarioSystemBackendTest, FeatureNotEnabled_2) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kEnablePerfettoSystemBackgroundTracing},
      /*disabled_features=*/{features::kEnablePerfettoSystemTracing});

  InitTracing();
  // Create a real tracing scenario. The system backend isn't used.
  auto tracing_scenario = TracingScenario::Create(
      ParseScenarioConfigFromText(kSystemProbeScenarioConfig), false, false,
      false, false, &delegate);

  tracing_scenario->Enable();
  EXPECT_EQ(TracingScenario::State::kEnabled,
            tracing_scenario->current_state());
  EXPECT_CALL(delegate, OnScenarioActive(tracing_scenario.get())).Times(0);
  EXPECT_FALSE(base::trace_event::EmitNamedTrigger("setup_trigger"));
  EXPECT_FALSE(base::trace_event::EmitNamedTrigger("start_trigger"));

  tracing_scenario->Disable();
  EXPECT_EQ(TracingScenario::State::kDisabled,
            tracing_scenario->current_state());
  // Not using the custom backend.
  EXPECT_FALSE(custom_backend_consumer_conn_created_);
}
#endif

const char* kScenarioConfigWithoutSystemBackend = R"pb(
  scenario_name: "test_scenario"
  setup_rules: { manual_trigger_name: "setup_trigger" }
  start_rules: { manual_trigger_name: "start_trigger" }
  stop_rules: { manual_trigger_name: "stop_trigger" }
  trace_config: { data_sources: { config: { name: "mock_data_source" } } }
)pb";

// The custom backend is used on a platform that has system background tracing
// enabled if the scenario doesn't specify the system backend
TEST_F(TracingScenarioSystemBackendTest, ScenarioConfigWithoutSystemBackend) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kEnablePerfettoSystemTracing,
                            features::kEnablePerfettoSystemBackgroundTracing},
      /*disabled_features=*/{});

  InitTracing();

  // Create a real tracing scenario. The system backend isn't used.
  auto tracing_scenario = TracingScenario::Create(
      ParseScenarioConfigFromText(kScenarioConfigWithoutSystemBackend), false,
      false, false, false, &delegate);

  tracing_scenario->Enable();
  EXPECT_EQ(TracingScenario::State::kEnabled,
            tracing_scenario->current_state());
  EXPECT_CALL(delegate, OnScenarioActive(tracing_scenario.get()))
      .WillOnce(testing::Return(true));
  EXPECT_TRUE(base::trace_event::EmitNamedTrigger("setup_trigger"));
  EXPECT_TRUE(base::trace_event::EmitNamedTrigger("start_trigger"));

  // The system data producer isn't used.
  EXPECT_CALL(*system_producer_.get(), OnStartDataSource(_, _)).Times(0);
  // Instead, the custom backend connection factory is called.
  base::test::RunUntil([&]() { return custom_backend_consumer_conn_created_; });

  EXPECT_TRUE(base::trace_event::EmitNamedTrigger("stop_trigger"));
  // Not waiting until OnScenarioIdle because mojo of |tracing_service_| isn't
  // fully set up.
}

#endif  //  BUILDFLAG(IS_POSIX)

}  // namespace content
