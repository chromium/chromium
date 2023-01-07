// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/typed_arrays/array_buffer/array_buffer_contents.h"

#include "testing/gtest/include/gtest/gtest.h"

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

}  // namespace blink
