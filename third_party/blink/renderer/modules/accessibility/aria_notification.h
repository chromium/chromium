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
 public:
  AriaNotification(const String& announcement,
                   const AriaNotificationOptions* options);

 private:
  String announcement_;
  String notification_id_;
  ax::mojom::blink::AriaNotificationInterrupt interrupt_;
  ax::mojom::blink::AriaNotificationPriority priority_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_ACCESSIBILITY_ARIA_NOTIFICATION_H_
