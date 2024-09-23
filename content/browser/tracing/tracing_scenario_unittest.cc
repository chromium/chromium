// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/tracing/tracing_scenario.h"
#include <memory>

#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/task/thread_pool.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/test_proto_loader.h"
#include "base/trace_event/named_trigger.h"
#include "content/public/browser/background_tracing_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

const char* kDefaultNestedConfig = R"pb(
  scenario_name: "test_nested_scenario"
  start_rules: { name: "start_trigger" manual_trigger_name: "start_trigger" }
  stop_rules: { name: "stop_trigger" manual_trigger_name: "stop_trigger" }
  upload_rules: { name: "upload_trigger" manual_trigger_name: "upload_trigger" }
)pb";

const char* kDefaultConfig = R"pb(
  scenario_name: "test_scenario"
  setup_rules: { name: "setup_trigger" manual_trigger_name: "setup_trigger" }
  start_rules: { name: "start_trigger" manual_trigger_name: "start_trigger" }
  stop_rules: { name: "stop_trigger" manual_trigger_name: "stop_trigger" }
  upload_rules: { name: "upload_trigger" manual_trigger_name: "upload_trigger" }
  trace_config: {
    data_sources: { config: { name: "org.chromium.trace_metadata" } }
  }
  nested_scenarios: {
    scenario_name: "nested_scenario"
    start_rules: {
      name: "nested_start_trigger"
      manual_trigger_name: "nested_start_trigger"
    }
    stop_rules: {
      name: "nested_stop_trigger"
      manual_trigger_name: "nested_stop_trigger"
    }
    upload_rules: {
      name: "nested_upload_trigger"
      manual_trigger_name: "nested_upload_trigger"
    }
  }
  nested_scenarios: {
    scenario_name: "other_nested_scenario"
    start_rules: {
      name: "other_nested_start_trigger"
      manual_trigger_name: "other_nested_start_trigger"
    }
    stop_rules: {
      name: "other_nested_stop_trigger"
      manual_trigger_name: "other_nested_stop_trigger"
    }
    upload_rules: {
      name: "other_nested_upload_trigger"
      manual_trigger_name: "other_nested_upload_trigger"
    }
  }
)pb";

using testing::_;

class TestTracingScenarioDelegate : public TracingScenario::Delegate {
 public:
  ~TestTracingScenarioDelegate() = default;

  MOCK_METHOD(bool, OnScenarioActive, (TracingScenario * scenario), (override));
  MOCK_METHOD(bool, OnScenarioIdle, (TracingScenario * scenario), (override));
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
                        /*request_startup_tracing=*/true) {
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
            content::BackgroundTracingManager::CreateInstance()) {}

 protected:
  BrowserTaskEnvironment task_environment;
  TestTracingScenarioDelegate delegate;
  TestNestedTracingScenarioDelegate nested_delegate;
  std::unique_ptr<content::BackgroundTracingManager>
      background_tracing_manager_;
};

class NestedTracingScenarioTest : public testing::Test {
 public:
  NestedTracingScenarioTest()
      : background_tracing_manager_(
            content::BackgroundTracingManager::CreateInstance()) {}

 protected:
  BrowserTaskEnvironment task_environment;
  TestNestedTracingScenarioDelegate delegate;
  std::unique_ptr<content::BackgroundTracingManager>
      background_tracing_manager_;
};

}  // namespace

TEST_F(TracingScenarioTest, Init) {
  TracingScenarioForTesting tracing_scenario(
      ParseScenarioConfigFromText(R"pb(
        scenario_name: "test_scenario"
        trace_config: {
          data_sources: { config: { name: "org.chromium.trace_metadata" } }
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

  EXPECT_FALSE(base::trace_event::EmitNamedTrigger("stop_trigger"));
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
  EXPECT_EQ(TracingScenario::State::kRecording,
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
  EXPECT_EQ(TracingScenario::State::kRecording,
            tracing_scenario.current_state());
  EXPECT_FALSE(
      base::trace_event::EmitNamedTrigger("other_nested_start_trigger"));
  EXPECT_FALSE(base::trace_event::EmitNamedTrigger("stop_trigger"));

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
  EXPECT_EQ(TracingScenario::State::kRecording,
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
  EXPECT_TRUE(base::trace_event::EmitNamedTrigger("start_trigger"));

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
  EXPECT_TRUE(base::trace_event::EmitNamedTrigger("start_trigger"));

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
  EXPECT_TRUE(base::trace_event::EmitNamedTrigger("start_trigger"));
  EXPECT_TRUE(base::trace_event::EmitNamedTrigger("nested_start_trigger"));

  base::Token trace_uuid = tracing_scenario.GetSessionID();
  base::RunLoop run_loop;
  EXPECT_CALL(delegate, SaveTrace(&tracing_scenario, trace_uuid, _,
                                  std::string("this is a trace")))
      .WillOnce(base::test::RunOnceClosure(run_loop.QuitClosure()));
  EXPECT_CALL(delegate, OnScenarioIdle(&tracing_scenario))
      .WillOnce(testing::Return(true));

  EXPECT_TRUE(base::trace_event::EmitNamedTrigger("nested_upload_trigger"));

  run_loop.Run();
  EXPECT_EQ(TracingScenario::State::kDisabled,
            tracing_scenario.current_state());
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

}  // namespace content
