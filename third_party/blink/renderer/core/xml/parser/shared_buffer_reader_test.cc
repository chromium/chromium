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

#include "third_party/blink/renderer/core/xml/parser/shared_buffer_reader.h"

#include <cstdlib>

#include "base/ranges/algorithm.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"

namespace blink {

TEST(SharedBufferReaderTest, readDataWithNullSharedBuffer) {
  SharedBufferReader reader(nullptr);
  char buffer[32];

  EXPECT_EQ(0u, reader.ReadData(buffer));
}

TEST(SharedBufferReaderTest, readDataWith0BytesRequest) {
  scoped_refptr<SharedBuffer> shared_buffer = SharedBuffer::Create();
  SharedBufferReader reader(shared_buffer);

  EXPECT_EQ(0u, reader.ReadData({}));
}

TEST(SharedBufferReaderTest, readDataWithSizeBiggerThanSharedBufferSize) {
  static const auto kTestData = base::span_with_nul_from_cstring("hello");
  scoped_refptr<SharedBuffer> shared_buffer = SharedBuffer::Create(kTestData);
  SharedBufferReader reader(shared_buffer);

  const int kExtraBytes = 3;
  char output_buffer[kTestData.size() + kExtraBytes];

  const char kInitializationByte = 'a';
  std::ranges::fill(output_buffer, kInitializationByte);

  EXPECT_EQ(kTestData.size(), reader.ReadData(output_buffer));

  EXPECT_EQ(kTestData, base::span(output_buffer).first(kTestData.size()));
  // Check that the bytes past index sizeof(kTestData) were not touched.
  EXPECT_EQ(kExtraBytes,
            base::ranges::count(output_buffer, kInitializationByte));
}

TEST(SharedBufferReaderTest, readDataInMultiples) {
  const int kIterationsCount = 8;
  const int kBytesPerIteration = 64;

  Vector<char> test_data(kIterationsCount * kBytesPerIteration);
  std::generate(test_data.begin(), test_data.end(), &std::rand);

  scoped_refptr<SharedBuffer> shared_buffer = SharedBuffer::Create(test_data);
  SharedBufferReader reader(shared_buffer);

  Vector<char> destination_vector(test_data.size());

  for (int i = 0; i < kIterationsCount; ++i) {
    const int offset = i * kBytesPerIteration;
    const int bytes_read = reader.ReadData(
        base::span(destination_vector).subspan(offset, kBytesPerIteration));
    EXPECT_EQ(kBytesPerIteration, bytes_read);
  }

  EXPECT_TRUE(base::ranges::equal(test_data, destination_vector));
}

TEST(SharedBufferReaderTest, clearSharedBufferBetweenCallsToReadData) {
  Vector<char> test_data(128);
  std::generate(test_data.begin(), test_data.end(), &std::rand);

  scoped_refptr<SharedBuffer> shared_buffer = SharedBuffer::Create(test_data);
  SharedBufferReader reader(shared_buffer);

  Vector<char> destination_vector(test_data.size());
  const size_t bytes_to_read = test_data.size() / 2;
  EXPECT_EQ(
      bytes_to_read,
      reader.ReadData(base::span(destination_vector).first(bytes_to_read)));

  shared_buffer->Clear();

  EXPECT_EQ(
      0u, reader.ReadData(base::span(destination_vector).first(bytes_to_read)));
}

}  // namespace blink
