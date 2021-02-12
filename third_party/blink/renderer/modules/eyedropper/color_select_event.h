// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_EYEDROPPER_COLOR_SELECT_EVENT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_EYEDROPPER_COLOR_SELECT_EVENT_H_

#include "third_party/blink/renderer/bindings/modules/v8/v8_color_select_event_init.h"
#include "third_party/blink/renderer/core/events/pointer_event.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {

class ColorSelectEvent final : public PointerEvent {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit ColorSelectEvent(const AtomicString& type,
                            const ColorSelectEventInit* initializer,
                            base::TimeTicks platform_time_stamp,
                            SyntheticEventType synthetic_event_type,
                            WebMenuSourceType menu_source_type);

  static ColorSelectEvent* Create(
      const AtomicString& type,
      const ColorSelectEventInit* initializer,
      base::TimeTicks platform_time_stamp = base::TimeTicks::Now(),
      SyntheticEventType synthetic_event_type = kRealOrIndistinguishable,
      WebMenuSourceType menu_source_type = kMenuSourceNone);

  String value() const;

  void Trace(Visitor*) const override;

 private:
  String value_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_EYEDROPPER_COLOR_SELECT_EVENT_H_
