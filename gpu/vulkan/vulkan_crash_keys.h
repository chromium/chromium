// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_VULKAN_VULKAN_CRASH_KEYS_H_
#define GPU_VULKAN_VULKAN_CRASH_KEYS_H_

#include <vulkan/vulkan_core.h>

#include "components/crash/core/common/crash_key.h"

namespace gpu {
namespace crash_keys {

extern crash_reporter::CrashKeyString<16> vulkan_api_version;
extern crash_reporter::CrashKeyString<16> vulkan_device_api_version;
extern crash_reporter::CrashKeyString<16> vulkan_device_driver_version;
extern crash_reporter::CrashKeyString<16> vulkan_device_vendor_id;
extern crash_reporter::CrashKeyString<16> vulkan_device_id;
extern crash_reporter::CrashKeyString<16> vulkan_device_type;
extern crash_reporter::CrashKeyString<VK_MAX_PHYSICAL_DEVICE_NAME_SIZE>
    vulkan_device_name;

}  // namespace crash_keys
}  // namespace gpu

#endif  // GPU_VULKAN_VULKAN_CRASH_KEYS_H_
