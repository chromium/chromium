// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SKIA_CONFIG_SKVULKANCONFIG_H_
#define SKIA_CONFIG_SKVULKANCONFIG_H_

#ifdef SK_VULKAN

#if defined(USE_X11)
// TODO(crbug.com/582554): As Skia backs on XCB for vulkan backend
// temporarily define VK_USE_PLATFORM_XCB_KHR to avoid build break.
// Change it to VK_USE_PLATFORM_XLIB_KHR if Skia supports xlib in future.
#define VK_USE_PLATFORM_XCB_KHR
#endif

#include <vulkan/vulkan.h>

#endif

#endif  // SKIA_CONFIG_SKVULKANCONFIG_H_
