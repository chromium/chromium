#!/usr/bin/env python
# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""code generator for Vulkan function pointers."""

import filecmp
import optparse
import os
import platform
import sys
from string import Template
from subprocess import call

VULKAN_UNASSOCIATED_FUNCTIONS = [
  {
    'functions': [
      # vkGetInstanceProcAddr belongs here but is handled specially.
      # vkEnumerateInstanceVersion belongs here but is handled specially.
      'vkCreateInstance',
      'vkEnumerateInstanceExtensionProperties',
      'vkEnumerateInstanceLayerProperties',
    ]
  }
]

VULKAN_INSTANCE_FUNCTIONS = [
  {
    'functions': [
      'vkCreateDevice',
      'vkDestroyInstance',
      'vkEnumerateDeviceLayerProperties',
      'vkEnumeratePhysicalDevices',
      'vkGetDeviceProcAddr',
      'vkGetPhysicalDeviceFeatures',
      'vkGetPhysicalDeviceFormatProperties',
      'vkGetPhysicalDeviceMemoryProperties',
      'vkGetPhysicalDeviceProperties',
      'vkGetPhysicalDeviceQueueFamilyProperties',
    ]
  },
  {
    'extension': 'VK_KHR_SURFACE_EXTENSION_NAME',
    'functions': [
      'vkDestroySurfaceKHR',
      'vkGetPhysicalDeviceSurfaceCapabilitiesKHR',
      'vkGetPhysicalDeviceSurfaceFormatsKHR',
      'vkGetPhysicalDeviceSurfaceSupportKHR',
    ]
  },
  {
    'ifdef': 'defined(USE_VULKAN_XLIB)',
    'extension': 'VK_KHR_XLIB_SURFACE_EXTENSION_NAME',
    'functions': [
      'vkCreateXlibSurfaceKHR',
      'vkGetPhysicalDeviceXlibPresentationSupportKHR',
    ]
  },
  {
    'ifdef': 'defined(OS_ANDROID)',
    'extension': 'VK_KHR_ANDROID_SURFACE_EXTENSION_NAME',
    'functions': [
      'vkCreateAndroidSurfaceKHR',
    ]
  },
  {
    'ifdef': 'defined(OS_FUCHSIA)',
    'extension': 'VK_FUCHSIA_IMAGEPIPE_SURFACE_EXTENSION_NAME',
    'functions': [
      'vkCreateImagePipeSurfaceFUCHSIA',
    ]
  },
  {
  'min_api_version': 'VK_API_VERSION_1_1',
    'functions': [
      'vkGetPhysicalDeviceImageFormatProperties2',
    ]
  },
  {
    # vkGetPhysicalDeviceFeatures2() is defined in Vulkan 1.1 or suffixed in the
    # VK_KHR_get_physical_device_properties2 extension.
    'min_api_version': 'VK_API_VERSION_1_1',
    'extension': 'VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME',
    'extension_suffix': 'KHR',
    'functions': [
      'vkGetPhysicalDeviceFeatures2',
    ]
  },
]

VULKAN_DEVICE_FUNCTIONS = [
  {
    'functions': [
      'vkAllocateCommandBuffers',
      'vkAllocateDescriptorSets',
      'vkAllocateMemory',
      'vkBeginCommandBuffer',
      'vkBindBufferMemory',
      'vkBindImageMemory',
      'vkCmdBeginRenderPass',
      'vkCmdCopyBufferToImage',
      'vkCmdEndRenderPass',
      'vkCmdExecuteCommands',
      'vkCmdNextSubpass',
      'vkCmdPipelineBarrier',
      'vkCreateBuffer',
      'vkCreateCommandPool',
      'vkCreateDescriptorPool',
      'vkCreateDescriptorSetLayout',
      'vkCreateFence',
      'vkCreateFramebuffer',
      'vkCreateImage',
      'vkCreateImageView',
      'vkCreateRenderPass',
      'vkCreateSampler',
      'vkCreateSemaphore',
      'vkCreateShaderModule',
      'vkDestroyBuffer',
      'vkDestroyCommandPool',
      'vkDestroyDescriptorPool',
      'vkDestroyDescriptorSetLayout',
      'vkDestroyDevice',
      'vkDestroyFence',
      'vkDestroyFramebuffer',
      'vkDestroyImage',
      'vkDestroyImageView',
      'vkDestroyRenderPass',
      'vkDestroySampler',
      'vkDestroySemaphore',
      'vkDestroyShaderModule',
      'vkDeviceWaitIdle',
      'vkEndCommandBuffer',
      'vkFreeCommandBuffers',
      'vkFreeDescriptorSets',
      'vkFreeMemory',
      'vkGetBufferMemoryRequirements',
      'vkGetDeviceQueue',
      'vkGetFenceStatus',
      'vkGetImageMemoryRequirements',
      'vkMapMemory',
      'vkQueueSubmit',
      'vkQueueWaitIdle',
      'vkResetCommandBuffer',
      'vkResetFences',
      'vkUnmapMemory',
      'vkUpdateDescriptorSets',
      'vkWaitForFences',
    ]
  },
  {
    'min_api_version': 'VK_API_VERSION_1_1',
    'functions': [
      'vkGetDeviceQueue2',
      'vkGetImageMemoryRequirements2',
    ]
  },
  {
    'ifdef': 'defined(OS_ANDROID)',
    'extension':
        'VK_ANDROID_EXTERNAL_MEMORY_ANDROID_HARDWARE_BUFFER_EXTENSION_NAME',
    'functions': [
      'vkGetAndroidHardwareBufferPropertiesANDROID',
    ]
  },
  {
    'ifdef': 'defined(OS_LINUX) || defined(OS_ANDROID)',
    'extension': 'VK_KHR_EXTERNAL_SEMAPHORE_FD_EXTENSION_NAME',
    'functions': [
      'vkGetSemaphoreFdKHR',
      'vkImportSemaphoreFdKHR',
    ]
  },
  {
    'ifdef': 'defined(OS_LINUX)',
    'extension': 'VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME',
    'functions': [
      'vkGetMemoryFdKHR',
      'vkGetMemoryFdPropertiesKHR',
    ]
  },
  {
    'ifdef': 'defined(OS_FUCHSIA)',
    'extension': 'VK_FUCHSIA_EXTERNAL_SEMAPHORE_EXTENSION_NAME',
    'functions': [
      'vkImportSemaphoreZirconHandleFUCHSIA',
      'vkGetSemaphoreZirconHandleFUCHSIA',
    ]
  },
  {
    'ifdef': 'defined(OS_FUCHSIA)',
    'extension': 'VK_FUCHSIA_BUFFER_COLLECTION_EXTENSION_NAME',
    'functions': [
      'vkCreateBufferCollectionFUCHSIA',
      'vkSetBufferCollectionConstraintsFUCHSIA',
      'vkGetBufferCollectionPropertiesFUCHSIA',
      'vkDestroyBufferCollectionFUCHSIA',
    ]
  },
  {
    'extension': 'VK_KHR_SWAPCHAIN_EXTENSION_NAME',
    'functions': [
      'vkAcquireNextImageKHR',
      'vkCreateSwapchainKHR',
      'vkDestroySwapchainKHR',
      'vkGetSwapchainImagesKHR',
      'vkQueuePresentKHR',
    ]
  }

]

SELF_LOCATION = os.path.dirname(os.path.abspath(__file__))

LICENSE_AND_HEADER = """\
// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file is auto-generated from
// gpu/vulkan/generate_bindings.py
// It's formatted by clang-format using chromium coding style:
//    clang-format -i -style=chromium filename
// DO NOT EDIT!

"""

def WriteFunctions(file, functions, template, check_extension=False):
  for group in functions:
    if 'ifdef' in group:
      file.write('#if %s\n' % group['ifdef'])

    extension = group['extension'] if 'extension' in group else ''
    min_api_version = \
        group['min_api_version'] if 'min_api_version' in group else ''

    if not check_extension:
      for func in group['functions']:
        file.write(template.substitute({'name': func}))
    elif not extension and not min_api_version:
      for func in group['functions']:
        file.write(template.substitute({'name': func, 'extension_suffix': ''}))
    else:
      if min_api_version:
        file.write('  if (api_version >= %s) {\n' % min_api_version)

        for func in group['functions']:
          file.write(
              template.substitute({'name': func,'extension_suffix': ''}))

        file.write('}\n')
        if extension:
          file.write('else ')

      if extension:
        file.write('if (gfx::HasExtension(enabled_extensions, %s)) {\n' %
                   extension)

        extension_suffix = \
            group['extension_suffix'] if 'extension_suffix' in group \
            else ''
        for func in group['functions']:
          file.write(template.substitute(
              {'name': func, 'extension_suffix': extension_suffix}))

        file.write('}\n')

    if 'ifdef' in group:
      file.write('#endif  // %s\n' % group['ifdef'])

    file.write('\n')

def WriteFunctionDeclarations(file, functions):
  template = Template('  PFN_${name} ${name}Fn = nullptr;\n')
  WriteFunctions(file, functions, template)

def WriteMacros(file, functions):
  template = Template(
      '#define $name gpu::GetVulkanFunctionPointers()->${name}Fn\n')
  WriteFunctions(file, functions, template)

def GenerateHeaderFile(file):
  """Generates gpu/vulkan/vulkan_function_pointers.h"""

  file.write(LICENSE_AND_HEADER +
"""

#ifndef GPU_VULKAN_VULKAN_FUNCTION_POINTERS_H_
#define GPU_VULKAN_VULKAN_FUNCTION_POINTERS_H_

#include <vulkan/vulkan.h>

#include "base/native_library.h"
#include "build/build_config.h"
#include "gpu/vulkan/vulkan_export.h"
#include "ui/gfx/extension_set.h"

#if defined(OS_ANDROID)
#include <vulkan/vulkan_android.h>
#endif

#if defined(OS_FUCHSIA)
#include <zircon/types.h>
// <vulkan/vulkan_fuchsia.h> must be included after <zircon/types.h>
#include <vulkan/vulkan_fuchsia.h>

#include "gpu/vulkan/fuchsia/vulkan_fuchsia_ext.h"
#endif

#if defined(USE_VULKAN_XLIB)
#include <X11/Xlib.h>
#include <vulkan/vulkan_xlib.h>
#endif

namespace gpu {

struct VulkanFunctionPointers;

VULKAN_EXPORT VulkanFunctionPointers* GetVulkanFunctionPointers();

struct VulkanFunctionPointers {
  VulkanFunctionPointers();
  ~VulkanFunctionPointers();

  VULKAN_EXPORT bool BindUnassociatedFunctionPointers();

  // These functions assume that vkGetInstanceProcAddr has been populated.
  VULKAN_EXPORT bool BindInstanceFunctionPointers(
      VkInstance vk_instance,
      uint32_t api_version,
      const gfx::ExtensionSet& enabled_extensions);

  // These functions assume that vkGetDeviceProcAddr has been populated.
  VULKAN_EXPORT bool BindDeviceFunctionPointers(
      VkDevice vk_device,
      uint32_t api_version,
      const gfx::ExtensionSet& enabled_extensions);

  base::NativeLibrary vulkan_loader_library_ = nullptr;

  // Unassociated functions
  PFN_vkEnumerateInstanceVersion vkEnumerateInstanceVersionFn = nullptr;
  PFN_vkGetInstanceProcAddr vkGetInstanceProcAddrFn = nullptr;
""")

  WriteFunctionDeclarations(file, VULKAN_UNASSOCIATED_FUNCTIONS)

  file.write("""\

  // Instance functions
""")

  WriteFunctionDeclarations(file, VULKAN_INSTANCE_FUNCTIONS);

  file.write("""\

  // Device functions
""")

  WriteFunctionDeclarations(file, VULKAN_DEVICE_FUNCTIONS)

  file.write("""\
};

}  // namespace gpu

// Unassociated functions
""")

  WriteMacros(file, [{'functions': [ 'vkGetInstanceProcAddr' ]}])
  WriteMacros(file, VULKAN_UNASSOCIATED_FUNCTIONS)

  file.write("""\

// Instance functions
""")

  WriteMacros(file, VULKAN_INSTANCE_FUNCTIONS);

  file.write("""\

// Device functions
""")

  WriteMacros(file, VULKAN_DEVICE_FUNCTIONS)

  file.write("""\

#endif  // GPU_VULKAN_VULKAN_FUNCTION_POINTERS_H_
""")

def WriteFunctionPointerInitialization(file, proc_addr_function, parent,
                                       functions):
  template = Template("""  ${name}Fn = reinterpret_cast<PFN_${name}>(
    ${get_proc_addr}(${parent}, "${name}${extension_suffix}"));
  if (!${name}Fn) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "${name}${extension_suffix}";
    return false;
  }

""")

  # Substitute all values in the template, except name, which is processed in
  # WriteFunctions().
  template = Template(template.substitute({
        'name': '${name}', 'extension_suffix': '${extension_suffix}',
        'get_proc_addr': proc_addr_function, 'parent': parent}))

  WriteFunctions(file, functions, template, check_extension=True)

def WriteUnassociatedFunctionPointerInitialization(file, functions):
  WriteFunctionPointerInitialization(file, 'vkGetInstanceProcAddrFn', 'nullptr',
                                     functions)

def WriteInstanceFunctionPointerInitialization(file, functions):
  WriteFunctionPointerInitialization(file, 'vkGetInstanceProcAddrFn',
                                     'vk_instance', functions)

def WriteDeviceFunctionPointerInitialization(file, functions):
  WriteFunctionPointerInitialization(file, 'vkGetDeviceProcAddrFn', 'vk_device',
                                     functions)

def GenerateSourceFile(file):
  """Generates gpu/vulkan/vulkan_function_pointers.cc"""

  file.write(LICENSE_AND_HEADER +
"""

#include "gpu/vulkan/vulkan_function_pointers.h"

#include "base/no_destructor.h"

namespace gpu {

VulkanFunctionPointers* GetVulkanFunctionPointers() {
  static base::NoDestructor<VulkanFunctionPointers> vulkan_function_pointers;
  return vulkan_function_pointers.get();
}

VulkanFunctionPointers::VulkanFunctionPointers() = default;
VulkanFunctionPointers::~VulkanFunctionPointers() = default;

bool VulkanFunctionPointers::BindUnassociatedFunctionPointers() {
  // vkGetInstanceProcAddr must be handled specially since it gets its function
  // pointer through base::GetFunctionPOinterFromNativeLibrary(). Other Vulkan
  // functions don't do this.
  vkGetInstanceProcAddrFn = reinterpret_cast<PFN_vkGetInstanceProcAddr>(
      base::GetFunctionPointerFromNativeLibrary(vulkan_loader_library_,
                                                "vkGetInstanceProcAddr"));
  if (!vkGetInstanceProcAddrFn)
    return false;

  vkEnumerateInstanceVersionFn =
      reinterpret_cast<PFN_vkEnumerateInstanceVersion>(
          vkGetInstanceProcAddrFn(nullptr, "vkEnumerateInstanceVersion"));
  // vkEnumerateInstanceVersion didn't exist in Vulkan 1.0, so we should
  // proceed even if we fail to get vkEnumerateInstanceVersion pointer.
""")

  WriteUnassociatedFunctionPointerInitialization(
      file, VULKAN_UNASSOCIATED_FUNCTIONS)

  file.write("""\

  return true;
}

bool VulkanFunctionPointers::BindInstanceFunctionPointers(
    VkInstance vk_instance,
    uint32_t api_version,
    const gfx::ExtensionSet& enabled_extensions) {
""")

  WriteInstanceFunctionPointerInitialization(file, VULKAN_INSTANCE_FUNCTIONS);

  file.write("""\

  return true;
}

bool VulkanFunctionPointers::BindDeviceFunctionPointers(
    VkDevice vk_device,
    uint32_t api_version,
    const gfx::ExtensionSet& enabled_extensions) {
  // Device functions
""")
  WriteDeviceFunctionPointerInitialization(file, VULKAN_DEVICE_FUNCTIONS)

  file.write("""\

  return true;
}

}  // namespace gpu
""")

def main(argv):
  """This is the main function."""

  parser = optparse.OptionParser()
  parser.add_option(
      "--output-dir",
      help="Output directory for generated files. Defaults to this script's "
      "directory.")
  parser.add_option(
      "-c", "--check", action="store_true",
      help="Check if output files match generated files in chromium root "
      "directory. Use this in PRESUBMIT scripts with --output-dir.")

  (options, _) = parser.parse_args(args=argv)

  # Support generating files for PRESUBMIT.
  if options.output_dir:
    output_dir = options.output_dir
  else:
    output_dir = SELF_LOCATION

  def ClangFormat(filename):
    formatter = "clang-format"
    if platform.system() == "Windows":
      formatter += ".bat"
    call([formatter, "-i", "-style=chromium", filename])

  header_file_name = 'vulkan_function_pointers.h'
  header_file = open(
      os.path.join(output_dir, header_file_name), 'wb')
  GenerateHeaderFile(header_file)
  header_file.close()
  ClangFormat(header_file.name)

  source_file_name = 'vulkan_function_pointers.cc'
  source_file = open(
      os.path.join(output_dir, source_file_name), 'wb')
  GenerateSourceFile(source_file)
  source_file.close()
  ClangFormat(source_file.name)

  check_failed_filenames = []
  if options.check:
    for filename in [header_file_name, source_file_name]:
      if not filecmp.cmp(os.path.join(output_dir, filename),
                         os.path.join(SELF_LOCATION, filename)):
        check_failed_filenames.append(filename)

  if len(check_failed_filenames) > 0:
    print 'Please run gpu/vulkan/generate_bindings.py'
    print 'Failed check on generated files:'
    for filename in check_failed_filenames:
      print filename
    return 1

  return 0

if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
