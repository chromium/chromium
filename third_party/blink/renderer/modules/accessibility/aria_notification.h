// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_ACCESSIBILITY_ARIA_NOTIFICATION_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_ACCESSIBILITY_ARIA_NOTIFICATION_H_

#include "third_party/blink/renderer/bindings/core/v8/v8_aria_notification_options.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

enum class AriaNotificationInsertionMode { kQueue, kStack, kClear };

class AriaNotification final : public GarbageCollected<AriaNotification> {
 public:
  AriaNotification(Node*,
                   const String announcement,
                   const AriaNotificationOptions* options);

  void Trace(Visitor* visitor) const { visitor->Trace(node_); }

 private:
  Member<Node> node_;
  const String announcement_;
  AriaNotificationInsertionMode insertion_mode_;
  bool interrupt_current_;
  bool prevent_interrupt_;
  String label_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_ACCESSIBILITY_ARIA_NOTIFICATION_H_
