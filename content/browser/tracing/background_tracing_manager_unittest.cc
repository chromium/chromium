// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/scoped_temp_dir.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "build/build_config.h"
#include "content/browser/tracing/background_tracing_config_impl.h"
#include "content/browser/tracing/background_tracing_manager_impl.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "content/public/test/browser_task_environment.h"
#include "net/base/network_change_notifier.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/zlib/google/compression_utils.h"

namespace content {

namespace {

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
    BackgroundTracingManagerImpl::GetInstance()
        .AddEnabledStateObserverForTesting(this);
  }

  ~TestBackgroundTracingHelper() {
    BackgroundTracingManagerImpl::GetInstance()
        .RemoveEnabledStateObserverForTesting(this);
  }

  void OnTraceSaved() override { wait_for_trace_saved_.Quit(); }

  void WaitForTraceSaved() { wait_for_trace_saved_.Run(); }

 private:
  base::RunLoop wait_for_trace_saved_;
};

class MockBrowserClient : public content::ContentBrowserClient {
 public:
  MockBrowserClient(base::FilePath traces_dir) : traces_dir_(traces_dir) {}
  ~MockBrowserClient() override {}

  absl::optional<base::FilePath> GetLocalTracesDirectory() override {
    return traces_dir_;
  }

 private:
  base::FilePath traces_dir_;
};

}  // namespace

class BackgroundTracingManagerTest : public testing::Test {
 public:
  BackgroundTracingManagerTest() {
    background_tracing_manager_ =
        content::BackgroundTracingManager::CreateInstance();
  }

 protected:
  BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<content::BackgroundTracingManager>
      background_tracing_manager_;
};

TEST_F(BackgroundTracingManagerTest, HasTraceToUpload) {
  base::Value::Dict dict;
  dict.Set("mode", "REACTIVE_TRACING_MODE");
  dict.Set("category", "BENCHMARK_STARTUP");

  base::Value::List rules_list;
  {
    base::Value::Dict rules_dict;
    rules_dict.Set("rule", "MONITOR_AND_DUMP_WHEN_TRIGGER_NAMED");
    rules_dict.Set("trigger_name", "reactive_test");
    rules_list.Append(std::move(rules_dict));
  }
  dict.Set("configs", std::move(rules_list));
  dict.Set("upload_limit_kb", 2);
  dict.Set("upload_limit_network_kb", 1);

  std::unique_ptr<BackgroundTracingConfig> config(
      BackgroundTracingConfigImpl::FromDict(std::move(dict)));
  EXPECT_TRUE(config);

  EXPECT_TRUE(background_tracing_manager_->SetActiveScenario(
      std::move(config), BackgroundTracingManager::ANONYMIZE_DATA));

  {
    std::string trace_content(1500, 'a');

    TestBackgroundTracingHelper background_tracing_helper;
    background_tracing_manager_->SaveTraceForTesting(
        std::move(trace_content), "test_scenario", "test_rule");
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
        kDummyTrace, "test_scenario", "test_rule");
    background_tracing_helper.WaitForTraceSaved();
  }

  EXPECT_TRUE(background_tracing_manager_->HasTraceToUpload());

  std::string compressed_trace;
  base::RunLoop run_loop;
  background_tracing_manager_->GetTraceToUpload(
      base::BindLambdaForTesting([&](std::string trace_content) {
        compressed_trace = std::move(trace_content);
        run_loop.Quit();
      }));
  run_loop.Run();

  std::string serialized_trace;
  ASSERT_TRUE(compression::GzipUncompress(compressed_trace, &serialized_trace));
  EXPECT_EQ(kDummyTrace, serialized_trace);

  EXPECT_FALSE(background_tracing_manager_->HasTraceToUpload());
}

TEST_F(BackgroundTracingManagerTest, GetTraceToUploadNoDatabase) {
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitAndDisableFeature(kBackgroundTracingDatabase);
  {
    TestBackgroundTracingHelper background_tracing_helper;
    background_tracing_manager_->SaveTraceForTesting(
        kDummyTrace, "test_scenario", "test_rule");
    background_tracing_helper.WaitForTraceSaved();
  }

  EXPECT_TRUE(background_tracing_manager_->HasTraceToUpload());

  std::string compressed_trace;
  base::RunLoop run_loop;
  background_tracing_manager_->GetTraceToUpload(
      base::BindLambdaForTesting([&](std::string trace_content) {
        compressed_trace = std::move(trace_content);
        run_loop.Quit();
      }));
  run_loop.Run();

  std::string serialized_trace;
  ASSERT_TRUE(compression::GzipUncompress(compressed_trace, &serialized_trace));
  EXPECT_EQ(kDummyTrace, serialized_trace);

  EXPECT_FALSE(background_tracing_manager_->HasTraceToUpload());
}

TEST_F(BackgroundTracingManagerTest, SavedCountAfterClean) {
  {
    TestBackgroundTracingHelper background_tracing_helper;
    background_tracing_manager_->SaveTraceForTesting(
        kDummyTrace, "test_scenario", "test_rule");
    background_tracing_helper.WaitForTraceSaved();
  }
  EXPECT_EQ(1U,
            BackgroundTracingManagerImpl::GetInstance().GetScenarioSavedCount(
                "test_scenario"));

  task_environment_.FastForwardBy(base::Days(15));

  EXPECT_EQ(0U,
            BackgroundTracingManagerImpl::GetInstance().GetScenarioSavedCount(
                "test_scenario"));
}

TEST_F(BackgroundTracingManagerTest, SavedCountAfterDelete) {
  {
    TestBackgroundTracingHelper background_tracing_helper;
    background_tracing_manager_->SaveTraceForTesting(
        kDummyTrace, "test_scenario", "test_rule");
    background_tracing_helper.WaitForTraceSaved();
  }
  EXPECT_EQ(1U,
            BackgroundTracingManagerImpl::GetInstance().GetScenarioSavedCount(
                "test_scenario"));
  background_tracing_manager_->DeleteTracesInDateRange(
      base::Time::Now() - base::Days(1), base::Time::Now());
  task_environment_.RunUntilIdle();

  EXPECT_EQ(0U,
            BackgroundTracingManagerImpl::GetInstance().GetScenarioSavedCount(
                "test_scenario"));
}

TEST(BackgroundTracingManagerPersistentTest, DeleteTracesInDateRange) {
  BrowserTaskEnvironment task_environment{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  base::ScopedTempDir traces_dir;
  ASSERT_TRUE(traces_dir.CreateUniqueTempDir());
  content::ContentClient content_client;
  MockBrowserClient browser_client(traces_dir.GetPath());

  content::SetContentClient(&content_client);
  content::SetBrowserClientForTesting(&browser_client);

  {
    std::unique_ptr<content::BackgroundTracingManager>
        background_tracing_manager =
            content::BackgroundTracingManager::CreateInstance();
    BackgroundTracingManagerImpl::GetInstance().InitializeTraceReportDatabase();

    TestBackgroundTracingHelper background_tracing_helper;
    background_tracing_manager->SaveTraceForTesting(
        kDummyTrace, "test_scenario", "test_rule");
    background_tracing_helper.WaitForTraceSaved();
    EXPECT_EQ(1U,
              BackgroundTracingManagerImpl::GetInstance().GetScenarioSavedCount(
                  "test_scenario"));
  }
  // Ensure the database tear down completed.
  task_environment.RunUntilIdle();

  {
    std::unique_ptr<content::BackgroundTracingManager>
        background_tracing_manager =
            content::BackgroundTracingManager::CreateInstance();
    BackgroundTracingManagerImpl::GetInstance().InitializeTraceReportDatabase();
    task_environment.RunUntilIdle();
    EXPECT_EQ(1U,
              BackgroundTracingManagerImpl::GetInstance().GetScenarioSavedCount(
                  "test_scenario"));
  }
  // Ensure the database tear down completed.
  task_environment.RunUntilIdle();

  {
    std::unique_ptr<content::BackgroundTracingManager>
        background_tracing_manager =
            content::BackgroundTracingManager::CreateInstance();
    BackgroundTracingManagerImpl::GetInstance().InitializeTraceReportDatabase();

    auto now = base::Time::Now();
    background_tracing_manager->DeleteTracesInDateRange(now - base::Days(1),
                                                        now);
    task_environment.RunUntilIdle();
    EXPECT_EQ(0U,
              BackgroundTracingManagerImpl::GetInstance().GetScenarioSavedCount(
                  "test_scenario"));
  }

  content::SetBrowserClientForTesting(nullptr);
  content::SetContentClient(nullptr);
}

}  // namespace content
