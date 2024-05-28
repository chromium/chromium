#!/usr/bin/env python3
# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Code generator for Vulkan function pointers."""

import filecmp
import optparse
import os
import platform
import sys
from os import path
from string import Template
from subprocess import call

vulkan_reg_path = path.join(path.dirname(__file__), "..", "..", "third_party",
                            "vulkan-headers", "src", "registry")
sys.path.append(vulkan_reg_path)
from reg import Registry

registry = Registry()
registry.loadFile(open(path.join(vulkan_reg_path, "vk.xml")))

VULKAN_REQUIRED_API_VERSION = 'VK_API_VERSION_1_1'

VULKAN_UNASSOCIATED_FUNCTIONS = [
  {
    'functions': [
      # vkGetInstanceProcAddr belongs here but is handled specially.
      'vkEnumerateInstanceVersion',
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
      'vkEnumerateDeviceExtensionProperties',
      'vkEnumerateDeviceLayerProperties',
      'vkEnumeratePhysicalDevices',
      'vkGetDeviceProcAddr',
      'vkGetPhysicalDeviceExternalSemaphoreProperties',
      'vkGetPhysicalDeviceFeatures2',
      'vkGetPhysicalDeviceFormatProperties',
      'vkGetPhysicalDeviceFormatProperties2',
      'vkGetPhysicalDeviceImageFormatProperties2',
      'vkGetPhysicalDeviceMemoryProperties',
      'vkGetPhysicalDeviceMemoryProperties2',
      'vkGetPhysicalDeviceProperties',
      'vkGetPhysicalDeviceProperties2',
      'vkGetPhysicalDeviceQueueFamilyProperties',
    ]
  },
  {
    'ifdef': 'DCHECK_IS_ON()',
    'extension': 'VK_EXT_DEBUG_REPORT_EXTENSION_NAME',
    'functions': [
      'vkCreateDebugReportCallbackEXT',
      'vkDestroyDebugReportCallbackEXT',
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
    'extension': 'VK_EXT_HEADLESS_SURFACE_EXTENSION_NAME',
    'functions': [
      'vkCreateHeadlessSurfaceEXT',
    ]
  },
  {
    'ifdef': 'defined(USE_VULKAN_XCB)',
    'extension': 'VK_KHR_XCB_SURFACE_EXTENSION_NAME',
    'functions': [
      'vkCreateXcbSurfaceKHR',
      'vkGetPhysicalDeviceXcbPresentationSupportKHR',
    ]
  },
  {
    'ifdef': 'BUILDFLAG(IS_WIN)',
    'extension': 'VK_KHR_WIN32_SURFACE_EXTENSION_NAME',
    'functions': [
      'vkCreateWin32SurfaceKHR',
      'vkGetPhysicalDeviceWin32PresentationSupportKHR',
    ]
  },
  {
    'ifdef': 'BUILDFLAG(IS_ANDROID)',
    'extension': 'VK_KHR_ANDROID_SURFACE_EXTENSION_NAME',
    'functions': [
      'vkCreateAndroidSurfaceKHR',
    ]
  },
  {
    'ifdef': 'BUILDFLAG(IS_FUCHSIA)',
    'extension': 'VK_FUCHSIA_IMAGEPIPE_SURFACE_EXTENSION_NAME',
    'functions': [
      'vkCreateImagePipeSurfaceFUCHSIA',
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
      'vkBindBufferMemory2',
      'vkBindImageMemory',
      'vkBindImageMemory2',
      'vkCmdBeginRenderPass',
      'vkCmdBindDescriptorSets',
      'vkCmdBindPipeline',
      'vkCmdBindVertexBuffers',
      'vkCmdCopyBuffer',
      'vkCmdCopyBufferToImage',
      'vkCmdCopyImage',
      'vkCmdCopyImageToBuffer',
      'vkCmdDraw',
      'vkCmdEndRenderPass',
      'vkCmdExecuteCommands',
      'vkCmdNextSubpass',
      'vkCmdPipelineBarrier',
      'vkCmdPushConstants',
      'vkCmdSetScissor',
      'vkCmdSetViewport',
      'vkCreateBuffer',
      'vkCreateCommandPool',
      'vkCreateDescriptorPool',
      'vkCreateDescriptorSetLayout',
      'vkCreateFence',
      'vkCreateFramebuffer',
      'vkCreateGraphicsPipelines',
      'vkCreateImage',
      'vkCreateImageView',
      'vkCreatePipelineLayout',
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
      'vkDestroyPipeline',
      'vkDestroyPipelineLayout',
      'vkDestroyRenderPass',
      'vkDestroySampler',
      'vkDestroySemaphore',
      'vkDestroyShaderModule',
      'vkDeviceWaitIdle',
      'vkFlushMappedMemoryRanges',
      'vkEndCommandBuffer',
      'vkFreeCommandBuffers',
      'vkFreeDescriptorSets',
      'vkFreeMemory',
      'vkInvalidateMappedMemoryRanges',
      'vkGetBufferMemoryRequirements',
      'vkGetBufferMemoryRequirements2',
      'vkGetDeviceQueue',
      'vkGetDeviceQueue2',
      'vkGetFenceStatus',
      'vkGetImageMemoryRequirements',
      'vkGetImageMemoryRequirements2',
      'vkGetImageSubresourceLayout',
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
    'ifdef': 'BUILDFLAG(IS_ANDROID)',
    'extension':
        'VK_ANDROID_EXTERNAL_MEMORY_ANDROID_HARDWARE_BUFFER_EXTENSION_NAME',
    'functions': [
      'vkGetAndroidHardwareBufferPropertiesANDROID',
    ]
  },
  {
    'ifdef':
    'BUILDFLAG(IS_POSIX)',
    'extension': 'VK_KHR_EXTERNAL_SEMAPHORE_FD_EXTENSION_NAME',
    'functions': [
      'vkGetSemaphoreFdKHR',
      'vkImportSemaphoreFdKHR',
    ]
  },
  {
    'ifdef': 'BUILDFLAG(IS_WIN)',
    'extension': 'VK_KHR_EXTERNAL_SEMAPHORE_WIN32_EXTENSION_NAME',
    'functions': [
      'vkGetSemaphoreWin32HandleKHR',
      'vkImportSemaphoreWin32HandleKHR',
    ]
  },
  {
    'ifdef':
    'BUILDFLAG(IS_POSIX)',
    'extension': 'VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME',
    'functions': [
      'vkGetMemoryFdKHR',
      'vkGetMemoryFdPropertiesKHR',
    ]
  },
  {
    'ifdef': 'BUILDFLAG(IS_WIN)',
    'extension': 'VK_KHR_EXTERNAL_MEMORY_WIN32_EXTENSION_NAME',
    'functions': [
      'vkGetMemoryWin32HandleKHR',
      'vkGetMemoryWin32HandlePropertiesKHR',
    ]
  },
  {
    'ifdef': 'BUILDFLAG(IS_FUCHSIA)',
    'extension': 'VK_FUCHSIA_EXTERNAL_SEMAPHORE_EXTENSION_NAME',
    'functions': [
      'vkImportSemaphoreZirconHandleFUCHSIA',
      'vkGetSemaphoreZirconHandleFUCHSIA',
    ]
  },
  {
    'ifdef': 'BUILDFLAG(IS_FUCHSIA)',
    'extension': 'VK_FUCHSIA_EXTERNAL_MEMORY_EXTENSION_NAME',
    'functions': [
      'vkGetMemoryZirconHandleFUCHSIA',
    ]
  },
  {
    'ifdef': 'BUILDFLAG(IS_FUCHSIA)',
    'extension': 'VK_FUCHSIA_BUFFER_COLLECTION_EXTENSION_NAME',
    'functions': [
      'vkCreateBufferCollectionFUCHSIA',
      'vkSetBufferCollectionImageConstraintsFUCHSIA',
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
  },
  {
    'ifdef': 'BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)',
    'extension': 'VK_EXT_IMAGE_DRM_FORMAT_MODIFIER_EXTENSION_NAME',
    'functions': [
      'vkGetImageDrmFormatModifierPropertiesEXT',
    ]
  }
]

SELF_LOCATION = os.path.dirname(os.path.abspath(__file__))

LICENSE_AND_HEADER = """\
// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file is auto-generated from
// gpu/vulkan/generate_bindings.py
// It's formatted by clang-format using chromium coding style:
//    clang-format -i -style=chromium filename
// DO NOT EDIT!

"""

def WriteReset(out_file, functions):
 for group in functions:
    if 'ifdef' in group:
      out_file.write('#if %s\n' % group['ifdef'])

    for func in group['functions']:
      out_file.write('%s = nullptr;\n' % func)

    if 'ifdef' in group:
      out_file.write('#endif  // %s\n' % group['ifdef'])
    out_file.write('\n')

def WriteFunctionsInternal(out_file, functions, gen_content,
                           check_extension=False):
  for group in functions:
    if 'ifdef' in group:
      out_file.write('#if %s\n' % group['ifdef'])

    extension = group['extension'] if 'extension' in group else ''
    min_api_version = \
        group['min_api_version'] if 'min_api_version' in group else ''

    if not check_extension:
      for func in group['functions']:
        out_file.write(gen_content(func))
    elif not extension and not min_api_version:
      for func in group['functions']:
        out_file.write(gen_content(func))
    else:
      if min_api_version:
        out_file.write('  if (api_version >= %s) {\n' % min_api_version)

        for func in group['functions']:
          out_file.write(
              gen_content(func))

        out_file.write('}\n')
        if extension:
          out_file.write('else ')

      if extension:
        out_file.write('if (gfx::HasExtension(enabled_extensions, %s)) {\n' %
                   extension)

        extension_suffix = \
            group['extension_suffix'] if 'extension_suffix' in group \
            else ''
        for func in group['functions']:
          out_file.write(gen_content(func, extension_suffix))

        out_file.write('}\n')
    if 'ifdef' in group:
      out_file.write('#endif  // %s\n' % group['ifdef'])
    out_file.write('\n')

def WriteFunctions(out_file, functions, template, check_extension=False):
  def gen_content(func, suffix=''):
    return template.substitute({'name': func,'extension_suffix': suffix})
  WriteFunctionsInternal(out_file, functions, gen_content, check_extension)

def WriteFunctionDeclarations(out_file, functions):
  template = Template('  VulkanFunction<PFN_${name}> ${name};\n')
  WriteFunctions(out_file, functions, template)

def WriteMacros(out_file, functions):
  def gen_content(func, suffix=''):
    if func not in registry.cmddict:
      # Some fuchsia functions are not in the vulkan registry, so use macro for
      # them.
      template = Template(
          '#define $name gpu::GetVulkanFunctionPointers()->${name}\n')
      return  template.substitute({'name': func, 'extension_suffix' : suffix})
    none_str = lambda s: s if s else ''
    cmd = registry.cmddict[func].elem
    proto = cmd.find('proto')
    params = cmd.findall('param')
    pdecl = none_str(proto.text)
    for elem in proto:
      text = none_str(elem.text)
      tail = none_str(elem.tail)
      pdecl += text + tail
    n = len(params)

    callstat = ''
    if func in ('vkQueueSubmit', 'vkQueueWaitIdle', 'vkQueuePresentKHR'):
        callstat = 'base::Lock* lock = nullptr;\n'
        callstat += '''auto it = gpu::GetVulkanFunctionPointers()->
        per_queue_lock_map.find(queue);\n'''
        callstat += '''if (it != gpu::GetVulkanFunctionPointers()->
        per_queue_lock_map.end()) {\n'''
        callstat += '\tlock = it->second.get();\n'
        callstat += '}\n'
        callstat += 'base::AutoLockMaybe auto_lock(lock);\n'

    callstat += 'return gpu::GetVulkanFunctionPointers()->%s(' % func
    paramdecl = '('
    if n > 0:
      paramnames = (''.join(t for t in p.itertext())
                    for p in params)
      paramdecl += ', '.join(paramnames)
      paramnames = (''.join(p[1].text)
                    for p in params)
      callstat += ', '.join(paramnames)
    else:
        paramdecl += 'void'
    paramdecl += ')'
    callstat += ')'
    pdecl += paramdecl
    return 'ALWAYS_INLINE %s { %s; }\n' % (pdecl, callstat)

  WriteFunctionsInternal(out_file, functions, gen_content)

def GenerateHeaderFile(out_file):
  """Generates gpu/vulkan/vulkan_function_pointers.h"""

  out_file.write(LICENSE_AND_HEADER +
"""

#ifndef GPU_VULKAN_VULKAN_FUNCTION_POINTERS_H_
#define GPU_VULKAN_VULKAN_FUNCTION_POINTERS_H_

#include <vulkan/vulkan.h>

#include <memory>

#include "base/compiler_specific.h"
#include "base/component_export.h"
#include "base/containers/flat_map.h"
#include "base/native_library.h"
#include "base/synchronization/lock.h"
#include "build/build_config.h"
#include "ui/gfx/extension_set.h"

#if BUILDFLAG(IS_ANDROID)
#include <vulkan/vulkan_android.h>
#endif

#if BUILDFLAG(IS_FUCHSIA)
#include <zircon/types.h>
// <vulkan/vulkan_fuchsia.h> must be included after <zircon/types.h>
#include <vulkan/vulkan_fuchsia.h>

#include "gpu/vulkan/fuchsia/vulkan_fuchsia_ext.h"
#endif

#if defined(USE_VULKAN_XCB)
#include <xcb/xcb.h>
// <vulkan/vulkan_xcb.h> must be included after <xcb/xcb.h>
#include <vulkan/vulkan_xcb.h>
#endif

#if BUILDFLAG(IS_WIN)
#include <vulkan/vulkan_win32.h>
#endif

namespace gpu {

struct VulkanFunctionPointers;

constexpr uint32_t kVulkanRequiredApiVersion = %s;

COMPONENT_EXPORT(VULKAN) VulkanFunctionPointers* GetVulkanFunctionPointers();

struct COMPONENT_EXPORT(VULKAN) VulkanFunctionPointers {
  VulkanFunctionPointers();
  ~VulkanFunctionPointers();

  bool BindUnassociatedFunctionPointersFromLoaderLib(base::NativeLibrary lib);
  bool BindUnassociatedFunctionPointersFromGetProcAddr(
      PFN_vkGetInstanceProcAddr proc);

  // These functions assume that vkGetInstanceProcAddr has been populated.
  bool BindInstanceFunctionPointers(
      VkInstance vk_instance,
      uint32_t api_version,
      const gfx::ExtensionSet& enabled_extensions);

  // These functions assume that vkGetDeviceProcAddr has been populated.
  bool BindDeviceFunctionPointers(
      VkDevice vk_device,
      uint32_t api_version,
      const gfx::ExtensionSet& enabled_extensions);

  void ResetForTesting();

  // This is used to allow thread safe access to a given vulkan queue when
  // multiple gpu threads are accessing it. Note that this map will be only
  // accessed by multiple gpu threads concurrently to read the data, so it
  // should be thread safe to use this map by multiple threads.
  base::flat_map<VkQueue, std::unique_ptr<base::Lock>> per_queue_lock_map;

  template<typename T>
  class VulkanFunction;
  template <typename R, typename ...Args>
  class VulkanFunction <R(VKAPI_PTR*)(Args...)> {
   public:
    using Fn = R(VKAPI_PTR*)(Args...);

    explicit operator bool() const {
      return !!fn_;
    }

    NO_SANITIZE("cfi-icall")
    R operator()(Args... args) const {
      return fn_(args...);
    }

    Fn get() const { return fn_; }

    void OverrideForTesting(Fn fn) { fn_ = fn; }

   private:
    friend VulkanFunctionPointers;

    Fn operator=(Fn fn) {
      fn_ = fn;
      return fn_;
    }

    Fn fn_ = nullptr;
  };

  // Unassociated functions
  VulkanFunction<PFN_vkGetInstanceProcAddr> vkGetInstanceProcAddr;

""" % VULKAN_REQUIRED_API_VERSION)

  WriteFunctionDeclarations(out_file, VULKAN_UNASSOCIATED_FUNCTIONS)

  out_file.write("""\

  // Instance functions
""")

  WriteFunctionDeclarations(out_file, VULKAN_INSTANCE_FUNCTIONS);

  out_file.write("""\

  // Device functions
""")

  WriteFunctionDeclarations(out_file, VULKAN_DEVICE_FUNCTIONS)

  out_file.write("""\

 private:
  bool BindUnassociatedFunctionPointersCommon();
  // The `Bind*` functions will acquires lock, so should not be called with
  // with this lock held. Code that writes to members directly should take this
  // lock as well.
  base::Lock write_lock_;

  base::NativeLibrary loader_library_ = nullptr;
};

}  // namespace gpu

// Unassociated functions
""")

  WriteMacros(out_file, [{'functions': [ 'vkGetInstanceProcAddr']}])
  WriteMacros(out_file, VULKAN_UNASSOCIATED_FUNCTIONS)

  out_file.write("""\

// Instance functions
""")

  WriteMacros(out_file, VULKAN_INSTANCE_FUNCTIONS);

  out_file.write("""\

// Device functions
""")

  WriteMacros(out_file, VULKAN_DEVICE_FUNCTIONS)

  out_file.write("""\

#endif  // GPU_VULKAN_VULKAN_FUNCTION_POINTERS_H_""")

def WriteFunctionPointerInitialization(out_file, proc_addr_function, parent,
                                       functions):
  template = Template("""  constexpr char k${name}${extension_suffix}[] =
    "${name}${extension_suffix}";
  ${name} = reinterpret_cast<PFN_${name}>(
    ${get_proc_addr}(${parent}, k${name}${extension_suffix}));
  if (!${name}) {
    LogGetProcError(k${name}${extension_suffix});
    return false;
  }

""")

  # Substitute all values in the template, except name, which is processed in
  # WriteFunctions().
  template = Template(template.substitute({
        'name': '${name}', 'extension_suffix': '${extension_suffix}',
        'get_proc_addr': proc_addr_function, 'parent': parent}))

  WriteFunctions(out_file, functions, template, check_extension=True)

def WriteUnassociatedFunctionPointerInitialization(out_file, functions):
  WriteFunctionPointerInitialization(out_file, 'vkGetInstanceProcAddr',
                                     'nullptr', functions)

def WriteInstanceFunctionPointerInitialization(out_file, functions):
  WriteFunctionPointerInitialization(out_file, 'vkGetInstanceProcAddr',
                                     'vk_instance', functions)

def WriteDeviceFunctionPointerInitialization(out_file, functions):
  WriteFunctionPointerInitialization(out_file, 'vkGetDeviceProcAddr',
                                     'vk_device', functions)

def GenerateSourceFile(out_file):
  """Generates gpu/vulkan/vulkan_function_pointers.cc"""

  out_file.write(LICENSE_AND_HEADER +
"""

#include "gpu/vulkan/vulkan_function_pointers.h"

#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/no_destructor.h"

namespace gpu {

namespace {
NOINLINE void LogGetProcError(const char* funcName) {
  LOG(WARNING) << "Failed to bind vulkan entrypoint: " << funcName;
}
}

VulkanFunctionPointers* GetVulkanFunctionPointers() {
  static base::NoDestructor<VulkanFunctionPointers> vulkan_function_pointers;
  return vulkan_function_pointers.get();
}

VulkanFunctionPointers::VulkanFunctionPointers() = default;
VulkanFunctionPointers::~VulkanFunctionPointers() = default;

bool VulkanFunctionPointers::BindUnassociatedFunctionPointersFromLoaderLib(
    base::NativeLibrary lib) {
  base::AutoLock lock(write_lock_);
  loader_library_ = lib;

  // vkGetInstanceProcAddr must be handled specially since it gets its
  // function pointer through base::GetFunctionPointerFromNativeLibrary().
  // Other Vulkan functions don't do this.
  vkGetInstanceProcAddr = reinterpret_cast<PFN_vkGetInstanceProcAddr>(
      base::GetFunctionPointerFromNativeLibrary(loader_library_,
                                                "vkGetInstanceProcAddr"));
  if (!vkGetInstanceProcAddr) {
    LOG(WARNING) << "Failed to find vkGetInstanceProcAddr";
    return false;
  }
  return BindUnassociatedFunctionPointersCommon();
}

bool VulkanFunctionPointers::BindUnassociatedFunctionPointersFromGetProcAddr(
  PFN_vkGetInstanceProcAddr proc) {
  DCHECK(proc);
  DCHECK(!loader_library_);

  base::AutoLock lock(write_lock_);
  vkGetInstanceProcAddr = proc;
  return BindUnassociatedFunctionPointersCommon();
}

bool VulkanFunctionPointers::BindUnassociatedFunctionPointersCommon() {
""")

  WriteUnassociatedFunctionPointerInitialization(
      out_file, VULKAN_UNASSOCIATED_FUNCTIONS)

  out_file.write("""\

  return true;
}

bool VulkanFunctionPointers::BindInstanceFunctionPointers(
    VkInstance vk_instance,
    uint32_t api_version,
    const gfx::ExtensionSet& enabled_extensions) {
  DCHECK_GE(api_version, kVulkanRequiredApiVersion);
  base::AutoLock lock(write_lock_);
""")

  WriteInstanceFunctionPointerInitialization(
      out_file, VULKAN_INSTANCE_FUNCTIONS);

  out_file.write("""\

  return true;
}

bool VulkanFunctionPointers::BindDeviceFunctionPointers(
    VkDevice vk_device,
    uint32_t api_version,
    const gfx::ExtensionSet& enabled_extensions) {
  DCHECK_GE(api_version, kVulkanRequiredApiVersion);
  base::AutoLock lock(write_lock_);
  // Device functions
""")
  WriteDeviceFunctionPointerInitialization(out_file, VULKAN_DEVICE_FUNCTIONS)

  out_file.write("""\

  return true;
}

void VulkanFunctionPointers::ResetForTesting() {
  base::AutoLock lock(write_lock_);

  per_queue_lock_map.clear();
  loader_library_ = nullptr;
  vkGetInstanceProcAddr = nullptr;

""")

  WriteReset(
      out_file, VULKAN_UNASSOCIATED_FUNCTIONS)
  WriteReset(
      out_file, VULKAN_INSTANCE_FUNCTIONS)
  WriteReset(
      out_file, VULKAN_DEVICE_FUNCTIONS)

  out_file.write("""\
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
      os.path.join(output_dir, header_file_name), 'w', newline='\n')
  GenerateHeaderFile(header_file)
  header_file.close()
  ClangFormat(header_file.name)

  source_file_name = 'vulkan_function_pointers.cc'
  source_file = open(
      os.path.join(output_dir, source_file_name), 'w', newline='\n')
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
    print('Please run gpu/vulkan/generate_bindings.py')
    print('Failed check on generated files:')
    for filename in check_failed_filenames:
      print(filename)
    return 1

  return 0

if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
