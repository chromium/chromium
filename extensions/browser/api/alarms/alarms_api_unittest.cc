// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file tests the chrome.alarms extension API.

#include "extensions/browser/api/alarms/alarms_api.h"

#include <stddef.h>

#include <array>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/strings/stringprintf.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/test/test_future.h"
#include "base/test/values_test_util.h"
#include "base/values.h"
#include "components/value_store/test_value_store_factory.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/fake_local_frame.h"
#include "extensions/browser/api/alarms/alarm_manager.h"
#include "extensions/browser/api/alarms/alarms_api_constants.h"
#include "extensions/browser/api_unittest.h"
#include "extensions/browser/state_store.h"
#include "extensions/common/api/alarms.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/extension_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

typedef extensions::api::alarms::Alarm JsAlarm;

namespace extensions {

class AlarmsObjectTest : public testing::Test {
 protected:
  JsAlarm CreateAlarm(std::string name,
                      double scheduled_time,
                      std::optional<double> period_in_minutes,
                      bool persist_across_sessions) {
    base::DictValue value =
        base::DictValue()
            .Set("name", name)
            .Set("scheduledTime", scheduled_time)
            .Set("persistAcrossSessions", persist_across_sessions);
    if (period_in_minutes) {
      value.Set("periodInMinutes", *period_in_minutes);
    }
    std::optional<JsAlarm> alarm = api::alarms::Alarm::FromValue(value);
    CHECK(alarm);
    return std::move(*alarm);
  }
};

TEST_F(AlarmsObjectTest, OperatorEqualsEquals) {
  // Everything is the same.
  EXPECT_TRUE(CreateAlarm("name", 10, 10, true) ==
              CreateAlarm("name", 10, 10, true));
  EXPECT_TRUE(CreateAlarm("name", 10, std::nullopt, true) ==
              CreateAlarm("name", 10, std::nullopt, true));

  // Different name.
  EXPECT_TRUE(CreateAlarm("name", 10, std::nullopt, true) !=
              CreateAlarm("none", 10, std::nullopt, true));
  // Different scheduledTime.
  EXPECT_TRUE(CreateAlarm("name", 10, std::nullopt, true) !=
              CreateAlarm("name", 20, std::nullopt, true));
  // Different periodInMinutes.
  EXPECT_TRUE(CreateAlarm("name", 10, 10, true) !=
              CreateAlarm("name", 10, std::nullopt, true));
  EXPECT_TRUE(CreateAlarm("name", 10, 10, true) !=
              CreateAlarm("name", 10, 20, true));
  // Different persistAcrossSessions.
  EXPECT_TRUE(CreateAlarm("name", 10, 10, true) !=
              CreateAlarm("name", 10, 10, false));
}

// If this test fails, please update api::alarms::Alarm comparison method in
// alarm_manager.cc.
TEST_F(AlarmsObjectTest, EnumerateMembers) {
  // If Alarm object WebIDL changes, so will its DictValue representation.
  JsAlarm alarm = CreateAlarm("name", 10, 10, true);
  const base::DictValue dict = alarm.ToValue();
  EXPECT_EQ(4u, dict.size());
  EXPECT_TRUE(dict.contains("name"));
  EXPECT_TRUE(dict.contains("scheduledTime"));
  EXPECT_TRUE(dict.contains("periodInMinutes"));
  EXPECT_TRUE(dict.contains("persistAcrossSessions"));
}

namespace {

// Test delegate which quits the message loop when an alarm fires.
class AlarmDelegate : public AlarmManager::Delegate {
 public:
  ~AlarmDelegate() override {}
  void OnAlarm(const ExtensionId& extension_id, const Alarm& alarm) override {
    alarms_seen.push_back(alarm.js_alarm->name);
    if (!quit_closure_.is_null()) {
      std::move(quit_closure_).Run();
    }
  }
  void WaitForAlarm() {
    base::RunLoop loop;
    quit_closure_ = loop.QuitClosure();
    loop.Run();
  }
  std::vector<std::string> alarms_seen;
  base::OnceClosure quit_closure_;
};

// Counts the number of times each extension's alarms are written to storage.
class AlarmsStorageWriteCounter : public StateStore::TestObserver {
 public:
  void WillSetExtensionValue(const ExtensionId& extension_id,
                             const std::string& key) override {
    // AlarmManager modifies only its own data.
    CHECK_EQ("alarms", key);
    ++write_counts[extension_id];
  }
  std::map<ExtensionId, int> write_counts;
};

}  // namespace

void RunScheduleNextPoll(AlarmManager* alarm_manager) {
  alarm_manager->ScheduleNextPoll();
}

class ExtensionAlarmsTest : public ApiUnitTest {
 public:
  using ApiUnitTest::RunFunction;

  void SetUp() override {
    ApiUnitTest::SetUp();

    alarm_manager_ = AlarmManager::Get(browser_context());
    alarm_manager_->SetClockForTesting(&test_clock_);

    auto delegate = std::make_unique<AlarmDelegate>();
    alarm_delegate_ = delegate.get();
    alarm_manager_->set_delegate(std::move(delegate));

    test_clock_.SetNow(base::Time::FromSecondsSinceUnixEpoch(10));
  }

  void TearDown() override {
    // Drop unowned references before superclass destroys them.
    alarm_delegate_ = nullptr;
    alarm_manager_ = nullptr;
    ApiUnitTest::TearDown();
  }

  void CreateAlarm(const std::string& args) {
    RunFunction(base::MakeRefCounted<AlarmsCreateFunction>(&test_clock_), args);
  }

  // Calls alarms.clear() and forwards the original boolean result.
  bool ClearAlarm(const std::string& name) {
    std::optional<base::Value> result = RunFunctionAndReturnValue(
        base::MakeRefCounted<AlarmsClearFunction>(), "[\"" + name + "\"]");
    CHECK(result);
    CHECK(result->is_bool());
    return result->GetBool();
  }

  std::string FailToCreateAlarm(const std::string& args) {
    return RunFunctionAndReturnError(
        base::MakeRefCounted<AlarmsCreateFunction>(&test_clock_), args);
  }

  void ClearAllAlarms() {
    RunFunctionAndReturnValue(base::MakeRefCounted<AlarmsClearAllFunction>(),
                              "[]");
  }

  // Takes a JSON result from a function and converts it to a vector of
  // JsAlarms.
  std::vector<JsAlarm> ToAlarmList(const std::optional<base::Value>& value) {
    std::vector<JsAlarm> list;
    if (!value) {
      return list;
    }
    for (const auto& item : value->GetList()) {
      auto alarm = JsAlarm::FromValue(item);
      if (!alarm) {
        ADD_FAILURE() << "Failed to parse JsAlarm." << item;
        return list;
      }
      list.push_back(std::move(alarm).value());
    }
    return list;
  }

  // Creates up to 3 alarms using the extension API.
  void CreateAlarms(size_t num_alarms) {
    CHECK_LE(num_alarms, 3U);

    static constexpr std::array kCreateArgs = {
        "[null, {\"periodInMinutes\": 0.001}]",
        "[\"7\", {\"periodInMinutes\": 7}]",
        "[\"0\", {\"delayInMinutes\": 0}]",
    };
    for (size_t i = 0; i < num_alarms; ++i) {
      std::optional<base::Value> result = RunFunctionAndReturnValue(
          base::MakeRefCounted<AlarmsCreateFunction>(&test_clock_),
          kCreateArgs[i]);
      EXPECT_FALSE(result);
    }
  }

  base::SimpleTestClock test_clock_;
  raw_ptr<AlarmManager> alarm_manager_;
  raw_ptr<AlarmDelegate> alarm_delegate_;
};

void ExtensionAlarmsTestGetAllAlarmsCallback(
    const AlarmManager::AlarmList* alarms) {
  // Ensure the alarm is gone.
  ASSERT_FALSE(alarms);
}

void ExtensionAlarmsTestGetAlarmCallback(ExtensionAlarmsTest* test,
                                         Alarm* alarm) {
  ASSERT_TRUE(alarm);
  EXPECT_EQ("", alarm->js_alarm->name);
  EXPECT_DOUBLE_EQ(10000, alarm->js_alarm->scheduled_time);
  EXPECT_FALSE(alarm->js_alarm->period_in_minutes);

  // Now wait for the alarm to fire. Our test delegate will quit the
  // MessageLoop when that happens.
  test->alarm_delegate_->WaitForAlarm();

  ASSERT_EQ(1u, test->alarm_delegate_->alarms_seen.size());
  EXPECT_EQ("", test->alarm_delegate_->alarms_seen[0]);

  // Ensure the alarm is gone.
  test->alarm_manager_->GetAllAlarms(
      test->extension()->id(),
      base::BindOnce(ExtensionAlarmsTestGetAllAlarmsCallback));
}

TEST_F(ExtensionAlarmsTest, Create) {
  test_clock_.SetNow(base::Time::FromSecondsSinceUnixEpoch(10));
  // Create 1 non-repeating alarm.
  CreateAlarm("[null, {\"delayInMinutes\": 0}]");

  alarm_manager_->GetAlarm(
      extension()->id(), std::string(),
      base::BindOnce(ExtensionAlarmsTestGetAlarmCallback, this));
}

TEST_F(ExtensionAlarmsTest, CreateNameInObject) {
  test_clock_.SetNow(base::Time::FromSecondsSinceUnixEpoch(10));
  // Create 1 non-repeating alarm passing name in object.
  CreateAlarm("[null, {\"name\": \"Alarm Name\", \"delayInMinutes\": 0}]");

  base::test::TestFuture<Alarm*> alarm_future;
  alarm_manager_->GetAlarm(extension()->id(), "Alarm Name",
                           alarm_future.GetCallback());
  Alarm* alarm = alarm_future.Get();
  ASSERT_TRUE(alarm);
  EXPECT_EQ("Alarm Name", alarm->js_alarm->name);
  EXPECT_DOUBLE_EQ(10000, alarm->js_alarm->scheduled_time);
  EXPECT_FALSE(alarm->js_alarm->period_in_minutes);

  // Now wait for the alarm to fire. Our test delegate will quit the
  // `MessageLoop` when that happens.
  alarm_delegate_->WaitForAlarm();

  ASSERT_EQ(1u, alarm_delegate_->alarms_seen.size());
  EXPECT_EQ("Alarm Name", alarm_delegate_->alarms_seen[0]);

  // Ensure the alarm is gone.
  base::test::TestFuture<const AlarmList*> alarm_list_future;
  alarm_manager_->GetAllAlarms(extension()->id(),
                               alarm_list_future.GetCallback());
  const AlarmList* alarm_list = alarm_list_future.Get();
  ASSERT_FALSE(alarm_list);
}

// Passing alarm name twice is not valid.
TEST_F(ExtensionAlarmsTest, CreateNameInvalid) {
  EXPECT_EQ("Cannot set alarm name in both separate argument and object form.",
            FailToCreateAlarm(
                R"(["Invalid", {"name": "Invalid", "delayInMinutes": 0}])"));
}

void ExtensionAlarmsTestCreateRepeatingGetAlarmCallback(
    ExtensionAlarmsTest* test,
    Alarm* alarm) {
  ASSERT_TRUE(alarm);
  EXPECT_EQ("", alarm->js_alarm->name);
  EXPECT_DOUBLE_EQ(10060, alarm->js_alarm->scheduled_time);
  EXPECT_THAT(alarm->js_alarm->period_in_minutes, testing::Eq(0.001));

  test->test_clock_.Advance(base::Seconds(1));
  // Now wait for the alarm to fire. Our test delegate will quit the
  // MessageLoop when that happens.
  test->alarm_delegate_->WaitForAlarm();

  test->test_clock_.Advance(base::Seconds(1));
  // Wait again, and ensure the alarm fires again.
  RunScheduleNextPoll(test->alarm_manager_);
  test->alarm_delegate_->WaitForAlarm();

  ASSERT_EQ(2u, test->alarm_delegate_->alarms_seen.size());
  EXPECT_EQ("", test->alarm_delegate_->alarms_seen[0]);
}

TEST_F(ExtensionAlarmsTest, CreateRepeating) {
  test_clock_.SetNow(base::Time::FromSecondsSinceUnixEpoch(10));

  // Create 1 repeating alarm.
  CreateAlarm("[null, {\"periodInMinutes\": 0.001}]");

  alarm_manager_->GetAlarm(
      extension()->id(), std::string(),
      base::BindOnce(ExtensionAlarmsTestCreateRepeatingGetAlarmCallback, this));
}

void ExtensionAlarmsTestCreateAbsoluteGetAlarm2Callback(
    ExtensionAlarmsTest* test,
    Alarm* alarm) {
  ASSERT_FALSE(alarm);

  ASSERT_EQ(1u, test->alarm_delegate_->alarms_seen.size());
  EXPECT_EQ("", test->alarm_delegate_->alarms_seen[0]);
}

void ExtensionAlarmsTestCreateAbsoluteGetAlarm1Callback(
    ExtensionAlarmsTest* test,
    Alarm* alarm) {
  ASSERT_TRUE(alarm);
  EXPECT_EQ("", alarm->js_alarm->name);
  EXPECT_DOUBLE_EQ(10001, alarm->js_alarm->scheduled_time);
  EXPECT_FALSE(alarm->js_alarm->period_in_minutes.has_value());

  test->test_clock_.SetNow(base::Time::FromSecondsSinceUnixEpoch(10.1));
  // Now wait for the alarm to fire. Our test delegate will quit the
  // MessageLoop when that happens.
  test->alarm_delegate_->WaitForAlarm();

  test->alarm_manager_->GetAlarm(
      test->extension()->id(), std::string(),
      base::BindOnce(ExtensionAlarmsTestCreateAbsoluteGetAlarm2Callback, test));
}

TEST_F(ExtensionAlarmsTest, CreateAbsolute) {
  test_clock_.SetNow(base::Time::FromSecondsSinceUnixEpoch(9.99));
  CreateAlarm("[null, {\"when\": 10001}]");

  alarm_manager_->GetAlarm(
      extension()->id(), std::string(),
      base::BindOnce(ExtensionAlarmsTestCreateAbsoluteGetAlarm1Callback, this));
}

void ExtensionAlarmsTestCreateRepeatingWithQuickFirstCallGetAlarm3Callback(
    ExtensionAlarmsTest* test,
    Alarm* alarm) {
  ASSERT_TRUE(alarm);
  EXPECT_THAT(test->alarm_delegate_->alarms_seen, testing::ElementsAre("", ""));
}

void ExtensionAlarmsTestCreateRepeatingWithQuickFirstCallGetAlarm2Callback(
    ExtensionAlarmsTest* test,
    Alarm* alarm) {
  ASSERT_TRUE(alarm);
  EXPECT_THAT(test->alarm_delegate_->alarms_seen, testing::ElementsAre(""));

  test->test_clock_.SetNow(base::Time::FromSecondsSinceUnixEpoch(11.1));

  test->alarm_delegate_->WaitForAlarm();

  test->alarm_manager_->GetAlarm(
      test->extension()->id(), std::string(),
      base::BindOnce(
          ExtensionAlarmsTestCreateRepeatingWithQuickFirstCallGetAlarm3Callback,
          test));
}

void ExtensionAlarmsTestCreateRepeatingWithQuickFirstCallGetAlarm1Callback(
    ExtensionAlarmsTest* test,
    Alarm* alarm) {
  ASSERT_TRUE(alarm);
  EXPECT_EQ("", alarm->js_alarm->name);
  EXPECT_DOUBLE_EQ(10001, alarm->js_alarm->scheduled_time);
  EXPECT_THAT(alarm->js_alarm->period_in_minutes, testing::Eq(0.001));

  test->test_clock_.SetNow(base::Time::FromSecondsSinceUnixEpoch(10.1));
  // Now wait for the alarm to fire. Our test delegate will quit the
  // MessageLoop when that happens.
  test->alarm_delegate_->WaitForAlarm();

  test->alarm_manager_->GetAlarm(
      test->extension()->id(), std::string(),
      base::BindOnce(
          ExtensionAlarmsTestCreateRepeatingWithQuickFirstCallGetAlarm2Callback,
          test));
}

TEST_F(ExtensionAlarmsTest, CreateRepeatingWithQuickFirstCall) {
  test_clock_.SetNow(base::Time::FromSecondsSinceUnixEpoch(9.99));
  CreateAlarm("[null, {\"when\": 10001, \"periodInMinutes\": 0.001}]");

  alarm_manager_->GetAlarm(
      extension()->id(), std::string(),
      base::BindOnce(
          ExtensionAlarmsTestCreateRepeatingWithQuickFirstCallGetAlarm1Callback,
          this));
}

void ExtensionAlarmsTestCreateDupeGetAllAlarmsCallback(
    const AlarmManager::AlarmList* alarms) {
  ASSERT_TRUE(alarms);
  EXPECT_EQ(1u, alarms->size());
  EXPECT_DOUBLE_EQ(430000, (*alarms)[0].js_alarm->scheduled_time);
}

TEST_F(ExtensionAlarmsTest, CreateDupe) {
  test_clock_.SetNow(base::Time::FromSecondsSinceUnixEpoch(10));

  // Create 2 duplicate alarms. The first should be overridden.
  CreateAlarm("[\"dup\", {\"delayInMinutes\": 1}]");
  CreateAlarm("[\"dup\", {\"delayInMinutes\": 7}]");

  alarm_manager_->GetAllAlarms(
      extension()->id(),
      base::BindOnce(ExtensionAlarmsTestCreateDupeGetAllAlarmsCallback));
}

class ConsoleLogMessageLocalFrame : public content::FakeLocalFrame {
 public:
  void AddMessageToConsole(blink::mojom::ConsoleMessageLevel level,
                           const std::string& message,
                           bool discard_duplicates) override {
    message_count_++;
    last_level_ = level;
    last_message_ = message;
  }
  unsigned message_count() const { return message_count_; }
  const std::string& last_message() const { return last_message_; }
  blink::mojom::ConsoleMessageLevel last_level() const {
    return last_level_.value();
  }

 private:
  unsigned message_count_ = 0;
  std::optional<blink::mojom::ConsoleMessageLevel> last_level_;
  std::string last_message_;
};

class ExtensionAlarmsLogTest : public ExtensionAlarmsTest {
  void SetUp() override {
    ExtensionAlarmsTest::SetUp();

    // Make sure there's a RenderViewHost for alarms to warn into.
    CreateExtensionPage();
  }
};

TEST_F(ExtensionAlarmsLogTest, CreateDelayBelowMinimum) {
  // Create an alarm with delay below the minimum accepted value.
  ConsoleLogMessageLocalFrame local_frame;
  local_frame.Init(
      contents()->GetPrimaryMainFrame()->GetRemoteAssociatedInterfaces());
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(local_frame.message_count(), 0u);
  CreateAlarm("[\"negative\", {\"delayInMinutes\": -0.2}]");
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(local_frame.message_count(), 1u);

  EXPECT_EQ(blink::mojom::ConsoleMessageLevel::kWarning,
            local_frame.last_level());
  EXPECT_THAT(local_frame.last_message(),
              testing::HasSubstr(
                  "delay is less than the minimum duration of 1 second"));
}

TEST_F(ExtensionAlarmsLogTest, RejectLongAlarmName) {
  // Set up context for the test.
  ConsoleLogMessageLocalFrame local_frame;
  local_frame.Init(
      contents()->GetPrimaryMainFrame()->GetRemoteAssociatedInterfaces());
  ASSERT_EQ(local_frame.message_count(), 0u);

  // Short alarm names (no longer than 1024 characters) do not result in
  // warnings.
  CreateAlarm("[\"" + std::string(1024, 'a') + "\", {\"when\": 0}]");
  this->alarm_delegate_->WaitForAlarm();
  ASSERT_EQ(local_frame.message_count(), 0u);

  // Long alarm names (1025 characters and longer) throw without altering
  // registered alarms. Attempt to create an alarm with a long name scheduled
  // far in the future (1 hour) should result in exteption with an appropriate
  // message and no alarm should be created.
  EXPECT_EQ(
      "Alarm name size is 1025 bytes which exceeds the limit of 1024 bytes.",
      FailToCreateAlarm("[\"" + std::string(1025, 'a') +
                        "\", {\"delayInMinutes\": 60}]"));

  // No alarm should be created.
  std::optional<base::Value> result = RunFunctionAndReturnValue(
      base::MakeRefCounted<AlarmsGetAllFunction>(), "[]");
  std::vector<JsAlarm> alarms = ToAlarmList(result);
  EXPECT_EQ(0u, alarms.size());
}

TEST_F(ExtensionAlarmsTest, Get) {
  test_clock_.SetNow(base::Time::FromSecondsSinceUnixEpoch(4));

  // Create 2 alarms, and make sure we can query them.
  CreateAlarms(2);

  // Get the default one.
  {
    std::optional<base::Value> result = RunFunctionAndReturnValue(
        base::MakeRefCounted<AlarmsGetFunction>(), "[null]");
    ASSERT_TRUE(result);
    ASSERT_TRUE(result->is_dict());
    auto alarm = JsAlarm::FromValue(result->GetDict());
    EXPECT_TRUE(alarm);
    EXPECT_EQ("", alarm->name);
    EXPECT_DOUBLE_EQ(4060, alarm->scheduled_time);
    EXPECT_THAT(alarm->period_in_minutes, testing::Eq(0.001));
  }

  // Get "7".
  {
    std::optional<base::Value> result = RunFunctionAndReturnValue(
        base::MakeRefCounted<AlarmsGetFunction>(), "[\"7\"]");
    ASSERT_TRUE(result);
    ASSERT_TRUE(result->is_dict());
    auto alarm = JsAlarm::FromValue(result->GetDict());
    EXPECT_TRUE(alarm);
    EXPECT_EQ("7", alarm->name);
    EXPECT_EQ(424000, alarm->scheduled_time);
    EXPECT_THAT(alarm->period_in_minutes, testing::Eq(7));
  }

  // Get a non-existent one.
  {
    std::optional<base::Value> result = RunFunctionAndReturnValue(
        base::MakeRefCounted<AlarmsGetFunction>(), "[\"nobody\"]");
    ASSERT_FALSE(result);
  }
}

TEST_F(ExtensionAlarmsTest, GetAll) {
  // Test getAll with 0 alarms.
  {
    std::optional<base::Value> result = RunFunctionAndReturnValue(
        base::MakeRefCounted<AlarmsGetAllFunction>(), "[]");
    std::vector<JsAlarm> alarms = ToAlarmList(result);
    EXPECT_EQ(0u, alarms.size());
  }

  // Create 2 alarms, and make sure we can query them.
  CreateAlarms(2);

  {
    std::optional<base::Value> result = RunFunctionAndReturnValue(
        base::MakeRefCounted<AlarmsGetAllFunction>(), "[null]");
    std::vector<JsAlarm> alarms = ToAlarmList(result);
    EXPECT_EQ(2u, alarms.size());

    // Test the "7" alarm.
    JsAlarm* alarm = &alarms[0];
    if (alarm->name != "7") {
      alarm = &alarms[1];
    }
    EXPECT_EQ("7", alarm->name);
    EXPECT_THAT(alarm->period_in_minutes, testing::Eq(7));
  }
}

void ExtensionAlarmsTestClearGetAllAlarms2Callback(
    const AlarmManager::AlarmList* alarms) {
  // Ensure the 0.001-minute alarm is still there, since it's repeating.
  ASSERT_TRUE(alarms);
  EXPECT_EQ(1u, alarms->size());
  EXPECT_THAT((*alarms)[0].js_alarm->period_in_minutes, testing::Eq(0.001));
}

void ExtensionAlarmsTestClearGetAllAlarms1Callback(
    ExtensionAlarmsTest* test,
    const AlarmManager::AlarmList* alarms) {
  ASSERT_TRUE(alarms);
  EXPECT_EQ(1u, alarms->size());
  EXPECT_THAT((*alarms)[0].js_alarm->period_in_minutes, testing::Eq(0.001));

  // Now wait for the alarms to fire, and ensure the cancelled alarms don't
  // fire.
  test->test_clock_.Advance(base::Milliseconds(60));
  RunScheduleNextPoll(test->alarm_manager_);

  test->alarm_delegate_->WaitForAlarm();

  ASSERT_EQ(1u, test->alarm_delegate_->alarms_seen.size());
  EXPECT_EQ("", test->alarm_delegate_->alarms_seen[0]);

  // Ensure the 0.001-minute alarm is still there, since it's repeating.
  test->alarm_manager_->GetAllAlarms(
      test->extension()->id(),
      base::BindOnce(ExtensionAlarmsTestClearGetAllAlarms2Callback));
}

TEST_F(ExtensionAlarmsTest, Clear) {
  // Clear a non-existent one.
  {
    const bool found = ClearAlarm("nobody");
    EXPECT_FALSE(found);
  }

  // Create 3 alarms.
  CreateAlarms(3);

  // Clear all but the 0.001-minute alarm.
  {
    const bool found = ClearAlarm("7");
    EXPECT_TRUE(found);
  }
  {
    const bool found = ClearAlarm("0");
    EXPECT_TRUE(found);
  }

  alarm_manager_->GetAllAlarms(
      extension()->id(),
      base::BindOnce(ExtensionAlarmsTestClearGetAllAlarms1Callback, this));
}

void ExtensionAlarmsTestClearAllGetAllAlarms2Callback(
    const AlarmManager::AlarmList* alarms) {
  ASSERT_FALSE(alarms);
}

void ExtensionAlarmsTestClearAllGetAllAlarms1Callback(
    ExtensionAlarmsTest* test,
    const AlarmManager::AlarmList* alarms) {
  ASSERT_TRUE(alarms);
  EXPECT_EQ(3u, alarms->size());

  // Clear them.
  test->RunFunction(base::MakeRefCounted<AlarmsClearAllFunction>(), "[]");
  test->alarm_manager_->GetAllAlarms(
      test->extension()->id(),
      base::BindOnce(ExtensionAlarmsTestClearAllGetAllAlarms2Callback));
}

TEST_F(ExtensionAlarmsTest, ClearAll) {
  // ClearAll with no alarms set and new behavior.
  {
    base::test::ScopedFeatureList features;
    features.InitAndEnableFeature(
        extensions_features::kApiAlarmsClearAllReturnUndefined);
    std::optional<base::Value> result = RunFunctionAndReturnValue(
        base::MakeRefCounted<AlarmsClearAllFunction>(), "[]");
    EXPECT_FALSE(result);
  }

  // ClearAll with an alarm set and new behavior.
  {
    base::test::ScopedFeatureList features;
    features.InitAndEnableFeature(
        extensions_features::kApiAlarmsClearAllReturnUndefined);
    CreateAlarm("[null, {\"delayInMinutes\": 10}]");
    std::optional<base::Value> result = RunFunctionAndReturnValue(
        base::MakeRefCounted<AlarmsClearAllFunction>(), "[]");
    EXPECT_FALSE(result);
  }

  // ClearAll with no alarms set and old behavior.
  {
    base::test::ScopedFeatureList features;
    features.InitAndDisableFeature(
        extensions_features::kApiAlarmsClearAllReturnUndefined);
    std::optional<base::Value> result = RunFunctionAndReturnValue(
        base::MakeRefCounted<AlarmsClearAllFunction>(), "[]");
    ASSERT_TRUE(result->is_bool());
    EXPECT_TRUE(result->GetBool());
  }

  // ClearAll with an alarm set and old behavior.
  {
    // Disable feature, create 1 non-repeating alarm and remove it, ensuring the
    // return value is always true irrespective of existence of alarms.
    base::test::ScopedFeatureList features;
    features.InitAndDisableFeature(
        extensions_features::kApiAlarmsClearAllReturnUndefined);
    CreateAlarm("[null, {\"delayInMinutes\": 10}]");
    // alarms.clearAll() always returns true matching legacy behavior.
    std::optional<base::Value> result = RunFunctionAndReturnValue(
        base::MakeRefCounted<AlarmsClearAllFunction>(), "[]");
    ASSERT_TRUE(result->is_bool());
    EXPECT_TRUE(result->GetBool());
  }

  // Create 3 alarms.
  CreateAlarms(3);
  alarm_manager_->GetAllAlarms(
      extension()->id(),
      base::BindOnce(ExtensionAlarmsTestClearAllGetAllAlarms1Callback, this));
}

class ExtensionAlarmsSchedulingTest : public ExtensionAlarmsTest {
  void GetAlarmCallback(Alarm* alarm) {
    CHECK(alarm);
    const base::Time scheduled_time =
        base::Time::FromMillisecondsSinceUnixEpoch(
            alarm->js_alarm->scheduled_time);
    EXPECT_EQ(scheduled_time, alarm_manager_->next_poll_time_);
  }

  static void RemoveAlarmCallback(bool found) { EXPECT_TRUE(found); }
  static void RemoveAllAlarmsCallback() {}

 public:
  // Get the time that the alarm named is scheduled to run.
  void VerifyScheduledTime(const std::string& alarm_name) {
    alarm_manager_->GetAlarm(
        extension()->id(), alarm_name,
        base::BindOnce(&ExtensionAlarmsSchedulingTest::GetAlarmCallback,
                       base::Unretained(this)));
  }

  void RemoveAlarm(const std::string& name) {
    alarm_manager_->RemoveAlarm(
        extension()->id(), name,
        base::BindOnce(&ExtensionAlarmsSchedulingTest::RemoveAlarmCallback));
  }

  void RemoveAllAlarms() {
    alarm_manager_->RemoveAllAlarms(
        extension()->id(),
        base::BindOnce(
            &ExtensionAlarmsSchedulingTest::RemoveAllAlarmsCallback));
  }
};

TEST_F(ExtensionAlarmsSchedulingTest, PollScheduling) {
  {
    CreateAlarm("[\"a\", {\"periodInMinutes\": 6}]");
    CreateAlarm("[\"bb\", {\"periodInMinutes\": 8}]");
    VerifyScheduledTime("a");
    RemoveAllAlarms();
  }
  {
    CreateAlarm("[\"a\", {\"delayInMinutes\": 10}]");
    CreateAlarm("[\"bb\", {\"delayInMinutes\": 21}]");
    VerifyScheduledTime("a");
    RemoveAllAlarms();
  }
  {
    test_clock_.SetNow(base::Time::FromSecondsSinceUnixEpoch(10));
    CreateAlarm("[\"a\", {\"periodInMinutes\": 10}]");
    Alarm alarm;
    alarm.js_alarm->name = "bb";
    alarm.js_alarm->scheduled_time = 30 * 60000;
    alarm.js_alarm->period_in_minutes = 30;
    alarm_manager_->AddAlarmImpl(extension()->id(), std::move(alarm));
    VerifyScheduledTime("a");
    RemoveAllAlarms();
  }
  {
    test_clock_.SetNow(base::Time::FromSecondsSinceUnixEpoch(3 * 60 + 1));
    Alarm alarm;
    alarm.js_alarm->name = "bb";
    alarm.js_alarm->scheduled_time = 3 * 60000;
    alarm.js_alarm->period_in_minutes = 3;
    alarm_manager_->AddAlarmImpl(extension()->id(), std::move(alarm));

    alarm_delegate_->WaitForAlarm();

    EXPECT_EQ(base::Time::FromSecondsSinceUnixEpoch(3 * 60) + base::Minutes(3),
              alarm_manager_->next_poll_time_);
    RemoveAllAlarms();
  }
  {
    test_clock_.SetNow(base::Time::FromSecondsSinceUnixEpoch(4 * 60 + 1));
    CreateAlarm("[\"a\", {\"periodInMinutes\": 2}]");
    RemoveAlarm("a");
    Alarm alarm2;
    alarm2.js_alarm->name = "bb";
    alarm2.js_alarm->scheduled_time = 4 * 60000;
    alarm2.js_alarm->period_in_minutes = 4;
    alarm_manager_->AddAlarmImpl(extension()->id(), std::move(alarm2));
    Alarm alarm3;
    alarm3.js_alarm->name = "ccc";
    alarm3.js_alarm->scheduled_time = 25 * 60000;
    alarm3.js_alarm->period_in_minutes = 25;
    alarm_manager_->AddAlarmImpl(extension()->id(), std::move(alarm3));
    alarm_delegate_->WaitForAlarm();
    EXPECT_EQ(base::Time::FromSecondsSinceUnixEpoch(4 * 60) + base::Minutes(4),
              alarm_manager_->next_poll_time_);
    RemoveAllAlarms();
  }
}

TEST_F(ExtensionAlarmsSchedulingTest, ReleasedExtensionPollsInfrequently) {
  set_extension(ExtensionBuilder("Test")
                    .SetLocation(mojom::ManifestLocation::kInternal)
                    .Build());
  test_clock_.SetNow(base::Time::FromSecondsSinceUnixEpoch(300));
  CreateAlarm("[\"a\", {\"when\": 300010}]");
  CreateAlarm("[\"b\", {\"when\": 300020}]");

  // On startup (when there's no "last poll"), we let alarms fire as
  // soon as they're scheduled.
  EXPECT_DOUBLE_EQ(
      300010, alarm_manager_->next_poll_time_.InMillisecondsFSinceUnixEpoch());

  alarm_manager_->last_poll_times_[extension()->id()] =
      base::Time::FromSecondsSinceUnixEpoch(300);
  // In packed extensions, we set the granularity to at least 1 second, which
  // makes AddAlarm schedule the next poll after the extension requested.
  alarm_manager_->ScheduleNextPoll();
  EXPECT_DOUBLE_EQ(
      (alarm_manager_->last_poll_times_[extension()->id()] + base::Seconds(1))
          .InMillisecondsFSinceUnixEpoch(),
      alarm_manager_->next_poll_time_.InMillisecondsFSinceUnixEpoch());
}

TEST_F(ExtensionAlarmsSchedulingTest, TimerRunning) {
  EXPECT_FALSE(alarm_manager_->timer_.IsRunning());
  CreateAlarm("[\"a\", {\"delayInMinutes\": 0.001}]");
  EXPECT_TRUE(alarm_manager_->timer_.IsRunning());
  test_clock_.Advance(base::Milliseconds(60));
  alarm_delegate_->WaitForAlarm();
  EXPECT_FALSE(alarm_manager_->timer_.IsRunning());
  CreateAlarm("[\"bb\", {\"delayInMinutes\": 10}]");
  EXPECT_TRUE(alarm_manager_->timer_.IsRunning());
  RemoveAllAlarms();
  EXPECT_FALSE(alarm_manager_->timer_.IsRunning());
}

TEST_F(ExtensionAlarmsSchedulingTest, MinimumGranularity) {
  set_extension(ExtensionBuilder("Test")
                    .SetLocation(mojom::ManifestLocation::kInternal)
                    .Build());
  test_clock_.SetNow(base::Time::UnixEpoch());
  CreateAlarm("[\"a\", {\"periodInMinutes\": 2}]");
  test_clock_.Advance(base::Milliseconds(500));
  CreateAlarm("[\"b\", {\"periodInMinutes\": 2}]");
  test_clock_.Advance(base::Minutes(2));

  alarm_manager_->last_poll_times_[extension()->id()] =
      base::Time::FromSecondsSinceUnixEpoch(2 * 60);
  // In packed extensions, we set the granularity to at least 1 second, which
  // makes the scheduler set it to 1 second, rather than 500 milliseconds later
  // (when b is supposed to go off).
  alarm_manager_->ScheduleNextPoll();
  EXPECT_DOUBLE_EQ(
      (alarm_manager_->last_poll_times_[extension()->id()] + base::Seconds(1))
          .InMillisecondsFSinceUnixEpoch(),
      alarm_manager_->next_poll_time_.InMillisecondsFSinceUnixEpoch());
}

void FrequencyTestGetAlarmsCallback(ExtensionAlarmsTest* test, Alarm* alarm) {
  ASSERT_TRUE(alarm);
  EXPECT_EQ("hello", alarm->js_alarm->name);
  EXPECT_DOUBLE_EQ(10000, alarm->js_alarm->scheduled_time);
  EXPECT_THAT(alarm->js_alarm->period_in_minutes, testing::Eq(0.0001));

  test->test_clock_.Advance(base::Milliseconds(10));
  // Now wait for the alarm to fire. Our test delegate will quit the
  // MessageLoop when that happens.
  test->alarm_delegate_->WaitForAlarm();
}

// Tests that alarms with very small period written to storage are also
// subjected to minimum polling interval.
// Regression test for https://crbug.com/40472348.
TEST_F(ExtensionAlarmsSchedulingTest, PollFrequencyFromStoredAlarm) {
  static constexpr struct {
    bool is_unpacked;
    int manifest_version;
    base::TimeDelta delay_minimum;
  } test_data[] = {
      {true, 2, alarms_api_constants::kDevDelayMinimum},
      {true, 3, alarms_api_constants::kDevDelayMinimum},
      {false, 2, alarms_api_constants::kMV2ReleaseDelayMinimum},
      {false, 3, alarms_api_constants::kMV3ReleaseDelayMinimum},
      {false, 4, alarms_api_constants::kMV3ReleaseDelayMinimum},
  };

  // Test once for unpacked and once for crx extension.
  for (const auto& entry : test_data) {
    test_clock_.SetNow(base::Time::FromSecondsSinceUnixEpoch(10));
    alarm_manager_->last_poll_times_.clear();

    // Mimic retrieving an alarm from StateStore.
    std::string alarm_args =
        "[{\"name\": \"hello\", \"scheduledTime\": 10000, "
        "\"periodInMinutes\": 0.0001, \"persistAcrossSessions\": true}]";
    base::TimeDelta min_delay = alarms_api_constants::GetMinimumDelay(
        entry.is_unpacked, entry.manifest_version);

    alarm_manager_->ReadFromStorage(extension()->id(), min_delay,
                                    base::test::ParseJson(alarm_args));

    // Let the alarm fire once, we will verify the next polling time afterwards.
    alarm_manager_->GetAlarm(
        extension()->id(), "hello",
        base::BindOnce(FrequencyTestGetAlarmsCallback, this));

    // The stored alarm's "periodInMinutes" is much smaller than allowed minimum
    // in this test (alarms_api_constants::kDevDelayMinimum or
    // alarms_api_constants::kReleaseDelayMinimum). Make sure
    // our next poll time corresponds to our allowed minimum and not to the
    // StateStore specified "periodInMinutes".
    base::Time expected_poll_time =
        // 10s initial clock.
        base::Time::FromSecondsSinceUnixEpoch(10) +
        // 10ms in FrequencyTestGetAlarmsCallback.
        base::Milliseconds(10) + entry.delay_minimum;
    // The alarm should not trigger before our expected poll time...
    EXPECT_GE(alarm_manager_->next_poll_time_, expected_poll_time);
    // And should trigger within a few seconds of it (to account for test
    // differences).
    EXPECT_LT(alarm_manager_->next_poll_time_,
              expected_poll_time + base::Seconds(10));
    RemoveAlarm("hello");
  }
}

void OldAlarmTestGetAlarmsCallback(ExtensionAlarmsTest* test, Alarm* alarm) {
  ASSERT_TRUE(alarm);
  EXPECT_EQ("hello", alarm->js_alarm->name);
  EXPECT_TRUE(alarm->js_alarm->persist_across_sessions);
}

// Tests that when reading an alarm from storage that was created before we had
// support for both persistent and non-persistent alarms, the alarm will be
// scheduled according to the old policy.
TEST_F(ExtensionAlarmsTest, OldPersistentAlarmFromStorage) {
  // Mimic retrieving an alarm from StateStore.
  std::string alarm_args = "[{\"name\": \"hello\", \"scheduledTime\": 10000}]";

  alarm_manager_->ReadFromStorage(extension()->id(),
                                  /*min_delay=*/base::Seconds(1),
                                  base::test::ParseJson(alarm_args));

  alarm_manager_->GetAlarm(extension()->id(), "hello",
                           base::BindOnce(OldAlarmTestGetAlarmsCallback, this));

  // This looks racy (we're removing the alarm that the callback is waiting to
  // fire on), but the callback is run synchronously, so this is fine.
  alarm_manager_->RemoveAlarm(extension()->id(), "hello", base::DoNothing());
}

// Test that scheduled alarms go off at set intervals, even if their actual
// trigger is off.
TEST_F(ExtensionAlarmsSchedulingTest, RepeatingAlarmsScheduledPredictably) {
  test_clock_.SetNow(base::Time::UnixEpoch());
  CreateAlarm("[\"a\", {\"periodInMinutes\": 2}]");

  alarm_manager_->last_poll_times_[extension()->id()] = base::Time::UnixEpoch();
  alarm_manager_->ScheduleNextPoll();

  // We expect the first poll to happen two minutes from the start.
  EXPECT_DOUBLE_EQ(
      (alarm_manager_->last_poll_times_[extension()->id()] + base::Seconds(120))
          .InMillisecondsFSinceUnixEpoch(),
      alarm_manager_->next_poll_time_.InMillisecondsFSinceUnixEpoch());

  // Poll more than two minutes later.
  test_clock_.Advance(base::Seconds(125));
  alarm_manager_->PollAlarms();

  // The alarm should have triggered once.
  EXPECT_EQ(1u, alarm_delegate_->alarms_seen.size());

  // The next poll should still be scheduled for four minutes from the start,
  // even though this is less than two minutes since the last alarm.
  // Last poll was at 125 seconds; next poll should be at 240 seconds.
  EXPECT_DOUBLE_EQ(
      (alarm_manager_->last_poll_times_[extension()->id()] + base::Seconds(115))
          .InMillisecondsFSinceUnixEpoch(),
      alarm_manager_->next_poll_time_.InMillisecondsFSinceUnixEpoch());

  // Completely miss a scheduled trigger.
  test_clock_.Advance(base::Seconds(255));  // Total Time: 380s
  alarm_manager_->PollAlarms();

  // The alarm should have triggered again at this last poll.
  EXPECT_EQ(2u, alarm_delegate_->alarms_seen.size());

  // The next poll should be the first poll that hasn't happened and is in-line
  // with the original scheduling.
  // Last poll was at 380 seconds; next poll should be at 480 seconds.
  EXPECT_DOUBLE_EQ(
      (alarm_manager_->last_poll_times_[extension()->id()] + base::Seconds(100))
          .InMillisecondsFSinceUnixEpoch(),
      alarm_manager_->next_poll_time_.InMillisecondsFSinceUnixEpoch());
}

TEST_F(ExtensionAlarmsSchedulingTest, PerExtensionLastPollTime) {
  test_clock_.SetNow(base::Time::UnixEpoch());

  // Extension 1 and Extension 2 are unpacked (minimum granularity = 1 second).
  scoped_refptr<const Extension> extension1(extension_ref());
  scoped_refptr<const Extension> extension2 =
      ExtensionBuilder("Test2").Build();

  // Create alarm for extension 1 scheduled at 10 seconds.
  set_extension(extension1);
  CreateAlarm("[\"ext1_alarm\", {\"when\": 10000}]");  // 10s.

  // Create alarm for extension 2 scheduled at 10.5 seconds (0.5s after extension 1).
  set_extension(extension2);
  CreateAlarm("[\"ext2_alarm\", {\"when\": 10500}]");  // 10.5s.

  // Set initial last poll time for both extensions.
  alarm_manager_->last_poll_times_[extension1->id()] = base::Time::UnixEpoch();
  alarm_manager_->last_poll_times_[extension2->id()] = base::Time::UnixEpoch();
  alarm_manager_->ScheduleNextPoll();

  // Extension 1's alarm at 10 seconds should run first.
  EXPECT_DOUBLE_EQ(
      (base::Time::UnixEpoch() + base::Seconds(10))
          .InMillisecondsFSinceUnixEpoch(),
      alarm_manager_->next_poll_time_.InMillisecondsFSinceUnixEpoch());

  // Advance time to 10 seconds and poll alarms.
  test_clock_.Advance(base::Seconds(10));
  alarm_manager_->PollAlarms();

  // Only extension 1's alarm should have fired.
  EXPECT_EQ(1u, alarm_delegate_->alarms_seen.size());
  EXPECT_EQ("ext1_alarm", alarm_delegate_->alarms_seen[0]);

  // Extension 1's last poll time should be 10 seconds.
  EXPECT_EQ(base::Time::UnixEpoch() + base::Seconds(10),
            alarm_manager_->last_poll_times_[extension1->id()]);

  // Extension 2's last poll time should NOT have been updated by Extension 1's
  // poll; it should remain UnixEpoch().
  EXPECT_EQ(base::Time::UnixEpoch(),
            alarm_manager_->last_poll_times_[extension2->id()]);

  // Next poll should be scheduled for Extension 2's alarm at 10.5 seconds.
  EXPECT_DOUBLE_EQ(
      (base::Time::UnixEpoch() + base::Milliseconds(10500))
          .InMillisecondsFSinceUnixEpoch(),
      alarm_manager_->next_poll_time_.InMillisecondsFSinceUnixEpoch());

  // Advance time by 0.5s to 10.5s (less than 1s after extension 1 fired).
  test_clock_.Advance(base::Milliseconds(500));
  alarm_manager_->PollAlarms();

  // Extension 2's alarm should fire immediately because its own last poll time
  // was at UnixEpoch(), even though extension 1 fired less than a second ago.
  EXPECT_EQ(2u, alarm_delegate_->alarms_seen.size());
  EXPECT_EQ("ext2_alarm", alarm_delegate_->alarms_seen[1]);
  EXPECT_EQ(base::Time::UnixEpoch() + base::Milliseconds(10500),
            alarm_manager_->last_poll_times_[extension2->id()]);
}

// Test that a poll writes each extension's alarms to storage once, no matter
// how many of its alarms fire.
TEST_F(ExtensionAlarmsSchedulingTest, PollWritesToStorageOncePerExtension) {
  test_clock_.SetNow(base::Time::UnixEpoch());

  scoped_refptr<const Extension> extension1(extension_ref());
  scoped_refptr<const Extension> extension2 = ExtensionBuilder("Test2").Build();
  scoped_refptr<const Extension> extension3 = ExtensionBuilder("Test3").Build();

  // Extension 1 has a repeating alarm, so its alarm list outlives the poll.
  set_extension(extension1);
  CreateAlarm("[\"a\", {\"when\": 10000}]");
  CreateAlarm("[\"b\", {\"when\": 10000, \"periodInMinutes\": 1}]");
  CreateAlarm("[\"c\", {\"when\": 10000}]");

  // Extension 2 has only one-shot alarms, so its alarm list is removed.
  set_extension(extension2);
  CreateAlarm("[\"d\", {\"when\": 10000}]");
  CreateAlarm("[\"e\", {\"when\": 10000}]");

  // Extension 3 has only an alarm that has not elapsed by the poll.
  set_extension(extension3);
  CreateAlarm("[\"f\", {\"when\": 25000}]");

  // Install the StateStore after the alarms are created, so that only the
  // writes made by the poll are counted.
  extension_system()->SetStateStore(std::make_unique<StateStore>(
      browser_context(),
      base::MakeRefCounted<value_store::TestValueStoreFactory>(),
      StateStore::BackendType::STATE, /*deferred_load=*/false));
  AlarmsStorageWriteCounter write_counter;
  base::ScopedObservation<StateStore, StateStore::TestObserver>
      write_observation(&write_counter);
  write_observation.Observe(extension_system()->state_store());

  test_clock_.Advance(base::Seconds(10));
  alarm_manager_->PollAlarms();

  EXPECT_EQ(5u, alarm_delegate_->alarms_seen.size());
  EXPECT_EQ(1, write_counter.write_counts[extension1->id()]);
  EXPECT_EQ(1, write_counter.write_counts[extension2->id()]);
  EXPECT_EQ(0, write_counter.write_counts[extension3->id()]);
}

TEST_F(ExtensionAlarmsSchedulingTest, ClearAll) {
  test_clock_.SetNow(base::Time::UnixEpoch());

  scoped_refptr<const Extension> extension1(extension_ref());
  scoped_refptr<const Extension> extension2 = ExtensionBuilder("Test2").Build();
  scoped_refptr<const Extension> extension3 = ExtensionBuilder("Test3").Build();

  // Extension 1 has one session and one persistent alarm.
  set_extension(extension1);
  CreateAlarm(
      "[\"session\", {\"when\": 10000, \"persistAcrossSessions\": false}]");
  CreateAlarm("[\"persistent\", {\"when\": 10000}]");

  // Extension 2 has only a session alarm.
  set_extension(extension2);
  CreateAlarm(
      "[\"session\", {\"when\": 10000, \"persistAcrossSessions\": false}]");

  // Extension 3 has no alarms.
  set_extension(extension3);

  // Install the StateStore after the alarms are created, so that only the
  // writes resulting from alarms.clearAll() calls are observed.
  extension_system()->SetStateStore(std::make_unique<StateStore>(
      browser_context(),
      base::MakeRefCounted<value_store::TestValueStoreFactory>(),
      StateStore::BackendType::STATE, /*deferred_load=*/false));
  AlarmsStorageWriteCounter write_counter;
  base::ScopedObservation<StateStore, StateStore::TestObserver>
      write_observation(&write_counter);
  write_observation.Observe(extension_system()->state_store());

  // After clearing alarms of extension 1, storage should be written and
  // timer still set.
  set_extension(extension1);
  ClearAllAlarms();
  EXPECT_EQ(1, write_counter.write_counts[extension1->id()]);
  EXPECT_EQ(0, write_counter.write_counts[extension2->id()]);
  EXPECT_EQ(0, write_counter.write_counts[extension3->id()]);
  EXPECT_EQ(base::Time::FromMillisecondsSinceUnixEpoch(10000),
            alarm_manager_->next_poll_time_);

  // After clearing of session alarm of extension 2, storage should not be
  // written and timer stopped.
  set_extension(extension2);
  ClearAllAlarms();
  EXPECT_EQ(1, write_counter.write_counts[extension1->id()]);
  EXPECT_EQ(0, write_counter.write_counts[extension2->id()]);
  EXPECT_EQ(0, write_counter.write_counts[extension3->id()]);
  EXPECT_EQ(base::Time(), alarm_manager_->next_poll_time_);

  // Since extension 3 has no alarms, alarms.clearAll() call is a no-op.
  set_extension(extension3);
  ClearAllAlarms();
  EXPECT_EQ(1, write_counter.write_counts[extension1->id()]);
  EXPECT_EQ(0, write_counter.write_counts[extension2->id()]);
  EXPECT_EQ(0, write_counter.write_counts[extension3->id()]);
  EXPECT_EQ(base::Time(), alarm_manager_->next_poll_time_);
}

TEST_F(ExtensionAlarmsSchedulingTest, RemoveAlarmStorageWrite) {
  // Start observing StateStore writes.
  extension_system()->SetStateStore(std::make_unique<StateStore>(
      browser_context(),
      base::MakeRefCounted<value_store::TestValueStoreFactory>(),
      StateStore::BackendType::STATE, /*deferred_load=*/false));
  AlarmsStorageWriteCounter write_counter;
  base::ScopedObservation<StateStore, StateStore::TestObserver>
      write_observation(&write_counter);
  write_observation.Observe(extension_system()->state_store());

  // Deletion of a non-existent alarm has no effect.
  {
    const bool found = ClearAlarm("nobody");
    EXPECT_FALSE(found);
    EXPECT_EQ(0, write_counter.write_counts[extension()->id()]);
  }

  // Deletion of a non-persistent alarm skips storage.
  {
    CreateAlarm(
        "[null, {\"name\": \"session\", \"periodInMinutes\": 10, "
        "\"persistAcrossSessions\": false}]");
    const bool found = ClearAlarm("session");
    EXPECT_TRUE(found);
    EXPECT_EQ(0, write_counter.write_counts[extension()->id()]);
  }

  // Deleting of a persistent alarm causes storage write.
  {
    CreateAlarm(
        "[null, {\"name\": \"persistent\", \"periodInMinutes\": 10, "
        "\"persistAcrossSessions\": true}]");
    EXPECT_EQ(1, write_counter.write_counts[extension()->id()]);
    const bool found = ClearAlarm("persistent");
    EXPECT_TRUE(found);
    EXPECT_EQ(2, write_counter.write_counts[extension()->id()]);
  }
}

class ExtensionAlarmsCreateStateStoreWriteEfficiency
    : public ExtensionAlarmsSchedulingTest,
      public testing::WithParamInterface<std::tuple<int64_t, bool, bool>> {
 protected:
  int64_t alarm_offset_in_hrs() const { return std::get<0>(GetParam()); }
  bool alarm1_is_persistent() const { return std::get<1>(GetParam()); }
  bool alarm2_is_persistent() const { return std::get<2>(GetParam()); }
};

// Repeated alarm creations with the same alarm or same time.
TEST_P(ExtensionAlarmsCreateStateStoreWriteEfficiency,
       CreateAlarmStateStoreWriteEfficiency) {
  // Install the StateStore observer.
  extension_system()->SetStateStore(std::make_unique<StateStore>(
      browser_context(),
      base::MakeRefCounted<value_store::TestValueStoreFactory>(),
      StateStore::BackendType::STATE, /*deferred_load=*/false));
  AlarmsStorageWriteCounter write_counter;
  base::ScopedObservation<StateStore, StateStore::TestObserver>
      write_observation(&write_counter);
  write_observation.Observe(extension_system()->state_store());

  constexpr int64_t millis_in_hour = 60 * 60 * 1000;
  const int64_t alarm1_time = 20 * millis_in_hour;
  const int64_t alarm2_time = (20 + alarm_offset_in_hrs()) * millis_in_hour;

  const int write_count_before = write_counter.write_counts[extension()->id()];
  // Write to StateStore after the first alarm if first alarm is persistent.
  const int write_count_after_first =
      alarm1_is_persistent() ? (write_count_before + 1) : write_count_before;
  // Write to StateStore after the second alarm if alarms are different and
  // at least one of them is persistent.
  const int write_count_after_second =
      (((alarm1_time != alarm2_time) ||
        (alarm1_is_persistent() != alarm2_is_persistent())) &&
       (alarm1_is_persistent() || alarm2_is_persistent()))
          ? (write_count_after_first + 1)
          : write_count_after_first;

  // Create first alarm.
  CreateAlarm(base::StringPrintf(
      "[null, {\"when\": %" PRId64 ", \"persistAcrossSessions\": %s}]",
      alarm1_time, alarm1_is_persistent() ? "true" : "false"));

  // StateStore is written iff first alarm is persistent.
  EXPECT_EQ(write_count_after_first,
            write_counter.write_counts[extension()->id()]);

  // Timer is set to alarm 1 time.
  EXPECT_TRUE(alarm_manager_->timer_.IsRunning());
  EXPECT_EQ(alarm_manager_->next_poll_time_,
            alarm_manager_->timer_.desired_run_time());
  EXPECT_EQ(base::Time::FromMillisecondsSinceUnixEpoch(alarm1_time),
            alarm_manager_->next_poll_time_);

  // Create second alarm (overwrite first alarm).
  CreateAlarm(base::StringPrintf(
      "[null, {\"when\": %" PRId64 ", \"persistAcrossSessions\": %s}]",
      alarm2_time, alarm2_is_persistent() ? "true" : "false"));

  // StateStore is written to iff alarms are not equal and at least one is
  // persistent.
  EXPECT_EQ(write_count_after_second,
            write_counter.write_counts[extension()->id()]);

  // Timer is set to alarm 2 time.
  EXPECT_TRUE(alarm_manager_->timer_.IsRunning());
  EXPECT_EQ(alarm_manager_->next_poll_time_,
            alarm_manager_->timer_.desired_run_time());
  EXPECT_EQ(base::Time::FromMillisecondsSinceUnixEpoch(alarm2_time),
            alarm_manager_->next_poll_time_);

  // Reset AlarmManager for the next test case.
  RunFunctionAndReturnValue(base::MakeRefCounted<AlarmsClearAllFunction>(),
                            "[]");
  EXPECT_FALSE(alarm_manager_->timer_.IsRunning());
}

// We have 12 = 3 * 2 * 2 possible test cases:
//  - Alarms can be at the same time (0), the first alarm can be earlier (1),
//    or the second alarm can be earlier (-1).
//  - First alarm can be persistent (true) or not persistent (false).
//  - Second alarm can be persistent (true) or not persistent (false).
INSTANTIATE_TEST_SUITE_P(All,
                         ExtensionAlarmsCreateStateStoreWriteEfficiency,
                         testing::Combine(testing::Values<int64_t>(0, 1, -1),
                                          testing::Bool(),
                                          testing::Bool()));

}  // namespace extensions
