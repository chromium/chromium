// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#ifndef REMOTING_HOST_MOJOM_REMOTING_MOJOM_TRAITS_H_
#define REMOTING_HOST_MOJOM_REMOTING_MOJOM_TRAITS_H_

#include <stddef.h>

#include <memory>
#include <optional>
#include <string>

#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/numerics/safe_conversions.h"
#include "base/numerics/safe_math.h"
#include "build/build_config.h"
#include "mojo/public/cpp/base/byte_string_mojom_traits.h"
#include "mojo/public/cpp/base/file_path_mojom_traits.h"
#include "mojo/public/cpp/bindings/array_traits.h"
#include "mojo/public/cpp/bindings/array_traits_protobuf.h"
#include "mojo/public/cpp/bindings/enum_traits.h"
#include "mojo/public/cpp/bindings/map_traits_protobuf.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "remoting/base/result.h"
#include "remoting/host/base/desktop_environment_options.h"
#include "remoting/host/base/screen_resolution.h"
#include "remoting/host/mojom/desktop_session.mojom-shared.h"
#include "remoting/host/mojom/remoting_host.mojom-shared.h"
#include "remoting/host/mojom/webrtc_types.mojom-shared.h"
#include "remoting/host/mojom/wrapped_primitives.mojom-shared.h"
#include "remoting/proto/audio.pb.h"
#include "remoting/proto/control.pb.h"
#include "remoting/proto/event.pb.h"
#include "remoting/proto/file_transfer.pb.h"
#include "remoting/protocol/file_transfer_helpers.h"
#include "remoting/protocol/transport.h"
#include "services/network/public/cpp/ip_endpoint_mojom_traits.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"
#include "third_party/webrtc/modules/desktop_capture/mouse_cursor.h"
#include "ui/gfx/geometry/mojom/geometry_mojom_traits.h"

namespace mojo {

template <>
class StructTraits<remoting::mojom::BoolDataView, bool> {
 public:
  static bool value(bool value) { return value; }

  static bool Read(remoting::mojom::BoolDataView data_view, bool* out_value) {
    *out_value = data_view.value();
    return true;
  }
};

template <>
class StructTraits<remoting::mojom::FloatDataView, float> {
 public:
  static float value(float value) { return value; }

  static bool Read(remoting::mojom::FloatDataView data_view, float* out_value) {
    *out_value = data_view.value();
    return true;
  }
};

template <>
class StructTraits<remoting::mojom::Int32DataView, int32_t> {
 public:
  static int32_t value(int32_t value) { return value; }

  static bool Read(remoting::mojom::Int32DataView data_view,
                   int32_t* out_value) {
    *out_value = data_view.value();
    return true;
  }
};

template <>
class StructTraits<remoting::mojom::UInt32DataView, uint32_t> {
 public:
  static uint32_t value(uint32_t value) { return value; }

  static bool Read(remoting::mojom::UInt32DataView data_view,
                   uint32_t* out_value) {
    *out_value = data_view.value();
    return true;
  }
};

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
  }
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
    return base::span<const uint8_t>(cursor.image()->data(),
                                     buffer_size.ValueOrDie());
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
  }
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
    if (result.is_success())
      return remoting::mojom::ReadChunkResultDataView::Tag::kData;
    else if (result.is_error())
      return remoting::mojom::ReadChunkResultDataView::Tag::kError;

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
      ::remoting::protocol::FileTransfer_Error_Type input) {
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

  static bool FromMojom(remoting::mojom::FileTransferError_Type input,
                        ::remoting::protocol::FileTransfer_Error_Type* out) {
    switch (input) {
      case remoting::mojom::FileTransferError_Type::kUnknown:
        *out = ::remoting::protocol::FileTransfer_Error::UNSPECIFIED;
        return true;
      case remoting::mojom::FileTransferError_Type::kCanceled:
        *out = ::remoting::protocol::FileTransfer_Error::CANCELED;
        return true;
      case remoting::mojom::FileTransferError_Type::kUnexpectedError:
        *out = ::remoting::protocol::FileTransfer_Error::UNEXPECTED_ERROR;
        return true;
      case remoting::mojom::FileTransferError_Type::kProtocolError:
        *out = ::remoting::protocol::FileTransfer_Error::PROTOCOL_ERROR;
        return true;
      case remoting::mojom::FileTransferError_Type::kPermissionDenied:
        *out = ::remoting::protocol::FileTransfer_Error::PERMISSION_DENIED;
        return true;
      case remoting::mojom::FileTransferError_Type::kOutOfDiskSpace:
        *out = ::remoting::protocol::FileTransfer_Error::OUT_OF_DISK_SPACE;
        return true;
      case remoting::mojom::FileTransferError_Type::kIoError:
        *out = ::remoting::protocol::FileTransfer_Error::IO_ERROR;
        return true;
      case remoting::mojom::FileTransferError_Type::kNotLoggedIn:
        *out = ::remoting::protocol::FileTransfer_Error::NOT_LOGGED_IN;
        return true;
    }

    NOTREACHED();
  }
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
    if (result.is_success())
      return remoting::mojom::FileChooserResultDataView::Tag::kFilepath;
    else if (result.is_error())
      return remoting::mojom::FileChooserResultDataView::Tag::kError;

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
      ::remoting::protocol::LayoutKeyFunction input) {
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

  static bool FromMojom(remoting::mojom::LayoutKeyFunction input,
                        ::remoting::protocol::LayoutKeyFunction* out) {
    switch (input) {
      case remoting::mojom::LayoutKeyFunction::kUnknown:
        *out = ::remoting::protocol::LayoutKeyFunction::UNKNOWN;
        return true;
      case remoting::mojom::LayoutKeyFunction::kControl:
        *out = ::remoting::protocol::LayoutKeyFunction::CONTROL;
        return true;
      case remoting::mojom::LayoutKeyFunction::kAlt:
        *out = ::remoting::protocol::LayoutKeyFunction::ALT;
        return true;
      case remoting::mojom::LayoutKeyFunction::kShift:
        *out = ::remoting::protocol::LayoutKeyFunction::SHIFT;
        return true;
      case remoting::mojom::LayoutKeyFunction::kMeta:
        *out = ::remoting::protocol::LayoutKeyFunction::META;
        return true;
      case remoting::mojom::LayoutKeyFunction::kAltGr:
        *out = ::remoting::protocol::LayoutKeyFunction::ALT_GR;
        return true;
      case remoting::mojom::LayoutKeyFunction::kMod5:
        *out = ::remoting::protocol::LayoutKeyFunction::MOD5;
        return true;
      case remoting::mojom::LayoutKeyFunction::kCompose:
        *out = ::remoting::protocol::LayoutKeyFunction::COMPOSE;
        return true;
      case remoting::mojom::LayoutKeyFunction::kOption:
        *out = ::remoting::protocol::LayoutKeyFunction::OPTION;
        return true;
      case remoting::mojom::LayoutKeyFunction::kCommand:
        *out = ::remoting::protocol::LayoutKeyFunction::COMMAND;
        return true;
      case remoting::mojom::LayoutKeyFunction::kSearch:
        *out = ::remoting::protocol::LayoutKeyFunction::SEARCH;
        return true;
      case remoting::mojom::LayoutKeyFunction::kNumLock:
        *out = ::remoting::protocol::LayoutKeyFunction::NUM_LOCK;
        return true;
      case remoting::mojom::LayoutKeyFunction::kCapsLock:
        *out = ::remoting::protocol::LayoutKeyFunction::CAPS_LOCK;
        return true;
      case remoting::mojom::LayoutKeyFunction::kScrollLock:
        *out = ::remoting::protocol::LayoutKeyFunction::SCROLL_LOCK;
        return true;
      case remoting::mojom::LayoutKeyFunction::kBackspace:
        *out = ::remoting::protocol::LayoutKeyFunction::BACKSPACE;
        return true;
      case remoting::mojom::LayoutKeyFunction::kEnter:
        *out = ::remoting::protocol::LayoutKeyFunction::ENTER;
        return true;
      case remoting::mojom::LayoutKeyFunction::kTab:
        *out = ::remoting::protocol::LayoutKeyFunction::TAB;
        return true;
      case remoting::mojom::LayoutKeyFunction::kInsert:
        *out = ::remoting::protocol::LayoutKeyFunction::INSERT;
        return true;
      case remoting::mojom::LayoutKeyFunction::kDelete:
        *out = ::remoting::protocol::LayoutKeyFunction::DELETE_;
        return true;
      case remoting::mojom::LayoutKeyFunction::kHome:
        *out = ::remoting::protocol::LayoutKeyFunction::HOME;
        return true;
      case remoting::mojom::LayoutKeyFunction::kEnd:
        *out = ::remoting::protocol::LayoutKeyFunction::END;
        return true;
      case remoting::mojom::LayoutKeyFunction::kPageUp:
        *out = ::remoting::protocol::LayoutKeyFunction::PAGE_UP;
        return true;
      case remoting::mojom::LayoutKeyFunction::kPageDown:
        *out = ::remoting::protocol::LayoutKeyFunction::PAGE_DOWN;
        return true;
      case remoting::mojom::LayoutKeyFunction::kClear:
        *out = ::remoting::protocol::LayoutKeyFunction::CLEAR;
        return true;
      case remoting::mojom::LayoutKeyFunction::kArrowUp:
        *out = ::remoting::protocol::LayoutKeyFunction::ARROW_UP;
        return true;
      case remoting::mojom::LayoutKeyFunction::kArrowDown:
        *out = ::remoting::protocol::LayoutKeyFunction::ARROW_DOWN;
        return true;
      case remoting::mojom::LayoutKeyFunction::kArrowLeft:
        *out = ::remoting::protocol::LayoutKeyFunction::ARROW_LEFT;
        return true;
      case remoting::mojom::LayoutKeyFunction::kArrowRight:
        *out = ::remoting::protocol::LayoutKeyFunction::ARROW_RIGHT;
        return true;
      case remoting::mojom::LayoutKeyFunction::kF1:
        *out = ::remoting::protocol::LayoutKeyFunction::F1;
        return true;
      case remoting::mojom::LayoutKeyFunction::kF2:
        *out = ::remoting::protocol::LayoutKeyFunction::F2;
        return true;
      case remoting::mojom::LayoutKeyFunction::kF3:
        *out = ::remoting::protocol::LayoutKeyFunction::F3;
        return true;
      case remoting::mojom::LayoutKeyFunction::kF4:
        *out = ::remoting::protocol::LayoutKeyFunction::F4;
        return true;
      case remoting::mojom::LayoutKeyFunction::kF5:
        *out = ::remoting::protocol::LayoutKeyFunction::F5;
        return true;
      case remoting::mojom::LayoutKeyFunction::kF6:
        *out = ::remoting::protocol::LayoutKeyFunction::F6;
        return true;
      case remoting::mojom::LayoutKeyFunction::kF7:
        *out = ::remoting::protocol::LayoutKeyFunction::F7;
        return true;
      case remoting::mojom::LayoutKeyFunction::kF8:
        *out = ::remoting::protocol::LayoutKeyFunction::F8;
        return true;
      case remoting::mojom::LayoutKeyFunction::kF9:
        *out = ::remoting::protocol::LayoutKeyFunction::F9;
        return true;
      case remoting::mojom::LayoutKeyFunction::kF10:
        *out = ::remoting::protocol::LayoutKeyFunction::F10;
        return true;
      case remoting::mojom::LayoutKeyFunction::kF11:
        *out = ::remoting::protocol::LayoutKeyFunction::F11;
        return true;
      case remoting::mojom::LayoutKeyFunction::kF12:
        *out = ::remoting::protocol::LayoutKeyFunction::F12;
        return true;
      case remoting::mojom::LayoutKeyFunction::kF13:
        *out = ::remoting::protocol::LayoutKeyFunction::F13;
        return true;
      case remoting::mojom::LayoutKeyFunction::kF14:
        *out = ::remoting::protocol::LayoutKeyFunction::F14;
        return true;
      case remoting::mojom::LayoutKeyFunction::kF15:
        *out = ::remoting::protocol::LayoutKeyFunction::F15;
        return true;
      case remoting::mojom::LayoutKeyFunction::kF16:
        *out = ::remoting::protocol::LayoutKeyFunction::F16;
        return true;
      case remoting::mojom::LayoutKeyFunction::kF17:
        *out = ::remoting::protocol::LayoutKeyFunction::F17;
        return true;
      case remoting::mojom::LayoutKeyFunction::kF18:
        *out = ::remoting::protocol::LayoutKeyFunction::F18;
        return true;
      case remoting::mojom::LayoutKeyFunction::kF19:
        *out = ::remoting::protocol::LayoutKeyFunction::F19;
        return true;
      case remoting::mojom::LayoutKeyFunction::kF20:
        *out = ::remoting::protocol::LayoutKeyFunction::F20;
        return true;
      case remoting::mojom::LayoutKeyFunction::kF21:
        *out = ::remoting::protocol::LayoutKeyFunction::F21;
        return true;
      case remoting::mojom::LayoutKeyFunction::kF22:
        *out = ::remoting::protocol::LayoutKeyFunction::F22;
        return true;
      case remoting::mojom::LayoutKeyFunction::kF23:
        *out = ::remoting::protocol::LayoutKeyFunction::F23;
        return true;
      case remoting::mojom::LayoutKeyFunction::kF24:
        *out = ::remoting::protocol::LayoutKeyFunction::F24;
        return true;
      case remoting::mojom::LayoutKeyFunction::kEscape:
        *out = ::remoting::protocol::LayoutKeyFunction::ESCAPE;
        return true;
      case remoting::mojom::LayoutKeyFunction::kContextMenu:
        *out = ::remoting::protocol::LayoutKeyFunction::CONTEXT_MENU;
        return true;
      case remoting::mojom::LayoutKeyFunction::kPause:
        *out = ::remoting::protocol::LayoutKeyFunction::PAUSE;
        return true;
      case remoting::mojom::LayoutKeyFunction::kPrintScreen:
        *out = ::remoting::protocol::LayoutKeyFunction::PRINT_SCREEN;
        return true;
      case remoting::mojom::LayoutKeyFunction::kHankakuZenkakuKanji:
        *out = ::remoting::protocol::LayoutKeyFunction::HANKAKU_ZENKAKU_KANJI;
        return true;
      case remoting::mojom::LayoutKeyFunction::kHenkan:
        *out = ::remoting::protocol::LayoutKeyFunction::HENKAN;
        return true;
      case remoting::mojom::LayoutKeyFunction::kMuhenkan:
        *out = ::remoting::protocol::LayoutKeyFunction::MUHENKAN;
        return true;
      case remoting::mojom::LayoutKeyFunction::kKatakanaHiriganaRomaji:
        *out =
            ::remoting::protocol::LayoutKeyFunction::KATAKANA_HIRAGANA_ROMAJI;
        return true;
      case remoting::mojom::LayoutKeyFunction::kKana:
        *out = ::remoting::protocol::LayoutKeyFunction::KANA;
        return true;
      case remoting::mojom::LayoutKeyFunction::kEisu:
        *out = ::remoting::protocol::LayoutKeyFunction::EISU;
        return true;
      case remoting::mojom::LayoutKeyFunction::kHanYeong:
        *out = ::remoting::protocol::LayoutKeyFunction::HAN_YEONG;
        return true;
      case remoting::mojom::LayoutKeyFunction::kHanja:
        *out = ::remoting::protocol::LayoutKeyFunction::HANJA;
        return true;
    }

    NOTREACHED();
  }
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
  }
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
  }
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
      case ::remoting::protocol::ErrorCode::LOCATION_AUTHZ_POLICY_CHECK_FAILED:
        return remoting::mojom::ProtocolErrorCode::
            kLocationAuthzPolicyCheckFailed;
      case ::remoting::protocol::ErrorCode::UNAUTHORIZED_ACCOUNT:
        return remoting::mojom::ProtocolErrorCode::kUnauthorizedAccount;
      case ::remoting::protocol::ErrorCode::REAUTHZ_POLICY_CHECK_FAILED:
        return remoting::mojom::ProtocolErrorCode::kReauthzPolicyCheckFailed;
      case ::remoting::protocol::ErrorCode::NO_COMMON_AUTH_METHOD:
        return remoting::mojom::ProtocolErrorCode::kNoCommonAuthMethod;
      case ::remoting::protocol::ErrorCode::LOGIN_SCREEN_NOT_SUPPORTED:
        return remoting::mojom::ProtocolErrorCode::kLoginScreenNotSupported;
      case ::remoting::protocol::ErrorCode::SESSION_POLICIES_CHANGED:
        return remoting::mojom::ProtocolErrorCode::kSessionPoliciesChanged;
    }

    NOTREACHED();
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
      case remoting::mojom::ProtocolErrorCode::kLocationAuthzPolicyCheckFailed:
        *out =
            ::remoting::protocol::ErrorCode::LOCATION_AUTHZ_POLICY_CHECK_FAILED;
        return true;
      case remoting::mojom::ProtocolErrorCode::kUnauthorizedAccount:
        *out = ::remoting::protocol::ErrorCode::UNAUTHORIZED_ACCOUNT;
        return true;
      case remoting::mojom::ProtocolErrorCode::kReauthzPolicyCheckFailed:
        *out = ::remoting::protocol::ErrorCode::REAUTHZ_POLICY_CHECK_FAILED;
        return true;
      case remoting::mojom::ProtocolErrorCode::kNoCommonAuthMethod:
        *out = ::remoting::protocol::ErrorCode::NO_COMMON_AUTH_METHOD;
        return true;
      case remoting::mojom::ProtocolErrorCode::kLoginScreenNotSupported:
        *out = ::remoting::protocol::ErrorCode::LOGIN_SCREEN_NOT_SUPPORTED;
        return true;
      case remoting::mojom::ProtocolErrorCode::kSessionPoliciesChanged:
        *out = ::remoting::protocol::ErrorCode::SESSION_POLICIES_CHANGED;
        return true;
    }

    NOTREACHED();
  }
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

  static bool Read(remoting::mojom::VideoLayoutDataView data_view,
                   ::remoting::protocol::VideoLayout* out_layout);
};

template <>
class StructTraits<remoting::mojom::VideoTrackLayoutDataView,
                   ::remoting::protocol::VideoTrackLayout> {
 public:
  static int64_t screen_id(
      const ::remoting::protocol::VideoTrackLayout& track) {
    return track.screen_id();
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

  static bool Read(remoting::mojom::VideoTrackLayoutDataView data_view,
                   ::remoting::protocol::VideoTrackLayout* out_track);
};

}  // namespace mojo

#endif  // REMOTING_HOST_MOJOM_REMOTING_MOJOM_TRAITS_H_
