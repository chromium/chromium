// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/notifications/notification_database_conversions.h"

#include <stddef.h>

#include <memory>

#include "base/logging.h"
#include "base/optional.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "content/browser/notifications/notification_database_data.pb.h"
#include "content/browser/notifications/notification_database_resources.pb.h"
#include "content/public/browser/notification_database_data.h"
#include "third_party/blink/public/common/notifications/notification_resources.h"
#include "third_party/blink/public/mojom/notifications/notification.mojom.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/png_codec.h"

namespace content {

namespace {

// static
SkBitmap DeserializeImage(const std::string& image_data) {
  base::AssertLongCPUWorkAllowed();
  SkBitmap image;
  gfx::PNGCodec::Decode(
      reinterpret_cast<const unsigned char*>(image_data.data()),
      image_data.length(), &image);
  return image;
}

// static
std::vector<unsigned char> SerializeImage(const SkBitmap& image) {
  base::AssertLongCPUWorkAllowed();
  std::vector<unsigned char> image_data;
  gfx::PNGCodec::EncodeBGRASkBitmap(image, false, &image_data);
  return image_data;
}

}  // namespace

bool DeserializeNotificationDatabaseData(const std::string& input,
                                         NotificationDatabaseData* output) {
  DCHECK(output);

  NotificationDatabaseDataProto message;
  if (!message.ParseFromString(input))
    return false;

  output->notification_id = message.notification_id();
  output->origin = GURL(message.origin());
  output->service_worker_registration_id =
      message.service_worker_registration_id();
  output->replaced_existing_notification =
      message.replaced_existing_notification();
  output->num_clicks = message.num_clicks();
  output->num_action_button_clicks = message.num_action_button_clicks();
  output->creation_time_millis = base::Time::FromDeltaSinceWindowsEpoch(
      base::TimeDelta::FromMicroseconds(message.creation_time_millis()));

  if (message.has_time_until_close_millis()) {
    output->time_until_close_millis =
        base::TimeDelta::FromMilliseconds(message.time_until_close_millis());
  } else {
    output->time_until_close_millis = base::nullopt;
  }
  if (message.has_time_until_first_click_millis()) {
    output->time_until_first_click_millis = base::TimeDelta::FromMilliseconds(
        message.time_until_first_click_millis());
  } else {
    output->time_until_first_click_millis = base::nullopt;
  }
  if (message.has_time_until_last_click_millis()) {
    output->time_until_last_click_millis = base::TimeDelta::FromMilliseconds(
        message.time_until_last_click_millis());
  } else {
    output->time_until_last_click_millis = base::nullopt;
  }

  switch (message.closed_reason()) {
    case NotificationDatabaseDataProto::USER:
      output->closed_reason = NotificationDatabaseData::ClosedReason::USER;
      break;
    case NotificationDatabaseDataProto::DEVELOPER:
      output->closed_reason = NotificationDatabaseData::ClosedReason::DEVELOPER;
      break;
    case NotificationDatabaseDataProto::UNKNOWN:
      output->closed_reason = NotificationDatabaseData::ClosedReason::UNKNOWN;
      break;
  }

  blink::PlatformNotificationData* notification_data =
      &output->notification_data;
  const NotificationDatabaseDataProto::NotificationData& payload =
      message.notification_data();

  notification_data->title = base::UTF8ToUTF16(payload.title());

  switch (payload.direction()) {
    case NotificationDatabaseDataProto::NotificationData::LEFT_TO_RIGHT:
      notification_data->direction =
          blink::mojom::NotificationDirection::LEFT_TO_RIGHT;
      break;
    case NotificationDatabaseDataProto::NotificationData::RIGHT_TO_LEFT:
      notification_data->direction =
          blink::mojom::NotificationDirection::RIGHT_TO_LEFT;
      break;
    case NotificationDatabaseDataProto::NotificationData::AUTO:
      notification_data->direction = blink::mojom::NotificationDirection::AUTO;
      break;
  }

  notification_data->lang = payload.lang();
  notification_data->body = base::UTF8ToUTF16(payload.body());
  notification_data->tag = payload.tag();
  notification_data->image = GURL(payload.image());
  notification_data->icon = GURL(payload.icon());
  notification_data->badge = GURL(payload.badge());

  if (payload.vibration_pattern().size() > 0) {
    notification_data->vibration_pattern.assign(
        payload.vibration_pattern().begin(), payload.vibration_pattern().end());
  } else {
    notification_data->vibration_pattern.clear();
  }

  notification_data->timestamp = base::Time::FromDeltaSinceWindowsEpoch(
      base::TimeDelta::FromMicroseconds(payload.timestamp()));

  notification_data->renotify = payload.renotify();
  notification_data->silent = payload.silent();
  notification_data->require_interaction = payload.require_interaction();

  if (payload.data().length()) {
    notification_data->data.assign(payload.data().begin(),
                                   payload.data().end());
  } else {
    notification_data->data.clear();
  }

  notification_data->actions.clear();

  for (const auto& payload_action : payload.actions()) {
    blink::PlatformNotificationAction action;

    switch (payload_action.type()) {
      case NotificationDatabaseDataProto::NotificationAction::BUTTON:
        action.type = blink::PLATFORM_NOTIFICATION_ACTION_TYPE_BUTTON;
        break;
      case NotificationDatabaseDataProto::NotificationAction::TEXT:
        action.type = blink::PLATFORM_NOTIFICATION_ACTION_TYPE_TEXT;
        break;
      default:
        NOTREACHED();
    }

    action.action = payload_action.action();
    action.title = base::UTF8ToUTF16(payload_action.title());
    action.icon = GURL(payload_action.icon());
    if (payload_action.has_placeholder()) {
      action.placeholder = base::NullableString16(
          base::UTF8ToUTF16(payload_action.placeholder()), false);
    }
    notification_data->actions.push_back(action);
  }

  if (payload.has_show_trigger_timestamp()) {
    notification_data->show_trigger_timestamp =
        base::Time::FromDeltaSinceWindowsEpoch(
            base::TimeDelta::FromMicroseconds(
                payload.show_trigger_timestamp()));
  } else {
    notification_data->show_trigger_timestamp = base::nullopt;
  }

  output->has_triggered = message.has_triggered();

  output->notification_resources = base::nullopt;

  return true;
}

bool SerializeNotificationDatabaseData(const NotificationDatabaseData& input,
                                       std::string* output) {
  DCHECK(output);

  auto payload =
      std::make_unique<NotificationDatabaseDataProto::NotificationData>();

  const blink::PlatformNotificationData& notification_data =
      input.notification_data;

  payload->set_title(base::UTF16ToUTF8(notification_data.title));

  switch (notification_data.direction) {
    case blink::mojom::NotificationDirection::LEFT_TO_RIGHT:
      payload->set_direction(
          NotificationDatabaseDataProto::NotificationData::LEFT_TO_RIGHT);
      break;
    case blink::mojom::NotificationDirection::RIGHT_TO_LEFT:
      payload->set_direction(
          NotificationDatabaseDataProto::NotificationData::RIGHT_TO_LEFT);
      break;
    case blink::mojom::NotificationDirection::AUTO:
      payload->set_direction(
          NotificationDatabaseDataProto::NotificationData::AUTO);
      break;
  }

  payload->set_lang(notification_data.lang);
  payload->set_body(base::UTF16ToUTF8(notification_data.body));
  payload->set_tag(notification_data.tag);
  payload->set_image(notification_data.image.spec());
  payload->set_icon(notification_data.icon.spec());
  payload->set_badge(notification_data.badge.spec());

  for (size_t i = 0; i < notification_data.vibration_pattern.size(); ++i)
    payload->add_vibration_pattern(notification_data.vibration_pattern[i]);

  payload->set_timestamp(
      notification_data.timestamp.ToDeltaSinceWindowsEpoch().InMicroseconds());
  payload->set_renotify(notification_data.renotify);
  payload->set_silent(notification_data.silent);
  payload->set_require_interaction(notification_data.require_interaction);

  if (notification_data.data.size()) {
    payload->set_data(&notification_data.data.front(),
                      notification_data.data.size());
  }

  for (const blink::PlatformNotificationAction& action :
       notification_data.actions) {
    NotificationDatabaseDataProto::NotificationAction* payload_action =
        payload->add_actions();

    switch (action.type) {
      case blink::PLATFORM_NOTIFICATION_ACTION_TYPE_BUTTON:
        payload_action->set_type(
            NotificationDatabaseDataProto::NotificationAction::BUTTON);
        break;
      case blink::PLATFORM_NOTIFICATION_ACTION_TYPE_TEXT:
        payload_action->set_type(
            NotificationDatabaseDataProto::NotificationAction::TEXT);
        break;
      default:
        NOTREACHED() << "Unknown action type: " << action.type;
    }

    payload_action->set_action(action.action);
    payload_action->set_title(base::UTF16ToUTF8(action.title));
    payload_action->set_icon(action.icon.spec());

    if (!action.placeholder.is_null()) {
      payload_action->set_placeholder(
          base::UTF16ToUTF8(action.placeholder.string()));
    }
  }

  if (notification_data.show_trigger_timestamp.has_value()) {
    payload->set_show_trigger_timestamp(
        notification_data.show_trigger_timestamp.value()
            .ToDeltaSinceWindowsEpoch()
            .InMicroseconds());
  }

  NotificationDatabaseDataProto message;
  message.set_notification_id(input.notification_id);
  message.set_origin(input.origin.spec());
  message.set_service_worker_registration_id(
      input.service_worker_registration_id);
  message.set_allocated_notification_data(payload.release());
  message.set_replaced_existing_notification(
      input.replaced_existing_notification);
  message.set_num_clicks(input.num_clicks);
  message.set_num_action_button_clicks(input.num_action_button_clicks);
  message.set_creation_time_millis(
      input.creation_time_millis.ToDeltaSinceWindowsEpoch().InMicroseconds());
  if (input.time_until_first_click_millis.has_value()) {
    message.set_time_until_first_click_millis(
        input.time_until_first_click_millis.value().InMilliseconds());
  }
  if (input.time_until_last_click_millis.has_value()) {
    message.set_time_until_last_click_millis(
        input.time_until_last_click_millis.value().InMilliseconds());
  }
  if (input.time_until_close_millis.has_value()) {
    message.set_time_until_close_millis(
        input.time_until_close_millis.value().InMilliseconds());
  }

  switch (input.closed_reason) {
    case NotificationDatabaseData::ClosedReason::USER:
      message.set_closed_reason(NotificationDatabaseDataProto::USER);
      break;
    case NotificationDatabaseData::ClosedReason::DEVELOPER:
      message.set_closed_reason(NotificationDatabaseDataProto::DEVELOPER);
      break;
    case NotificationDatabaseData::ClosedReason::UNKNOWN:
      message.set_closed_reason(NotificationDatabaseDataProto::UNKNOWN);
      break;
  }

  message.set_has_triggered(input.has_triggered);

  return message.SerializeToString(output);
}

bool DeserializeNotificationDatabaseResources(
    const std::string& serialized_resources,
    blink::NotificationResources* output) {
  DCHECK(output);

  NotificationDatabaseResourcesProto message;
  if (!message.ParseFromString(serialized_resources))
    return false;

  if (message.has_image())
    output->image = DeserializeImage(message.image());
  else
    output->image = SkBitmap();

  if (message.has_notification_icon())
    output->notification_icon = DeserializeImage(message.notification_icon());
  else
    output->notification_icon = SkBitmap();

  if (message.has_badge())
    output->badge = DeserializeImage(message.badge());
  else
    output->badge = SkBitmap();

  output->action_icons.clear();
  for (int i = 0; i < message.action_icons_size(); ++i)
    output->action_icons.push_back(DeserializeImage(message.action_icons(i)));

  return true;
}

bool SerializeNotificationDatabaseResources(
    const blink::NotificationResources& input,
    std::string* serialized_resources) {
  DCHECK(serialized_resources);

  NotificationDatabaseResourcesProto message;

  if (!input.image.isNull()) {
    auto image_data = SerializeImage(input.image);
    message.set_image(image_data.data(), image_data.size());
  }
  if (!input.notification_icon.isNull()) {
    auto image_data = SerializeImage(input.notification_icon);
    message.set_notification_icon(image_data.data(), image_data.size());
  }
  if (!input.badge.isNull()) {
    auto image_data = SerializeImage(input.badge);
    message.set_badge(image_data.data(), image_data.size());
  }
  for (const auto& image : input.action_icons) {
    if (!image.isNull()) {
      auto image_data = SerializeImage(image);
      message.add_action_icons(image_data.data(), image_data.size());
    } else {
      message.add_action_icons();
    }
  }

  return message.SerializeToString(serialized_resources);
}

}  // namespace content
