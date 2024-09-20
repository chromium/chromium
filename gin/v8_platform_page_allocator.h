// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GIN_V8_PLATFORM_PAGE_ALLOCATOR_H_
#define GIN_V8_PLATFORM_PAGE_ALLOCATOR_H_

#include "build/build_config.h"
#include "build/buildflag.h"
#include "partition_alloc/buildflags.h"

#if PA_BUILDFLAG(USE_PARTITION_ALLOC)

#include "gin/gin_export.h"
#include "partition_alloc/page_allocator.h"
#include "v8/include/v8-platform.h"

namespace gin {

// A v8::PageAllocator implementation to use with gin.
class GIN_EXPORT PageAllocator final : public v8::PageAllocator {
 public:
  ~PageAllocator() override;

  size_t AllocatePageSize() override;

  size_t CommitPageSize() override;

  void SetRandomMmapSeed(int64_t seed) override;

  void* GetRandomMmapAddr() override;

  void* AllocatePages(void* address,
                      size_t length,
                      size_t alignment,
                      v8::PageAllocator::Permission permissions) override;

  bool FreePages(void* address, size_t length) override;

  bool ReleasePages(void* address, size_t length, size_t new_length) override;

  bool SetPermissions(void* address,
                      size_t length,
                      Permission permissions) override;

  bool RecommitPages(void* address,
                     size_t length,
                     Permission permissions) override;

  bool DiscardSystemPages(void* address, size_t size) override;

  bool DecommitPages(void* address, size_t size) override;

  bool SealPages(void* address, size_t length) override;

  // For testing purposes only: Map the v8 page permissions into a page
  // configuration from base.
  ::partition_alloc::PageAccessibilityConfiguration::Permissions
  GetPageConfigPermissionsForTesting(v8::PageAllocator::Permission permission);
};
}  // namespace gin

#endif  // PA_BUILDFLAG(USE_PARTITION_ALLOC)

#endif  // GIN_V8_PLATFORM_PAGE_ALLOCATOR_H_
