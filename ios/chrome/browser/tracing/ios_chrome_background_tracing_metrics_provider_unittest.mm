// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tracing/ios_chrome_background_tracing_metrics_provider.h"

#import "base/functional/callback_helpers.h"
#import "base/task/thread_pool.h"
#import "base/task/thread_pool/thread_pool_instance.h"
#import "base/test/bind.h"
#import "base/test/run_until.h"
#import "base/test/task_environment.h"
#import "base/tracing/perfetto_platform.h"
#import "components/variations/active_field_trials.h"
#import "components/variations/synthetic_trial_registry.h"
#import "ios/chrome/browser/tracing/ios_tracing_controller.h"
#import "services/tracing/public/cpp/background_tracing/background_tracing_rule.h"
#import "services/tracing/public/cpp/background_tracing/tracing_scenario.h"
#import "services/tracing/public/cpp/startup_tracing_controller.h"
#import "services/tracing/public/cpp/trace_startup_config.h"
#import "testing/platform_test.h"
#import "third_party/metrics_proto/chrome_user_metrics_extension.pb.h"
#import "third_party/perfetto/protos/perfetto/config/chrome/scenario_config.gen.h"

namespace tracing {

class IOSChromeBackgroundTracingMetricsProviderTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    IOSTracingController::MaybeCreateInstanceForTesting();
    IOSTracingController::GetInstance().InitializeForTesting();

    base::ThreadPoolInstance::Get()->FlushForTesting();
  }

  void TearDown() override {
    IOSTracingController::GetInstance().ResetForTesting();
    PlatformTest::TearDown();
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

TEST_F(IOSChromeBackgroundTracingMetricsProviderTest, HasIndependentMetrics) {
  IOSChromeBackgroundTracingMetricsProvider provider(nullptr);
  EXPECT_FALSE(provider.HasIndependentMetrics());

  // Add mock trace data
  perfetto::protos::gen::TriggerRule config;
  config.set_manual_trigger_name("test_trigger");
  auto rule = tracing::BackgroundTracingRule::Create(config);

  perfetto::protos::gen::ScenarioConfig scenario_config;
  scenario_config.set_scenario_name("test_scenario");
  auto scenario = tracing::TracingScenario::Create(
      scenario_config, /*enable_privacy_filter=*/false,
      /*is_local_scenario=*/false, /*enable_package_name_filter=*/false,
      /*request_startup_tracing=*/false,
      static_cast<tracing::TracingScenario::Delegate*>(
          &IOSTracingController::GetInstance()));

  base::Token uuid = base::Token::CreateRandom();
  std::string fake_trace = "fake trace data";
  IOSTracingController::GetInstance().SaveTraceForTesting(
      std::move(fake_trace), scenario->scenario_name(), rule->rule_name(),
      uuid);

  base::ThreadPoolInstance::Get()->FlushForTesting();
  EXPECT_TRUE(base::test::RunUntil(
      [&]() -> bool { return provider.HasIndependentMetrics(); }));
}

TEST_F(IOSChromeBackgroundTracingMetricsProviderTest, ProvideMetrics) {
  IOSChromeBackgroundTracingMetricsProvider provider(nullptr);

  // Add mock trace data
  perfetto::protos::gen::TriggerRule config;
  config.set_manual_trigger_name("test_trigger");
  auto rule = tracing::BackgroundTracingRule::Create(config);

  perfetto::protos::gen::ScenarioConfig scenario_config;
  scenario_config.set_scenario_name("test_scenario");
  auto scenario = tracing::TracingScenario::Create(
      scenario_config, /*enable_privacy_filter=*/false,
      /*is_local_scenario=*/false, /*enable_package_name_filter=*/false,
      /*request_startup_tracing=*/false,
      static_cast<tracing::TracingScenario::Delegate*>(
          &IOSTracingController::GetInstance()));

  base::Token uuid = base::Token::CreateRandom();
  std::string fake_trace = "fake trace data";
  IOSTracingController::GetInstance().SaveTraceForTesting(
      std::move(fake_trace), scenario->scenario_name(), rule->rule_name(),
      uuid);

  base::ThreadPoolInstance::Get()->FlushForTesting();
  ASSERT_TRUE(base::test::RunUntil(
      [&]() -> bool { return provider.HasIndependentMetrics(); }));

  metrics::ChromeUserMetricsExtension uma_proto;
  bool callback_called = false;
  bool callback_success = false;

  provider.ProvideIndependentMetrics(
      base::DoNothing(), base::BindLambdaForTesting([&](bool success) {
        callback_called = true;
        callback_success = success;
      }),
      &uma_proto, /*snapshot_manager=*/nullptr);

  base::ThreadPoolInstance::Get()->FlushForTesting();
  EXPECT_TRUE(base::test::RunUntil([&]() -> bool { return callback_called; }));
  EXPECT_TRUE(callback_success);
  EXPECT_EQ(uma_proto.trace_log_size(), 1);
  if (uma_proto.trace_log_size() > 0) {
    EXPECT_FALSE(uma_proto.trace_log(0).raw_data().empty());
  }
}

TEST_F(IOSChromeBackgroundTracingMetricsProviderTest, ProvideSeedVersion) {
  variations::SyntheticTrialRegistry registry;
  IOSChromeBackgroundTracingMetricsProvider provider(&registry);

  variations::SetSeedVersion("test_seed_version");
  provider.Init();

  metrics::SystemProfileProto system_profile_proto;
  provider.RecordSystemProfileMetrics(system_profile_proto);

  EXPECT_EQ(system_profile_proto.variations_seed_version(),
            "test_seed_version");
}

}  // namespace tracing
