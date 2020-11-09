// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FUCHSIA_COMMON_SYSMEM_BUFFER_WRITER_H_
#define MEDIA_FUCHSIA_COMMON_SYSMEM_BUFFER_WRITER_H_

#include <fuchsia/media/cpp/fidl.h>
#include <fuchsia/sysmem/cpp/fidl.h>

#include <memory>

#include "base/containers/span.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/optional.h"

namespace media {

// Helper class to write content into fuchsia::sysmem::BufferCollection.
class SysmemBufferWriter {
 public:
  class Buffer;

  static base::Optional<fuchsia::sysmem::BufferCollectionConstraints>
  GetRecommendedConstraints(size_t min_buffer_count, size_t min_buffer_size);

  static std::unique_ptr<SysmemBufferWriter> Create(
      fuchsia::sysmem::BufferCollectionInfo_2 info);

  explicit SysmemBufferWriter(std::vector<Buffer> buffers);
  ~SysmemBufferWriter();

  SysmemBufferWriter(const SysmemBufferWriter&) = delete;
  SysmemBufferWriter& operator=(const SysmemBufferWriter&) = delete;

  // Write the content of |data| into the buffer at |index|. Return num of bytes
  // written into the buffer. Can be called only for an unused buffer. Marks
  // the buffer as used.
  size_t Write(size_t index, base::span<const uint8_t> data);

  // Returns a span for the memory-mapping of the buffer with the specified
  // |index|. Can be called only for an unused buffer. Marks the buffer as used.
  // Callers must call FlushCache() after they are finished updating the buffer.
  base::span<uint8_t> ReserveAndMapBuffer(size_t index);

  // Flushes CPU cache for specified range in the buffer with the specified
  // |index| in case the buffer collection uses RAM coherency. No-op for
  // collections with RAM coherency.
  void FlushBuffer(size_t index, size_t flush_offset, size_t flush_size);

  // Acquire unused buffer for write. If |min_size| is provided, the returned
  // buffer will have available size larger than |min_size|. This will NOT
  // mark the buffer as "used".
  base::Optional<size_t> Acquire();

  // Notify the pool buffer at |index| is free to write new data.
  void Release(size_t index);

  // Mark all buffers as unused.
  void ReleaseAll();

  size_t num_buffers() const;

 private:
  std::vector<Buffer> buffers_;
};

}  // namespace media

#endif  // MEDIA_FUCHSIA_COMMON_SYSMEM_BUFFER_WRITER_H_
