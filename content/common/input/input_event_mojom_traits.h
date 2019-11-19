// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_INPUT_INPUT_EVENT_MOJOM_TRAITS_H_
#define CONTENT_COMMON_INPUT_INPUT_EVENT_MOJOM_TRAITS_H_

#include "content/common/input/input_handler.mojom.h"

namespace content {
class InputEvent;
}

namespace mojo {

using InputEventUniquePtr = std::unique_ptr<content::InputEvent>;

template <>
struct StructTraits<content::mojom::EventDataView, InputEventUniquePtr> {
  static blink::WebInputEvent::Type type(const InputEventUniquePtr& event) {
    return event->web_event->GetType();
  }

  static int32_t modifiers(const InputEventUniquePtr& event) {
    return event->web_event->GetModifiers();
  }

  static base::TimeTicks timestamp(const InputEventUniquePtr& event) {
    return event->web_event->TimeStamp();
  }

  static const ui::LatencyInfo& latency(const InputEventUniquePtr& event) {
    return event->latency_info;
  }

  static content::mojom::KeyDataPtr key_data(const InputEventUniquePtr& event);
  static content::mojom::PointerDataPtr pointer_data(
      const InputEventUniquePtr& event);
  static content::mojom::GestureDataPtr gesture_data(
      const InputEventUniquePtr& event);
  static content::mojom::TouchDataPtr touch_data(
      const InputEventUniquePtr& event);

  static bool Read(content::mojom::EventDataView r, InputEventUniquePtr* out);
};

}  // namespace mojo

#endif  // CONTENT_COMMON_INPUT_INPUT_EVENT_MOJOM_TRAITS_H_
