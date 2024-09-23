/*
 * Copyright (C) 2014 Google Inc. All rights reserved.
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

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/core/xml/parser/shared_buffer_reader.h"

#include <cstdlib>

#include "base/ranges/algorithm.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"

namespace blink {

TEST(SharedBufferReaderTest, readDataWithNullSharedBuffer) {
  SharedBufferReader reader(nullptr);
  char buffer[32];

  EXPECT_EQ(0, reader.ReadData(buffer, sizeof(buffer)));
}

TEST(SharedBufferReaderTest, readDataWith0BytesRequest) {
  scoped_refptr<SharedBuffer> shared_buffer = SharedBuffer::Create();
  SharedBufferReader reader(shared_buffer);

  EXPECT_EQ(0, reader.ReadData(nullptr, 0));
}

TEST(SharedBufferReaderTest, readDataWithSizeBiggerThanSharedBufferSize) {
  static const char kTestData[] = "hello";
  scoped_refptr<SharedBuffer> shared_buffer =
      SharedBuffer::Create(kTestData, sizeof(kTestData));

  SharedBufferReader reader(shared_buffer);

  const int kExtraBytes = 3;
  char output_buffer[sizeof(kTestData) + kExtraBytes];

  const char kInitializationByte = 'a';
  memset(output_buffer, kInitializationByte, sizeof(output_buffer));
  EXPECT_EQ(sizeof(kTestData), static_cast<size_t>(reader.ReadData(
                                   output_buffer, sizeof(output_buffer))));

  EXPECT_TRUE(
      std::equal(kTestData, kTestData + sizeof(kTestData), output_buffer));
  // Check that the bytes past index sizeof(testData) were not touched.
  EXPECT_EQ(kExtraBytes,
            base::ranges::count(output_buffer, kInitializationByte));
}

TEST(SharedBufferReaderTest, readDataInMultiples) {
  const int kIterationsCount = 8;
  const int kBytesPerIteration = 64;

  Vector<char> test_data(kIterationsCount * kBytesPerIteration);
  std::generate(test_data.begin(), test_data.end(), &std::rand);

  scoped_refptr<SharedBuffer> shared_buffer =
      SharedBuffer::Create(&test_data[0], test_data.size());
  SharedBufferReader reader(shared_buffer);

  Vector<char> destination_vector(test_data.size());

  for (int i = 0; i < kIterationsCount; ++i) {
    const int offset = i * kBytesPerIteration;
    const int bytes_read =
        reader.ReadData(&destination_vector[0] + offset, kBytesPerIteration);
    EXPECT_EQ(kBytesPerIteration, bytes_read);
  }

  EXPECT_TRUE(base::ranges::equal(test_data, destination_vector));
}

TEST(SharedBufferReaderTest, clearSharedBufferBetweenCallsToReadData) {
  Vector<char> test_data(128);
  std::generate(test_data.begin(), test_data.end(), &std::rand);

  scoped_refptr<SharedBuffer> shared_buffer =
      SharedBuffer::Create(&test_data[0], test_data.size());
  SharedBufferReader reader(shared_buffer);

  Vector<char> destination_vector(test_data.size());
  const int bytes_to_read = test_data.size() / 2;
  EXPECT_EQ(bytes_to_read,
            reader.ReadData(&destination_vector[0], bytes_to_read));

  shared_buffer->Clear();

  EXPECT_EQ(0, reader.ReadData(&destination_vector[0], bytes_to_read));
}

}  // namespace blink
