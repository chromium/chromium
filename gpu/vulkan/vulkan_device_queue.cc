// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "gpu/vulkan/vulkan_device_queue.h"

#include <bit>
#include <cstring>
#include <unordered_set>
#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/logging.h"
#include "base/ranges/algorithm.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/process_memory_dump.h"
#include "build/build_config.h"
#include "gpu/config/gpu_info.h"  // nogncheck
#include "gpu/config/vulkan_info.h"
#include "gpu/vulkan/vulkan_command_pool.h"
#include "gpu/vulkan/vulkan_crash_keys.h"
#include "gpu/vulkan/vulkan_fence_helper.h"
#include "gpu/vulkan/vulkan_function_pointers.h"
#include "gpu/vulkan/vulkan_util.h"
#include "ui/gl/gl_angle_util_vulkan.h"

namespace features {
// Based on Finch experiment results, the VMA block size does not significantly
// affect performance.  Too small sizes (such as 4KB) result in instability,
// likely due to running out of allowed allocations (the
// |maxMemoryAllocationCount| Vulkan limit).  Too large sizes (such as 4MB)
// result in significant memory waste due to fragmentation.  Finch results
// have shown that with a block size of 64KB and below, the amount of
// fragmentation is ~1MB in the 99th percentile.  For 128KB and higher block
// sizes, the amount of fragmentation exponentially increases (with 2MB for
// 128KB block size, 4MB for 256KB, etc).
BASE_FEATURE(kVulkanVMALargeHeapBlockSizeExperiment,
             "VulkanVMALargeHeapBlockSizeExperiment",
             base::FEATURE_ENABLED_BY_DEFAULT);
constexpr base::FeatureParam<int> kVulkanVMALargeHeapBlockSize{
    &kVulkanVMALargeHeapBlockSizeExperiment, "VulkanVMALargeHeapBlockSize",
    64 * 1024};
}  // namespace features

namespace gpu {
namespace {
VkDeviceSize GetPreferredVMALargeHeapBlockSize() {
  const VkDeviceSize block_size =
      ::features::kVulkanVMALargeHeapBlockSize.Get();
  DCHECK(std::has_single_bit(block_size));
  return block_size;
}

#if BUILDFLAG(IS_ANDROID)
class VulkanMetric final
    : public base::android::PreFreezeBackgroundMemoryTrimmer::PreFreezeMetric {
 public:
  explicit VulkanMetric(VmaAllocator vma_allocator)
      : PreFreezeMetric("Vulkan"), vma_allocator_(vma_allocator) {
    base::android::PreFreezeBackgroundMemoryTrimmer::RegisterMemoryMetric(this);
  }

  ~VulkanMetric() override {
    base::android::PreFreezeBackgroundMemoryTrimmer::UnregisterMemoryMetric(
        this);
  }

 private:
  std::optional<uint64_t> Measure() const override {
    auto allocated_used = vma::GetTotalAllocatedAndUsedMemory(vma_allocator_);
    return allocated_used.first;
  }
  VmaAllocator vma_allocator_;
};
#endif  // BUILDFLAG(IS_ANDROID)

}  // anonymous namespace

VulkanDeviceQueue::VulkanDeviceQueue(VkInstance vk_instance)
    : vk_instance_(vk_instance) {}

VulkanDeviceQueue::VulkanDeviceQueue(VulkanInstance* instance)
    : vk_instance_(instance->vk_instance()), instance_(instance) {}

VulkanDeviceQueue::~VulkanDeviceQueue() {
  // Destroy() should have been called.
  DCHECK_EQ(static_cast<VkPhysicalDevice>(VK_NULL_HANDLE), vk_physical_device_);
  DCHECK_EQ(static_cast<VkDevice>(VK_NULL_HANDLE), vk_device_);
  DCHECK_EQ(static_cast<VkQueue>(VK_NULL_HANDLE), vk_queue_);
}

bool VulkanDeviceQueue::Initialize(
    uint32_t options,
    const GPUInfo* gpu_info,
    const std::vector<const char*>& required_extensions,
    const std::vector<const char*>& optional_extensions,
    bool allow_protected_memory,
    const GetPresentationSupportCallback& get_presentation_support,
    uint32_t heap_memory_limit,
    const bool is_thread_safe) {
  DCHECK_EQ(static_cast<VkPhysicalDevice>(VK_NULL_HANDLE), vk_physical_device_);
  DCHECK_EQ(static_cast<VkDevice>(VK_NULL_HANDLE), owned_vk_device_);
  DCHECK_EQ(static_cast<VkDevice>(VK_NULL_HANDLE), vk_device_);
  DCHECK_EQ(static_cast<VkQueue>(VK_NULL_HANDLE), vk_queue_);
  DCHECK_EQ(static_cast<VmaAllocator>(VK_NULL_HANDLE), owned_vma_allocator_);
  DCHECK_EQ(static_cast<VmaAllocator>(VK_NULL_HANDLE), vma_allocator_);

  if (VK_NULL_HANDLE == vk_instance_)
    return false;

  const VulkanInfo& info = instance_->vulkan_info();

  VkResult result = VK_SUCCESS;

  VkQueueFlags queue_flags = 0;
  if (options & DeviceQueueOption::GRAPHICS_QUEUE_FLAG) {
    queue_flags |= VK_QUEUE_GRAPHICS_BIT;
  }
  if (allow_protected_memory) {
    queue_flags |= VK_QUEUE_PROTECTED_BIT;
  }

  // We prefer to use discrete GPU, integrated GPU is the second, and then
  // others.
  static constexpr int kDeviceTypeScores[] = {
      0,  // VK_PHYSICAL_DEVICE_TYPE_OTHER
      3,  // VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU
      4,  // VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU
      2,  // VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU
      1,  // VK_PHYSICAL_DEVICE_TYPE_CPU
  };
  static_assert(VK_PHYSICAL_DEVICE_TYPE_OTHER == 0, "");
  static_assert(VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU == 1, "");
  static_assert(VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU == 2, "");
  static_assert(VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU == 3, "");
  static_assert(VK_PHYSICAL_DEVICE_TYPE_CPU == 4, "");

  int device_index = -1;
  int queue_index = -1;
  int device_score = -1;
  for (size_t i = 0; i < info.physical_devices.size(); ++i) {
    const auto& device_info = info.physical_devices[i];
    const auto& device_properties = device_info.properties;
    if (device_properties.apiVersion < info.used_api_version)
      continue;

      // In dual-CPU cases, we cannot detect the active GPU correctly on Linux,
      // so don't select GPU device based on the |gpu_info|.
#if !BUILDFLAG(IS_LINUX)
    // If gpu_info is provided, the device should match it.
    if (gpu_info && (device_properties.vendorID != gpu_info->gpu.vendor_id ||
                     device_properties.deviceID != gpu_info->gpu.device_id)) {
      continue;
    }
#endif

    if (device_properties.deviceType < 0 ||
        device_properties.deviceType > VK_PHYSICAL_DEVICE_TYPE_CPU) {
      DLOG(ERROR) << "Unsupported device type: "
                  << device_properties.deviceType;
      continue;
    }

    const VkPhysicalDevice& device = device_info.device;
    bool found = false;
    for (size_t n = 0; n < device_info.queue_families.size(); ++n) {
      if ((device_info.queue_families[n].queueFlags & queue_flags) !=
          queue_flags) {
        continue;
      }

      if (options & DeviceQueueOption::PRESENTATION_SUPPORT_QUEUE_FLAG &&
          !get_presentation_support.Run(device, device_info.queue_families,
                                        n)) {
        continue;
      }

      if (kDeviceTypeScores[device_properties.deviceType] > device_score) {
        device_index = i;
        queue_index = static_cast<int>(n);
        device_score = kDeviceTypeScores[device_properties.deviceType];
        found = true;
        break;
      }
    }

    if (!found)
      continue;

    // Use the device, if it matches gpu_info.
    if (gpu_info)
      break;

    // If the device is a discrete GPU, we will use it. Otherwise go through
    // all the devices and find the device with the highest score.
    if (device_properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
      break;
  }

  if (device_index == -1) {
    DLOG(ERROR) << "Cannot find capable device.";
    return false;
  }

  const auto& physical_device_info = info.physical_devices[device_index];
  vk_physical_device_ = physical_device_info.device;
  vk_physical_device_properties_ = physical_device_info.properties;
  vk_physical_device_driver_properties_ =
      physical_device_info.driver_properties;
  drm_device_id_ = physical_device_info.drm_device_id;
  vk_queue_index_ = queue_index;

  float queue_priority = 0.0f;
  VkDeviceQueueCreateInfo queue_create_info = {};
  queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
  queue_create_info.queueFamilyIndex = queue_index;
  queue_create_info.queueCount = 1;
  queue_create_info.pQueuePriorities = &queue_priority;
  queue_create_info.flags =
      allow_protected_memory ? VK_DEVICE_QUEUE_CREATE_PROTECTED_BIT : 0;

  std::vector<const char*> enabled_extensions;
  for (const char* extension : required_extensions) {
    if (base::ranges::none_of(physical_device_info.extensions,
                              [extension](const VkExtensionProperties& p) {
                                return std::strcmp(extension,
                                                   p.extensionName) == 0;
                              })) {
      // On Fuchsia, some device extensions are provided by layers.
      // TODO(penghuang): checking extensions against layer device extensions
      // too.
#if !BUILDFLAG(IS_FUCHSIA)
      DLOG(ERROR) << "Required Vulkan extension " << extension
                  << " is not supported.";
      return false;
#endif
    }
    enabled_extensions.push_back(extension);
  }

  for (const char* extension : optional_extensions) {
    if (base::ranges::none_of(physical_device_info.extensions,
                              [extension](const VkExtensionProperties& p) {
                                return std::strcmp(extension,
                                                   p.extensionName) == 0;
                              })) {
      DLOG(ERROR) << "Optional Vulkan extension " << extension
                  << " is not supported.";
    } else {
      enabled_extensions.push_back(extension);
    }
  }

  crash_keys::vulkan_device_api_version.Set(
      VkVersionToString(vk_physical_device_properties_.apiVersion));
  if (vk_physical_device_properties_.vendorID == 0x10DE) {
    // NVIDIA
    // 10 bits = major version (up to r1023)
    // 8 bits = minor version (up to 255)
    // 8 bits = secondary branch version/build version (up to 255)
    // 6 bits = tertiary branch/build version (up to 63)
    auto version = vk_physical_device_properties_.driverVersion;
    uint32_t major = (version >> 22) & 0x3ff;
    uint32_t minor = (version >> 14) & 0x0ff;
    uint32_t secondary_branch = (version >> 6) & 0x0ff;
    uint32_t tertiary_branch = version & 0x003f;
    crash_keys::vulkan_device_driver_version.Set(base::StringPrintf(
        "%d.%d.%d.%d", major, minor, secondary_branch, tertiary_branch));
  } else {
    crash_keys::vulkan_device_driver_version.Set(
        VkVersionToString(vk_physical_device_properties_.driverVersion));
  }
  crash_keys::vulkan_device_vendor_id.Set(
      base::StringPrintf("0x%04x", vk_physical_device_properties_.vendorID));
  crash_keys::vulkan_device_id.Set(
      base::StringPrintf("0x%04x", vk_physical_device_properties_.deviceID));
  static const char* kDeviceTypeNames[] = {
      "other", "integrated", "discrete", "virtual", "cpu",
  };
  uint32_t gpu_type = vk_physical_device_properties_.deviceType;
  if (gpu_type >= std::size(kDeviceTypeNames))
    gpu_type = 0;
  crash_keys::vulkan_device_type.Set(kDeviceTypeNames[gpu_type]);
  crash_keys::vulkan_device_name.Set(vk_physical_device_properties_.deviceName);

  // Disable all physical device features by default.
  enabled_device_features_2_ = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};

  // Android, Fuchsia, Linux, and CrOS (VaapiVideoDecoder) need YCbCr sampler
  // support.
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_FUCHSIA) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
  if (!physical_device_info.feature_sampler_ycbcr_conversion) {
    LOG(ERROR) << "samplerYcbcrConversion is not supported.";
    return false;
  }
  sampler_ycbcr_conversion_features_ = {
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SAMPLER_YCBCR_CONVERSION_FEATURES};
  sampler_ycbcr_conversion_features_.samplerYcbcrConversion = VK_TRUE;

  // Add VkPhysicalDeviceSamplerYcbcrConversionFeatures struct to pNext chain
  // of VkPhysicalDeviceFeatures2 to enable YCbCr sampler support.
  sampler_ycbcr_conversion_features_.pNext = enabled_device_features_2_.pNext;
  enabled_device_features_2_.pNext = &sampler_ycbcr_conversion_features_;
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_FUCHSIA) || BUILDFLAG(IS_LINUX)
        // || BUILDFLAG(IS_CHROMEOS)

  if (allow_protected_memory) {
    if (!physical_device_info.feature_protected_memory) {
      LOG(DFATAL)
          << "Protected memory is not supported. Vulkan is unavailable.";
      return false;
    }
    protected_memory_features_ = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROTECTED_MEMORY_FEATURES};
    protected_memory_features_.protectedMemory = VK_TRUE;

    // Add VkPhysicalDeviceProtectedMemoryFeatures struct to pNext chain
    // of VkPhysicalDeviceFeatures2 to enable YCbCr sampler support.
    protected_memory_features_.pNext = enabled_device_features_2_.pNext;
    enabled_device_features_2_.pNext = &protected_memory_features_;
  }

  VkDeviceCreateInfo device_create_info = {
      VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
  device_create_info.pNext = enabled_device_features_2_.pNext;
  device_create_info.queueCreateInfoCount = 1;
  device_create_info.pQueueCreateInfos = &queue_create_info;
  device_create_info.enabledExtensionCount = enabled_extensions.size();
  device_create_info.ppEnabledExtensionNames = enabled_extensions.data();
  device_create_info.pEnabledFeatures = &enabled_device_features_2_.features;

  result = vkCreateDevice(vk_physical_device_, &device_create_info, nullptr,
                          &owned_vk_device_);
  if (VK_SUCCESS != result) {
    DLOG(ERROR) << "vkCreateDevice failed. result:" << result;
    return false;
  }

  enabled_extensions_ = gfx::ExtensionSet(std::begin(enabled_extensions),
                                          std::end(enabled_extensions));

  if (!gpu::GetVulkanFunctionPointers()->BindDeviceFunctionPointers(
          owned_vk_device_, info.used_api_version, enabled_extensions_)) {
    vkDestroyDevice(owned_vk_device_, nullptr);
    owned_vk_device_ = VK_NULL_HANDLE;
    return false;
  }

  vk_device_ = owned_vk_device_;

  if (allow_protected_memory) {
    VkDeviceQueueInfo2 queue_info2 = {};
    queue_info2.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_INFO_2;
    queue_info2.flags = VK_DEVICE_QUEUE_CREATE_PROTECTED_BIT;
    queue_info2.queueFamilyIndex = queue_index;
    queue_info2.queueIndex = 0;
    vkGetDeviceQueue2(vk_device_, &queue_info2, &vk_queue_);
  } else {
    vkGetDeviceQueue(vk_device_, queue_index, 0, &vk_queue_);
  }

  std::vector<VkDeviceSize> heap_size_limit(
      VK_MAX_MEMORY_HEAPS,
      heap_memory_limit ? heap_memory_limit : VK_WHOLE_SIZE);
  vma::CreateAllocator(vk_physical_device_, vk_device_, vk_instance_,
                       enabled_extensions_, GetPreferredVMALargeHeapBlockSize(),
                       heap_size_limit.data(), is_thread_safe,
                       &owned_vma_allocator_);
  vma_allocator_ = owned_vma_allocator_;

  cleanup_helper_ = std::make_unique<VulkanFenceHelper>(this);

  allow_protected_memory_ = allow_protected_memory;

#if BUILDFLAG(IS_ANDROID)
  if (!metric_) {
    metric_ = std::make_unique<VulkanMetric>(vma_allocator());
  }
#endif  // BUILDFLAG(IS_ANDROID)

  if (base::SingleThreadTaskRunner::HasCurrentDefault()) {
    base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
        this, "vulkan", base::SingleThreadTaskRunner::GetCurrentDefault());
  }

  return true;
}

bool VulkanDeviceQueue::InitCommon(VkPhysicalDevice vk_physical_device,
                                   VkDevice vk_device,
                                   VkQueue vk_queue,
                                   uint32_t vk_queue_index,
                                   gfx::ExtensionSet enabled_extensions) {
  DCHECK_EQ(static_cast<VkPhysicalDevice>(VK_NULL_HANDLE), vk_physical_device_);
  DCHECK_EQ(static_cast<VkDevice>(VK_NULL_HANDLE), owned_vk_device_);
  DCHECK_EQ(static_cast<VkDevice>(VK_NULL_HANDLE), vk_device_);
  DCHECK_EQ(static_cast<VkQueue>(VK_NULL_HANDLE), vk_queue_);
  DCHECK_EQ(static_cast<VmaAllocator>(VK_NULL_HANDLE), owned_vma_allocator_);

  vk_physical_device_ = vk_physical_device;
  vk_device_ = vk_device;
  vk_queue_ = vk_queue;
  vk_queue_index_ = vk_queue_index;
  enabled_extensions_ = std::move(enabled_extensions);

  if (vma_allocator_ == VK_NULL_HANDLE) {
    vma::CreateAllocator(vk_physical_device_, vk_device_, vk_instance_,
                         enabled_extensions_,
                         GetPreferredVMALargeHeapBlockSize(),
                         /*heap_size_limit=*/nullptr,
                         /*is_thread_safe =*/false, &owned_vma_allocator_);
    vma_allocator_ = owned_vma_allocator_;
#if BUILDFLAG(IS_ANDROID)
    if (!metric_) {
      metric_ = std::make_unique<VulkanMetric>(vma_allocator());
    }
#endif  // BUILDFLAG(IS_ANDROID)
  }

  cleanup_helper_ = std::make_unique<VulkanFenceHelper>(this);

  if (base::SingleThreadTaskRunner::HasCurrentDefault()) {
    base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
        this, "vulkan", base::SingleThreadTaskRunner::GetCurrentDefault());
  }
  return true;
}

bool VulkanDeviceQueue::InitializeFromANGLE() {
  const VulkanInfo& info = instance_->vulkan_info();
  VkPhysicalDevice vk_physical_device = gl::QueryVkPhysicalDeviceFromANGLE();
  if (vk_physical_device == VK_NULL_HANDLE)
    return false;

  int device_index = -1;
  for (size_t i = 0; i < info.physical_devices.size(); ++i) {
    if (info.physical_devices[i].device == vk_physical_device) {
      device_index = i;
      break;
    }
  }

  if (device_index == -1) {
    DLOG(ERROR) << "Cannot find physical device match ANGLE.";
    return false;
  }

  const auto& physical_device_info = info.physical_devices[device_index];
  vk_physical_device_properties_ = physical_device_info.properties;
  vk_physical_device_driver_properties_ =
      physical_device_info.driver_properties;

  VkDevice vk_device = gl::QueryVkDeviceFromANGLE();
  VkQueue vk_queue = gl::QueryVkQueueFromANGLE();
  uint32_t vk_queue_index = gl::QueryVkQueueFramiliyIndexFromANGLE();
  auto enabled_extensions = gl::QueryVkDeviceExtensionsFromANGLE();

  if (!gpu::GetVulkanFunctionPointers()->BindDeviceFunctionPointers(
          vk_device, info.used_api_version, enabled_extensions)) {
    return false;
  }

  enabled_device_features_2_from_angle_ =
      gl::QueryVkEnabledDeviceFeaturesFromANGLE();
  if (!enabled_device_features_2_from_angle_)
    return false;

  return InitCommon(vk_physical_device, vk_device, vk_queue, vk_queue_index,
                    enabled_extensions);
}

bool VulkanDeviceQueue::InitializeForWebView(
    VkPhysicalDevice vk_physical_device,
    VkDevice vk_device,
    VkQueue vk_queue,
    uint32_t vk_queue_index,
    gfx::ExtensionSet enabled_extensions) {
  return InitCommon(vk_physical_device, vk_device, vk_queue, vk_queue_index,
                    enabled_extensions);
}

bool VulkanDeviceQueue::InitializeForCompositorGpuThread(
    VkPhysicalDevice vk_physical_device,
    VkDevice vk_device,
    VkQueue vk_queue,
    uint32_t vk_queue_index,
    gfx::ExtensionSet enabled_extensions,
    const VkPhysicalDeviceFeatures2& vk_physical_device_features2,
    VmaAllocator vma_allocator) {
  // Currently VulkanDeviceQueue for drdc thread(aka CompositorGpuThread) uses
  // the same vulkan queue as the gpu main thread. Now since both gpu main and
  // drdc threads would be accessing/submitting work to the same queue, all the
  // queue access should be made thread safe. This is done by using locks. This
  // lock is per |vk_queue|. Note that we are intentionally overwriting a
  // previous lock if any.
  // Since the map itself would be accessed by multiple gpu threads, we need to
  // ensure that the access are thread safe. Here the locks are created and
  // written into the map only when drdc thread is initialized which happens
  // during GpuServiceImpl init. At this point none of the gpu threads would be
  // doing read access until GpuServiceImpl init completed. Hence its safe to
  // access map here.
  GetVulkanFunctionPointers()->per_queue_lock_map[vk_queue] =
      std::make_unique<base::Lock>();
  enabled_device_features_2_ = vk_physical_device_features2;

  // Note that CompositorGpuThread uses same vma allocator as gpu main thread.
  vma_allocator_ = vma_allocator;
  return InitCommon(vk_physical_device, vk_device, vk_queue, vk_queue_index,
                    enabled_extensions);
}

void VulkanDeviceQueue::Destroy() {
  base::trace_event::MemoryDumpManager::GetInstance()->UnregisterDumpProvider(
      this);
#if BUILDFLAG(IS_ANDROID)
  metric_ = nullptr;
#endif

  if (cleanup_helper_) {
    cleanup_helper_->Destroy();
    cleanup_helper_.reset();
  }

  if (owned_vma_allocator_ != VK_NULL_HANDLE) {
    vma::DestroyAllocator(owned_vma_allocator_);
    owned_vma_allocator_ = VK_NULL_HANDLE;
  }

  if (owned_vk_device_ != VK_NULL_HANDLE) {
    vkDestroyDevice(owned_vk_device_, nullptr);
    owned_vk_device_ = VK_NULL_HANDLE;

    // Clear all the entries from this map since the device and hence all the
    // generated queue(and their corresponding lock) from this device is
    // destroyed.
    // This happens when VulkanDeviceQueue is destroyed on gpu main thread
    // during GpuServiceImpl destruction which happens after CompositorGpuThread
    // is destroyed. Hence CompositorGpuThread would not be accessing the map at
    // this point and its thread safe to delete map entries here.
    GetVulkanFunctionPointers()->per_queue_lock_map.clear();
  }
  vk_device_ = VK_NULL_HANDLE;
  vk_queue_ = VK_NULL_HANDLE;
  vk_queue_index_ = 0;
  vk_physical_device_ = VK_NULL_HANDLE;
  vma_allocator_ = VK_NULL_HANDLE;
}

std::unique_ptr<VulkanCommandPool> VulkanDeviceQueue::CreateCommandPool() {
  std::unique_ptr<VulkanCommandPool> command_pool(new VulkanCommandPool(this));
  if (!command_pool->Initialize())
    return nullptr;

  return command_pool;
}

bool VulkanDeviceQueue::OnMemoryDump(
    const base::trace_event::MemoryDumpArgs& args,
    base::trace_event::ProcessMemoryDump* pmd) {
  std::string path =
      base::StringPrintf("gpu/vulkan/vma_allocator_%p", vma_allocator());
  // There are cases where the same VMA is used by several device queues. Make
  // sure to not double count by using the VMA address in the path.
  //
  // This is still a success case, as the other device queue may disappear, so
  // return true.
  if (pmd->GetAllocatorDump(path)) {
    return true;
  }

  auto* dump = pmd->CreateAllocatorDump(path);
  auto allocated_used = vma::GetTotalAllocatedAndUsedMemory(vma_allocator());
  // `allocated_size` is memory allocated from the device, used is what is
  // actually used.
  dump->AddScalar("allocated_size", "bytes", allocated_used.first);
  dump->AddScalar("used_size", "bytes", allocated_used.second);
  dump->AddScalar("fragmentation_size", "bytes",
                  allocated_used.first - allocated_used.second);
  return true;
}

}  // namespace gpu
