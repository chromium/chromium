// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/mac/video_capture_device_avfoundation_utils_mac.h"

#import <IOKit/audio/IOAudioTypes.h>

#include "base/mac/mac_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "media/base/mac/video_capture_device_avfoundation_helpers.h"
#include "media/base/media_switches.h"
#include "media/capture/video/mac/video_capture_device_avfoundation_mac.h"
#include "media/capture/video/mac/video_capture_device_factory_mac.h"
#include "media/capture/video/mac/video_capture_device_mac.h"
#include "media/capture/video_capture_types.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace media {
namespace {

enum MacBookVersions {
  OTHER = 0,
  MACBOOK_5,  // MacBook5.X
  MACBOOK_6,
  MACBOOK_7,
  MACBOOK_8,
  MACBOOK_PRO_11,  // MacBookPro11.X
  MACBOOK_PRO_12,
  MACBOOK_PRO_13,
  MACBOOK_AIR_5,  // MacBookAir5.X
  MACBOOK_AIR_6,
  MACBOOK_AIR_7,
  MACBOOK_AIR_8,
  MACBOOK_AIR_3,
  MACBOOK_AIR_4,
  MACBOOK_4,
  MACBOOK_9,
  MACBOOK_10,
  MACBOOK_PRO_10,
  MACBOOK_PRO_9,
  MACBOOK_PRO_8,
  MACBOOK_PRO_7,
  MACBOOK_PRO_6,
  MACBOOK_PRO_5,
  MAX_MACBOOK_VERSION = MACBOOK_PRO_5
};

MacBookVersions GetMacBookModel(const std::string& model) {
  struct {
    const char* name;
    MacBookVersions version;
  } static const kModelToVersion[] = {
      {"MacBook4,", MACBOOK_4},          {"MacBook5,", MACBOOK_5},
      {"MacBook6,", MACBOOK_6},          {"MacBook7,", MACBOOK_7},
      {"MacBook8,", MACBOOK_8},          {"MacBook9,", MACBOOK_9},
      {"MacBook10,", MACBOOK_10},        {"MacBookPro5,", MACBOOK_PRO_5},
      {"MacBookPro6,", MACBOOK_PRO_6},   {"MacBookPro7,", MACBOOK_PRO_7},
      {"MacBookPro8,", MACBOOK_PRO_8},   {"MacBookPro9,", MACBOOK_PRO_9},
      {"MacBookPro10,", MACBOOK_PRO_10}, {"MacBookPro11,", MACBOOK_PRO_11},
      {"MacBookPro12,", MACBOOK_PRO_12}, {"MacBookPro13,", MACBOOK_PRO_13},
      {"MacBookAir3,", MACBOOK_AIR_3},   {"MacBookAir4,", MACBOOK_AIR_4},
      {"MacBookAir5,", MACBOOK_AIR_5},   {"MacBookAir6,", MACBOOK_AIR_6},
      {"MacBookAir7,", MACBOOK_AIR_7},   {"MacBookAir8,", MACBOOK_AIR_8},
  };

  for (const auto& entry : kModelToVersion) {
    if (base::StartsWith(model, entry.name,
                         base::CompareCase::INSENSITIVE_ASCII)) {
      return entry.version;
    }
  }
  return OTHER;
}

// Add Uma stats for number of detected devices on MacBooks. These are used for
// investigating crbug/582931.
void MaybeWriteUma(int number_of_devices, int number_of_suspended_devices) {
  std::string model = base::mac::GetModelIdentifier();
  if (!base::StartsWith(model, "MacBook",
                        base::CompareCase::INSENSITIVE_ASCII)) {
    return;
  }
  static int attempt_since_process_start_counter = 0;
  static bool has_seen_zero_device_count = false;
  const int attempt_count_since_process_start =
      ++attempt_since_process_start_counter;
  const int device_count = number_of_devices + number_of_suspended_devices;
  UMA_HISTOGRAM_COUNTS_1M("Media.VideoCapture.MacBook.NumberOfDevices",
                          device_count);
  if (device_count == 0) {
    UMA_HISTOGRAM_ENUMERATION(
        "Media.VideoCapture.MacBook.HardwareVersionWhenNoCamera",
        GetMacBookModel(model), MAX_MACBOOK_VERSION + 1);
    if (!has_seen_zero_device_count) {
      UMA_HISTOGRAM_COUNTS_1M(
          "Media.VideoCapture.MacBook.AttemptCountWhenNoCamera",
          attempt_count_since_process_start);
      has_seen_zero_device_count = true;
    }
  }
}

}  // namespace

std::string MacFourCCToString(OSType fourcc) {
  char arr[] = {static_cast<char>(fourcc >> 24),
                static_cast<char>(fourcc >> 16), static_cast<char>(fourcc >> 8),
                static_cast<char>(fourcc), 0};
  return arr;
}

bool ExtractBaseAddressAndLength(char** base_address,
                                 size_t* length,
                                 CMSampleBufferRef sample_buffer) {
  CMBlockBufferRef block_buffer = CMSampleBufferGetDataBuffer(sample_buffer);
  DCHECK(block_buffer);

  size_t length_at_offset;
  const OSStatus status = CMBlockBufferGetDataPointer(
      block_buffer, 0, &length_at_offset, length, base_address);
  DCHECK_EQ(noErr, status);
  // Expect the (M)JPEG data to be available as a contiguous reference, i.e.
  // not covered by multiple memory blocks.
  DCHECK_EQ(length_at_offset, *length);
  return status == noErr && length_at_offset == *length;
}

NSDictionary<NSString*, DeviceNameAndTransportType*>*
GetVideoCaptureDeviceNames() {
  // At this stage we already know that AVFoundation is supported and the whole
  // library is loaded and initialised, by the device monitoring.
  NSMutableDictionary<NSString*, DeviceNameAndTransportType*>* device_names =
      [[NSMutableDictionary alloc] init];

  NSArray<AVCaptureDevice*>* devices = GetVideoCaptureDevices(
      base::FeatureList::IsEnabled(media::kUseAVCaptureDeviceDiscoverySession));

  int number_of_suspended_devices = 0;
  for (AVCaptureDevice* device in devices) {
    if ([device hasMediaType:AVMediaTypeVideo] ||
        [device hasMediaType:AVMediaTypeMuxed]) {
      if (device.suspended) {
        ++number_of_suspended_devices;
        continue;
      }

      // Transport types are defined for Audio devices and reused for video.
      int transport_type = device.transportType;
      VideoCaptureTransportType device_transport_type =
          (transport_type == kIOAudioDeviceTransportTypeBuiltIn ||
           transport_type == kIOAudioDeviceTransportTypeUSB)
              ? VideoCaptureTransportType::MACOSX_USB_OR_BUILT_IN
              : VideoCaptureTransportType::OTHER_TRANSPORT;
      DeviceNameAndTransportType* name_and_transport_type =
          [[DeviceNameAndTransportType alloc]
               initWithName:device.localizedName
              transportType:device_transport_type];
      device_names[device.uniqueID] = name_and_transport_type;
    }
  }
  MaybeWriteUma(device_names.count, number_of_suspended_devices);
  return device_names;
}

gfx::Size GetPixelBufferSize(CVPixelBufferRef pixel_buffer) {
  return gfx::Size(CVPixelBufferGetWidth(pixel_buffer),
                   CVPixelBufferGetHeight(pixel_buffer));
}

gfx::Size GetSampleBufferSize(CMSampleBufferRef sample_buffer) {
  if (CVPixelBufferRef pixel_buffer =
          CMSampleBufferGetImageBuffer(sample_buffer)) {
    return GetPixelBufferSize(pixel_buffer);
  }
  CMFormatDescriptionRef format_description =
      CMSampleBufferGetFormatDescription(sample_buffer);
  CMVideoDimensions dimensions =
      CMVideoFormatDescriptionGetDimensions(format_description);
  return gfx::Size(dimensions.width, dimensions.height);
}

BASE_FEATURE(kUseAVCaptureDeviceDiscoverySession,
             "UseAVCaptureDeviceDiscoverySession",
             base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace media
