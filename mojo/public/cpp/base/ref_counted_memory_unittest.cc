// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/base/big_buffer_mojom_traits.h"
#include "mojo/public/cpp/base/ref_counted_memory_mojom_traits.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "mojo/public/mojom/base/ref_counted_memory.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo_base {

TEST(RefCountedMemoryTest, Data) {
  uint8_t data[] = {'a', 'b', 'c', 'd', 'e'};

  scoped_refptr<base::RefCountedMemory> in =
      new base::RefCountedStaticMemory(&data, std::size(data));

  scoped_refptr<base::RefCountedMemory> out;
  ASSERT_TRUE(
      mojo::test::SerializeAndDeserialize<mojom::RefCountedMemory>(in, out));
  ASSERT_EQ(out->size(), in->size());
  for (size_t i = 0; i < out->size(); ++i)
    EXPECT_EQ(in->front()[i], out->front()[i]);
}

TEST(RefCountedMemoryTest, Null) {
  // Stuff real data in out to ensure it gets overwritten with a null.
  uint8_t data[] = {'a', 'b', 'c', 'd', 'e'};
  scoped_refptr<base::RefCountedMemory> out =
      new base::RefCountedStaticMemory(&data, std::size(data));

  scoped_refptr<base::RefCountedMemory> in;
  ASSERT_TRUE(
      mojo::test::SerializeAndDeserialize<mojom::RefCountedMemory>(in, out));

  ASSERT_FALSE(out.get());
}

}  // namespace mojo_base
