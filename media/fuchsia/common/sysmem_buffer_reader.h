// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FUCHSIA_COMMON_SYSMEM_BUFFER_READER_H_
#define MEDIA_FUCHSIA_COMMON_SYSMEM_BUFFER_READER_H_

#include <fuchsia/media/cpp/fidl.h>
#include <fuchsia/sysmem/cpp/fidl.h>

#include <memory>

#include "base/containers/span.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/optional.h"

namespace media {

// Helper class to read content from fuchsia::sysmem::BufferCollection.
class SysmemBufferReader {
 public:
  // Returns sysmem buffer constraints with the specified |min_buffer_count|.
  // Currently it doesn't request buffers for camping or any shared slack, so
  // the clients are expected to read incoming buffers (using Read() or
  // GetMappingForBuffer()) and then release them back to the source.
  static fuchsia::sysmem::BufferCollectionConstraints GetRecommendedConstraints(
      size_t min_buffer_count,
      base::Optional<size_t> min_buffer_size);

  static std::unique_ptr<SysmemBufferReader> Create(
      fuchsia::sysmem::BufferCollectionInfo_2 info);

  explicit SysmemBufferReader(fuchsia::sysmem::BufferCollectionInfo_2 info);
  ~SysmemBufferReader();

  size_t num_buffers() const { return collection_info_.buffer_count; }

  const fuchsia::sysmem::SingleBufferSettings& buffer_settings() {
    return collection_info_.settings;
  }

  // Read the buffer content at |index| into |data|, starting from |offset|.
  bool Read(size_t index, size_t offset, base::span<uint8_t> data);

  // Returns a span for the memory-mapping of the buffer with the specified
  // |index|, invalidating the CPU cache for the specified buffer in the sysmem
  // collection if necessary. Buffers are mapped lazily and remain mapped for
  // the lifetime of SysmemBufferReader. Should be called every time before
  // accessing the mapping to ensure that the CPU cache is invalidated for
  // buffers with RAM coherency.
  base::span<const uint8_t> GetMappingForBuffer(size_t index);

 private:
  // Invalidates CPU cache for the specified range of the specified vmo in
  // case the collection was allocated with RAM coherency. No-op for collections
  // with CPU coherency. Called from Read() and GetMapping() to ensure clients
  // get up-to-date buffer content in case the buffer was updated by other
  // participants directly in RAM (bypassing CPU cache).
  void InvalidateCacheIfNecessary(const zx::vmo& buffer,
                                  size_t offset,
                                  size_t size);

  fuchsia::sysmem::BufferCollectionInfo_2 collection_info_;
  std::vector<base::ReadOnlySharedMemoryMapping> mappings_;

  DISALLOW_COPY_AND_ASSIGN(SysmemBufferReader);
};

}  // namespace media

#endif  // MEDIA_FUCHSIA_COMMON_SYSMEM_BUFFER_READER_H_
