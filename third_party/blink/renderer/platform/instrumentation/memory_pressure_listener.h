// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_INSTRUMENTATION_MEMORY_PRESSURE_LISTENER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_INSTRUMENTATION_MEMORY_PRESSURE_LISTENER_H_

#include "third_party/blink/renderer/platform/platform_export.h"

namespace blink {

// TODO(crbug.com/441951621): This class no longer has anything to do with
// memory pressure and only contains low-end device helpers. Move them to a
// dedicated file and delete this one.
class PLATFORM_EXPORT MemoryPressureListenerRegistry final {
 public:
  // See: SysUtils::IsLowEndDevice for the full details of what "low-end" means.
  // This returns true for devices that can use more extreme tradeoffs for
  // performance. Many low memory devices (<=1GB) are not considered low-end.
  // Can be overridden in web tests via internals.
  static bool IsLowEndDevice();

  // Returns true when IsLowEndDevice() returns true or when the feature
  // PartialLowEndModeOnMidEndDevices is enabled on Android devices.
  static bool IsLowEndDeviceOrPartialLowEndModeEnabled();

  // Returns true when IsLowEndDevice() Or PartialLowEndModeOnMidEndDevices is
  // enabled and canvas font cache is not excluded from the features.
  static bool
  IsLowEndDeviceOrPartialLowEndModeEnabledIncludingCanvasFontCache();

  // Caches whether this device is a low-end device and the device physical
  // memory in static members. instance() is not used as it's a heap allocated
  // object - meaning it's not thread-safe as well as might break tests counting
  // the heap size.
  static void Initialize();

 private:
  friend class Internals;

  static void SetIsLowEndDeviceForTesting(bool);

  static bool is_low_end_device_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_INSTRUMENTATION_MEMORY_PRESSURE_LISTENER_H_
