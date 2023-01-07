// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crazy_linker_ashmem.h"

#include <sys/mman.h>

#include <gtest/gtest.h>

namespace crazy {

TEST(AshmemRegion, Construction) {
  AshmemRegion region;
  EXPECT_EQ(-1, region.fd());
}

TEST(AshmemRegion, Allocate) {
  AshmemRegion region;
  const size_t kSize = 4096 * 10;
  EXPECT_TRUE(region.Allocate(kSize, __FUNCTION__));
  void* map = ::mmap(NULL,
                     kSize,
                     PROT_READ | PROT_WRITE,
                     MAP_ANONYMOUS | MAP_SHARED,
                     region.fd(),
                     0);
  EXPECT_NE(MAP_FAILED, map);

  for (size_t n = 0; n < kSize; ++n) {
    EXPECT_EQ(0, ((char*)map)[n]) << "Checking region[" << n << "]";
  }

  EXPECT_EQ(0, ::munmap(map, kSize));
}

}  // namespace crazy
