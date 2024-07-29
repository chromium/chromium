// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/capture/video/apple/video_capture_device_factory_apple.h"

#include <stddef.h>
#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "media/base/mac/video_capture_device_avfoundation_helpers.h"
#include "media/capture/capture_switches.h"
#include "media/capture/video/apple/video_capture_device_apple.h"
#import "media/capture/video/apple/video_capture_device_avfoundation.h"
#import "media/capture/video/apple/video_capture_device_avfoundation_utils.h"
#include "media/capture/video/video_capture_metrics.h"

#if BUILDFLAG(IS_MAC)
#import <IOKit/audio/IOAudioTypes.h>

#import "media/capture/video/mac/video_capture_device_decklink_mac.h"
#import "media/capture/video/mac/video_capture_metrics_mac.h"
#endif

namespace {

void EnsureRunsOnCFRunLoopEnabledThread() {
  static bool has_checked_cfrunloop_for_video_capture = false;
  if (!has_checked_cfrunloop_for_video_capture) {
    base::apple::ScopedCFTypeRef<CFRunLoopMode> mode(
        CFRunLoopCopyCurrentMode(CFRunLoopGetCurrent()));
    CHECK(mode)
        << "The MacOS video capture code must be run on a CFRunLoop-enabled "
           "thread";
    has_checked_cfrunloop_for_video_capture = true;
  }
}

media::VideoCaptureFormats GetDeviceSupportedFormats(AVCaptureDevice* device) {
  media::VideoCaptureFormats formats;

  for (AVCaptureDeviceFormat* device_format in device.formats) {
    // MediaSubType is a CMPixelFormatType but can be used as CVPixelFormatType
    // as well according to CMFormatDescription.h
    const media::VideoPixelFormat pixelFormat = [VideoCaptureDeviceAVFoundation
        FourCCToChromiumPixelFormat:CMFormatDescriptionGetMediaSubType(
                                        device_format.formatDescription)];

    CMVideoDimensions dimensions =
        CMVideoFormatDescriptionGetDimensions(device_format.formatDescription);

    for (AVFrameRateRange* frameRate in device_format
             .videoSupportedFrameRateRanges) {
      media::VideoCaptureFormat format(
          gfx::Size(dimensions.width, dimensions.height),
          frameRate.maxFrameRate, pixelFormat);
      DVLOG(2) << device.localizedName << " "
               << media::VideoCaptureFormat::ToString(format);
      formats.push_back(std::move(format));
    }
  }
  return formats;
}

// Blocked devices are identified by a characteristic trailing substring of
// uniqueId. At the moment these are just Blackmagic devices.
const char* kBlockedCamerasIdSignature[] = {"-01FDA82C8A9C"};

bool IsDeviceBlockedForAVFoundation(const std::string& device_id) {
  bool is_device_blocked = false;
  for (size_t i = 0;
       !is_device_blocked && i < std::size(kBlockedCamerasIdSignature); ++i) {
    is_device_blocked = base::EndsWith(device_id, kBlockedCamerasIdSignature[i],
                                       base::CompareCase::INSENSITIVE_ASCII);
  }
  return is_device_blocked;
}

bool IsDeviceBlocked(const media::VideoCaptureDeviceDescriptor& descriptor) {
  bool is_device_blocked = IsDeviceBlockedForAVFoundation(descriptor.device_id);
  DVLOG_IF(2, is_device_blocked)
      << "Blocked camera: " << descriptor.display_name()
      << ", id: " << descriptor.device_id;
  return is_device_blocked;
}

}  // anonymous namespace

namespace media {

VideoCaptureDeviceFactoryApple::VideoCaptureDeviceFactoryApple() {
  thread_checker_.DetachFromThread();
}

VideoCaptureErrorOrDevice VideoCaptureDeviceFactoryApple::CreateDevice(
    const VideoCaptureDeviceDescriptor& descriptor) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_NE(descriptor.capture_api, VideoCaptureApi::UNKNOWN);
  EnsureRunsOnCFRunLoopEnabledThread();

  std::unique_ptr<VideoCaptureDevice> capture_device;
  if (descriptor.capture_api != VideoCaptureApi::MACOSX_DECKLINK) {
    VideoCaptureDeviceApple* device = new VideoCaptureDeviceApple(descriptor);
    capture_device.reset(device);
    if (!device->Init(descriptor.capture_api)) {
      LOG(ERROR) << "Could not initialize VideoCaptureDevice.";
      capture_device.reset();
    }
  }
#if BUILDFLAG(IS_MAC)
  else {
    capture_device =
        std::make_unique<VideoCaptureDeviceDeckLinkMac>(descriptor);
  }
#endif

  if (capture_device) {
    LogCaptureDeviceHashedModelId(descriptor);
#if BUILDFLAG(IS_MAC)
    LogReactionEffectsGesturesState();
#endif
  }

  return capture_device ? VideoCaptureErrorOrDevice(std::move(capture_device))
                        : VideoCaptureErrorOrDevice(
                              VideoCaptureError::kMacSetCaptureDeviceFailed);
}

void VideoCaptureDeviceFactoryApple::GetDevicesInfo(
    GetDevicesInfoCallback callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  EnsureRunsOnCFRunLoopEnabledThread();

  NSArray<AVCaptureDevice*>* devices = media::GetVideoCaptureDevices();

  // Loop through all available devices and add to |devices_info|.
  std::vector<VideoCaptureDeviceInfo> devices_info;
  DVLOG(1) << "Enumerating video capture devices using AVFoundation";

  for (AVCaptureDevice* device in devices) {
    if ([device hasMediaType:AVMediaTypeVideo] ||
        [device hasMediaType:AVMediaTypeMuxed]) {
      if (device.suspended) {
        continue;
      }

      const std::string device_id = base::SysNSStringToUTF8(device.uniqueID);
      const VideoCaptureApi capture_api = VideoCaptureApi::MACOSX_AVFOUNDATION;
#if BUILDFLAG(IS_MAC)
      // Transport types are defined for Audio devices and reused for video.
      int transport_type = device.transportType;
      VideoCaptureTransportType device_transport_type =
          (transport_type == kIOAudioDeviceTransportTypeBuiltIn ||
           transport_type == kIOAudioDeviceTransportTypeUSB)
              ? VideoCaptureTransportType::APPLE_USB_OR_BUILT_IN
              : VideoCaptureTransportType::OTHER_TRANSPORT;
#else
      VideoCaptureTransportType device_transport_type =
          VideoCaptureTransportType::APPLE_USB_OR_BUILT_IN;
#endif
      const std::string model_id = VideoCaptureDeviceApple::GetDeviceModelId(
          device_id, capture_api, device_transport_type);
      const VideoCaptureControlSupport control_support =
          VideoCaptureDeviceApple::GetControlSupport(model_id);
      VideoCaptureDeviceDescriptor descriptor(
          base::SysNSStringToUTF8(device.localizedName), device_id, model_id,
          capture_api, control_support, device_transport_type);
      if (IsDeviceBlocked(descriptor)) {
        continue;
      }
      devices_info.emplace_back(descriptor);

      // Get supported formats
      devices_info.back().supported_formats = GetDeviceSupportedFormats(device);
    }
  }

#if BUILDFLAG(IS_MAC)
  // Also retrieve Blackmagic devices, if present, via DeckLink SDK API.
  VideoCaptureDeviceDeckLinkMac::EnumerateDevices(&devices_info);
#endif
  std::move(callback).Run(std::move(devices_info));
}

bool ShouldEnableGpuMemoryBuffer(const std::string& device_id) {
  return !base::CommandLine::ForCurrentProcess()->HasSwitch(
             switches::kDisableVideoCaptureUseGpuMemoryBuffer) &&
         !IsDeviceBlockedForAVFoundation(device_id);
}

}  // namespace media
