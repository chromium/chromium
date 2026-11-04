// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/mojom/remoting_mojom_traits.h"

#include <string_view>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "remoting/base/buildflags.h"
#include "remoting/base/source_location.h"

#if BUILDFLAG(REMOTING_MULTI_PROCESS)
#include "remoting/host/mojom/desktop_session.mojom-shared.h"
#endif  // BUILDFLAG(REMOTING_MULTI_PROCESS)

namespace mojo {

#if BUILDFLAG(REMOTING_MULTI_PROCESS)

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
  out_options->set_enable_privacy_overlay(data_view.enable_privacy_overlay());
  out_options->set_enable_user_interface(data_view.enable_user_interface());
  out_options->set_enable_notifications(data_view.enable_notifications());
  out_options->set_terminate_upon_input(data_view.terminate_upon_input());
  out_options->set_enable_remote_webauthn(data_view.enable_remote_webauthn());
  out_options->set_enable_security_key(data_view.enable_security_key());

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

  if (data_view.api_error_code().has_value()) {
    out_error->set_api_error_code(data_view.api_error_code().value());
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

  if (data_view.caps_lock_state().has_value()) {
    out_event->set_caps_lock_state(data_view.caps_lock_state().value());
  }

  if (data_view.num_lock_state().has_value()) {
    out_event->set_num_lock_state(data_view.num_lock_state().value());
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

  auto new_frame = std::make_unique<::webrtc::BasicDesktopFrame>(
      image_size, webrtc::FOURCC_ARGB);
  UNSAFE_TODO(memcpy(new_frame->data(), image_data.data(), image_data.size()));

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
  if (data_view.x().has_value()) {
    out_event->set_x(data_view.x().value());
  }

  if (data_view.y().has_value()) {
    out_event->set_y(data_view.y().value());
  }

  if (data_view.button() != remoting::mojom::MouseButton::kUndefined) {
    ::remoting::protocol::MouseEvent::MouseButton mouse_button;
    if (!data_view.ReadButton(&mouse_button)) {
      return false;
    }
    out_event->set_button(mouse_button);
  }

  if (data_view.button_down().has_value()) {
    out_event->set_button_down(data_view.button_down().value());
  }

  if (data_view.wheel_delta_x().has_value()) {
    out_event->set_wheel_delta_x(data_view.wheel_delta_x().value());
  }

  if (data_view.wheel_delta_y().has_value()) {
    out_event->set_wheel_delta_y(data_view.wheel_delta_y().value());
  }

  if (data_view.wheel_ticks_x().has_value()) {
    out_event->set_wheel_ticks_x(data_view.wheel_ticks_x().value());
  }

  if (data_view.wheel_ticks_y().has_value()) {
    out_event->set_wheel_ticks_y(data_view.wheel_ticks_y().value());
  }

  if (data_view.delta_x().has_value()) {
    out_event->set_delta_x(data_view.delta_x().value());
  }

  if (data_view.delta_y().has_value()) {
    out_event->set_delta_y(data_view.delta_y().value());
  }

  remoting::mojom::FractionalCoordinateDataView fractional_coordinate_data_view;
  data_view.GetFractionalCoordinateDataView(&fractional_coordinate_data_view);

  if (!fractional_coordinate_data_view.is_null()) {
    mojo::StructTraits<remoting::mojom::FractionalCoordinateDataView,
                       ::remoting::protocol::FractionalCoordinate>::
        Read(fractional_coordinate_data_view,
             out_event->mutable_fractional_coordinate());
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
  std::optional<int64_t> screen_id = data_view.screen_id();
  if (screen_id.has_value()) {
    out_track->set_screen_id(screen_id.value());
  }

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

  std::string display_name;
  if (!data_view.ReadDisplayName(&display_name)) {
    return false;
  }
  out_track->set_display_name(std::move(display_name));

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

  std::optional<::remoting::protocol::VideoLayout::PixelType> pixel_type;
  if (!data_view.ReadPixelType(&pixel_type)) {
    return false;
  }
  if (pixel_type.has_value()) {
    out_layout->set_pixel_type(pixel_type.value());
  }

  return true;
}

#endif  // BUILDFLAG(REMOTING_MULTI_PROCESS)

// static
bool mojo::StructTraits<remoting::mojom::SourceLocationDataView,
                        ::remoting::SourceLocation>::
    Read(remoting::mojom::SourceLocationDataView data_view,
         ::remoting::SourceLocation* out_source_info) {
  std::optional<std::string_view> function_name;
  std::optional<std::string_view> file_name;
  if (!data_view.ReadFunctionName(&function_name)) {
    return false;
  }
  if (!data_view.ReadFileName(&file_name)) {
    return false;
  }
  out_source_info->InitializeWithBackingStore(function_name, file_name,
                                              data_view.line_number());

  return true;
}

// static
bool mojo::StructTraits<
    remoting::mojom::PortRangeDataView,
    ::remoting::PortRange>::Read(remoting::mojom::PortRangeDataView data_view,
                                 ::remoting::PortRange* out_range) {
  auto port_range =
      ::remoting::PortRange::Create(data_view.min_port(), data_view.max_port());
  if (!port_range) {
    return false;
  }
  *out_range = *std::move(port_range);
  return true;
}

// static
bool mojo::StructTraits<remoting::mojom::SessionPoliciesDataView,
                        ::remoting::SessionPolicies>::
    Read(remoting::mojom::SessionPoliciesDataView data_view,
         ::remoting::SessionPolicies* out_policies) {
  if (auto clipboard_size = data_view.clipboard_size_bytes();
      clipboard_size.has_value()) {
    // This check is needed because the Windows host is still 32-bit.
    if (!base::IsValueInRangeForNumericType<size_t>(*clipboard_size)) {
      return false;
    }
    out_policies->clipboard_size_bytes =
        base::checked_cast<size_t>(*clipboard_size);
  } else {
    out_policies->clipboard_size_bytes = std::nullopt;
  }
  out_policies->allow_stun_connections = data_view.allow_stun_connections();
  out_policies->allow_relayed_connections =
      data_view.allow_relayed_connections();
  if (!data_view.ReadHostUdpPortRange(&out_policies->host_udp_port_range)) {
    return false;
  }
  out_policies->allow_file_transfer = data_view.allow_file_transfer();
  out_policies->allow_uri_forwarding = data_view.allow_uri_forwarding();
  if (!data_view.ReadMaximumSessionDuration(
          &out_policies->maximum_session_duration)) {
    return false;
  }
  out_policies->curtain_required = data_view.curtain_required();
  out_policies->host_username_match_required =
      data_view.host_username_match_required();
  out_policies->allow_remote_input = data_view.allow_remote_input();
  out_policies->allow_webauthn_forwarding =
      data_view.allow_webauthn_forwarding();
  out_policies->allow_security_key_forwarding =
      data_view.allow_security_key_forwarding();
  out_policies->allow_terminal_mode = data_view.allow_terminal_mode();
  if (!data_view.ReadAudioMode(&out_policies->audio_mode)) {
    return false;
  }
  return true;
}

// static
bool mojo::StructTraits<remoting::mojom::SessionOptionsDataView,
                        ::remoting::SessionOptions>::
    Read(remoting::mojom::SessionOptionsDataView data_view,
         ::remoting::SessionOptions* out_options) {
  out_options->detect_updated_region = data_view.detect_updated_region();
  out_options->capture_video_on_dedicated_thread =
      data_view.capture_video_on_dedicated_thread();
#if BUILDFLAG(IS_MAC)
  out_options->enable_sck_capturer = data_view.enable_sck_capturer();
#endif  // BUILDFLAG(IS_MAC)
#if BUILDFLAG(IS_WIN)
  out_options->allow_dxgi_capturer = data_view.allow_dxgi_capturer();
#endif  // BUILDFLAG(IS_WIN)
  out_options->disable_udp = data_view.disable_udp();
  out_options->vp9_encoder_speed = data_view.vp9_encoder_speed();
  out_options->av1_active_map = data_view.av1_active_map();
  out_options->av1_encoder_speed = data_view.av1_encoder_speed();
  out_options->enable_privacy_overlay = data_view.enable_privacy_overlay();
  return true;
}

// static
bool mojo::StructTraits<remoting::mojom::AgentStatusDataView,
                        ::remoting::protocol::TerminalControl_AgentInfo>::
    Read(remoting::mojom::AgentStatusDataView data_view,
         ::remoting::protocol::TerminalControl_AgentInfo* out_status) {
  std::optional<::remoting::protocol::TerminalControl_AgentInfo_AgentState>
      state;
  if (!data_view.ReadAgentState(&state)) {
    return false;
  }
  std::optional<std::string_view> conversation_title;
  if (!data_view.ReadConversationTitle(&conversation_title)) {
    return false;
  }
  out_status->Clear();
  if (state.has_value()) {
    out_status->set_agent_state(*state);
  }
  if (conversation_title.has_value()) {
    out_status->set_conversation_title(*conversation_title);
  }
  return true;
}

// static
remoting::mojom::ProtocolErrorCode
mojo::EnumTraits<remoting::mojom::ProtocolErrorCode,
                 ::remoting::ErrorCode>::ToMojom(::remoting::ErrorCode input) {
  switch (input) {
    case ::remoting::ErrorCode::OK:
      return remoting::mojom::ProtocolErrorCode::kOk;
    case ::remoting::ErrorCode::PEER_IS_OFFLINE:
      return remoting::mojom::ProtocolErrorCode::kPeerIsOffline;
    case ::remoting::ErrorCode::SESSION_REJECTED:
      return remoting::mojom::ProtocolErrorCode::kSessionRejected;
    case ::remoting::ErrorCode::INCOMPATIBLE_PROTOCOL:
      return remoting::mojom::ProtocolErrorCode::kIncompatibleProtocol;
    case ::remoting::ErrorCode::AUTHENTICATION_FAILED:
      return remoting::mojom::ProtocolErrorCode::kAuthenticationFailed;
    case ::remoting::ErrorCode::INVALID_ACCOUNT:
      return remoting::mojom::ProtocolErrorCode::kInvalidAccount;
    case ::remoting::ErrorCode::CHANNEL_CONNECTION_ERROR:
      return remoting::mojom::ProtocolErrorCode::kChannelConnectionError;
    case ::remoting::ErrorCode::SIGNALING_ERROR:
      return remoting::mojom::ProtocolErrorCode::kSignalingError;
    case ::remoting::ErrorCode::SIGNALING_TIMEOUT:
      return remoting::mojom::ProtocolErrorCode::kSignalingTimeout;
    case ::remoting::ErrorCode::HOST_OVERLOAD:
      return remoting::mojom::ProtocolErrorCode::kHostOverload;
    case ::remoting::ErrorCode::MAX_SESSION_LENGTH:
      return remoting::mojom::ProtocolErrorCode::kMaxSessionLength;
    case ::remoting::ErrorCode::HOST_CONFIGURATION_ERROR:
      return remoting::mojom::ProtocolErrorCode::kHostConfigurationError;
    case ::remoting::ErrorCode::UNKNOWN_ERROR:
      return remoting::mojom::ProtocolErrorCode::kUnknownError;
    case ::remoting::ErrorCode::ELEVATION_ERROR:
      return remoting::mojom::ProtocolErrorCode::kElevationError;
    case ::remoting::ErrorCode::HOST_CERTIFICATE_ERROR:
      return remoting::mojom::ProtocolErrorCode::kHostCertificateError;
    case ::remoting::ErrorCode::HOST_REGISTRATION_ERROR:
      return remoting::mojom::ProtocolErrorCode::kHostRegistrationError;
    case ::remoting::ErrorCode::EXISTING_ADMIN_SESSION:
      return remoting::mojom::ProtocolErrorCode::kExistingAdminSession;
    case ::remoting::ErrorCode::AUTHZ_POLICY_CHECK_FAILED:
      return remoting::mojom::ProtocolErrorCode::kAuthzPolicyCheckFailed;
    case ::remoting::ErrorCode::DISALLOWED_BY_POLICY:
      return remoting::mojom::ProtocolErrorCode::kDisallowedByPolicy;
    case ::remoting::ErrorCode::LOCATION_AUTHZ_POLICY_CHECK_FAILED:
      return remoting::mojom::ProtocolErrorCode::
          kLocationAuthzPolicyCheckFailed;
    case ::remoting::ErrorCode::UNAUTHORIZED_ACCOUNT:
      return remoting::mojom::ProtocolErrorCode::kUnauthorizedAccount;
    case ::remoting::ErrorCode::REAUTHZ_POLICY_CHECK_FAILED:
      return remoting::mojom::ProtocolErrorCode::kReauthzPolicyCheckFailed;
    case ::remoting::ErrorCode::NO_COMMON_AUTH_METHOD:
      return remoting::mojom::ProtocolErrorCode::kNoCommonAuthMethod;
    case ::remoting::ErrorCode::LOGIN_SCREEN_NOT_SUPPORTED:
      return remoting::mojom::ProtocolErrorCode::kLoginScreenNotSupported;
    case ::remoting::ErrorCode::SESSION_POLICIES_CHANGED:
      return remoting::mojom::ProtocolErrorCode::kSessionPoliciesChanged;
    case ::remoting::ErrorCode::UNEXPECTED_AUTHENTICATOR_ERROR:
      return remoting::mojom::ProtocolErrorCode::kUnexpectedAuthenticatorError;
    case ::remoting::ErrorCode::INVALID_STATE:
      return remoting::mojom::ProtocolErrorCode::kInvalidState;
    case ::remoting::ErrorCode::INVALID_ARGUMENT:
      return remoting::mojom::ProtocolErrorCode::kInvalidArgument;
    case ::remoting::ErrorCode::NETWORK_FAILURE:
      return remoting::mojom::ProtocolErrorCode::kNetworkFailure;
    case ::remoting::ErrorCode::OPERATION_TIMEOUT:
      return remoting::mojom::ProtocolErrorCode::kOperationTimeout;
    case ::remoting::ErrorCode::SOFTWARE_UPGRADED:
      return remoting::mojom::ProtocolErrorCode::kSoftwareUpgraded;
  }

  NOTREACHED();
}

// static
::remoting::ErrorCode
mojo::EnumTraits<remoting::mojom::ProtocolErrorCode, ::remoting::ErrorCode>::
    FromMojom(remoting::mojom::ProtocolErrorCode input) {
  switch (input) {
    case remoting::mojom::ProtocolErrorCode::kOk:
      return ::remoting::ErrorCode::OK;
    case remoting::mojom::ProtocolErrorCode::kPeerIsOffline:
      return ::remoting::ErrorCode::PEER_IS_OFFLINE;
    case remoting::mojom::ProtocolErrorCode::kSessionRejected:
      return ::remoting::ErrorCode::SESSION_REJECTED;
    case remoting::mojom::ProtocolErrorCode::kIncompatibleProtocol:
      return ::remoting::ErrorCode::INCOMPATIBLE_PROTOCOL;
    case remoting::mojom::ProtocolErrorCode::kAuthenticationFailed:
      return ::remoting::ErrorCode::AUTHENTICATION_FAILED;
    case remoting::mojom::ProtocolErrorCode::kInvalidAccount:
      return ::remoting::ErrorCode::INVALID_ACCOUNT;
    case remoting::mojom::ProtocolErrorCode::kChannelConnectionError:
      return ::remoting::ErrorCode::CHANNEL_CONNECTION_ERROR;
    case remoting::mojom::ProtocolErrorCode::kSignalingError:
      return ::remoting::ErrorCode::SIGNALING_ERROR;
    case remoting::mojom::ProtocolErrorCode::kSignalingTimeout:
      return ::remoting::ErrorCode::SIGNALING_TIMEOUT;
    case remoting::mojom::ProtocolErrorCode::kHostOverload:
      return ::remoting::ErrorCode::HOST_OVERLOAD;
    case remoting::mojom::ProtocolErrorCode::kMaxSessionLength:
      return ::remoting::ErrorCode::MAX_SESSION_LENGTH;
    case remoting::mojom::ProtocolErrorCode::kHostConfigurationError:
      return ::remoting::ErrorCode::HOST_CONFIGURATION_ERROR;
    case remoting::mojom::ProtocolErrorCode::kUnknownError:
      return ::remoting::ErrorCode::UNKNOWN_ERROR;
    case remoting::mojom::ProtocolErrorCode::kElevationError:
      return ::remoting::ErrorCode::ELEVATION_ERROR;
    case remoting::mojom::ProtocolErrorCode::kHostCertificateError:
      return ::remoting::ErrorCode::HOST_CERTIFICATE_ERROR;
    case remoting::mojom::ProtocolErrorCode::kHostRegistrationError:
      return ::remoting::ErrorCode::HOST_REGISTRATION_ERROR;
    case remoting::mojom::ProtocolErrorCode::kExistingAdminSession:
      return ::remoting::ErrorCode::EXISTING_ADMIN_SESSION;
    case remoting::mojom::ProtocolErrorCode::kAuthzPolicyCheckFailed:
      return ::remoting::ErrorCode::AUTHZ_POLICY_CHECK_FAILED;
    case remoting::mojom::ProtocolErrorCode::kDisallowedByPolicy:
      return ::remoting::ErrorCode::DISALLOWED_BY_POLICY;
    case remoting::mojom::ProtocolErrorCode::kLocationAuthzPolicyCheckFailed:
      return ::remoting::ErrorCode::LOCATION_AUTHZ_POLICY_CHECK_FAILED;
    case remoting::mojom::ProtocolErrorCode::kUnauthorizedAccount:
      return ::remoting::ErrorCode::UNAUTHORIZED_ACCOUNT;
    case remoting::mojom::ProtocolErrorCode::kReauthzPolicyCheckFailed:
      return ::remoting::ErrorCode::REAUTHZ_POLICY_CHECK_FAILED;
    case remoting::mojom::ProtocolErrorCode::kNoCommonAuthMethod:
      return ::remoting::ErrorCode::NO_COMMON_AUTH_METHOD;
    case remoting::mojom::ProtocolErrorCode::kLoginScreenNotSupported:
      return ::remoting::ErrorCode::LOGIN_SCREEN_NOT_SUPPORTED;
    case remoting::mojom::ProtocolErrorCode::kSessionPoliciesChanged:
      return ::remoting::ErrorCode::SESSION_POLICIES_CHANGED;
    case remoting::mojom::ProtocolErrorCode::kUnexpectedAuthenticatorError:
      return ::remoting::ErrorCode::UNEXPECTED_AUTHENTICATOR_ERROR;
    case remoting::mojom::ProtocolErrorCode::kInvalidState:
      return ::remoting::ErrorCode::INVALID_STATE;
    case remoting::mojom::ProtocolErrorCode::kInvalidArgument:
      return ::remoting::ErrorCode::INVALID_ARGUMENT;
    case remoting::mojom::ProtocolErrorCode::kNetworkFailure:
      return ::remoting::ErrorCode::NETWORK_FAILURE;
    case remoting::mojom::ProtocolErrorCode::kOperationTimeout:
      return ::remoting::ErrorCode::OPERATION_TIMEOUT;
    case remoting::mojom::ProtocolErrorCode::kSoftwareUpgraded:
      return ::remoting::ErrorCode::SOFTWARE_UPGRADED;
  }

  NOTREACHED();
}

// static
remoting::mojom::SessionPolicies_AudioMode
mojo::EnumTraits<remoting::mojom::SessionPolicies_AudioMode,
                 ::remoting::SessionPolicies::AudioMode>::
    ToMojom(::remoting::SessionPolicies::AudioMode input) {
  switch (input) {
    case ::remoting::SessionPolicies::AudioMode::kUnspecified:
      return remoting::mojom::SessionPolicies_AudioMode::kUnspecified;
    case ::remoting::SessionPolicies::AudioMode::kNone:
      return remoting::mojom::SessionPolicies_AudioMode::kNone;
    case ::remoting::SessionPolicies::AudioMode::kClientToHost:
      return remoting::mojom::SessionPolicies_AudioMode::kClientToHost;
    case ::remoting::SessionPolicies::AudioMode::kHostToClient:
      return remoting::mojom::SessionPolicies_AudioMode::kHostToClient;
    case ::remoting::SessionPolicies::AudioMode::kBoth:
      return remoting::mojom::SessionPolicies_AudioMode::kBoth;
  }

  NOTREACHED();
}

// static
::remoting::SessionPolicies::AudioMode
mojo::EnumTraits<remoting::mojom::SessionPolicies_AudioMode,
                 ::remoting::SessionPolicies::AudioMode>::
    FromMojom(remoting::mojom::SessionPolicies_AudioMode input) {
  switch (input) {
    case remoting::mojom::SessionPolicies_AudioMode::kUnspecified:
      return ::remoting::SessionPolicies::AudioMode::kUnspecified;
    case remoting::mojom::SessionPolicies_AudioMode::kNone:
      return ::remoting::SessionPolicies::AudioMode::kNone;
    case remoting::mojom::SessionPolicies_AudioMode::kClientToHost:
      return ::remoting::SessionPolicies::AudioMode::kClientToHost;
    case remoting::mojom::SessionPolicies_AudioMode::kHostToClient:
      return ::remoting::SessionPolicies::AudioMode::kHostToClient;
    case remoting::mojom::SessionPolicies_AudioMode::kBoth:
      return ::remoting::SessionPolicies::AudioMode::kBoth;
  }

  NOTREACHED();
}

// static
remoting::mojom::AgentState
mojo::EnumTraits<remoting::mojom::AgentState,
                 ::remoting::protocol::TerminalControl_AgentInfo_AgentState>::
    ToMojom(::remoting::protocol::TerminalControl_AgentInfo_AgentState input) {
  switch (input) {
    case ::remoting::protocol::TerminalControl_AgentInfo::IDLE:
      return remoting::mojom::AgentState::kIdle;
    case ::remoting::protocol::TerminalControl_AgentInfo::WORKING:
      return remoting::mojom::AgentState::kWorking;
    case ::remoting::protocol::TerminalControl_AgentInfo::TOOL_USE:
      return remoting::mojom::AgentState::kToolUse;
    case ::remoting::protocol::TerminalControl_AgentInfo::REVIEWING:
      return remoting::mojom::AgentState::kReviewing;
    case ::remoting::protocol::TerminalControl_AgentInfo::AUTHENTICATING:
      return remoting::mojom::AgentState::kAuthenticating;
    case ::remoting::protocol::TerminalControl_AgentInfo::INITIALIZING:
      return remoting::mojom::AgentState::kInitializing;
    case ::remoting::protocol::TerminalControl_AgentInfo_AgentState_ERROR:
      return remoting::mojom::AgentState::kError;
    case ::remoting::protocol::TerminalControl_AgentInfo::UNSPECIFIED:
      break;
  }
  NOTREACHED();
}

// static
std::optional<::remoting::protocol::TerminalControl_AgentInfo_AgentState>
mojo::EnumTraits<remoting::mojom::AgentState,
                 ::remoting::protocol::TerminalControl_AgentInfo_AgentState>::
    FromMojom(remoting::mojom::AgentState input) {
  switch (input) {
    case remoting::mojom::AgentState::kIdle:
      return ::remoting::protocol::TerminalControl_AgentInfo::IDLE;
    case remoting::mojom::AgentState::kWorking:
      return ::remoting::protocol::TerminalControl_AgentInfo::WORKING;
    case remoting::mojom::AgentState::kToolUse:
      return ::remoting::protocol::TerminalControl_AgentInfo::TOOL_USE;
    case remoting::mojom::AgentState::kReviewing:
      return ::remoting::protocol::TerminalControl_AgentInfo::REVIEWING;
    case remoting::mojom::AgentState::kAuthenticating:
      return ::remoting::protocol::TerminalControl_AgentInfo::AUTHENTICATING;
    case remoting::mojom::AgentState::kInitializing:
      return ::remoting::protocol::TerminalControl_AgentInfo::INITIALIZING;
    case remoting::mojom::AgentState::kError:
      return ::remoting::protocol::TerminalControl_AgentInfo_AgentState_ERROR;
  }
  return std::nullopt;
}

#if BUILDFLAG(REMOTING_MULTI_PROCESS)

// static
bool mojo::StructTraits<remoting::mojom::FractionalCoordinateDataView,
                        ::remoting::protocol::FractionalCoordinate>::
    Read(remoting::mojom::FractionalCoordinateDataView data_view,
         ::remoting::protocol::FractionalCoordinate* out_coordinate) {
  out_coordinate->set_screen_id(data_view.screen_id());
  out_coordinate->set_x(data_view.x());
  out_coordinate->set_y(data_view.y());
  return true;
}

// static
bool mojo::StructTraits<remoting::mojom::MicrophoneControlDataView,
                        ::remoting::protocol::MicrophoneControl>::
    Read(remoting::mojom::MicrophoneControlDataView data_view,
         ::remoting::protocol::MicrophoneControl* out_control) {
  out_control->set_enable(data_view.enable());
  return true;
}

// static
bool mojo::StructTraits<remoting::mojom::IpcFifoBufferReaderDataView,
                        std::unique_ptr<remoting::IpcFifoBufferReader>>::
    Read(remoting::mojom::IpcFifoBufferReaderDataView data_view,
         std::unique_ptr<remoting::IpcFifoBufferReader>* out_reader) {
  mojo::ScopedDataPipeConsumerHandle consumer_handle =
      data_view.TakeConsumerHandle();
  if (!consumer_handle.is_valid()) {
    return false;
  }
  *out_reader = std::make_unique<remoting::IpcFifoBufferReader>(
      std::move(consumer_handle));
  return true;
}

// static
bool mojo::StructTraits<remoting::mojom::AudioSampleInfoDataView,
                        ::remoting::protocol::AudioSampleInfo>::
    Read(remoting::mojom::AudioSampleInfoDataView data_view,
         ::remoting::protocol::AudioSampleInfo* out_info) {
  out_info->sampling_rate = data_view.sampling_rate();
  out_info->channels = data_view.channels();
  return true;
}

// static
bool mojo::StructTraits<remoting::mojom::WebrtcSocketAddressDataView,
                        ::webrtc::SocketAddress>::
    Read(remoting::mojom::WebrtcSocketAddressDataView data_view,
         ::webrtc::SocketAddress* out_address) {
  std::string_view hostname_view;
  if (!data_view.ReadHostname(&hostname_view)) {
    return false;
  }

  ::webrtc::SocketAddress temp;
  temp.SetIP(hostname_view);
  temp.SetPort(data_view.port());

  *out_address = std::move(temp);
  return true;
}

// static
bool mojo::StructTraits<remoting::mojom::WebrtcCandidateDataView,
                        ::webrtc::Candidate>::
    Read(remoting::mojom::WebrtcCandidateDataView data_view,
         ::webrtc::Candidate* out_candidate) {
  ::webrtc::Candidate temp;

  std::string_view foundation_view;
  if (!data_view.ReadFoundation(&foundation_view)) {
    return false;
  }
  temp.set_foundation(foundation_view);

  temp.set_component(data_view.component());

  std::string_view protocol_view;
  if (!data_view.ReadProtocol(&protocol_view)) {
    return false;
  }
  temp.set_protocol(protocol_view);

  temp.set_priority(data_view.priority());

  ::webrtc::SocketAddress address;
  if (!data_view.ReadAddress(&address)) {
    return false;
  }
  temp.set_address(address);

  ::webrtc::IceCandidateType candidate_type;
  if (!data_view.ReadType(&candidate_type)) {
    return false;
  }
  temp.set_type(candidate_type);

  std::string_view tcptype_view;
  if (!data_view.ReadTcptype(&tcptype_view)) {
    return false;
  }
  temp.set_tcptype(tcptype_view);

  ::webrtc::SocketAddress related_address;
  if (!data_view.ReadRelatedAddress(&related_address)) {
    return false;
  }
  temp.set_related_address(related_address);

  std::string_view username_view;
  if (!data_view.ReadUsername(&username_view)) {
    return false;
  }
  temp.set_username(username_view);

  std::string_view password_view;
  if (!data_view.ReadPassword(&password_view)) {
    return false;
  }
  temp.set_password(password_view);

  temp.set_generation(data_view.generation());
  temp.set_network_id(data_view.network_id());
  temp.set_network_cost(data_view.network_cost());

  *out_candidate = std::move(temp);
  return true;
}

// static
bool mojo::StructTraits<remoting::mojom::SessionDescriptionDataView,
                        ::remoting::SessionDescription>::
    Read(remoting::mojom::SessionDescriptionDataView data_view,
         ::remoting::SessionDescription* out_description) {
  ::remoting::SessionDescription temp;

  if (!data_view.ReadType(&temp.type) || !data_view.ReadSdp(&temp.sdp) ||
      !data_view.ReadSignature(&temp.signature)) {
    return false;
  }

  *out_description = std::move(temp);
  return true;
}

// static
bool mojo::StructTraits<remoting::mojom::NamedCandidateDataView,
                        ::remoting::IceTransportInfo::NamedCandidate>::
    Read(remoting::mojom::NamedCandidateDataView data_view,
         ::remoting::IceTransportInfo::NamedCandidate* out_candidate) {
  ::remoting::IceTransportInfo::NamedCandidate temp;

  if (!data_view.ReadSdpMid(&temp.name)) {
    return false;
  }

  temp.sdp_m_line_index = data_view.sdp_m_line_index();

  if (!data_view.ReadCandidate(&temp.candidate)) {
    return false;
  }

  *out_candidate = std::move(temp);
  return true;
}

// static
bool mojo::StructTraits<remoting::mojom::JingleTransportInfoDataView,
                        ::remoting::JingleTransportInfo>::
    Read(remoting::mojom::JingleTransportInfoDataView data_view,
         ::remoting::JingleTransportInfo* out_transport_info) {
  ::remoting::JingleTransportInfo temp;

  if (!data_view.ReadCandidates(&temp.candidates) ||
      !data_view.ReadSessionDescription(&temp.session_description)) {
    return false;
  }

  *out_transport_info = std::move(temp);
  return true;
}

// static
remoting::mojom::WebrtcProtocolType mojo::EnumTraits<
    remoting::mojom::WebrtcProtocolType,
    ::webrtc::ProtocolType>::ToMojom(::webrtc::ProtocolType protocol) {
  switch (protocol) {
    case ::webrtc::PROTO_UDP:
      return remoting::mojom::WebrtcProtocolType::kUdp;
    case ::webrtc::PROTO_DTLS:
      return remoting::mojom::WebrtcProtocolType::kDtls;
    case ::webrtc::PROTO_TCP:
      return remoting::mojom::WebrtcProtocolType::kTcp;
    case ::webrtc::PROTO_SSLTCP:
      return remoting::mojom::WebrtcProtocolType::kSslTcp;
    case ::webrtc::PROTO_TLS:
      return remoting::mojom::WebrtcProtocolType::kTls;
  }
  NOTREACHED();
}

// static
std::optional<::webrtc::ProtocolType>
mojo::EnumTraits<remoting::mojom::WebrtcProtocolType, ::webrtc::ProtocolType>::
    FromMojom(remoting::mojom::WebrtcProtocolType input) {
  switch (input) {
    case remoting::mojom::WebrtcProtocolType::kUdp:
      return ::webrtc::PROTO_UDP;
    case remoting::mojom::WebrtcProtocolType::kDtls:
      return ::webrtc::PROTO_DTLS;
    case remoting::mojom::WebrtcProtocolType::kTcp:
      return ::webrtc::PROTO_TCP;
    case remoting::mojom::WebrtcProtocolType::kSslTcp:
      return ::webrtc::PROTO_SSLTCP;
    case remoting::mojom::WebrtcProtocolType::kTls:
      return ::webrtc::PROTO_TLS;
  }
  return std::nullopt;
}

// static
bool mojo::StructTraits<remoting::mojom::WebrtcRelayServerConfigDataView,
                        ::webrtc::RelayServerConfig>::
    Read(remoting::mojom::WebrtcRelayServerConfigDataView data_view,
         ::webrtc::RelayServerConfig* out_config) {
  std::string username;
  if (!data_view.ReadUsername(&username)) {
    return false;
  }
  std::string password;
  if (!data_view.ReadPassword(&password)) {
    return false;
  }

  mojo::ArrayDataView<remoting::mojom::WebrtcProtocolAddressDataView>
      ports_data;
  data_view.GetPortsDataView(&ports_data);
  std::vector<::webrtc::ProtocolAddress> ports;
  ports.reserve(ports_data.size());
  for (size_t i = 0; i < ports_data.size(); ++i) {
    remoting::mojom::WebrtcProtocolAddressDataView port_data;
    ports_data.GetDataView(i, &port_data);

    ::webrtc::SocketAddress address;
    if (!port_data.ReadAddress(&address)) {
      return false;
    }
    ::webrtc::ProtocolType proto;
    if (!port_data.ReadProtocol(&proto)) {
      return false;
    }
    ports.emplace_back(std::move(address), proto);
  }

  out_config->credentials.username = std::move(username);
  out_config->credentials.password = std::move(password);
  out_config->ports = std::move(ports);
  return true;
}

// static
bool mojo::StructTraits<remoting::mojom::IceConfigDataView,
                        ::remoting::protocol::IceConfig>::
    Read(remoting::mojom::IceConfigDataView data_view,
         ::remoting::protocol::IceConfig* out_config) {
  if (!data_view.ReadExpirationTime(&out_config->expiration_time) ||
      !data_view.ReadStunServers(&out_config->stun_servers) ||
      !data_view.ReadTurnServers(&out_config->turn_servers)) {
    return false;
  }
  out_config->max_bitrate_kbps = data_view.max_bitrate_kbps();
  return true;
}

// static
bool mojo::StructTraits<remoting::mojom::PairingResponseDataView,
                        ::remoting::protocol::PairingResponse>::
    Read(remoting::mojom::PairingResponseDataView data_view,
         ::remoting::protocol::PairingResponse* out_response) {
  std::string client_id;
  std::string shared_secret;
  if (!data_view.ReadClientId(&client_id) ||
      !data_view.ReadSharedSecret(&shared_secret)) {
    return false;
  }
  out_response->set_client_id(std::move(client_id));
  out_response->set_shared_secret(std::move(shared_secret));
  return true;
}

// static
remoting::mojom::DesktopCaptureResult
mojo::EnumTraits<remoting::mojom::DesktopCaptureResult,
                 ::webrtc::DesktopCapturer::Result>::
    ToMojom(::webrtc::DesktopCapturer::Result input) {
  switch (input) {
    case ::webrtc::DesktopCapturer::Result::SUCCESS:
      return remoting::mojom::DesktopCaptureResult::kSuccess;
    case ::webrtc::DesktopCapturer::Result::ERROR_TEMPORARY:
      return remoting::mojom::DesktopCaptureResult::kErrorTemporary;
    case ::webrtc::DesktopCapturer::Result::ERROR_PERMANENT:
      return remoting::mojom::DesktopCaptureResult::kErrorPermanent;
  }

  NOTREACHED();
}

// static
::webrtc::DesktopCapturer::Result
mojo::EnumTraits<remoting::mojom::DesktopCaptureResult,
                 ::webrtc::DesktopCapturer::Result>::
    FromMojom(remoting::mojom::DesktopCaptureResult input) {
  switch (input) {
    case remoting::mojom::DesktopCaptureResult::kSuccess:
      return ::webrtc::DesktopCapturer::Result::SUCCESS;
    case remoting::mojom::DesktopCaptureResult::kErrorTemporary:
      return ::webrtc::DesktopCapturer::Result::ERROR_TEMPORARY;
    case remoting::mojom::DesktopCaptureResult::kErrorPermanent:
      return ::webrtc::DesktopCapturer::Result::ERROR_PERMANENT;
  }

  NOTREACHED();
}

// static
remoting::mojom::MouseButton
mojo::EnumTraits<remoting::mojom::MouseButton,
                 ::remoting::protocol::MouseEvent::MouseButton>::
    ToMojom(::remoting::protocol::MouseEvent::MouseButton input) {
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
}

// static
::remoting::protocol::MouseEvent::MouseButton
mojo::EnumTraits<remoting::mojom::MouseButton,
                 ::remoting::protocol::MouseEvent::MouseButton>::
    FromMojom(remoting::mojom::MouseButton input) {
  switch (input) {
    case remoting::mojom::MouseButton::kUndefined:
      return ::remoting::protocol::MouseEvent::BUTTON_UNDEFINED;
    case remoting::mojom::MouseButton::kLeft:
      return ::remoting::protocol::MouseEvent::BUTTON_LEFT;
    case remoting::mojom::MouseButton::kMiddle:
      return ::remoting::protocol::MouseEvent::BUTTON_MIDDLE;
    case remoting::mojom::MouseButton::kRight:
      return ::remoting::protocol::MouseEvent::BUTTON_RIGHT;
    case remoting::mojom::MouseButton::kBack:
      return ::remoting::protocol::MouseEvent::BUTTON_BACK;
    case remoting::mojom::MouseButton::kForward:
      return ::remoting::protocol::MouseEvent::BUTTON_FORWARD;
  }

  NOTREACHED();
}

// static
remoting::mojom::AudioPacket_BytesPerSample
mojo::EnumTraits<remoting::mojom::AudioPacket_BytesPerSample,
                 ::remoting::AudioPacket::BytesPerSample>::
    ToMojom(::remoting::AudioPacket::BytesPerSample input) {
  switch (input) {
    case ::remoting::AudioPacket::BYTES_PER_SAMPLE_INVALID:
      return remoting::mojom::AudioPacket_BytesPerSample::kInvalid;
    case ::remoting::AudioPacket::BYTES_PER_SAMPLE_2:
      return remoting::mojom::AudioPacket_BytesPerSample::kBytesPerSample_2;
  }

  NOTREACHED();
}

// static
::remoting::AudioPacket::BytesPerSample
mojo::EnumTraits<remoting::mojom::AudioPacket_BytesPerSample,
                 ::remoting::AudioPacket::BytesPerSample>::
    FromMojom(remoting::mojom::AudioPacket_BytesPerSample input) {
  switch (input) {
    case remoting::mojom::AudioPacket_BytesPerSample::kInvalid:
      return ::remoting::AudioPacket::BYTES_PER_SAMPLE_INVALID;
    case remoting::mojom::AudioPacket_BytesPerSample::kBytesPerSample_2:
      return ::remoting::AudioPacket::BYTES_PER_SAMPLE_2;
  }

  NOTREACHED();
}

// static
remoting::mojom::AudioPacket_Channels
mojo::EnumTraits<remoting::mojom::AudioPacket_Channels,
                 ::remoting::AudioPacket::Channels>::
    ToMojom(::remoting::AudioPacket::Channels input) {
  switch (input) {
    case ::remoting::AudioPacket::CHANNELS_INVALID:
      return remoting::mojom::AudioPacket_Channels::kInvalid;
    case ::remoting::AudioPacket::CHANNELS_MONO:
      return remoting::mojom::AudioPacket_Channels::kMono;
    case ::remoting::AudioPacket::CHANNELS_STEREO:
      return remoting::mojom::AudioPacket_Channels::kStereo;
    case ::remoting::AudioPacket::CHANNELS_SURROUND:
      return remoting::mojom::AudioPacket_Channels::kSurround;
    case ::remoting::AudioPacket::CHANNELS_4_0:
      return remoting::mojom::AudioPacket_Channels::kChannel_4_0;
    case ::remoting::AudioPacket::CHANNELS_4_1:
      return remoting::mojom::AudioPacket_Channels::kChannel_4_1;
    case ::remoting::AudioPacket::CHANNELS_5_1:
      return remoting::mojom::AudioPacket_Channels::kChannel_5_1;
    case ::remoting::AudioPacket::CHANNELS_6_1:
      return remoting::mojom::AudioPacket_Channels::kChannel_6_1;
    case ::remoting::AudioPacket::CHANNELS_7_1:
      return remoting::mojom::AudioPacket_Channels::kChannel_7_1;
  }

  NOTREACHED();
}

// static
::remoting::AudioPacket::Channels
mojo::EnumTraits<remoting::mojom::AudioPacket_Channels,
                 ::remoting::AudioPacket::Channels>::
    FromMojom(remoting::mojom::AudioPacket_Channels input) {
  switch (input) {
    case remoting::mojom::AudioPacket_Channels::kInvalid:
      return ::remoting::AudioPacket::CHANNELS_INVALID;
    case remoting::mojom::AudioPacket_Channels::kMono:
      return ::remoting::AudioPacket::CHANNELS_MONO;
    case remoting::mojom::AudioPacket_Channels::kStereo:
      return ::remoting::AudioPacket::CHANNELS_STEREO;
    case remoting::mojom::AudioPacket_Channels::kSurround:
      return ::remoting::AudioPacket::CHANNELS_SURROUND;
    case remoting::mojom::AudioPacket_Channels::kChannel_4_0:
      return ::remoting::AudioPacket::CHANNELS_4_0;
    case remoting::mojom::AudioPacket_Channels::kChannel_4_1:
      return ::remoting::AudioPacket::CHANNELS_4_1;
    case remoting::mojom::AudioPacket_Channels::kChannel_5_1:
      return ::remoting::AudioPacket::CHANNELS_5_1;
    case remoting::mojom::AudioPacket_Channels::kChannel_6_1:
      return ::remoting::AudioPacket::CHANNELS_6_1;
    case remoting::mojom::AudioPacket_Channels::kChannel_7_1:
      return ::remoting::AudioPacket::CHANNELS_7_1;
  }

  NOTREACHED();
}

// static
remoting::mojom::AudioPacket_Encoding
mojo::EnumTraits<remoting::mojom::AudioPacket_Encoding,
                 ::remoting::AudioPacket::Encoding>::
    ToMojom(::remoting::AudioPacket::Encoding input) {
  switch (input) {
    case ::remoting::AudioPacket::ENCODING_INVALID:
      return remoting::mojom::AudioPacket_Encoding::kInvalid;
    case ::remoting::AudioPacket::ENCODING_RAW:
      return remoting::mojom::AudioPacket_Encoding::kRaw;
    case ::remoting::AudioPacket::ENCODING_OPUS:
      return remoting::mojom::AudioPacket_Encoding::kOpus;
  }

  NOTREACHED();
}

// static
::remoting::AudioPacket::Encoding
mojo::EnumTraits<remoting::mojom::AudioPacket_Encoding,
                 ::remoting::AudioPacket::Encoding>::
    FromMojom(remoting::mojom::AudioPacket_Encoding input) {
  switch (input) {
    case remoting::mojom::AudioPacket_Encoding::kInvalid:
      return ::remoting::AudioPacket::ENCODING_INVALID;
    case remoting::mojom::AudioPacket_Encoding::kRaw:
      return ::remoting::AudioPacket::ENCODING_RAW;
    case remoting::mojom::AudioPacket_Encoding::kOpus:
      return ::remoting::AudioPacket::ENCODING_OPUS;
  }

  NOTREACHED();
}

// static
remoting::mojom::AudioPacket_SamplingRate
mojo::EnumTraits<remoting::mojom::AudioPacket_SamplingRate,
                 ::remoting::AudioPacket::SamplingRate>::
    ToMojom(::remoting::AudioPacket::SamplingRate input) {
  switch (input) {
    case ::remoting::AudioPacket::SAMPLING_RATE_INVALID:
      return remoting::mojom::AudioPacket_SamplingRate::kInvalid;
    case ::remoting::AudioPacket::SAMPLING_RATE_44100:
      return remoting::mojom::AudioPacket_SamplingRate::kRate_44100;
    case ::remoting::AudioPacket::SAMPLING_RATE_48000:
      return remoting::mojom::AudioPacket_SamplingRate::kRate_48000;
  }

  NOTREACHED();
}

// static
::remoting::AudioPacket::SamplingRate
mojo::EnumTraits<remoting::mojom::AudioPacket_SamplingRate,
                 ::remoting::AudioPacket::SamplingRate>::
    FromMojom(remoting::mojom::AudioPacket_SamplingRate input) {
  switch (input) {
    case remoting::mojom::AudioPacket_SamplingRate::kInvalid:
      return ::remoting::AudioPacket::SAMPLING_RATE_INVALID;
    case remoting::mojom::AudioPacket_SamplingRate::kRate_44100:
      return ::remoting::AudioPacket::SAMPLING_RATE_44100;
    case remoting::mojom::AudioPacket_SamplingRate::kRate_48000:
      return ::remoting::AudioPacket::SAMPLING_RATE_48000;
  }

  NOTREACHED();
}

// static
remoting::mojom::FileTransferError_Type
mojo::EnumTraits<remoting::mojom::FileTransferError_Type,
                 ::remoting::protocol::FileTransfer_Error_Type>::
    ToMojom(::remoting::protocol::FileTransfer_Error_Type input) {
  switch (input) {
    case ::remoting::protocol::FileTransfer_Error::UNSPECIFIED:
      return remoting::mojom::FileTransferError_Type::kUnknown;
    case ::remoting::protocol::FileTransfer_Error::CANCELED:
      return remoting::mojom::FileTransferError_Type::kCanceled;
    case ::remoting::protocol::FileTransfer_Error::UNEXPECTED_ERROR:
      return remoting::mojom::FileTransferError_Type::kUnexpectedError;
    case ::remoting::protocol::FileTransfer_Error::PROTOCOL_ERROR:
      return remoting::mojom::FileTransferError_Type::kProtocolError;
    case ::remoting::protocol::FileTransfer_Error::PERMISSION_DENIED:
      return remoting::mojom::FileTransferError_Type::kPermissionDenied;
    case ::remoting::protocol::FileTransfer_Error::OUT_OF_DISK_SPACE:
      return remoting::mojom::FileTransferError_Type::kOutOfDiskSpace;
    case ::remoting::protocol::FileTransfer_Error::IO_ERROR:
      return remoting::mojom::FileTransferError_Type::kIoError;
    case ::remoting::protocol::FileTransfer_Error::NOT_LOGGED_IN:
      return remoting::mojom::FileTransferError_Type::kNotLoggedIn;
  }

  NOTREACHED();
}

// static
::remoting::protocol::FileTransfer_Error_Type
mojo::EnumTraits<remoting::mojom::FileTransferError_Type,
                 ::remoting::protocol::FileTransfer_Error_Type>::
    FromMojom(remoting::mojom::FileTransferError_Type input) {
  switch (input) {
    case remoting::mojom::FileTransferError_Type::kUnknown:
      return ::remoting::protocol::FileTransfer_Error::UNSPECIFIED;
    case remoting::mojom::FileTransferError_Type::kCanceled:
      return ::remoting::protocol::FileTransfer_Error::CANCELED;
    case remoting::mojom::FileTransferError_Type::kUnexpectedError:
      return ::remoting::protocol::FileTransfer_Error::UNEXPECTED_ERROR;
    case remoting::mojom::FileTransferError_Type::kProtocolError:
      return ::remoting::protocol::FileTransfer_Error::PROTOCOL_ERROR;
    case remoting::mojom::FileTransferError_Type::kPermissionDenied:
      return ::remoting::protocol::FileTransfer_Error::PERMISSION_DENIED;
    case remoting::mojom::FileTransferError_Type::kOutOfDiskSpace:
      return ::remoting::protocol::FileTransfer_Error::OUT_OF_DISK_SPACE;
    case remoting::mojom::FileTransferError_Type::kIoError:
      return ::remoting::protocol::FileTransfer_Error::IO_ERROR;
    case remoting::mojom::FileTransferError_Type::kNotLoggedIn:
      return ::remoting::protocol::FileTransfer_Error::NOT_LOGGED_IN;
  }

  NOTREACHED();
}

// static
remoting::mojom::LayoutKeyFunction
mojo::EnumTraits<remoting::mojom::LayoutKeyFunction,
                 ::remoting::protocol::LayoutKeyFunction>::
    ToMojom(::remoting::protocol::LayoutKeyFunction input) {
  switch (input) {
    case ::remoting::protocol::LayoutKeyFunction::UNKNOWN:
      return remoting::mojom::LayoutKeyFunction::kUnknown;
    case ::remoting::protocol::LayoutKeyFunction::CONTROL:
      return remoting::mojom::LayoutKeyFunction::kControl;
    case ::remoting::protocol::LayoutKeyFunction::ALT:
      return remoting::mojom::LayoutKeyFunction::kAlt;
    case ::remoting::protocol::LayoutKeyFunction::SHIFT:
      return remoting::mojom::LayoutKeyFunction::kShift;
    case ::remoting::protocol::LayoutKeyFunction::META:
      return remoting::mojom::LayoutKeyFunction::kMeta;
    case ::remoting::protocol::LayoutKeyFunction::ALT_GR:
      return remoting::mojom::LayoutKeyFunction::kAltGr;
    case ::remoting::protocol::LayoutKeyFunction::MOD5:
      return remoting::mojom::LayoutKeyFunction::kMod5;
    case ::remoting::protocol::LayoutKeyFunction::COMPOSE:
      return remoting::mojom::LayoutKeyFunction::kCompose;
    case ::remoting::protocol::LayoutKeyFunction::OPTION:
      return remoting::mojom::LayoutKeyFunction::kOption;
    case ::remoting::protocol::LayoutKeyFunction::COMMAND:
      return remoting::mojom::LayoutKeyFunction::kCommand;
    case ::remoting::protocol::LayoutKeyFunction::SEARCH:
      return remoting::mojom::LayoutKeyFunction::kSearch;
    case ::remoting::protocol::LayoutKeyFunction::NUM_LOCK:
      return remoting::mojom::LayoutKeyFunction::kNumLock;
    case ::remoting::protocol::LayoutKeyFunction::CAPS_LOCK:
      return remoting::mojom::LayoutKeyFunction::kCapsLock;
    case ::remoting::protocol::LayoutKeyFunction::SCROLL_LOCK:
      return remoting::mojom::LayoutKeyFunction::kScrollLock;
    case ::remoting::protocol::LayoutKeyFunction::BACKSPACE:
      return remoting::mojom::LayoutKeyFunction::kBackspace;
    case ::remoting::protocol::LayoutKeyFunction::ENTER:
      return remoting::mojom::LayoutKeyFunction::kEnter;
    case ::remoting::protocol::LayoutKeyFunction::TAB:
      return remoting::mojom::LayoutKeyFunction::kTab;
    case ::remoting::protocol::LayoutKeyFunction::INSERT:
      return remoting::mojom::LayoutKeyFunction::kInsert;
    case ::remoting::protocol::LayoutKeyFunction::DELETE_:
      return remoting::mojom::LayoutKeyFunction::kDelete;
    case ::remoting::protocol::LayoutKeyFunction::HOME:
      return remoting::mojom::LayoutKeyFunction::kHome;
    case ::remoting::protocol::LayoutKeyFunction::END:
      return remoting::mojom::LayoutKeyFunction::kEnd;
    case ::remoting::protocol::LayoutKeyFunction::PAGE_UP:
      return remoting::mojom::LayoutKeyFunction::kPageUp;
    case ::remoting::protocol::LayoutKeyFunction::PAGE_DOWN:
      return remoting::mojom::LayoutKeyFunction::kPageDown;
    case ::remoting::protocol::LayoutKeyFunction::CLEAR:
      return remoting::mojom::LayoutKeyFunction::kClear;
    case ::remoting::protocol::LayoutKeyFunction::ARROW_UP:
      return remoting::mojom::LayoutKeyFunction::kArrowUp;
    case ::remoting::protocol::LayoutKeyFunction::ARROW_DOWN:
      return remoting::mojom::LayoutKeyFunction::kArrowDown;
    case ::remoting::protocol::LayoutKeyFunction::ARROW_LEFT:
      return remoting::mojom::LayoutKeyFunction::kArrowLeft;
    case ::remoting::protocol::LayoutKeyFunction::ARROW_RIGHT:
      return remoting::mojom::LayoutKeyFunction::kArrowRight;
    case ::remoting::protocol::LayoutKeyFunction::F1:
      return remoting::mojom::LayoutKeyFunction::kF1;
    case ::remoting::protocol::LayoutKeyFunction::F2:
      return remoting::mojom::LayoutKeyFunction::kF2;
    case ::remoting::protocol::LayoutKeyFunction::F3:
      return remoting::mojom::LayoutKeyFunction::kF3;
    case ::remoting::protocol::LayoutKeyFunction::F4:
      return remoting::mojom::LayoutKeyFunction::kF4;
    case ::remoting::protocol::LayoutKeyFunction::F5:
      return remoting::mojom::LayoutKeyFunction::kF5;
    case ::remoting::protocol::LayoutKeyFunction::F6:
      return remoting::mojom::LayoutKeyFunction::kF6;
    case ::remoting::protocol::LayoutKeyFunction::F7:
      return remoting::mojom::LayoutKeyFunction::kF7;
    case ::remoting::protocol::LayoutKeyFunction::F8:
      return remoting::mojom::LayoutKeyFunction::kF8;
    case ::remoting::protocol::LayoutKeyFunction::F9:
      return remoting::mojom::LayoutKeyFunction::kF9;
    case ::remoting::protocol::LayoutKeyFunction::F10:
      return remoting::mojom::LayoutKeyFunction::kF10;
    case ::remoting::protocol::LayoutKeyFunction::F11:
      return remoting::mojom::LayoutKeyFunction::kF11;
    case ::remoting::protocol::LayoutKeyFunction::F12:
      return remoting::mojom::LayoutKeyFunction::kF12;
    case ::remoting::protocol::LayoutKeyFunction::F13:
      return remoting::mojom::LayoutKeyFunction::kF13;
    case ::remoting::protocol::LayoutKeyFunction::F14:
      return remoting::mojom::LayoutKeyFunction::kF14;
    case ::remoting::protocol::LayoutKeyFunction::F15:
      return remoting::mojom::LayoutKeyFunction::kF15;
    case ::remoting::protocol::LayoutKeyFunction::F16:
      return remoting::mojom::LayoutKeyFunction::kF16;
    case ::remoting::protocol::LayoutKeyFunction::F17:
      return remoting::mojom::LayoutKeyFunction::kF17;
    case ::remoting::protocol::LayoutKeyFunction::F18:
      return remoting::mojom::LayoutKeyFunction::kF18;
    case ::remoting::protocol::LayoutKeyFunction::F19:
      return remoting::mojom::LayoutKeyFunction::kF19;
    case ::remoting::protocol::LayoutKeyFunction::F20:
      return remoting::mojom::LayoutKeyFunction::kF20;
    case ::remoting::protocol::LayoutKeyFunction::F21:
      return remoting::mojom::LayoutKeyFunction::kF21;
    case ::remoting::protocol::LayoutKeyFunction::F22:
      return remoting::mojom::LayoutKeyFunction::kF22;
    case ::remoting::protocol::LayoutKeyFunction::F23:
      return remoting::mojom::LayoutKeyFunction::kF23;
    case ::remoting::protocol::LayoutKeyFunction::F24:
      return remoting::mojom::LayoutKeyFunction::kF24;
    case ::remoting::protocol::LayoutKeyFunction::ESCAPE:
      return remoting::mojom::LayoutKeyFunction::kEscape;
    case ::remoting::protocol::LayoutKeyFunction::CONTEXT_MENU:
      return remoting::mojom::LayoutKeyFunction::kContextMenu;
    case ::remoting::protocol::LayoutKeyFunction::PAUSE:
      return remoting::mojom::LayoutKeyFunction::kPause;
    case ::remoting::protocol::LayoutKeyFunction::PRINT_SCREEN:
      return remoting::mojom::LayoutKeyFunction::kPrintScreen;
    case ::remoting::protocol::LayoutKeyFunction::HANKAKU_ZENKAKU_KANJI:
      return remoting::mojom::LayoutKeyFunction::kHankakuZenkakuKanji;
    case ::remoting::protocol::LayoutKeyFunction::HENKAN:
      return remoting::mojom::LayoutKeyFunction::kHenkan;
    case ::remoting::protocol::LayoutKeyFunction::MUHENKAN:
      return remoting::mojom::LayoutKeyFunction::kMuhenkan;
    case ::remoting::protocol::LayoutKeyFunction::KATAKANA_HIRAGANA_ROMAJI:
      return remoting::mojom::LayoutKeyFunction::kKatakanaHiriganaRomaji;
    case ::remoting::protocol::LayoutKeyFunction::KANA:
      return remoting::mojom::LayoutKeyFunction::kKana;
    case ::remoting::protocol::LayoutKeyFunction::EISU:
      return remoting::mojom::LayoutKeyFunction::kEisu;
    case ::remoting::protocol::LayoutKeyFunction::HAN_YEONG:
      return remoting::mojom::LayoutKeyFunction::kHanYeong;
    case ::remoting::protocol::LayoutKeyFunction::HANJA:
      return remoting::mojom::LayoutKeyFunction::kHanja;
  }

  NOTREACHED();
}

// static
::remoting::protocol::LayoutKeyFunction
mojo::EnumTraits<remoting::mojom::LayoutKeyFunction,
                 ::remoting::protocol::LayoutKeyFunction>::
    FromMojom(remoting::mojom::LayoutKeyFunction input) {
  switch (input) {
    case remoting::mojom::LayoutKeyFunction::kUnknown:
      return ::remoting::protocol::LayoutKeyFunction::UNKNOWN;
    case remoting::mojom::LayoutKeyFunction::kControl:
      return ::remoting::protocol::LayoutKeyFunction::CONTROL;
    case remoting::mojom::LayoutKeyFunction::kAlt:
      return ::remoting::protocol::LayoutKeyFunction::ALT;
    case remoting::mojom::LayoutKeyFunction::kShift:
      return ::remoting::protocol::LayoutKeyFunction::SHIFT;
    case remoting::mojom::LayoutKeyFunction::kMeta:
      return ::remoting::protocol::LayoutKeyFunction::META;
    case remoting::mojom::LayoutKeyFunction::kAltGr:
      return ::remoting::protocol::LayoutKeyFunction::ALT_GR;
    case remoting::mojom::LayoutKeyFunction::kMod5:
      return ::remoting::protocol::LayoutKeyFunction::MOD5;
    case remoting::mojom::LayoutKeyFunction::kCompose:
      return ::remoting::protocol::LayoutKeyFunction::COMPOSE;
    case remoting::mojom::LayoutKeyFunction::kOption:
      return ::remoting::protocol::LayoutKeyFunction::OPTION;
    case remoting::mojom::LayoutKeyFunction::kCommand:
      return ::remoting::protocol::LayoutKeyFunction::COMMAND;
    case remoting::mojom::LayoutKeyFunction::kSearch:
      return ::remoting::protocol::LayoutKeyFunction::SEARCH;
    case remoting::mojom::LayoutKeyFunction::kNumLock:
      return ::remoting::protocol::LayoutKeyFunction::NUM_LOCK;
    case remoting::mojom::LayoutKeyFunction::kCapsLock:
      return ::remoting::protocol::LayoutKeyFunction::CAPS_LOCK;
    case remoting::mojom::LayoutKeyFunction::kScrollLock:
      return ::remoting::protocol::LayoutKeyFunction::SCROLL_LOCK;
    case remoting::mojom::LayoutKeyFunction::kBackspace:
      return ::remoting::protocol::LayoutKeyFunction::BACKSPACE;
    case remoting::mojom::LayoutKeyFunction::kEnter:
      return ::remoting::protocol::LayoutKeyFunction::ENTER;
    case remoting::mojom::LayoutKeyFunction::kTab:
      return ::remoting::protocol::LayoutKeyFunction::TAB;
    case remoting::mojom::LayoutKeyFunction::kInsert:
      return ::remoting::protocol::LayoutKeyFunction::INSERT;
    case remoting::mojom::LayoutKeyFunction::kDelete:
      return ::remoting::protocol::LayoutKeyFunction::DELETE_;
    case remoting::mojom::LayoutKeyFunction::kHome:
      return ::remoting::protocol::LayoutKeyFunction::HOME;
    case remoting::mojom::LayoutKeyFunction::kEnd:
      return ::remoting::protocol::LayoutKeyFunction::END;
    case remoting::mojom::LayoutKeyFunction::kPageUp:
      return ::remoting::protocol::LayoutKeyFunction::PAGE_UP;
    case remoting::mojom::LayoutKeyFunction::kPageDown:
      return ::remoting::protocol::LayoutKeyFunction::PAGE_DOWN;
    case remoting::mojom::LayoutKeyFunction::kClear:
      return ::remoting::protocol::LayoutKeyFunction::CLEAR;
    case remoting::mojom::LayoutKeyFunction::kArrowUp:
      return ::remoting::protocol::LayoutKeyFunction::ARROW_UP;
    case remoting::mojom::LayoutKeyFunction::kArrowDown:
      return ::remoting::protocol::LayoutKeyFunction::ARROW_DOWN;
    case remoting::mojom::LayoutKeyFunction::kArrowLeft:
      return ::remoting::protocol::LayoutKeyFunction::ARROW_LEFT;
    case remoting::mojom::LayoutKeyFunction::kArrowRight:
      return ::remoting::protocol::LayoutKeyFunction::ARROW_RIGHT;
    case remoting::mojom::LayoutKeyFunction::kF1:
      return ::remoting::protocol::LayoutKeyFunction::F1;
    case remoting::mojom::LayoutKeyFunction::kF2:
      return ::remoting::protocol::LayoutKeyFunction::F2;
    case remoting::mojom::LayoutKeyFunction::kF3:
      return ::remoting::protocol::LayoutKeyFunction::F3;
    case remoting::mojom::LayoutKeyFunction::kF4:
      return ::remoting::protocol::LayoutKeyFunction::F4;
    case remoting::mojom::LayoutKeyFunction::kF5:
      return ::remoting::protocol::LayoutKeyFunction::F5;
    case remoting::mojom::LayoutKeyFunction::kF6:
      return ::remoting::protocol::LayoutKeyFunction::F6;
    case remoting::mojom::LayoutKeyFunction::kF7:
      return ::remoting::protocol::LayoutKeyFunction::F7;
    case remoting::mojom::LayoutKeyFunction::kF8:
      return ::remoting::protocol::LayoutKeyFunction::F8;
    case remoting::mojom::LayoutKeyFunction::kF9:
      return ::remoting::protocol::LayoutKeyFunction::F9;
    case remoting::mojom::LayoutKeyFunction::kF10:
      return ::remoting::protocol::LayoutKeyFunction::F10;
    case remoting::mojom::LayoutKeyFunction::kF11:
      return ::remoting::protocol::LayoutKeyFunction::F11;
    case remoting::mojom::LayoutKeyFunction::kF12:
      return ::remoting::protocol::LayoutKeyFunction::F12;
    case remoting::mojom::LayoutKeyFunction::kF13:
      return ::remoting::protocol::LayoutKeyFunction::F13;
    case remoting::mojom::LayoutKeyFunction::kF14:
      return ::remoting::protocol::LayoutKeyFunction::F14;
    case remoting::mojom::LayoutKeyFunction::kF15:
      return ::remoting::protocol::LayoutKeyFunction::F15;
    case remoting::mojom::LayoutKeyFunction::kF16:
      return ::remoting::protocol::LayoutKeyFunction::F16;
    case remoting::mojom::LayoutKeyFunction::kF17:
      return ::remoting::protocol::LayoutKeyFunction::F17;
    case remoting::mojom::LayoutKeyFunction::kF18:
      return ::remoting::protocol::LayoutKeyFunction::F18;
    case remoting::mojom::LayoutKeyFunction::kF19:
      return ::remoting::protocol::LayoutKeyFunction::F19;
    case remoting::mojom::LayoutKeyFunction::kF20:
      return ::remoting::protocol::LayoutKeyFunction::F20;
    case remoting::mojom::LayoutKeyFunction::kF21:
      return ::remoting::protocol::LayoutKeyFunction::F21;
    case remoting::mojom::LayoutKeyFunction::kF22:
      return ::remoting::protocol::LayoutKeyFunction::F22;
    case remoting::mojom::LayoutKeyFunction::kF23:
      return ::remoting::protocol::LayoutKeyFunction::F23;
    case remoting::mojom::LayoutKeyFunction::kF24:
      return ::remoting::protocol::LayoutKeyFunction::F24;
    case remoting::mojom::LayoutKeyFunction::kEscape:
      return ::remoting::protocol::LayoutKeyFunction::ESCAPE;
    case remoting::mojom::LayoutKeyFunction::kContextMenu:
      return ::remoting::protocol::LayoutKeyFunction::CONTEXT_MENU;
    case remoting::mojom::LayoutKeyFunction::kPause:
      return ::remoting::protocol::LayoutKeyFunction::PAUSE;
    case remoting::mojom::LayoutKeyFunction::kPrintScreen:
      return ::remoting::protocol::LayoutKeyFunction::PRINT_SCREEN;
    case remoting::mojom::LayoutKeyFunction::kHankakuZenkakuKanji:
      return ::remoting::protocol::LayoutKeyFunction::HANKAKU_ZENKAKU_KANJI;
    case remoting::mojom::LayoutKeyFunction::kHenkan:
      return ::remoting::protocol::LayoutKeyFunction::HENKAN;
    case remoting::mojom::LayoutKeyFunction::kMuhenkan:
      return ::remoting::protocol::LayoutKeyFunction::MUHENKAN;
    case remoting::mojom::LayoutKeyFunction::kKatakanaHiriganaRomaji:
      return ::remoting::protocol::LayoutKeyFunction::KATAKANA_HIRAGANA_ROMAJI;
    case remoting::mojom::LayoutKeyFunction::kKana:
      return ::remoting::protocol::LayoutKeyFunction::KANA;
    case remoting::mojom::LayoutKeyFunction::kEisu:
      return ::remoting::protocol::LayoutKeyFunction::EISU;
    case remoting::mojom::LayoutKeyFunction::kHanYeong:
      return ::remoting::protocol::LayoutKeyFunction::HAN_YEONG;
    case remoting::mojom::LayoutKeyFunction::kHanja:
      return ::remoting::protocol::LayoutKeyFunction::HANJA;
  }

  NOTREACHED();
}

// static
remoting::mojom::TouchEventType
mojo::EnumTraits<remoting::mojom::TouchEventType,
                 ::remoting::protocol::TouchEvent::TouchEventType>::
    ToMojom(::remoting::protocol::TouchEvent::TouchEventType input) {
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
}

// static
::remoting::protocol::TouchEvent::TouchEventType
mojo::EnumTraits<remoting::mojom::TouchEventType,
                 ::remoting::protocol::TouchEvent::TouchEventType>::
    FromMojom(remoting::mojom::TouchEventType input) {
  switch (input) {
    case remoting::mojom::TouchEventType::kUndefined:
      return ::remoting::protocol::TouchEvent::TOUCH_POINT_UNDEFINED;
    case remoting::mojom::TouchEventType::kStart:
      return ::remoting::protocol::TouchEvent::TOUCH_POINT_START;
    case remoting::mojom::TouchEventType::kMove:
      return ::remoting::protocol::TouchEvent::TOUCH_POINT_MOVE;
    case remoting::mojom::TouchEventType::kEnd:
      return ::remoting::protocol::TouchEvent::TOUCH_POINT_END;
    case remoting::mojom::TouchEventType::kCancel:
      return ::remoting::protocol::TouchEvent::TOUCH_POINT_CANCEL;
  }

  NOTREACHED();
}

// static
remoting::mojom::TransportRouteType
mojo::EnumTraits<remoting::mojom::TransportRouteType,
                 ::remoting::protocol::TransportRoute::RouteType>::
    ToMojom(::remoting::protocol::TransportRoute::RouteType input) {
  switch (input) {
    case ::remoting::protocol::TransportRoute::RouteType::DIRECT:
      return remoting::mojom::TransportRouteType::kDirect;
    case ::remoting::protocol::TransportRoute::RouteType::STUN:
      return remoting::mojom::TransportRouteType::kStun;
    case ::remoting::protocol::TransportRoute::RouteType::RELAY:
      return remoting::mojom::TransportRouteType::kRelay;
  }

  NOTREACHED();
}

// static
std::optional<::remoting::protocol::TransportRoute::RouteType>
mojo::EnumTraits<remoting::mojom::TransportRouteType,
                 ::remoting::protocol::TransportRoute::RouteType>::
    FromMojom(remoting::mojom::TransportRouteType input) {
  switch (input) {
    case remoting::mojom::TransportRouteType::kUndefined:
      // kUndefined does not map to a value in TransportRoute::RouteType.
      return std::nullopt;
    case remoting::mojom::TransportRouteType::kDirect:
      return ::remoting::protocol::TransportRoute::RouteType::DIRECT;
    case remoting::mojom::TransportRouteType::kStun:
      return ::remoting::protocol::TransportRoute::RouteType::STUN;
    case remoting::mojom::TransportRouteType::kRelay:
      return ::remoting::protocol::TransportRoute::RouteType::RELAY;
  }

  return std::nullopt;
}

// static
remoting::mojom::VideoLayout_PixelType
mojo::EnumTraits<remoting::mojom::VideoLayout_PixelType,
                 ::remoting::protocol::VideoLayout::PixelType>::
    ToMojom(::remoting::protocol::VideoLayout::PixelType input) {
  switch (input) {
    case ::remoting::protocol::VideoLayout::UNSPECIFIED_PIXEL_TYPE:
      return remoting::mojom::VideoLayout_PixelType::kUnspecifiedPixelType;
    case ::remoting::protocol::VideoLayout::PHYSICAL:
      return remoting::mojom::VideoLayout_PixelType::kPhysical;
    case ::remoting::protocol::VideoLayout::LOGICAL:
      return remoting::mojom::VideoLayout_PixelType::kLogical;
  }

  NOTREACHED();
}

// static
::remoting::protocol::VideoLayout::PixelType
mojo::EnumTraits<remoting::mojom::VideoLayout_PixelType,
                 ::remoting::protocol::VideoLayout::PixelType>::
    FromMojom(remoting::mojom::VideoLayout_PixelType input) {
  switch (input) {
    case remoting::mojom::VideoLayout_PixelType::kUnspecifiedPixelType:
      return ::remoting::protocol::VideoLayout::UNSPECIFIED_PIXEL_TYPE;
    case remoting::mojom::VideoLayout_PixelType::kPhysical:
      return ::remoting::protocol::VideoLayout::PHYSICAL;
    case remoting::mojom::VideoLayout_PixelType::kLogical:
      return ::remoting::protocol::VideoLayout::LOGICAL;
  }

  NOTREACHED();
}

// static
remoting::mojom::WebrtcIceCandidateType mojo::EnumTraits<
    remoting::mojom::WebrtcIceCandidateType,
    ::webrtc::IceCandidateType>::ToMojom(::webrtc::IceCandidateType type) {
  switch (type) {
    case ::webrtc::IceCandidateType::kHost:
      return remoting::mojom::WebrtcIceCandidateType::kHost;
    case ::webrtc::IceCandidateType::kSrflx:
      return remoting::mojom::WebrtcIceCandidateType::kSrflx;
    case ::webrtc::IceCandidateType::kPrflx:
      return remoting::mojom::WebrtcIceCandidateType::kPrflx;
    case ::webrtc::IceCandidateType::kRelay:
      return remoting::mojom::WebrtcIceCandidateType::kRelay;
  }
  NOTREACHED();
}

// static
std::optional<::webrtc::IceCandidateType>
mojo::EnumTraits<remoting::mojom::WebrtcIceCandidateType,
                 ::webrtc::IceCandidateType>::
    FromMojom(remoting::mojom::WebrtcIceCandidateType input) {
  switch (input) {
    case remoting::mojom::WebrtcIceCandidateType::kHost:
      return ::webrtc::IceCandidateType::kHost;
    case remoting::mojom::WebrtcIceCandidateType::kSrflx:
      return ::webrtc::IceCandidateType::kSrflx;
    case remoting::mojom::WebrtcIceCandidateType::kPrflx:
      return ::webrtc::IceCandidateType::kPrflx;
    case remoting::mojom::WebrtcIceCandidateType::kRelay:
      return ::webrtc::IceCandidateType::kRelay;
  }
  return std::nullopt;
}

// static
remoting::mojom::SdpType
mojo::EnumTraits<remoting::mojom::SdpType,
                 ::remoting::SessionDescription::Type>::
    ToMojom(::remoting::SessionDescription::Type type) {
  switch (type) {
    case ::remoting::SessionDescription::Type::kUnspecified:
      return remoting::mojom::SdpType::kUnspecified;
    case ::remoting::SessionDescription::Type::kOffer:
      return remoting::mojom::SdpType::kOffer;
    case ::remoting::SessionDescription::Type::kAnswer:
      return remoting::mojom::SdpType::kAnswer;
  }
  NOTREACHED();
}

// static
std::optional<::remoting::SessionDescription::Type> mojo::EnumTraits<
    remoting::mojom::SdpType,
    ::remoting::SessionDescription::Type>::FromMojom(remoting::mojom::SdpType
                                                         input) {
  switch (input) {
    case remoting::mojom::SdpType::kUnspecified:
      return ::remoting::SessionDescription::Type::kUnspecified;
    case remoting::mojom::SdpType::kOffer:
      return ::remoting::SessionDescription::Type::kOffer;
    case remoting::mojom::SdpType::kAnswer:
      return ::remoting::SessionDescription::Type::kAnswer;
  }
  return std::nullopt;
}

#endif  // BUILDFLAG(REMOTING_MULTI_PROCESS)

}  // namespace mojo
