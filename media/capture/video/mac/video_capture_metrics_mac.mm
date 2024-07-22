// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "media/capture/video/mac/video_capture_metrics_mac.h"

#include "base/metrics/histogram_functions.h"
#import "media/capture/video/apple/video_capture_device_avfoundation.h"
#include "media/capture/video/video_capture_device_info.h"

namespace media {

namespace {

enum class ResolutionComparison {
  kWidthGtHeightEq = 0,
  kWidthLtHeightEq = 1,
  kWidthEqHeightGt = 2,
  kWidthEqHeightLt = 3,
  kEq = 4,
  kWidthGtHeightGt = 5,
  kWidthLtHeightGt = 6,
  kWidthGtHeightLt = 7,
  kWidthLtHeightLt = 8,
  kMaxValue = kWidthLtHeightLt,
};

ResolutionComparison CompareDimensions(const CMVideoDimensions& requested,
                                       const CMVideoDimensions& captured) {
  if (requested.width > captured.width) {
    if (requested.height > captured.height)
      return ResolutionComparison::kWidthGtHeightGt;
    if (requested.height < captured.height)
      return ResolutionComparison::kWidthGtHeightLt;
    return ResolutionComparison::kWidthGtHeightEq;
  } else if (requested.width < captured.width) {
    if (requested.height > captured.height)
      return ResolutionComparison::kWidthLtHeightGt;
    if (requested.height < captured.height)
      return ResolutionComparison::kWidthLtHeightLt;
    return ResolutionComparison::kWidthLtHeightEq;
  } else {
    if (requested.height > captured.height)
      return ResolutionComparison::kWidthEqHeightGt;
    if (requested.height < captured.height)
      return ResolutionComparison::kWidthEqHeightLt;
    return ResolutionComparison::kEq;
  }
}

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class ReactionEffectsGesturesState {
  kNotSupported = 0,      // Reaction effects not supported
  kDisabled = 1,          // Reaction effects supported, but disabled
  kGesturesDisabled = 2,  // Reaction effects enabled, not triggered by gestures
  kGesturesEnabled = 3,   // Reaction effects enabled and triggered by gestures
  kMaxValue = kGesturesEnabled,
};

}  // namespace

void LogFirstCapturedVideoFrame(const AVCaptureDeviceFormat* bestCaptureFormat,
                                const CMSampleBufferRef buffer) {
  if (bestCaptureFormat) {
    const CMFormatDescriptionRef requestedFormat =
        bestCaptureFormat.formatDescription;
    base::UmaHistogramEnumeration(
        "Media.VideoCapture.Mac.Device.RequestedPixelFormat",
        [VideoCaptureDeviceAVFoundation
            FourCCToChromiumPixelFormat:CMFormatDescriptionGetMediaSubType(
                                            requestedFormat)],
        media::VideoPixelFormat::PIXEL_FORMAT_MAX);

    if (buffer) {
      const CMFormatDescriptionRef capturedFormat =
          CMSampleBufferGetFormatDescription(buffer);
      base::UmaHistogramBoolean(
          "Media.VideoCapture.Mac.Device.CapturedWithRequestedPixelFormat",
          CMFormatDescriptionGetMediaSubType(capturedFormat) ==
              CMFormatDescriptionGetMediaSubType(requestedFormat));
      base::UmaHistogramEnumeration(
          "Media.VideoCapture.Mac.Device.CapturedWithRequestedResolution",
          CompareDimensions(
              CMVideoFormatDescriptionGetDimensions(requestedFormat),
              CMVideoFormatDescriptionGetDimensions(capturedFormat)));

      const CVPixelBufferRef pixelBufferRef =
          CMSampleBufferGetImageBuffer(buffer);
      bool is_io_surface =
          pixelBufferRef && CVPixelBufferGetIOSurface(pixelBufferRef);
      base::UmaHistogramBoolean(
          "Media.VideoCapture.Mac.Device.CapturedIOSurface", is_io_surface);
    }
  }
}

void LogReactionEffectsGesturesState() {
  ReactionEffectsGesturesState state =
      ReactionEffectsGesturesState::kNotSupported;
  if (@available(macOS 14.0, *)) {
    state = ReactionEffectsGesturesState::kDisabled;
    if (AVCaptureDevice.reactionEffectsEnabled) {
      state = AVCaptureDevice.reactionEffectGesturesEnabled
                  ? ReactionEffectsGesturesState::kGesturesEnabled
                  : ReactionEffectsGesturesState::kGesturesDisabled;
    }
  }
  base::UmaHistogramEnumeration(
      "Media.VideoCapture.Mac.Device.ReactionEffectsGesturesState", state);
}

}  // namespace media
