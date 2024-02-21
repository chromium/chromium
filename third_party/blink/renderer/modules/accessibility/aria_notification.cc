// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/accessibility/aria_notification.h"

namespace blink {
AriaNotification::AriaNotification(Node* node,
                                   const String& announcement,
                                   const AriaNotificationOptions* options)
    : node_(node),
      announcement_(announcement),
      notification_id_(options->notificationId()) {}

}  // namespace blink
