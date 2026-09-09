// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/fetch/url_loader/vector_data_buffer_factory.h"

#include "services/network/public/cpp/basic_data_buffer_factory.h"
#include "services/network/public/cpp/data_buffer_factory_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {
INSTANTIATE_TYPED_TEST_SUITE_P(VectorDataBufferFactory,
                               DataBufferFactoryTest,
                               blink::VectorDataBufferFactory);
}  // namespace network

namespace blink {

TEST(VectorDataBufferFactoryTest, TakeSegmentedBuffer) {
  auto factory = base::MakeRefCounted<VectorDataBufferFactory>();
  auto buffers = factory->CreateDataBufferList();

  const uint8_t data1[] = {1, 2, 3};
  const uint8_t data2[] = {4, 5};
  const uint8_t data3[] = {6, 7, 8, 9};

  buffers->Append(factory->CreateDataBuffer(data1));
  buffers->Append(factory->CreateDataBuffer(data2));
  buffers->Append(factory->CreateDataBuffer(data3));

  EXPECT_EQ(buffers->GetClassIdentifier(),
            SegmentedDataBufferList::kClassIdentifier);

  auto* segmented_data_buffer_list =
      static_cast<SegmentedDataBufferList*>(buffers.get());
  EXPECT_EQ(segmented_data_buffer_list->GetSegmentedBuffer().GetSegmentCount(),
            3u);
  SegmentedBuffer segmented_buffer =
      segmented_data_buffer_list->TakeSegmentedBuffer();

  EXPECT_EQ(segmented_buffer.GetSegmentCount(), 3u);

  base::span<const char> segment0 = segmented_buffer.GetSegment(0);
  EXPECT_EQ(segment0.size(), 3u);
  EXPECT_EQ(segment0[0], 1);
  EXPECT_EQ(segment0[1], 2);
  EXPECT_EQ(segment0[2], 3);

  base::span<const char> segment1 = segmented_buffer.GetSegment(1);
  EXPECT_EQ(segment1.size(), 2u);
  EXPECT_EQ(segment1[0], 4);
  EXPECT_EQ(segment1[1], 5);

  base::span<const char> segment2 = segmented_buffer.GetSegment(2);
  EXPECT_EQ(segment2.size(), 4u);
  EXPECT_EQ(segment2[0], 6);
  EXPECT_EQ(segment2[1], 7);
  EXPECT_EQ(segment2[2], 8);
  EXPECT_EQ(segment2[3], 9);
}

TEST(VectorDataBufferFactoryTest, AppendForeignDataBuffer) {
  auto vector_factory = base::MakeRefCounted<VectorDataBufferFactory>();
  auto basic_factory = base::MakeRefCounted<network::BasicDataBufferFactory>();

  auto buffers = vector_factory->CreateDataBufferList();

  const uint8_t data[] = {10, 20, 30};
  // Append a BasicDataBuffer, which exercises the fallback copy path in
  // SegmentedDataBufferList::Append().
  buffers->Append(basic_factory->CreateDataBuffer(data));

  EXPECT_EQ(buffers->size(), 1u);
  EXPECT_EQ(buffers->Get(0).size(), 3u);
  EXPECT_EQ(buffers->Get(0)[0], 10);
  EXPECT_EQ(buffers->Get(0)[1], 20);
  EXPECT_EQ(buffers->Get(0)[2], 30);

  auto* segmented_data_buffer_list =
      static_cast<SegmentedDataBufferList*>(buffers.get());
  SegmentedBuffer segmented_buffer =
      segmented_data_buffer_list->TakeSegmentedBuffer();
  EXPECT_EQ(segmented_buffer.GetSegmentCount(), 1u);
  base::span<const char> segment = segmented_buffer.GetSegment(0);
  EXPECT_EQ(base::as_bytes(segment), base::span(data));
}

}  // namespace blink
