/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/platform/wtf/typed_arrays/array_buffer_builder.h"

#include <limits.h>
#include <string.h>
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"

namespace WTF {

TEST(ArrayBufferBuilderTest, Constructor) {
  ArrayBufferBuilder zero_builder(0);
  EXPECT_EQ(0u, zero_builder.ByteLength());
  EXPECT_EQ(0u, zero_builder.Capacity());

  ArrayBufferBuilder small_builder(1024);
  EXPECT_EQ(0u, zero_builder.ByteLength());
  EXPECT_EQ(1024u, small_builder.Capacity());

  ArrayBufferBuilder big_builder(2048);
  EXPECT_EQ(0u, zero_builder.ByteLength());
  EXPECT_EQ(2048u, big_builder.Capacity());
}

TEST(ArrayBufferBuilderTest, Append) {
  const char kData[] = "HelloWorld";
  uint32_t data_size = sizeof(kData) - 1;

  ArrayBufferBuilder builder(2 * data_size);

  EXPECT_EQ(data_size, builder.Append(kData, data_size));
  EXPECT_EQ(data_size, builder.ByteLength());
  EXPECT_EQ(data_size * 2, builder.Capacity());

  EXPECT_EQ(data_size, builder.Append(kData, data_size));
  EXPECT_EQ(data_size * 2, builder.ByteLength());
  EXPECT_EQ(data_size * 2, builder.Capacity());

  EXPECT_EQ(data_size, builder.Append(kData, data_size));
  EXPECT_EQ(data_size * 3, builder.ByteLength());
  EXPECT_GE(builder.Capacity(), data_size * 3);
}

TEST(ArrayBufferBuilderTest, AppendRepeatedly) {
  const char kData[] = "HelloWorld";
  uint32_t data_size = sizeof(kData) - 1;

  ArrayBufferBuilder builder(37);  // Some number coprime with dataSize.

  for (uint32_t i = 1; i < 1000U; ++i) {
    EXPECT_EQ(data_size, builder.Append(kData, data_size));
    EXPECT_EQ(data_size * i, builder.ByteLength());
    EXPECT_GE(builder.Capacity(), data_size * i);
  }
}

TEST(ArrayBufferBuilderTest, DefaultConstructorAndAppendRepeatedly) {
  const char kData[] = "HelloWorld";
  uint32_t data_size = sizeof(kData) - 1;

  ArrayBufferBuilder builder;

  for (uint32_t i = 1; i < 4000U; ++i) {
    EXPECT_EQ(data_size, builder.Append(kData, data_size));
    EXPECT_EQ(data_size * i, builder.ByteLength());
    EXPECT_GE(builder.Capacity(), data_size * i);
  }
}

TEST(ArrayBufferBuilderTest, AppendFixedCapacity) {
  const char kData[] = "HelloWorld";
  uint32_t data_size = sizeof(kData) - 1;

  ArrayBufferBuilder builder(15);
  builder.SetVariableCapacity(false);

  EXPECT_EQ(data_size, builder.Append(kData, data_size));
  EXPECT_EQ(data_size, builder.ByteLength());
  EXPECT_EQ(15u, builder.Capacity());

  EXPECT_EQ(5u, builder.Append(kData, data_size));
  EXPECT_EQ(15u, builder.ByteLength());
  EXPECT_EQ(15u, builder.Capacity());

  EXPECT_EQ(0u, builder.Append(kData, data_size));
  EXPECT_EQ(15u, builder.ByteLength());
  EXPECT_EQ(15u, builder.Capacity());
}

TEST(ArrayBufferBuilderTest, ToArrayBuffer) {
  const char kData1[] = "HelloWorld";
  uint32_t data1_size = sizeof(kData1) - 1;

  const char kData2[] = "GoodbyeWorld";
  uint32_t data2_size = sizeof(kData2) - 1;

  ArrayBufferBuilder builder(1024);
  builder.Append(kData1, data1_size);
  builder.Append(kData2, data2_size);

  const char kExpected[] = "HelloWorldGoodbyeWorld";
  uint32_t expected_size = sizeof(kExpected) - 1;

  scoped_refptr<ArrayBuffer> result = builder.ToArrayBuffer();
  ASSERT_EQ(data1_size + data2_size, result->ByteLength());
  ASSERT_EQ(expected_size, result->ByteLength());
  EXPECT_EQ(0, memcmp(kExpected, result->Data(), expected_size));
}

TEST(ArrayBufferBuilderTest, ToArrayBufferSameAddressIfExactCapacity) {
  const char kData[] = "HelloWorld";
  uint32_t data_size = sizeof(kData) - 1;

  ArrayBufferBuilder builder(data_size);
  builder.Append(kData, data_size);

  scoped_refptr<ArrayBuffer> result1 = builder.ToArrayBuffer();
  scoped_refptr<ArrayBuffer> result2 = builder.ToArrayBuffer();
  EXPECT_EQ(result1.get(), result2.get());
}

TEST(ArrayBufferBuilderTest, ToString) {
  const char kData1[] = "HelloWorld";
  uint32_t data1_size = sizeof(kData1) - 1;

  const char kData2[] = "GoodbyeWorld";
  uint32_t data2_size = sizeof(kData2) - 1;

  ArrayBufferBuilder builder(1024);
  builder.Append(kData1, data1_size);
  builder.Append(kData2, data2_size);

  const char kExpected[] = "HelloWorldGoodbyeWorld";
  uint32_t expected_size = sizeof(kExpected) - 1;

  String result = builder.ToString();
  EXPECT_EQ(expected_size, result.length());
  for (uint32_t i = 0; i < result.length(); ++i)
    EXPECT_EQ(kExpected[i], result[i]);
}

TEST(ArrayBufferBuilderTest, ShrinkToFitNoAppend) {
  ArrayBufferBuilder builder(1024);
  EXPECT_EQ(1024u, builder.Capacity());
  builder.ShrinkToFit();
  EXPECT_EQ(0u, builder.ByteLength());
  EXPECT_EQ(0u, builder.Capacity());
}

TEST(ArrayBufferBuilderTest, ShrinkToFit) {
  const char kData[] = "HelloWorld";
  uint32_t data_size = sizeof(kData) - 1;

  ArrayBufferBuilder builder(32);

  EXPECT_EQ(data_size, builder.Append(kData, data_size));
  EXPECT_EQ(data_size, builder.ByteLength());
  EXPECT_EQ(32u, builder.Capacity());

  builder.ShrinkToFit();
  EXPECT_EQ(data_size, builder.ByteLength());
  EXPECT_EQ(data_size, builder.Capacity());
}

TEST(ArrayBufferBuilderTest, ShrinkToFitFullyUsed) {
  const char kData[] = "HelloWorld";
  uint32_t data_size = sizeof(kData) - 1;

  ArrayBufferBuilder builder(data_size);
  const void* internal_address = builder.Data();

  EXPECT_EQ(data_size, builder.Append(kData, data_size));
  EXPECT_EQ(data_size, builder.ByteLength());
  EXPECT_EQ(data_size, builder.Capacity());

  builder.ShrinkToFit();
  // Reallocation should not happen.
  EXPECT_EQ(internal_address, builder.Data());
  EXPECT_EQ(data_size, builder.ByteLength());
  EXPECT_EQ(data_size, builder.Capacity());
}

TEST(ArrayBufferBuilderTest, ShrinkToFitAfterGrowth) {
  const char kData[] = "HelloWorld";
  uint32_t data_size = sizeof(kData) - 1;

  ArrayBufferBuilder builder(5);

  EXPECT_EQ(data_size, builder.Append(kData, data_size));
  EXPECT_GE(builder.Capacity(), data_size);
  builder.ShrinkToFit();
  EXPECT_EQ(data_size, builder.ByteLength());
  EXPECT_EQ(data_size, builder.Capacity());
}

}  // namespace WTF
