// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_MOJOM_VIDEO_CAPTURE_TYPES_MOJOM_TRAITS_H_
#define MEDIA_CAPTURE_MOJOM_VIDEO_CAPTURE_TYPES_MOJOM_TRAITS_H_

#include "media/base/video_facing.h"
#include "media/capture/mojom/video_capture_types.mojom.h"
#include "media/capture/video/video_capture_device_descriptor.h"
#include "media/capture/video/video_capture_device_info.h"
#include "media/capture/video_capture_types.h"

namespace mojo {

template <>
struct EnumTraits<media::mojom::ResolutionChangePolicy,
                  media::ResolutionChangePolicy> {
  static media::mojom::ResolutionChangePolicy ToMojom(
      media::ResolutionChangePolicy policy);

  static bool FromMojom(media::mojom::ResolutionChangePolicy input,
                        media::ResolutionChangePolicy* out);
};

template <>
struct EnumTraits<media::mojom::PowerLineFrequency, media::PowerLineFrequency> {
  static media::mojom::PowerLineFrequency ToMojom(
      media::PowerLineFrequency frequency);

  static bool FromMojom(media::mojom::PowerLineFrequency input,
                        media::PowerLineFrequency* out);
};

template <>
struct EnumTraits<media::mojom::VideoCapturePixelFormat,
                  media::VideoPixelFormat> {
  static media::mojom::VideoCapturePixelFormat ToMojom(
      media::VideoPixelFormat input);
  static bool FromMojom(media::mojom::VideoCapturePixelFormat input,
                        media::VideoPixelFormat* output);
};

template <>
struct EnumTraits<media::mojom::VideoCaptureBufferType,
                  media::VideoCaptureBufferType> {
  static media::mojom::VideoCaptureBufferType ToMojom(
      media::VideoCaptureBufferType buffer_type);

  static bool FromMojom(media::mojom::VideoCaptureBufferType input,
                        media::VideoCaptureBufferType* out);
};

template <>
struct EnumTraits<media::mojom::VideoCaptureError, media::VideoCaptureError> {
  static media::mojom::VideoCaptureError ToMojom(
      media::VideoCaptureError buffer_type);

  static bool FromMojom(media::mojom::VideoCaptureError input,
                        media::VideoCaptureError* out);
};

template <>
struct EnumTraits<media::mojom::VideoCaptureFrameDropReason,
                  media::VideoCaptureFrameDropReason> {
  static media::mojom::VideoCaptureFrameDropReason ToMojom(
      media::VideoCaptureFrameDropReason buffer_type);

  static bool FromMojom(media::mojom::VideoCaptureFrameDropReason input,
                        media::VideoCaptureFrameDropReason* out);
};

template <>
struct EnumTraits<media::mojom::VideoFacingMode, media::VideoFacingMode> {
  static media::mojom::VideoFacingMode ToMojom(media::VideoFacingMode input);
  static bool FromMojom(media::mojom::VideoFacingMode input,
                        media::VideoFacingMode* output);
};

template <>
struct EnumTraits<media::mojom::VideoCaptureApi, media::VideoCaptureApi> {
  static media::mojom::VideoCaptureApi ToMojom(media::VideoCaptureApi input);
  static bool FromMojom(media::mojom::VideoCaptureApi input,
                        media::VideoCaptureApi* output);
};

template <>
struct EnumTraits<media::mojom::VideoCaptureTransportType,
                  media::VideoCaptureTransportType> {
  static media::mojom::VideoCaptureTransportType ToMojom(
      media::VideoCaptureTransportType input);
  static bool FromMojom(media::mojom::VideoCaptureTransportType input,
                        media::VideoCaptureTransportType* output);
};

template <>
struct StructTraits<media::mojom::VideoCaptureFormatDataView,
                    media::VideoCaptureFormat> {
  static const gfx::Size& frame_size(const media::VideoCaptureFormat& format) {
    return format.frame_size;
  }

  static float frame_rate(const media::VideoCaptureFormat& format) {
    return format.frame_rate;
  }

  static media::VideoPixelFormat pixel_format(
      const media::VideoCaptureFormat& format) {
    return format.pixel_format;
  }

  static bool Read(media::mojom::VideoCaptureFormatDataView data,
                   media::VideoCaptureFormat* out);
};

template <>
struct StructTraits<media::mojom::VideoCaptureParamsDataView,
                    media::VideoCaptureParams> {
  static media::VideoCaptureFormat requested_format(
      const media::VideoCaptureParams& params) {
    return params.requested_format;
  }

  static media::VideoCaptureBufferType buffer_type(
      const media::VideoCaptureParams& params) {
    return params.buffer_type;
  }

  static media::ResolutionChangePolicy resolution_change_policy(
      const media::VideoCaptureParams& params) {
    return params.resolution_change_policy;
  }

  static media::PowerLineFrequency power_line_frequency(
      const media::VideoCaptureParams& params) {
    return params.power_line_frequency;
  }

  static bool enable_face_detection(
      const media::VideoCaptureParams& params) {
    return params.enable_face_detection;
  }

  static bool Read(media::mojom::VideoCaptureParamsDataView data,
                   media::VideoCaptureParams* out);
};

template <>
struct StructTraits<media::mojom::VideoCaptureDeviceDescriptorDataView,
                    media::VideoCaptureDeviceDescriptor> {
  static const std::string& display_name(
      const media::VideoCaptureDeviceDescriptor& input) {
    return input.display_name();
  }

  static const std::string& device_id(
      const media::VideoCaptureDeviceDescriptor& input) {
    return input.device_id;
  }

  static const std::string& model_id(
      const media::VideoCaptureDeviceDescriptor& input) {
    return input.model_id;
  }

  static media::VideoFacingMode facing_mode(
      const media::VideoCaptureDeviceDescriptor& input) {
    return input.facing;
  }

  static media::VideoCaptureApi capture_api(
      const media::VideoCaptureDeviceDescriptor& input) {
    return input.capture_api;
  }

  static media::VideoCaptureTransportType transport_type(
      const media::VideoCaptureDeviceDescriptor& input) {
    return input.transport_type;
  }

  static bool Read(media::mojom::VideoCaptureDeviceDescriptorDataView data,
                   media::VideoCaptureDeviceDescriptor* output);
};

template <>
struct StructTraits<media::mojom::VideoCaptureDeviceInfoDataView,
                    media::VideoCaptureDeviceInfo> {
  static const media::VideoCaptureDeviceDescriptor& descriptor(
      const media::VideoCaptureDeviceInfo& input) {
    return input.descriptor;
  }

  static const std::vector<media::VideoCaptureFormat>& supported_formats(
      const media::VideoCaptureDeviceInfo& input) {
    return input.supported_formats;
  }

  static bool Read(media::mojom::VideoCaptureDeviceInfoDataView data,
                   media::VideoCaptureDeviceInfo* output);
};
}  // namespace mojo

#endif  // MEDIA_CAPTURE_MOJOM_VIDEO_CAPTURE_TYPES_MOJOM_TRAITS_H_
