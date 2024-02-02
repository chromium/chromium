// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "third_party/blink/public/common/notifications/notification_mojom_traits.h"
#include "third_party/blink/public/mojom/notifications/notification.mojom.h"

namespace {

// Maximum number of entries in a vibration pattern.
constexpr int kMaximumVibrationPatternLength = 99;

// Maximum duration of each vibration in a pattern.
constexpr int kMaximumVibrationDurationMs = 10000;  // 10 seconds.

// Maximum number of developer-provided actions on a notification.
constexpr size_t kMaximumActions = 2;

bool ValidateVibrationPattern(const std::vector<int>& vibration_pattern) {
  if (vibration_pattern.size() > kMaximumVibrationPatternLength)
    return false;
  for (const int duration : vibration_pattern) {
    if (duration < 0 || duration > kMaximumVibrationDurationMs)
      return false;
  }
  return true;
}

bool ValidateActions(
    const std::vector<blink::mojom::NotificationActionPtr>& actions) {
  return actions.size() <= kMaximumActions;
}

bool ValidateData(const std::vector<char>& data) {
  return data.size() <=
         blink::mojom::NotificationData::kMaximumDeveloperDataSize;
}

}  // namespace

namespace mojo {

// static
bool StructTraits<blink::mojom::NotificationDataDataView,
                  blink::PlatformNotificationData>::
    Read(blink::mojom::NotificationDataDataView notification_data,
         blink::PlatformNotificationData* platform_notification_data) {
  // TODO(https://crbug.com/798466): Read the data directly into
  // platform_notification_data.data once it stores a vector of ints not chars.
  std::vector<uint8_t> data;

  std::optional<std::string> lang;
  if (!notification_data.ReadTitle(&platform_notification_data->title) ||
      !notification_data.ReadDirection(
          &platform_notification_data->direction) ||
      !notification_data.ReadLang(&lang) ||
      !notification_data.ReadBody(&platform_notification_data->body) ||
      !notification_data.ReadTag(&platform_notification_data->tag) ||
      !notification_data.ReadImage(&platform_notification_data->image) ||
      !notification_data.ReadIcon(&platform_notification_data->icon) ||
      !notification_data.ReadBadge(&platform_notification_data->badge) ||
      !notification_data.ReadVibrationPattern(
          &platform_notification_data->vibration_pattern) ||
      !notification_data.ReadActions(&platform_notification_data->actions) ||
      !notification_data.ReadData(&data) ||
      !notification_data.ReadShowTriggerTimestamp(
          &platform_notification_data->show_trigger_timestamp) ||
      !notification_data.ReadScenario(&platform_notification_data->scenario)) {
    return false;
  }

  platform_notification_data->lang = std::move(lang).value_or(std::string());

  platform_notification_data->data.assign(data.begin(), data.end());

  platform_notification_data->timestamp =
      base::Time::FromMillisecondsSinceUnixEpoch(notification_data.timestamp());

  platform_notification_data->renotify = notification_data.renotify();

  platform_notification_data->silent = notification_data.silent();

  platform_notification_data->require_interaction =
      notification_data.require_interaction();

  return ValidateVibrationPattern(
             platform_notification_data->vibration_pattern) &&
         ValidateActions(platform_notification_data->actions) &&
         ValidateData(platform_notification_data->data);
}

// static
bool StructTraits<blink::mojom::NotificationResourcesDataView,
                  blink::NotificationResources>::
    Read(blink::mojom::NotificationResourcesDataView in,
         blink::NotificationResources* out) {
  return in.ReadImage(&out->image) && in.ReadIcon(&out->notification_icon) &&
         in.ReadBadge(&out->badge) && in.ReadActionIcons(&out->action_icons);
}

}  // namespace mojo
