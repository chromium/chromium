// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_EVENTS_POINTER_EVENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_EVENTS_POINTER_EVENT_H_

#include "third_party/blink/renderer/core/events/mouse_event.h"
#include "third_party/blink/renderer/core/events/pointer_event_init.h"

namespace blink {

class CORE_EXPORT PointerEvent final : public MouseEvent {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static PointerEvent* Create(
      const AtomicString& type,
      const PointerEventInit* initializer,
      base::TimeTicks platform_time_stamp,
      MouseEvent::SyntheticEventType synthetic_event_type =
          kRealOrIndistinguishable,
      WebMenuSourceType menu_source_type = kMenuSourceNone) {
    return MakeGarbageCollected<PointerEvent>(
        type, initializer, platform_time_stamp, synthetic_event_type,
        menu_source_type);
  }
  static PointerEvent* Create(const AtomicString& type,
                              const PointerEventInit* initializer) {
    return PointerEvent::Create(type, initializer, base::TimeTicks::Now());
  }

  PointerEvent(const AtomicString&,
               const PointerEventInit*,
               base::TimeTicks platform_time_stamp,
               MouseEvent::SyntheticEventType synthetic_event_type,
               WebMenuSourceType menu_source_type);

  PointerId pointerId() const { return pointer_id_; }
  double width() const { return width_; }
  double height() const { return height_; }
  float pressure() const { return pressure_; }
  int32_t tiltX() const { return tilt_x_; }
  int32_t tiltY() const { return tilt_y_; }
  float tangentialPressure() const { return tangential_pressure_; }
  int32_t twist() const { return twist_; }
  const String& pointerType() const { return pointer_type_; }
  bool isPrimary() const { return is_primary_; }

  int16_t button() const override { return RawButton(); }
  bool IsMouseEvent() const override;
  bool IsPointerEvent() const override;

  double screenX() const override { return screen_location_.X(); }
  double screenY() const override { return screen_location_.Y(); }
  double clientX() const override { return client_location_.X(); }
  double clientY() const override { return client_location_.Y(); }
  double pageX() const override { return page_location_.X(); }
  double pageY() const override { return page_location_.Y(); }

  double offsetX() override;
  double offsetY() override;

  void ReceivedTarget() override;

  // Always return null for fromElement and toElement because these fields
  // (inherited from MouseEvents) are non-standard.
  Node* fromElement() const final;
  Node* toElement() const final;

  HeapVector<Member<PointerEvent>> getCoalescedEvents();
  HeapVector<Member<PointerEvent>> getPredictedEvents();
  base::TimeTicks OldestPlatformTimeStamp() const;

  DispatchEventResult DispatchEvent(EventDispatcher&) override;

  void Trace(blink::Visitor*) override;

 private:
  PointerId pointer_id_;
  double width_;
  double height_;
  float pressure_;
  int32_t tilt_x_;
  int32_t tilt_y_;
  float tangential_pressure_;
  int32_t twist_;
  String pointer_type_;
  bool is_primary_;

  bool coalesced_events_targets_dirty_;
  bool predicted_events_targets_dirty_;

  HeapVector<Member<PointerEvent>> coalesced_events_;

  HeapVector<Member<PointerEvent>> predicted_events_;
};

DEFINE_EVENT_TYPE_CASTS(PointerEvent);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_EVENTS_POINTER_EVENT_H_
