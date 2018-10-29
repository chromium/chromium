// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image_manager.h"
#include "gpu/command_buffer/service/shared_image_representation.h"

#include <inttypes.h>

#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/process_memory_dump.h"
#include "base/trace_event/trace_event.h"
#include "gpu/command_buffer/common/shared_image_trace_utils.h"
#include "ui/gl/trace_util.h"

namespace gpu {
// Overrides for flat_set lookups:
bool operator<(const gpu::SharedImageManager::BackingAndRefCount& lhs,
               const gpu::SharedImageManager::BackingAndRefCount& rhs) {
  return lhs.backing->mailbox() < rhs.backing->mailbox();
}

bool operator<(const gpu::Mailbox& lhs,
               const gpu::SharedImageManager::BackingAndRefCount& rhs) {
  return lhs < rhs.backing->mailbox();
}

bool operator<(const gpu::SharedImageManager::BackingAndRefCount& lhs,
               const gpu::Mailbox& rhs) {
  return lhs.backing->mailbox() < rhs;
}

SharedImageManager::SharedImageManager() = default;

SharedImageManager::~SharedImageManager() {
  DCHECK(images_.empty());
}

bool SharedImageManager::Register(std::unique_ptr<SharedImageBacking> backing) {
  auto found = images_.find(backing->mailbox());
  if (found != images_.end()) {
    backing->Destroy();
    return false;
  }

  images_.emplace(std::move(backing), 1 /* ref_count */);
  return true;
}

void SharedImageManager::Unregister(const Mailbox& mailbox) {
  auto found = images_.find(mailbox);
  if (found == images_.end()) {
    LOG(ERROR) << "SharedImageManager::Unregister: Trying to unregister a "
                  "non existent mailbox.";
    return;
  }

  found->ref_count--;
  if (found->ref_count == 0) {
    found->backing->Destroy();
    images_.erase(found);
  }
}

void SharedImageManager::OnContextLost(const Mailbox& mailbox) {
  auto found = images_.find(mailbox);
  if (found == images_.end()) {
    LOG(ERROR) << "SharedImageManager::OnContextLost: Trying to mark constext "
                  "lost on a non existent mailbox.";
    return;
  }

  found->backing->OnContextLost();
}

bool SharedImageManager::IsSharedImage(const Mailbox& mailbox) {
  auto found = images_.find(mailbox);
  return found != images_.end();
}

std::unique_ptr<SharedImageRepresentationGLTexture>
SharedImageManager::ProduceGLTexture(const Mailbox& mailbox) {
  auto found = images_.find(mailbox);
  if (found == images_.end()) {
    LOG(ERROR) << "SharedImageManager::ProduceGLTexture: Trying to produce a "
                  "representation from a non-existent mailbox.";
    return nullptr;
  }

  auto representation = found->backing->ProduceGLTexture(this);
  if (!representation) {
    LOG(ERROR) << "SharedImageManager::ProduceGLTexture: Trying to produce a "
                  "representation from an incompatible mailbox.";
    return nullptr;
  }

  // Take a ref. This is released when we destroy the generated representation.
  found->ref_count++;
  return representation;
}

std::unique_ptr<SharedImageRepresentationGLTexturePassthrough>
SharedImageManager::ProduceGLTexturePassthrough(const Mailbox& mailbox) {
  auto found = images_.find(mailbox);
  if (found == images_.end()) {
    LOG(ERROR) << "SharedImageManager::ProduceGLTexturePassthrough: Trying to "
                  "produce a representation from a non-existent mailbox.";
    return nullptr;
  }

  auto representation = found->backing->ProduceGLTexturePassthrough(this);
  if (!representation) {
    LOG(ERROR) << "SharedImageManager::ProduceGLTexturePassthrough: Trying to "
                  "produce a representation from an incompatible mailbox.";
    return nullptr;
  }

  // Take a ref. This is released when we destroy the generated representation.
  found->ref_count++;
  return representation;
}

std::unique_ptr<SharedImageRepresentationSkia> SharedImageManager::ProduceSkia(
    const Mailbox& mailbox) {
  auto found = images_.find(mailbox);
  if (found == images_.end()) {
    LOG(ERROR) << "SharedImageManager::ProduceSkia: Trying to Produce a "
                  "Skia representation from a non-existent mailbox.";
    return nullptr;
  }

  auto representation = found->backing->ProduceSkia(this);
  if (!representation) {
    LOG(ERROR) << "SharedImageManager::ProduceSkia: Trying to produce a "
                  "Skia representation from an incompatible mailbox.";
    return nullptr;
  }

  // Take a ref. This is released when we destroy the generated representation.
  found->ref_count++;
  return representation;
}

void SharedImageManager::OnRepresentationDestroyed(const Mailbox& mailbox) {
  // Just call Unregister, which releases a ref on our backing.
  Unregister(mailbox);
}

void SharedImageManager::OnMemoryDump(const Mailbox& mailbox,
                                      base::trace_event::ProcessMemoryDump* pmd,
                                      int client_id,
                                      uint64_t client_tracing_id) {
  auto found = images_.find(mailbox);
  if (found == images_.end()) {
    LOG(ERROR) << "SharedImageManager::OnMemoryDump: Trying to dump memory for "
                  "a non existent mailbox.";
    return;
  }

  auto* backing = found->backing.get();
  size_t estimated_size = backing->EstimatedSize();
  if (estimated_size == 0)
    return;

  // Unique name in the process.
  std::string dump_name =
      base::StringPrintf("gpu/shared-images/client_0x%" PRIX32 "/mailbox_%s",
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

SharedImageManager::BackingAndRefCount::BackingAndRefCount(
    std::unique_ptr<SharedImageBacking> backing,
    uint32_t ref_count)
    : backing(std::move(backing)), ref_count(ref_count) {}
SharedImageManager::BackingAndRefCount::BackingAndRefCount(
    BackingAndRefCount&& other) = default;
SharedImageManager::BackingAndRefCount& SharedImageManager::BackingAndRefCount::
operator=(BackingAndRefCount&& rhs) = default;
SharedImageManager::BackingAndRefCount::~BackingAndRefCount() = default;

}  // namespace gpu
