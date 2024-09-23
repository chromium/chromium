// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/video_capture_metrics.h"

#include "base/containers/contains.h"
#include "base/containers/fixed_flat_map.h"
#include "base/containers/flat_set.h"
#include "base/containers/span.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "ui/gfx/geometry/size.h"

namespace {

// These resolutions are ones supported on a test webcam. Names given
// where possible, from https://en.wikipedia.org/wiki/List_of_common_resolutions
enum class VideoResolutionDesignation {
  kUnknown = 0,  // Catch-all for resolutions not understood.
  // Video Graphics Array resolutions
  kQQVGA = 1,  // 160x120
  kHQVGA = 2,  // 240x160
  kQVGA = 3,   // 320x240
  kWQVGA = 4,  // 432x240
  kHVGA = 5,   // 480x320
  kVGA = 6,    // 640x480
  kWVGA = 7,   // 720x480
  kWSVGA = 8,  // 1024x576
  kSVGA = 9,   // 800x600

  // Extended Graphics Array resolutions
  kSXGA_MINUS = 10,  // 1280x960
  kUXGA = 11,        // 1600x1200
  kQXGA = 12,        // 2048x1536

  // Common Intermediate Format resolutions
  kQCIF = 13,  // 176x144
  kCIF = 14,   // 352x288

  // High-definition resolutions.
  kNHD = 15,            // 640x360
  kQHD = 16,            // 960x540
  kHD_FULLSCREEN = 17,  // 960x720
  kHD = 18,             // 1280x720
  kHD_PLUS = 19,        // 1600x900
  kFHD = 20,            // 1920x1080
  kWQHD = 21,           // 2560x1440
  kQHD_PLUS = 22,       // 3200x1800
  k4K_UHD = 23,         // 3840x2160
  kDCI_4K = 24,         // 4096x2160
  k5K = 25,             // 5120x2880
  k8K_UHD = 26,         // 7680x4320

  // Odd resolutions with no name
  k160x90 = 27,
  k320x176 = 28,
  k320x180 = 29,
  k480x270 = 30,
  k544x288 = 31,
  k752x416 = 32,
  k864x480 = 33,
  k800x448 = 34,
  k960x544 = 35,
  k1184x656 = 36,
  k1392x768 = 37,
  k1504x832 = 38,
  k1600x896 = 39,
  k1712x960 = 40,
  k1792x1008 = 41,
  k2592x1944 = 42,

  kMaxValue = k2592x1944,
};

struct FrameSizeCompare {
  // Return true iff lhs < rhs.
  constexpr bool operator()(const gfx::Size& lhs, const gfx::Size& rhs) const {
    return (lhs.height() < rhs.height() ||
            (lhs.height() == rhs.height() && lhs.width() < rhs.width()));
  }
};

constexpr auto kResolutions =
    base::MakeFixedFlatMap<gfx::Size, VideoResolutionDesignation>(
        {
            {{160, 120}, VideoResolutionDesignation::kQQVGA},
            {{240, 160}, VideoResolutionDesignation::kHQVGA},
            {{320, 240}, VideoResolutionDesignation::kQVGA},
            {{432, 240}, VideoResolutionDesignation::kWQVGA},
            {{480, 320}, VideoResolutionDesignation::kHVGA},
            {{640, 480}, VideoResolutionDesignation::kVGA},
            {{720, 480}, VideoResolutionDesignation::kWVGA},
            {{1024, 576}, VideoResolutionDesignation::kWSVGA},
            {{800, 600}, VideoResolutionDesignation::kSVGA},
            {{1280, 960}, VideoResolutionDesignation::kSXGA_MINUS},
            {{1600, 1200}, VideoResolutionDesignation::kUXGA},
            {{2048, 1536}, VideoResolutionDesignation::kQXGA},
            {{176, 144}, VideoResolutionDesignation::kQCIF},
            {{352, 288}, VideoResolutionDesignation::kCIF},
            {{640, 360}, VideoResolutionDesignation::kNHD},
            {{960, 540}, VideoResolutionDesignation::kQHD},
            {{960, 720}, VideoResolutionDesignation::kHD_FULLSCREEN},
            {{1280, 720}, VideoResolutionDesignation::kHD},
            {{1600, 900}, VideoResolutionDesignation::kHD_PLUS},
            {{1920, 1080}, VideoResolutionDesignation::kFHD},
            {{2560, 1440}, VideoResolutionDesignation::kWQHD},
            {{3200, 1800}, VideoResolutionDesignation::kQHD_PLUS},
            {{3840, 2160}, VideoResolutionDesignation::k4K_UHD},
            {{4096, 2160}, VideoResolutionDesignation::kDCI_4K},
            {{5120, 2880}, VideoResolutionDesignation::k5K},
            {{7680, 4320}, VideoResolutionDesignation::k8K_UHD},
            {{160, 90}, VideoResolutionDesignation::k160x90},
            {{320, 176}, VideoResolutionDesignation::k320x176},
            {{320, 180}, VideoResolutionDesignation::k320x180},
            {{480, 270}, VideoResolutionDesignation::k480x270},
            {{544, 288}, VideoResolutionDesignation::k544x288},
            {{752, 416}, VideoResolutionDesignation::k752x416},
            {{864, 480}, VideoResolutionDesignation::k864x480},
            {{800, 448}, VideoResolutionDesignation::k800x448},
            {{960, 544}, VideoResolutionDesignation::k960x544},
            {{1184, 656}, VideoResolutionDesignation::k1184x656},
            {{1392, 768}, VideoResolutionDesignation::k1392x768},
            {{1504, 832}, VideoResolutionDesignation::k1504x832},
            {{1600, 896}, VideoResolutionDesignation::k1600x896},
            {{1712, 960}, VideoResolutionDesignation::k1712x960},
            {{1792, 1008}, VideoResolutionDesignation::k1792x1008},
            {{2592, 1944}, VideoResolutionDesignation::k2592x1944},
        },
        FrameSizeCompare());

static_assert(kResolutions.size() ==
                  static_cast<size_t>(VideoResolutionDesignation::kMaxValue),
              "Each resolution must have one entry in kResolutions.");

VideoResolutionDesignation ResolutionNameFromSize(gfx::Size frame_size) {
  // Rotate such that we are always in landscape.
  if (frame_size.width() < frame_size.height()) {
    int tmp = frame_size.width();
    frame_size.set_width(frame_size.height());
    frame_size.set_width(tmp);
  }
  auto it = kResolutions.find(frame_size);
  return it != kResolutions.end() ? it->second
                                  : VideoResolutionDesignation::kUnknown;
}

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class VideoEffectStatus {
  kUnsupported = 0,
  kSupported = 1,
  kEnabled = 2,
  kMaxValue = kEnabled
};

}  // namespace

namespace media {

void LogCaptureDeviceMetrics(
    base::span<const media::VideoCaptureDeviceInfo> devices_info) {
  for (const auto& device : devices_info) {
    base::flat_set<media::VideoPixelFormat> supported_pixel_formats;
    base::flat_set<gfx::Size, FrameSizeCompare> resolutions;
    for (const auto& format : device.supported_formats) {
      VLOG(2) << "Device supports "
              << media::VideoPixelFormatToString(format.pixel_format) << " at "
              << format.frame_size.ToString() << " ("
              << static_cast<int>(ResolutionNameFromSize(format.frame_size))
              << ")";
      media::VideoPixelFormat pixel_format = format.pixel_format;
      bool inserted = supported_pixel_formats.insert(pixel_format).second;
      if (inserted) {
        base::UmaHistogramEnumeration(
            "Media.VideoCapture.Device.SupportedPixelFormat", pixel_format,
            media::VideoPixelFormat::PIXEL_FORMAT_MAX);
      }
      if (!resolutions.contains(format.frame_size)) {
        resolutions.insert(format.frame_size);
        base::UmaHistogramEnumeration(
            "Media.VideoCapture.Device.SupportedResolution",
            ResolutionNameFromSize(format.frame_size));
      }
    }
  }
  base::UmaHistogramCustomCounts("Media.VideoCapture.Device.TotalAvailable",
                                 devices_info.size(), 0, 5, 5);
}

void LogCaptureDeviceHashedModelId(
    const media::VideoCaptureDeviceDescriptor& descriptor) {
  // descriptor.model_id has the form "XXXX:XXXX" when a USB device is detected,
  // and empty otherwise.
  constexpr int kModelIdStrLength = 9;
  constexpr int kColonPosIndex = 4;
  uint32_t mapping = 0;
  if (descriptor.model_id.length() == kModelIdStrLength) {
    const std::string vid = descriptor.model_id.substr(0, kColonPosIndex);
    const std::string pid =
        descriptor.model_id.substr(kColonPosIndex + 1, kModelIdStrLength);
    const std::string usb_id = vid + pid;
    // Check if resulting usb_id is a valid Hex Number, otherwise reporting 0
    if (std::all_of(usb_id.begin(), usb_id.end(), ::isxdigit)) {
      std::stringstream ss;
      ss << std::hex << usb_id;
      ss >> mapping;
    }
  }
  UMA_HISTOGRAM_SPARSE("Media.VideoCapture.Device.Opened.ByModelId", mapping);
}

VideoEffectStatus GetStatus(bool is_supported, bool is_enabled) {
  if (!is_supported) {
    return VideoEffectStatus::kUnsupported;
  }
  return is_enabled ? VideoEffectStatus::kEnabled
                    : VideoEffectStatus::kSupported;
}

void LogCaptureDeviceEffects(mojom::PhotoStatePtr photo_state) {
  const bool has_background_blur =
      photo_state->supported_background_blur_modes &&
      base::Contains(photo_state->supported_background_blur_modes.value(),
                     mojom::BackgroundBlurMode::BLUR);
  const bool background_blur_enabled =
      photo_state->background_blur_mode != mojom::BackgroundBlurMode::OFF;
  UMA_HISTOGRAM_ENUMERATION(
      "Media.VideoCapture.Device.Effect2.BackgroundBlur",
      GetStatus(has_background_blur, background_blur_enabled));

  const bool has_face_framing =
      photo_state->supported_face_framing_modes &&
      photo_state->supported_face_framing_modes.value().size() > 0;
  const bool face_framing_enabled =
      photo_state->current_face_framing_mode != mojom::MeteringMode::NONE;
  UMA_HISTOGRAM_ENUMERATION("Media.VideoCapture.Device.Effect2.FaceFraming",
                            GetStatus(has_face_framing, face_framing_enabled));

  const bool has_eye_gaze_correction =
      photo_state->supported_eye_gaze_correction_modes &&
      (base::Contains(photo_state->supported_eye_gaze_correction_modes.value(),
                      mojom::EyeGazeCorrectionMode::ON) ||
       base::Contains(photo_state->supported_eye_gaze_correction_modes.value(),
                      mojom::EyeGazeCorrectionMode::STARE));
  const bool eye_gaze_correction_enabled =
      photo_state->current_eye_gaze_correction_mode !=
      mojom::EyeGazeCorrectionMode::OFF;
  UMA_HISTOGRAM_ENUMERATION(
      "Media.VideoCapture.Device.Effect2.EyeGazeCorrection",
      GetStatus(has_eye_gaze_correction, eye_gaze_correction_enabled));
}

void LogCaptureCurrentDeviceResolution(int width, int height) {
  // This method combines width and height into a single uint32_t value.
  constexpr int kMinValue = 0;
  constexpr int kMaxValue = 65535;
  uint32_t result = 0;

  // Check if width and height are valid, otherwise metric will be 0.
  if (width > kMinValue && width <= kMaxValue && height > kMinValue &&
      height <= kMaxValue) {
    // Store the width in the first 16 bits
    result |= static_cast<uint16_t>(width) << 16;
    // Store the height in the last 16 bits
    result |= static_cast<uint16_t>(height);
  }
  base::UmaHistogramSparse("Media.VideoCapture.Device.Opened.Resolution",
                           result);
}

void LogCaptureCurrentDevicePixelFormat(
    const media::VideoPixelFormat pixel_format) {
  base::UmaHistogramEnumeration("Media.VideoCapture.Device.Opened.PixelFormat",
                                pixel_format,
                                media::VideoPixelFormat::PIXEL_FORMAT_MAX);
}

}  // namespace media
