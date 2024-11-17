// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/tracing/background_tracing_rule.h"

#include <memory>

#include "base/base_paths.h"
#include "base/metrics/histogram_macros_local.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/system/sys_info.h"
#include "base/test/bind.h"
#include "base/test/test_proto_loader.h"
#include "base/test/test_timeouts.h"
#include "base/time/time.h"
#include "base/trace_event/named_trigger.h"
#include "build/build_config.h"
#include "content/public/browser/background_tracing_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/perfetto/protos/perfetto/config/chrome/scenario_config.gen.h"

namespace content {

namespace {

base::FilePath GetTestDataRoot() {
  return base::PathService::CheckedGet(base::DIR_GEN_TEST_DATA_ROOT);
}

void CreateRuleConfig(const std::string& proto_text,
                      perfetto::protos::gen::TriggerRule& destination) {
  base::TestProtoLoader loader(GetTestDataRoot().Append(FILE_PATH_LITERAL(
                                   "third_party/perfetto/protos/perfetto/"
                                   "config/chrome/scenario_config.descriptor")),
                               "perfetto.protos.TriggerRule");
  std::string serialized_message;
  loader.ParseFromText(proto_text, serialized_message);
  ASSERT_TRUE(destination.ParseFromString(serialized_message));
}

}  // namespace

class BackgroundTracingRuleTest : public testing::Test {
 public:
  BackgroundTracingRuleTest() = default;

 protected:
  BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  std::unique_ptr<content::BackgroundTracingManager>
      background_tracing_manager =
          content::BackgroundTracingManager::CreateInstance();
};

TEST_F(BackgroundTracingRuleTest, HistogramRuleFromValidProto) {
  perfetto::protos::gen::TriggerRule config;
  CreateRuleConfig(
      R"pb(
        name: "test_rule"
        trigger_chance: 0.5
        delay_ms: 500
        histogram: { histogram_name: "foo" min_value: 1 max_value: 2 }
      )pb",
      config);
  auto rule = BackgroundTracingRule::Create(config);
  auto result = rule->ToProtoForTesting();
  EXPECT_EQ("test_rule", result.name());
  EXPECT_EQ(0.5, result.trigger_chance());
  EXPECT_EQ(500U, result.delay_ms());
  EXPECT_TRUE(result.has_histogram());
  EXPECT_EQ("foo", result.histogram().histogram_name());
  EXPECT_EQ(1, result.histogram().min_value());
  EXPECT_EQ(2, result.histogram().max_value());
}

TEST_F(BackgroundTracingRuleTest, HistogramRuleSucceedsOnLowerReferenceValue) {
  perfetto::protos::gen::TriggerRule config;
  CreateRuleConfig(
      R"pb(
        name: "test_rule"
        histogram: { histogram_name: "foo" min_value: 1 max_value: 2 }
      )pb",
      config);
  auto rule = BackgroundTracingRule::Create(config);
  base::RunLoop run_loop;
  rule->Install(base::BindLambdaForTesting([&](const BackgroundTracingRule*) {
    run_loop.Quit();
    return true;
  }));
  LOCAL_HISTOGRAM_COUNTS("foo", 1);
  run_loop.Run();
  rule->Uninstall();
}

TEST_F(BackgroundTracingRuleTest, HistogramRuleSucceedsOnUpperReferenceValue) {
  perfetto::protos::gen::TriggerRule config;
  CreateRuleConfig(
      R"pb(
        name: "test_rule"
        histogram: { histogram_name: "foo" min_value: 1 max_value: 2 }
      )pb",
      config);
  auto rule = BackgroundTracingRule::Create(config);
  base::RunLoop run_loop;
  rule->Install(base::BindLambdaForTesting([&](const BackgroundTracingRule*) {
    run_loop.Quit();
    return true;
  }));
  LOCAL_HISTOGRAM_COUNTS("foo", 2);
  run_loop.Run();
  rule->Uninstall();
}

TEST_F(BackgroundTracingRuleTest, HistogramRuleSucceedsOnSingleEnumValue) {
  perfetto::protos::gen::TriggerRule config;
  CreateRuleConfig(
      R"pb(
        name: "test_rule"
        histogram: { histogram_name: "foo" min_value: 1 max_value: 1 }
      )pb",
      config);
  auto rule = BackgroundTracingRule::Create(config);
  base::RunLoop run_loop;
  rule->Install(base::BindLambdaForTesting([&](const BackgroundTracingRule*) {
    run_loop.Quit();
    return true;
  }));
  LOCAL_HISTOGRAM_COUNTS("foo", 1);
  run_loop.Run();
  rule->Uninstall();
}

TEST_F(BackgroundTracingRuleTest, HistogramRuleFailsOnLowerHistogramSample) {
  perfetto::protos::gen::TriggerRule config;
  CreateRuleConfig(
      R"pb(
        name: "test_rule"
        histogram: { histogram_name: "foo" min_value: 1 max_value: 2 }
      )pb",
      config);
  auto rule = BackgroundTracingRule::Create(config);
  rule->Install(
      base::BindLambdaForTesting([](const BackgroundTracingRule*) -> bool {
        ADD_FAILURE();
        return true;
      }));
  LOCAL_HISTOGRAM_COUNTS("foo", 0);
  task_environment_.FastForwardBy(TestTimeouts::tiny_timeout());
  rule->Uninstall();
}

TEST_F(BackgroundTracingRuleTest, HistogramRuleFailsOnHigherHistogramSample) {
  perfetto::protos::gen::TriggerRule config;
  CreateRuleConfig(
      R"pb(
        name: "test_rule"
        histogram: { histogram_name: "foo" min_value: 1 max_value: 2 }
      )pb",
      config);
  auto rule = BackgroundTracingRule::Create(config);
  rule->Install(
      base::BindLambdaForTesting([](const BackgroundTracingRule*) -> bool {
        ADD_FAILURE();
        return true;
      }));
  LOCAL_HISTOGRAM_COUNTS("foo", 3);
  task_environment_.FastForwardBy(TestTimeouts::tiny_timeout());
  rule->Uninstall();
}

TEST_F(BackgroundTracingRuleTest, NamedRuleFromValidProto) {
  perfetto::protos::gen::TriggerRule config;
  CreateRuleConfig(R"pb(
                     name: "test_rule"
                     trigger_chance: 0.5
                     delay_ms: 500
                     manual_trigger_name: "test_trigger"
                   )pb",
                   config);
  auto rule = BackgroundTracingRule::Create(config);
  auto result = rule->ToProtoForTesting();
  EXPECT_EQ("test_rule", result.name());
  EXPECT_EQ(0.5, result.trigger_chance());
  EXPECT_EQ(500U, result.delay_ms());
  EXPECT_EQ("test_trigger", result.manual_trigger_name());
}

TEST_F(BackgroundTracingRuleTest, RuleFromEmptyProto) {
  perfetto::protos::gen::TriggerRule config;
  CreateRuleConfig(R"pb(
                     name: "test_rule"
                   )pb",
                   config);
  auto rule = BackgroundTracingRule::Create(config);
  EXPECT_EQ(nullptr, rule);
}

TEST_F(BackgroundTracingRuleTest, TimerRuleFromValidProto) {
  perfetto::protos::gen::TriggerRule config;
  CreateRuleConfig(R"pb(
                     name: "test_rule" trigger_chance: 0.5 delay_ms: 500
                   )pb",
                   config);
  auto rule = BackgroundTracingRule::Create(config);
  auto result = rule->ToProtoForTesting();
  EXPECT_EQ("test_rule", result.name());
  EXPECT_EQ(0.5, result.trigger_chance());
  EXPECT_EQ(500U, result.delay_ms());
}

TEST_F(BackgroundTracingRuleTest, TimerRuleTriggersAfterDelay) {
  perfetto::protos::gen::TriggerRule config;
  CreateRuleConfig(R"pb(
                     name: "test_rule" delay_ms: 10000
                   )pb",
                   config);

  base::TimeTicks start = base::TimeTicks::Now();
  auto rule = BackgroundTracingRule::Create(config);
  base::RunLoop run_loop;
  rule->Install(base::BindLambdaForTesting([&](const BackgroundTracingRule*) {
    run_loop.Quit();
    return true;
  }));
  run_loop.Run();
  DCHECK_GE(base::TimeTicks::Now(), start + base::Milliseconds(10000));
  rule->Uninstall();
}

TEST_F(BackgroundTracingRuleTest, RuleActivatesAfterDelay) {
  perfetto::protos::gen::TriggerRule config;
  CreateRuleConfig(R"pb(
                     name: "test_rule"
                     manual_trigger_name: "test_rule"
                     activation_delay_ms: 10000
                   )pb",
                   config);

  auto rule = BackgroundTracingRule::Create(config);

  base::RunLoop run_loop;
  rule->Install(base::BindLambdaForTesting([&](const BackgroundTracingRule*) {
    run_loop.Quit();
    return true;
  }));

  // Rule is not activated yet.
  EXPECT_FALSE(base::trace_event::EmitNamedTrigger("test_rule"));
  task_environment_.FastForwardBy(base::Seconds(10));
  EXPECT_TRUE(base::trace_event::EmitNamedTrigger("test_rule"));
  run_loop.Run();
  rule->Uninstall();
}

TEST_F(BackgroundTracingRuleTest, RepeatingIntervalRuleFromValidProto) {
  perfetto::protos::gen::TriggerRule config;
  CreateRuleConfig(
      R"pb(
        name: "test_rule"
        trigger_chance: 0.5
        delay_ms: 500
        repeating_interval: { period_ms: 1000 randomized: true }
      )pb",
      config);
  auto rule = BackgroundTracingRule::Create(config);
  auto result = rule->ToProtoForTesting();
  EXPECT_EQ("test_rule", result.name());
  EXPECT_EQ(0.5, result.trigger_chance());
  EXPECT_EQ(500U, result.delay_ms());
  EXPECT_TRUE(result.has_repeating_interval());
  EXPECT_EQ(1000U, result.repeating_interval().period_ms());
  EXPECT_TRUE(result.repeating_interval().randomized());
}

TEST_F(BackgroundTracingRuleTest, RepeatingIntervalRuleTriggersAfterDelay) {
  perfetto::protos::gen::TriggerRule config;
  CreateRuleConfig(R"pb(
                     name: "test_rule"
                     repeating_interval: { period_ms: 2000 }
                   )pb",
                   config);

  base::TimeTicks start = base::TimeTicks::Now();
  auto rule = BackgroundTracingRule::Create(config);
  std::vector<base::TimeTicks> trigger_times;
  auto callback = base::BindLambdaForTesting([&](const BackgroundTracingRule*) {
    trigger_times.push_back(base::TimeTicks::Now());
    return true;
  });
  rule->Install(callback);
  task_environment_.FastForwardBy(base::Seconds(2));  // Triggers at 2s
  rule->Uninstall();
  task_environment_.FastForwardBy(base::Seconds(1));
  rule->Install(callback);
  task_environment_.FastForwardBy(base::Seconds(2));  // Triggers at 4s
  rule->Uninstall();
  task_environment_.FastForwardBy(base::Seconds(2));  // Skips 6s
  rule->Install(callback);
  task_environment_.FastForwardBy(base::Seconds(1));  // Triggers at 8s

  EXPECT_EQ(std::vector<base::TimeTicks>({
                start + base::Seconds(2),
                start + base::Seconds(4),
                start + base::Seconds(8),
            }),
            trigger_times);
  rule->Uninstall();
}

TEST_F(BackgroundTracingRuleTest, RepeatingIntervalRuleTriggersRandomized) {
  perfetto::protos::gen::TriggerRule config;
  CreateRuleConfig(
      R"pb(
        name: "test_rule"
        repeating_interval: { period_ms: 2000 randomized: true }
      )pb",
      config);

  base::TimeTicks start = base::TimeTicks::Now();
  auto rule = BackgroundTracingRule::Create(config);
  std::vector<base::TimeTicks> trigger_times;
  auto callback = base::BindLambdaForTesting([&](const BackgroundTracingRule*) {
    trigger_times.push_back(base::TimeTicks::Now());
    return true;
  });
  rule->Install(callback);
  task_environment_.FastForwardBy(base::Seconds(4));  // Triggers twice
  ASSERT_EQ(2U, trigger_times.size());
  EXPECT_GE(trigger_times[0], start);
  EXPECT_LT(trigger_times[0], trigger_times[1]);
  EXPECT_GE(trigger_times[1], start + base::Seconds(2));
  rule->Uninstall();
}

}  // namespace content
