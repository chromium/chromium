// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/accessibility/aria_notification.h"

#include "ui/accessibility/ax_enums.mojom-blink.h"

namespace blink {

namespace {

ax::mojom::blink::AriaNotificationInterrupt AsEnum(
    const V8AriaNotifyInterrupt& interrupt) {
  switch (interrupt.AsEnum()) {
    case V8AriaNotifyInterrupt::Enum::kNone:
      return ax::mojom::blink::AriaNotificationInterrupt::kNone;
    case V8AriaNotifyInterrupt::Enum::kAll:
      return ax::mojom::blink::AriaNotificationInterrupt::kAll;
    case V8AriaNotifyInterrupt::Enum::kPending:
      return ax::mojom::blink::AriaNotificationInterrupt::kPending;
  }
  NOTREACHED();
}

ax::mojom::blink::AriaNotificationPriority AsEnum(
    const V8AriaNotifyPriority& priority) {
  switch (priority.AsEnum()) {
    case V8AriaNotifyPriority::Enum::kNone:
      return ax::mojom::blink::AriaNotificationPriority::kNone;
    case V8AriaNotifyPriority::Enum::kImportant:
      return ax::mojom::blink::AriaNotificationPriority::kImportant;
  }
  NOTREACHED();
}

}  // namespace

AriaNotification::AriaNotification(const String& announcement,
                                   const AriaNotificationOptions* options)
    : announcement_(announcement),
      notification_id_(options->notificationId()),
      interrupt_(AsEnum(options->interrupt())),
      priority_(AsEnum(options->priority())) {}

void AriaNotifications::Add(const String& announcement,
                            const AriaNotificationOptions* options) {
  notifications_.emplace_back(announcement, options);
}

}  // namespace blink
