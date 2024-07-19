// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "services/tracing/public/cpp/perfetto/java_heap_profiler/hprof_buffer_android.h"

#include <stddef.h>
#include <cstdint>

#include "testing/gtest/include/gtest/gtest.h"

namespace tracing {

TEST(HprofBufferTest, VerifyBasicGetBytes) {
  const int length = 7;
  unsigned char file_data[length]{1, 1, 1, 1, 1, 1, 1};
  HprofBuffer hprof(file_data, length);
  EXPECT_EQ(hprof.GetOneByte(), 1u);
  EXPECT_EQ(hprof.GetTwoBytes(), 257u);
  EXPECT_EQ(hprof.GetFourBytes(), 16843009u);
  EXPECT_EQ(hprof.HasRemaining(), false);
}

TEST(HprofBufferTest, VerifyBasicGetId) {
  const int length = 12;
  unsigned char file_data[length]{1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
  HprofBuffer hprof(file_data, length);
  EXPECT_EQ(hprof.GetId(), 16843009u);
  hprof.set_id_size(8);
  EXPECT_EQ(hprof.GetId(), 72340172838076673u);
  EXPECT_EQ(hprof.HasRemaining(), false);
}

TEST(HprofBufferTest, VerifyBasicPositionalMethods) {
  const int length = 4;
  unsigned char file_data[length]{1, 2, 3, 4};
  HprofBuffer hprof(file_data, length);
  EXPECT_EQ(hprof.GetOneByte(), 1u);
  hprof.Skip(2);
  EXPECT_EQ(hprof.GetOneByte(), 4u);
  EXPECT_EQ(hprof.HasRemaining(), false);

  hprof.set_position(1);
  EXPECT_EQ(hprof.GetOneByte(), 2u);
}

TEST(HprofBufferTest, VerifySkipIdAndDataPositionMethods) {
  const int length = 12;
  unsigned char file_data[length]{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
  HprofBuffer hprof(file_data, length);
  EXPECT_EQ(static_cast<unsigned>(*hprof.DataPosition()), 1u);
  hprof.SkipId();
  EXPECT_EQ(static_cast<unsigned>(*hprof.DataPosition()), 5u);
  hprof.set_id_size(8);
  hprof.SkipId();
  EXPECT_EQ(hprof.HasRemaining(), false);
}

TEST(HprofBufferTest, VerifySizeOfTypeMethod) {
  const int length = 34;
  unsigned char file_data[length]{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                                  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                                  0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  HprofBuffer hprof(file_data, length);
  unsigned correct_sizes[] = {4, 4, 4, 4, 1, 2, 4, 8, 1, 2, 4, 8};
  for (uint32_t i = 0; i < 12; i++) {
    EXPECT_EQ(hprof.SizeOfType(i), correct_sizes[i]);
  }

  DataType data_types[9]{DataType::OBJECT, DataType::BOOLEAN, DataType::CHAR,
                         DataType::FLOAT,  DataType::DOUBLE,  DataType::BYTE,
                         DataType::SHORT,  DataType::INT,     DataType::LONG};

  size_t correct_offsets[] = {4, 5, 7, 11, 19, 20, 22, 26, 34};
  for (uint32_t i = 0; i < 9; i++) {
    hprof.SkipBytesByType(data_types[i]);
    EXPECT_EQ(hprof.offset(), correct_offsets[i]);
  }

  EXPECT_EQ(hprof.HasRemaining(), false);
}

}  // namespace tracing
