// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CDM_SIMPLE_CDM_ALLOCATOR_H_
#define MEDIA_CDM_SIMPLE_CDM_ALLOCATOR_H_

#include <stddef.h>
#include <stdint.h>
#include <memory>

#include "media/cdm/cdm_allocator.h"

namespace media {

// This is a simple CdmAllocator for testing.
class SimpleCdmAllocator final : public CdmAllocator {
 public:
  SimpleCdmAllocator();

  SimpleCdmAllocator(const SimpleCdmAllocator&) = delete;
  SimpleCdmAllocator& operator=(const SimpleCdmAllocator&) = delete;

  ~SimpleCdmAllocator() override;

  // CdmAllocator implementation.
  cdm::Buffer* CreateCdmBuffer(size_t capacity) override;
  std::unique_ptr<VideoFrameImpl> CreateCdmVideoFrame() override;
};

}  // namespace media

#endif  // MEDIA_CDM_SIMPLE_CDM_ALLOCATOR_H_
