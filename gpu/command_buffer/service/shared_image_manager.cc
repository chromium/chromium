// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image_manager.h"

#include <inttypes.h>

#include <memory>
#include <utility>

#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/process_memory_dump.h"
#include "base/trace_event/trace_event.h"
#include "gpu/command_buffer/common/shared_image_trace_utils.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image_representation.h"
#include "ui/gl/trace_util.h"

#if defined(OS_ANDROID)
#include "gpu/command_buffer/service/shared_image_batch_access_manager.h"
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
      : start_time_(base::TimeTicks::Now()),
        auto_lock_(manager->is_thread_safe() ? &manager->lock_.value()
                                             : nullptr) {
    if (manager->is_thread_safe()) {
      UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
          "GPU.SharedImageManager.TimeToAcquireLock",
          base::TimeTicks::Now() - start_time_,
          base::TimeDelta::FromMicroseconds(1), base::TimeDelta::FromSeconds(1),
          50);
    }
  }

  ~AutoLock() UNLOCK_FUNCTION() = default;

 private:
  base::TimeTicks start_time_;
  base::AutoLockMaybe auto_lock_;

  DISALLOW_COPY_AND_ASSIGN(AutoLock);
};

SharedImageManager::SharedImageManager(bool thread_safe,
                                       bool display_context_on_another_thread)
    : display_context_on_another_thread_(display_context_on_another_thread) {
  DCHECK(!display_context_on_another_thread || thread_safe);
  if (thread_safe)
    lock_.emplace();
#if defined(OS_ANDROID)
  batch_access_manager_ = std::make_unique<SharedImageBatchAccessManager>();
#endif
  CALLED_ON_VALID_THREAD();
}

SharedImageManager::~SharedImageManager() {
  CALLED_ON_VALID_THREAD();
#if DCHECK_IS_ON()
  AutoLock auto_lock(this);
#endif
  DCHECK(images_.empty());
}

std::unique_ptr<SharedImageRepresentationFactoryRef>
SharedImageManager::Register(std::unique_ptr<SharedImageBacking> backing,
                             MemoryTypeTracker* tracker) {
  CALLED_ON_VALID_THREAD();
  DCHECK(backing->mailbox().IsSharedImage());

  AutoLock autolock(this);
  if (images_.find(backing->mailbox()) != images_.end()) {
    LOG(ERROR) << "SharedImageManager::Register: Trying to register an "
                  "already registered mailbox.";
    return nullptr;
  }

  // TODO(jonross): Determine how the direct destruction of a
  // SharedImageRepresentationFactoryRef leads to ref-counting issues as
  // well as thread-checking failures in tests.
  auto factory_ref = std::make_unique<SharedImageRepresentationFactoryRef>(
      this, backing.get(), tracker);
  images_.emplace(std::move(backing));

  return factory_ref;
}

void SharedImageManager::OnContextLost(const Mailbox& mailbox) {
  CALLED_ON_VALID_THREAD();

  AutoLock autolock(this);
  auto found = images_.find(mailbox);
  if (found == images_.end()) {
    LOG(ERROR) << "SharedImageManager::OnContextLost: Trying to mark constext "
                  "lost on a non existent mailbox.";
    return;
  }
  (*found)->OnContextLost();
}

std::unique_ptr<SharedImageRepresentationGLTexture>
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
                  "representation from an incompatible mailbox.";
    return nullptr;
  }

  return representation;
}

std::unique_ptr<SharedImageRepresentationGLTexture>
SharedImageManager::ProduceRGBEmulationGLTexture(const Mailbox& mailbox,
                                                 MemoryTypeTracker* tracker) {
  CALLED_ON_VALID_THREAD();

  AutoLock autolock(this);
  auto found = images_.find(mailbox);
  if (found == images_.end()) {
    LOG(ERROR) << "SharedImageManager::ProduceRGBEmulationGLTexture: Trying to "
                  "produce a representation from a non-existent mailbox.";
    return nullptr;
  }

  auto representation = (*found)->ProduceRGBEmulationGLTexture(this, tracker);
  if (!representation) {
    LOG(ERROR) << "SharedImageManager::ProduceRGBEmulationGLTexture: Trying to "
                  "produce a representation from an incompatible mailbox.";
    return nullptr;
  }

  return representation;
}

std::unique_ptr<SharedImageRepresentationGLTexturePassthrough>
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
                  "produce a representation from an incompatible mailbox.";
    return nullptr;
  }

  return representation;
}

std::unique_ptr<SharedImageRepresentationSkia> SharedImageManager::ProduceSkia(
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
                  "Skia representation from an incompatible mailbox.";
    return nullptr;
  }

  return representation;
}

std::unique_ptr<SharedImageRepresentationDawn> SharedImageManager::ProduceDawn(
    const Mailbox& mailbox,
    MemoryTypeTracker* tracker,
    WGPUDevice device) {
  CALLED_ON_VALID_THREAD();

  AutoLock autolock(this);
  auto found = images_.find(mailbox);
  if (found == images_.end()) {
    LOG(ERROR) << "SharedImageManager::ProduceDawn: Trying to Produce a "
                  "Dawn representation from a non-existent mailbox.";
    return nullptr;
  }

  auto representation = (*found)->ProduceDawn(this, tracker, device);
  if (!representation) {
    LOG(ERROR) << "SharedImageManager::ProduceDawn: Trying to produce a "
                  "Dawn representation from an incompatible mailbox.";
    return nullptr;
  }

  return representation;
}

std::unique_ptr<SharedImageRepresentationOverlay>
SharedImageManager::ProduceOverlay(const gpu::Mailbox& mailbox,
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
                  "Overlay representation from an incompatible mailbox.";
    return nullptr;
  }

  return representation;
}

std::unique_ptr<SharedImageRepresentationVaapi>
SharedImageManager::ProduceVASurface(const Mailbox& mailbox,
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
                  "VA-API representation from an incompatible mailbox.";
    return nullptr;
  }
  return representation;
}

std::unique_ptr<SharedImageRepresentationMemory>
SharedImageManager::ProduceMemory(const Mailbox& mailbox,
                                  MemoryTypeTracker* tracker) {
  CALLED_ON_VALID_THREAD();

  AutoLock autolock(this);
  auto found = images_.find(mailbox);
  if (found == images_.end()) {
    LOG(ERROR) << "SharedImageManager::Producememory: Trying to Produce a "
                  "Memory representation from a non-existent mailbox.";
    return nullptr;
  }

  // This is expected to fail based on the SharedImageBacking type, so don't log
  // error here. Caller is expected to handle nullptr.
  return (*found)->ProduceMemory(this, tracker);
}

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

void SharedImageManager::OnMemoryDump(const Mailbox& mailbox,
                                      base::trace_event::ProcessMemoryDump* pmd,
                                      int client_id,
                                      uint64_t client_tracing_id) {
  CALLED_ON_VALID_THREAD();

  AutoLock autolock(this);
  auto found = images_.find(mailbox);
  if (found == images_.end()) {
    LOG(ERROR) << "SharedImageManager::OnMemoryDump: Trying to dump memory for "
                  "a non existent mailbox.";
    return;
  }

  auto* backing = found->get();
  size_t estimated_size = backing->EstimatedSizeForMemTracking();
  if (estimated_size == 0)
    return;

  // Unique name in the process.
  std::string dump_name =
      base::StringPrintf("gpu/shared_images/client_0x%" PRIX32 "/mailbox_%s",
                         client_id, mailbox.ToDebugString().c_str());

  base::trace_event::MemoryAllocatorDump* dump =
      pmd->CreateAllocatorDump(dump_name);
  dump->AddScalar(base::trace_event::MemoryAllocatorDump::kNameSize,
                  base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                  estimated_size);
  // Add a mailbox guid which expresses shared ownership with the client
  // process.
  // This must match the client-side.
  auto client_guid = GetSharedImageGUIDForTracing(mailbox);
  pmd->CreateSharedGlobalAllocatorDump(client_guid);
  pmd->AddOwnershipEdge(dump->guid(), client_guid);

  // Allow the SharedImageBacking to attach additional data to the dump
  // or dump additional sub-paths.
  backing->OnMemoryDump(dump_name, dump, pmd, client_tracing_id);
}

scoped_refptr<gfx::NativePixmap> SharedImageManager::GetNativePixmap(
    const gpu::Mailbox& mailbox) {
  AutoLock autolock(this);
  auto found = images_.find(mailbox);
  if (found == images_.end())
    return nullptr;
  return (*found)->GetNativePixmap();
}

bool SharedImageManager::BeginBatchReadAccess() {
#if defined(OS_ANDROID)
  return batch_access_manager_->BeginBatchReadAccess();
#else
  return true;
#endif
}

bool SharedImageManager::EndBatchReadAccess() {
#if defined(OS_ANDROID)
  return batch_access_manager_->EndBatchReadAccess();
#else
  return true;
#endif
}

}  // namespace gpu
