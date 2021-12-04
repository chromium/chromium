// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/mojom/remoting_mojom_traits.h"

namespace mojo {

// static
bool mojo::StructTraits<remoting::mojom::ClipboardEventDataView,
                        ::remoting::protocol::ClipboardEvent>::
    Read(remoting::mojom::ClipboardEventDataView data_view,
         ::remoting::protocol::ClipboardEvent* out_event) {
  std::string mime_type;
  if (!data_view.ReadMimeType(&mime_type)) {
    return false;
  }
  out_event->set_mime_type(std::move(mime_type));
  std::string data;
  if (!data_view.ReadData(&data)) {
    return false;
  }
  out_event->set_data(std::move(data));
  return true;
}

// static
bool mojo::StructTraits<remoting::mojom::KeyEventDataView,
                        ::remoting::protocol::KeyEvent>::
    Read(remoting::mojom::KeyEventDataView data_view,
         ::remoting::protocol::KeyEvent* out_event) {
  out_event->set_pressed(data_view.pressed());
  out_event->set_usb_keycode(data_view.usb_keycode());
  out_event->set_lock_states(data_view.lock_states());

  absl::optional<bool> caps_lock_state;
  if (!data_view.ReadCapsLockState(&caps_lock_state)) {
    return false;
  }
  if (caps_lock_state.has_value()) {
    out_event->set_caps_lock_state(caps_lock_state.value());
  }

  absl::optional<bool> num_lock_state;
  if (!data_view.ReadNumLockState(&num_lock_state)) {
    return false;
  }
  if (num_lock_state.has_value()) {
    out_event->set_num_lock_state(num_lock_state.value());
  }

  return true;
}

// static
bool mojo::StructTraits<remoting::mojom::MouseEventDataView,
                        ::remoting::protocol::MouseEvent>::
    Read(remoting::mojom::MouseEventDataView data_view,
         ::remoting::protocol::MouseEvent* out_event) {
  absl::optional<int32_t> x;
  if (!data_view.ReadX(&x)) {
    return false;
  }
  if (x.has_value()) {
    out_event->set_x(x.value());
  }

  absl::optional<int32_t> y;
  if (!data_view.ReadY(&y)) {
    return false;
  }
  if (y.has_value()) {
    out_event->set_y(y.value());
  }

  if (data_view.button() != remoting::mojom::MouseButton::kUndefined) {
    ::remoting::protocol::MouseEvent::MouseButton mouse_button;
    if (!EnumTraits<remoting::mojom::MouseButton,
                    ::remoting::protocol::MouseEvent::MouseButton>::
            FromMojom(data_view.button(), &mouse_button)) {
      return false;
    }
    out_event->set_button(mouse_button);
  }

  absl::optional<bool> button_down;
  if (!data_view.ReadButtonDown(&button_down)) {
    return false;
  }
  if (button_down.has_value()) {
    out_event->set_button_down(button_down.value());
  }

  absl::optional<float> wheel_delta_x;
  if (!data_view.ReadWheelDeltaX(&wheel_delta_x)) {
    return false;
  }
  if (wheel_delta_x.has_value()) {
    out_event->set_wheel_delta_x(wheel_delta_x.value());
  }

  absl::optional<float> wheel_delta_y;
  if (!data_view.ReadWheelDeltaY(&wheel_delta_y)) {
    return false;
  }
  if (wheel_delta_y.has_value()) {
    out_event->set_wheel_delta_y(wheel_delta_y.value());
  }

  absl::optional<float> wheel_ticks_x;
  if (!data_view.ReadWheelTicksX(&wheel_ticks_x)) {
    return false;
  }
  if (wheel_ticks_x.has_value()) {
    out_event->set_wheel_ticks_x(wheel_ticks_x.value());
  }

  absl::optional<float> wheel_ticks_y;
  if (!data_view.ReadWheelTicksY(&wheel_ticks_y)) {
    return false;
  }
  if (wheel_ticks_y.has_value()) {
    out_event->set_wheel_ticks_y(wheel_ticks_y.value());
  }

  absl::optional<int32_t> delta_x;
  if (!data_view.ReadDeltaX(&delta_x)) {
    return false;
  }
  if (delta_x.has_value()) {
    out_event->set_delta_x(delta_x.value());
  }

  absl::optional<int32_t> delta_y;
  if (!data_view.ReadDeltaY(&delta_y)) {
    return false;
  }
  if (delta_y.has_value()) {
    out_event->set_delta_y(delta_y.value());
  }

  return true;
}

// static
bool mojo::StructTraits<remoting::mojom::TextEventDataView,
                        ::remoting::protocol::TextEvent>::
    Read(remoting::mojom::TextEventDataView data_view,
         ::remoting::protocol::TextEvent* out_event) {
  std::string text;
  if (!data_view.ReadText(&text)) {
    return false;
  }
  out_event->set_text(std::move(text));
  return true;
}

// static
bool mojo::StructTraits<remoting::mojom::TouchEventPointDataView,
                        ::remoting::protocol::TouchEventPoint>::
    Read(remoting::mojom::TouchEventPointDataView data_view,
         ::remoting::protocol::TouchEventPoint* out_event) {
  out_event->set_id(data_view.id());
  gfx::PointF position;
  if (!data_view.ReadPosition(&position)) {
    return false;
  }
  out_event->set_x(position.x());
  out_event->set_y(position.y());
  gfx::PointF radius;
  if (!data_view.ReadRadius(&radius)) {
    return false;
  }
  out_event->set_radius_x(radius.x());
  out_event->set_radius_y(radius.y());
  out_event->set_angle(data_view.angle());
  out_event->set_pressure(data_view.pressure());
  return true;
}

// static
bool mojo::StructTraits<remoting::mojom::TouchEventDataView,
                        ::remoting::protocol::TouchEvent>::
    Read(remoting::mojom::TouchEventDataView data_view,
         ::remoting::protocol::TouchEvent* out_event) {
  ::remoting::protocol::TouchEvent::TouchEventType touch_event_type;
  if (!EnumTraits<remoting::mojom::TouchEventType,
                  ::remoting::protocol::TouchEvent::TouchEventType>::
          FromMojom(data_view.event_type(), &touch_event_type)) {
    return false;
  }
  out_event->set_event_type(touch_event_type);

  if (!data_view.ReadTouchPoints(out_event->mutable_touch_points())) {
    return false;
  }

  return true;
}

}  // namespace mojo
