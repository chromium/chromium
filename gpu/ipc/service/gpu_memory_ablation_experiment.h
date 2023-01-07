// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_SERVICE_GPU_MEMORY_ABLATION_EXPERIMENT_H_
#define GPU_IPC_SERVICE_GPU_MEMORY_ABLATION_EXPERIMENT_H_

#include <memory>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/service/memory_tracking.h"
#include "gpu/ipc/service/gpu_ipc_service_export.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gl/scoped_make_current.h"

namespace base {
class SequencedTaskRunner;
}

namespace gpu {
class GpuChannelManager;
class SharedContextState;
class SharedImageFactory;
class SharedImageRepresentationFactory;

BASE_DECLARE_FEATURE(kGPUMemoryAblationFeature);

// When enabled, this experiment allocates additional memory alongside each
// normal allocation. This will allow a study of the correlation between
// memory usage and performance metrics.
//
// Each increase reported to OnMemoryAllocated will allocate a chunk of memory.
// Each decrease reported will release a previously allocated chunk.
//
// GpuMemoryAblationExperiment acts as the MemoryTracker for all of its own
// allocations. This prevents a cycle of memory allocations:
//    - GpuChannelManager::GpuPeakMemoryMonitor::OnMemoryAllocatedChange
//    - GpuMemoryAblationExperiment::OnMemoryAllocated
//    - MemoryTracker::TrackMemoryAllocatedChange
//    - GpuChannelManager::GpuPeakMemoryMonitor::OnMemoryAllocatedChange
//    - etc.
//
// Instead this will track the memory it allocated, which can be retrieved via
// GetPeakMemory.
class GPU_IPC_SERVICE_EXPORT GpuMemoryAblationExperiment
    : public MemoryTracker {
 public:
  // Checks that the memory ablation experiment feature is enabled. As well as
  // that a supported gl::GLImplementation is available.
  static bool ExperimentSupported();

  GpuMemoryAblationExperiment(
      GpuChannelManager* channel_manager,
      scoped_refptr<base::SequencedTaskRunner> task_runner);
  ~GpuMemoryAblationExperiment() override;

  // Allocates a chunk of memory in response to increases. Reported decreases
  // will release previously allocated chunks. The amount of memory allocated
  // is returned in bytes.
  void OnMemoryAllocated(uint64_t old_size, uint64_t new_size);

  uint64_t GetPeakMemory(uint32_t sequence_num) const;
  void StartSequence(uint32_t sequence_num);
  void StopSequence(uint32_t sequence_num);

 private:
  // The initialization status of the feature. It defaults to |UNINITIALIZED|
  // and is updated upon the success or failure of initializing the needed GPU
  // resources.
  enum class Status {
    UNINITIALIZED,
    ENABLED,
    DISABLED,
  };

  void AllocateGpuMemory();
  void DeleteGpuMemory();

  // Setups the Gpu resources needed to allocate Gpu RAM. These are influenced
  // by SharedImageStub. Which is not used directly as there is no external
  // host to pair a GpuChannel with. Returns true if initialization succeeded.
  bool InitGpu(GpuChannelManager* channel_manager);

  // This must be called before any actions on |factory_|.
  // This provides a ui::ScopedMakeCurrent which will reset the previous
  // context upon deletion. As the we allocate memory in-response to other
  // allocations, we are changing the context in a nested fashion. Several
  // areas of code do not support this, and re-verify the current context at
  // later points. (https://crbug.com/1104316)
  // If this method fails then a nullptr is returned. All subsequent work on
  // the |factory_| would also fail.
  std::unique_ptr<ui::ScopedMakeCurrent> ScopedMakeContextCurrent();

  // MemoryTracker:
  void TrackMemoryAllocatedChange(int64_t delta) override;
  uint64_t GetSize() const override;
  uint64_t ClientTracingId() const override;
  int ClientId() const override;
  uint64_t ContextGroupTracingId() const override;

  // The status of the initialization. Will be updated based on the results of
  // initializing the necessary GPU resources.
  Status init_status_ = Status::UNINITIALIZED;

  // Size of image to allocate, determined by experiment parameters.
  gfx::Size size_;

  // The Mailboxes allocated for each image.
  std::vector<Mailbox> mailboxes_;

  // The memory allocated for ablation is not reported directly to
  // GpuChannelManager::GpuPeakMemoryMonitor, as GpuMemoryAblationExperiment
  // acts as the MemoryTracker for its own allocations. This tracks the peak
  // allocation so that it can be reported.
  base::flat_map<uint32_t /*sequence_num*/, uint64_t /*peak_memory*/>
      sequences_;

  // The memory allocated for ablation is not reported directly to
  // GpuChannelManager::GpuPeakMemoryMonitor, as this class acts as the
  // MemoryTracker for its own allocations. Tracks the current amount of
  //  memory allocated as a part of the ablation.
  uint64_t gpu_allocation_size_ = 0;

  scoped_refptr<SharedContextState> context_state_;
  std::unique_ptr<SharedImageFactory> factory_;
  std::unique_ptr<SharedImageRepresentationFactory> rep_factory_;
  raw_ptr<GpuChannelManager> channel_manager_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  base::WeakPtrFactory<GpuMemoryAblationExperiment> weak_factory_{this};
};

}  // namespace gpu

#endif  // GPU_IPC_SERVICE_GPU_MEMORY_ABLATION_EXPERIMENT_H_
