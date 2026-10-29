// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/public/cpp/background_tracing/background_tracing_manager.h"

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback_helpers.h"
#include "base/path_service.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_proto_loader.h"
#include "base/test/tracing/trace_test_utils.h"
#include "base/threading/thread_restrictions.h"
#include "base/token.h"
#include "base/trace_event/named_trigger.h"
#include "base/values.h"
#include "build/build_config.h"
#include "net/base/network_change_notifier.h"
#include "services/tracing/public/cpp/trace_startup_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/zlib/google/compression_utils.h"

namespace tracing {
namespace {

using testing::_;

const char kDummyTrace[] = "Trace bytes as serialized proto";

class MockNetworkChangeNotifier : public net::NetworkChangeNotifier {
 public:
  ConnectionType GetCurrentConnectionType() const override { return type_; }
  void set_type(ConnectionType type) { type_ = type; }

 private:
  ConnectionType type_;
};

class TestBackgroundTracingHelper
    : public BackgroundTracingManager::EnabledStateTestObserver {
 public:
  TestBackgroundTracingHelper() {
    BackgroundTracingManager::GetInstance().AddEnabledStateObserverForTesting(
        this);
  }

  ~TestBackgroundTracingHelper() {
    BackgroundTracingManager::GetInstance()
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
  void WaitForTraceSaved() { wait_for_trace_saved_.Run(); }

 private:
  base::RunLoop wait_for_scenario_idle_;
  base::RunLoop wait_for_trace_started_;
  base::RunLoop wait_for_trace_saved_;
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

class TestBackgroundTracingManager : public BackgroundTracingManager {
 public:
  explicit TestBackgroundTracingManager(
      base::FilePath traces_dir = base::FilePath())
      : traces_dir_(traces_dir) {}
  ~TestBackgroundTracingManager() override = default;

  MOCK_METHOD(void, MaybeConstructPendingAgents, (), (override));
  MOCK_METHOD(bool,
              IsRecordingAllowed,
              (bool is_local_scenario, base::TimeTicks scenario_start_time),
              (override));
  MOCK_METHOD(bool, ShouldSaveUnuploadedTrace, (), (override));
  MOCK_METHOD(bool, GetBackgroundStartupTracingEnabled, (), (const, override));
  MOCK_METHOD(std::string,
              RecordSerializedSystemProfileMetrics,
              (),
              (override));

  std::optional<base::FilePath> GetLocalTracesDirectory() override {
    return traces_dir_;
  }

 private:
  base::FilePath traces_dir_;
};

class BackgroundTracingManagerTest : public testing::Test {
 public:
  BackgroundTracingManagerTest() {
    CHECK(traces_dir_.CreateUniqueTempDir());
    background_tracing_manager_ =
        std::make_unique<TestBackgroundTracingManager>(traces_dir_.GetPath());
    ON_CALL(*background_tracing_manager_, IsRecordingAllowed(_, _))
        .WillByDefault(testing::Return(true));
    ON_CALL(*background_tracing_manager_, ShouldSaveUnuploadedTrace())
        .WillByDefault(testing::Return(true));
    ON_CALL(*background_tracing_manager_,
            RecordSerializedSystemProfileMetrics())
        .WillByDefault(testing::Return(""));
  }

 protected:
  base::ScopedTempDir traces_dir_;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  TraceStartupConfig startup_config_;
  base::test::TracingEnvironment tracing_environment_{
      base::SingleThreadTaskRunner::GetCurrentDefault()};
  std::unique_ptr<TestBackgroundTracingManager> background_tracing_manager_;
};

TEST_F(BackgroundTracingManagerTest, HasTraceToUpload) {
  background_tracing_manager_->SetUploadLimitsForTesting(2, 1);
  {
    std::string trace_content(1500, 'a');

    TestBackgroundTracingHelper background_tracing_helper;
    background_tracing_manager_->SaveTraceForTesting(
        std::move(trace_content), "test_scenario", "test_rule",
        base::Token::CreateRandom());
    background_tracing_helper.WaitForTraceSaved();
  }

  MockNetworkChangeNotifier notifier;
  notifier.set_type(net::NetworkChangeNotifier::CONNECTION_2G);
#if BUILDFLAG(IS_ANDROID)
  EXPECT_FALSE(background_tracing_manager_->HasTraceToUpload());
#endif

  notifier.set_type(net::NetworkChangeNotifier::CONNECTION_WIFI);
  EXPECT_TRUE(background_tracing_manager_->HasTraceToUpload());
}

TEST_F(BackgroundTracingManagerTest, GetTraceToUpload) {
  {
    TestBackgroundTracingHelper background_tracing_helper;
    background_tracing_manager_->SaveTraceForTesting(
        kDummyTrace, "test_scenario", "test_rule", base::Token::CreateRandom());
    background_tracing_helper.WaitForTraceSaved();
  }

  EXPECT_TRUE(background_tracing_manager_->HasTraceToUpload());

  std::string compressed_trace;
  base::RunLoop run_loop;
  background_tracing_manager_->GetTraceToUpload(
      base::BindLambdaForTesting([&](std::optional<std::string> trace_content,
                                     std::optional<std::string> system_profile,
                                     base::OnceClosure upload_complete) {
        ASSERT_TRUE(trace_content);
        compressed_trace = std::move(*trace_content);
        std::move(upload_complete).Run();
        run_loop.Quit();
      }));
  run_loop.Run();

  std::string serialized_trace;
  ASSERT_TRUE(compression::GzipUncompress(compressed_trace, &serialized_trace));
  EXPECT_EQ(kDummyTrace, serialized_trace);

  EXPECT_FALSE(background_tracing_manager_->HasTraceToUpload());
}

TEST_F(BackgroundTracingManagerTest, DISABLED_SavedCountPreventsStart) {
  constexpr const char kScenarioConfig[] = R"pb(
    scenarios: {
      scenario_name: "test_scenario"
      start_rules: {
        name: "start_trigger"
        manual_trigger_name: "start_trigger"
      }
      trace_config: {
        data_sources: { config: { name: "org.chromium.trace_metadata2" } }
      }
    }
  )pb";

  constexpr size_t kNumSavedTraces = 200;
  for (size_t i = 0; i < kNumSavedTraces; ++i) {
    TestBackgroundTracingHelper background_tracing_helper;
    background_tracing_manager_->SaveTraceForTesting(
        kDummyTrace, "test_scenario", "test_rule", base::Token::CreateRandom());
    background_tracing_helper.WaitForTraceSaved();
  }
  EXPECT_EQ(kNumSavedTraces, background_tracing_manager_->GetScenarioSavedCount(
                                 "test_scenario"));

  background_tracing_manager_->InitializeFieldScenarios(
      ParseFieldTracingConfigFromText(kScenarioConfig),
      BackgroundTracingManager::NO_DATA_FILTERING, false, 0);

  EXPECT_FALSE(base::trace_event::EmitNamedTrigger("start_trigger"));
}

TEST_F(BackgroundTracingManagerTest, SavedCountAfterClean) {
  {
    TestBackgroundTracingHelper background_tracing_helper;
    background_tracing_manager_->SaveTraceForTesting(
        kDummyTrace, "test_scenario", "test_rule", base::Token::CreateRandom());
    background_tracing_helper.WaitForTraceSaved();
  }
  EXPECT_EQ(
      1U, background_tracing_manager_->GetScenarioSavedCount("test_scenario"));

  task_environment_.FastForwardBy(base::Days(15));

  EXPECT_EQ(
      0U, background_tracing_manager_->GetScenarioSavedCount("test_scenario"));
}

TEST_F(BackgroundTracingManagerTest, SavedCountAfterDelete) {
  {
    TestBackgroundTracingHelper background_tracing_helper;
    background_tracing_manager_->SaveTraceForTesting(
        kDummyTrace, "test_scenario", "test_rule", base::Token::CreateRandom());
    background_tracing_helper.WaitForTraceSaved();
  }
  EXPECT_EQ(
      1U, background_tracing_manager_->GetScenarioSavedCount("test_scenario"));
  background_tracing_manager_->DeleteTracesInDateRange(
      base::Time::Now() - base::Days(1), base::Time::Now());
  task_environment_.RunUntilIdle();

  EXPECT_EQ(
      0U, background_tracing_manager_->GetScenarioSavedCount("test_scenario"));
}

TEST_F(BackgroundTracingManagerTest, UploadScenarioQuotaExceeded) {
  {
    TestBackgroundTracingHelper background_tracing_helper;
    background_tracing_manager_->SaveTraceForTesting(
        kDummyTrace, "test_scenario", "test_rule", base::Token::CreateRandom());
    background_tracing_helper.WaitForTraceSaved();
  }
  EXPECT_TRUE(background_tracing_manager_->HasTraceToUpload());

  base::RunLoop run_loop;
  background_tracing_manager_->GetTraceToUpload(
      base::BindLambdaForTesting([&](std::optional<std::string> trace_content,
                                     std::optional<std::string> system_profile,
                                     base::OnceClosure upload_complete) {
        std::move(upload_complete).Run();
        run_loop.Quit();
      }));
  run_loop.Run();

  {
    TestBackgroundTracingHelper background_tracing_helper;
    background_tracing_manager_->SaveTraceForTesting(
        kDummyTrace, "test_scenario", "test_rule", base::Token::CreateRandom());
    background_tracing_helper.WaitForTraceSaved();
  }
  EXPECT_FALSE(background_tracing_manager_->HasTraceToUpload());
}

TEST_F(BackgroundTracingManagerTest, UploadScenarioQuotaReset) {
  {
    TestBackgroundTracingHelper background_tracing_helper;
    background_tracing_manager_->SaveTraceForTesting(
        kDummyTrace, "test_scenario", "test_rule", base::Token::CreateRandom());
    background_tracing_helper.WaitForTraceSaved();
  }
  EXPECT_TRUE(background_tracing_manager_->HasTraceToUpload());

  base::RunLoop run_loop;
  background_tracing_manager_->GetTraceToUpload(
      base::IgnoreArgs<std::optional<std::string>, std::optional<std::string>,
                       base::OnceClosure>(run_loop.QuitClosure()));
  run_loop.Run();

  task_environment_.FastForwardBy(base::Days(8));

  {
    TestBackgroundTracingHelper background_tracing_helper;
    background_tracing_manager_->SaveTraceForTesting(
        kDummyTrace, "test_scenario", "test_rule", base::Token::CreateRandom());
    background_tracing_helper.WaitForTraceSaved();
  }
  EXPECT_TRUE(background_tracing_manager_->HasTraceToUpload());
}

TEST_F(BackgroundTracingManagerTest, SetupStartupTracing) {
  EXPECT_CALL(*background_tracing_manager_,
              GetBackgroundStartupTracingEnabled())
      .WillOnce(testing::Return(false));

  perfetto::protos::gen::ChromeFieldTracingConfig config;
  EXPECT_TRUE(background_tracing_manager_->InitializeFieldScenarios(
      config, BackgroundTracingManager::ANONYMIZE_DATA, false, 0));

  EXPECT_FALSE(base::trace_event::EmitNamedTrigger(
      base::trace_event::kStartupTracingTriggerName));
}

TEST_F(BackgroundTracingManagerTest, RunStartupTracing) {
  TestBackgroundTracingHelper background_tracing_helper;

  EXPECT_CALL(*background_tracing_manager_,
              GetBackgroundStartupTracingEnabled())
      .WillOnce(testing::Return(true));

  perfetto::protos::gen::ChromeFieldTracingConfig config;
  EXPECT_TRUE(background_tracing_manager_->InitializeFieldScenarios(
      config, BackgroundTracingManager::ANONYMIZE_DATA, false, 0));

  background_tracing_helper.ExpectOnScenarioActive("Startup");
  EXPECT_TRUE(base::trace_event::EmitNamedTrigger(
      base::trace_event::kStartupTracingTriggerName));

  background_tracing_helper.WaitForTraceStarted();

  background_tracing_helper.ExpectOnScenarioIdle("Startup");
  background_tracing_helper.WaitForScenarioIdle();

  background_tracing_helper.WaitForTraceSaved();
  EXPECT_TRUE(background_tracing_manager_->HasTraceToUpload());
}

TEST(BackgroundTracingManagerPersistentTest, DeleteTracesInDateRange) {
  base::test::TaskEnvironment task_environment{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  base::ScopedTempDir traces_dir;
  ASSERT_TRUE(traces_dir.CreateUniqueTempDir());

  {
    TestBackgroundTracingManager manager(traces_dir.GetPath());
    manager.InitializeTraceReportDatabase();

    TestBackgroundTracingHelper background_tracing_helper;
    manager.SaveTraceForTesting(kDummyTrace, "test_scenario", "test_rule",
                                base::Token::CreateRandom());
    background_tracing_helper.WaitForTraceSaved();
    EXPECT_EQ(1U, manager.GetScenarioSavedCount("test_scenario"));
  }
  // Ensure the database tear down completed.
  task_environment.RunUntilIdle();

  {
    TestBackgroundTracingManager manager(traces_dir.GetPath());
    manager.InitializeTraceReportDatabase();
    task_environment.RunUntilIdle();
    EXPECT_EQ(1U, manager.GetScenarioSavedCount("test_scenario"));
  }
  // Ensure the database tear down completed.
  task_environment.RunUntilIdle();

  {
    TestBackgroundTracingManager manager(traces_dir.GetPath());
    manager.InitializeTraceReportDatabase();

    auto now = base::Time::Now();
    manager.DeleteTracesInDateRange(now - base::Days(1), now);
    task_environment.RunUntilIdle();
    EXPECT_EQ(0U, manager.GetScenarioSavedCount("test_scenario"));
  }
}

}  // namespace
}  // namespace tracing
