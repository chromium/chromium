// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/notifications/notification_database_conversions.h"

#include <stddef.h>
#include <stdint.h>

#include <array>
#include <optional>

#include "base/compiler_specific.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "content/browser/notifications/notification_database_data.pb.h"
#include "content/browser/notifications/notification_database_resources.pb.h"
#include "content/public/browser/notification_database_data.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/notifications/notification_constants.h"
#include "third_party/blink/public/common/notifications/notification_resources.h"
#include "third_party/blink/public/mojom/notifications/notification.mojom.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/image/image_unittest_util.h"

namespace content {

const blink::mojom::NotificationActionType kNotificationActionType =
    blink::mojom::NotificationActionType::TEXT;
const int kNotificationVibrationPattern[] = {100, 200, 300};

TEST(NotificationDatabaseConversionsTest, SerializeAndDeserializeData) {
  std::vector<int> vibration_pattern(std::begin(kNotificationVibrationPattern),
                                     std::end(kNotificationVibrationPattern));

  std::vector<char> developer_data({
      '\xdf',
      '\xff',
      '\x00',
      '\x00',
      '\xff',
      '\xdf',
  });

  blink::PlatformNotificationData notification_data;
  notification_data.title = u"My Notification";
  notification_data.direction =
      blink::mojom::NotificationDirection::RIGHT_TO_LEFT;
  notification_data.lang = "nl";
  notification_data.body = u"Hello, world!";
  notification_data.tag = "my_tag";
  notification_data.image = GURL("https://example.com/image.jpg");
  notification_data.icon = GURL("https://example.com/icon.png");
  notification_data.badge = GURL("https://example.com/badge.png");
  notification_data.vibration_pattern = vibration_pattern;
  notification_data.timestamp =
      base::Time::FromMillisecondsSinceUnixEpoch(621046800.);
  notification_data.renotify = true;
  notification_data.silent = true;
  notification_data.require_interaction = true;
  notification_data.show_trigger_timestamp =
      base::Time::FromMillisecondsSinceUnixEpoch(621086800.);
  notification_data.data = developer_data;
  for (size_t i = 0; i < blink::kNotificationMaxActions; i++) {
    auto notification_action = blink::mojom::NotificationAction::New();
    notification_action->type = kNotificationActionType;
    notification_action->action = base::NumberToString(i);
    notification_action->title = base::NumberToString16(i);
    notification_action->icon = GURL("https://example.com/action_icon.png");
    notification_action->placeholder = base::NumberToString16(i);
    notification_data.actions.push_back(std::move(notification_action));
  }

  NotificationDatabaseData database_data;
  database_data.notification_id = "my-notification";
  database_data.origin = GURL("https://example.com/");
  database_data.service_worker_registration_id = 9001;
  database_data.notification_data = notification_data;
  database_data.replaced_existing_notification = true;
  database_data.num_clicks = 8;
  database_data.num_action_button_clicks = 9;
  database_data.creation_time_millis =
      base::Time::FromSecondsSinceUnixEpoch(12345);
  database_data.time_until_first_click_millis = base::Milliseconds(11111);
  database_data.time_until_last_click_millis = base::Milliseconds(22222);
  database_data.time_until_close_millis = base::Milliseconds(33333);
  database_data.closed_reason = NotificationDatabaseData::ClosedReason::USER;
  database_data.has_triggered = true;
  database_data.is_shown_by_browser = true;
  database_data.serialized_metadata = {
      {"content-detection", "{\"dummy\":\"value\"}"},
  };
  std::string serialized_data;

  // Serialize the data in |notification_data| to the string |serialized_data|.
  ASSERT_TRUE(
      SerializeNotificationDatabaseData(database_data, &serialized_data));

  NotificationDatabaseData copied_data;

  // Deserialize the data in |serialized_data| to |copied_data|.
  ASSERT_TRUE(
      DeserializeNotificationDatabaseData(serialized_data, &copied_data));

  EXPECT_EQ(database_data.notification_id, copied_data.notification_id);
  EXPECT_EQ(database_data.origin, copied_data.origin);
  EXPECT_EQ(database_data.service_worker_registration_id,
            copied_data.service_worker_registration_id);
  EXPECT_EQ(database_data.num_clicks, copied_data.num_clicks);
  EXPECT_EQ(database_data.num_action_button_clicks,
            copied_data.num_action_button_clicks);
  EXPECT_EQ(database_data.replaced_existing_notification,
            copied_data.replaced_existing_notification);
  EXPECT_EQ(database_data.creation_time_millis,
            copied_data.creation_time_millis);
  EXPECT_EQ(database_data.time_until_first_click_millis,
            copied_data.time_until_first_click_millis);
  EXPECT_EQ(database_data.time_until_last_click_millis,
            copied_data.time_until_last_click_millis);
  EXPECT_EQ(database_data.time_until_close_millis,
            copied_data.time_until_close_millis);
  EXPECT_EQ(database_data.closed_reason, copied_data.closed_reason);
  EXPECT_EQ(database_data.has_triggered, copied_data.has_triggered);
  EXPECT_EQ(database_data.is_shown_by_browser, copied_data.is_shown_by_browser);
  EXPECT_EQ(database_data.serialized_metadata, copied_data.serialized_metadata);

  const blink::PlatformNotificationData& copied_notification_data =
      copied_data.notification_data;

  EXPECT_EQ(notification_data.title, copied_notification_data.title);
  EXPECT_EQ(notification_data.direction, copied_notification_data.direction);
  EXPECT_EQ(notification_data.lang, copied_notification_data.lang);
  EXPECT_EQ(notification_data.body, copied_notification_data.body);
  EXPECT_EQ(notification_data.tag, copied_notification_data.tag);
  EXPECT_EQ(notification_data.image, copied_notification_data.image);
  EXPECT_EQ(notification_data.icon, copied_notification_data.icon);
  EXPECT_EQ(notification_data.badge, copied_notification_data.badge);

  EXPECT_THAT(copied_notification_data.vibration_pattern,
              testing::ElementsAreArray(kNotificationVibrationPattern));

  EXPECT_EQ(notification_data.timestamp, copied_notification_data.timestamp);
  EXPECT_EQ(notification_data.renotify, copied_notification_data.renotify);
  EXPECT_EQ(notification_data.silent, copied_notification_data.silent);
  EXPECT_EQ(notification_data.require_interaction,
            copied_notification_data.require_interaction);
  EXPECT_EQ(notification_data.show_trigger_timestamp,
            copied_notification_data.show_trigger_timestamp);

  ASSERT_EQ(developer_data.size(), copied_notification_data.data.size());
  for (size_t i = 0; i < developer_data.size(); ++i)
    EXPECT_EQ(developer_data[i], copied_notification_data.data[i]);

  ASSERT_EQ(notification_data.actions.size(),
            copied_notification_data.actions.size());
  for (size_t i = 0; i < notification_data.actions.size(); ++i) {
    EXPECT_EQ(notification_data.actions[i]->type,
              copied_notification_data.actions[i]->type);
    EXPECT_EQ(notification_data.actions[i]->action,
              copied_notification_data.actions[i]->action);
    EXPECT_EQ(notification_data.actions[i]->title,
              copied_notification_data.actions[i]->title);
    EXPECT_EQ(notification_data.actions[i]->icon,
              copied_notification_data.actions[i]->icon);
    EXPECT_EQ(notification_data.actions[i]->placeholder,
              copied_notification_data.actions[i]->placeholder);
    EXPECT_TRUE(copied_notification_data.actions[i]->placeholder);
  }
}

TEST(NotificationDatabaseConversionsTest, ActionDeserializationIsNotAdditive) {
  NotificationDatabaseData database_data;

  for (size_t i = 0; i < blink::kNotificationMaxActions; ++i) {
    database_data.notification_data.actions.emplace_back(
        blink::mojom::NotificationAction::New());
  }

  std::string serialized_data;
  NotificationDatabaseData copied_database_data;

  // Serialize the data in |notification_data| to the string |serialized_data|,
  // and then deserialize it again immediately to |copied_database_data|.
  ASSERT_TRUE(
      SerializeNotificationDatabaseData(database_data, &serialized_data));
  ASSERT_TRUE(DeserializeNotificationDatabaseData(serialized_data,
                                                  &copied_database_data));

  EXPECT_EQ(copied_database_data.notification_data.actions.size(),
            blink::kNotificationMaxActions);

  // Deserialize it again in the same |copied_database_data|. The number of
  // actions in the structure should not be affected.
  ASSERT_TRUE(DeserializeNotificationDatabaseData(serialized_data,
                                                  &copied_database_data));

  EXPECT_EQ(copied_database_data.notification_data.actions.size(),
            blink::kNotificationMaxActions);
}

TEST(NotificationDatabaseConversionsTest, SerializeAndDeserializeActionTypes) {
  for (blink::mojom::NotificationActionType action_type : {
           blink::mojom::NotificationActionType::BUTTON,
           blink::mojom::NotificationActionType::TEXT,
       }) {
    blink::PlatformNotificationData notification_data;

    auto action = blink::mojom::NotificationAction::New();
    action->type = action_type;
    notification_data.actions.push_back(std::move(action));

    NotificationDatabaseData database_data;
    database_data.notification_data = notification_data;

    std::string serialized_data;
    ASSERT_TRUE(
        SerializeNotificationDatabaseData(database_data, &serialized_data));

    NotificationDatabaseData copied_data;
    ASSERT_TRUE(
        DeserializeNotificationDatabaseData(serialized_data, &copied_data));

    EXPECT_EQ(action_type, copied_data.notification_data.actions[0]->type);
  }
}

TEST(NotificationDatabaseConversionsTest, SerializeAndDeserializeDirections) {
  auto directions = std::to_array<blink::mojom::NotificationDirection>({
      blink::mojom::NotificationDirection::LEFT_TO_RIGHT,
      blink::mojom::NotificationDirection::RIGHT_TO_LEFT,
      blink::mojom::NotificationDirection::AUTO,
  });

  for (size_t i = 0; i < std::size(directions); ++i) {
    blink::PlatformNotificationData notification_data;
    notification_data.direction = directions[i];

    NotificationDatabaseData database_data;
    database_data.notification_data = notification_data;

    std::string serialized_data;
    ASSERT_TRUE(
        SerializeNotificationDatabaseData(database_data, &serialized_data));

    NotificationDatabaseData copied_data;
    ASSERT_TRUE(
        DeserializeNotificationDatabaseData(serialized_data, &copied_data));

    EXPECT_EQ(directions[i], copied_data.notification_data.direction);
  }
}

TEST(NotificationDatabaseConversionsTest,
     SerializeAndDeserializeClosedReasons) {
  auto closed_reasons = std::to_array<NotificationDatabaseData::ClosedReason>({
      NotificationDatabaseData::ClosedReason::USER,
      NotificationDatabaseData::ClosedReason::DEVELOPER,
      NotificationDatabaseData::ClosedReason::UNKNOWN,
  });

  for (size_t i = 0; i < std::size(closed_reasons); ++i) {
    NotificationDatabaseData database_data;
    database_data.closed_reason = closed_reasons[i];

    std::string serialized_data;
    ASSERT_TRUE(
        SerializeNotificationDatabaseData(database_data, &serialized_data));

    NotificationDatabaseData copied_data;
    ASSERT_TRUE(
        DeserializeNotificationDatabaseData(serialized_data, &copied_data));

    EXPECT_EQ(closed_reasons[i], copied_data.closed_reason);
  }
}

TEST(NotificationDatabaseConversionsTest,
     SerializeAndDeserializeNullPlaceholder) {
  auto action = blink::mojom::NotificationAction::New();
  action->type = kNotificationActionType;
  action->placeholder = std::nullopt;  // null string.

  blink::PlatformNotificationData notification_data;
  notification_data.actions.push_back(std::move(action));

  NotificationDatabaseData database_data;
  database_data.notification_data = notification_data;

  std::string serialized_data;
  ASSERT_TRUE(
      SerializeNotificationDatabaseData(database_data, &serialized_data));

  NotificationDatabaseData copied_data;
  ASSERT_TRUE(
      DeserializeNotificationDatabaseData(serialized_data, &copied_data));

  EXPECT_FALSE(copied_data.notification_data.actions[0]->placeholder);
}

TEST(NotificationDatabaseConversionsTest,
     SerializeAndDeserializeNullShowTriggerTimestamp) {
  blink::PlatformNotificationData notification_data;

  // explicitly empty timestamp
  notification_data.show_trigger_timestamp = std::nullopt;

  NotificationDatabaseData database_data;
  database_data.notification_data = notification_data;

  std::string serialized_data;
  ASSERT_TRUE(
      SerializeNotificationDatabaseData(database_data, &serialized_data));

  NotificationDatabaseData copied_data;
  ASSERT_TRUE(
      DeserializeNotificationDatabaseData(serialized_data, &copied_data));

  EXPECT_FALSE(
      copied_data.notification_data.show_trigger_timestamp.has_value());
}

TEST(NotificationDatabaseConversionsTest, OptionalFieldsGetCleared) {
  NotificationDatabaseData data_without_fields;
  NotificationDatabaseData data_with_fields;

  data_with_fields.time_until_close_millis = base::Seconds(1);
  data_with_fields.time_until_first_click_millis = base::Seconds(2);
  data_with_fields.time_until_last_click_millis = base::Seconds(3);
  data_with_fields.notification_resources = blink::NotificationResources();

  std::string serialized_data;
  NotificationDatabaseData copied_database_data;

  // Serialize the |data_with_fields| to the string |serialized_data|,
  // and then deserialize it again immediately to |copied_database_data|.
  ASSERT_TRUE(
      SerializeNotificationDatabaseData(data_with_fields, &serialized_data));
  ASSERT_TRUE(DeserializeNotificationDatabaseData(serialized_data,
                                                  &copied_database_data));

  EXPECT_EQ(base::Seconds(1), copied_database_data.time_until_close_millis);
  EXPECT_EQ(base::Seconds(2),
            copied_database_data.time_until_first_click_millis);
  EXPECT_EQ(base::Seconds(3),
            copied_database_data.time_until_last_click_millis);
  EXPECT_FALSE(copied_database_data.notification_resources.has_value());

  // Deserialize the |data_without_fields| in the same |copied_database_data|.
  // The optional fields should now be gone.
  ASSERT_TRUE(
      SerializeNotificationDatabaseData(data_without_fields, &serialized_data));
  ASSERT_TRUE(DeserializeNotificationDatabaseData(serialized_data,
                                                  &copied_database_data));

  EXPECT_FALSE(copied_database_data.time_until_close_millis.has_value());
  EXPECT_FALSE(copied_database_data.time_until_first_click_millis.has_value());
  EXPECT_FALSE(copied_database_data.time_until_last_click_millis.has_value());
  EXPECT_FALSE(copied_database_data.notification_resources.has_value());
  EXPECT_EQ(0u, copied_database_data.serialized_metadata.size());
}

TEST(NotificationDatabaseConversionsTest,
     SerializeAndDeserializeNotificationResources) {
  blink::NotificationResources notification_resources;

  notification_resources.notification_icon =
      gfx::test::CreateBitmap(/*size=*/10, SK_ColorBLUE);
  notification_resources.image =
      gfx::test::CreateBitmap(/*size=*/20, SK_ColorGREEN);
  notification_resources.badge =
      gfx::test::CreateBitmap(/*size=*/30, SK_ColorRED);

  notification_resources.action_icons.push_back(
      gfx::test::CreateBitmap(/*size=*/40, SK_ColorYELLOW));
  notification_resources.action_icons.push_back(
      gfx::test::CreateBitmap(/*size=*/41, SK_ColorCYAN));
  notification_resources.action_icons.push_back(
      gfx::test::CreateBitmap(/*size=*/42, SK_ColorMAGENTA));

  std::string serialized_resources;
  ASSERT_TRUE(SerializeNotificationDatabaseResources(notification_resources,
                                                     &serialized_resources));

  blink::NotificationResources copied_resources;
  ASSERT_TRUE(DeserializeNotificationDatabaseResources(serialized_resources,
                                                       &copied_resources));

  EXPECT_EQ(10, copied_resources.notification_icon.width());
  EXPECT_EQ(20, copied_resources.image.width());
  EXPECT_EQ(30, copied_resources.badge.width());
  EXPECT_EQ(3u, copied_resources.action_icons.size());
  EXPECT_EQ(40, copied_resources.action_icons[0].width());
  EXPECT_EQ(41, copied_resources.action_icons[1].width());
  EXPECT_EQ(42, copied_resources.action_icons[2].width());
}

}  // namespace content
