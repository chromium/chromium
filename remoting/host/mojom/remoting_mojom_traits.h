// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_MOJOM_REMOTING_MOJOM_TRAITS_H_
#define REMOTING_HOST_MOJOM_REMOTING_MOJOM_TRAITS_H_

#include <stddef.h>
#include <string>

#include "base/numerics/safe_conversions.h"
#include "build/build_config.h"
#include "mojo/public/cpp/base/byte_string_mojom_traits.h"
#include "mojo/public/cpp/bindings/array_traits.h"
#include "mojo/public/cpp/bindings/array_traits_protobuf.h"
#include "mojo/public/cpp/bindings/enum_traits.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "remoting/host/base/desktop_environment_options.h"
#include "remoting/host/base/screen_resolution.h"
#include "remoting/host/mojom/desktop_session.mojom-shared.h"
#include "remoting/host/mojom/remoting_host.mojom-shared.h"
#include "remoting/host/mojom/webrtc_types.mojom-shared.h"
#include "remoting/host/mojom/wrapped_primitives.mojom-shared.h"
#include "remoting/proto/audio.pb.h"
#include "remoting/proto/event.pb.h"
#include "remoting/protocol/transport.h"
#include "services/network/public/cpp/ip_endpoint_mojom_traits.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"
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
class mojo::StructTraits<remoting::mojom::UInt32DataView, uint32_t> {
 public:
  static uint32_t value(uint32_t value) { return value; }

  static bool Read(remoting::mojom::UInt32DataView data_view,
                   uint32_t* out_value) {
    *out_value = data_view.value();
    return true;
  }
};

template <>
class mojo::StructTraits<remoting::mojom::DesktopCaptureOptionsDataView,
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
class mojo::StructTraits<remoting::mojom::DesktopEnvironmentOptionsDataView,
                         ::remoting::DesktopEnvironmentOptions> {
 public:
  static bool enable_curtaining(
      const ::remoting::DesktopEnvironmentOptions& options) {
    return options.enable_curtaining();
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

  static bool enable_file_transfer(
      const ::remoting::DesktopEnvironmentOptions& options) {
    return options.enable_file_transfer();
  }

  static bool enable_remote_open_url(
      const ::remoting::DesktopEnvironmentOptions& options) {
    return options.enable_remote_open_url();
  }

  static bool enable_remote_webauthn(
      const ::remoting::DesktopEnvironmentOptions& options) {
    return options.enable_remote_webauthn();
  }

  static absl::optional<uint32_t> clipboard_size(
      const ::remoting::DesktopEnvironmentOptions& options) {
    if (!options.clipboard_size().has_value()) {
      return absl::nullopt;
    }

    size_t clipboard_size = options.clipboard_size().value();
    return base::IsValueInRangeForNumericType<int>(clipboard_size)
               ? clipboard_size
               : INT_MAX;
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
      ::webrtc::DesktopCapturer::Result input) {
    switch (input) {
      case ::webrtc::DesktopCapturer::Result::SUCCESS:
        return remoting::mojom::DesktopCaptureResult::kSuccess;
      case ::webrtc::DesktopCapturer::Result::ERROR_TEMPORARY:
        return remoting::mojom::DesktopCaptureResult::kErrorTemporary;
      case ::webrtc::DesktopCapturer::Result::ERROR_PERMANENT:
        return remoting::mojom::DesktopCaptureResult::kErrorPermanent;
    }

    NOTREACHED();
    return remoting::mojom::DesktopCaptureResult::kSuccess;
  }

  static bool FromMojom(remoting::mojom::DesktopCaptureResult input,
                        ::webrtc::DesktopCapturer::Result* out) {
    switch (input) {
      case remoting::mojom::DesktopCaptureResult::kSuccess:
        *out = ::webrtc::DesktopCapturer::Result::SUCCESS;
        return true;
      case remoting::mojom::DesktopCaptureResult::kErrorTemporary:
        *out = ::webrtc::DesktopCapturer::Result::ERROR_TEMPORARY;
        return true;
      case remoting::mojom::DesktopCaptureResult::kErrorPermanent:
        *out = ::webrtc::DesktopCapturer::Result::ERROR_PERMANENT;
        return true;
    }

    NOTREACHED();
    return false;
  }
};

template <>
class mojo::StructTraits<remoting::mojom::DesktopRectDataView,
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
class mojo::StructTraits<remoting::mojom::DesktopSizeDataView,
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
class mojo::StructTraits<remoting::mojom::DesktopVectorDataView,
                         ::webrtc::DesktopVector> {
 public:
  static int32_t x(const ::webrtc::DesktopVector& vector) { return vector.x(); }

  static int32_t y(const ::webrtc::DesktopVector& vector) { return vector.y(); }

  static bool Read(remoting::mojom::DesktopVectorDataView data_view,
                   ::webrtc::DesktopVector* out_vector);
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
struct EnumTraits<remoting::mojom::AudioPacket_BytesPerSample,
                  ::remoting::AudioPacket::BytesPerSample> {
  static remoting::mojom::AudioPacket_BytesPerSample ToMojom(
      ::remoting::AudioPacket::BytesPerSample input) {
    switch (input) {
      case ::remoting::AudioPacket::BYTES_PER_SAMPLE_INVALID:
        return remoting::mojom::AudioPacket_BytesPerSample::kInvalid;
      case ::remoting::AudioPacket::BYTES_PER_SAMPLE_2:
        return remoting::mojom::AudioPacket_BytesPerSample::kBytesPerSample_2;
    }

    NOTREACHED();
    return remoting::mojom::AudioPacket_BytesPerSample::kInvalid;
  }

  static bool FromMojom(remoting::mojom::AudioPacket_BytesPerSample input,
                        ::remoting::AudioPacket::BytesPerSample* out) {
    switch (input) {
      case remoting::mojom::AudioPacket_BytesPerSample::kInvalid:
        *out = ::remoting::AudioPacket::BYTES_PER_SAMPLE_INVALID;
        return true;
      case remoting::mojom::AudioPacket_BytesPerSample::kBytesPerSample_2:
        *out = ::remoting::AudioPacket::BYTES_PER_SAMPLE_2;
        return true;
    }

    NOTREACHED();
    return false;
  }
};

template <>
struct EnumTraits<remoting::mojom::AudioPacket_Channels,
                  ::remoting::AudioPacket::Channels> {
  static remoting::mojom::AudioPacket_Channels ToMojom(
      ::remoting::AudioPacket::Channels input) {
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
    return remoting::mojom::AudioPacket_Channels::kInvalid;
  }

  static bool FromMojom(remoting::mojom::AudioPacket_Channels input,
                        ::remoting::AudioPacket::Channels* out) {
    switch (input) {
      case remoting::mojom::AudioPacket_Channels::kInvalid:
        *out = ::remoting::AudioPacket::CHANNELS_INVALID;
        return true;
      case remoting::mojom::AudioPacket_Channels::kMono:
        *out = ::remoting::AudioPacket::CHANNELS_MONO;
        return true;
      case remoting::mojom::AudioPacket_Channels::kStereo:
        *out = ::remoting::AudioPacket::CHANNELS_STEREO;
        return true;
      case remoting::mojom::AudioPacket_Channels::kSurround:
        *out = ::remoting::AudioPacket::CHANNELS_SURROUND;
        return true;
      case remoting::mojom::AudioPacket_Channels::kChannel_4_0:
        *out = ::remoting::AudioPacket::CHANNELS_4_0;
        return true;
      case remoting::mojom::AudioPacket_Channels::kChannel_4_1:
        *out = ::remoting::AudioPacket::CHANNELS_4_1;
        return true;
      case remoting::mojom::AudioPacket_Channels::kChannel_5_1:
        *out = ::remoting::AudioPacket::CHANNELS_5_1;
        return true;
      case remoting::mojom::AudioPacket_Channels::kChannel_6_1:
        *out = ::remoting::AudioPacket::CHANNELS_6_1;
        return true;
      case remoting::mojom::AudioPacket_Channels::kChannel_7_1:
        *out = ::remoting::AudioPacket::CHANNELS_7_1;
        return true;
    }

    NOTREACHED();
    return false;
  }
};

template <>
struct EnumTraits<remoting::mojom::AudioPacket_Encoding,
                  ::remoting::AudioPacket::Encoding> {
  static remoting::mojom::AudioPacket_Encoding ToMojom(
      ::remoting::AudioPacket::Encoding input) {
    switch (input) {
      case ::remoting::AudioPacket::ENCODING_INVALID:
        return remoting::mojom::AudioPacket_Encoding::kInvalid;
      case ::remoting::AudioPacket::ENCODING_RAW:
        return remoting::mojom::AudioPacket_Encoding::kRaw;
      case ::remoting::AudioPacket::ENCODING_OPUS:
        return remoting::mojom::AudioPacket_Encoding::kOpus;
    }

    NOTREACHED();
    return remoting::mojom::AudioPacket_Encoding::kInvalid;
  }

  static bool FromMojom(remoting::mojom::AudioPacket_Encoding input,
                        ::remoting::AudioPacket::Encoding* out) {
    switch (input) {
      case remoting::mojom::AudioPacket_Encoding::kInvalid:
        *out = ::remoting::AudioPacket::ENCODING_INVALID;
        return true;
      case remoting::mojom::AudioPacket_Encoding::kRaw:
        *out = ::remoting::AudioPacket::ENCODING_RAW;
        return true;
      case remoting::mojom::AudioPacket_Encoding::kOpus:
        *out = ::remoting::AudioPacket::ENCODING_OPUS;
        return true;
    }

    NOTREACHED();
    return false;
  }
};

template <>
struct EnumTraits<remoting::mojom::AudioPacket_SamplingRate,
                  ::remoting::AudioPacket::SamplingRate> {
  static remoting::mojom::AudioPacket_SamplingRate ToMojom(
      ::remoting::AudioPacket::SamplingRate input) {
    switch (input) {
      case ::remoting::AudioPacket::SAMPLING_RATE_INVALID:
        return remoting::mojom::AudioPacket_SamplingRate::kInvalid;
      case ::remoting::AudioPacket::SAMPLING_RATE_44100:
        return remoting::mojom::AudioPacket_SamplingRate::kRate_44100;
      case ::remoting::AudioPacket::SAMPLING_RATE_48000:
        return remoting::mojom::AudioPacket_SamplingRate::kRate_48000;
    }

    NOTREACHED();
    return remoting::mojom::AudioPacket_SamplingRate::kInvalid;
  }

  static bool FromMojom(remoting::mojom::AudioPacket_SamplingRate input,
                        ::remoting::AudioPacket::SamplingRate* out) {
    switch (input) {
      case remoting::mojom::AudioPacket_SamplingRate::kInvalid:
        *out = ::remoting::AudioPacket::SAMPLING_RATE_INVALID;
        return true;
      case remoting::mojom::AudioPacket_SamplingRate::kRate_44100:
        *out = ::remoting::AudioPacket::SAMPLING_RATE_44100;
        return true;
      case remoting::mojom::AudioPacket_SamplingRate::kRate_48000:
        *out = ::remoting::AudioPacket::SAMPLING_RATE_48000;
        return true;
    }

    NOTREACHED();
    return false;
  }
};

template <>
class mojo::StructTraits<remoting::mojom::AudioPacketDataView,
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
class mojo::StructTraits<remoting::mojom::ScreenResolutionDataView,
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
      ::remoting::protocol::TransportRoute::RouteType input) {
    switch (input) {
      case ::remoting::protocol::TransportRoute::RouteType::DIRECT:
        return remoting::mojom::TransportRouteType::kDirect;
      case ::remoting::protocol::TransportRoute::RouteType::STUN:
        return remoting::mojom::TransportRouteType::kStun;
      case ::remoting::protocol::TransportRoute::RouteType::RELAY:
        return remoting::mojom::TransportRouteType::kRelay;
    }

    NOTREACHED();
    return remoting::mojom::TransportRouteType::kUndefined;
  }

  static bool FromMojom(remoting::mojom::TransportRouteType input,
                        ::remoting::protocol::TransportRoute::RouteType* out) {
    switch (input) {
      case remoting::mojom::TransportRouteType::kUndefined:
        // kUndefined does not map to a value in TransportRoute::RouteType so
        // it should be treated the same as any other unknown value.
        break;
      case remoting::mojom::TransportRouteType::kDirect:
        *out = ::remoting::protocol::TransportRoute::RouteType::DIRECT;
        return true;
      case remoting::mojom::TransportRouteType::kStun:
        *out = ::remoting::protocol::TransportRoute::RouteType::STUN;
        return true;
      case remoting::mojom::TransportRouteType::kRelay:
        *out = ::remoting::protocol::TransportRoute::RouteType::RELAY;
        return true;
    }

    NOTREACHED();
    return false;
  }
};

template <>
class mojo::StructTraits<remoting::mojom::TransportRouteDataView,
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

template <>
struct EnumTraits<remoting::mojom::ProtocolErrorCode,
                  ::remoting::protocol::ErrorCode> {
  static remoting::mojom::ProtocolErrorCode ToMojom(
      ::remoting::protocol::ErrorCode input) {
    switch (input) {
      case ::remoting::protocol::ErrorCode::OK:
        return remoting::mojom::ProtocolErrorCode::kOk;
      case ::remoting::protocol::ErrorCode::PEER_IS_OFFLINE:
        return remoting::mojom::ProtocolErrorCode::kPeerIsOffline;
      case ::remoting::protocol::ErrorCode::SESSION_REJECTED:
        return remoting::mojom::ProtocolErrorCode::kSessionRejected;
      case ::remoting::protocol::ErrorCode::INCOMPATIBLE_PROTOCOL:
        return remoting::mojom::ProtocolErrorCode::kIncompatibleProtocol;
      case ::remoting::protocol::ErrorCode::AUTHENTICATION_FAILED:
        return remoting::mojom::ProtocolErrorCode::kAuthenticationFailed;
      case ::remoting::protocol::ErrorCode::INVALID_ACCOUNT:
        return remoting::mojom::ProtocolErrorCode::kInvalidAccount;
      case ::remoting::protocol::ErrorCode::CHANNEL_CONNECTION_ERROR:
        return remoting::mojom::ProtocolErrorCode::kChannelConnectionError;
      case ::remoting::protocol::ErrorCode::SIGNALING_ERROR:
        return remoting::mojom::ProtocolErrorCode::kSignalingError;
      case ::remoting::protocol::ErrorCode::SIGNALING_TIMEOUT:
        return remoting::mojom::ProtocolErrorCode::kSignalingTimeout;
      case ::remoting::protocol::ErrorCode::HOST_OVERLOAD:
        return remoting::mojom::ProtocolErrorCode::kHostOverload;
      case ::remoting::protocol::ErrorCode::MAX_SESSION_LENGTH:
        return remoting::mojom::ProtocolErrorCode::kMaxSessionLength;
      case ::remoting::protocol::ErrorCode::HOST_CONFIGURATION_ERROR:
        return remoting::mojom::ProtocolErrorCode::kHostConfigurationError;
      case ::remoting::protocol::ErrorCode::UNKNOWN_ERROR:
        return remoting::mojom::ProtocolErrorCode::kUnknownError;
      case ::remoting::protocol::ErrorCode::ELEVATION_ERROR:
        return remoting::mojom::ProtocolErrorCode::kElevationError;
      case ::remoting::protocol::ErrorCode::HOST_CERTIFICATE_ERROR:
        return remoting::mojom::ProtocolErrorCode::kHostCertificateError;
      case ::remoting::protocol::ErrorCode::HOST_REGISTRATION_ERROR:
        return remoting::mojom::ProtocolErrorCode::kHostRegistrationError;
      case ::remoting::protocol::ErrorCode::EXISTING_ADMIN_SESSION:
        return remoting::mojom::ProtocolErrorCode::kExistingAdminSession;
      case ::remoting::protocol::ErrorCode::AUTHZ_POLICY_CHECK_FAILED:
        return remoting::mojom::ProtocolErrorCode::kAuthzPolicyCheckFailed;
      case ::remoting::protocol::ErrorCode::DISALLOWED_BY_POLICY:
        return remoting::mojom::ProtocolErrorCode::kDisallowedByPolicy;
    }

    NOTREACHED();
    return remoting::mojom::ProtocolErrorCode::kUnknownError;
  }

  static bool FromMojom(remoting::mojom::ProtocolErrorCode input,
                        ::remoting::protocol::ErrorCode* out) {
    switch (input) {
      case remoting::mojom::ProtocolErrorCode::kOk:
        *out = ::remoting::protocol::ErrorCode::OK;
        return true;
      case remoting::mojom::ProtocolErrorCode::kPeerIsOffline:
        *out = ::remoting::protocol::ErrorCode::PEER_IS_OFFLINE;
        return true;
      case remoting::mojom::ProtocolErrorCode::kSessionRejected:
        *out = ::remoting::protocol::ErrorCode::SESSION_REJECTED;
        return true;
      case remoting::mojom::ProtocolErrorCode::kIncompatibleProtocol:
        *out = ::remoting::protocol::ErrorCode::INCOMPATIBLE_PROTOCOL;
        return true;
      case remoting::mojom::ProtocolErrorCode::kAuthenticationFailed:
        *out = ::remoting::protocol::ErrorCode::AUTHENTICATION_FAILED;
        return true;
      case remoting::mojom::ProtocolErrorCode::kInvalidAccount:
        *out = ::remoting::protocol::ErrorCode::INVALID_ACCOUNT;
        return true;
      case remoting::mojom::ProtocolErrorCode::kChannelConnectionError:
        *out = ::remoting::protocol::ErrorCode::CHANNEL_CONNECTION_ERROR;
        return true;
      case remoting::mojom::ProtocolErrorCode::kSignalingError:
        *out = ::remoting::protocol::ErrorCode::SIGNALING_ERROR;
        return true;
      case remoting::mojom::ProtocolErrorCode::kSignalingTimeout:
        *out = ::remoting::protocol::ErrorCode::SIGNALING_TIMEOUT;
        return true;
      case remoting::mojom::ProtocolErrorCode::kHostOverload:
        *out = ::remoting::protocol::ErrorCode::HOST_OVERLOAD;
        return true;
      case remoting::mojom::ProtocolErrorCode::kMaxSessionLength:
        *out = ::remoting::protocol::ErrorCode::MAX_SESSION_LENGTH;
        return true;
      case remoting::mojom::ProtocolErrorCode::kHostConfigurationError:
        *out = ::remoting::protocol::ErrorCode::HOST_CONFIGURATION_ERROR;
        return true;
      case remoting::mojom::ProtocolErrorCode::kUnknownError:
        *out = ::remoting::protocol::ErrorCode::UNKNOWN_ERROR;
        return true;
      case remoting::mojom::ProtocolErrorCode::kElevationError:
        *out = ::remoting::protocol::ErrorCode::ELEVATION_ERROR;
        return true;
      case remoting::mojom::ProtocolErrorCode::kHostCertificateError:
        *out = ::remoting::protocol::ErrorCode::HOST_CERTIFICATE_ERROR;
        return true;
      case remoting::mojom::ProtocolErrorCode::kHostRegistrationError:
        *out = ::remoting::protocol::ErrorCode::HOST_REGISTRATION_ERROR;
        return true;
      case remoting::mojom::ProtocolErrorCode::kExistingAdminSession:
        *out = ::remoting::protocol::ErrorCode::EXISTING_ADMIN_SESSION;
        return true;
      case remoting::mojom::ProtocolErrorCode::kAuthzPolicyCheckFailed:
        *out = ::remoting::protocol::ErrorCode::AUTHZ_POLICY_CHECK_FAILED;
        return true;
      case remoting::mojom::ProtocolErrorCode::kDisallowedByPolicy:
        *out = ::remoting::protocol::ErrorCode::DISALLOWED_BY_POLICY;
        return true;
    }

    NOTREACHED();
    return false;
  }
};

}  // namespace mojo

#endif  // REMOTING_HOST_MOJOM_REMOTING_MOJOM_TRAITS_H_
