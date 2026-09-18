// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_VULKAN_IN_PROCESS_CONTEXT_PROVIDER_H_
#define GPU_COMMAND_BUFFER_SERVICE_VULKAN_IN_PROCESS_CONTEXT_PROVIDER_H_

#include <atomic>
#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory_coordinator/async_memory_consumer_registration.h"
#include "base/memory_coordinator/memory_consumer.h"
#include "base/time/time.h"
#include "gpu/command_buffer/service/vulkan_context_provider.h"
#include "gpu/gpu_gles2_export.h"
#include "gpu/vulkan/buildflags.h"
#include "third_party/skia/include/gpu/ganesh/GrContextOptions.h"

namespace gpu {

class VulkanImplementation;
class VulkanDeviceQueue;
struct GPUInfo;

class GPU_GLES2_EXPORT VulkanInProcessContextProvider
    : public VulkanContextProvider,
      public base::MemoryConsumer {
 public:
  // If |sync_cpu_memory_limit| is set and greater than zero, it is the
  // threshold above which GPU work should be synchronized with the CPU to free
  // memory immediately. |cooldown_duration_at_memory_pressure_critical| is the
  // duration (default 15s to sync with memory monitor cycles) for which a zero
  // limit is applied after a CRITICAL memory pressure signal is received. If
  // the kStatefulMemoryPressure feature is enabled, the limit scales
  // dynamically based on the pressure level instead of using the fixed cooldown
  // period.
  static scoped_refptr<VulkanInProcessContextProvider> Create(
      VulkanImplementation* vulkan_implementation,
      uint32_t heap_memory_limit = 0,
      uint32_t sync_cpu_memory_limit = 0,
      const bool is_thread_safe = false,
      const GPUInfo* gpu_info = nullptr,
      base::TimeDelta cooldown_duration_at_memory_pressure_critical =
          base::Seconds(15));

  // Creates a VulkanContextProvider for the CompositorGpuThread.
  static scoped_refptr<VulkanInProcessContextProvider>
  CreateForCompositorGpuThread(
      VulkanImplementation* vulkan_implementation,
      std::unique_ptr<VulkanDeviceQueue> vulkan_device_queue,
      uint32_t sync_cpu_memory_limit = 0,
      base::TimeDelta cooldown_duration_at_memory_pressure_critical =
          base::Seconds(15));

  VulkanInProcessContextProvider(const VulkanInProcessContextProvider&) =
      delete;
  VulkanInProcessContextProvider& operator=(
      const VulkanInProcessContextProvider&) = delete;

  void Destroy();

  // VulkanContextProvider implementation
  bool InitializeGrContext(const GrContextOptions& context_options) override;
  VulkanImplementation* GetVulkanImplementation() override;
  VulkanDeviceQueue* GetDeviceQueue() override;
  GrDirectContext* GetGrContext() override;
  GrVkSecondaryCBDrawContext* GetGrSecondaryCBDrawContext() override;
  void EnqueueSecondaryCBSemaphores(
      std::vector<VkSemaphore> semaphores) override;
  void EnqueueSecondaryCBPostSubmitTask(base::OnceClosure closure) override;
  std::optional<uint32_t> GetSyncCpuMemoryLimit() const override;

 private:
  friend class VulkanInProcessContextProviderTest;

  VulkanInProcessContextProvider(
      VulkanImplementation* vulkan_implementation,
      uint32_t heap_memory_limit,
      uint32_t sync_cpu_memory_limit,
      base::TimeDelta cooldown_duration_at_memory_pressure_critical);
  ~VulkanInProcessContextProvider() override;

  bool Initialize(const GPUInfo* gpu_info, const bool is_thread_safe = false);

  void InitializeForCompositorGpuThread(
      std::unique_ptr<VulkanDeviceQueue> vulkan_device_queue);

  // base::MemoryConsumer:
  void OnReleaseMemory() override;
  void OnUpdateMemoryLimit() override;

#if BUILDFLAG(ENABLE_VULKAN)
  uint32_t GetCurrentGpuMemoryUsage() const;

  sk_sp<GrDirectContext> gr_context_;
  raw_ptr<VulkanImplementation> vulkan_implementation_;
  std::unique_ptr<VulkanDeviceQueue> device_queue_;
  const uint32_t heap_memory_limit_;
  const std::optional<uint32_t> sync_cpu_memory_limit_;
  const base::TimeDelta cooldown_duration_at_memory_pressure_critical_;
  std::atomic<base::TimeTicks> critical_memory_pressure_expiration_time_;
  std::atomic<uint32_t> active_sync_cpu_memory_limit_;
#endif

  std::unique_ptr<base::AsyncMemoryConsumerRegistration>
      memory_consumer_registration_;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_VULKAN_IN_PROCESS_CONTEXT_PROVIDER_H_
