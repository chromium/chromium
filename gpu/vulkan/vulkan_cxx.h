// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_VULKAN_VULKAN_CXX_H_
#define GPU_VULKAN_VULKAN_CXX_H_

#include <ostream>

#include "base/compiler_specific.h"

// Disable vulkan prototypes.
#if !defined(VK_NO_PROTOTYPES)
#define VK_NO_PROTOTYPES 1
#endif

// Disable dynamic loader tool.
#define VULKAN_HPP_ENABLE_DYNAMIC_LOADER_TOOL 0

// Disable c++ exceptions.
#define VULKAN_HPP_NO_EXCEPTIONS 1

// Disable dynamic dispatch loader.
#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 0

// Set gpu::VulkanFunctionPointers as the default dispatcher.
#define VULKAN_HPP_DEFAULT_DISPATCHER (*gpu::GetVulkanFunctionPointers())
#define VULKAN_HPP_DEFAULT_DISPATCHER_TYPE gpu::VulkanFunctionPointers

#define VULKAN_HPP_TYPESAFE_CONVERSION

#include "gpu/vulkan/vulkan_function_pointers.h"

#include <vulkan/vulkan.hpp>

// operator for LOG() << result
ALWAYS_INLINE std::ostream& operator<<(std::ostream& out, vk::Result result) {
  out << static_cast<VkResult>(result);
  return out;
}

#endif  // GPU_VULKAN_VULKAN_CXX_H_