// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/notifications/notification_data.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/notifications/notification_constants.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_unsignedlong_unsignedlongsequence.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_notification_action.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_notification_options.h"
#include "third_party/blink/renderer/modules/notifications/notification.h"
#include "third_party/blink/renderer/modules/notifications/timestamp_trigger.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/testing/exception_state_matchers.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {
namespace {

const char kNotificationBaseUrl[] = "https://example.com/directory/";
const char kNotificationTitle[] = "My Notification";

const char kNotificationDir[] = "rtl";
const char kNotificationLang[] = "nl";
const char kNotificationBody[] = "Hello, world";
const char kNotificationTag[] = "my_tag";
const char kNotificationEmptyTag[] = "";
const char kNotificationImage[] = "https://example.com/image.jpg";
const char kNotificationIcon[] = "/icon.png";
const char kNotificationIconInvalid[] = "https://invalid:icon:url";
const char kNotificationBadge[] = "badge.png";
const std::array<unsigned, 5> kNotificationVibration = {42, 10, 20, 30, 40};
const uint64_t kNotificationTimestamp = 621046800ull;
const bool kNotificationRenotify = true;
const bool kNotificationSilent = false;
const bool kNotificationRequireInteraction = true;

const mojom::blink::NotificationActionType kBlinkNotificationActionType =
    mojom::blink::NotificationActionType::TEXT;
const char kNotificationActionType[] = "text";
const char kNotificationActionAction[] = "my_action";
const char kNotificationActionTitle[] = "My Action";
const char kNotificationActionIcon[] = "https://example.com/action_icon.png";
const char kNotificationActionPlaceholder[] = "Placeholder...";

const std::array<unsigned, 4> kNotificationVibrationUnnormalized = {10, 1000000,
                                                                    50, 42};
const std::array<int, 3> kNotificationVibrationNormalized = {10, 10000, 50};

TEST(NotificationDataTest, ReflectProperties) {
  test::TaskEnvironment task_environment;
  const KURL base_url(kNotificationBaseUrl);
  V8TestingScope scope(base_url);

  Vector<unsigned> vibration_pattern(kNotificationVibration);

  auto* vibration_sequence =
      MakeGarbageCollected<V8UnionUnsignedLongOrUnsignedLongSequence>(
          vibration_pattern);

  HeapVector<Member<NotificationAction>> actions;
  for (size_t i = 0; i < Notification::maxActions(); ++i) {
    NotificationAction* action = NotificationAction::Create(scope.GetIsolate());
    action->setType(kNotificationActionType);
    action->setAction(kNotificationActionAction);
    action->setTitle(kNotificationActionTitle);
    action->setIcon(kNotificationActionIcon);
    action->setPlaceholder(kNotificationActionPlaceholder);

    actions.push_back(action);
  }

  const DOMTimeStamp show_timestamp =
      base::Time::Now().InMillisecondsSinceUnixEpoch();
  TimestampTrigger* showTrigger = TimestampTrigger::Create(show_timestamp);

  NotificationOptions* options =
      NotificationOptions::Create(scope.GetIsolate());
  options->setDir(kNotificationDir);
  options->setLang(kNotificationLang);
  options->setBody(kNotificationBody);
  options->setTag(kNotificationTag);
  options->setImage(kNotificationImage);
  options->setIcon(kNotificationIcon);
  options->setBadge(kNotificationBadge);
  options->setVibrate(vibration_sequence);
  options->setTimestamp(kNotificationTimestamp);
  options->setRenotify(kNotificationRenotify);
  options->setSilent(kNotificationSilent);
  options->setRequireInteraction(kNotificationRequireInteraction);
  options->setActions(actions);
  options->setShowTrigger(showTrigger);

  // TODO(peter): Test |options.data| and |notificationData.data|.

  ExceptionState& exception_state = scope.GetExceptionState();
  mojom::blink::NotificationDataPtr notification_data =
      CreateNotificationData(scope.GetExecutionContext(), kNotificationTitle,
                             options, exception_state);
  ASSERT_THAT(exception_state, HadNoException());

  EXPECT_EQ(kNotificationTitle, notification_data->title);

  EXPECT_EQ(mojom::blink::NotificationDirection::RIGHT_TO_LEFT,
            notification_data->direction);
  EXPECT_EQ(kNotificationLang, notification_data->lang);
  EXPECT_EQ(kNotificationBody, notification_data->body);
  EXPECT_EQ(kNotificationTag, notification_data->tag);
  EXPECT_EQ(base::Time::FromMillisecondsSinceUnixEpoch(
                static_cast<int64_t>(show_timestamp)),
            notification_data->show_trigger_timestamp);

  // URLs should be resolved against the base URL of the execution context.
  EXPECT_EQ(KURL(base_url, kNotificationImage), notification_data->image);
  EXPECT_EQ(KURL(base_url, kNotificationIcon), notification_data->icon);
  EXPECT_EQ(KURL(base_url, kNotificationBadge), notification_data->badge);

  ASSERT_EQ(vibration_pattern.size(),
            notification_data->vibration_pattern->size());
  for (wtf_size_t i = 0; i < vibration_pattern.size(); ++i) {
    EXPECT_EQ(
        vibration_pattern[i],
        static_cast<unsigned>(notification_data->vibration_pattern.value()[i]));
  }

  EXPECT_EQ(kNotificationTimestamp, notification_data->timestamp);
  EXPECT_EQ(kNotificationRenotify, notification_data->renotify);
  EXPECT_EQ(kNotificationSilent, notification_data->silent);
  EXPECT_EQ(kNotificationRequireInteraction,
            notification_data->require_interaction);
  EXPECT_EQ(actions.size(), notification_data->actions->size());
  for (const auto& action : notification_data->actions.value()) {
    EXPECT_EQ(kBlinkNotificationActionType, action->type);
    EXPECT_EQ(kNotificationActionAction, action->action);
    EXPECT_EQ(kNotificationActionTitle, action->title);
    EXPECT_EQ(kNotificationActionPlaceholder, action->placeholder);
  }
}

TEST(NotificationDataTest, SilentNotificationWithVibration) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;

  Vector<unsigned> vibration_pattern(kNotificationVibration);

  auto* vibration_sequence =
      MakeGarbageCollected<V8UnionUnsignedLongOrUnsignedLongSequence>(
          std::move(vibration_pattern));

  NotificationOptions* options =
      NotificationOptions::Create(scope.GetIsolate());
  options->setVibrate(vibration_sequence);
  options->setSilent(true);

  ExceptionState& exception_state = scope.GetExceptionState();
  mojom::blink::NotificationDataPtr notification_data =
      CreateNotificationData(scope.GetExecutionContext(), kNotificationTitle,
                             options, exception_state);
  ASSERT_THAT(exception_state,
              HadException(
                  ESErrorType::kTypeError,
                  "Silent notifications must not specify vibration patterns."));
}

TEST(NotificationDataTest, ActionTypeButtonWithPlaceholder) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;

  HeapVector<Member<NotificationAction>> actions;
  NotificationAction* action = NotificationAction::Create();
  action->setType("button");
  action->setPlaceholder("I'm afraid I can't do that...");
  actions.push_back(action);

  NotificationOptions* options =
      NotificationOptions::Create(scope.GetIsolate());
  options->setActions(actions);

  ExceptionState& exception_state = scope.GetExceptionState();
  mojom::blink::NotificationDataPtr notification_data =
      CreateNotificationData(scope.GetExecutionContext(), kNotificationTitle,
                             options, exception_state);
  ASSERT_THAT(
      exception_state,
      HadException(
          ESErrorType::kTypeError,
          "Notifications of type \"button\" cannot specify a placeholder."));
}

TEST(NotificationDataTest, RenotifyWithEmptyTag) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;

  NotificationOptions* options =
      NotificationOptions::Create(scope.GetIsolate());
  options->setTag(kNotificationEmptyTag);
  options->setRenotify(true);

  ExceptionState& exception_state = scope.GetExceptionState();
  mojom::blink::NotificationDataPtr notification_data =
      CreateNotificationData(scope.GetExecutionContext(), kNotificationTitle,
                             options, exception_state);
  ASSERT_THAT(exception_state,
              HadException(ESErrorType::kTypeError,
                           "Notifications which set the renotify flag must "
                           "specify a non-empty tag."));
}

TEST(NotificationDataTest, InvalidIconUrls) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;

  HeapVector<Member<NotificationAction>> actions;
  for (size_t i = 0; i < Notification::maxActions(); ++i) {
    NotificationAction* action = NotificationAction::Create();
    action->setAction(kNotificationActionAction);
    action->setTitle(kNotificationActionTitle);
    action->setIcon(kNotificationIconInvalid);
    actions.push_back(action);
  }

  NotificationOptions* options =
      NotificationOptions::Create(scope.GetIsolate());
  options->setImage(kNotificationIconInvalid);
  options->setIcon(kNotificationIconInvalid);
  options->setBadge(kNotificationIconInvalid);
  options->setActions(actions);

  ExceptionState& exception_state = scope.GetExceptionState();
  mojom::blink::NotificationDataPtr notification_data =
      CreateNotificationData(scope.GetExecutionContext(), kNotificationTitle,
                             options, exception_state);
  ASSERT_THAT(exception_state, HadNoException());

  EXPECT_TRUE(notification_data->image.IsEmpty());
  EXPECT_TRUE(notification_data->icon.IsEmpty());
  EXPECT_TRUE(notification_data->badge.IsEmpty());
  for (const auto& action : notification_data->actions.value())
    EXPECT_TRUE(action->icon.IsEmpty());
}

TEST(NotificationDataTest, VibrationNormalization) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;

  Vector<unsigned> unnormalized_pattern(kNotificationVibrationUnnormalized);

  auto* vibration_sequence =
      MakeGarbageCollected<V8UnionUnsignedLongOrUnsignedLongSequence>(
          unnormalized_pattern);

  NotificationOptions* options =
      NotificationOptions::Create(scope.GetIsolate());
  options->setVibrate(vibration_sequence);

  ExceptionState& exception_state = scope.GetExceptionState();
  mojom::blink::NotificationDataPtr notification_data =
      CreateNotificationData(scope.GetExecutionContext(), kNotificationTitle,
                             options, exception_state);
  EXPECT_THAT(exception_state, HadNoException());

  Vector<int> normalized_pattern(kNotificationVibrationNormalized);

  ASSERT_EQ(normalized_pattern.size(),
            notification_data->vibration_pattern->size());
  for (wtf_size_t i = 0; i < normalized_pattern.size(); ++i) {
    EXPECT_EQ(normalized_pattern[i],
              notification_data->vibration_pattern.value()[i]);
  }
}

TEST(NotificationDataTest, DefaultTimestampValue) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;

  NotificationOptions* options =
      NotificationOptions::Create(scope.GetIsolate());

  ExceptionState& exception_state = scope.GetExceptionState();
  mojom::blink::NotificationDataPtr notification_data =
      CreateNotificationData(scope.GetExecutionContext(), kNotificationTitle,
                             options, exception_state);
  EXPECT_THAT(exception_state, HadNoException());

  // The timestamp should be set to the current time since the epoch if it
  // wasn't supplied by the developer. "32" has no significance, but an equal
  // comparison of the value could lead to flaky failures.
  EXPECT_NEAR(notification_data->timestamp,
              base::Time::Now().InMillisecondsFSinceUnixEpoch(), 32);
}

TEST(NotificationDataTest, DirectionValues) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;

  WTF::HashMap<String, mojom::blink::NotificationDirection> mappings;
  mappings.insert("ltr", mojom::blink::NotificationDirection::LEFT_TO_RIGHT);
  mappings.insert("rtl", mojom::blink::NotificationDirection::RIGHT_TO_LEFT);
  mappings.insert("auto", mojom::blink::NotificationDirection::AUTO);

  for (const String& direction : mappings.Keys()) {
    NotificationOptions* options =
        NotificationOptions::Create(scope.GetIsolate());
    options->setDir(direction);

    ExceptionState& exception_state = scope.GetExceptionState();
    mojom::blink::NotificationDataPtr notification_data =
        CreateNotificationData(scope.GetExecutionContext(), kNotificationTitle,
                               options, exception_state);
    ASSERT_THAT(exception_state, HadNoException());

    EXPECT_EQ(mappings.at(direction), notification_data->direction);
  }
}

TEST(NotificationDataTest, MaximumActionCount) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;

  HeapVector<Member<NotificationAction>> actions;
  for (size_t i = 0; i < Notification::maxActions() + 2; ++i) {
    NotificationAction* action = NotificationAction::Create();
    action->setAction(String::Number(i));
    action->setTitle(kNotificationActionTitle);

    actions.push_back(action);
  }

  NotificationOptions* options =
      NotificationOptions::Create(scope.GetIsolate());
  options->setActions(actions);

  ExceptionState& exception_state = scope.GetExceptionState();
  mojom::blink::NotificationDataPtr notification_data =
      CreateNotificationData(scope.GetExecutionContext(), kNotificationTitle,
                             options, exception_state);
  ASSERT_THAT(exception_state, HadNoException());

  // The stored actions will be capped to |maxActions| entries.
  ASSERT_EQ(Notification::maxActions(), notification_data->actions->size());

  for (wtf_size_t i = 0; i < Notification::maxActions(); ++i) {
    String expected_action = String::Number(i);
    EXPECT_EQ(expected_action, notification_data->actions.value()[i]->action);
  }
}

TEST(NotificationDataTest, RejectsTriggerTimestampOverAYear) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;

  base::Time show_timestamp =
      base::Time::Now() + kMaxNotificationShowTriggerDelay + base::Days(1);
  TimestampTrigger* show_trigger =
      TimestampTrigger::Create(show_timestamp.InMillisecondsFSinceUnixEpoch());

  NotificationOptions* options =
      NotificationOptions::Create(scope.GetIsolate());
  options->setShowTrigger(show_trigger);

  ExceptionState& exception_state = scope.GetExceptionState();
  mojom::blink::NotificationDataPtr notification_data =
      CreateNotificationData(scope.GetExecutionContext(), kNotificationTitle,
                             options, exception_state);
  ASSERT_THAT(
      exception_state,
      HadException(
          ESErrorType::kTypeError,
          "Notification trigger timestamp too far ahead in the future."));
}

}  // namespace
}  // namespace blink
