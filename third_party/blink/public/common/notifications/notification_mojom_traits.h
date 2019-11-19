// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_NOTIFICATIONS_NOTIFICATION_MOJOM_TRAITS_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_NOTIFICATIONS_NOTIFICATION_MOJOM_TRAITS_H_

#include "base/containers/span.h"
#include "base/strings/string16.h"
#include "mojo/public/cpp/base/string16_mojom_traits.h"
#include "mojo/public/cpp/base/time_mojom_traits.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "skia/public/mojom/bitmap_skbitmap_mojom_traits.h"
#include "third_party/blink/public/common/common_export.h"
#include "third_party/blink/public/common/notifications/platform_notification_data.h"
#include "third_party/blink/public/mojom/notifications/notification.mojom-forward.h"
#include "url/gurl.h"
#include "url/mojom/url_gurl_mojom_traits.h"

namespace mojo {

template <>
struct BLINK_COMMON_EXPORT EnumTraits<blink::mojom::NotificationActionType,
                                      blink::PlatformNotificationActionType> {
  static blink::mojom::NotificationActionType ToMojom(
      blink::PlatformNotificationActionType input);

  static bool FromMojom(blink::mojom::NotificationActionType input,
                        blink::PlatformNotificationActionType* out);
};

template <>
struct BLINK_COMMON_EXPORT
    StructTraits<blink::mojom::NotificationActionDataView,
                 blink::PlatformNotificationAction> {
  static blink::PlatformNotificationActionType type(
      const blink::PlatformNotificationAction& action) {
    return action.type;
  }

  static const std::string& action(
      const blink::PlatformNotificationAction& action) {
    return action.action;
  }

  static const base::string16& title(
      const blink::PlatformNotificationAction& action) {
    return action.title;
  }

  static const GURL& icon(const blink::PlatformNotificationAction& action) {
    return action.icon;
  }

  static const base::Optional<base::string16>& placeholder(
      const blink::PlatformNotificationAction& action) {
    return action.placeholder.as_optional_string16();
  }

  static bool Read(
      blink::mojom::NotificationActionDataView notification_action,
      blink::PlatformNotificationAction* platform_notification_action);
};

template <>
struct BLINK_COMMON_EXPORT StructTraits<blink::mojom::NotificationDataDataView,
                                        blink::PlatformNotificationData> {
  static const base::string16& title(
      const blink::PlatformNotificationData& data) {
    return data.title;
  }

  static blink::mojom::NotificationDirection direction(
      const blink::PlatformNotificationData& data) {
    return data.direction;
  }

  static const std::string& lang(const blink::PlatformNotificationData& data) {
    return data.lang;
  }

  static const base::string16& body(
      const blink::PlatformNotificationData& data) {
    return data.body;
  }

  static const std::string& tag(const blink::PlatformNotificationData& data) {
    return data.tag;
  }

  static const GURL& image(const blink::PlatformNotificationData& data) {
    return data.image;
  }

  static const GURL& icon(const blink::PlatformNotificationData& data) {
    return data.icon;
  }

  static const GURL& badge(const blink::PlatformNotificationData& data) {
    return data.badge;
  }

  static const base::span<const int32_t> vibration_pattern(
      const blink::PlatformNotificationData& data) {
    // TODO(https://crbug.com/798466): Store as int32s to avoid this cast.
    return base::make_span(
        reinterpret_cast<const int32_t*>(data.vibration_pattern.data()),
        data.vibration_pattern.size());
  }

  static double timestamp(const blink::PlatformNotificationData& data) {
    return data.timestamp.ToJsTime();
  }

  static bool renotify(const blink::PlatformNotificationData& data) {
    return data.renotify;
  }

  static bool silent(const blink::PlatformNotificationData& data) {
    return data.silent;
  }

  static bool require_interaction(const blink::PlatformNotificationData& data) {
    return data.require_interaction;
  }

  static const base::span<const uint8_t> data(
      const blink::PlatformNotificationData& data) {
    // TODO(https://crbug.com/798466): Align data types to avoid this cast.
    return base::make_span(reinterpret_cast<const uint8_t*>(data.data.data()),
                           data.data.size());
  }

  static const std::vector<blink::PlatformNotificationAction>& actions(
      const blink::PlatformNotificationData& data) {
    return data.actions;
  }

  static base::Optional<base::Time> show_trigger_timestamp(
      const blink::PlatformNotificationData& data) {
    return data.show_trigger_timestamp;
  }

  static bool Read(blink::mojom::NotificationDataDataView notification_data,
                   blink::PlatformNotificationData* platform_notification_data);
};

template <>
struct BLINK_COMMON_EXPORT
    StructTraits<blink::mojom::NotificationResourcesDataView,
                 blink::NotificationResources> {
  static const SkBitmap& image(const blink::NotificationResources& resources) {
    return resources.image;
  }

  static const SkBitmap& icon(const blink::NotificationResources& resources) {
    return resources.notification_icon;
  }

  static const SkBitmap& badge(const blink::NotificationResources& resources) {
    return resources.badge;
  }

  static const std::vector<SkBitmap>& action_icons(
      const blink::NotificationResources& resources) {
    return resources.action_icons;
  }

  static bool Read(blink::mojom::NotificationResourcesDataView in,
                   blink::NotificationResources* out);
};

}  // namespace mojo

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_NOTIFICATIONS_NOTIFICATION_MOJOM_TRAITS_H_
