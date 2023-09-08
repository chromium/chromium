// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/shared_image_manager.h"

#include <inttypes.h>

#include <memory>
#include <utility>

#include "base/containers/contains.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/process_memory_dump.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "gpu/command_buffer/common/shared_image_trace_utils.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"
#include "ui/gl/trace_util.h"

#if BUILDFLAG(IS_WIN)
#include "gpu/command_buffer/service/dxgi_shared_handle_manager.h"
#include "ui/gl/gl_angle_util_win.h"
#endif

#if BUILDFLAG(IS_OZONE)
#include "ui/ozone/public/ozone_platform.h"
#endif

#if BUILDFLAG(IS_ANDROID)
#include "base/android/android_hardware_buffer_compat.h"
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
// Overrides for flat_set lookups:
bool operator<(const std::unique_ptr<SharedImageBacking>& lhs,
               const std::unique_ptr<SharedImageBacking>& rhs) {
  return lhs->mailbox() < rhs->mailbox();
}

bool operator<(const Mailbox& lhs,
               const std::unique_ptr<SharedImageBacking>& rhs) {
  return lhs < rhs->mailbox();
}

bool operator<(const std::unique_ptr<SharedImageBacking>& lhs,
               const Mailbox& rhs) {
  return lhs->mailbox() < rhs;
}

class SCOPED_LOCKABLE SharedImageManager::AutoLock {
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

SharedImageManager::SharedImageManager(bool thread_safe,
                                       bool display_context_on_another_thread)
    : display_context_on_another_thread_(display_context_on_another_thread)
#if BUILDFLAG(IS_WIN)
      ,
      dxgi_shared_handle_manager_(
          base::MakeRefCounted<DXGISharedHandleManager>())
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
  DCHECK(backing->mailbox().IsSharedImage());

  AutoLock autolock(this);
  if (base::Contains(images_, backing->mailbox())) {
    LOG(ERROR) << "SharedImageManager::Register: Trying to register an "
                  "already registered mailbox.";
    return nullptr;
  }

  UMA_HISTOGRAM_ENUMERATION("GPU.SharedImage.BackingType", backing->GetType());

  // TODO(jonross): Determine how the direct destruction of a
  // SharedImageRepresentationFactoryRef leads to ref-counting issues as
  // well as thread-checking failures in tests.
  auto factory_ref = std::make_unique<SharedImageRepresentationFactoryRef>(
      this, backing.get(), tracker, /*is_primary=*/true);
  images_.emplace(std::move(backing));

  return factory_ref;
}

std::unique_ptr<SharedImageRepresentationFactoryRef>
SharedImageManager::AddSecondaryReference(const Mailbox& mailbox,
                                          MemoryTypeTracker* tracker) {
  CALLED_ON_VALID_THREAD();
  DCHECK(mailbox.IsSharedImage());

  AutoLock autolock(this);
  auto found = images_.find(mailbox);
  if (found == images_.end()) {
    LOG(ERROR) << "SharedImageManager::AddSecondaryReference: Trying to add "
                  "reference to non-existent mailbox.";
    return nullptr;
  }

  return std::make_unique<SharedImageRepresentationFactoryRef>(
      this, found->get(), tracker, /*is_primary=*/false);
}

std::unique_ptr<GLTextureImageRepresentation>
SharedImageManager::ProduceGLTexture(const Mailbox& mailbox,
                                     MemoryTypeTracker* tracker) {
  CALLED_ON_VALID_THREAD();

  AutoLock autolock(this);
  auto found = images_.find(mailbox);
  if (found == images_.end()) {
    LOG(ERROR) << "SharedImageManager::ProduceGLTexture: Trying to produce a "
                  "representation from a non-existent mailbox. "
               << mailbox.ToDebugString();
    return nullptr;
  }

  auto representation = (*found)->ProduceGLTexture(this, tracker);
  if (!representation) {
    LOG(ERROR) << "SharedImageManager::ProduceGLTexture: Trying to produce a "
                  "representation from an incompatible backing: "
               << (*found)->GetName();
    return nullptr;
  }

  return representation;
}

std::unique_ptr<GLTexturePassthroughImageRepresentation>
SharedImageManager::ProduceGLTexturePassthrough(const Mailbox& mailbox,
                                                MemoryTypeTracker* tracker) {
  CALLED_ON_VALID_THREAD();

  AutoLock autolock(this);
  auto found = images_.find(mailbox);
  if (found == images_.end()) {
    LOG(ERROR) << "SharedImageManager::ProduceGLTexturePassthrough: Trying to "
                  "produce a representation from a non-existent mailbox.";
    return nullptr;
  }

  auto representation = (*found)->ProduceGLTexturePassthrough(this, tracker);
  if (!representation) {
    LOG(ERROR) << "SharedImageManager::ProduceGLTexturePassthrough: Trying to "
                  "produce a representation from an incompatible backing: "
               << (*found)->GetName();
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
  auto found = images_.find(mailbox);
  if (found == images_.end()) {
    LOG(ERROR) << "SharedImageManager::ProduceSkia: Trying to Produce a "
                  "Skia representation from a non-existent mailbox.";
    return nullptr;
  }

  auto representation = (*found)->ProduceSkia(this, tracker, context_state);
  if (!representation) {
    LOG(ERROR) << "SharedImageManager::ProduceSkia: Trying to produce a "
                  "Skia representation from an incompatible backing: "
               << (*found)->GetName();
    return nullptr;
  }

  return representation;
}

std::unique_ptr<DawnImageRepresentation> SharedImageManager::ProduceDawn(
    const Mailbox& mailbox,
    MemoryTypeTracker* tracker,
    const wgpu::Device& device,
    wgpu::BackendType backend_type,
    std::vector<wgpu::TextureFormat> view_formats) {
  CALLED_ON_VALID_THREAD();

  AutoLock autolock(this);
  auto found = images_.find(mailbox);
  if (found == images_.end()) {
    LOG(ERROR) << "SharedImageManager::ProduceDawn: Trying to Produce a "
                  "Dawn representation from a non-existent mailbox.";
    return nullptr;
  }

  auto representation = (*found)->ProduceDawn(
      this, tracker, device, backend_type, std::move(view_formats));
  if (!representation) {
    LOG(ERROR) << "SharedImageManager::ProduceDawn: Trying to produce a "
                  "Dawn representation from an incompatible backing: "
               << (*found)->GetName();
    return nullptr;
  }

  return representation;
}

std::unique_ptr<OverlayImageRepresentation> SharedImageManager::ProduceOverlay(
    const gpu::Mailbox& mailbox,
    gpu::MemoryTypeTracker* tracker) {
  CALLED_ON_VALID_THREAD();

  AutoLock autolock(this);
  auto found = images_.find(mailbox);
  if (found == images_.end()) {
    LOG(ERROR) << "SharedImageManager::ProduceOverlay: Trying to Produce a "
                  "Overlay representation from a non-existent mailbox.";
    return nullptr;
  }

  auto representation = (*found)->ProduceOverlay(this, tracker);
  if (!representation) {
    LOG(ERROR) << "SharedImageManager::ProduceOverlay: Trying to produce a "
                  "Overlay representation from an incompatible backing: "
               << (*found)->GetName();
    return nullptr;
  }

  return representation;
}

std::unique_ptr<VaapiImageRepresentation> SharedImageManager::ProduceVASurface(
    const Mailbox& mailbox,
    MemoryTypeTracker* tracker,
    VaapiDependenciesFactory* dep_factory) {
  CALLED_ON_VALID_THREAD();

  AutoLock autolock(this);
  auto found = images_.find(mailbox);
  if (found == images_.end()) {
    LOG(ERROR) << "SharedImageManager::ProduceVASurface: Trying to produce a "
                  "VA-API representation from a non-existent mailbox.";
    return nullptr;
  }

  auto representation = (*found)->ProduceVASurface(this, tracker, dep_factory);

  if (!representation) {
    LOG(ERROR) << "SharedImageManager::ProduceVASurface: Trying to produce a "
                  "VA-API representation from an incompatible backing: "
               << (*found)->GetName();
    return nullptr;
  }
  return representation;
}

std::unique_ptr<MemoryImageRepresentation> SharedImageManager::ProduceMemory(
    const Mailbox& mailbox,
    MemoryTypeTracker* tracker) {
  CALLED_ON_VALID_THREAD();

  AutoLock autolock(this);
  auto found = images_.find(mailbox);
  if (found == images_.end()) {
    LOG(ERROR) << "SharedImageManager::ProduceMemory: Trying to Produce a "
                  "Memory representation from a non-existent mailbox.";
    return nullptr;
  }

  // This is expected to fail based on the SharedImageBacking type, so don't log
  // error here. Caller is expected to handle nullptr.
  return (*found)->ProduceMemory(this, tracker);
}

std::unique_ptr<RasterImageRepresentation> SharedImageManager::ProduceRaster(
    const Mailbox& mailbox,
    MemoryTypeTracker* tracker) {
  CALLED_ON_VALID_THREAD();

  AutoLock autolock(this);
  auto found = images_.find(mailbox);
  if (found == images_.end()) {
    LOG(ERROR) << "SharedImageManager::ProduceRaster: Trying to Produce a "
                  "Raster representation from a non-existent mailbox.";
    return nullptr;
  }

  // This is expected to fail based on the SharedImageBacking type, so don't log
  // error here. Caller is expected to handle nullptr.
  return (*found)->ProduceRaster(this, tracker);
}

std::unique_ptr<VideoDecodeImageRepresentation>
SharedImageManager::ProduceVideoDecode(VideoDecodeDevice device,
                                       const Mailbox& mailbox,
                                       MemoryTypeTracker* tracker) {
  CALLED_ON_VALID_THREAD();

  AutoLock autolock(this);
  auto found = images_.find(mailbox);
  if (found == images_.end()) {
    LOG(ERROR)
        << "SharedImageManager::ProduceVideoDecode: Trying to Produce a D3D"
           "representation from a non-existent mailbox.";
    return nullptr;
  }

  // This is expected to fail based on the SharedImageBacking type, so don't log
  // error here. Caller is expected to handle nullptr.
  return (*found)->ProduceVideoDecode(this, tracker, device);
}

#if BUILDFLAG(IS_ANDROID)
std::unique_ptr<LegacyOverlayImageRepresentation>
SharedImageManager::ProduceLegacyOverlay(const Mailbox& mailbox,
                                         MemoryTypeTracker* tracker) {
  CALLED_ON_VALID_THREAD();

  AutoLock autolock(this);
  auto found = images_.find(mailbox);
  if (found == images_.end()) {
    LOG(ERROR)
        << "SharedImageManager::ProduceLegacyOverlay: Trying to Produce a "
           "Legacy Overlay representation from a non-existent mailbox.";
    return nullptr;
  }

  auto representation = (*found)->ProduceLegacyOverlay(this, tracker);
  if (!representation) {
    LOG(ERROR)
        << "SharedImageManager::ProduceLegacyOverlay: Trying to produce a "
           "Legacy Overlay representation from an incompatible backing: "
        << (*found)->GetName();
    return nullptr;
  }

  return representation;
}
#endif

void SharedImageManager::OnRepresentationDestroyed(
    const Mailbox& mailbox,
    SharedImageRepresentation* representation) {
  CALLED_ON_VALID_THREAD();

  AutoLock autolock(this);

  {
    auto found = images_.find(mailbox);
    if (found == images_.end()) {
      LOG(ERROR) << "SharedImageManager::OnRepresentationDestroyed: Trying to "
                    "destroy a non existent mailbox.";
      return;
    }

    // TODO(piman): When the original (factory) representation is destroyed, we
    // should treat the backing as pending destruction and prevent additional
    // representations from being created. This will help avoid races due to a
    // consumer getting lucky with timing due to a representation inadvertently
    // extending a backing's lifetime.
    (*found)->ReleaseRef(representation);
  }

  {
    // TODO(jonross): Once the pending destruction TODO above is addressed then
    // this block can be removed, and the deletion can occur directly. Currently
    // SharedImageManager::OnRepresentationDestroyed can be nested, so we need
    // to get the iterator again.
    auto found = images_.find(mailbox);
    if (found != images_.end() && (!(*found)->HasAnyRefs()))
      images_.erase(found);
  }
}

void SharedImageManager::SetPurgeable(const Mailbox& mailbox, bool purgeable) {
  AutoLock autolock(this);
  auto found = images_.find(mailbox);
  if (found == images_.end()) {
    LOG(ERROR) << "SharedImageManager::SetPurgeable: Non-existent mailbox.";
    return;
  }
  (*found)->SetPurgeable(purgeable);
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
    for (auto& backing : images_) {
      size_t size = backing->GetEstimatedSizeForMemoryDump();
      total_size += size;
      total_purgeable_size += backing->IsPurgeable() ? size : 0;
    }

    base::trace_event::MemoryAllocatorDump* dump =
        pmd->CreateAllocatorDump(base_dump_name);
    dump->AddScalar(base::trace_event::MemoryAllocatorDump::kNameSize,
                    base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                    total_size);
    dump->AddScalar("purgeable_size",
                    base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                    total_purgeable_size);

    // Early out, no need for more detail in a BACKGROUND dump.
    return true;
  }

  for (auto& backing : images_) {
    auto* memory_tracker = backing->GetMemoryTracker();

    // All the backings registered here should have a memory tracker.
    DCHECK(memory_tracker);

    // Unique name in the process.
    std::string dump_name = base::StringPrintf(
        "%s/client_0x%" PRIX32 "/mailbox_%s", base_dump_name,
        memory_tracker->ClientId(), backing->mailbox().ToDebugString().c_str());

    // GUID which expresses shared ownership with the client process. This must
    // match the client-side GUID for mailbox.
    auto client_guid = GetSharedImageGUIDForTracing(backing->mailbox());

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
  auto found = images_.find(mailbox);
  if (found == images_.end())
    return nullptr;
  return (*found)->GetNativePixmap();
}

bool SharedImageManager::SupportsScanoutImages() {
#if BUILDFLAG(IS_APPLE)
  return true;
#elif BUILDFLAG(IS_ANDROID)
  return base::AndroidHardwareBufferCompat::IsSupportAvailable();
#elif BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_FUCHSIA)
  return ui::OzonePlatform::GetInstance()
      ->GetPlatformRuntimeProperties()
      .supports_native_pixmaps;
#elif BUILDFLAG(IS_WIN)
  return false;
#else
  return false;
#endif
}

}  // namespace gpu
