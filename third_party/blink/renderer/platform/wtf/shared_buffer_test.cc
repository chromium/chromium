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
#include <array>
#include <cstdlib>
#include <memory>

#include "base/containers/heap_array.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/string_view_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

TEST(SegmentedBufferTest, TakeData) {
  static constexpr char kTestData0[] = "Hello";
  static constexpr char kTestData1[] = "World";
  static constexpr char kTestData2[] = "Goodbye";

  const auto span0 = base::span_from_cstring(kTestData0);
  const auto span1 = base::span_from_cstring(kTestData1);
  const auto span2 = base::span_from_cstring(kTestData2);

  SegmentedBuffer buffer;
  buffer.Append(span0);
  buffer.Append(span1);
  buffer.Append(span2);
  Vector<Vector<char>> data = std::move(buffer).TakeData();
  ASSERT_EQ(3U, data.size());
  EXPECT_EQ(base::span(data[0]), span0);
  EXPECT_EQ(base::span(data[1]), span1);
  EXPECT_EQ(base::span(data[2]), span2);
}

TEST(SharedBufferTest, getAsBytes) {
  static constexpr char kTestData0[] = "Hello";
  static constexpr char kTestData1[] = "World";
  static constexpr char kTestData2[] = "Goodbye";

  scoped_refptr<SharedBuffer> shared_buffer =
      SharedBuffer::Create(base::span_from_cstring(kTestData0));
  shared_buffer->Append(base::span_from_cstring(kTestData1));
  shared_buffer->Append(base::span_from_cstring(kTestData2));

  const size_t size = shared_buffer->size();
  auto data = base::HeapArray<uint8_t>::Uninit(size);
  ASSERT_TRUE(shared_buffer->GetBytes(data));

  EXPECT_EQ(base::byte_span_from_cstring("HelloWorldGoodbye"), data.as_span());
}

TEST(SharedBufferTest, getPartAsBytes) {
  static constexpr char kTestData0[] = "Hello";
  static constexpr char kTestData1[] = "World";
  static constexpr char kTestData2[] = "Goodbye";

  scoped_refptr<SharedBuffer> shared_buffer =
      SharedBuffer::Create(base::span_from_cstring(kTestData0));
  shared_buffer->Append(base::span_from_cstring(kTestData1));
  shared_buffer->Append(base::span_from_cstring(kTestData2));

  struct TestData {
    size_t size;
    std::string_view expected;
  } kTestData[] = {
      {17, "HelloWorldGoodbye"},
      {7, "HelloWo"},
      {3, "Hel"},
  };
  for (TestData& test : kTestData) {
    auto data = base::HeapArray<uint8_t>::Uninit(test.size);
    ASSERT_TRUE(shared_buffer->GetBytes(data));
    EXPECT_EQ(test.expected, base::as_string_view(data));
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
  auto data = base::HeapArray<uint8_t>::Uninit(size);
  ASSERT_TRUE(shared_buffer->GetBytes(data));

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

  scoped_refptr<SharedBuffer> shared_buffer = SharedBuffer::Create(test_data);
  shared_buffer->Append(test_data);
  shared_buffer->Append(test_data);
  shared_buffer->Append(test_data);
  // sharedBuffer must contain data more than segmentSize (= 0x1000) to check
  // copy().
  ASSERT_EQ(test_data.size() * 4, shared_buffer->size());

  Vector<char> clone = shared_buffer->CopyAs<Vector<char>>();
  ASSERT_EQ(test_data.size() * 4, clone.size());
  const Vector<char> contiguous = shared_buffer->CopyAs<Vector<char>>();
  ASSERT_EQ(contiguous.size(), shared_buffer->size());
  ASSERT_EQ(clone, contiguous);

  clone.AppendVector(test_data);
  ASSERT_EQ(test_data.size() * 5, clone.size());
}

TEST(SharedBufferTest, constructorWithFlatData) {
  Vector<char> data;

  while (data.size() < 10000ul) {
    data.AppendSpan(base::span_from_cstring("FooBarBaz"));
    auto shared_buffer = SharedBuffer::Create(base::span(data));

    Vector<Vector<char>> segments;
    for (const auto& span : *shared_buffer) {
      segments.emplace_back();
      segments.back().AppendSpan(span);
    }

    // Shared buffers constructed from flat data should stay flat.
    ASSERT_EQ(segments.size(), 1ul);
    ASSERT_EQ(segments.front().size(), data.size());
    EXPECT_EQ(segments.front(), data);
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
    shared_buffer->Append(base::span_from_cstring("FooBarBaz"));
  }
}

TEST(SharedBufferTest, GetIteratorAt) {
  Vector<char> data(300);
  std::generate(data.begin(), data.end(), &std::rand);
  auto [first_segment, second_segment] = base::span(data).split_at(127u);
  auto buffer = SharedBuffer::Create();
  buffer->Append(first_segment);
  buffer->Append(second_segment);

  const auto it0 = buffer->GetIteratorAt(static_cast<size_t>(0));
  EXPECT_EQ(it0, buffer->cbegin());
  ASSERT_NE(it0, buffer->cend());
  ASSERT_EQ(it0->size(), 127u);
  EXPECT_EQ(*it0, first_segment);

  const auto it1 = buffer->GetIteratorAt(static_cast<size_t>(1));
  EXPECT_NE(it1, buffer->cbegin());
  ASSERT_NE(it1, buffer->cend());
  ASSERT_EQ(it1->size(), 126u);
  EXPECT_EQ(*it1, first_segment.subspan(1u));

  const auto it126 = buffer->GetIteratorAt(static_cast<size_t>(126));
  EXPECT_NE(it126, buffer->cbegin());
  ASSERT_NE(it126, buffer->cend());
  ASSERT_EQ(it126->size(), 1u);
  EXPECT_EQ(*it126, first_segment.subspan(126u));

  const auto it127 = buffer->GetIteratorAt(static_cast<size_t>(127));
  EXPECT_NE(it127, buffer->cbegin());
  ASSERT_NE(it127, buffer->cend());
  ASSERT_EQ(it127->size(), second_segment.size());
  EXPECT_EQ(*it127, second_segment);

  const auto it128 = buffer->GetIteratorAt(static_cast<size_t>(128));
  EXPECT_NE(it128, buffer->cbegin());
  ASSERT_NE(it128, buffer->cend());
  ASSERT_EQ(it128->size(), second_segment.size() - 1);
  EXPECT_EQ(*it128, second_segment.subspan(1u));

  const auto it299 = buffer->GetIteratorAt(static_cast<size_t>(299));
  EXPECT_NE(it299, buffer->cbegin());
  ASSERT_NE(it299, buffer->cend());
  ASSERT_EQ(it299->size(), 1u);
  EXPECT_EQ(*it299, second_segment.last(1u));

  // All of the iterators above are different each other.
  const auto iters = std::to_array<SharedBuffer::Iterator>({
      it0,
      it1,
      it126,
      it127,
      it128,
      it299,
  });
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
  auto buffer = SharedBuffer::Create(base::span_from_cstring("hello"));

  EXPECT_EQ(buffer->begin(), buffer->cbegin());
  EXPECT_EQ(buffer->end(), buffer->cend());

  auto it = buffer->cbegin();
  ASSERT_NE(it, buffer->cend());

  EXPECT_EQ(String(base::as_bytes(*it)), "hello");

  ++it;

  EXPECT_EQ(it, buffer->cend());

  it = buffer->GetIteratorAt(static_cast<size_t>(0));
  EXPECT_EQ(String(base::as_bytes(*it)), "hello");

  it = buffer->GetIteratorAt(static_cast<size_t>(1));
  EXPECT_EQ(String(base::as_bytes(*it)), "ello");
  it = buffer->GetIteratorAt(static_cast<size_t>(4));
  EXPECT_EQ(String(base::as_bytes(*it)), "o");
  EXPECT_EQ(buffer->GetIteratorAt(static_cast<size_t>(5)), buffer->cend());
}

}  // namespace blink
