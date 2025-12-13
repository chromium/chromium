// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/shared_image_manager.h"

#include <inttypes.h>

#include <cstdint>
#include <memory>
#include <utility>

#include "base/containers/contains.h"
#include "base/debug/crash_logging.h"
#include "base/debug/dump_without_crashing.h"
#include "base/logging.h"
#include "base/memory/stack_allocated.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/process_memory_dump.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "gpu/command_buffer/common/shared_image_trace_utils.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"
#include "ui/gl/trace_util.h"

#if BUILDFLAG(IS_WIN)
#include "gpu/command_buffer/service/dxgi_shared_handle_manager.h"
#include "ui/gfx/win/d3d_shared_fence.h"
#include "ui/gl/direct_composition_support.h"
#include "ui/gl/gl_angle_util_win.h"
#endif

#if BUILDFLAG(IS_OZONE)
#include "components/viz/common/gpu/vulkan_context_provider.h"
#include "gpu/config/gpu_finch_features.h"
#include "ui/ozone/public/ozone_platform.h"
#endif

#if DCHECK_IS_ON()
#define CALLED_ON_VALID_THREAD()                      \
  do {                                                \
    if (!this->is_thread_safe())                      \
      DCHECK_CALLED_ON_VALID_THREAD(thread_checker_); \
  } while (false)
#else
#define CALLED_ON_VALID_THREAD()
#endif

namespace gpu {

namespace {

// `DCHECKS` and dumps without crashing that `backing`'s usage overlaps with
// `usage`.
void EnforceSharedImageUsage(const SharedImageBacking* backing,
                             SharedImageUsageSet usage) {
  if (!backing->usage().HasAny(usage)) {
    SCOPED_CRASH_KEY_STRING32("SharedImageUsage", "debug_label",
                              backing->debug_label());
    SCOPED_CRASH_KEY_STRING32("SharedImageUsage", "name", backing->GetName());
    SCOPED_CRASH_KEY_NUMBER("SharedImageUsage", "required_usage",
                            static_cast<uint32_t>(usage));
    SCOPED_CRASH_KEY_NUMBER("SharedImageUsage", "actual_usage",
                            static_cast<uint32_t>(backing->usage()));
    base::debug::DumpWithoutCrashing();
  }
}

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// Used for logging values to GPU.SharedImage.SharedImageFormat UMA.
enum class SharedImageFormatUMA {
  kRGBA_8888 = 0,
  kRGBA_4444 = 1,
  kBGRA_8888 = 2,
  kALPHA_8 = 3,
  kLUMINANCE_8 = 4,
  kRGB_565 = 5,
  kBGR_565 = 6,
  kETC1 = 7,
  kR_8 = 8,
  kRG_88 = 9,
  kLUMINANCE_F16 = 10,
  kRGBA_F16 = 11,
  kR_16 = 12,
  kRG_1616 = 13,
  kRGBX_8888 = 14,
  kBGRX_8888 = 15,
  kRGBA_1010102 = 16,
  kBGRA_1010102 = 17,
  kR_F16 = 18,
  kYV12 = 19,
  kNV12 = 20,
  kNV12A = 21,
  kP010 = 22,
  kNV16 = 23,
  kNV24 = 24,
  kP210 = 25,
  kP410 = 26,
  kI420 = 27,
  kI420A = 28,
  kI422 = 29,
  kI444 = 30,
  kYUV420P10 = 31,
  kYUV422P10 = 32,
  kYUV444P10 = 33,
  kYUV420P16 = 34,
  kYUV422P16 = 35,
  kYUV444P16 = 36,
  kOther = 37,
  kMaxValue = kOther
};

SharedImageFormatUMA GetSharedImageFormatUMA(viz::SharedImageFormat format) {
  if (format.is_single_plane()) {
    if (format == viz::SinglePlaneFormat::kRGBA_8888) {
      return SharedImageFormatUMA::kRGBA_8888;
    } else if (format == viz::SinglePlaneFormat::kRGBA_4444) {
      return SharedImageFormatUMA::kRGBA_4444;
    } else if (format == viz::SinglePlaneFormat::kBGRA_8888) {
      return SharedImageFormatUMA::kBGRA_8888;
    } else if (format == viz::SinglePlaneFormat::kALPHA_8) {
      return SharedImageFormatUMA::kALPHA_8;
    } else if (format == viz::SinglePlaneFormat::kBGR_565) {
      return SharedImageFormatUMA::kBGR_565;
    } else if (format == viz::SinglePlaneFormat::kETC1) {
      return SharedImageFormatUMA::kETC1;
    } else if (format == viz::SinglePlaneFormat::kR_8) {
      return SharedImageFormatUMA::kR_8;
    } else if (format == viz::SinglePlaneFormat::kRG_88) {
      return SharedImageFormatUMA::kRG_88;
    } else if (format == viz::SinglePlaneFormat::kLUMINANCE_F16) {
      return SharedImageFormatUMA::kLUMINANCE_F16;
    } else if (format == viz::SinglePlaneFormat::kRGBA_F16) {
      return SharedImageFormatUMA::kRGBA_F16;
    } else if (format == viz::SinglePlaneFormat::kR_16) {
      return SharedImageFormatUMA::kR_16;
    } else if (format == viz::SinglePlaneFormat::kRG_1616) {
      return SharedImageFormatUMA::kRG_1616;
    } else if (format == viz::SinglePlaneFormat::kRGBX_8888) {
      return SharedImageFormatUMA::kRGBX_8888;
    } else if (format == viz::SinglePlaneFormat::kBGRX_8888) {
      return SharedImageFormatUMA::kBGRX_8888;
    } else if (format == viz::SinglePlaneFormat::kRGBA_1010102) {
      return SharedImageFormatUMA::kRGBA_1010102;
    } else if (format == viz::SinglePlaneFormat::kBGRA_1010102) {
      return SharedImageFormatUMA::kBGRA_1010102;
    } else {
      DCHECK_EQ(format, viz::SinglePlaneFormat::kR_F16);
      return SharedImageFormatUMA::kR_F16;
    }
  }

  using PlaneConfig = viz::SharedImageFormat::PlaneConfig;
  using Subsampling = viz::SharedImageFormat::Subsampling;
  using ChannelFormat = viz::SharedImageFormat::ChannelFormat;

  if (format == viz::MultiPlaneFormat::kYV12) {
    return SharedImageFormatUMA::kYV12;
  } else if (format == viz::MultiPlaneFormat::kNV12) {
    return SharedImageFormatUMA::kNV12;
  } else if (format == viz::MultiPlaneFormat::kNV12A) {
    return SharedImageFormatUMA::kNV12A;
  } else if (format == viz::MultiPlaneFormat::kP010) {
    return SharedImageFormatUMA::kP010;
  } else if (format == viz::MultiPlaneFormat::kNV16) {
    return SharedImageFormatUMA::kNV16;
  } else if (format == viz::MultiPlaneFormat::kNV24) {
    return SharedImageFormatUMA::kNV24;
  } else if (format == viz::MultiPlaneFormat::kP210) {
    return SharedImageFormatUMA::kP210;
  } else if (format == viz::MultiPlaneFormat::kP410) {
    return SharedImageFormatUMA::kP410;
  } else if (format == viz::MultiPlaneFormat::kI420A) {
    return SharedImageFormatUMA::kI420A;
  } else if (format.is_multi_plane() &&
             format.plane_config() == PlaneConfig::kY_U_V) {
    // Y_U_V planar formats are usually used by software video frames.
    switch (format.channel_format()) {
      case ChannelFormat::k8:
        switch (format.subsampling()) {
          case Subsampling::k420:
            return SharedImageFormatUMA::kI420;
          case Subsampling::k422:
            return SharedImageFormatUMA::kI422;
          case Subsampling::k444:
            return SharedImageFormatUMA::kI444;
        }
      case ChannelFormat::k10:
        switch (format.subsampling()) {
          case Subsampling::k420:
            return SharedImageFormatUMA::kYUV420P10;
          case Subsampling::k422:
            return SharedImageFormatUMA::kYUV422P10;
          case Subsampling::k444:
            return SharedImageFormatUMA::kYUV444P10;
        }
      case ChannelFormat::k16:
      case ChannelFormat::k16F:
        switch (format.subsampling()) {
          case Subsampling::k420:
            return SharedImageFormatUMA::kYUV420P16;
          case Subsampling::k422:
            return SharedImageFormatUMA::kYUV422P16;
          case Subsampling::k444:
            return SharedImageFormatUMA::kYUV444P16;
        }
    }
  } else {
    return SharedImageFormatUMA::kOther;
  }
}

}  // namespace

class SCOPED_LOCKABLE SharedImageManager::AutoLock {
  STACK_ALLOCATED();

 public:
  explicit AutoLock(SharedImageManager* manager)
      EXCLUSIVE_LOCK_FUNCTION(manager->lock_)
      : auto_lock_(manager->is_thread_safe() ? &manager->lock_.value()
                                             : nullptr) {}

  AutoLock(const AutoLock&) = delete;
  AutoLock& operator=(const AutoLock&) = delete;

  ~AutoLock() UNLOCK_FUNCTION() = default;

 private:
  base::AutoLockMaybe auto_lock_;
};

SharedImageManager::SharedImageManager(
    bool thread_safe,
    bool display_context_on_another_thread,
    viz::VulkanContextProvider* vulkan_context_provider,
    scoped_refptr<base::SingleThreadTaskRunner> io_runner)
    : display_context_on_another_thread_(display_context_on_another_thread)
#if BUILDFLAG(IS_WIN)
      ,
      dxgi_shared_handle_manager_(
          base::MakeRefCounted<DXGISharedHandleManager>())
#endif
#if BUILDFLAG(IS_OZONE)
      ,
      vulkan_context_provider_(vulkan_context_provider)
#endif
#if BUILDFLAG(IS_WIN)
      ,
      io_runner_(std::move(io_runner))
#endif
{
  DCHECK(!display_context_on_another_thread || thread_safe);
  if (thread_safe) {
    lock_.emplace();
  }
  CALLED_ON_VALID_THREAD();

  // In tests there might not be a SingleThreadTaskRunner for this thread.
  if (base::SingleThreadTaskRunner::HasCurrentDefault()) {
    is_registered_as_memory_dump_provider_ = true;
    base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
        this, "SharedImageManager",
        base::SingleThreadTaskRunner::GetCurrentDefault());
  }
}

SharedImageManager::~SharedImageManager() {
  CALLED_ON_VALID_THREAD();
#if DCHECK_IS_ON()
  AutoLock auto_lock(this);
#endif
  DCHECK(images_.empty());

  if (is_registered_as_memory_dump_provider_) {
    base::trace_event::MemoryDumpManager::GetInstance()->UnregisterDumpProvider(
        this);
  }
}

std::unique_ptr<SharedImageRepresentationFactoryRef>
SharedImageManager::Register(std::unique_ptr<SharedImageBacking> backing,
                             MemoryTypeTracker* tracker) {
  CALLED_ON_VALID_THREAD();

  AutoLock autolock(this);
  if (base::Contains(images_, backing->mailbox())) {
    LOG(ERROR) << "SharedImageManager::Register: Trying to register an "
                  "already registered mailbox.";
    backing->MarkForDestruction();
    return nullptr;
  }

  UMA_HISTOGRAM_ENUMERATION("GPU.SharedImage.BackingType", backing->GetType());
  UMA_HISTOGRAM_ENUMERATION("GPU.SharedImage.SharedImageFormat",
                            GetSharedImageFormatUMA(backing->format()));

  // TODO(jonross): Determine how the direct destruction of a
  // SharedImageRepresentationFactoryRef leads to ref-counting issues as
  // well as thread-checking failures in tests.
  auto factory_ref = std::make_unique<SharedImageRepresentationFactoryRef>(
      this, backing.get(), tracker, /*is_primary=*/true);
  gpu::Mailbox mailbox = backing->mailbox();
  images_.emplace(std::move(mailbox), std::move(backing));

  return factory_ref;
}

SharedImageBacking* SharedImageManager::GetBacking(
    const Mailbox& mailbox) const {
  CALLED_ON_VALID_THREAD();
  auto it = images_.find(mailbox);
  return it != images_.end() ? it->second.get() : nullptr;
}

std::unique_ptr<SharedImageRepresentationFactoryRef>
SharedImageManager::AddSecondaryReference(const Mailbox& mailbox,
                                          MemoryTypeTracker* tracker) {
  CALLED_ON_VALID_THREAD();

  AutoLock autolock(this);
  auto* backing = GetBacking(mailbox);
  if (!backing) {
    LOG(ERROR) << "SharedImageManager::AddSecondaryReference: Trying to add "
                  "reference to non-existent mailbox.";
    return nullptr;
  }

  return std::make_unique<SharedImageRepresentationFactoryRef>(
      this, backing, tracker, /*is_primary=*/false);
}

std::unique_ptr<GLTextureImageRepresentation>
SharedImageManager::ProduceGLTexture(const Mailbox& mailbox,
                                     MemoryTypeTracker* tracker) {
  CALLED_ON_VALID_THREAD();

  AutoLock autolock(this);
  auto* backing = GetBacking(mailbox);
  if (!backing) {
    LOG(ERROR) << "SharedImageManager::ProduceGLTexture: Trying to produce a "
                  "representation from a non-existent mailbox. "
               << mailbox.ToDebugString();
    return nullptr;
  }

  auto representation = backing->ProduceGLTexture(this, tracker);
  if (!representation) {
    LOG(ERROR) << "SharedImageManager::ProduceGLTexture: Trying to produce a "
                  "representation from an incompatible backing: "
               << backing->GetName();
    return nullptr;
  }

  return representation;
}

std::unique_ptr<GLTexturePassthroughImageRepresentation>
SharedImageManager::ProduceGLTexturePassthrough(const Mailbox& mailbox,
                                                MemoryTypeTracker* tracker) {
  CALLED_ON_VALID_THREAD();

  AutoLock autolock(this);
  auto* backing = GetBacking(mailbox);
  if (!backing) {
    LOG(ERROR) << "SharedImageManager::ProduceGLTexturePassthrough: Trying to "
                  "produce a representation from a non-existent mailbox.";
    return nullptr;
  }

  auto representation = backing->ProduceGLTexturePassthrough(this, tracker);
  if (!representation) {
    LOG(ERROR) << "SharedImageManager::ProduceGLTexturePassthrough: Trying to "
                  "produce a representation from an incompatible backing: "
               << backing->GetName();
    return nullptr;
  }

  return representation;
}

std::unique_ptr<SkiaImageRepresentation> SharedImageManager::ProduceSkia(
    const Mailbox& mailbox,
    MemoryTypeTracker* tracker,
    scoped_refptr<SharedContextState> context_state) {
  CALLED_ON_VALID_THREAD();

  AutoLock autolock(this);
  auto* backing = GetBacking(mailbox);
  if (!backing) {
    LOG(ERROR) << "SharedImageManager::ProduceSkia: Trying to Produce a "
                  "Skia representation from a non-existent mailbox.";
    return nullptr;
  }

  auto representation = backing->ProduceSkia(this, tracker, context_state);
  if (!representation) {
    LOG(ERROR) << "SharedImageManager::ProduceSkia: Trying to produce a "
                  "Skia representation from an incompatible backing: "
               << backing->GetName();
    return nullptr;
  }

  return representation;
}

std::unique_ptr<DawnImageRepresentation> SharedImageManager::ProduceDawn(
    const Mailbox& mailbox,
    MemoryTypeTracker* tracker,
    const wgpu::Device& device,
    wgpu::BackendType backend_type,
    std::vector<wgpu::TextureFormat> view_formats,
    scoped_refptr<SharedContextState> context_state) {
  CALLED_ON_VALID_THREAD();

  AutoLock autolock(this);
  auto* backing = GetBacking(mailbox);
  if (!backing) {
    LOG(ERROR) << "SharedImageManager::ProduceDawn: Trying to Produce a "
                  "Dawn representation from a non-existent mailbox.";
    return nullptr;
  }

  EnforceSharedImageUsage(backing, {SHARED_IMAGE_USAGE_WEBGPU_READ,
                                    SHARED_IMAGE_USAGE_WEBGPU_WRITE});
  auto representation =
      backing->ProduceDawn(this, tracker, device, backend_type,
                           std::move(view_formats), context_state);
  if (!representation) {
    LOG(ERROR) << "SharedImageManager::ProduceDawn: Trying to produce a "
                  "Dawn representation from an incompatible backing: "
               << backing->GetName();
    return nullptr;
  }

  return representation;
}

std::unique_ptr<DawnBufferRepresentation> SharedImageManager::ProduceDawnBuffer(
    const Mailbox& mailbox,
    MemoryTypeTracker* tracker,
    const wgpu::Device& device,
    wgpu::BackendType backend_type,
    scoped_refptr<SharedContextState> context_state) {
  CALLED_ON_VALID_THREAD();

  AutoLock autolock(this);
  auto* backing = GetBacking(mailbox);
  if (!backing) {
    LOG(ERROR) << "SharedImageManager::ProduceDawnBuffer: Trying to produce a "
                  "Dawn buffer representation from a non-existent mailbox.";
    return nullptr;
  }

  auto representation = backing->ProduceDawnBuffer(this, tracker, device,
                                                   backend_type, context_state);
  if (!representation) {
    LOG(ERROR) << "SharedImageManager::ProduceDawnBuffer: Trying to produce a "
                  "Dawn buffer representation from an incompatible backing: "
               << backing->GetName();
    return nullptr;
  }

  return representation;
}

std::unique_ptr<WebNNTensorRepresentation>
SharedImageManager::ProduceWebNNTensor(const Mailbox& mailbox,
                                       MemoryTypeTracker* tracker) {
  CALLED_ON_VALID_THREAD();

  AutoLock autolock(this);
  SharedImageBacking* backing = GetBacking(mailbox);
  if (!backing) {
    LOG(ERROR) << "SharedImageManager::ProduceWebNNTensor: Trying to produce a "
                  "WebNN tensor representation from a non-existent mailbox.";
    return nullptr;
  }

  std::unique_ptr<WebNNTensorRepresentation> representation =
      backing->ProduceWebNNTensor(this, tracker);
  if (!representation) {
    LOG(ERROR) << "SharedImageManager::ProduceWebNNTensor: Trying to produce a "
                  "WebNN tensor representation from an incompatible backing: "
               << backing->GetName();
    return nullptr;
  }
  return representation;
}

std::unique_ptr<OverlayImageRepresentation> SharedImageManager::ProduceOverlay(
    const gpu::Mailbox& mailbox,
    gpu::MemoryTypeTracker* tracker) {
  CALLED_ON_VALID_THREAD();

  AutoLock autolock(this);
  auto* backing = GetBacking(mailbox);
  if (!backing) {
    LOG(ERROR) << "SharedImageManager::ProduceOverlay: Trying to Produce a "
                  "Overlay representation from a non-existent mailbox.";
    return nullptr;
  }

  EnforceSharedImageUsage(backing, {SHARED_IMAGE_USAGE_SCANOUT});
  auto representation = backing->ProduceOverlay(this, tracker);
  if (!representation) {
    LOG(ERROR) << "SharedImageManager::ProduceOverlay: Trying to produce a "
                  "Overlay representation from an incompatible backing: "
               << backing->GetName();
    return nullptr;
  }

  return representation;
}

std::unique_ptr<MemoryImageRepresentation> SharedImageManager::ProduceMemory(
    const Mailbox& mailbox,
    MemoryTypeTracker* tracker) {
  CALLED_ON_VALID_THREAD();

  AutoLock autolock(this);
  auto* backing = GetBacking(mailbox);
  if (!backing) {
    LOG(ERROR) << "SharedImageManager::ProduceMemory: Trying to Produce a "
                  "Memory representation from a non-existent mailbox.";
    return nullptr;
  }

  // This is expected to fail based on the SharedImageBacking type, so don't log
  // error here. Caller is expected to handle nullptr.
  return backing->ProduceMemory(this, tracker);
}

std::unique_ptr<RasterImageRepresentation> SharedImageManager::ProduceRaster(
    const Mailbox& mailbox,
    MemoryTypeTracker* tracker) {
  CALLED_ON_VALID_THREAD();

  AutoLock autolock(this);
  auto* backing = GetBacking(mailbox);
  if (!backing) {
    LOG(ERROR) << "SharedImageManager::ProduceRaster: Trying to Produce a "
                  "Raster representation from a non-existent mailbox.";
    return nullptr;
  }

  EnforceSharedImageUsage(backing, {SHARED_IMAGE_USAGE_RAW_DRAW});
  // This is expected to fail based on the SharedImageBacking type, so don't log
  // error here. Caller is expected to handle nullptr.
  return backing->ProduceRaster(this, tracker);
}

std::unique_ptr<VideoImageRepresentation> SharedImageManager::ProduceVideo(
    VideoDevice device,
    const Mailbox& mailbox,
    MemoryTypeTracker* tracker) {
  CALLED_ON_VALID_THREAD();

  AutoLock autolock(this);
  auto* backing = GetBacking(mailbox);
  if (!backing) {
    LOG(ERROR)
        << "SharedImageManager::ProduceVideoDecode: Trying to Produce a D3D"
           "representation from a non-existent mailbox.";
    return nullptr;
  }

  // This is expected to fail based on the SharedImageBacking type, so don't log
  // error here. Caller is expected to handle nullptr.
  return backing->ProduceVideo(this, tracker, device);
}

#if BUILDFLAG(ENABLE_VULKAN)
std::unique_ptr<VulkanImageRepresentation> SharedImageManager::ProduceVulkan(
    const Mailbox& mailbox,
    MemoryTypeTracker* tracker,
    gpu::VulkanDeviceQueue* vulkan_device_queue,
    gpu::VulkanImplementation& vulkan_impl,
    bool needs_detiling) {
  CALLED_ON_VALID_THREAD();

  AutoLock autolock(this);
  auto* backing = GetBacking(mailbox);
  if (!backing) {
    LOG(ERROR)
        << "SharedImageManager::ProduceVulkanImage: Trying to produce vulkan"
           "representation from a non-existent mailbox.";
    return nullptr;
  }

  return backing->ProduceVulkan(this, tracker, vulkan_device_queue, vulkan_impl,
                                needs_detiling);
}
#endif

#if BUILDFLAG(IS_ANDROID)
std::unique_ptr<LegacyOverlayImageRepresentation>
SharedImageManager::ProduceLegacyOverlay(const Mailbox& mailbox,
                                         MemoryTypeTracker* tracker) {
  CALLED_ON_VALID_THREAD();

  AutoLock autolock(this);
  auto* backing = GetBacking(mailbox);
  if (!backing) {
    LOG(ERROR)
        << "SharedImageManager::ProduceLegacyOverlay: Trying to Produce a "
           "Legacy Overlay representation from a non-existent mailbox.";
    return nullptr;
  }

  EnforceSharedImageUsage(backing, {SHARED_IMAGE_USAGE_SCANOUT});
  auto representation = backing->ProduceLegacyOverlay(this, tracker);
  if (!representation) {
    LOG(ERROR)
        << "SharedImageManager::ProduceLegacyOverlay: Trying to produce a "
           "Legacy Overlay representation from an incompatible backing: "
        << backing->GetName();
    return nullptr;
  }

  return representation;
}
#endif

#if BUILDFLAG(IS_WIN)
void SharedImageManager::UpdateExternalFence(
    const Mailbox& mailbox,
    scoped_refptr<gfx::D3DSharedFence> external_fence) {
  CALLED_ON_VALID_THREAD();
  AutoLock autolock(this);
  auto* backing = GetBacking(mailbox);
  if (!backing) {
    LOG(ERROR)
        << "SharedImageManager::ProduceVideoDecode: Trying to Produce a D3D"
           "representation from a non-existent mailbox.";
    return;
  }

  backing->UpdateExternalFence(std::move(external_fence));
}
#endif

std::optional<SharedImageUsageSet> SharedImageManager::GetUsageForMailbox(
    const Mailbox& mailbox) {
  AutoLock autolock(this);
  auto* backing = GetBacking(mailbox);
  return backing ? std::make_optional(backing->usage()) : std::nullopt;
}

void SharedImageManager::OnRepresentationDestroyed(
    const Mailbox& mailbox,
    SharedImageRepresentation* representation) {
  CALLED_ON_VALID_THREAD();

  AutoLock autolock(this);
  {
    auto* backing = GetBacking(mailbox);
    if (!backing) {
      LOG(ERROR) << "SharedImageManager::OnRepresentationDestroyed: Trying to "
                    "destroy a non existent mailbox.";
      return;
    }

    // TODO(piman): When the original (factory) representation is destroyed, we
    // should treat the backing as pending destruction and prevent additional
    // representations from being created. This will help avoid races due to a
    // consumer getting lucky with timing due to a representation inadvertently
    // extending a backing's lifetime.
    backing->ReleaseRef(representation);
  }

  {
    // TODO(jonross): Once the pending destruction TODO above is addressed then
    // this block can be removed, and the deletion can occur directly. Currently
    // SharedImageManager::OnRepresentationDestroyed can be nested, so we need
    // to get the iterator again.
    auto it = images_.find(mailbox);
    if (it != images_.end() && !it->second->HasAnyRefs()) {
      images_.erase(it);
    }
  }
}

void SharedImageManager::SetPurgeable(const Mailbox& mailbox, bool purgeable) {
  AutoLock autolock(this);
  auto* backing = GetBacking(mailbox);
  if (!backing) {
    LOG(ERROR) << "SharedImageManager::SetPurgeable: Non-existent mailbox.";
    return;
  }
  backing->SetPurgeable(purgeable);
}

bool SharedImageManager::OnMemoryDump(
    const base::trace_event::MemoryDumpArgs& args,
    base::trace_event::ProcessMemoryDump* pmd) {
  CALLED_ON_VALID_THREAD();
  AutoLock autolock(this);

  const char* base_dump_name = "gpu/shared_images";

  if (args.level_of_detail ==
      base::trace_event::MemoryDumpLevelOfDetail::kBackground) {
    size_t total_size = 0;
    size_t total_purgeable_size = 0;
    size_t total_non_exo_size = 0;
    for (auto& [_, backing] : images_) {
      size_t size = backing->GetEstimatedSizeForMemoryDump();
      total_size += size;
      total_purgeable_size += backing->IsPurgeable() ? size : 0;
      total_non_exo_size += backing->IsImportedFromExo() ? 0 : size;
    }

    base::trace_event::MemoryAllocatorDump* dump =
        pmd->CreateAllocatorDump(base_dump_name);
    dump->AddScalar(base::trace_event::MemoryAllocatorDump::kNameSize,
                    base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                    total_size);
    dump->AddScalar("purgeable_size",
                    base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                    total_purgeable_size);
    dump->AddScalar("non_exo_size",
                    base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                    total_non_exo_size);

    // Early out, no need for more detail in a BACKGROUND dump.
    return true;
  }

  for (auto& [mailbox, backing] : images_) {
    auto* memory_tracker = backing->GetMemoryTracker();

    // All the backings registered here should have a memory tracker.
    DCHECK(memory_tracker);

    // Unique name in the process.
    std::string dump_name = base::StringPrintf(
        "%s/client_0x%" PRIX32 "/mailbox_%s", base_dump_name,
        memory_tracker->ClientId(), mailbox.ToDebugString().c_str());

    // GUID which expresses shared ownership with the client process. This must
    // match the client-side GUID for mailbox.
    auto client_guid = GetSharedImageGUIDForTracing(mailbox);

    // Backing will produce dump with relevant information along with ownership
    // edge to `client_guid`.
    backing->OnMemoryDump(dump_name, client_guid, pmd,
                          memory_tracker->ClientTracingId());
  }

  return true;
}

scoped_refptr<gfx::NativePixmap> SharedImageManager::GetNativePixmap(
    const gpu::Mailbox& mailbox) {
  AutoLock autolock(this);
  auto* backing = GetBacking(mailbox);
  return backing ? backing->GetNativePixmap() : nullptr;
}

bool SharedImageManager::SupportsScanoutImages() {
#if BUILDFLAG(IS_APPLE)
  return true;
#elif BUILDFLAG(IS_ANDROID)
  return true;
#elif BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_FUCHSIA)
  return supports_overlays_on_ozone_;
#elif BUILDFLAG(IS_WIN)
  return gl::DirectCompositionTextureSupported();
#else
  return false;
#endif
}

}  // namespace gpu
