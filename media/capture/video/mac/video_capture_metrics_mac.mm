// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "media/capture/video/mac/video_capture_metrics_mac.h"

#include <AVFoundation/AVFoundation.h>
#include <CoreMediaIO/CoreMediaIO.h>
#include <Foundation/Foundation.h>
#import <IOKit/audio/IOAudioTypes.h>

#include "base/apple/bridging.h"
#include "base/apple/scoped_cftyperef.h"
#include "base/metrics/histogram_functions.h"
#import "media/capture/video/apple/video_capture_device_avfoundation.h"
#include "media/capture/video/video_capture_device_info.h"

@interface AVCaptureDevice (SPI)
- (UInt32)connectionID;
@end

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

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class AVCaptureDeviceClassification {
  kNonPluginApple = 0,
  kNonPluginThirdPartyTransportBuiltIn = 1,
  kNonPluginThirdPartyTransportOther = 2,
  kNonPluginThirdPartyTransportVirtual = 3,
  kNonPluginThirdPartyTransportNoneOfTheAbove = 4,
  kPluginApple = 5,
  kPluginThirdPartyExtension = 6,
  kPluginThirdPartyNonExtension = 7,
  kMaxValue = kPluginThirdPartyNonExtension,
};

bool HasPrefix(NSString* string, NSString* prefix) {
  return [string rangeOfString:prefix
                       options:NSCaseInsensitiveSearch | NSAnchoredSearch]
             .location != NSNotFound;
}

// Returns a classification of an AVCaptureDevice not based on its underlying
// plugin.
AVCaptureDeviceClassification ClassifyAVCaptureDeviceNonPlugin(
    AVCaptureDevice* device) {
  // The docs say that Apple devices return the manufacturer string "Apple Inc."
  // but in reality, variations are returned, so look for a slightly more broad
  // result.
  if (HasPrefix(device.manufacturer, @"apple")) {
    return AVCaptureDeviceClassification::kNonPluginApple;
  }

  // For now, it is believed that all old-style DAL plugins use the "built-in"
  // transport type, but log a few other types for completeness.
  switch (device.transportType) {
    case kIOAudioDeviceTransportTypeBuiltIn:
      return AVCaptureDeviceClassification::
          kNonPluginThirdPartyTransportBuiltIn;
    case kIOAudioDeviceTransportTypeOther:
      return AVCaptureDeviceClassification::kNonPluginThirdPartyTransportOther;
    case kIOAudioDeviceTransportTypeVirtual:
      return AVCaptureDeviceClassification::
          kNonPluginThirdPartyTransportVirtual;
    default:
      return AVCaptureDeviceClassification::
          kNonPluginThirdPartyTransportNoneOfTheAbove;
  }
}

AVCaptureDeviceClassification ClassifyAVCaptureDevice(AVCaptureDevice* device) {
  CMIOObjectID plugin;
  UInt32 plugin_size = sizeof(plugin);
  CMIOObjectPropertyAddress plugin_address{
      .mSelector = kCMIODevicePropertyPlugIn,
      .mScope = kCMIOObjectPropertyScopeGlobal,
      .mElement = kCMIOObjectPropertyElementMain};
  OSStatus err =
      CMIOObjectGetPropertyData(device.connectionID, &plugin_address, 0,
                                nullptr, plugin_size, &plugin_size, &plugin);
  if (err != noErr) {
    return ClassifyAVCaptureDeviceNonPlugin(device);
  }

  base::apple::ScopedCFTypeRef<CFStringRef> bundle_id;
  UInt32 bundle_id_size = sizeof(CFStringRef);
  CMIOObjectPropertyAddress bundle_id_address{
      .mSelector = kCMIOPlugInPropertyBundleID,
      .mScope = kCMIOObjectPropertyScopeGlobal,
      .mElement = kCMIOObjectPropertyElementMain};
  err = CMIOObjectGetPropertyData(plugin, &bundle_id_address, 0, nullptr,
                                  bundle_id_size, &bundle_id_size,
                                  bundle_id.InitializeInto());
  if (err != noErr || !bundle_id) {
    return ClassifyAVCaptureDeviceNonPlugin(device);
  }

  if (HasPrefix(base::apple::CFToNSPtrCast(bundle_id.get()), @"com.apple")) {
    return AVCaptureDeviceClassification::kPluginApple;
  }

  UInt32 is_extension = 0xFFFF'FFFF;
  UInt32 is_extension_size = sizeof(is_extension);
  CMIOObjectPropertyAddress is_extension_address{
      .mSelector = kCMIOPlugInPropertyIsExtension,
      .mScope = kCMIOObjectPropertyScopeGlobal,
      .mElement = kCMIOObjectPropertyElementMain};
  err = CMIOObjectGetPropertyData(plugin, &is_extension_address, 0, nullptr,
                                  is_extension_size, &is_extension_size,
                                  &is_extension);
  if (err != noErr || is_extension == 0xFFFF'FFFF) {
    return ClassifyAVCaptureDeviceNonPlugin(device);
  }

  return is_extension
             ? AVCaptureDeviceClassification::kPluginThirdPartyExtension
             : AVCaptureDeviceClassification::kPluginThirdPartyNonExtension;
}

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

// NB: This is for determining if it is safe to remove the plugin helper type;
// see https://crbug.com/461717105. When removing this code, be sure to do a
// full revert as to strip references to CoreMediaIO.framework that are no
// longer needed.
void LogAVCaptureDeviceInfo(AVCaptureDevice* device) {
  base::UmaHistogramEnumeration(
      "Media.VideoCapture.Mac.Device.ImplementationClassification",
      ClassifyAVCaptureDevice(device));
}

}  // namespace media
