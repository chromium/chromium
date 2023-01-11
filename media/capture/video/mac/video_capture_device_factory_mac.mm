// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/mac/video_capture_device_factory_mac.h"

#import <IOKit/audio/IOAudioTypes.h>
#include <stddef.h>

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#import "media/capture/video/mac/video_capture_device_avfoundation_mac.h"
#import "media/capture/video/mac/video_capture_device_avfoundation_utils_mac.h"
#import "media/capture/video/mac/video_capture_device_decklink_mac.h"
#include "media/capture/video/mac/video_capture_device_mac.h"

namespace {

void EnsureRunsOnCFRunLoopEnabledThread() {
  static bool has_checked_cfrunloop_for_video_capture = false;
  if (!has_checked_cfrunloop_for_video_capture) {
    base::ScopedCFTypeRef<CFRunLoopMode> mode(
        CFRunLoopCopyCurrentMode(CFRunLoopGetCurrent()));
    CHECK(mode != NULL)
        << "The MacOS video capture code must be run on a CFRunLoop-enabled "
           "thread";
    has_checked_cfrunloop_for_video_capture = true;
  }
}

media::VideoCaptureFormats GetDeviceSupportedFormats(
    const media::VideoCaptureDeviceDescriptor& descriptor) {
  media::VideoCaptureFormats formats;

  NSArray<AVCaptureDevice*>* devices = nil;
  // The awkward repeated if statements are required for the compiler to
  // recognise that the contained code is protected by an API version check.
  if (@available(macOS 10.15, *)) {
    if (base::FeatureList::IsEnabled(
            media::kUseAVCaptureDeviceDiscoverySession)) {
      // Query for all camera device types available on macOS. The others in the
      // enum are only supported on iOS/iPadOS.
      NSArray* captureDeviceType = @[
        AVCaptureDeviceTypeBuiltInWideAngleCamera,
        AVCaptureDeviceTypeExternalUnknown
      ];
      AVCaptureDeviceDiscoverySession* deviceDescoverySession =
          [AVCaptureDeviceDiscoverySession
              discoverySessionWithDeviceTypes:captureDeviceType
                                    mediaType:AVMediaTypeVideo
                                     position:
                                         AVCaptureDevicePositionUnspecified];
      devices = deviceDescoverySession.devices;
    }
  }
  if (!devices) {
    devices = [AVCaptureDevice devices];
  }

  AVCaptureDevice* device = nil;
  for (device in devices) {
    if (base::SysNSStringToUTF8([device uniqueID]) == descriptor.device_id)
      break;
  }
  if (device == nil)
    return media::VideoCaptureFormats();
  for (AVCaptureDeviceFormat* device_format in device.formats) {
    // MediaSubType is a CMPixelFormatType but can be used as CVPixelFormatType
    // as well according to CMFormatDescription.h
    const media::VideoPixelFormat pixelFormat = [VideoCaptureDeviceAVFoundation
        FourCCToChromiumPixelFormat:CMFormatDescriptionGetMediaSubType(
                                        [device_format formatDescription])];

    CMVideoDimensions dimensions = CMVideoFormatDescriptionGetDimensions(
        [device_format formatDescription]);

    for (AVFrameRateRange* frameRate in
         [device_format videoSupportedFrameRateRanges]) {
      media::VideoCaptureFormat format(
          gfx::Size(dimensions.width, dimensions.height),
          frameRate.maxFrameRate, pixelFormat);
      DVLOG(2) << descriptor.display_name() << " "
               << media::VideoCaptureFormat::ToString(format);
      formats.push_back(std::move(format));
    }
  }
  return formats;
}

// Blocked devices are identified by a characteristic trailing substring of
// uniqueId. At the moment these are just Blackmagic devices.
const char* kBlockedCamerasIdSignature[] = {"-01FDA82C8A9C"};

int32_t get_device_descriptors_retry_count = 0;

}  // anonymous namespace

namespace media {

static bool IsDeviceBlocked(const VideoCaptureDeviceDescriptor& descriptor) {
  bool is_device_blocked = false;
  for (size_t i = 0;
       !is_device_blocked && i < std::size(kBlockedCamerasIdSignature); ++i) {
    is_device_blocked =
        base::EndsWith(descriptor.device_id, kBlockedCamerasIdSignature[i],
                       base::CompareCase::INSENSITIVE_ASCII);
  }
  DVLOG_IF(2, is_device_blocked)
      << "Blocked camera: " << descriptor.display_name()
      << ", id: " << descriptor.device_id;
  return is_device_blocked;
}

VideoCaptureDeviceFactoryMac::VideoCaptureDeviceFactoryMac() {
  thread_checker_.DetachFromThread();
}

VideoCaptureDeviceFactoryMac::~VideoCaptureDeviceFactoryMac() {
}

// static
void VideoCaptureDeviceFactoryMac::SetGetDevicesInfoRetryCount(int count) {
  get_device_descriptors_retry_count = count;
}

// static
int VideoCaptureDeviceFactoryMac::GetGetDevicesInfoRetryCount() {
  return get_device_descriptors_retry_count;
}

VideoCaptureErrorOrDevice VideoCaptureDeviceFactoryMac::CreateDevice(
    const VideoCaptureDeviceDescriptor& descriptor) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_NE(descriptor.capture_api, VideoCaptureApi::UNKNOWN);
  EnsureRunsOnCFRunLoopEnabledThread();

  std::unique_ptr<VideoCaptureDevice> capture_device;
  if (descriptor.capture_api == VideoCaptureApi::MACOSX_DECKLINK) {
    capture_device =
        std::make_unique<VideoCaptureDeviceDeckLinkMac>(descriptor);
  } else {
    VideoCaptureDeviceMac* device = new VideoCaptureDeviceMac(descriptor);
    capture_device.reset(device);
    if (!device->Init(descriptor.capture_api)) {
      LOG(ERROR) << "Could not initialize VideoCaptureDevice.";
      capture_device.reset();
    }
  }
  return capture_device ? VideoCaptureErrorOrDevice(std::move(capture_device))
                        : VideoCaptureErrorOrDevice(
                              VideoCaptureError::kMacSetCaptureDeviceFailed);
}

void VideoCaptureDeviceFactoryMac::GetDevicesInfo(
    GetDevicesInfoCallback callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  EnsureRunsOnCFRunLoopEnabledThread();

  // Loop through all available devices and add to |devices_info|.
  std::vector<VideoCaptureDeviceInfo> devices_info;
  DVLOG(1) << "Enumerating video capture devices using AVFoundation";
  base::scoped_nsobject<NSDictionary> capture_devices =
      GetVideoCaptureDeviceNames();
  // Enumerate all devices found by AVFoundation, translate the info for each
  // to class Name and add it to |device_names|.
  for (NSString* key in capture_devices.get()) {
    const std::string device_id = [key UTF8String];
    const VideoCaptureApi capture_api = VideoCaptureApi::MACOSX_AVFOUNDATION;
    int transport_type = [[capture_devices valueForKey:key] transportType];
    // Transport types are defined for Audio devices and reused for video.
    VideoCaptureTransportType device_transport_type =
        (transport_type == kIOAudioDeviceTransportTypeBuiltIn ||
         transport_type == kIOAudioDeviceTransportTypeUSB)
            ? VideoCaptureTransportType::MACOSX_USB_OR_BUILT_IN
            : VideoCaptureTransportType::OTHER_TRANSPORT;
    const std::string model_id = VideoCaptureDeviceMac::GetDeviceModelId(
        device_id, capture_api, device_transport_type);
    const VideoCaptureControlSupport control_support =
        VideoCaptureDeviceMac::GetControlSupport(model_id);
    VideoCaptureDeviceDescriptor descriptor(
        [[[capture_devices valueForKey:key] deviceName] UTF8String], device_id,
        model_id, capture_api, control_support, device_transport_type);
    if (IsDeviceBlocked(descriptor))
      continue;
    devices_info.emplace_back(descriptor);

    // Get supported formats
    devices_info.back().supported_formats =
        GetDeviceSupportedFormats(descriptor);
  }

  // Also retrieve Blackmagic devices, if present, via DeckLink SDK API.
  VideoCaptureDeviceDeckLinkMac::EnumerateDevices(&devices_info);

  std::move(callback).Run(std::move(devices_info));
}

}  // namespace media
