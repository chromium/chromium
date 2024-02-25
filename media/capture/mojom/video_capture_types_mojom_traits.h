// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_MOJOM_VIDEO_CAPTURE_TYPES_MOJOM_TRAITS_H_
#define MEDIA_CAPTURE_MOJOM_VIDEO_CAPTURE_TYPES_MOJOM_TRAITS_H_

#include "media/base/video_facing.h"
#include "media/capture/mojom/video_capture_types.mojom-shared.h"
#include "media/capture/video/video_capture_device_descriptor.h"
#include "media/capture/video/video_capture_device_info.h"
#include "media/capture/video/video_capture_feedback.h"
#include "media/capture/video_capture_types.h"

namespace mojo {

template <>
struct COMPONENT_EXPORT(MEDIA_CAPTURE_MOJOM_TRAITS)
    EnumTraits<media::mojom::ResolutionChangePolicy,
               media::ResolutionChangePolicy> {
  static media::mojom::ResolutionChangePolicy ToMojom(
      media::ResolutionChangePolicy policy);

  static bool FromMojom(media::mojom::ResolutionChangePolicy input,
                        media::ResolutionChangePolicy* out);
};

template <>
struct COMPONENT_EXPORT(MEDIA_CAPTURE_MOJOM_TRAITS)
    EnumTraits<media::mojom::PowerLineFrequency, media::PowerLineFrequency> {
  static media::mojom::PowerLineFrequency ToMojom(
      media::PowerLineFrequency frequency);

  static bool FromMojom(media::mojom::PowerLineFrequency input,
                        media::PowerLineFrequency* out);
};

template <>
struct COMPONENT_EXPORT(MEDIA_CAPTURE_MOJOM_TRAITS)
    EnumTraits<media::mojom::VideoCapturePixelFormat, media::VideoPixelFormat> {
  static media::mojom::VideoCapturePixelFormat ToMojom(
      media::VideoPixelFormat input);
  static bool FromMojom(media::mojom::VideoCapturePixelFormat input,
                        media::VideoPixelFormat* output);
};

template <>
struct COMPONENT_EXPORT(MEDIA_CAPTURE_MOJOM_TRAITS)
    EnumTraits<media::mojom::VideoCaptureBufferType,
               media::VideoCaptureBufferType> {
  static media::mojom::VideoCaptureBufferType ToMojom(
      media::VideoCaptureBufferType buffer_type);

  static bool FromMojom(media::mojom::VideoCaptureBufferType input,
                        media::VideoCaptureBufferType* out);
};

template <>
struct COMPONENT_EXPORT(MEDIA_CAPTURE_MOJOM_TRAITS)
    EnumTraits<media::mojom::VideoCaptureError, media::VideoCaptureError> {
  static media::mojom::VideoCaptureError ToMojom(
      media::VideoCaptureError buffer_type);

  static bool FromMojom(media::mojom::VideoCaptureError input,
                        media::VideoCaptureError* out);
};

template <>
struct COMPONENT_EXPORT(MEDIA_CAPTURE_MOJOM_TRAITS)
    EnumTraits<media::mojom::VideoCaptureFrameDropReason,
               media::VideoCaptureFrameDropReason> {
  static media::mojom::VideoCaptureFrameDropReason ToMojom(
      media::VideoCaptureFrameDropReason buffer_type);

  static bool FromMojom(media::mojom::VideoCaptureFrameDropReason input,
                        media::VideoCaptureFrameDropReason* out);
};

template <>
struct COMPONENT_EXPORT(MEDIA_CAPTURE_MOJOM_TRAITS)
    EnumTraits<media::mojom::VideoFacingMode, media::VideoFacingMode> {
  static media::mojom::VideoFacingMode ToMojom(media::VideoFacingMode input);
  static bool FromMojom(media::mojom::VideoFacingMode input,
                        media::VideoFacingMode* output);
};

template <>
struct COMPONENT_EXPORT(MEDIA_CAPTURE_MOJOM_TRAITS)
    EnumTraits<media::mojom::VideoCaptureApi, media::VideoCaptureApi> {
  static media::mojom::VideoCaptureApi ToMojom(media::VideoCaptureApi input);
  static bool FromMojom(media::mojom::VideoCaptureApi input,
                        media::VideoCaptureApi* output);
};

template <>
struct COMPONENT_EXPORT(MEDIA_CAPTURE_MOJOM_TRAITS)
    EnumTraits<media::mojom::CameraAvailability, media::CameraAvailability> {
  static media::mojom::CameraAvailability ToMojom(
      media::CameraAvailability input);
  static bool FromMojom(media::mojom::CameraAvailability input,
                        media::CameraAvailability* output);
};

template <>
struct COMPONENT_EXPORT(MEDIA_CAPTURE_MOJOM_TRAITS)
    EnumTraits<media::mojom::VideoCaptureTransportType,
               media::VideoCaptureTransportType> {
  static media::mojom::VideoCaptureTransportType ToMojom(
      media::VideoCaptureTransportType input);
  static bool FromMojom(media::mojom::VideoCaptureTransportType input,
                        media::VideoCaptureTransportType* output);
};

template <>
struct COMPONENT_EXPORT(MEDIA_CAPTURE_MOJOM_TRAITS)
    StructTraits<media::mojom::VideoCaptureControlSupportDataView,
                 media::VideoCaptureControlSupport> {
  static bool pan(const media::VideoCaptureControlSupport& input) {
    return input.pan;
  }

  static bool tilt(const media::VideoCaptureControlSupport& input) {
    return input.tilt;
  }

  static bool zoom(const media::VideoCaptureControlSupport& input) {
    return input.zoom;
  }

  static bool Read(media::mojom::VideoCaptureControlSupportDataView data,
                   media::VideoCaptureControlSupport* out);
};

template <>
struct COMPONENT_EXPORT(MEDIA_CAPTURE_MOJOM_TRAITS)
    StructTraits<media::mojom::VideoCaptureFormatDataView,
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
struct COMPONENT_EXPORT(MEDIA_CAPTURE_MOJOM_TRAITS)
    StructTraits<media::mojom::VideoCaptureParamsDataView,
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

  static bool is_high_dpi_enabled(const media::VideoCaptureParams& params) {
    return params.is_high_dpi_enabled;
  }

  static bool Read(media::mojom::VideoCaptureParamsDataView data,
                   media::VideoCaptureParams* out);
};

template <>
struct COMPONENT_EXPORT(MEDIA_CAPTURE_MOJOM_TRAITS)
    StructTraits<media::mojom::VideoCaptureDeviceDescriptorDataView,
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

  static std::optional<media::CameraAvailability> availability(
      const media::VideoCaptureDeviceDescriptor& input) {
    return input.availability;
  }

  static media::VideoCaptureApi capture_api(
      const media::VideoCaptureDeviceDescriptor& input) {
    return input.capture_api;
  }

  static media::VideoCaptureControlSupport control_support(
      const media::VideoCaptureDeviceDescriptor& input) {
    return input.control_support();
  }

  static media::VideoCaptureTransportType transport_type(
      const media::VideoCaptureDeviceDescriptor& input) {
    return input.transport_type;
  }

  static bool Read(media::mojom::VideoCaptureDeviceDescriptorDataView data,
                   media::VideoCaptureDeviceDescriptor* output);
};

template <>
struct COMPONENT_EXPORT(MEDIA_CAPTURE_MOJOM_TRAITS)
    StructTraits<media::mojom::VideoCaptureDeviceInfoDataView,
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

template <>
struct COMPONENT_EXPORT(MEDIA_CAPTURE_MOJOM_TRAITS)
    StructTraits<media::mojom::VideoCaptureFeedbackDataView,
                 media::VideoCaptureFeedback> {
  static double resource_utilization(
      const media::VideoCaptureFeedback& feedback) {
    return feedback.resource_utilization;
  }

  static float max_framerate_fps(const media::VideoCaptureFeedback& feedback) {
    return feedback.max_framerate_fps;
  }

  static int max_pixels(const media::VideoCaptureFeedback& feedback) {
    return feedback.max_pixels;
  }

  static bool require_mapped_frame(
      const media::VideoCaptureFeedback& feedback) {
    return feedback.require_mapped_frame;
  }

  // Deprecated.
  static std::vector<gfx::Size> DEPRECATED_mapped_sizes(
      const media::VideoCaptureFeedback& feedback) {
    return std::vector<gfx::Size>();
  }

  static bool has_frame_id(const media::VideoCaptureFeedback& feedback) {
    return feedback.frame_id.has_value();
  }

  static int frame_id(const media::VideoCaptureFeedback& feedback) {
    return feedback.frame_id.value_or(0);
  }

  static bool Read(media::mojom::VideoCaptureFeedbackDataView data,
                   media::VideoCaptureFeedback* output);
};
}  // namespace mojo

#endif  // MEDIA_CAPTURE_MOJOM_VIDEO_CAPTURE_TYPES_MOJOM_TRAITS_H_
