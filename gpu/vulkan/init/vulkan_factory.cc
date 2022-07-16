// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/vulkan/init/vulkan_factory.h"
#include "build/build_config.h"

#include <memory>

#if defined(OS_ANDROID)
#include "gpu/vulkan/android/vulkan_implementation_android.h"
#endif

#if defined(OS_WIN)
#include "gpu/vulkan/win32/vulkan_implementation_win32.h"
#endif

#if defined(USE_OZONE)
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/surface_factory_ozone.h"
#endif

namespace gpu {

std::unique_ptr<VulkanImplementation> CreateVulkanImplementation(
    bool use_swiftshader,
    bool allow_protected_memory) {
#if defined(USE_OZONE)
  return ui::OzonePlatform::GetInstance()
      ->GetSurfaceFactoryOzone()
      ->CreateVulkanImplementation(use_swiftshader, allow_protected_memory);
#else

#if !defined(OS_WIN)
  // TODO(samans): Support Swiftshader on more platforms.
  // https://crbug.com/963988
  DCHECK(!use_swiftshader)
      << "Vulkan Swiftshader is not supported on this platform.";
#endif  // !defined(OS_WIN)

  // Protected memory is supported only on Fuchsia, which uses Ozone, i.e.
  // VulkanImplementation is initialized above.
  DCHECK(!allow_protected_memory)
      << "Protected memory is not supported on this platform.";

#if defined(OS_ANDROID)
  return std::make_unique<VulkanImplementationAndroid>();
#elif defined(OS_WIN)
  return std::make_unique<VulkanImplementationWin32>(use_swiftshader);
#else
  NOTREACHED();
  return {};
#endif
#endif
}

}  // namespace gpu
