// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "build/blink_buildflags.h"
#include "mojo/public/cpp/base/shared_memory_mojom_traits.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "mojo/public/mojom/base/shared_memory.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(SharedMemoryMojomTest, ReadOnly) {
  auto region = base::ReadOnlySharedMemoryRegion::Create(64);
  const std::string kTestData = "Hello, world!";
  base::as_writable_chars(base::span(region.mapping))
      .copy_prefix_from(kTestData);

  base::ReadOnlySharedMemoryRegion read_only_out;
  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<
              mojo_base::mojom::ReadOnlySharedMemoryRegion>(region.region,
                                                            read_only_out));
  base::ReadOnlySharedMemoryMapping mapping = read_only_out.Map();
  EXPECT_EQ(base::as_chars(base::span(region.mapping)).first(kTestData.size()),
            kTestData);
}

#if BUILDFLAG(USE_BLINK)
TEST(SharedMemoryMojomTest, Writable) {
  auto region = base::WritableSharedMemoryRegion::Create(64);
  auto mapping = region.Map();
  const std::string kTestData = "Hello, world!";
  base::as_writable_chars(base::span(mapping)).copy_prefix_from(kTestData);

  base::WritableSharedMemoryRegion writable_out;
  ASSERT_TRUE(
      mojo::test::SerializeAndDeserialize<
          mojo_base::mojom::WritableSharedMemoryRegion>(region, writable_out));

  mapping = writable_out.Map();
  EXPECT_EQ(base::as_chars(base::span(mapping)).first(kTestData.size()),
            kTestData);
}

TEST(SharedMemoryMojomTest, UnwrapReadOnlyModeMismatch) {
  auto region = base::WritableSharedMemoryRegion::Create(64);
  auto handle = mojo::WrapWritableSharedMemoryRegion(std::move(region));

  // Try to unwrap it as a ReadOnlySharedMemoryRegion. This should return an
  // invalid region instead of crashing.
  base::ReadOnlySharedMemoryRegion read_only_out =
      mojo::UnwrapReadOnlySharedMemoryRegion(std::move(handle));
  EXPECT_FALSE(read_only_out.IsValid());
}

TEST(SharedMemoryMojomTest, UnwrapWritableModeMismatch) {
  auto region = base::ReadOnlySharedMemoryRegion::Create(64);
  auto handle = mojo::WrapReadOnlySharedMemoryRegion(std::move(region.region));

  // Try to unwrap it as a WritableSharedMemoryRegion. This should return an
  // invalid region instead of crashing.
  base::WritableSharedMemoryRegion writable_out =
      mojo::UnwrapWritableSharedMemoryRegion(std::move(handle));
  EXPECT_FALSE(writable_out.IsValid());
}
#endif  // BUILDFLAG(USE_BLINK)

TEST(SharedMemoryMojomTest, Unsafe) {
  auto region = base::UnsafeSharedMemoryRegion::Create(64);
  auto mapping = region.Map();
  const std::string kTestData = "Hello, world!";
  base::as_writable_chars(base::span(mapping)).copy_prefix_from(kTestData);

  base::UnsafeSharedMemoryRegion unsafe_out;
  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<
              mojo_base::mojom::UnsafeSharedMemoryRegion>(region, unsafe_out));

  mapping = unsafe_out.Map();
  EXPECT_EQ(base::as_chars(base::span(mapping)).first(kTestData.size()),
            kTestData);
}

TEST(SharedMemoryMojomTest, UnwrapUnsafeModeMismatch) {
  auto region = base::ReadOnlySharedMemoryRegion::Create(64);
  auto handle = mojo::WrapReadOnlySharedMemoryRegion(std::move(region.region));

  // Try to unwrap it as an UnsafeSharedMemoryRegion. This should return an
  // invalid region instead of crashing.
  base::UnsafeSharedMemoryRegion unsafe_out =
      mojo::UnwrapUnsafeSharedMemoryRegion(std::move(handle));
  EXPECT_FALSE(unsafe_out.IsValid());
}
