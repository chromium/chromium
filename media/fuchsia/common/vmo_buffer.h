// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FUCHSIA_COMMON_VMO_BUFFER_H_
#define MEDIA_FUCHSIA_COMMON_VMO_BUFFER_H_

#include <fuchsia/media/cpp/fidl.h>
#include <fuchsia/sysmem2/cpp/fidl.h>

#include <memory>
#include <optional>

#include "base/containers/span.h"
#include "base/memory/shared_memory_mapping.h"
#include "media/base/media_export.h"

namespace media {

class MEDIA_EXPORT VmoBuffer {
 public:
  // Returns sysmem buffer constraints to use to ensure that sysmem buffer
  // collection is compatible with this class.
  static fuchsia::sysmem2::BufferCollectionConstraints
  GetRecommendedConstraints(size_t min_buffer_count,
                            std::optional<size_t> min_buffer_size,
                            bool writable);

  // Creates a set of buffers from a sysmem collection. An empty vector is
  // returned in case of a failure.
  static std::vector<VmoBuffer> CreateBuffersFromSysmemCollection(
      fuchsia::sysmem2::BufferCollectionInfo* info,
      bool writable);

  VmoBuffer();
  ~VmoBuffer();

  VmoBuffer(VmoBuffer&&);
  VmoBuffer& operator=(VmoBuffer&&);

  // Initializes the buffer from the specified |vmo|. |writable| should be true
  // for writable buffers. |offset| and |size| are used to specify the portion
  // of the |vmo| that should be used for this buffer. Returns false if it fails
  // to map the buffer.
  [[nodiscard]] bool Initialize(
      zx::vmo vmo,
      bool writable,
      size_t offset,
      size_t size,
      fuchsia::sysmem2::CoherencyDomain coherency_domain);

  size_t size() const { return size_; }

  // Read the buffer content into |data|, starting from |offset|. For buffers
  // with RAM coherency the cache is invalidated prior to read to ensure the
  // data is read from RAM instead of the CPU cache. Returns number of bytes
  // that have been copied.
  size_t Read(size_t offset, base::span<uint8_t> data);

  // Writes |data| to this input buffer. If the |data| is larger than the buffer
  // then writes only size() bytes from the head of the |data|. Returns number
  // of bytes that have been copied.
  size_t Write(base::span<const uint8_t> data);

  // Returns read-only memory corresponding to the buffer. Also invalidates CPU
  // cache for buffers with RAM coherency.
  base::span<const uint8_t> GetMemory();

  // Returns writable memory span. The caller should call Flush() after writing
  // to the buffer in order to ensure that the buffer is flushed in case it uses
  // RAM coherency.
  base::span<uint8_t> GetWritableMemory();

  // Flushes the CPU cache if the buffers uses RAM coherency. No-op for buffers
  // with CPU coherency. If |invalidate| flag is set then the cache is also
  // invalidated.
  void FlushCache(size_t flush_offset, size_t flush_size, bool invalidate);

  // Duplicates VMO.
  zx::vmo Duplicate(bool writable);

 private:
  // Returns size of the mapped region.
  size_t mapped_size();

  zx::vmo vmo_;

  uint8_t* base_address_ = nullptr;

  bool writable_ = false;
  size_t offset_ = 0;
  size_t size_ = 0;
  fuchsia::sysmem2::CoherencyDomain coherency_domain_;
};

}  // namespace media

#endif  // MEDIA_FUCHSIA_COMMON_VMO_BUFFER_H_
