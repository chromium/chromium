// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_PLUGIN_ARRAY_BUFFER_VAR_H_
#define PPAPI_PROXY_PLUGIN_ARRAY_BUFFER_VAR_H_

#include <memory>
#include <vector>

#include "base/memory/shared_memory_mapping.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_stdint.h"
#include "ppapi/shared_impl/var.h"

namespace ppapi {

// Represents a plugin-side ArrayBufferVar. In the plugin process, it's
// owned as a vector.
class PluginArrayBufferVar : public ArrayBufferVar {
 public:
  explicit PluginArrayBufferVar(uint32_t size_in_bytes);
  PluginArrayBufferVar(uint32_t size_in_bytes,
                       base::UnsafeSharedMemoryRegion plugin_handle);

  PluginArrayBufferVar(const PluginArrayBufferVar&) = delete;
  PluginArrayBufferVar& operator=(const PluginArrayBufferVar&) = delete;

  ~PluginArrayBufferVar() override;

  // ArrayBufferVar implementation.
  void* Map() override;
  void Unmap() override;
  uint32_t ByteLength() override;
  bool CopyToNewShmem(PP_Instance instance,
                      int* host_handle,
                      base::UnsafeSharedMemoryRegion* plugin_handle) override;

 private:
  // Non-shared memory
  std::vector<uint8_t> buffer_;

  // Shared memory
  base::UnsafeSharedMemoryRegion plugin_handle_;
  base::WritableSharedMemoryMapping shmem_;
  uint32_t size_in_bytes_;
};

}  // namespace ppapi

#endif  // PPAPI_PROXY_PLUGIN_ARRAY_BUFFER_VAR_H_
