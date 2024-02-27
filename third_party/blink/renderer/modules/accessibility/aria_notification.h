// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_ACCESSIBILITY_ARIA_NOTIFICATION_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_ACCESSIBILITY_ARIA_NOTIFICATION_H_

#include "third_party/blink/renderer/bindings/core/v8/v8_aria_notification_options.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "ui/accessibility/ax_enums.mojom-blink-forward.h"

namespace blink {

class AriaNotification {
  USING_FAST_MALLOC(AriaNotification);

 public:
  using AriaNotificationInterrupt = ax::mojom::blink::AriaNotificationInterrupt;
  using AriaNotificationPriority = ax::mojom::blink::AriaNotificationPriority;

  AriaNotification(const String& announcement,
                   const AriaNotificationOptions* options);

  const String& Announcement() const { return announcement_; }
  const String& NotificationId() const { return notification_id_; }
  AriaNotificationInterrupt Interrupt() const { return interrupt_; }
  AriaNotificationPriority Priority() const { return priority_; }

 private:
  String announcement_;
  String notification_id_;
  AriaNotificationInterrupt interrupt_;
  AriaNotificationPriority priority_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_ACCESSIBILITY_ARIA_NOTIFICATION_H_
