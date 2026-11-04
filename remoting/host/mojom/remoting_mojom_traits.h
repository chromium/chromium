// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_MOJOM_REMOTING_MOJOM_TRAITS_H_
#define REMOTING_HOST_MOJOM_REMOTING_MOJOM_TRAITS_H_

#include <stddef.h>

#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/numerics/safe_conversions.h"
#include "base/numerics/safe_math.h"
#include "build/build_config.h"
#include "mojo/public/cpp/base/byte_string_mojom_traits.h"
#include "mojo/public/cpp/base/file_path_mojom_traits.h"
#include "mojo/public/cpp/base/time_mojom_traits.h"
#include "mojo/public/cpp/bindings/array_traits.h"
#include "mojo/public/cpp/bindings/array_traits_protobuf.h"
#include "mojo/public/cpp/bindings/enum_traits.h"
#include "mojo/public/cpp/bindings/map_traits_protobuf.h"
#include "mojo/public/cpp/bindings/optional_as_pointer.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "remoting/base/buildflags.h"
#include "remoting/base/errors.h"
#include "remoting/base/ipc_fifo_buffer.h"
#include "remoting/base/port_range.h"
#include "remoting/base/result.h"
#include "remoting/base/session_options.h"
#include "remoting/base/session_policies.h"
#include "remoting/base/source_location.h"
#include "remoting/host/mojom/common.mojom-shared.h"
#include "remoting/proto/control.pb.h"
#include "remoting/proto/terminal.pb.h"

#if BUILDFLAG(REMOTING_MULTI_PROCESS)
#include "remoting/host/base/desktop_environment_options.h"
#include "remoting/host/base/screen_resolution.h"
#include "remoting/host/mojom/desktop_session.mojom-shared.h"
#include "remoting/host/mojom/peer_session.mojom-shared.h"
#include "remoting/host/mojom/remoting_host.mojom-shared.h"
#include "remoting/host/mojom/webrtc_types.mojom-shared.h"
#include "remoting/proto/audio.pb.h"
#include "remoting/proto/coordinates.pb.h"
#include "remoting/proto/event.pb.h"
#include "remoting/proto/file_transfer.pb.h"
#include "remoting/protocol/audio_sample_info.h"
#include "remoting/protocol/file_transfer_helpers.h"
#include "remoting/protocol/ice_config.h"
#include "remoting/protocol/transport.h"
#include "remoting/signaling/jingle_data_structures.h"
#include "services/network/public/cpp/ip_endpoint_mojom_traits.h"
#include "third_party/webrtc/api/candidate.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"
#include "third_party/webrtc/modules/desktop_capture/mouse_cursor.h"
#include "third_party/webrtc/p2p/base/port.h"
#include "third_party/webrtc/p2p/base/port_allocator.h"
#include "third_party/webrtc/rtc_base/net_helper.h"
#include "third_party/webrtc/rtc_base/socket_address.h"
#include "ui/gfx/geometry/mojom/geometry_mojom_traits.h"
#endif  // BUILDFLAG(REMOTING_MULTI_PROCESS)

namespace mojo {

#if BUILDFLAG(REMOTING_MULTI_PROCESS)

template <>
class StructTraits<remoting::mojom::DesktopCaptureOptionsDataView,
                   ::webrtc::DesktopCaptureOptions> {
 public:
  static bool use_update_notifications(
      const ::webrtc::DesktopCaptureOptions& options) {
    return options.use_update_notifications();
  }

  static bool detect_updated_region(
      const ::webrtc::DesktopCaptureOptions& options) {
    return options.detect_updated_region();
  }

#if BUILDFLAG(IS_WIN)
  static bool allow_directx_capturer(
      const ::webrtc::DesktopCaptureOptions& options) {
    return options.allow_directx_capturer();
  }
#endif  // BUILDFLAG(IS_WIN)

  static bool Read(remoting::mojom::DesktopCaptureOptionsDataView data_view,
                   ::webrtc::DesktopCaptureOptions* out_options);
};

template <>
class StructTraits<remoting::mojom::DesktopEnvironmentOptionsDataView,
                   ::remoting::DesktopEnvironmentOptions> {
 public:
  static bool enable_curtaining(
      const ::remoting::DesktopEnvironmentOptions& options) {
    return options.enable_curtaining();
  }

  static bool enable_privacy_overlay(
      const ::remoting::DesktopEnvironmentOptions& options) {
    return options.enable_privacy_overlay();
  }

  static bool enable_user_interface(
      const ::remoting::DesktopEnvironmentOptions& options) {
    return options.enable_user_interface();
  }

  static bool enable_notifications(
      const ::remoting::DesktopEnvironmentOptions& options) {
    return options.enable_notifications();
  }

  static bool terminate_upon_input(
      const ::remoting::DesktopEnvironmentOptions& options) {
    return options.terminate_upon_input();
  }

  static bool enable_remote_webauthn(
      const ::remoting::DesktopEnvironmentOptions& options) {
    return options.enable_remote_webauthn();
  }

  static bool enable_security_key(
      const ::remoting::DesktopEnvironmentOptions& options) {
    return options.enable_security_key();
  }

  static const webrtc::DesktopCaptureOptions& desktop_capture_options(
      const ::remoting::DesktopEnvironmentOptions& options) {
    return *options.desktop_capture_options();
  }

  static bool Read(remoting::mojom::DesktopEnvironmentOptionsDataView data_view,
                   ::remoting::DesktopEnvironmentOptions* out_options);
};

template <>
struct EnumTraits<remoting::mojom::DesktopCaptureResult,
                  ::webrtc::DesktopCapturer::Result> {
  static remoting::mojom::DesktopCaptureResult ToMojom(
      ::webrtc::DesktopCapturer::Result input);
  static ::webrtc::DesktopCapturer::Result FromMojom(
      remoting::mojom::DesktopCaptureResult input);
};

template <>
class StructTraits<remoting::mojom::DesktopRectDataView,
                   ::webrtc::DesktopRect> {
 public:
  static int32_t left(const ::webrtc::DesktopRect& rect) { return rect.left(); }

  static int32_t top(const ::webrtc::DesktopRect& rect) { return rect.top(); }

  static int32_t right(const ::webrtc::DesktopRect& rect) {
    return rect.right();
  }

  static int32_t bottom(const ::webrtc::DesktopRect& rect) {
    return rect.bottom();
  }

  static bool Read(remoting::mojom::DesktopRectDataView data_view,
                   ::webrtc::DesktopRect* out_rect);
};

template <>
class StructTraits<remoting::mojom::DesktopSizeDataView,
                   ::webrtc::DesktopSize> {
 public:
  static int32_t width(const ::webrtc::DesktopSize& size) {
    return size.width();
  }

  static int32_t height(const ::webrtc::DesktopSize& size) {
    return size.height();
  }

  static bool Read(remoting::mojom::DesktopSizeDataView data_view,
                   ::webrtc::DesktopSize* out_size);
};

template <>
class StructTraits<remoting::mojom::DesktopVectorDataView,
                   ::webrtc::DesktopVector> {
 public:
  static int32_t x(const ::webrtc::DesktopVector& vector) { return vector.x(); }

  static int32_t y(const ::webrtc::DesktopVector& vector) { return vector.y(); }

  static bool Read(remoting::mojom::DesktopVectorDataView data_view,
                   ::webrtc::DesktopVector* out_vector);
};

template <>
class StructTraits<remoting::mojom::MouseCursorDataView,
                   ::webrtc::MouseCursor> {
 public:
  static const webrtc::DesktopSize& image_size(
      const ::webrtc::MouseCursor& cursor) {
    return cursor.image()->size();
  }

  static base::span<const uint8_t> image_data(
      const ::webrtc::MouseCursor& cursor) {
    auto& image_size = cursor.image()->size();
    base::CheckedNumeric<size_t> buffer_size(
        ::webrtc::DesktopFrame::kBytesPerPixel);
    buffer_size *= image_size.width();
    buffer_size *= image_size.height();
    CHECK_EQ(cursor.image()->pixel_format(), webrtc::FOURCC_ARGB);
    return UNSAFE_TODO(base::span<const uint8_t>(cursor.image()->data(),
                                                 buffer_size.ValueOrDie()));
  }

  static const webrtc::DesktopVector& hotspot(
      const ::webrtc::MouseCursor& cursor) {
    return cursor.hotspot();
  }

  static bool Read(remoting::mojom::MouseCursorDataView data_view,
                   ::webrtc::MouseCursor* out_cursor);
};

template <>
struct EnumTraits<remoting::mojom::MouseButton,
                  ::remoting::protocol::MouseEvent::MouseButton> {
  static remoting::mojom::MouseButton ToMojom(
      ::remoting::protocol::MouseEvent::MouseButton input);
  static ::remoting::protocol::MouseEvent::MouseButton FromMojom(
      remoting::mojom::MouseButton input);
};

template <>
struct EnumTraits<remoting::mojom::AudioPacket_BytesPerSample,
                  ::remoting::AudioPacket::BytesPerSample> {
  static remoting::mojom::AudioPacket_BytesPerSample ToMojom(
      ::remoting::AudioPacket::BytesPerSample input);
  static ::remoting::AudioPacket::BytesPerSample FromMojom(
      remoting::mojom::AudioPacket_BytesPerSample input);
};

template <>
struct EnumTraits<remoting::mojom::AudioPacket_Channels,
                  ::remoting::AudioPacket::Channels> {
  static remoting::mojom::AudioPacket_Channels ToMojom(
      ::remoting::AudioPacket::Channels input);
  static ::remoting::AudioPacket::Channels FromMojom(
      remoting::mojom::AudioPacket_Channels input);
};

template <>
struct EnumTraits<remoting::mojom::AudioPacket_Encoding,
                  ::remoting::AudioPacket::Encoding> {
  static remoting::mojom::AudioPacket_Encoding ToMojom(
      ::remoting::AudioPacket::Encoding input);
  static ::remoting::AudioPacket::Encoding FromMojom(
      remoting::mojom::AudioPacket_Encoding input);
};

template <>
struct EnumTraits<remoting::mojom::AudioPacket_SamplingRate,
                  ::remoting::AudioPacket::SamplingRate> {
  static remoting::mojom::AudioPacket_SamplingRate ToMojom(
      ::remoting::AudioPacket::SamplingRate input);
  static ::remoting::AudioPacket::SamplingRate FromMojom(
      remoting::mojom::AudioPacket_SamplingRate input);
};

template <>
class StructTraits<remoting::mojom::AudioPacketDataView,
                   ::std::unique_ptr<::remoting::AudioPacket>> {
 public:
  static int32_t timestamp(
      const ::std::unique_ptr<::remoting::AudioPacket>& packet) {
    return packet->timestamp();
  }

  static const ::google::protobuf::RepeatedPtrField<std::string>& data(
      const ::std::unique_ptr<::remoting::AudioPacket>& packet) {
    return packet->data();
  }

  static ::remoting::AudioPacket::Encoding encoding(
      const ::std::unique_ptr<::remoting::AudioPacket>& packet) {
    return packet->encoding();
  }

  static ::remoting::AudioPacket::SamplingRate sampling_rate(
      const ::std::unique_ptr<::remoting::AudioPacket>& packet) {
    return packet->sampling_rate();
  }

  static ::remoting::AudioPacket::BytesPerSample bytes_per_sample(
      const ::std::unique_ptr<::remoting::AudioPacket>& packet) {
    return packet->bytes_per_sample();
  }

  static ::remoting::AudioPacket::Channels channels(
      const ::std::unique_ptr<::remoting::AudioPacket>& packet) {
    return packet->channels();
  }

  static bool Read(remoting::mojom::AudioPacketDataView data_view,
                   ::std::unique_ptr<::remoting::AudioPacket>* out_packet);
};

template <>
class StructTraits<remoting::mojom::ClipboardEventDataView,
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
class UnionTraits<
    remoting::mojom::ReadChunkResultDataView,
    ::remoting::Result<std::vector<uint8_t>,
                       ::remoting::protocol::FileTransfer_Error>> {
 public:
  static remoting::mojom::ReadChunkResultDataView::Tag GetTag(
      const ::remoting::Result<std::vector<uint8_t>,
                               ::remoting::protocol::FileTransfer_Error>&
          result) {
    if (result.is_success()) {
      return remoting::mojom::ReadChunkResultDataView::Tag::kData;
    } else if (result.is_error()) {
      return remoting::mojom::ReadChunkResultDataView::Tag::kError;
    }

    NOTREACHED();
  }

  static const std::vector<uint8_t>& data(
      const ::remoting::Result<std::vector<uint8_t>,
                               ::remoting::protocol::FileTransfer_Error>&
          result) {
    return result.success();
  }

  static const ::remoting::protocol::FileTransfer_Error& error(
      const ::remoting::Result<std::vector<uint8_t>,
                               ::remoting::protocol::FileTransfer_Error>&
          result) {
    return result.error();
  }

  static bool Read(
      remoting::mojom::ReadChunkResultDataView data_view,
      ::remoting::Result<std::vector<uint8_t>,
                         ::remoting::protocol::FileTransfer_Error>* out_result);
};

template <>
class StructTraits<remoting::mojom::FileTransferErrorDataView,
                   ::remoting::protocol::FileTransfer_Error> {
 public:
  static ::remoting::protocol::FileTransfer_Error_Type type(
      const ::remoting::protocol::FileTransfer_Error& error) {
    return error.type();
  }

  static std::optional<int32_t> api_error_code(
      const ::remoting::protocol::FileTransfer_Error& error) {
    if (error.has_api_error_code()) {
      return error.api_error_code();
    }
    return std::nullopt;
  }

  static const std::string& function(
      const ::remoting::protocol::FileTransfer_Error& error) {
    return error.function();
  }

  static const std::string& source_file(
      const ::remoting::protocol::FileTransfer_Error& error) {
    return error.source_file();
  }

  static uint32_t line_number(
      const ::remoting::protocol::FileTransfer_Error& error) {
    return error.line_number();
  }

  static bool Read(remoting::mojom::FileTransferErrorDataView data_view,
                   ::remoting::protocol::FileTransfer_Error* out_error);
};

template <>
struct EnumTraits<remoting::mojom::FileTransferError_Type,
                  ::remoting::protocol::FileTransfer_Error_Type> {
  static remoting::mojom::FileTransferError_Type ToMojom(
      ::remoting::protocol::FileTransfer_Error_Type input);
  static ::remoting::protocol::FileTransfer_Error_Type FromMojom(
      remoting::mojom::FileTransferError_Type input);
};

#if BUILDFLAG(IS_WIN)
template <>
class UnionTraits<
    remoting::mojom::FileChooserResultDataView,
    ::remoting::Result<base::FilePath,
                       ::remoting::protocol::FileTransfer_Error>> {
 public:
  static remoting::mojom::FileChooserResultDataView::Tag GetTag(
      const ::remoting::Result<base::FilePath,
                               ::remoting::protocol::FileTransfer_Error>&
          result) {
    if (result.is_success()) {
      return remoting::mojom::FileChooserResultDataView::Tag::kFilepath;
    } else if (result.is_error()) {
      return remoting::mojom::FileChooserResultDataView::Tag::kError;
    }

    NOTREACHED();
  }

  static const base::FilePath& filepath(
      const ::remoting::Result<base::FilePath,
                               ::remoting::protocol::FileTransfer_Error>&
          result) {
    return result.success();
  }

  static const ::remoting::protocol::FileTransfer_Error& error(
      const ::remoting::Result<base::FilePath,
                               ::remoting::protocol::FileTransfer_Error>&
          result) {
    return result.error();
  }

  static bool Read(
      remoting::mojom::FileChooserResultDataView data_view,
      ::remoting::Result<base::FilePath,
                         ::remoting::protocol::FileTransfer_Error>* out_result);
};
#endif  // BUILDFLAG(IS_WIN)

template <>
class StructTraits<remoting::mojom::KeyboardLayoutDataView,
                   ::remoting::protocol::KeyboardLayout> {
 public:
  static const ::google::protobuf::
      Map<uint32_t, ::remoting::protocol::KeyboardLayout_KeyBehavior>&
      keys(const ::remoting::protocol::KeyboardLayout& layout) {
    return layout.keys();
  }

  static bool Read(remoting::mojom::KeyboardLayoutDataView data_view,
                   ::remoting::protocol::KeyboardLayout* out_layout);
};

template <>
class UnionTraits<remoting::mojom::KeyActionDataView,
                  ::remoting::protocol::KeyboardLayout_KeyAction> {
 public:
  static remoting::mojom::KeyActionDataView::Tag GetTag(
      const ::remoting::protocol::KeyboardLayout_KeyAction& value) {
    switch (value.action_case()) {
      case ::remoting::protocol::KeyboardLayout_KeyAction::kFunction:
        return remoting::mojom::KeyActionDataView::Tag::kFunction;
      case ::remoting::protocol::KeyboardLayout_KeyAction::kCharacter:
        return remoting::mojom::KeyActionDataView::Tag::kCharacter;
      case ::remoting::protocol::KeyboardLayout_KeyAction::ACTION_NOT_SET:
        NOTREACHED();
    }
  }

  static ::remoting::protocol::LayoutKeyFunction function(
      const ::remoting::protocol::KeyboardLayout_KeyAction& value) {
    return value.function();
  }

  static const std::string& character(
      const ::remoting::protocol::KeyboardLayout_KeyAction& value) {
    return value.character();
  }

  static bool Read(remoting::mojom::KeyActionDataView data_view,
                   ::remoting::protocol::KeyboardLayout_KeyAction* out_action);
};

template <>
class StructTraits<remoting::mojom::KeyBehaviorDataView,
                   ::remoting::protocol::KeyboardLayout_KeyBehavior> {
 public:
  static const ::google::protobuf::Map<
      uint32_t,
      ::remoting::protocol::KeyboardLayout_KeyAction>&
  actions(const ::remoting::protocol::KeyboardLayout_KeyBehavior& behavior) {
    return behavior.actions();
  }
  static bool Read(
      remoting::mojom::KeyBehaviorDataView data_view,
      ::remoting::protocol::KeyboardLayout_KeyBehavior* out_behavior);
};

template <>
struct EnumTraits<remoting::mojom::LayoutKeyFunction,
                  ::remoting::protocol::LayoutKeyFunction> {
  static remoting::mojom::LayoutKeyFunction ToMojom(
      ::remoting::protocol::LayoutKeyFunction input);
  static ::remoting::protocol::LayoutKeyFunction FromMojom(
      remoting::mojom::LayoutKeyFunction input);
};

template <>
class StructTraits<remoting::mojom::KeyEventDataView,
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

  static std::optional<bool> caps_lock_state(
      const ::remoting::protocol::KeyEvent& event) {
    if (event.has_caps_lock_state()) {
      return event.caps_lock_state();
    }
    return std::nullopt;
  }

  static std::optional<bool> num_lock_state(
      const ::remoting::protocol::KeyEvent& event) {
    if (event.has_num_lock_state()) {
      return event.num_lock_state();
    }
    return std::nullopt;
  }

  static bool Read(remoting::mojom::KeyEventDataView data_view,
                   ::remoting::protocol::KeyEvent* out_event);
};

template <>
class StructTraits<remoting::mojom::MouseEventDataView,
                   ::remoting::protocol::MouseEvent> {
 public:
  static std::optional<int32_t> x(
      const ::remoting::protocol::MouseEvent& event) {
    if (event.has_x()) {
      return event.x();
    }
    return std::nullopt;
  }

  static std::optional<int32_t> y(
      const ::remoting::protocol::MouseEvent& event) {
    if (event.has_y()) {
      return event.y();
    }
    return std::nullopt;
  }

  static ::remoting::protocol::MouseEvent::MouseButton button(
      const ::remoting::protocol::MouseEvent& event) {
    if (event.has_button()) {
      return event.button();
    }
    return ::remoting::protocol::MouseEvent::BUTTON_UNDEFINED;
  }

  static std::optional<bool> button_down(
      const ::remoting::protocol::MouseEvent& event) {
    if (event.has_button_down()) {
      DCHECK(event.has_button());
      return event.button_down();
    }
    return std::nullopt;
  }

  static std::optional<float> wheel_delta_x(
      const ::remoting::protocol::MouseEvent& event) {
    if (event.has_wheel_delta_x()) {
      return event.wheel_delta_x();
    }
    return std::nullopt;
  }

  static std::optional<float> wheel_delta_y(
      const ::remoting::protocol::MouseEvent& event) {
    if (event.has_wheel_delta_y()) {
      return event.wheel_delta_y();
    }
    return std::nullopt;
  }

  static std::optional<float> wheel_ticks_x(
      const ::remoting::protocol::MouseEvent& event) {
    if (event.wheel_ticks_x()) {
      return event.wheel_ticks_x();
    }
    return std::nullopt;
  }

  static std::optional<float> wheel_ticks_y(
      const ::remoting::protocol::MouseEvent& event) {
    if (event.wheel_ticks_y()) {
      return event.wheel_ticks_y();
    }
    return std::nullopt;
  }

  static std::optional<int32_t> delta_x(
      const ::remoting::protocol::MouseEvent& event) {
    if (event.has_delta_x()) {
      return event.delta_x();
    }
    return std::nullopt;
  }

  static std::optional<int32_t> delta_y(
      const ::remoting::protocol::MouseEvent& event) {
    if (event.has_delta_y()) {
      return event.delta_y();
    }
    return std::nullopt;
  }

  static mojo::OptionalAsPointer<
      const ::remoting::protocol::FractionalCoordinate>
  fractional_coordinate(const ::remoting::protocol::MouseEvent& event) {
    return event.has_fractional_coordinate()
               ? mojo::OptionalAsPointer(&event.fractional_coordinate())
               : nullptr;
  }

  static bool Read(remoting::mojom::MouseEventDataView data_view,
                   ::remoting::protocol::MouseEvent* out_event);
};

template <>
class StructTraits<remoting::mojom::ScreenResolutionDataView,
                   ::remoting::ScreenResolution> {
 public:
  static const ::webrtc::DesktopSize& dimensions(
      const ::remoting::ScreenResolution& resolution) {
    return resolution.dimensions();
  }

  static const ::webrtc::DesktopVector& dpi(
      const ::remoting::ScreenResolution& resolution) {
    return resolution.dpi();
  }

  static bool Read(remoting::mojom::ScreenResolutionDataView data_view,
                   ::remoting::ScreenResolution* out_resolution);
};

template <>
class StructTraits<remoting::mojom::TextEventDataView,
                   ::remoting::protocol::TextEvent> {
 public:
  static const std::string& text(const ::remoting::protocol::TextEvent& event) {
    return event.text();
  }

  static bool Read(remoting::mojom::TextEventDataView data_view,
                   ::remoting::protocol::TextEvent* out_event);
};

template <>
class StructTraits<remoting::mojom::TouchEventPointDataView,
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
      ::remoting::protocol::TouchEvent::TouchEventType input);
  static ::remoting::protocol::TouchEvent::TouchEventType FromMojom(
      remoting::mojom::TouchEventType input);
};

template <>
class StructTraits<remoting::mojom::TouchEventDataView,
                   ::remoting::protocol::TouchEvent> {
 public:
  static ::remoting::protocol::TouchEvent::TouchEventType event_type(
      const ::remoting::protocol::TouchEvent& event) {
    return event.event_type();
  }

  static const ::google::protobuf::RepeatedPtrField<
      ::remoting::protocol::TouchEventPoint>&
  touch_points(const ::remoting::protocol::TouchEvent& event) {
    return event.touch_points();
  }

  static bool Read(remoting::mojom::TouchEventDataView data_view,
                   ::remoting::protocol::TouchEvent* out_event);
};

template <>
struct EnumTraits<remoting::mojom::TransportRouteType,
                  ::remoting::protocol::TransportRoute::RouteType> {
  static remoting::mojom::TransportRouteType ToMojom(
      ::remoting::protocol::TransportRoute::RouteType input);
  static std::optional<::remoting::protocol::TransportRoute::RouteType>
  FromMojom(remoting::mojom::TransportRouteType input);
};

template <>
class StructTraits<remoting::mojom::TransportRouteDataView,
                   ::remoting::protocol::TransportRoute> {
 public:
  static ::remoting::protocol::TransportRoute::RouteType type(
      const ::remoting::protocol::TransportRoute& transport_route) {
    return transport_route.type;
  }

  static const ::net::IPEndPoint& remote_address(
      const ::remoting::protocol::TransportRoute& transport_route) {
    return transport_route.remote_address;
  }

  static const ::net::IPEndPoint& local_address(
      const ::remoting::protocol::TransportRoute& transport_route) {
    return transport_route.local_address;
  }

  static bool Read(remoting::mojom::TransportRouteDataView data_view,
                   ::remoting::protocol::TransportRoute* out_transport_route);
};

#endif  // BUILDFLAG(REMOTING_MULTI_PROCESS)

template <>
struct EnumTraits<remoting::mojom::ProtocolErrorCode, ::remoting::ErrorCode> {
  static remoting::mojom::ProtocolErrorCode ToMojom(
      ::remoting::ErrorCode input);
  static ::remoting::ErrorCode FromMojom(
      remoting::mojom::ProtocolErrorCode input);
};

#if BUILDFLAG(REMOTING_MULTI_PROCESS)

template <>
struct EnumTraits<remoting::mojom::VideoLayout_PixelType,
                  ::remoting::protocol::VideoLayout::PixelType> {
  static remoting::mojom::VideoLayout_PixelType ToMojom(
      ::remoting::protocol::VideoLayout::PixelType input);
  static ::remoting::protocol::VideoLayout::PixelType FromMojom(
      remoting::mojom::VideoLayout_PixelType input);
};

template <>
class StructTraits<remoting::mojom::VideoLayoutDataView,
                   ::remoting::protocol::VideoLayout> {
 public:
  static const ::google::protobuf::RepeatedPtrField<
      ::remoting::protocol::VideoTrackLayout>&
  tracks(const ::remoting::protocol::VideoLayout& layout) {
    return layout.video_track();
  }

  static bool supports_full_desktop_capture(
      const ::remoting::protocol::VideoLayout& layout) {
    return layout.supports_full_desktop_capture();
  }

  static int64_t primary_screen_id(
      const ::remoting::protocol::VideoLayout& layout) {
    return layout.primary_screen_id();
  }

  static std::optional<::remoting::protocol::VideoLayout::PixelType> pixel_type(
      const ::remoting::protocol::VideoLayout& layout) {
    if (layout.has_pixel_type()) {
      return layout.pixel_type();
    }
    return std::nullopt;
  }

  static bool Read(remoting::mojom::VideoLayoutDataView data_view,
                   ::remoting::protocol::VideoLayout* out_layout);
};

template <>
class StructTraits<remoting::mojom::VideoTrackLayoutDataView,
                   ::remoting::protocol::VideoTrackLayout> {
 public:
  static std::optional<int64_t> screen_id(
      const ::remoting::protocol::VideoTrackLayout& track) {
    if (track.has_screen_id()) {
      return track.screen_id();
    }
    return std::nullopt;
  }

  static const std::string& media_stream_id(
      const ::remoting::protocol::VideoTrackLayout& track) {
    return track.media_stream_id();
  }

  static gfx::Point position(
      const ::remoting::protocol::VideoTrackLayout& track) {
    return {track.position_x(), track.position_y()};
  }

  static webrtc::DesktopSize size(
      const ::remoting::protocol::VideoTrackLayout& track) {
    return {track.width(), track.height()};
  }

  static webrtc::DesktopVector dpi(
      const ::remoting::protocol::VideoTrackLayout& track) {
    return {track.x_dpi(), track.y_dpi()};
  }

  static const std::string& display_name(
      const ::remoting::protocol::VideoTrackLayout& track) {
    return track.display_name();
  }

  static bool Read(remoting::mojom::VideoTrackLayoutDataView data_view,
                   ::remoting::protocol::VideoTrackLayout* out_track);
};

#endif  // BUILDFLAG(REMOTING_MULTI_PROCESS)

template <>
class StructTraits<remoting::mojom::SourceLocationDataView,
                   ::remoting::SourceLocation> {
 public:
  static std::optional<std::string_view> function_name(
      const ::remoting::SourceLocation& source_info) {
    return source_info.function_name()
               ? std::optional<std::string_view>(source_info.function_name())
               : std::nullopt;
  }

  static std::optional<std::string_view> file_name(
      const ::remoting::SourceLocation& source_info) {
    return source_info.file_name()
               ? std::optional<std::string_view>(source_info.file_name())
               : std::nullopt;
  }

  static int32_t line_number(const ::remoting::SourceLocation& source_info) {
    return source_info.line_number();
  }

  static bool Read(remoting::mojom::SourceLocationDataView data_view,
                   ::remoting::SourceLocation* out_source_info);
};

template <>
class StructTraits<remoting::mojom::PortRangeDataView, ::remoting::PortRange> {
 public:
  static uint16_t min_port(const ::remoting::PortRange& range) {
    return range.min_port();
  }
  static uint16_t max_port(const ::remoting::PortRange& range) {
    return range.max_port();
  }
  static bool Read(remoting::mojom::PortRangeDataView data_view,
                   ::remoting::PortRange* out_range);
};

template <>
struct EnumTraits<remoting::mojom::SessionPolicies_AudioMode,
                  ::remoting::SessionPolicies::AudioMode> {
  static remoting::mojom::SessionPolicies_AudioMode ToMojom(
      ::remoting::SessionPolicies::AudioMode input);
  static ::remoting::SessionPolicies::AudioMode FromMojom(
      remoting::mojom::SessionPolicies_AudioMode input);
};

template <>
class StructTraits<remoting::mojom::SessionPoliciesDataView,
                   ::remoting::SessionPolicies> {
 public:
  static std::optional<uint64_t> clipboard_size_bytes(
      const ::remoting::SessionPolicies& policies) {
    return policies.clipboard_size_bytes;
  }

  static std::optional<bool> allow_stun_connections(
      const ::remoting::SessionPolicies& policies) {
    return policies.allow_stun_connections;
  }

  static std::optional<bool> allow_relayed_connections(
      const ::remoting::SessionPolicies& policies) {
    return policies.allow_relayed_connections;
  }

  static const ::remoting::PortRange& host_udp_port_range(
      const ::remoting::SessionPolicies& policies) {
    return policies.host_udp_port_range;
  }

  static std::optional<bool> allow_file_transfer(
      const ::remoting::SessionPolicies& policies) {
    return policies.allow_file_transfer;
  }

  static std::optional<bool> allow_uri_forwarding(
      const ::remoting::SessionPolicies& policies) {
    return policies.allow_uri_forwarding;
  }

  static std::optional<base::TimeDelta> maximum_session_duration(
      const ::remoting::SessionPolicies& policies) {
    return policies.maximum_session_duration;
  }

  static std::optional<bool> curtain_required(
      const ::remoting::SessionPolicies& policies) {
    return policies.curtain_required;
  }

  static std::optional<bool> host_username_match_required(
      const ::remoting::SessionPolicies& policies) {
    return policies.host_username_match_required;
  }

  static std::optional<bool> allow_remote_input(
      const ::remoting::SessionPolicies& policies) {
    return policies.allow_remote_input;
  }

  static std::optional<bool> allow_webauthn_forwarding(
      const ::remoting::SessionPolicies& policies) {
    return policies.allow_webauthn_forwarding;
  }

  static std::optional<bool> allow_security_key_forwarding(
      const ::remoting::SessionPolicies& policies) {
    return policies.allow_security_key_forwarding;
  }

  static std::optional<bool> allow_terminal_mode(
      const ::remoting::SessionPolicies& policies) {
    return policies.allow_terminal_mode;
  }

  static std::optional<::remoting::SessionPolicies::AudioMode> audio_mode(
      const ::remoting::SessionPolicies& policies) {
    return policies.audio_mode;
  }

  static bool Read(remoting::mojom::SessionPoliciesDataView data_view,
                   ::remoting::SessionPolicies* out_policies);
};

template <>
class StructTraits<remoting::mojom::SessionOptionsDataView,
                   ::remoting::SessionOptions> {
 public:
  static std::optional<bool> detect_updated_region(
      const ::remoting::SessionOptions& options) {
    return options.detect_updated_region;
  }

  static std::optional<bool> capture_video_on_dedicated_thread(
      const ::remoting::SessionOptions& options) {
    return options.capture_video_on_dedicated_thread;
  }

#if BUILDFLAG(IS_MAC)
  static std::optional<bool> enable_sck_capturer(
      const ::remoting::SessionOptions& options) {
    return options.enable_sck_capturer;
  }
#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_WIN)
  static std::optional<bool> allow_dxgi_capturer(
      const ::remoting::SessionOptions& options) {
    return options.allow_dxgi_capturer;
  }
#endif  // BUILDFLAG(IS_WIN)

  static std::optional<bool> disable_udp(
      const ::remoting::SessionOptions& options) {
    return options.disable_udp;
  }

  static std::optional<int32_t> vp9_encoder_speed(
      const ::remoting::SessionOptions& options) {
    return options.vp9_encoder_speed;
  }

  static std::optional<bool> av1_active_map(
      const ::remoting::SessionOptions& options) {
    return options.av1_active_map;
  }

  static std::optional<int32_t> av1_encoder_speed(
      const ::remoting::SessionOptions& options) {
    return options.av1_encoder_speed;
  }

  static std::optional<bool> enable_privacy_overlay(
      const ::remoting::SessionOptions& options) {
    return options.enable_privacy_overlay;
  }

  static bool Read(remoting::mojom::SessionOptionsDataView data_view,
                   ::remoting::SessionOptions* out_options);
};

template <>
struct EnumTraits<remoting::mojom::AgentState,
                  ::remoting::protocol::TerminalControl_AgentInfo_AgentState> {
  static remoting::mojom::AgentState ToMojom(
      ::remoting::protocol::TerminalControl_AgentInfo_AgentState input);
  static std::optional<
      ::remoting::protocol::TerminalControl_AgentInfo_AgentState>
  FromMojom(remoting::mojom::AgentState input);
};

template <>
class StructTraits<remoting::mojom::AgentStatusDataView,
                   ::remoting::protocol::TerminalControl_AgentInfo> {
 public:
  static std::optional<
      ::remoting::protocol::TerminalControl_AgentInfo_AgentState>
  agent_state(const ::remoting::protocol::TerminalControl_AgentInfo& status) {
    return status.has_agent_state() &&
                   status.agent_state() !=
                       ::remoting::protocol::TerminalControl_AgentInfo::
                           UNSPECIFIED
               ? std::optional<::remoting::protocol::
                                   TerminalControl_AgentInfo_AgentState>(
                     status.agent_state())
               : std::nullopt;
  }

  static std::optional<std::string_view> conversation_title(
      const ::remoting::protocol::TerminalControl_AgentInfo& status) {
    return status.has_conversation_title()
               ? std::optional<std::string_view>(status.conversation_title())
               : std::nullopt;
  }

  static bool Read(remoting::mojom::AgentStatusDataView data_view,
                   ::remoting::protocol::TerminalControl_AgentInfo* out_status);
};

#if BUILDFLAG(REMOTING_MULTI_PROCESS)

template <>
class StructTraits<remoting::mojom::FractionalCoordinateDataView,
                   ::remoting::protocol::FractionalCoordinate> {
 public:
  static int64_t screen_id(
      const ::remoting::protocol::FractionalCoordinate& coordinate) {
    return coordinate.screen_id();
  }

  static float x(const ::remoting::protocol::FractionalCoordinate& coordinate) {
    return coordinate.x();
  }

  static float y(const ::remoting::protocol::FractionalCoordinate& coordinate) {
    return coordinate.y();
  }

  static bool Read(remoting::mojom::FractionalCoordinateDataView data_view,
                   ::remoting::protocol::FractionalCoordinate* out_coordinate);
};

template <>
class StructTraits<remoting::mojom::MicrophoneControlDataView,
                   ::remoting::protocol::MicrophoneControl> {
 public:
  static bool enable(const ::remoting::protocol::MicrophoneControl& control) {
    return control.enable();
  }

  static bool Read(remoting::mojom::MicrophoneControlDataView data_view,
                   ::remoting::protocol::MicrophoneControl* out_control);
};

template <>
class StructTraits<remoting::mojom::IpcFifoBufferReaderDataView,
                   std::unique_ptr<remoting::IpcFifoBufferReader>> {
 public:
  static bool IsNull(
      const std::unique_ptr<remoting::IpcFifoBufferReader>& reader) {
    return !reader;
  }

  static void SetToNull(std::unique_ptr<remoting::IpcFifoBufferReader>* out) {
    out->reset();
  }

  static mojo::ScopedDataPipeConsumerHandle consumer_handle(
      std::unique_ptr<remoting::IpcFifoBufferReader>& reader) {
    return reader->TakeConsumerHandle();
  }

  static bool Read(remoting::mojom::IpcFifoBufferReaderDataView data_view,
                   std::unique_ptr<remoting::IpcFifoBufferReader>* out_reader);
};

template <>
class StructTraits<remoting::mojom::AudioSampleInfoDataView,
                   ::remoting::protocol::AudioSampleInfo> {
 public:
  static uint32_t sampling_rate(
      const ::remoting::protocol::AudioSampleInfo& info) {
    return info.sampling_rate;
  }

  static uint8_t channels(const ::remoting::protocol::AudioSampleInfo& info) {
    return info.channels;
  }

  static bool Read(remoting::mojom::AudioSampleInfoDataView data_view,
                   ::remoting::protocol::AudioSampleInfo* out_info);
};

template <>
class StructTraits<remoting::mojom::WebrtcSocketAddressDataView,
                   ::webrtc::SocketAddress> {
 public:
  static const std::string& hostname(const ::webrtc::SocketAddress& address) {
    return address.hostname();
  }

  static uint16_t port(const ::webrtc::SocketAddress& address) {
    return address.port();
  }

  static bool Read(remoting::mojom::WebrtcSocketAddressDataView data_view,
                   ::webrtc::SocketAddress* out_address);
};

template <>
struct EnumTraits<remoting::mojom::WebrtcIceCandidateType,
                  ::webrtc::IceCandidateType> {
  static remoting::mojom::WebrtcIceCandidateType ToMojom(
      ::webrtc::IceCandidateType type);
  static std::optional<::webrtc::IceCandidateType> FromMojom(
      remoting::mojom::WebrtcIceCandidateType input);
};

template <>
class StructTraits<remoting::mojom::WebrtcCandidateDataView,
                   ::webrtc::Candidate> {
 public:
  static const std::string& foundation(const ::webrtc::Candidate& candidate) {
    return candidate.foundation();
  }

  static int32_t component(const ::webrtc::Candidate& candidate) {
    return candidate.component();
  }

  static const std::string& protocol(const ::webrtc::Candidate& candidate) {
    return candidate.protocol();
  }

  static uint32_t priority(const ::webrtc::Candidate& candidate) {
    return candidate.priority();
  }

  static const ::webrtc::SocketAddress& address(
      const ::webrtc::Candidate& candidate) {
    return candidate.address();
  }

  static ::webrtc::IceCandidateType type(const ::webrtc::Candidate& candidate) {
    return candidate.type();
  }

  static const std::string& tcptype(const ::webrtc::Candidate& candidate) {
    return candidate.tcptype();
  }

  static const ::webrtc::SocketAddress& related_address(
      const ::webrtc::Candidate& candidate) {
    return candidate.related_address();
  }

  static const std::string& username(const ::webrtc::Candidate& candidate) {
    return candidate.username();
  }

  static const std::string& password(const ::webrtc::Candidate& candidate) {
    return candidate.password();
  }

  static uint32_t generation(const ::webrtc::Candidate& candidate) {
    return candidate.generation();
  }

  static uint16_t network_id(const ::webrtc::Candidate& candidate) {
    return candidate.network_id();
  }

  static uint16_t network_cost(const ::webrtc::Candidate& candidate) {
    return candidate.network_cost();
  }

  static bool Read(remoting::mojom::WebrtcCandidateDataView data_view,
                   ::webrtc::Candidate* out_candidate);
};

template <>
struct EnumTraits<remoting::mojom::SdpType,
                  ::remoting::SessionDescription::Type> {
  static remoting::mojom::SdpType ToMojom(
      ::remoting::SessionDescription::Type type);
  static std::optional<::remoting::SessionDescription::Type> FromMojom(
      remoting::mojom::SdpType input);
};

template <>
class StructTraits<remoting::mojom::SessionDescriptionDataView,
                   ::remoting::SessionDescription> {
 public:
  static ::remoting::SessionDescription::Type type(
      const ::remoting::SessionDescription& description) {
    return description.type;
  }

  static const std::string& sdp(
      const ::remoting::SessionDescription& description) {
    return description.sdp;
  }

  static const std::vector<uint8_t>& signature(
      const ::remoting::SessionDescription& description) {
    return description.signature;
  }

  static bool Read(remoting::mojom::SessionDescriptionDataView data_view,
                   ::remoting::SessionDescription* out_description);
};

template <>
class StructTraits<remoting::mojom::NamedCandidateDataView,
                   ::remoting::IceTransportInfo::NamedCandidate> {
 public:
  static const std::string& sdp_mid(
      const ::remoting::IceTransportInfo::NamedCandidate& candidate) {
    return candidate.name;
  }

  static std::optional<int32_t> sdp_m_line_index(
      const ::remoting::IceTransportInfo::NamedCandidate& candidate) {
    return candidate.sdp_m_line_index;
  }

  static const ::webrtc::Candidate& candidate(
      const ::remoting::IceTransportInfo::NamedCandidate& candidate) {
    return candidate.candidate;
  }

  static bool Read(remoting::mojom::NamedCandidateDataView data_view,
                   ::remoting::IceTransportInfo::NamedCandidate* out_candidate);
};

template <>
class StructTraits<remoting::mojom::JingleTransportInfoDataView,
                   ::remoting::JingleTransportInfo> {
 public:
  static const std::vector<::remoting::IceTransportInfo::NamedCandidate>&
  candidates(const ::remoting::JingleTransportInfo& transport_info) {
    return transport_info.candidates;
  }

  static const std::optional<::remoting::SessionDescription>&
  session_description(const ::remoting::JingleTransportInfo& transport_info) {
    return transport_info.session_description;
  }

  static bool Read(remoting::mojom::JingleTransportInfoDataView data_view,
                   ::remoting::JingleTransportInfo* out_transport_info);
};

template <>
struct EnumTraits<remoting::mojom::WebrtcProtocolType, ::webrtc::ProtocolType> {
  static remoting::mojom::WebrtcProtocolType ToMojom(
      ::webrtc::ProtocolType protocol);
  static std::optional<::webrtc::ProtocolType> FromMojom(
      remoting::mojom::WebrtcProtocolType input);
};

template <>
class StructTraits<remoting::mojom::WebrtcProtocolAddressDataView,
                   ::webrtc::ProtocolAddress> {
 public:
  static const ::webrtc::SocketAddress& address(
      const ::webrtc::ProtocolAddress& proto_addr) {
    return proto_addr.address;
  }

  static ::webrtc::ProtocolType protocol(
      const ::webrtc::ProtocolAddress& proto_addr) {
    return proto_addr.proto;
  }
};

template <>
class StructTraits<remoting::mojom::WebrtcRelayServerConfigDataView,
                   ::webrtc::RelayServerConfig> {
 public:
  static const std::string& username(
      const ::webrtc::RelayServerConfig& config) {
    return config.credentials.username;
  }

  static const std::string& password(
      const ::webrtc::RelayServerConfig& config) {
    return config.credentials.password;
  }

  static const std::vector<::webrtc::ProtocolAddress>& ports(
      const ::webrtc::RelayServerConfig& config) {
    return config.ports;
  }

  static bool Read(remoting::mojom::WebrtcRelayServerConfigDataView data_view,
                   ::webrtc::RelayServerConfig* out);
};

template <>
class StructTraits<remoting::mojom::IceConfigDataView,
                   ::remoting::protocol::IceConfig> {
 public:
  static base::Time expiration_time(
      const ::remoting::protocol::IceConfig& config) {
    return config.expiration_time;
  }

  static const std::vector<::webrtc::SocketAddress>& stun_servers(
      const ::remoting::protocol::IceConfig& config) {
    return config.stun_servers;
  }

  static const std::vector<::webrtc::RelayServerConfig>& turn_servers(
      const ::remoting::protocol::IceConfig& config) {
    return config.turn_servers;
  }

  static int32_t max_bitrate_kbps(
      const ::remoting::protocol::IceConfig& config) {
    return config.max_bitrate_kbps;
  }

  static bool Read(remoting::mojom::IceConfigDataView data_view,
                   ::remoting::protocol::IceConfig* out);
};

template <>
class StructTraits<remoting::mojom::PairingResponseDataView,
                   ::remoting::protocol::PairingResponse> {
 public:
  static const std::string& client_id(
      const ::remoting::protocol::PairingResponse& response) {
    return response.client_id();
  }

  static const std::string& shared_secret(
      const ::remoting::protocol::PairingResponse& response) {
    return response.shared_secret();
  }

  static bool Read(remoting::mojom::PairingResponseDataView data_view,
                   ::remoting::protocol::PairingResponse* out);
};

#endif  // BUILDFLAG(REMOTING_MULTI_PROCESS)

}  // namespace mojo

#endif  // REMOTING_HOST_MOJOM_REMOTING_MOJOM_TRAITS_H_
