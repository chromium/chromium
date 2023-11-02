// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/plugin_array_buffer_var.h"

#include <stdlib.h>

#include <limits>

#include "ppapi/c/dev/ppb_buffer_dev.h"
#include "ppapi/proxy/plugin_dispatcher.h"
#include "ppapi/proxy/plugin_globals.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/serialized_structs.h"
#include "ppapi/shared_impl/host_resource.h"
#include "ppapi/shared_impl/resource.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_buffer_api.h"

using ppapi::proxy::PluginGlobals;
using ppapi::proxy::PluginResourceTracker;

namespace ppapi {

PluginArrayBufferVar::PluginArrayBufferVar(uint32_t size_in_bytes)
    : buffer_(size_in_bytes),
      size_in_bytes_(size_in_bytes) {}

PluginArrayBufferVar::PluginArrayBufferVar(
    uint32_t size_in_bytes,
    base::UnsafeSharedMemoryRegion plugin_handle)
    : plugin_handle_(std::move(plugin_handle)), size_in_bytes_(size_in_bytes) {}

PluginArrayBufferVar::~PluginArrayBufferVar() = default;

void* PluginArrayBufferVar::Map() {
  if (shmem_.IsValid())
    return shmem_.memory();
  if (plugin_handle_.IsValid()) {
    shmem_ = plugin_handle_.MapAt(0, size_in_bytes_);
    if (!shmem_.IsValid()) {
      return nullptr;
    }
    return shmem_.memory();
  }
  if (buffer_.empty())
    return nullptr;
  return &(buffer_[0]);
}

void PluginArrayBufferVar::Unmap() {
  shmem_ = base::WritableSharedMemoryMapping();
}

uint32_t PluginArrayBufferVar::ByteLength() {
  return size_in_bytes_;
}

bool PluginArrayBufferVar::CopyToNewShmem(
    PP_Instance instance,
    int* host_handle_id,
    base::UnsafeSharedMemoryRegion* plugin_out_handle) {
  ppapi::proxy::PluginDispatcher* dispatcher =
      ppapi::proxy::PluginDispatcher::GetForInstance(instance);
  if (!dispatcher)
    return false;

  ppapi::proxy::SerializedHandle plugin_handle;
  dispatcher->Send(new PpapiHostMsg_SharedMemory_CreateSharedMemory(
      instance, ByteLength(), host_handle_id, &plugin_handle));
  if (!plugin_handle.IsHandleValid() || !plugin_handle.is_shmem_region() ||
      *host_handle_id == -1)
    return false;

  base::UnsafeSharedMemoryRegion tmp_handle =
      base::UnsafeSharedMemoryRegion::Deserialize(
          plugin_handle.TakeSharedMemoryRegion());
  base::WritableSharedMemoryMapping s = tmp_handle.MapAt(0, ByteLength());
  if (!s.IsValid())
    return false;
  memcpy(s.memory(), Map(), ByteLength());

  // We don't need to keep the shared memory around on the plugin side;
  // we've already copied all our data into it. We'll make it invalid
  // just to be safe.
  *plugin_out_handle = base::UnsafeSharedMemoryRegion();

  return true;
}

}  // namespace ppapi
