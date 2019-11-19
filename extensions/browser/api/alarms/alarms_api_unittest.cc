// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file tests the chrome.alarms extension API.

#include <stddef.h>

#include "base/bind.h"
#include "base/json/json_reader.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/test/simple_test_clock.h"
#include "base/values.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/fake_local_frame.h"
#include "extensions/browser/api/alarms/alarm_manager.h"
#include "extensions/browser/api/alarms/alarms_api.h"
#include "extensions/browser/api/alarms/alarms_api_constants.h"
#include "extensions/browser/api_unittest.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_messages.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

typedef extensions::api::alarms::Alarm JsAlarm;

namespace extensions {

namespace {

// Test delegate which quits the message loop when an alarm fires.
class AlarmDelegate : public AlarmManager::Delegate {
 public:
  ~AlarmDelegate() override {}
  void OnAlarm(const std::string& extension_id, const Alarm& alarm) override {
    alarms_seen.push_back(alarm.js_alarm->name);
    if (base::RunLoop::IsRunningOnCurrentThread())
      base::RunLoop::QuitCurrentWhenIdleDeprecated();
  }

  std::vector<std::string> alarms_seen;
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

    alarm_delegate_ = new AlarmDelegate();
    alarm_manager_->set_delegate(alarm_delegate_);

    // Make sure there's a RenderViewHost for alarms to warn into.
    CreateBackgroundPage();

    test_clock_.SetNow(base::Time::FromDoubleT(10));
  }

  void CreateAlarm(const std::string& args) {
    RunFunction(new AlarmsCreateFunction(&test_clock_), args);
  }

  // Takes a JSON result from a function and converts it to a vector of
  // JsAlarms.
  std::vector<std::unique_ptr<JsAlarm>> ToAlarmList(base::ListValue* value) {
    std::vector<std::unique_ptr<JsAlarm>> list;
    for (size_t i = 0; i < value->GetSize(); ++i) {
      std::unique_ptr<JsAlarm> alarm(new JsAlarm());
      base::DictionaryValue* alarm_value;
      if (!value->GetDictionary(i, &alarm_value)) {
        ADD_FAILURE() << "Expected a list of Alarm objects.";
        return list;
      }
      EXPECT_TRUE(JsAlarm::Populate(*alarm_value, alarm.get()));
      list.push_back(std::move(alarm));
    }
    return list;
  }

  // Creates up to 3 alarms using the extension API.
  void CreateAlarms(size_t num_alarms) {
    CHECK_LE(num_alarms, 3U);

    const char* const kCreateArgs[] = {
        "[null, {\"periodInMinutes\": 0.001}]",
        "[\"7\", {\"periodInMinutes\": 7}]",
        "[\"0\", {\"delayInMinutes\": 0}]",
    };
    for (size_t i = 0; i < num_alarms; ++i) {
      std::unique_ptr<base::DictionaryValue> result(
          RunFunctionAndReturnDictionary(new AlarmsCreateFunction(&test_clock_),
                                         kCreateArgs[i]));
      EXPECT_FALSE(result.get());
    }
  }

  base::SimpleTestClock test_clock_;
  AlarmManager* alarm_manager_;
  AlarmDelegate* alarm_delegate_;
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
  EXPECT_FALSE(alarm->js_alarm->period_in_minutes.get());

  // Now wait for the alarm to fire. Our test delegate will quit the
  // MessageLoop when that happens.
  base::RunLoop().Run();

  ASSERT_EQ(1u, test->alarm_delegate_->alarms_seen.size());
  EXPECT_EQ("", test->alarm_delegate_->alarms_seen[0]);

  // Ensure the alarm is gone.
  test->alarm_manager_->GetAllAlarms(
      test->extension()->id(),
      base::Bind(ExtensionAlarmsTestGetAllAlarmsCallback));
}

TEST_F(ExtensionAlarmsTest, Create) {
  test_clock_.SetNow(base::Time::FromDoubleT(10));
  // Create 1 non-repeating alarm.
  CreateAlarm("[null, {\"delayInMinutes\": 0}]");

  alarm_manager_->GetAlarm(
      extension()->id(), std::string(),
      base::Bind(ExtensionAlarmsTestGetAlarmCallback, this));
}

void ExtensionAlarmsTestCreateRepeatingGetAlarmCallback(
    ExtensionAlarmsTest* test,
    Alarm* alarm) {
  ASSERT_TRUE(alarm);
  EXPECT_EQ("", alarm->js_alarm->name);
  EXPECT_DOUBLE_EQ(10060, alarm->js_alarm->scheduled_time);
  EXPECT_THAT(alarm->js_alarm->period_in_minutes,
              testing::Pointee(testing::DoubleEq(0.001)));

  test->test_clock_.Advance(base::TimeDelta::FromSeconds(1));
  // Now wait for the alarm to fire. Our test delegate will quit the
  // MessageLoop when that happens.
  base::RunLoop().Run();

  test->test_clock_.Advance(base::TimeDelta::FromSeconds(1));
  // Wait again, and ensure the alarm fires again.
  RunScheduleNextPoll(test->alarm_manager_);
  base::RunLoop().Run();

  ASSERT_EQ(2u, test->alarm_delegate_->alarms_seen.size());
  EXPECT_EQ("", test->alarm_delegate_->alarms_seen[0]);
}

TEST_F(ExtensionAlarmsTest, CreateRepeating) {
  test_clock_.SetNow(base::Time::FromDoubleT(10));

  // Create 1 repeating alarm.
  CreateAlarm("[null, {\"periodInMinutes\": 0.001}]");

  alarm_manager_->GetAlarm(
      extension()->id(), std::string(),
      base::Bind(ExtensionAlarmsTestCreateRepeatingGetAlarmCallback, this));
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
  EXPECT_THAT(alarm->js_alarm->period_in_minutes, testing::IsNull());

  test->test_clock_.SetNow(base::Time::FromDoubleT(10.1));
  // Now wait for the alarm to fire. Our test delegate will quit the
  // MessageLoop when that happens.
  base::RunLoop().Run();

  test->alarm_manager_->GetAlarm(
      test->extension()->id(), std::string(),
      base::Bind(ExtensionAlarmsTestCreateAbsoluteGetAlarm2Callback, test));
}

TEST_F(ExtensionAlarmsTest, CreateAbsolute) {
  test_clock_.SetNow(base::Time::FromDoubleT(9.99));
  CreateAlarm("[null, {\"when\": 10001}]");

  alarm_manager_->GetAlarm(
      extension()->id(), std::string(),
      base::Bind(ExtensionAlarmsTestCreateAbsoluteGetAlarm1Callback, this));
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

  test->test_clock_.SetNow(base::Time::FromDoubleT(10.7));
  base::RunLoop().Run();

  test->alarm_manager_->GetAlarm(
      test->extension()->id(), std::string(),
      base::Bind(
          ExtensionAlarmsTestCreateRepeatingWithQuickFirstCallGetAlarm3Callback,
          test));
}

void ExtensionAlarmsTestCreateRepeatingWithQuickFirstCallGetAlarm1Callback(
    ExtensionAlarmsTest* test,
    Alarm* alarm) {
  ASSERT_TRUE(alarm);
  EXPECT_EQ("", alarm->js_alarm->name);
  EXPECT_DOUBLE_EQ(10001, alarm->js_alarm->scheduled_time);
  EXPECT_THAT(alarm->js_alarm->period_in_minutes,
              testing::Pointee(testing::DoubleEq(0.001)));

  test->test_clock_.SetNow(base::Time::FromDoubleT(10.1));
  // Now wait for the alarm to fire. Our test delegate will quit the
  // MessageLoop when that happens.
  base::RunLoop().Run();

  test->alarm_manager_->GetAlarm(
      test->extension()->id(), std::string(),
      base::Bind(
          ExtensionAlarmsTestCreateRepeatingWithQuickFirstCallGetAlarm2Callback,
          test));
}

TEST_F(ExtensionAlarmsTest, CreateRepeatingWithQuickFirstCall) {
  test_clock_.SetNow(base::Time::FromDoubleT(9.99));
  CreateAlarm("[null, {\"when\": 10001, \"periodInMinutes\": 0.001}]");

  alarm_manager_->GetAlarm(
      extension()->id(), std::string(),
      base::Bind(
          ExtensionAlarmsTestCreateRepeatingWithQuickFirstCallGetAlarm1Callback,
          this));
}

void ExtensionAlarmsTestCreateDupeGetAllAlarmsCallback(
    const AlarmManager::AlarmList* alarms) {
  ASSERT_TRUE(alarms);
  EXPECT_EQ(1u, alarms->size());
  EXPECT_DOUBLE_EQ(430000, (*alarms)[0]->js_alarm->scheduled_time);
}

TEST_F(ExtensionAlarmsTest, CreateDupe) {
  test_clock_.SetNow(base::Time::FromDoubleT(10));

  // Create 2 duplicate alarms. The first should be overridden.
  CreateAlarm("[\"dup\", {\"delayInMinutes\": 1}]");
  CreateAlarm("[\"dup\", {\"delayInMinutes\": 7}]");

  alarm_manager_->GetAllAlarms(
      extension()->id(),
      base::Bind(ExtensionAlarmsTestCreateDupeGetAllAlarmsCallback));
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
  base::Optional<blink::mojom::ConsoleMessageLevel> last_level_;
  std::string last_message_;
};

TEST_F(ExtensionAlarmsTest, CreateDelayBelowMinimum) {
  // Create an alarm with delay below the minimum accepted value.
  ConsoleLogMessageLocalFrame local_frame;
  local_frame.Init(contents()->GetMainFrame()->GetRemoteAssociatedInterfaces());
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(local_frame.message_count(), 0u);
  CreateAlarm("[\"negative\", {\"delayInMinutes\": -0.2}]");
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(local_frame.message_count(), 1u);

  EXPECT_EQ(blink::mojom::ConsoleMessageLevel::kWarning,
            local_frame.last_level());
  EXPECT_THAT(local_frame.last_message(),
              testing::HasSubstr("delay is less than minimum of 1"));
}

TEST_F(ExtensionAlarmsTest, Get) {
  test_clock_.SetNow(base::Time::FromDoubleT(4));

  // Create 2 alarms, and make sure we can query them.
  CreateAlarms(2);

  // Get the default one.
  {
    JsAlarm alarm;
    std::unique_ptr<base::DictionaryValue> result(
        RunFunctionAndReturnDictionary(new AlarmsGetFunction(), "[null]"));
    ASSERT_TRUE(result.get());
    EXPECT_TRUE(JsAlarm::Populate(*result, &alarm));
    EXPECT_EQ("", alarm.name);
    EXPECT_DOUBLE_EQ(4060, alarm.scheduled_time);
    EXPECT_THAT(alarm.period_in_minutes,
                testing::Pointee(testing::DoubleEq(0.001)));
  }

  // Get "7".
  {
    JsAlarm alarm;
    std::unique_ptr<base::DictionaryValue> result(
        RunFunctionAndReturnDictionary(new AlarmsGetFunction(), "[\"7\"]"));
    ASSERT_TRUE(result.get());
    EXPECT_TRUE(JsAlarm::Populate(*result, &alarm));
    EXPECT_EQ("7", alarm.name);
    EXPECT_EQ(424000, alarm.scheduled_time);
    EXPECT_THAT(alarm.period_in_minutes, testing::Pointee(7));
  }

  // Get a non-existent one.
  {
    std::unique_ptr<base::DictionaryValue> result(
        RunFunctionAndReturnDictionary(new AlarmsGetFunction(),
                                       "[\"nobody\"]"));
    ASSERT_FALSE(result.get());
  }
}

TEST_F(ExtensionAlarmsTest, GetAll) {
  // Test getAll with 0 alarms.
  {
    std::unique_ptr<base::ListValue> result(
        RunFunctionAndReturnList(new AlarmsGetAllFunction(), "[]"));
    std::vector<std::unique_ptr<JsAlarm>> alarms = ToAlarmList(result.get());
    EXPECT_EQ(0u, alarms.size());
  }

  // Create 2 alarms, and make sure we can query them.
  CreateAlarms(2);

  {
    std::unique_ptr<base::ListValue> result(
        RunFunctionAndReturnList(new AlarmsGetAllFunction(), "[null]"));
    std::vector<std::unique_ptr<JsAlarm>> alarms = ToAlarmList(result.get());
    EXPECT_EQ(2u, alarms.size());

    // Test the "7" alarm.
    JsAlarm* alarm = alarms[0].get();
    if (alarm->name != "7")
      alarm = alarms[1].get();
    EXPECT_EQ("7", alarm->name);
    EXPECT_THAT(alarm->period_in_minutes, testing::Pointee(7));
  }
}

void ExtensionAlarmsTestClearGetAllAlarms2Callback(
    const AlarmManager::AlarmList* alarms) {
  // Ensure the 0.001-minute alarm is still there, since it's repeating.
  ASSERT_TRUE(alarms);
  EXPECT_EQ(1u, alarms->size());
  EXPECT_THAT((*alarms)[0]->js_alarm->period_in_minutes,
              testing::Pointee(0.001));
}

void ExtensionAlarmsTestClearGetAllAlarms1Callback(
    ExtensionAlarmsTest* test,
    const AlarmManager::AlarmList* alarms) {
  ASSERT_TRUE(alarms);
  EXPECT_EQ(1u, alarms->size());
  EXPECT_THAT((*alarms)[0]->js_alarm->period_in_minutes,
              testing::Pointee(0.001));

  // Now wait for the alarms to fire, and ensure the cancelled alarms don't
  // fire.
  test->test_clock_.Advance(base::TimeDelta::FromMilliseconds(60));
  RunScheduleNextPoll(test->alarm_manager_);
  base::RunLoop().Run();

  ASSERT_EQ(1u, test->alarm_delegate_->alarms_seen.size());
  EXPECT_EQ("", test->alarm_delegate_->alarms_seen[0]);

  // Ensure the 0.001-minute alarm is still there, since it's repeating.
  test->alarm_manager_->GetAllAlarms(
      test->extension()->id(),
      base::Bind(ExtensionAlarmsTestClearGetAllAlarms2Callback));
}

TEST_F(ExtensionAlarmsTest, Clear) {
  // Clear a non-existent one.
  {
    std::unique_ptr<base::Value> result(
        RunFunctionAndReturnValue(new AlarmsClearFunction(), "[\"nobody\"]"));
    bool copy_bool_result = false;
    ASSERT_TRUE(result->GetAsBoolean(&copy_bool_result));
    EXPECT_FALSE(copy_bool_result);
  }

  // Create 3 alarms.
  CreateAlarms(3);

  // Clear all but the 0.001-minute alarm.
  {
    std::unique_ptr<base::Value> result(
        RunFunctionAndReturnValue(new AlarmsClearFunction(), "[\"7\"]"));
    bool copy_bool_result = false;
    ASSERT_TRUE(result->GetAsBoolean(&copy_bool_result));
    EXPECT_TRUE(copy_bool_result);
  }
  {
    std::unique_ptr<base::Value> result(
        RunFunctionAndReturnValue(new AlarmsClearFunction(), "[\"0\"]"));
    bool copy_bool_result = false;
    ASSERT_TRUE(result->GetAsBoolean(&copy_bool_result));
    EXPECT_TRUE(copy_bool_result);
  }

  alarm_manager_->GetAllAlarms(
      extension()->id(),
      base::Bind(ExtensionAlarmsTestClearGetAllAlarms1Callback, this));
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
  test->RunFunction(new AlarmsClearAllFunction(), "[]");
  test->alarm_manager_->GetAllAlarms(
      test->extension()->id(),
      base::Bind(ExtensionAlarmsTestClearAllGetAllAlarms2Callback));
}

TEST_F(ExtensionAlarmsTest, ClearAll) {
  // ClearAll with no alarms set.
  {
    std::unique_ptr<base::Value> result(
        RunFunctionAndReturnValue(new AlarmsClearAllFunction(), "[]"));
    bool copy_bool_result = false;
    ASSERT_TRUE(result->GetAsBoolean(&copy_bool_result));
    EXPECT_TRUE(copy_bool_result);
  }

  // Create 3 alarms.
  CreateAlarms(3);
  alarm_manager_->GetAllAlarms(
      extension()->id(),
      base::Bind(ExtensionAlarmsTestClearAllGetAllAlarms1Callback, this));
}

class ExtensionAlarmsSchedulingTest : public ExtensionAlarmsTest {
  void GetAlarmCallback(Alarm* alarm) {
    CHECK(alarm);
    const base::Time scheduled_time =
        base::Time::FromJsTime(alarm->js_alarm->scheduled_time);
    EXPECT_EQ(scheduled_time, alarm_manager_->next_poll_time_);
  }

  static void RemoveAlarmCallback(bool success) { EXPECT_TRUE(success); }
  static void RemoveAllAlarmsCallback() {}

 public:
  // Get the time that the alarm named is scheduled to run.
  void VerifyScheduledTime(const std::string& alarm_name) {
    alarm_manager_->GetAlarm(
        extension()->id(), alarm_name,
        base::Bind(&ExtensionAlarmsSchedulingTest::GetAlarmCallback,
                   base::Unretained(this)));
  }

  void RemoveAlarm(const std::string& name) {
    alarm_manager_->RemoveAlarm(
        extension()->id(), name,
        base::Bind(&ExtensionAlarmsSchedulingTest::RemoveAlarmCallback));
  }

  void RemoveAllAlarms() {
    alarm_manager_->RemoveAllAlarms(
        extension()->id(),
        base::Bind(&ExtensionAlarmsSchedulingTest::RemoveAllAlarmsCallback));
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
    test_clock_.SetNow(base::Time::FromDoubleT(10));
    CreateAlarm("[\"a\", {\"periodInMinutes\": 10}]");
    std::unique_ptr<Alarm> alarm(new Alarm);
    alarm->js_alarm->name = "bb";
    alarm->js_alarm->scheduled_time = 30 * 60000;
    alarm->js_alarm->period_in_minutes.reset(new double(30));
    alarm_manager_->AddAlarmImpl(extension()->id(), std::move(alarm));
    VerifyScheduledTime("a");
    RemoveAllAlarms();
  }
  {
    test_clock_.SetNow(base::Time::FromDoubleT(3 * 60 + 1));
    std::unique_ptr<Alarm> alarm(new Alarm);
    alarm->js_alarm->name = "bb";
    alarm->js_alarm->scheduled_time = 3 * 60000;
    alarm->js_alarm->period_in_minutes.reset(new double(3));
    alarm_manager_->AddAlarmImpl(extension()->id(), std::move(alarm));
    base::RunLoop().Run();
    EXPECT_EQ(
        base::Time::FromJsTime(3 * 60000) + base::TimeDelta::FromMinutes(3),
        alarm_manager_->next_poll_time_);
    RemoveAllAlarms();
  }
  {
    test_clock_.SetNow(base::Time::FromDoubleT(4 * 60 + 1));
    CreateAlarm("[\"a\", {\"periodInMinutes\": 2}]");
    RemoveAlarm("a");
    std::unique_ptr<Alarm> alarm2(new Alarm);
    alarm2->js_alarm->name = "bb";
    alarm2->js_alarm->scheduled_time = 4 * 60000;
    alarm2->js_alarm->period_in_minutes.reset(new double(4));
    alarm_manager_->AddAlarmImpl(extension()->id(), std::move(alarm2));
    std::unique_ptr<Alarm> alarm3(new Alarm);
    alarm3->js_alarm->name = "ccc";
    alarm3->js_alarm->scheduled_time = 25 * 60000;
    alarm3->js_alarm->period_in_minutes.reset(new double(25));
    alarm_manager_->AddAlarmImpl(extension()->id(), std::move(alarm3));
    base::RunLoop().Run();
    EXPECT_EQ(
        base::Time::FromJsTime(4 * 60000) + base::TimeDelta::FromMinutes(4),
        alarm_manager_->next_poll_time_);
    RemoveAllAlarms();
  }
}

TEST_F(ExtensionAlarmsSchedulingTest, ReleasedExtensionPollsInfrequently) {
  set_extension(
      ExtensionBuilder("Test").SetLocation(Manifest::INTERNAL).Build());
  test_clock_.SetNow(base::Time::FromJsTime(300000));
  CreateAlarm("[\"a\", {\"when\": 300010}]");
  CreateAlarm("[\"b\", {\"when\": 340000}]");

  // On startup (when there's no "last poll"), we let alarms fire as
  // soon as they're scheduled.
  EXPECT_DOUBLE_EQ(300010, alarm_manager_->next_poll_time_.ToJsTime());

  alarm_manager_->last_poll_time_ = base::Time::FromJsTime(290000);
  // In released extensions, we set the granularity to at least 1
  // minute, which makes AddAlarm schedule the next poll after the
  // extension requested.
  alarm_manager_->ScheduleNextPoll();
  EXPECT_DOUBLE_EQ((alarm_manager_->last_poll_time_ +
                    base::TimeDelta::FromMinutes(1)).ToJsTime(),
                   alarm_manager_->next_poll_time_.ToJsTime());
}

TEST_F(ExtensionAlarmsSchedulingTest, TimerRunning) {
  EXPECT_FALSE(alarm_manager_->timer_.IsRunning());
  CreateAlarm("[\"a\", {\"delayInMinutes\": 0.001}]");
  EXPECT_TRUE(alarm_manager_->timer_.IsRunning());
  test_clock_.Advance(base::TimeDelta::FromMilliseconds(60));
  base::RunLoop().Run();
  EXPECT_FALSE(alarm_manager_->timer_.IsRunning());
  CreateAlarm("[\"bb\", {\"delayInMinutes\": 10}]");
  EXPECT_TRUE(alarm_manager_->timer_.IsRunning());
  RemoveAllAlarms();
  EXPECT_FALSE(alarm_manager_->timer_.IsRunning());
}

TEST_F(ExtensionAlarmsSchedulingTest, MinimumGranularity) {
  set_extension(
      ExtensionBuilder("Test").SetLocation(Manifest::INTERNAL).Build());
  test_clock_.SetNow(base::Time::FromJsTime(0));
  CreateAlarm("[\"a\", {\"periodInMinutes\": 2}]");
  test_clock_.Advance(base::TimeDelta::FromSeconds(1));
  CreateAlarm("[\"b\", {\"periodInMinutes\": 2}]");
  test_clock_.Advance(base::TimeDelta::FromMinutes(2));

  alarm_manager_->last_poll_time_ = base::Time::FromJsTime(2 * 60000);
  // In released extensions, we set the granularity to at least 1
  // minute, which makes scheduler set it to 1 minute, rather than
  // 1 second later (when b is supposed to go off).
  alarm_manager_->ScheduleNextPoll();
  EXPECT_DOUBLE_EQ((alarm_manager_->last_poll_time_ +
                    base::TimeDelta::FromMinutes(1)).ToJsTime(),
                   alarm_manager_->next_poll_time_.ToJsTime());
}

TEST_F(ExtensionAlarmsSchedulingTest, DifferentMinimumGranularities) {
  test_clock_.SetNow(base::Time::FromJsTime(0));
  // Create an alarm to go off in 12 seconds. This uses the default, unpacked
  // extension - so there is no minimum granularity.
  CreateAlarm("[\"a\", {\"periodInMinutes\": 0.2}]");  // 12 seconds.

  // Create a new extension, which is packed, and has a granularity of 1 minute.
  // CreateAlarm() uses extension_, so keep a ref of the old one around, and
  // repopulate extension_.
  scoped_refptr<const Extension> extension2(extension_ref());
  set_extension(
      ExtensionBuilder("Test").SetLocation(Manifest::INTERNAL).Build());

  CreateAlarm("[\"b\", {\"periodInMinutes\": 2}]");

  alarm_manager_->last_poll_time_ = base::Time::FromJsTime(0);
  alarm_manager_->ScheduleNextPoll();

  // The next poll time should be 12 seconds from now - the time at which the
  // first alarm should go off.
  EXPECT_DOUBLE_EQ((alarm_manager_->last_poll_time_ +
                    base::TimeDelta::FromSeconds(12)).ToJsTime(),
                   alarm_manager_->next_poll_time_.ToJsTime());
}

void FrequencyTestGetAlarmsCallback(ExtensionAlarmsTest* test, Alarm* alarm) {
  ASSERT_TRUE(alarm);
  EXPECT_EQ("hello", alarm->js_alarm->name);
  EXPECT_DOUBLE_EQ(10000, alarm->js_alarm->scheduled_time);
  EXPECT_THAT(alarm->js_alarm->period_in_minutes,
              testing::Pointee(testing::DoubleEq(0.0001)));

  test->test_clock_.Advance(base::TimeDelta::FromMilliseconds(10));
  // Now wait for the alarm to fire. Our test delegate will quit the
  // MessageLoop when that happens.
  base::RunLoop().Run();
}

// Tests that alarms with very small period written to storage are also
// subjected to minimum polling interval.
// Regression test for https://crbug.com/618540.
TEST_F(ExtensionAlarmsSchedulingTest, PollFrequencyFromStoredAlarm) {
  struct {
    bool is_unpacked;
    double delay_minimum;
  } test_data[] = {
      {true, alarms_api_constants::kDevDelayMinimum},
      {false, alarms_api_constants::kReleaseDelayMinimum},
  };

  // Test once for unpacked and once for crx extension.
  for (size_t i = 0; i < base::size(test_data); ++i) {
    test_clock_.SetNow(base::Time::FromDoubleT(10));

    // Mimic retrieving an alarm from StateStore.
    std::string alarm_args =
        "[{\"name\": \"hello\", \"scheduledTime\": 10000, "
        "\"periodInMinutes\": 0.0001}]";
    std::unique_ptr<base::ListValue> value =
        base::ListValue::From(base::JSONReader::ReadDeprecated(alarm_args));
    alarm_manager_->ReadFromStorage(extension()->id(), test_data[i].is_unpacked,
                                    std::move(value));

    // Let the alarm fire once, we will verify the next polling time afterwards.
    alarm_manager_->GetAlarm(extension()->id(), "hello",
                             base::Bind(FrequencyTestGetAlarmsCallback, this));

    // The stored alarm's "periodInMinutes" is much smaller than allowed minimum
    // in this test (alarms_api_constants::kDevDelayMinimum or
    // alarms_api_constants::kReleaseDelayMinimum). Make sure
    // our next poll time corresponds to our allowed minimum and not to the
    // StateStore specified "periodInMinutes".
    EXPECT_GE(
        alarm_manager_->next_poll_time_,
        // 10s initial clock.
        base::Time::FromJsTime(10000) +
            // 10ms in FrequencyTestGetAlarmsCallback.
            base::TimeDelta::FromMilliseconds(10) +
            base::TimeDelta::FromSecondsD(test_data[i].delay_minimum * 60));
    RemoveAlarm("hello");
  }
}

// Test that scheduled alarms go off at set intervals, even if their actual
// trigger is off.
TEST_F(ExtensionAlarmsSchedulingTest, RepeatingAlarmsScheduledPredictably) {
  test_clock_.SetNow(base::Time::FromJsTime(0));
  CreateAlarm("[\"a\", {\"periodInMinutes\": 2}]");

  alarm_manager_->last_poll_time_ = base::Time::FromJsTime(0);
  alarm_manager_->ScheduleNextPoll();

  // We expect the first poll to happen two minutes from the start.
  EXPECT_DOUBLE_EQ((alarm_manager_->last_poll_time_ +
                    base::TimeDelta::FromSeconds(120)).ToJsTime(),
                   alarm_manager_->next_poll_time_.ToJsTime());

  // Poll more than two minutes later.
  test_clock_.Advance(base::TimeDelta::FromSeconds(125));
  alarm_manager_->PollAlarms();

  // The alarm should have triggered once.
  EXPECT_EQ(1u, alarm_delegate_->alarms_seen.size());

  // The next poll should still be scheduled for four minutes from the start,
  // even though this is less than two minutes since the last alarm.
  // Last poll was at 125 seconds; next poll should be at 240 seconds.
  EXPECT_DOUBLE_EQ((alarm_manager_->last_poll_time_ +
                    base::TimeDelta::FromSeconds(115)).ToJsTime(),
                   alarm_manager_->next_poll_time_.ToJsTime());

  // Completely miss a scheduled trigger.
  test_clock_.Advance(base::TimeDelta::FromSeconds(255));  // Total Time: 380s
  alarm_manager_->PollAlarms();

  // The alarm should have triggered again at this last poll.
  EXPECT_EQ(2u, alarm_delegate_->alarms_seen.size());

  // The next poll should be the first poll that hasn't happened and is in-line
  // with the original scheduling.
  // Last poll was at 380 seconds; next poll should be at 480 seconds.
  EXPECT_DOUBLE_EQ((alarm_manager_->last_poll_time_ +
                    base::TimeDelta::FromSeconds(100)).ToJsTime(),
                   alarm_manager_->next_poll_time_.ToJsTime());
}

}  // namespace extensions
