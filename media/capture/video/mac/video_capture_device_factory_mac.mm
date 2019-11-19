// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/mac/video_capture_device_factory_mac.h"

#import <IOKit/audio/IOAudioTypes.h>
#include <stddef.h>

#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/task_runner_util.h"
#import "media/capture/video/mac/video_capture_device_avfoundation_mac.h"
#import "media/capture/video/mac/video_capture_device_decklink_mac.h"
#include "media/capture/video/mac/video_capture_device_mac.h"
#include "services/video_capture/public/uma/video_capture_service_event.h"

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

// Blacklisted devices are identified by a characteristic trailing substring of
// uniqueId. At the moment these are just Blackmagic devices.
const char* kBlacklistedCamerasIdSignature[] = {"-01FDA82C8A9C"};

int32_t get_device_descriptors_retry_count = 0;

}  // anonymous namespace

namespace media {

static bool IsDeviceBlacklisted(
    const VideoCaptureDeviceDescriptor& descriptor) {
  bool is_device_blacklisted = false;
  for (size_t i = 0;
       !is_device_blacklisted && i < base::size(kBlacklistedCamerasIdSignature);
       ++i) {
    is_device_blacklisted =
        base::EndsWith(descriptor.device_id, kBlacklistedCamerasIdSignature[i],
                       base::CompareCase::INSENSITIVE_ASCII);
  }
  DVLOG_IF(2, is_device_blacklisted)
      << "Blacklisted camera: " << descriptor.display_name()
      << ", id: " << descriptor.device_id;
  return is_device_blacklisted;
}

VideoCaptureDeviceFactoryMac::VideoCaptureDeviceFactoryMac() {
  thread_checker_.DetachFromThread();
}

VideoCaptureDeviceFactoryMac::~VideoCaptureDeviceFactoryMac() {
}

// static
void VideoCaptureDeviceFactoryMac::SetGetDeviceDescriptorsRetryCount(
    int count) {
  get_device_descriptors_retry_count = count;
}

// static
int VideoCaptureDeviceFactoryMac::GetGetDeviceDescriptorsRetryCount() {
  return get_device_descriptors_retry_count;
}

std::unique_ptr<VideoCaptureDevice> VideoCaptureDeviceFactoryMac::CreateDevice(
    const VideoCaptureDeviceDescriptor& descriptor) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_NE(descriptor.capture_api, VideoCaptureApi::UNKNOWN);
  EnsureRunsOnCFRunLoopEnabledThread();

  std::unique_ptr<VideoCaptureDevice> capture_device;
  if (descriptor.capture_api == VideoCaptureApi::MACOSX_DECKLINK) {
    capture_device.reset(new VideoCaptureDeviceDeckLinkMac(descriptor));
  } else {
    VideoCaptureDeviceMac* device = new VideoCaptureDeviceMac(descriptor);
    capture_device.reset(device);
    if (!device->Init(descriptor.capture_api)) {
      LOG(ERROR) << "Could not initialize VideoCaptureDevice.";
      capture_device.reset();
    }
  }
  return std::unique_ptr<VideoCaptureDevice>(std::move(capture_device));
}

void VideoCaptureDeviceFactoryMac::GetDeviceDescriptors(
    VideoCaptureDeviceDescriptors* device_descriptors) {
  DCHECK(thread_checker_.CalledOnValidThread());
  EnsureRunsOnCFRunLoopEnabledThread();

  // Loop through all available devices and add to |device_descriptors|.
  NSDictionary* capture_devices;
  DVLOG(1) << "Enumerating video capture devices using AVFoundation";
  capture_devices = [VideoCaptureDeviceAVFoundation deviceNames];
  // Enumerate all devices found by AVFoundation, translate the info for each
  // to class Name and add it to |device_names|.
  for (NSString* key in capture_devices) {
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
    VideoCaptureDeviceDescriptor descriptor(
        [[[capture_devices valueForKey:key] deviceName] UTF8String], device_id,
        model_id, capture_api, device_transport_type);
    if (IsDeviceBlacklisted(descriptor))
      continue;
    device_descriptors->push_back(descriptor);
  }
  // Also retrieve Blackmagic devices, if present, via DeckLink SDK API.
  VideoCaptureDeviceDeckLinkMac::EnumerateDevices(device_descriptors);

  if ([capture_devices count] > 0 && device_descriptors->empty()) {
    video_capture::uma::LogMacbookRetryGetDeviceInfosEvent(
        video_capture::uma::AVF_DROPPED_DESCRIPTORS_AT_FACTORY);
  }
}

void VideoCaptureDeviceFactoryMac::GetSupportedFormats(
    const VideoCaptureDeviceDescriptor& device,
    VideoCaptureFormats* supported_formats) {
  DCHECK(thread_checker_.CalledOnValidThread());
  switch (device.capture_api) {
    case VideoCaptureApi::MACOSX_AVFOUNDATION:
      DVLOG(1) << "Enumerating video capture capabilities, AVFoundation";
      [VideoCaptureDeviceAVFoundation getDevice:device
                               supportedFormats:supported_formats];
      break;
    case VideoCaptureApi::MACOSX_DECKLINK:
      DVLOG(1) << "Enumerating video capture capabilities "
               << device.display_name();
      VideoCaptureDeviceDeckLinkMac::EnumerateDeviceCapabilities(
          device, supported_formats);
      break;
    default:
      NOTREACHED();
  }
}

}  // namespace media
