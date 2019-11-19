// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_EVENTS_DRAG_EVENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_EVENTS_DRAG_EVENT_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/events/drag_event_init.h"
#include "third_party/blink/renderer/core/events/mouse_event.h"

namespace blink {

class DataTransfer;

class CORE_EXPORT DragEvent final : public MouseEvent {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static DragEvent* Create() { return MakeGarbageCollected<DragEvent>(); }

  static DragEvent* Create(const AtomicString& type,
                           const DragEventInit* initializer,
                           base::TimeTicks platform_time_stamp,
                           SyntheticEventType synthetic_event_type) {
    return MakeGarbageCollected<DragEvent>(
        type, initializer, platform_time_stamp, synthetic_event_type);
  }

  static DragEvent* Create(const AtomicString& type,
                           const DragEventInit* initializer) {
    return MakeGarbageCollected<DragEvent>(
        type, initializer, base::TimeTicks::Now(), kRealOrIndistinguishable);
  }

  DragEvent();
  DragEvent(const AtomicString& type,
            const DragEventInit*,
            base::TimeTicks platform_time_stamp,
            SyntheticEventType);

  DataTransfer* getDataTransfer() const override {
    return IsDragEvent() ? data_transfer_.Get() : nullptr;
  }

  bool IsDragEvent() const override;
  bool IsMouseEvent() const override;

  DispatchEventResult DispatchEvent(EventDispatcher&) override;

  void Trace(blink::Visitor*) override;

 private:
  Member<DataTransfer> data_transfer_;
};

DEFINE_EVENT_TYPE_CASTS(DragEvent);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_EVENTS_DRAG_EVENT_H_
