// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_HID_HID_CONNECTION_EVENT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_HID_HID_CONNECTION_EVENT_H_

#include "third_party/blink/renderer/modules/event_modules.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

class HIDConnectionEventInit;
class DOMWrapperWorld;
class HIDDevice;

class HIDConnectionEvent final : public Event {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static HIDConnectionEvent* Create(const AtomicString& type,
                                    const HIDConnectionEventInit*);
  static HIDConnectionEvent* Create(const AtomicString& type,
                                    HIDDevice*,
                                    const DOMWrapperWorld*);

  HIDConnectionEvent(const AtomicString& type, const HIDConnectionEventInit*);
  HIDConnectionEvent(const AtomicString& type,
                     HIDDevice*,
                     const DOMWrapperWorld*);

  HIDDevice* device() const { return device_.Get(); }

  // Returns true if the event is allowed to be dispatched in the given `world`.
  // Restricts dispatch to the world the event was created in, unless `world_`
  // is null.
  bool CanBeDispatchedInWorld(const DOMWrapperWorld& world) const override;
  void Trace(Visitor*) const override;

 private:
  Member<HIDDevice> device_;
  // The V8 world in which this event's device wrapper was created.
  // Used to restrict the event dispatch to this world only.
  // When `WebHIDWorldIsolatedCache` is disabled, this is `nullptr`,
  // in which case `CanBeDispatchedInWorld` will always return true (legacy
  // behavior).
  Member<const DOMWrapperWorld> world_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_HID_HID_CONNECTION_EVENT_H_
