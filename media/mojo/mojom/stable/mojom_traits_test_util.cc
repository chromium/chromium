// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/mojom/stable/mojom_traits_test_util.h"

#include "base/memory/unsafe_shared_memory_region.h"

namespace media {
base::ScopedFD CreateValidLookingBufferHandle(size_t size) {
  return base::UnsafeSharedMemoryRegion::TakeHandleForSerialization(
             base::UnsafeSharedMemoryRegion::Create(size))
      .PassPlatformHandle()
      .fd;
}
}  // namespace media
