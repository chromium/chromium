// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/accessibility/aria_notification.h"

namespace blink {
AriaNotification::AriaNotification(Node* node,
                                   const String announcement,
                                   const AriaNotificationOptions* options)
    : node_(node), announcement_(announcement) {
  label_ = options->label();
  interrupt_current_ = options->interruptCurrent();
  prevent_interrupt_ = options->preventInterrupt();

  if (options->insertionMode() == "queue") {
    insertion_mode_ = AriaNotificationInsertionMode::kQueue;
  } else if (options->insertionMode() == "stack") {
    insertion_mode_ = AriaNotificationInsertionMode::kStack;
  } else if (options->insertionMode() == "clear") {
    insertion_mode_ = AriaNotificationInsertionMode::kClear;
  }
}

}  // namespace blink
