// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/host_array_buffer_var.h"

#include <stdio.h>
#include <string.h>

#include <memory>

#include "base/memory/shared_memory_mapping.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/process/process_handle.h"
#include "content/common/pepper_file_util.h"
#include "content/renderer/pepper/host_globals.h"
#include "content/renderer/pepper/plugin_module.h"
#include "content/renderer/pepper/renderer_ppapi_host_impl.h"
#include "ppapi/c/pp_instance.h"

using ppapi::ArrayBufferVar;
using blink::WebArrayBuffer;

namespace content {

HostArrayBufferVar::HostArrayBufferVar(uint32_t size_in_bytes)
    : buffer_(WebArrayBuffer::Create(size_in_bytes, 1 /* element_size */)),
      valid_(true) {}

HostArrayBufferVar::HostArrayBufferVar(const WebArrayBuffer& buffer)
    : buffer_(buffer), valid_(true) {}

HostArrayBufferVar::HostArrayBufferVar(
    uint32_t size_in_bytes,
    const base::UnsafeSharedMemoryRegion& region)
    : buffer_(WebArrayBuffer::Create(size_in_bytes, 1 /* element_size */)) {
  base::WritableSharedMemoryMapping shm_mapping =
      region.MapAt(0, size_in_bytes);
  if (shm_mapping.IsValid()) {
    memcpy(buffer_.Data(), shm_mapping.memory(), size_in_bytes);
  }
}

HostArrayBufferVar::~HostArrayBufferVar() {}

void* HostArrayBufferVar::Map() {
  if (!valid_)
    return nullptr;
  return buffer_.Data();
}

void HostArrayBufferVar::Unmap() {
  // We do not used shared memory on the host side. Nothing to do.
}

uint32_t HostArrayBufferVar::ByteLength() {
  return base::checked_cast<uint32_t>(buffer_.ByteLength());
}

bool HostArrayBufferVar::CopyToNewShmem(
    PP_Instance instance,
    int* host_shm_handle_id,
    base::UnsafeSharedMemoryRegion* plugin_shm_region) {
  base::UnsafeSharedMemoryRegion shm =
      base::UnsafeSharedMemoryRegion::Create(ByteLength());
  if (!shm.IsValid())
    return false;

  base::WritableSharedMemoryMapping shm_mapping = shm.MapAt(0, ByteLength());
  if (!shm_mapping.IsValid())
    return false;
  memcpy(shm_mapping.memory(), Map(), ByteLength());

  // Duplicate the handle here; the UnsafeSharedMemoryRegion destructor closes
  // its handle on us.
  HostGlobals* hg = HostGlobals::Get();
  PluginModule* pm = hg->GetModule(hg->GetModuleForInstance(instance));

  *plugin_shm_region =
      pm->renderer_ppapi_host()->ShareUnsafeSharedMemoryRegionWithRemote(shm);
  *host_shm_handle_id = -1;
  return true;
}

}  // namespace content
