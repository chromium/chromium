// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_TRANSFER_BUFFER_MANAGER_H_
#define GPU_COMMAND_BUFFER_SERVICE_TRANSFER_BUFFER_MANAGER_H_

#include <stddef.h>
#include <stdint.h>

#include "base/compiler_specific.h"
#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/trace_event/memory_dump_provider.h"
#include "gpu/command_buffer/common/command_buffer.h"

namespace gpu {
class MemoryTracker;

class GPU_EXPORT TransferBufferManager
    : public base::trace_event::MemoryDumpProvider {
 public:
  explicit TransferBufferManager(MemoryTracker* memory_tracker);

  TransferBufferManager(const TransferBufferManager&) = delete;
  TransferBufferManager& operator=(const TransferBufferManager&) = delete;

  ~TransferBufferManager() override;

  // Overridden from base::trace_event::MemoryDumpProvider:
  bool OnMemoryDump(const base::trace_event::MemoryDumpArgs& args,
                    base::trace_event::ProcessMemoryDump* pmd) override;

  bool RegisterTransferBuffer(int32_t id, scoped_refptr<Buffer> buffer);
  void DestroyTransferBuffer(int32_t id);
  scoped_refptr<Buffer> GetTransferBuffer(int32_t id);

  size_t shared_memory_bytes_allocated() const {
    return shared_memory_bytes_allocated_;
  }

 private:
  typedef base::flat_map<int32_t, scoped_refptr<Buffer>> BufferMap;
  BufferMap registered_buffers_;
  size_t shared_memory_bytes_allocated_;
  raw_ptr<MemoryTracker, AcrossTasksDanglingUntriaged> memory_tracker_;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_TRANSFER_BUFFER_MANAGER_H_
