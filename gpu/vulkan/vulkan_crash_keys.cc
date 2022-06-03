// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/vulkan/vulkan_crash_keys.h"

namespace gpu {
namespace crash_keys {

crash_reporter::CrashKeyString<16> vulkan_api_version("vulkan-api-version");
crash_reporter::CrashKeyString<16> vulkan_device_api_version(
    "vulkan-device-api-version");
crash_reporter::CrashKeyString<16> vulkan_device_driver_version(
    "vulkan-device-driver-version");
crash_reporter::CrashKeyString<16> vulkan_device_vendor_id(
    "vulkan-device-vendor-id");
crash_reporter::CrashKeyString<16> vulkan_device_id("vulkan-device-id");
crash_reporter::CrashKeyString<16> vulkan_device_type("vulkan-device-type");
crash_reporter::CrashKeyString<VK_MAX_PHYSICAL_DEVICE_NAME_SIZE>
    vulkan_device_name("vulkan-device-name");

}  // namespace crash_keys
}  // namespace gpu
