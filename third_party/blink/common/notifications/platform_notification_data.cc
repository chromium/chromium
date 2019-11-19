// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/notifications/platform_notification_data.h"
#include "third_party/blink/public/mojom/notifications/notification.mojom.h"

namespace blink {

PlatformNotificationAction::PlatformNotificationAction() {}

PlatformNotificationAction::PlatformNotificationAction(
    const PlatformNotificationAction& other) = default;

PlatformNotificationAction::~PlatformNotificationAction() {}

PlatformNotificationData::PlatformNotificationData()
    : direction(mojom::NotificationDirection::LEFT_TO_RIGHT) {}

PlatformNotificationData::PlatformNotificationData(
    const PlatformNotificationData& other) = default;

PlatformNotificationData::~PlatformNotificationData() {}

}  // namespace blink
