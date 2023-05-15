// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/scoped_temp_dir.h"
#include "base/memory/ptr_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "gpu/ipc/service/built_in_shader_cache_loader.h"
#include "gpu/ipc/service/built_in_shader_cache_writer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gpu {

TEST(BuiltInShaderCacheTest, Basic) {
  const std::vector<uint8_t> kKey1{1, 2, 3, 4, 5};
  const std::vector<uint8_t> kValue1{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
  const std::vector<uint8_t> kKey2{9, 101, 55};
  std::vector<uint8_t> kValue2(257);
  for (int i = 0; i < 257; ++i) {
    kValue2[i] = static_cast<uint8_t>(i);
  }
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  auto path = temp_dir.GetPath().AppendASCII("shaders");
  {
    BuiltInShaderCacheWriter writer(path);
    writer.OnValueAddedToCache(kKey1, kValue1);
    writer.OnValueAddedToCache(kKey2, kValue2);
  }

  {
    base::HistogramTester histogram_tester;
    BuiltInShaderCacheLoader loader;
    loader.Load(path);
    auto entries = loader.TakeEntriesImpl();
    ASSERT_EQ(2u, entries->size());
    EXPECT_EQ(kKey1, (*entries)[0].key);
    EXPECT_EQ(kValue1, (*entries)[0].value);
    EXPECT_EQ(kKey2, (*entries)[1].key);
    EXPECT_EQ(kValue2, (*entries)[1].value);
  }
}

}  // namespace gpu
