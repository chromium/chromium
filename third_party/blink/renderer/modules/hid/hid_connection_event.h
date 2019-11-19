// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_HID_HID_CONNECTION_EVENT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_HID_HID_CONNECTION_EVENT_H_

#include "third_party/blink/renderer/modules/event_modules.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

class HIDConnectionEventInit;
class HIDDevice;

class HIDConnectionEvent final : public Event {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static HIDConnectionEvent* Create(const AtomicString& type,
                                    const HIDConnectionEventInit*);
  static HIDConnectionEvent* Create(const AtomicString& type, HIDDevice*);

  HIDConnectionEvent(const AtomicString& type, const HIDConnectionEventInit*);
  HIDConnectionEvent(const AtomicString& type, HIDDevice*);

  HIDDevice* device() const { return nullptr; }

  void Trace(blink::Visitor*) override;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_HID_HID_CONNECTION_EVENT_H_
