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

#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"

#include <algorithm>
#include <cstdlib>
#include <memory>
#include "base/memory/scoped_refptr.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

TEST(SharedBufferTest, getAsBytes) {
  char test_data0[] = "Hello";
  char test_data1[] = "World";
  char test_data2[] = "Goodbye";

  scoped_refptr<SharedBuffer> shared_buffer =
      SharedBuffer::Create(test_data0, strlen(test_data0));
  shared_buffer->Append(test_data1, strlen(test_data1));
  shared_buffer->Append(test_data2, strlen(test_data2));

  const size_t size = shared_buffer->size();
  auto data = std::make_unique<char[]>(size);
  ASSERT_TRUE(shared_buffer->GetBytes(data.get(), size));

  char expected_concatenation[] = "HelloWorldGoodbye";
  ASSERT_EQ(strlen(expected_concatenation), size);
  EXPECT_EQ(0, memcmp(expected_concatenation, data.get(),
                      strlen(expected_concatenation)));
}

TEST(SharedBufferTest, getPartAsBytes) {
  char test_data0[] = "Hello";
  char test_data1[] = "World";
  char test_data2[] = "Goodbye";

  scoped_refptr<SharedBuffer> shared_buffer =
      SharedBuffer::Create(test_data0, strlen(test_data0));
  shared_buffer->Append(test_data1, strlen(test_data1));
  shared_buffer->Append(test_data2, strlen(test_data2));

  struct TestData {
    size_t size;
    const char* expected;
  } test_data[] = {
      {17, "HelloWorldGoodbye"}, {7, "HelloWo"}, {3, "Hel"},
  };
  for (TestData& test : test_data) {
    auto data = std::make_unique<char[]>(test.size);
    ASSERT_TRUE(shared_buffer->GetBytes(data.get(), test.size));
    EXPECT_EQ(0, memcmp(test.expected, data.get(), test.size));
  }
}

TEST(SharedBufferTest, getAsBytesLargeSegments) {
  Vector<char> vector0(0x4000);
  for (size_t i = 0; i < vector0.size(); ++i)
    vector0[i] = 'a';
  Vector<char> vector1(0x4000);
  for (size_t i = 0; i < vector1.size(); ++i)
    vector1[i] = 'b';
  Vector<char> vector2(0x4000);
  for (size_t i = 0; i < vector2.size(); ++i)
    vector2[i] = 'c';

  scoped_refptr<SharedBuffer> shared_buffer =
      SharedBuffer::AdoptVector(vector0);
  shared_buffer->Append(vector1);
  shared_buffer->Append(vector2);

  const size_t size = shared_buffer->size();
  auto data = std::make_unique<char[]>(size);
  ASSERT_TRUE(shared_buffer->GetBytes(data.get(), size));

  ASSERT_EQ(0x4000U + 0x4000U + 0x4000U, size);
  int position = 0;
  for (int i = 0; i < 0x4000; ++i) {
    EXPECT_EQ('a', data[position]);
    ++position;
  }
  for (int i = 0; i < 0x4000; ++i) {
    EXPECT_EQ('b', data[position]);
    ++position;
  }
  for (int i = 0; i < 0x4000; ++i) {
    EXPECT_EQ('c', data[position]);
    ++position;
  }
}

TEST(SharedBufferTest, copy) {
  Vector<char> test_data(10000);
  std::generate(test_data.begin(), test_data.end(), &std::rand);

  size_t length = test_data.size();
  scoped_refptr<SharedBuffer> shared_buffer =
      SharedBuffer::Create(test_data.data(), length);
  shared_buffer->Append(test_data.data(), length);
  shared_buffer->Append(test_data.data(), length);
  shared_buffer->Append(test_data.data(), length);
  // sharedBuffer must contain data more than segmentSize (= 0x1000) to check
  // copy().
  ASSERT_EQ(length * 4, shared_buffer->size());

  Vector<char> clone = shared_buffer->CopyAs<Vector<char>>();
  ASSERT_EQ(length * 4, clone.size());
  const Vector<char> contiguous = shared_buffer->CopyAs<Vector<char>>();
  ASSERT_EQ(contiguous.size(), shared_buffer->size());
  ASSERT_EQ(0, memcmp(clone.data(), contiguous.data(), clone.size()));

  clone.Append(test_data.data(), length);
  ASSERT_EQ(length * 5, clone.size());
}

TEST(SharedBufferTest, constructorWithSizeOnly) {
  size_t length = 10000;
  scoped_refptr<SharedBuffer> shared_buffer = SharedBuffer::Create(length);
  ASSERT_EQ(length, shared_buffer->size());

  // The internal flat buffer should have been resized to |length| therefore
  // the buffer consists of one big buffer.
  const auto it = shared_buffer->cbegin();
  ASSERT_NE(it, shared_buffer->cend());
  ASSERT_EQ(length, it->size());
}

TEST(SharedBufferTest, constructorWithFlatData) {
  Vector<char> data;

  while (data.size() < 10000ul) {
    data.Append("FooBarBaz", 9ul);
    auto shared_buffer = SharedBuffer::Create(data.begin(), data.size());

    Vector<Vector<char>> segments;
    for (const auto& span : *shared_buffer) {
      segments.emplace_back();
      segments.back().Append(span.data(), span.size());
    }

    // Shared buffers constructed from flat data should stay flat.
    ASSERT_EQ(segments.size(), 1ul);
    ASSERT_EQ(segments.front().size(), data.size());
    EXPECT_EQ(memcmp(segments.front().begin(), data.begin(), data.size()), 0);
  }
}

TEST(SharedBufferTest, FlatData) {
  auto check_flat_data = [](scoped_refptr<const SharedBuffer> shared_buffer) {
    const SharedBuffer::DeprecatedFlatData flat_buffer(shared_buffer);

    EXPECT_EQ(shared_buffer->size(), flat_buffer.size());
    size_t offset = 0;
    for (const auto& span : *shared_buffer) {
      EXPECT_EQ(memcmp(span.data(), flat_buffer.Data() + offset, span.size()),
                0);
      offset += span.size();

      // If the SharedBuffer is not segmented, FlatData doesn't copy any data.
      EXPECT_EQ(span.size() == flat_buffer.size(),
                span.data() == flat_buffer.Data());
    }
  };

  scoped_refptr<SharedBuffer> shared_buffer = SharedBuffer::Create();

  // Add enough data to hit a couple of segments.
  while (shared_buffer->size() < 10000) {
    check_flat_data(shared_buffer);
    shared_buffer->Append("FooBarBaz", 9u);
  }
}

TEST(SharedBufferTest, GetIteratorAt) {
  Vector<char> data(SharedBuffer::kSegmentSize + 256);
  std::generate(data.begin(), data.end(), &std::rand);
  auto buffer = SharedBuffer::Create();
  buffer->Append(data.data(), static_cast<size_t>(127));
  buffer->Append(data.data() + 127, data.size() - 127);

  const auto it0 = buffer->GetIteratorAt(static_cast<size_t>(0));
  EXPECT_EQ(it0, buffer->cbegin());
  ASSERT_NE(it0, buffer->cend());
  ASSERT_EQ(it0->size(), 127u);
  EXPECT_EQ(0, memcmp(it0->data(), data.data(), it0->size()));

  const auto it1 = buffer->GetIteratorAt(static_cast<size_t>(1));
  EXPECT_NE(it1, buffer->cbegin());
  ASSERT_NE(it1, buffer->cend());
  ASSERT_EQ(it1->size(), 126u);
  EXPECT_EQ(0, memcmp(it1->data(), data.data() + 1, it1->size()));

  const auto it126 = buffer->GetIteratorAt(static_cast<size_t>(126));
  EXPECT_NE(it126, buffer->cbegin());
  ASSERT_NE(it126, buffer->cend());
  ASSERT_EQ(it126->size(), 1u);
  EXPECT_EQ(0, memcmp(it126->data(), data.data() + 126, it126->size()));

  const auto it127 = buffer->GetIteratorAt(static_cast<size_t>(127));
  EXPECT_NE(it127, buffer->cbegin());
  ASSERT_NE(it127, buffer->cend());
  ASSERT_EQ(it127->size(), SharedBuffer::kSegmentSize);
  EXPECT_EQ(0, memcmp(it127->data(), data.data() + 127, it127->size()));

  const auto it128 = buffer->GetIteratorAt(static_cast<size_t>(128));
  EXPECT_NE(it128, buffer->cbegin());
  ASSERT_NE(it128, buffer->cend());
  ASSERT_EQ(it128->size(), SharedBuffer::kSegmentSize - 1);
  EXPECT_EQ(0, memcmp(it128->data(), data.data() + 128, it128->size()));

  const auto it4222 = buffer->GetIteratorAt(static_cast<size_t>(4222));
  EXPECT_NE(it4222, buffer->cbegin());
  ASSERT_NE(it4222, buffer->cend());
  ASSERT_EQ(it4222->size(), 1u);
  EXPECT_EQ(0, memcmp(it4222->data(), data.data() + 4222, it4222->size()));

  const auto it4223 = buffer->GetIteratorAt(static_cast<size_t>(4223));
  EXPECT_NE(it4223, buffer->cbegin());
  ASSERT_NE(it4223, buffer->cend());
  ASSERT_EQ(it4223->size(), 129u);
  EXPECT_EQ(0, memcmp(it4223->data(), data.data() + 4223, it4223->size()));

  const auto it4224 = buffer->GetIteratorAt(static_cast<size_t>(4224));
  EXPECT_NE(it4224, buffer->cbegin());
  ASSERT_NE(it4224, buffer->cend());
  ASSERT_EQ(it4224->size(), 128u);
  EXPECT_EQ(0, memcmp(it4224->data(), data.data() + 4224, it4224->size()));

  const auto it4351 = buffer->GetIteratorAt(static_cast<size_t>(4351));
  EXPECT_NE(it4351, buffer->cbegin());
  ASSERT_NE(it4351, buffer->cend());
  ASSERT_EQ(it4351->size(), 1u);
  EXPECT_EQ(0, memcmp(it4351->data(), data.data() + 4351, it4351->size()));

  // All of the iterators above are different each other.
  const SharedBuffer::Iterator iters[] = {
      it0, it1, it126, it127, it128, it4222, it4223, it4224, it4351,
  };
  for (size_t i = 0; i < base::size(iters); ++i) {
    for (size_t j = 0; j < base::size(iters); ++j) {
      EXPECT_EQ(i == j, iters[i] == iters[j]);
    }
  }

  auto it = it0;
  ++it;
  EXPECT_EQ(it, it127);

  it = it1;
  ++it;
  EXPECT_EQ(it, it127);

  it = it126;
  ++it;
  EXPECT_EQ(it, it127);

  it = it127;
  ++it;
  EXPECT_EQ(it, it4223);

  it = it4222;
  ++it;
  EXPECT_EQ(it, it4223);

  it = it4223;
  ++it;
  EXPECT_EQ(it, buffer->cend());

  it = it4224;
  ++it;
  EXPECT_EQ(it, buffer->cend());

  const auto it4352 = buffer->GetIteratorAt(static_cast<size_t>(4352));
  EXPECT_EQ(it4352, buffer->cend());

  const auto it4353 = buffer->GetIteratorAt(static_cast<size_t>(4353));
  EXPECT_EQ(it4353, buffer->cend());
}

TEST(SharedBufferIteratorTest, Empty) {
  auto buffer = SharedBuffer::Create();

  EXPECT_EQ(buffer->begin(), buffer->end());
  EXPECT_EQ(buffer->cbegin(), buffer->cend());
}

TEST(SharedBufferIteratorTest, ConsecutivePartOnly) {
  auto buffer = SharedBuffer::Create("hello", static_cast<size_t>(5));

  EXPECT_EQ(buffer->begin(), buffer->cbegin());
  EXPECT_EQ(buffer->end(), buffer->cend());

  auto it = buffer->cbegin();
  ASSERT_NE(it, buffer->cend());

  EXPECT_EQ(String(it->data(), it->size()), "hello");

  ++it;

  EXPECT_EQ(it, buffer->cend());
}

TEST(SharedBufferIteratorTest, SegmentedPartOnly) {
  Vector<char> data(SharedBuffer::kSegmentSize * 2 + 256);
  std::generate(data.begin(), data.end(), &std::rand);
  auto buffer = SharedBuffer::Create();
  buffer->Append(data);

  EXPECT_EQ(buffer->begin(), buffer->cbegin());
  EXPECT_EQ(buffer->end(), buffer->cend());

  auto it = buffer->cbegin();
  ASSERT_NE(it, buffer->cend());

  ASSERT_EQ(it->size(), SharedBuffer::kSegmentSize);
  EXPECT_EQ(0, memcmp(data.data(), it->data(), it->size()));

  ++it;
  ASSERT_NE(it, buffer->cend());
  ASSERT_EQ(it->size(), SharedBuffer::kSegmentSize);
  EXPECT_EQ(0, memcmp(data.data() + SharedBuffer::kSegmentSize, it->data(),
                      it->size()));

  ++it;
  ASSERT_NE(it, buffer->cend());
  ASSERT_EQ(it->size(), 256u);
  EXPECT_EQ(0, memcmp(data.data() + 2 * SharedBuffer::kSegmentSize, it->data(),
                      it->size()));

  ++it;
  EXPECT_EQ(it, buffer->cend());
}

TEST(SharedBufferIteratorTest, ConsecutivePartAndSegmentedPart) {
  Vector<char> data(SharedBuffer::kSegmentSize * 2 + 256);
  std::generate(data.begin(), data.end(), &std::rand);
  auto buffer = SharedBuffer::Create();
  buffer->Append(data.data(), static_cast<size_t>(128));
  buffer->Append(data.data() + 128, data.size() - 128);

  EXPECT_EQ(buffer->begin(), buffer->cbegin());
  EXPECT_EQ(buffer->end(), buffer->cend());

  auto it = buffer->cbegin();
  ASSERT_NE(it, buffer->cend());

  ASSERT_EQ(it->size(), 128u);
  EXPECT_EQ(0, memcmp(data.data(), it->data(), it->size()));

  ++it;
  ASSERT_NE(it, buffer->cend());
  ASSERT_EQ(it->size(), SharedBuffer::kSegmentSize);
  EXPECT_EQ(0, memcmp(data.data() + 128, it->data(), it->size()));

  ++it;
  ASSERT_NE(it, buffer->cend());
  ASSERT_EQ(it->size(), SharedBuffer::kSegmentSize);
  EXPECT_EQ(0, memcmp(data.data() + 128 + SharedBuffer::kSegmentSize,
                      it->data(), it->size()));

  ++it;
  ASSERT_NE(it, buffer->cend());
  ASSERT_EQ(it->size(), 128u);
  EXPECT_EQ(0, memcmp(data.data() + 128 + 2 * SharedBuffer::kSegmentSize,
                      it->data(), it->size()));

  ++it;
  EXPECT_EQ(it, buffer->cend());
}

}  // namespace blink
