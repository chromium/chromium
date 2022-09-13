// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_VULKAN_INIT_VULKAN_FACTORY_H_
#define GPU_VULKAN_INIT_VULKAN_FACTORY_H_

#include <memory>

#include "base/component_export.h"
#include "gpu/vulkan/vulkan_implementation.h"

namespace gpu {

COMPONENT_EXPORT(VULKAN_INIT)
std::unique_ptr<VulkanImplementation> CreateVulkanImplementation(
    bool use_swiftshader = false,
    bool allow_protected_memory = false);

}  // namespace gpu

#endif  // GPU_VULKAN_INIT_VULKAN_FACTORY_H_
