// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/disk_data_metadata.h"

#include "third_party/blink/renderer/platform/disk_data_allocator.h"

namespace blink {

ReservedChunk::ReservedChunk(DiskDataAllocator* allocator,
                             std::unique_ptr<DiskDataMetadata> metadata)
    : allocator_(allocator), metadata_(std::move(metadata)) {}

ReservedChunk::~ReservedChunk() {
  if (metadata_) {
    allocator_->Discard(std::move(metadata_));
  }
}

std::unique_ptr<DiskDataMetadata> ReservedChunk::Take() {
  return std::move(metadata_);
}

}  // namespace blink
