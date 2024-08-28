// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/mojom/remoting_mojom_traits.h"

namespace mojo {

// static
bool mojo::StructTraits<remoting::mojom::AudioPacketDataView,
                        ::std::unique_ptr<::remoting::AudioPacket>>::
    Read(remoting::mojom::AudioPacketDataView data_view,
         ::std::unique_ptr<::remoting::AudioPacket>* out_packet) {
  auto packet = std::make_unique<::remoting::AudioPacket>();

  packet->set_timestamp(data_view.timestamp());

  if (!data_view.ReadData(packet->mutable_data())) {
    return false;
  }

  ::remoting::AudioPacket::Encoding encoding;
  if (!data_view.ReadEncoding(&encoding)) {
    return false;
  }
  packet->set_encoding(encoding);

  ::remoting::AudioPacket::SamplingRate sampling_rate;
  if (!data_view.ReadSamplingRate(&sampling_rate)) {
    return false;
  }
  packet->set_sampling_rate(sampling_rate);

  ::remoting::AudioPacket::BytesPerSample bytes_per_sample;
  if (!data_view.ReadBytesPerSample(&bytes_per_sample)) {
    return false;
  }
  packet->set_bytes_per_sample(bytes_per_sample);

  ::remoting::AudioPacket::Channels channels;
  if (!data_view.ReadChannels(&channels)) {
    return false;
  }
  packet->set_channels(channels);

  *out_packet = std::move(packet);

  return true;
}

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
bool mojo::StructTraits<remoting::mojom::DesktopCaptureOptionsDataView,
                        ::webrtc::DesktopCaptureOptions>::
    Read(remoting::mojom::DesktopCaptureOptionsDataView data_view,
         ::webrtc::DesktopCaptureOptions* out_options) {
  out_options->set_use_update_notifications(
      data_view.use_update_notifications());
  out_options->set_detect_updated_region(data_view.detect_updated_region());

#if BUILDFLAG(IS_WIN)
  out_options->set_allow_directx_capturer(data_view.allow_directx_capturer());
#endif  // BUILDFLAG(IS_WIN)

  return true;
}

// static
bool mojo::StructTraits<remoting::mojom::DesktopEnvironmentOptionsDataView,
                        ::remoting::DesktopEnvironmentOptions>::
    Read(remoting::mojom::DesktopEnvironmentOptionsDataView data_view,
         ::remoting::DesktopEnvironmentOptions* out_options) {
  out_options->set_enable_curtaining(data_view.enable_curtaining());
  out_options->set_enable_user_interface(data_view.enable_user_interface());
  out_options->set_enable_notifications(data_view.enable_notifications());
  out_options->set_terminate_upon_input(data_view.terminate_upon_input());
  out_options->set_enable_remote_webauthn(data_view.enable_remote_webauthn());

  if (!data_view.ReadDesktopCaptureOptions(
          out_options->desktop_capture_options())) {
    return false;
  }

  return true;
}

// static
bool mojo::StructTraits<
    remoting::mojom::DesktopRectDataView,
    ::webrtc::DesktopRect>::Read(remoting::mojom::DesktopRectDataView data_view,
                                 ::webrtc::DesktopRect* out_rect) {
  *out_rect = webrtc::DesktopRect::MakeLTRB(
      data_view.left(), data_view.top(), data_view.right(), data_view.bottom());
  return true;
}

// static
bool mojo::StructTraits<
    remoting::mojom::DesktopSizeDataView,
    ::webrtc::DesktopSize>::Read(remoting::mojom::DesktopSizeDataView data_view,
                                 ::webrtc::DesktopSize* out_size) {
  out_size->set(data_view.width(), data_view.height());
  return true;
}

// static
bool mojo::StructTraits<remoting::mojom::DesktopVectorDataView,
                        ::webrtc::DesktopVector>::
    Read(remoting::mojom::DesktopVectorDataView data_view,
         ::webrtc::DesktopVector* out_vector) {
  out_vector->set(data_view.x(), data_view.y());
  return true;
}

#if BUILDFLAG(IS_WIN)
// static
bool mojo::UnionTraits<
    remoting::mojom::FileChooserResultDataView,
    ::remoting::Result<base::FilePath,
                       ::remoting::protocol::FileTransfer_Error>>::
    Read(remoting::mojom::FileChooserResultDataView data_view,
         ::remoting::Result<base::FilePath,
                            ::remoting::protocol::FileTransfer_Error>*
             out_result) {
  switch (data_view.tag()) {
    case remoting::mojom::FileChooserResultDataView::Tag::kFilepath: {
      base::FilePath filepath;
      if (!data_view.ReadFilepath(&filepath)) {
        return false;
      }
      out_result->EmplaceSuccess(std::move(filepath));
      return true;
    }
    case remoting::mojom::FileChooserResultDataView::Tag::kError: {
      ::remoting::protocol::FileTransfer_Error error;
      if (!data_view.ReadError(&error)) {
        return false;
      }
      out_result->EmplaceError(std::move(error));
      return true;
    }
  }
}
#endif  // BUILDFLAG(IS_WIN)

// static
bool mojo::UnionTraits<
    remoting::mojom::ReadChunkResultDataView,
    ::remoting::Result<std::vector<uint8_t>,
                       ::remoting::protocol::FileTransfer_Error>>::
    Read(remoting::mojom::ReadChunkResultDataView data_view,
         ::remoting::Result<std::vector<uint8_t>,
                            ::remoting::protocol::FileTransfer_Error>*
             out_result) {
  switch (data_view.tag()) {
    case remoting::mojom::ReadChunkResultDataView::Tag::kData: {
      std::vector<uint8_t> data;
      if (!data_view.ReadData(&data)) {
        return false;
      }
      out_result->EmplaceSuccess(std::move(data));
      return true;
    }
    case remoting::mojom::ReadChunkResultDataView::Tag::kError: {
      ::remoting::protocol::FileTransfer_Error error;
      if (!data_view.ReadError(&error)) {
        return false;
      }
      out_result->EmplaceError(std::move(error));
      return true;
    }
  }
}

// static
bool mojo::StructTraits<remoting::mojom::FileTransferErrorDataView,
                        ::remoting::protocol::FileTransfer_Error>::
    Read(remoting::mojom::FileTransferErrorDataView data_view,
         ::remoting::protocol::FileTransfer_Error* out_error) {
  ::remoting::protocol::FileTransfer_Error_Type type;
  if (!data_view.ReadType(&type)) {
    return false;
  }
  out_error->set_type(type);

  std::optional<int32_t> api_error_code;
  if (!data_view.ReadApiErrorCode(&api_error_code)) {
    return false;
  }
  if (api_error_code) {
    out_error->set_api_error_code(*api_error_code);
  }

  std::string function;
  if (!data_view.ReadFunction(&function)) {
    return false;
  }
  out_error->set_function(std::move(function));

  std::string source_file;
  if (!data_view.ReadSourceFile(&source_file)) {
    return false;
  }
  out_error->set_source_file(std::move(source_file));

  out_error->set_line_number(data_view.line_number());

  return true;
}

// static
bool mojo::StructTraits<remoting::mojom::KeyboardLayoutDataView,
                        ::remoting::protocol::KeyboardLayout>::
    Read(remoting::mojom::KeyboardLayoutDataView data_view,
         ::remoting::protocol::KeyboardLayout* out_layout) {
  return data_view.ReadKeys(out_layout->mutable_keys());
}

// static
bool mojo::UnionTraits<remoting::mojom::KeyActionDataView,
                       ::remoting::protocol::KeyboardLayout_KeyAction>::
    Read(remoting::mojom::KeyActionDataView data_view,
         ::remoting::protocol::KeyboardLayout_KeyAction* out_action) {
  switch (data_view.tag()) {
    case remoting::mojom::KeyActionDataView::Tag::kFunction:
      ::remoting::protocol::LayoutKeyFunction function;
      if (!data_view.ReadFunction(&function)) {
        return false;
      }
      out_action->set_function(function);
      return true;
    case remoting::mojom::KeyActionDataView::Tag::kCharacter:
      std::string character;
      if (!data_view.ReadCharacter(&character)) {
        return false;
      }
      out_action->set_character(std::move(character));
      return true;
  }
}

// static
bool mojo::StructTraits<remoting::mojom::KeyBehaviorDataView,
                        ::remoting::protocol::KeyboardLayout::KeyBehavior>::
    Read(remoting::mojom::KeyBehaviorDataView data_view,
         ::remoting::protocol::KeyboardLayout::KeyBehavior* out_behavior) {
  return data_view.ReadActions(out_behavior->mutable_actions());
}

// static
bool mojo::StructTraits<remoting::mojom::KeyEventDataView,
                        ::remoting::protocol::KeyEvent>::
    Read(remoting::mojom::KeyEventDataView data_view,
         ::remoting::protocol::KeyEvent* out_event) {
  out_event->set_pressed(data_view.pressed());
  out_event->set_usb_keycode(data_view.usb_keycode());
  out_event->set_lock_states(data_view.lock_states());

  std::optional<bool> caps_lock_state;
  if (!data_view.ReadCapsLockState(&caps_lock_state)) {
    return false;
  }
  if (caps_lock_state.has_value()) {
    out_event->set_caps_lock_state(*caps_lock_state);
  }

  std::optional<bool> num_lock_state;
  if (!data_view.ReadNumLockState(&num_lock_state)) {
    return false;
  }
  if (num_lock_state.has_value()) {
    out_event->set_num_lock_state(*num_lock_state);
  }

  return true;
}

// static
bool mojo::StructTraits<
    remoting::mojom::MouseCursorDataView,
    ::webrtc::MouseCursor>::Read(remoting::mojom::MouseCursorDataView data_view,
                                 ::webrtc::MouseCursor* out_cursor) {
  ::webrtc::DesktopSize image_size;
  if (!data_view.ReadImageSize(&image_size)) {
    return false;
  }

  mojo::ArrayDataView<uint8_t> image_data;
  data_view.GetImageDataDataView(&image_data);

  base::CheckedNumeric<size_t> expected_image_data_size =
      ::webrtc::DesktopFrame::kBytesPerPixel * image_size.width() *
      image_size.height();
  if (!expected_image_data_size.IsValid()) {
    return false;
  }

  // ValueOrDie() won't CHECK since we've already verified the value is valid.
  if (image_data.size() != expected_image_data_size.ValueOrDie()) {
    return false;
  }

  ::webrtc::DesktopVector hotspot;
  if (!data_view.ReadHotspot(&hotspot)) {
    return false;
  }

  std::unique_ptr<::webrtc::DesktopFrame> new_frame(
      new ::webrtc::BasicDesktopFrame(image_size));
  memcpy(new_frame->data(), image_data.data(), image_data.size());

  // ::webrtc::MouseCursor methods take a raw pointer *and* take ownership.
  // TODO(joedow): Update webrtc::MouseCursor to use std::unique_ptr.
  out_cursor->set_image(new_frame.release());
  out_cursor->set_hotspot(hotspot);

  return true;
}

// static
bool mojo::StructTraits<remoting::mojom::MouseEventDataView,
                        ::remoting::protocol::MouseEvent>::
    Read(remoting::mojom::MouseEventDataView data_view,
         ::remoting::protocol::MouseEvent* out_event) {
  std::optional<int32_t> x;
  if (!data_view.ReadX(&x)) {
    return false;
  }
  if (x.has_value()) {
    out_event->set_x(*x);
  }

  std::optional<int32_t> y;
  if (!data_view.ReadY(&y)) {
    return false;
  }
  if (y.has_value()) {
    out_event->set_y(*y);
  }

  if (data_view.button() != remoting::mojom::MouseButton::kUndefined) {
    ::remoting::protocol::MouseEvent::MouseButton mouse_button;
    if (!data_view.ReadButton(&mouse_button)) {
      return false;
    }
    out_event->set_button(mouse_button);
  }

  std::optional<bool> button_down;
  if (!data_view.ReadButtonDown(&button_down)) {
    return false;
  }
  if (button_down.has_value()) {
    out_event->set_button_down(*button_down);
  }

  std::optional<float> wheel_delta_x;
  if (!data_view.ReadWheelDeltaX(&wheel_delta_x)) {
    return false;
  }
  if (wheel_delta_x.has_value()) {
    out_event->set_wheel_delta_x(*wheel_delta_x);
  }

  std::optional<float> wheel_delta_y;
  if (!data_view.ReadWheelDeltaY(&wheel_delta_y)) {
    return false;
  }
  if (wheel_delta_y.has_value()) {
    out_event->set_wheel_delta_y(*wheel_delta_y);
  }

  std::optional<float> wheel_ticks_x;
  if (!data_view.ReadWheelTicksX(&wheel_ticks_x)) {
    return false;
  }
  if (wheel_ticks_x.has_value()) {
    out_event->set_wheel_ticks_x(*wheel_ticks_x);
  }

  std::optional<float> wheel_ticks_y;
  if (!data_view.ReadWheelTicksY(&wheel_ticks_y)) {
    return false;
  }
  if (wheel_ticks_y.has_value()) {
    out_event->set_wheel_ticks_y(*wheel_ticks_y);
  }

  std::optional<int32_t> delta_x;
  if (!data_view.ReadDeltaX(&delta_x)) {
    return false;
  }
  if (delta_x.has_value()) {
    out_event->set_delta_x(*delta_x);
  }

  std::optional<int32_t> delta_y;
  if (!data_view.ReadDeltaY(&delta_y)) {
    return false;
  }
  if (delta_y.has_value()) {
    out_event->set_delta_y(*delta_y);
  }

  return true;
}

// static
bool mojo::StructTraits<remoting::mojom::ScreenResolutionDataView,
                        ::remoting::ScreenResolution>::
    Read(remoting::mojom::ScreenResolutionDataView data_view,
         ::remoting::ScreenResolution* out_resolution) {
  ::webrtc::DesktopSize desktop_size;
  if (!data_view.ReadDimensions(&desktop_size)) {
    return false;
  }

  ::webrtc::DesktopVector dpi;
  if (!data_view.ReadDpi(&dpi)) {
    return false;
  }

  *out_resolution =
      ::remoting::ScreenResolution(std::move(desktop_size), std::move(dpi));

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
  if (!data_view.ReadEventType(&touch_event_type)) {
    return false;
  }
  out_event->set_event_type(touch_event_type);

  if (!data_view.ReadTouchPoints(out_event->mutable_touch_points())) {
    return false;
  }

  return true;
}

// static
bool mojo::StructTraits<remoting::mojom::TransportRouteDataView,
                        ::remoting::protocol::TransportRoute>::
    Read(remoting::mojom::TransportRouteDataView data_view,
         ::remoting::protocol::TransportRoute* out_transport_route) {
  if (!data_view.ReadType(&out_transport_route->type)) {
    return false;
  }

  if (!data_view.ReadRemoteAddress(&out_transport_route->remote_address)) {
    return false;
  }

  if (!data_view.ReadLocalAddress(&out_transport_route->local_address)) {
    return false;
  }

  return true;
}

// static
bool mojo::StructTraits<remoting::mojom::VideoTrackLayoutDataView,
                        ::remoting::protocol::VideoTrackLayout>::
    Read(remoting::mojom::VideoTrackLayoutDataView data_view,
         ::remoting::protocol::VideoTrackLayout* out_track) {
  out_track->set_screen_id(data_view.screen_id());

  std::string media_stream_id;
  if (!data_view.ReadMediaStreamId(&media_stream_id)) {
    return false;
  }
  // Don't set |media_stream_id| if the value is empty as the client will
  // misinterpret an empty protobuf value and multi-mon scenarios will break.
  if (!media_stream_id.empty()) {
    out_track->set_media_stream_id(std::move(media_stream_id));
  }

  gfx::Point position;
  if (!data_view.ReadPosition(&position)) {
    return false;
  }
  out_track->set_position_x(position.x());
  out_track->set_position_y(position.y());

  webrtc::DesktopSize size;
  if (!data_view.ReadSize(&size)) {
    return false;
  }
  out_track->set_width(size.width());
  out_track->set_height(size.height());

  webrtc::DesktopVector dpi;
  if (!data_view.ReadDpi(&dpi)) {
    return false;
  }
  out_track->set_x_dpi(dpi.x());
  out_track->set_y_dpi(dpi.y());

  return true;
}

// static
bool mojo::StructTraits<remoting::mojom::VideoLayoutDataView,
                        ::remoting::protocol::VideoLayout>::
    Read(remoting::mojom::VideoLayoutDataView data_view,
         ::remoting::protocol::VideoLayout* out_layout) {
  if (!data_view.ReadTracks(out_layout->mutable_video_track())) {
    return false;
  }

  out_layout->set_supports_full_desktop_capture(
      data_view.supports_full_desktop_capture());

  out_layout->set_primary_screen_id(data_view.primary_screen_id());

  return true;
}

}  // namespace mojo
