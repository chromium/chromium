// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/public/common/notifications/notification_mojom_traits.h"

#include <optional>

#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/notifications/platform_notification_data.h"
#include "third_party/blink/public/mojom/notifications/notification.mojom.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "url/gurl.h"

namespace blink {

namespace {

// Returns true if |lhs| and |rhs| have the same width and height and the
// pixel at position (0, 0) is the same color in both.
bool ImagesShareDimensionsAndColor(const SkBitmap& lhs, const SkBitmap& rhs) {
  return lhs.width() == rhs.width() && lhs.height() == rhs.height() &&
         lhs.getColor(0, 0) == rhs.getColor(0, 0);
}

}  // namespace

TEST(NotificationStructTraitsTest, NotificationDataRoundtrip) {
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
  notification_data.vibration_pattern.assign(
      vibration_pattern, vibration_pattern + std::size(vibration_pattern));

  notification_data.timestamp =
      base::Time::FromMillisecondsSinceUnixEpoch(1513966159000.);
  notification_data.renotify = true;
  notification_data.silent = true;
  notification_data.require_interaction = true;
  notification_data.show_trigger_timestamp = base::Time::Now();
  notification_data.scenario = mojom::NotificationScenario::INCOMING_CALL;

  const char data[] = "mock binary notification data";
  notification_data.data.assign(data, data + std::size(data));

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

  PlatformNotificationData roundtrip_notification_data;

  ASSERT_TRUE(
      mojo::test::SerializeAndDeserialize<blink::mojom::NotificationData>(
          notification_data, roundtrip_notification_data));

  EXPECT_EQ(roundtrip_notification_data.title, notification_data.title);
  EXPECT_EQ(roundtrip_notification_data.direction, notification_data.direction);
  EXPECT_EQ(roundtrip_notification_data.lang, notification_data.lang);
  EXPECT_EQ(roundtrip_notification_data.body, notification_data.body);
  EXPECT_EQ(roundtrip_notification_data.tag, notification_data.tag);
  EXPECT_EQ(roundtrip_notification_data.image, notification_data.image);
  EXPECT_EQ(roundtrip_notification_data.icon, notification_data.icon);
  EXPECT_EQ(roundtrip_notification_data.badge, notification_data.badge);
  EXPECT_EQ(roundtrip_notification_data.vibration_pattern,
            notification_data.vibration_pattern);
  EXPECT_EQ(roundtrip_notification_data.timestamp, notification_data.timestamp);
  EXPECT_EQ(roundtrip_notification_data.renotify, notification_data.renotify);
  EXPECT_EQ(roundtrip_notification_data.silent, notification_data.silent);
  EXPECT_EQ(roundtrip_notification_data.require_interaction,
            notification_data.require_interaction);
  EXPECT_EQ(roundtrip_notification_data.data, notification_data.data);
  ASSERT_EQ(notification_data.actions.size(),
            roundtrip_notification_data.actions.size());
  for (size_t i = 0; i < notification_data.actions.size(); ++i) {
    SCOPED_TRACE(base::StringPrintf("Action index: %zd", i));
    EXPECT_EQ(notification_data.actions[i]->type,
              roundtrip_notification_data.actions[i]->type);
    EXPECT_EQ(notification_data.actions[i]->action,
              roundtrip_notification_data.actions[i]->action);
    EXPECT_EQ(notification_data.actions[i]->title,
              roundtrip_notification_data.actions[i]->title);
    EXPECT_EQ(notification_data.actions[i]->icon,
              roundtrip_notification_data.actions[i]->icon);
    EXPECT_EQ(notification_data.actions[i]->placeholder,
              roundtrip_notification_data.actions[i]->placeholder);
  }
  EXPECT_EQ(roundtrip_notification_data.show_trigger_timestamp,
            notification_data.show_trigger_timestamp);
  EXPECT_EQ(roundtrip_notification_data.scenario, notification_data.scenario);
}

// Check upper bound on vibration entries (99).
TEST(NotificationStructTraitsTest, ValidVibrationPattern) {
  constexpr int kEntries = 99;      // valid
  constexpr int kDurationMs = 999;  // valid

  PlatformNotificationData notification_data;
  notification_data.title = u"Notification with 99 x 999ms entries (valid)";

  for (size_t i = 0; i < kEntries; ++i)
    notification_data.vibration_pattern.push_back(kDurationMs);

  PlatformNotificationData platform_notification_data;

  ASSERT_TRUE(
      mojo::test::SerializeAndDeserialize<blink::mojom::NotificationData>(
          notification_data, platform_notification_data));
}

// Check round-trip fails when there are too many entries in the vibration
// pattern.
TEST(NotificationStructTraitsTest, TooManyVibrations) {
  constexpr int kEntries = 100;   // invalid
  constexpr int kDurationMs = 1;  // valid

  PlatformNotificationData notification_data;
  notification_data.title = u"Notification with 100 x 1ms entries (invalid)";

  for (size_t i = 0; i < kEntries; ++i)
    notification_data.vibration_pattern.push_back(kDurationMs);

  PlatformNotificationData platform_notification_data;

  ASSERT_FALSE(
      mojo::test::SerializeAndDeserialize<blink::mojom::NotificationData>(
          notification_data, platform_notification_data));
}

// Check round-trip fails when there is a too-long vibration duration.
TEST(NotificationStructTraitsTest, TooLongVibrationDuration) {
  constexpr int kEntries = 1;         // valid
  constexpr int kDurationMs = 10001;  // invalid (>10 seconds)

  PlatformNotificationData notification_data;
  notification_data.title = u"Notification with 1 x 10001ms entries (invalid)";

  for (size_t i = 0; i < kEntries; ++i)
    notification_data.vibration_pattern.push_back(kDurationMs);

  PlatformNotificationData platform_notification_data;

  ASSERT_FALSE(
      mojo::test::SerializeAndDeserialize<blink::mojom::NotificationData>(
          notification_data, platform_notification_data));
}

// Check round-trip fails when there are too many actions provided.
TEST(NotificationStructTraitsTest, TooManyActions) {
  constexpr int kActions = 3;  // invalid (max is 2)

  PlatformNotificationData notification_data;
  notification_data.title = u"Notification with 3 actions provided (invalid)";

  notification_data.actions.resize(kActions);
  for (size_t i = 0; i < kActions; ++i) {
    notification_data.actions[i] = blink::mojom::NotificationAction::New();
    notification_data.actions[i]->title = u"action title";
  }

  PlatformNotificationData platform_notification_data;

  ASSERT_FALSE(
      mojo::test::SerializeAndDeserialize<blink::mojom::NotificationData>(
          notification_data, platform_notification_data));
}

// Check round-trip fails when the data size is too big.
TEST(NotificationStructTraitsTest, DataExceedsMaximumSize) {
  constexpr size_t kDataSize = 1024 * 1024 + 1;  // 1 more than max data size.

  PlatformNotificationData notification_data;
  notification_data.title = u"Notification with too much data";

  notification_data.data.resize(kDataSize);

  PlatformNotificationData platform_notification_data;

  ASSERT_FALSE(
      mojo::test::SerializeAndDeserialize<blink::mojom::NotificationData>(
          notification_data, platform_notification_data));
}

TEST(NotificationStructTraitsTest, NotificationResourcesRoundtrip) {
  NotificationResources resources;

  resources.image = gfx::test::CreateBitmap(200, 100, SK_ColorMAGENTA);
  resources.notification_icon = gfx::test::CreateBitmap(100, 50, SK_ColorGREEN);
  resources.badge = gfx::test::CreateBitmap(20, 10, SK_ColorBLUE);

  resources.action_icons.resize(2);
  resources.action_icons[0] =
      gfx::test::CreateBitmap(/*size=*/10, SK_ColorLTGRAY);
  resources.action_icons[1] =
      gfx::test::CreateBitmap(/*size=*/11, SK_ColorDKGRAY);

  NotificationResources roundtrip_resources;

  ASSERT_TRUE(
      mojo::test::SerializeAndDeserialize<blink::mojom::NotificationResources>(
          resources, roundtrip_resources));

  ASSERT_FALSE(roundtrip_resources.image.empty());
  EXPECT_TRUE(ImagesShareDimensionsAndColor(resources.image,
                                            roundtrip_resources.image));

  ASSERT_FALSE(roundtrip_resources.notification_icon.empty());
  EXPECT_TRUE(ImagesShareDimensionsAndColor(
      resources.notification_icon, roundtrip_resources.notification_icon));

  ASSERT_FALSE(roundtrip_resources.badge.empty());
  EXPECT_TRUE(ImagesShareDimensionsAndColor(resources.badge,
                                            roundtrip_resources.badge));

  ASSERT_EQ(resources.action_icons.size(),
            roundtrip_resources.action_icons.size());

  for (size_t i = 0; i < roundtrip_resources.action_icons.size(); ++i) {
    SCOPED_TRACE(base::StringPrintf("Action icon index: %zd", i));
    ASSERT_FALSE(roundtrip_resources.action_icons[i].empty());
    EXPECT_TRUE(ImagesShareDimensionsAndColor(
        resources.action_icons[i], roundtrip_resources.action_icons[i]));
  }
}

}  // namespace blink
