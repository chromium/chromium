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

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"

#include <algorithm>
#include <cstdlib>
#include <memory>

#include "base/memory/scoped_refptr.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

TEST(SegmentedBufferTest, TakeData) {
  char test_data0[] = "Hello";
  char test_data1[] = "World";
  char test_data2[] = "Goodbye";

  SegmentedBuffer buffer;
  buffer.Append(test_data0, strlen(test_data0));
  buffer.Append(test_data1, strlen(test_data1));
  buffer.Append(test_data2, strlen(test_data2));
  Vector<Vector<char>> data = std::move(buffer).TakeData();
  ASSERT_EQ(3U, data.size());
  EXPECT_EQ(data[0], base::make_span(test_data0, strlen(test_data0)));
  EXPECT_EQ(data[1], base::make_span(test_data1, strlen(test_data1)));
  EXPECT_EQ(data[2], base::make_span(test_data2, strlen(test_data2)));
}

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
      SharedBuffer::Create(std::move(vector0));
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

  clone.AppendVector(test_data);
  ASSERT_EQ(length * 5, clone.size());
}

TEST(SharedBufferTest, constructorWithFlatData) {
  Vector<char> data;

  while (data.size() < 10000ul) {
    data.Append("FooBarBaz", 9ul);
    auto shared_buffer = SharedBuffer::Create(base::span(data));

    Vector<Vector<char>> segments;
    for (const auto& span : *shared_buffer) {
      segments.emplace_back();
      segments.back().AppendSpan(span);
    }

    // Shared buffers constructed from flat data should stay flat.
    ASSERT_EQ(segments.size(), 1ul);
    ASSERT_EQ(segments.front().size(), data.size());
    EXPECT_EQ(memcmp(segments.front().data(), data.data(), data.size()), 0);
  }
}

TEST(SharedBufferTest, FlatData) {
  auto check_flat_data = [](scoped_refptr<const SharedBuffer> shared_buffer) {
    const SegmentedBuffer::DeprecatedFlatData flat_buffer(shared_buffer.get());

    EXPECT_EQ(shared_buffer->size(), flat_buffer.size());
    size_t offset = 0;
    for (const auto& span : *shared_buffer) {
      EXPECT_EQ(span, base::span(flat_buffer).subspan(offset, span.size()));
      offset += span.size();

      // If the SharedBuffer is not segmented, FlatData doesn't copy any data.
      EXPECT_EQ(span.size() == flat_buffer.size(),
                span.data() == flat_buffer.data());
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
  Vector<char> data(300);
  std::generate(data.begin(), data.end(), &std::rand);
  auto buffer = SharedBuffer::Create();
  const size_t first_segment_size = 127;
  const size_t second_segment_size = data.size() - first_segment_size;
  buffer->Append(data.data(), first_segment_size);
  buffer->Append(data.data() + first_segment_size, second_segment_size);

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
  ASSERT_EQ(it127->size(), second_segment_size);
  EXPECT_EQ(0, memcmp(it127->data(), data.data() + 127, it127->size()));

  const auto it128 = buffer->GetIteratorAt(static_cast<size_t>(128));
  EXPECT_NE(it128, buffer->cbegin());
  ASSERT_NE(it128, buffer->cend());
  ASSERT_EQ(it128->size(), second_segment_size - 1);
  EXPECT_EQ(0, memcmp(it128->data(), data.data() + 128, it128->size()));

  const auto it299 = buffer->GetIteratorAt(static_cast<size_t>(299));
  EXPECT_NE(it299, buffer->cbegin());
  ASSERT_NE(it299, buffer->cend());
  ASSERT_EQ(it299->size(), 1u);
  EXPECT_EQ(0, memcmp(it299->data(), data.data() + 299, it299->size()));

  // All of the iterators above are different each other.
  const SharedBuffer::Iterator iters[] = {
      it0, it1, it126, it127, it128, it299,
  };
  for (size_t i = 0; i < std::size(iters); ++i) {
    for (size_t j = 0; j < std::size(iters); ++j) {
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
  EXPECT_EQ(it, buffer->cend());

  it = it128;
  ++it;
  EXPECT_EQ(it, buffer->cend());

  const auto it300 = buffer->GetIteratorAt(static_cast<size_t>(300));
  EXPECT_EQ(it300, buffer->cend());

  const auto it301 = buffer->GetIteratorAt(static_cast<size_t>(301));
  EXPECT_EQ(it301, buffer->cend());
}

TEST(SharedBufferIteratorTest, Empty) {
  auto buffer = SharedBuffer::Create();

  EXPECT_EQ(buffer->begin(), buffer->end());
  EXPECT_EQ(buffer->cbegin(), buffer->cend());
  EXPECT_EQ(buffer->GetIteratorAt(static_cast<size_t>(0)), buffer->end());
  EXPECT_EQ(buffer->GetIteratorAt(static_cast<size_t>(1)), buffer->end());
}

TEST(SharedBufferIteratorTest, SingleSegment) {
  auto buffer = SharedBuffer::Create("hello", static_cast<size_t>(5));

  EXPECT_EQ(buffer->begin(), buffer->cbegin());
  EXPECT_EQ(buffer->end(), buffer->cend());

  auto it = buffer->cbegin();
  ASSERT_NE(it, buffer->cend());

  EXPECT_EQ(String(it->data(), it->size()), "hello");

  ++it;

  EXPECT_EQ(it, buffer->cend());

  it = buffer->GetIteratorAt(static_cast<size_t>(0));
  EXPECT_EQ(String(it->data(), it->size()), "hello");

  it = buffer->GetIteratorAt(static_cast<size_t>(1));
  EXPECT_EQ(String(it->data(), it->size()), "ello");
  it = buffer->GetIteratorAt(static_cast<size_t>(4));
  EXPECT_EQ(String(it->data(), it->size()), "o");
  EXPECT_EQ(buffer->GetIteratorAt(static_cast<size_t>(5)), buffer->cend());
}

}  // namespace blink
