// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FUCHSIA_COMMON_SYSMEM_BUFFER_WRITER_H_
#define MEDIA_FUCHSIA_COMMON_SYSMEM_BUFFER_WRITER_H_

#include <fuchsia/media/cpp/fidl.h>
#include <fuchsia/sysmem/cpp/fidl.h>

#include <memory>

#include "base/containers/span.h"
#include "base/optional.h"

namespace media {

// Helper class to write content into fuchsia::sysmem::BufferCollection.
class SysmemBufferWriter {
 public:
  class Buffer;

  static base::Optional<fuchsia::sysmem::BufferCollectionConstraints>
  GetRecommendedConstraints(
      const fuchsia::media::StreamBufferConstraints& stream_constraints);

  static std::unique_ptr<SysmemBufferWriter> Create(
      fuchsia::sysmem::BufferCollectionInfo_2 info);

  explicit SysmemBufferWriter(std::vector<Buffer> buffers);
  ~SysmemBufferWriter();

  // Write the content of |data| into buffer at |index|. Return num of bytes
  // written into the buffer. Write a used buffer will fail. It will mark the
  // buffer as "used".
  size_t Write(size_t index, base::span<const uint8_t> data);

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

  DISALLOW_COPY_AND_ASSIGN(SysmemBufferWriter);
};

}  // namespace media

#endif  // MEDIA_FUCHSIA_COMMON_SYSMEM_BUFFER_WRITER_H_
