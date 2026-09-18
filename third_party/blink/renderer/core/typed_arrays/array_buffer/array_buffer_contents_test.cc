// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/typed_arrays/array_buffer/array_buffer_contents.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

class ArrayBufferContentsTest : public testing::Test {};

#if defined(ADDRESS_SANITIZER)
#define DISABLE_ON_ASAN(test_name) DISABLED_##test_name
#else
#define DISABLE_ON_ASAN(test_name) test_name
#endif  // defined(ADDRESS_SANITIZER)

// Disable on ASAN to avoid crashing on failed allocations, see
// https://crbug.com/1038741.
TEST_F(ArrayBufferContentsTest, DISABLE_ON_ASAN(AllocationFail)) {
  // This should be an amount of memory that cannot be allocated.
  size_t length = sizeof(size_t) == 4 ? 0x4fffffff : 0x8000000000;
  size_t element_byte_size = 1;
  ArrayBufferContents content1(length, element_byte_size,
                               blink::ArrayBufferContents::kNotShared,
                               blink::ArrayBufferContents::kDontInitialize);
  ArrayBufferContents content2(length, element_byte_size,
                               blink::ArrayBufferContents::kNotShared,
                               blink::ArrayBufferContents::kDontInitialize);
  // Check that no memory got allocated, and that DataLength is set accordingly.
  ASSERT_EQ(content2.DataLength(), 0u);
  ASSERT_EQ(content2.Data(), nullptr);
}

TEST_F(ArrayBufferContentsTest, CopyPreservesResizability) {
  test::TaskEnvironment task_environment;
  V8TestingScope v8_scope;

  ArrayBufferContents src(16, /*max_num_elements=*/64, 1,
                          ArrayBufferContents::kNotShared,
                          ArrayBufferContents::kDontInitialize);
  ASSERT_TRUE(src.IsValid());
  ASSERT_TRUE(src.IsResizableByUserJavaScript());
  ASSERT_EQ(src.DataLength(), 16u);
  ASSERT_EQ(src.MaxDataLength(), 64u);
  static_cast<uint32_t*>(src.Data())[0] = 0x12345678u;

  ArrayBufferContents dst;
  src.CopyTo(dst);

  ASSERT_TRUE(dst.IsValid());
  EXPECT_NE(src.Data(), dst.Data());
  EXPECT_TRUE(dst.IsResizableByUserJavaScript());
  EXPECT_EQ(dst.DataLength(), 16u);
  EXPECT_EQ(dst.MaxDataLength(), 64u);
  EXPECT_EQ(static_cast<const uint32_t*>(dst.Data())[0], 0x12345678u);
}

}  // namespace blink
