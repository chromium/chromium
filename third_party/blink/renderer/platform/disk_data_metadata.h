// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_DISK_DATA_METADATA_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_DISK_DATA_METADATA_H_

#include <stddef.h>
#include <stdint.h>

namespace blink {

class DiskDataMetadata {
 public:
  int64_t start_offset() const { return start_offset_; }
  size_t size() const { return size_; }

 private:
  DiskDataMetadata(int64_t start_offset, size_t size)
      : start_offset_(start_offset), size_(size) {}
  DiskDataMetadata(const DiskDataMetadata& other) = default;
  DiskDataMetadata(DiskDataMetadata&& other) = default;
  DiskDataMetadata& operator=(const DiskDataMetadata& other) = default;

  int64_t start_offset_;
  size_t size_;

  friend class DiskDataAllocator;
};

}  // namespace blink
#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_DISK_DATA_METADATA_H_
