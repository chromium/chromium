// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FUCHSIA_COMMON_SYSMEM_BUFFER_READER_H_
#define MEDIA_FUCHSIA_COMMON_SYSMEM_BUFFER_READER_H_

#include <fuchsia/media/cpp/fidl.h>
#include <fuchsia/sysmem/cpp/fidl.h>

#include <memory>

#include "base/containers/span.h"

namespace media {

// Helper class to read content from fuchsia::sysmem::BufferCollection.
class SysmemBufferReader {
 public:
  static fuchsia::sysmem::BufferCollectionConstraints GetRecommendedConstraints(
      size_t max_used_output_frames);

  static std::unique_ptr<SysmemBufferReader> Create(
      fuchsia::sysmem::BufferCollectionInfo_2 info);

  explicit SysmemBufferReader(fuchsia::sysmem::BufferCollectionInfo_2 info);
  ~SysmemBufferReader();

  // Read the buffer content at |index| into |data|, starting from |offset|.
  bool Read(size_t index, size_t offset, base::span<uint8_t> data);

 private:
  fuchsia::sysmem::BufferCollectionInfo_2 collection_info_;

  DISALLOW_COPY_AND_ASSIGN(SysmemBufferReader);
};

}  // namespace media

#endif  // MEDIA_FUCHSIA_COMMON_SYSMEM_BUFFER_READER_H_
