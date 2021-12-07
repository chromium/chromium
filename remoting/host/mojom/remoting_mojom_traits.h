// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_MOJOM_REMOTING_MOJOM_TRAITS_H_
#define REMOTING_HOST_MOJOM_REMOTING_MOJOM_TRAITS_H_

#include <stddef.h>
#include <string>

#include "mojo/public/cpp/base/byte_string_mojom_traits.h"
#include "mojo/public/cpp/bindings/array_traits_protobuf.h"
#include "mojo/public/cpp/bindings/enum_traits.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "remoting/host/mojom/desktop_session.mojom-shared.h"
#include "remoting/host/mojom/wrapped_primitives.mojom-shared.h"
#include "remoting/proto/event.pb.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/geometry/mojom/geometry_mojom_traits.h"

namespace mojo {

template <>
class mojo::StructTraits<remoting::mojom::BoolDataView, bool> {
 public:
  static bool value(bool value) { return value; }

  static bool Read(remoting::mojom::BoolDataView data_view, bool* out_value) {
    *out_value = data_view.value();
    return true;
  }
};

template <>
class mojo::StructTraits<remoting::mojom::FloatDataView, float> {
 public:
  static float value(float value) { return value; }

  static bool Read(remoting::mojom::FloatDataView data_view, float* out_value) {
    *out_value = data_view.value();
    return true;
  }
};

template <>
class mojo::StructTraits<remoting::mojom::Int32DataView, int32_t> {
 public:
  static int32_t value(int32_t value) { return value; }

  static bool Read(remoting::mojom::Int32DataView data_view,
                   int32_t* out_value) {
    *out_value = data_view.value();
    return true;
  }
};

template <>
struct EnumTraits<remoting::mojom::MouseButton,
                  ::remoting::protocol::MouseEvent::MouseButton> {
  static remoting::mojom::MouseButton ToMojom(
      ::remoting::protocol::MouseEvent::MouseButton input) {
    switch (input) {
      case ::remoting::protocol::MouseEvent::BUTTON_UNDEFINED:
        return remoting::mojom::MouseButton::kUndefined;
      case ::remoting::protocol::MouseEvent::BUTTON_LEFT:
        return remoting::mojom::MouseButton::kLeft;
      case ::remoting::protocol::MouseEvent::BUTTON_MIDDLE:
        return remoting::mojom::MouseButton::kMiddle;
      case ::remoting::protocol::MouseEvent::BUTTON_RIGHT:
        return remoting::mojom::MouseButton::kRight;
      case ::remoting::protocol::MouseEvent::BUTTON_BACK:
        return remoting::mojom::MouseButton::kBack;
      case ::remoting::protocol::MouseEvent::BUTTON_FORWARD:
        return remoting::mojom::MouseButton::kForward;
      case ::remoting::protocol::MouseEvent::BUTTON_MAX:
        break;
    }

    NOTREACHED();
    return remoting::mojom::MouseButton::kUndefined;
  }

  static bool FromMojom(remoting::mojom::MouseButton input,
                        ::remoting::protocol::MouseEvent::MouseButton* out) {
    switch (input) {
      case remoting::mojom::MouseButton::kUndefined:
        *out = ::remoting::protocol::MouseEvent::BUTTON_UNDEFINED;
        return true;
      case remoting::mojom::MouseButton::kLeft:
        *out = ::remoting::protocol::MouseEvent::BUTTON_LEFT;
        return true;
      case remoting::mojom::MouseButton::kMiddle:
        *out = ::remoting::protocol::MouseEvent::BUTTON_MIDDLE;
        return true;
      case remoting::mojom::MouseButton::kRight:
        *out = ::remoting::protocol::MouseEvent::BUTTON_RIGHT;
        return true;
      case remoting::mojom::MouseButton::kBack:
        *out = ::remoting::protocol::MouseEvent::BUTTON_BACK;
        return true;
      case remoting::mojom::MouseButton::kForward:
        *out = ::remoting::protocol::MouseEvent::BUTTON_FORWARD;
        return true;
    }

    NOTREACHED();
    return false;
  }
};

template <>
class mojo::StructTraits<remoting::mojom::ClipboardEventDataView,
                         ::remoting::protocol::ClipboardEvent> {
 public:
  static const std::string& mime_type(
      const ::remoting::protocol::ClipboardEvent& event) {
    return event.mime_type();
  }

  static const std::string& data(
      const ::remoting::protocol::ClipboardEvent& event) {
    return event.data();
  }

  static bool Read(remoting::mojom::ClipboardEventDataView data_view,
                   ::remoting::protocol::ClipboardEvent* out_event);
};

template <>
class mojo::StructTraits<remoting::mojom::KeyEventDataView,
                         ::remoting::protocol::KeyEvent> {
 public:
  static bool pressed(const ::remoting::protocol::KeyEvent& event) {
    return event.pressed();
  }

  static uint32_t usb_keycode(const ::remoting::protocol::KeyEvent& event) {
    return event.usb_keycode();
  }

  static uint32_t lock_states(const ::remoting::protocol::KeyEvent& event) {
    return event.lock_states();
  }

  static absl::optional<bool> caps_lock_state(
      const ::remoting::protocol::KeyEvent& event) {
    if (event.has_caps_lock_state()) {
      return event.caps_lock_state();
    }
    return absl::nullopt;
  }

  static absl::optional<bool> num_lock_state(
      const ::remoting::protocol::KeyEvent& event) {
    if (event.has_num_lock_state()) {
      return event.num_lock_state();
    }
    return absl::nullopt;
  }

  static bool Read(remoting::mojom::KeyEventDataView data_view,
                   ::remoting::protocol::KeyEvent* out_event);
};

template <>
class mojo::StructTraits<remoting::mojom::MouseEventDataView,
                         ::remoting::protocol::MouseEvent> {
 public:
  static absl::optional<int32_t> x(
      const ::remoting::protocol::MouseEvent& event) {
    if (event.has_x()) {
      return event.x();
    }
    return absl::nullopt;
  }

  static absl::optional<int32_t> y(
      const ::remoting::protocol::MouseEvent& event) {
    if (event.has_y()) {
      return event.y();
    }
    return absl::nullopt;
  }

  static ::remoting::protocol::MouseEvent::MouseButton button(
      const ::remoting::protocol::MouseEvent& event) {
    if (event.has_button()) {
      return event.button();
    }
    return ::remoting::protocol::MouseEvent::BUTTON_UNDEFINED;
  }

  static absl::optional<bool> button_down(
      const ::remoting::protocol::MouseEvent& event) {
    if (event.has_button_down()) {
      DCHECK(event.has_button());
      return event.button_down();
    }
    return absl::nullopt;
  }

  static absl::optional<float> wheel_delta_x(
      const ::remoting::protocol::MouseEvent& event) {
    if (event.has_wheel_delta_x()) {
      return event.wheel_delta_x();
    }
    return absl::nullopt;
  }

  static absl::optional<float> wheel_delta_y(
      const ::remoting::protocol::MouseEvent& event) {
    if (event.has_wheel_delta_y()) {
      return event.wheel_delta_y();
    }
    return absl::nullopt;
  }

  static absl::optional<float> wheel_ticks_x(
      const ::remoting::protocol::MouseEvent& event) {
    if (event.wheel_ticks_x()) {
      return event.wheel_ticks_x();
    }
    return absl::nullopt;
  }

  static absl::optional<float> wheel_ticks_y(
      const ::remoting::protocol::MouseEvent& event) {
    if (event.wheel_ticks_y()) {
      return event.wheel_ticks_y();
    }
    return absl::nullopt;
  }

  static absl::optional<int32_t> delta_x(
      const ::remoting::protocol::MouseEvent& event) {
    if (event.has_delta_x()) {
      return event.delta_x();
    }
    return absl::nullopt;
  }

  static absl::optional<int32_t> delta_y(
      const ::remoting::protocol::MouseEvent& event) {
    if (event.has_delta_y()) {
      return event.delta_y();
    }
    return absl::nullopt;
  }

  static bool Read(remoting::mojom::MouseEventDataView data_view,
                   ::remoting::protocol::MouseEvent* out_event);
};

template <>
class mojo::StructTraits<remoting::mojom::TextEventDataView,
                         ::remoting::protocol::TextEvent> {
 public:
  static const std::string& text(const ::remoting::protocol::TextEvent& event) {
    return event.text();
  }

  static bool Read(remoting::mojom::TextEventDataView data_view,
                   ::remoting::protocol::TextEvent* out_event);
};

template <>
class mojo::StructTraits<remoting::mojom::TouchEventPointDataView,
                         ::remoting::protocol::TouchEventPoint> {
 public:
  static uint32_t id(const ::remoting::protocol::TouchEventPoint& event) {
    return event.id();
  }

  static gfx::PointF position(
      const ::remoting::protocol::TouchEventPoint& event) {
    return {event.x(), event.y()};
  }

  static gfx::PointF radius(
      const ::remoting::protocol::TouchEventPoint& event) {
    return {event.radius_x(), event.radius_y()};
  }

  static float angle(const ::remoting::protocol::TouchEventPoint& event) {
    return event.angle();
  }

  static float pressure(const ::remoting::protocol::TouchEventPoint& event) {
    return event.pressure();
  }

  static bool Read(remoting::mojom::TouchEventPointDataView data_view,
                   ::remoting::protocol::TouchEventPoint* out_event);
};

template <>
struct EnumTraits<remoting::mojom::TouchEventType,
                  ::remoting::protocol::TouchEvent::TouchEventType> {
  static remoting::mojom::TouchEventType ToMojom(
      ::remoting::protocol::TouchEvent::TouchEventType input) {
    switch (input) {
      case ::remoting::protocol::TouchEvent::TOUCH_POINT_UNDEFINED:
        return remoting::mojom::TouchEventType::kUndefined;
      case ::remoting::protocol::TouchEvent::TOUCH_POINT_START:
        return remoting::mojom::TouchEventType::kStart;
      case ::remoting::protocol::TouchEvent::TOUCH_POINT_MOVE:
        return remoting::mojom::TouchEventType::kMove;
      case ::remoting::protocol::TouchEvent::TOUCH_POINT_END:
        return remoting::mojom::TouchEventType::kEnd;
      case ::remoting::protocol::TouchEvent::TOUCH_POINT_CANCEL:
        return remoting::mojom::TouchEventType::kCancel;
    }

    NOTREACHED();
    return remoting::mojom::TouchEventType::kUndefined;
  }

  static bool FromMojom(remoting::mojom::TouchEventType input,
                        ::remoting::protocol::TouchEvent::TouchEventType* out) {
    switch (input) {
      case remoting::mojom::TouchEventType::kUndefined:
        *out = ::remoting::protocol::TouchEvent::TOUCH_POINT_UNDEFINED;
        return true;
      case remoting::mojom::TouchEventType::kStart:
        *out = ::remoting::protocol::TouchEvent::TOUCH_POINT_START;
        return true;
      case remoting::mojom::TouchEventType::kMove:
        *out = ::remoting::protocol::TouchEvent::TOUCH_POINT_MOVE;
        return true;
      case remoting::mojom::TouchEventType::kEnd:
        *out = ::remoting::protocol::TouchEvent::TOUCH_POINT_END;
        return true;
      case remoting::mojom::TouchEventType::kCancel:
        *out = ::remoting::protocol::TouchEvent::TOUCH_POINT_CANCEL;
        return true;
    }

    NOTREACHED();
    return false;
  }
};

template <>
class mojo::StructTraits<remoting::mojom::TouchEventDataView,
                         ::remoting::protocol::TouchEvent> {
 public:
  static ::remoting::protocol::TouchEvent::TouchEventType event_type(
      const ::remoting::protocol::TouchEvent& event) {
    return event.event_type();
  }

  static const ::google::protobuf::RepeatedPtrField<
      ::remoting::protocol::TouchEventPoint>
  touch_points(const ::remoting::protocol::TouchEvent& event) {
    return event.touch_points();
  }

  static bool Read(remoting::mojom::TouchEventDataView data_view,
                   ::remoting::protocol::TouchEvent* out_event);
};

}  // namespace mojo

#endif  // REMOTING_HOST_MOJOM_REMOTING_MOJOM_TRAITS_H_
