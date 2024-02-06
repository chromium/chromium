// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_CLIENT_READBACK_BUFFER_SHADOW_TRACKER_H_
#define GPU_COMMAND_BUFFER_CLIENT_READBACK_BUFFER_SHADOW_TRACKER_H_

#include <GLES2/gl2.h>
#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"

namespace gpu {

class MappedMemoryManager;

namespace gles2 {

class GLES2CmdHelper;

class ReadbackBufferShadowTracker {
 public:
  class Buffer final {
   public:
    explicit Buffer(GLuint buffer_id,
                    MappedMemoryManager* mapped_memory,
                    GLES2CmdHelper* helper);

    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;

    ~Buffer();

    uint32_t Alloc(int32_t* shm_id,
                   uint32_t* shm_offset,
                   bool* already_allocated);
    void Free();
    void FreePendingToken(int32_t token);

    void* MapReadbackShm(uint32_t offset, uint32_t map_size);
    bool UnmapReadbackShm();

    void UpdateSerialTo(uint64_t serial);

    GLuint id() const { return buffer_id_; }

    base::WeakPtr<Buffer> AsWeakPtr() { return weak_ptr_factory_.GetWeakPtr(); }

   private:
    friend class ReadbackBufferShadowTracker;

    GLuint buffer_id_ = 0;
    raw_ptr<MappedMemoryManager> mapped_memory_;
    raw_ptr<GLES2CmdHelper> helper_;
    int32_t shm_id_ = 0;
    uint32_t shm_offset_ = 0;
    raw_ptr<void> readback_shm_address_ = nullptr;
    uint64_t serial_of_last_write_ = 1;  // will be updated right after creation
    uint64_t serial_of_readback_data_ = 0;
    uint32_t size_ = 0;
    bool is_mapped_ = false;
    base::WeakPtrFactory<Buffer> weak_ptr_factory_{this};
  };

  ReadbackBufferShadowTracker(MappedMemoryManager* mapped_memory,
                              GLES2CmdHelper* helper);

  ReadbackBufferShadowTracker(const ReadbackBufferShadowTracker&) = delete;
  ReadbackBufferShadowTracker& operator=(const ReadbackBufferShadowTracker&) =
      delete;

  ~ReadbackBufferShadowTracker();

  Buffer* GetOrCreateBuffer(GLuint id, GLuint size);
  Buffer* GetBuffer(GLuint id) {
    auto it = buffers_.find(id);
    return it != buffers_.end() ? it->second.get() : nullptr;
  }
  // Un-tracks a buffer. Should only be called *after* the glDeleteBuffers
  // command has been issued.
  void RemoveBuffer(GLuint id) { buffers_.erase(id); }

  void OnBufferWrite(GLuint id);

  using BufferList = std::vector<base::WeakPtr<Buffer>>;
  BufferList TakeUnfencedBufferList();
  const BufferList& GetUnfencedBufferList() const {
    return buffers_written_but_not_fenced_;
  }

  uint64_t buffer_shadow_serial() const { return buffer_shadow_serial_; }
  void IncrementSerial() { buffer_shadow_serial_++; }

 private:
  using BufferMap = base::flat_map<GLuint, std::unique_ptr<Buffer>>;
  BufferMap buffers_;
  BufferList buffers_written_but_not_fenced_;
  uint64_t buffer_shadow_serial_ = 1;

  raw_ptr<MappedMemoryManager> mapped_memory_;
  raw_ptr<GLES2CmdHelper> helper_;
};

}  // namespace gles2
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_CLIENT_READBACK_BUFFER_SHADOW_TRACKER_H_
