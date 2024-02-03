// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/notifications/platform_notification_data.h"

#include "base/strings/stringprintf.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/notifications/notification.mojom.h"

namespace blink {

TEST(PlatformNotificationDataTest, AssignmentOperator) {
  PlatformNotificationData notification_data;
  notification_data.title = u"Title of my notification";
  notification_data.direction = mojom::NotificationDirection::AUTO;
  notification_data.lang = "test-lang";
  notification_data.body = u"Notification body.";
  notification_data.tag = "notification-tag";
  notification_data.image = GURL("https://example.com/image.png");
  notification_data.icon = GURL("https://example.com/icon.png");
  notification_data.badge = GURL("https://example.com/badge.png");

  const int vibration_pattern[] = {500, 100, 30};
  notification_data.vibration_pattern.assign(std::begin(vibration_pattern),
                                             std::end(vibration_pattern));

  notification_data.timestamp =
      base::Time::FromMillisecondsSinceUnixEpoch(1513966159000.);
  notification_data.renotify = true;
  notification_data.silent = true;
  notification_data.require_interaction = true;
  notification_data.show_trigger_timestamp = base::Time::Now();
  notification_data.scenario = mojom::NotificationScenario::INCOMING_CALL;

  const char data[] = "mock binary notification data";
  notification_data.data.assign(std::begin(data), std::end(data));

  notification_data.actions.resize(2);
  notification_data.actions[0] = blink::mojom::NotificationAction::New();
  notification_data.actions[0]->type =
      blink::mojom::NotificationActionType::BUTTON;
  notification_data.actions[0]->action = "buttonAction";
  notification_data.actions[0]->title = u"Button Title!";
  notification_data.actions[0]->icon = GURL("https://example.com/aButton.png");
  notification_data.actions[0]->placeholder = std::nullopt;

  notification_data.actions[1] = blink::mojom::NotificationAction::New();
  notification_data.actions[1]->type =
      blink::mojom::NotificationActionType::TEXT;
  notification_data.actions[1]->action = "textAction";
  notification_data.actions[1]->title = u"Reply Button Title";
  notification_data.actions[1]->icon = GURL("https://example.com/reply.png");
  notification_data.actions[1]->placeholder = u"Placeholder Text";

  // Initialize the PlatformNotificationData object and then reassign it to
  // make sure that the reassignement happens when all the internal variables
  // are already initialized. We do that to make sure that the assignment
  // operator is not making any implicit assumptions about the variables' state
  // - e.g., implcitily assuming that the `actions` vector is empty.
  PlatformNotificationData assigned_notification_data = notification_data;
  assigned_notification_data = notification_data;

  EXPECT_EQ(assigned_notification_data.title, notification_data.title);
  EXPECT_EQ(assigned_notification_data.direction, notification_data.direction);
  EXPECT_EQ(assigned_notification_data.lang, notification_data.lang);
  EXPECT_EQ(assigned_notification_data.body, notification_data.body);
  EXPECT_EQ(assigned_notification_data.tag, notification_data.tag);
  EXPECT_EQ(assigned_notification_data.image, notification_data.image);
  EXPECT_EQ(assigned_notification_data.icon, notification_data.icon);
  EXPECT_EQ(assigned_notification_data.badge, notification_data.badge);
  EXPECT_EQ(assigned_notification_data.vibration_pattern,
            notification_data.vibration_pattern);
  EXPECT_EQ(assigned_notification_data.timestamp, notification_data.timestamp);
  EXPECT_EQ(assigned_notification_data.renotify, notification_data.renotify);
  EXPECT_EQ(assigned_notification_data.silent, notification_data.silent);
  EXPECT_EQ(assigned_notification_data.require_interaction,
            notification_data.require_interaction);
  EXPECT_EQ(assigned_notification_data.data, notification_data.data);
  ASSERT_EQ(notification_data.actions.size(),
            assigned_notification_data.actions.size());
  for (size_t i = 0; i < notification_data.actions.size(); ++i) {
    SCOPED_TRACE(base::StringPrintf("Action index: %zd", i));
    EXPECT_EQ(notification_data.actions[i]->type,
              assigned_notification_data.actions[i]->type);
    EXPECT_EQ(notification_data.actions[i]->action,
              assigned_notification_data.actions[i]->action);
    EXPECT_EQ(notification_data.actions[i]->title,
              assigned_notification_data.actions[i]->title);
    EXPECT_EQ(notification_data.actions[i]->icon,
              assigned_notification_data.actions[i]->icon);
    EXPECT_EQ(notification_data.actions[i]->placeholder,
              assigned_notification_data.actions[i]->placeholder);
  }
  EXPECT_EQ(assigned_notification_data.show_trigger_timestamp,
            notification_data.show_trigger_timestamp);
  EXPECT_EQ(assigned_notification_data.scenario, notification_data.scenario);
}

}  // namespace blink
