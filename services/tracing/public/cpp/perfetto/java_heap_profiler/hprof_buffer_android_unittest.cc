// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <cstdint>

#include "services/tracing/public/cpp/perfetto/java_heap_profiler/hprof_buffer_android.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace tracing {

TEST(HprofBufferTest, VerifyBasicGetBytes) {
  unsigned char file_data[7]{1, 1, 1, 1, 1, 1, 1};
  HprofBuffer hprof(file_data, 7);
  EXPECT_EQ(hprof.GetOneByte(), 1u);
  EXPECT_EQ(hprof.GetTwoBytes(), 257u);
  EXPECT_EQ(hprof.GetFourBytes(), 16843009u);
  EXPECT_EQ(hprof.HasRemaining(), false);
}

TEST(HprofBufferTest, VerifyBasicGetId) {
  unsigned char file_data[12]{1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
  HprofBuffer hprof(file_data, 12);
  EXPECT_EQ(hprof.GetId(), 16843009u);
  hprof.set_id_size(8);
  EXPECT_EQ(hprof.GetId(), 72340172838076673u);
  EXPECT_EQ(hprof.HasRemaining(), false);
}

TEST(HprofBufferTest, VerifyBasicPositionalMethods) {
  unsigned char file_data[4]{1, 2, 3, 4};
  HprofBuffer hprof(file_data, 4);
  EXPECT_EQ(hprof.GetOneByte(), 1u);
  hprof.Skip(2);
  EXPECT_EQ(hprof.GetOneByte(), 4u);
  EXPECT_EQ(hprof.HasRemaining(), false);

  hprof.set_position(1);
  EXPECT_EQ(hprof.GetOneByte(), 2u);
}
}  // namespace tracing
