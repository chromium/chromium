// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_DATA_BUFFER_FACTORY_TEST_UTIL_H_
#define SERVICES_NETWORK_PUBLIC_CPP_DATA_BUFFER_FACTORY_TEST_UTIL_H_

#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/test/gtest_util.h"
#include "services/network/public/cpp/data_buffer_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {

// Reusable typed test suite for testing implementations of DataBufferFactory.
// Subclasses instantiate this test suite via INSTANTIATE_TYPED_TEST_SUITE_P.
template <typename T>
class DataBufferFactoryTest : public testing::Test {
 public:
  DataBufferFactoryTest() : factory_(base::MakeRefCounted<T>()) {}
  scoped_refptr<T> factory() { return factory_; }

 private:
  scoped_refptr<T> factory_;
};

TYPED_TEST_SUITE_P(DataBufferFactoryTest);

TYPED_TEST_P(DataBufferFactoryTest, CreateWithEmptySpan) {
  const std::vector<uint8_t> kData;
  auto buffer = this->factory()->CreateDataBuffer(kData);
  EXPECT_TRUE(buffer->data().empty());
  EXPECT_EQ(buffer->data().size(), 0u);
}

TYPED_TEST_P(DataBufferFactoryTest, CreateWithNonEmptySpan) {
  const std::vector<uint8_t> kData = {10, 20, 30};
  auto buffer = this->factory()->CreateDataBuffer(kData);
  EXPECT_EQ(buffer->data().size(), 3u);
  EXPECT_EQ(buffer->data()[0], 10);
  EXPECT_EQ(buffer->data()[2], 30);
}

TYPED_TEST_P(DataBufferFactoryTest, AllocateWithZeroSize) {
  auto buffer = this->factory()->AllocateDataBuffer(0);
  EXPECT_TRUE(buffer->data().empty());
  EXPECT_EQ(buffer->data().size(), 0u);
}

TYPED_TEST_P(DataBufferFactoryTest, AllocateWithNonZeroSize) {
  auto buffer = this->factory()->AllocateDataBuffer(5);
  EXPECT_EQ(buffer->data().size(), 5u);
  for (uint8_t byte : buffer->data()) {
    EXPECT_EQ(byte, 0);
  }
}

TYPED_TEST_P(DataBufferFactoryTest, MutableData) {
  auto buffer = this->factory()->AllocateDataBuffer(3);
  auto span = buffer->data();
  span[0] = 1;
  span[1] = 2;
  span[2] = 3;

  const DataBuffer& const_buffer = *buffer;
  auto const_span = const_buffer.data();
  EXPECT_EQ(const_span[0], 1);
  EXPECT_EQ(const_span[1], 2);
  EXPECT_EQ(const_span[2], 3);
}

TYPED_TEST_P(DataBufferFactoryTest, ShrinkToSameSize) {
  auto buffer = this->factory()->AllocateDataBuffer(10);
  buffer->Shrink(10);
  EXPECT_EQ(buffer->data().size(), 10u);
}

TYPED_TEST_P(DataBufferFactoryTest, ShrinkToSmallerSize) {
  const std::vector<uint8_t> kData = {1, 2, 3, 4, 5};
  auto buffer = this->factory()->CreateDataBuffer(kData);
  buffer->Shrink(3);
  EXPECT_EQ(buffer->data().size(), 3u);
  EXPECT_EQ(buffer->data()[0], 1);
  EXPECT_EQ(buffer->data()[2], 3);
}

TYPED_TEST_P(DataBufferFactoryTest, ShrinkToZero) {
  auto buffer = this->factory()->AllocateDataBuffer(5);
  buffer->Shrink(0);
  EXPECT_TRUE(buffer->data().empty());
}

TYPED_TEST_P(DataBufferFactoryTest, EmptyState) {
  auto buffers = this->factory()->CreateDataBufferList();
  EXPECT_TRUE(buffers->empty());
  EXPECT_EQ(buffers->size(), 0u);
  EXPECT_EQ(buffers->begin(), buffers->end());
}

TYPED_TEST_P(DataBufferFactoryTest, AppendSingleBuffer) {
  auto buffers = this->factory()->CreateDataBufferList();
  const std::vector<uint8_t> kData = {42};
  buffers->Append(this->factory()->CreateDataBuffer(kData));

  EXPECT_FALSE(buffers->empty());
  EXPECT_EQ(buffers->size(), 1u);
  auto span = buffers->Get(0);
  EXPECT_EQ(span.size(), 1u);
  EXPECT_EQ(span[0], 42);
}

TYPED_TEST_P(DataBufferFactoryTest, AppendMultipleBuffers) {
  auto buffers = this->factory()->CreateDataBufferList();
  buffers->Append(
      this->factory()->CreateDataBuffer(std::vector<uint8_t>{1, 2}));
  buffers->Append(this->factory()->CreateDataBuffer(std::vector<uint8_t>{3}));
  buffers->Append(
      this->factory()->CreateDataBuffer(std::vector<uint8_t>{4, 5, 6}));

  EXPECT_EQ(buffers->size(), 3u);

  EXPECT_EQ(buffers->Get(0).size(), 2u);
  EXPECT_EQ(buffers->Get(0)[0], 1);

  EXPECT_EQ(buffers->Get(1).size(), 1u);
  EXPECT_EQ(buffers->Get(1)[0], 3);

  EXPECT_EQ(buffers->Get(2).size(), 3u);
  EXPECT_EQ(buffers->Get(2)[2], 6);
}

TYPED_TEST_P(DataBufferFactoryTest, Iteration) {
  auto buffers = this->factory()->CreateDataBufferList();
  buffers->Append(this->factory()->CreateDataBuffer(std::vector<uint8_t>{10}));
  buffers->Append(
      this->factory()->CreateDataBuffer(std::vector<uint8_t>{20, 30}));

  auto it = buffers->begin();
  ASSERT_NE(it, buffers->end());
  EXPECT_EQ((*it).size(), 1u);
  EXPECT_EQ((*it)[0], 10);

  ++it;
  ASSERT_NE(it, buffers->end());
  EXPECT_EQ((*it).size(), 2u);
  EXPECT_EQ((*it)[0], 20);
  EXPECT_EQ((*it)[1], 30);

  auto prev = it++;
  EXPECT_EQ((*prev).size(), 2u);
  EXPECT_EQ(it, buffers->end());
}

TYPED_TEST_P(DataBufferFactoryTest, IteratorCopyAndAssign) {
  auto buffers = this->factory()->CreateDataBufferList();
  buffers->Append(this->factory()->CreateDataBuffer(std::vector<uint8_t>{99}));

  auto it1 = buffers->begin();
  auto it2 = it1;  // Copy constructor
  EXPECT_EQ(it1, it2);

  auto it3 = buffers->end();
  EXPECT_NE(it1, it3);
  it3 = it1;  // Copy assignment
  EXPECT_EQ(it1, it3);
}

TYPED_TEST_P(DataBufferFactoryTest, ShrinkLargerThanCurrentSize) {
  auto buffer = this->factory()->AllocateDataBuffer(5);
  EXPECT_CHECK_DEATH(buffer->Shrink(10));
}

TYPED_TEST_P(DataBufferFactoryTest, AppendNullBuffer) {
  auto buffers = this->factory()->CreateDataBufferList();
  EXPECT_CHECK_DEATH(buffers->Append(nullptr));
}

TYPED_TEST_P(DataBufferFactoryTest, AppendEmptyBuffer) {
  auto buffers = this->factory()->CreateDataBufferList();
  EXPECT_CHECK_DEATH(buffers->Append(
      this->factory()->CreateDataBuffer(std::vector<uint8_t>{})));
}

TYPED_TEST_P(DataBufferFactoryTest, IteratorDereferenceAtEnd) {
  auto buffers = this->factory()->CreateDataBufferList();
  auto it = buffers->end();
  EXPECT_CHECK_DEATH(*it);
}

REGISTER_TYPED_TEST_SUITE_P(DataBufferFactoryTest,
                            CreateWithEmptySpan,
                            CreateWithNonEmptySpan,
                            AllocateWithZeroSize,
                            AllocateWithNonZeroSize,
                            MutableData,
                            ShrinkToSameSize,
                            ShrinkToSmallerSize,
                            ShrinkToZero,
                            EmptyState,
                            AppendSingleBuffer,
                            AppendMultipleBuffers,
                            Iteration,
                            IteratorCopyAndAssign,
                            ShrinkLargerThanCurrentSize,
                            AppendNullBuffer,
                            AppendEmptyBuffer,
                            IteratorDereferenceAtEnd);

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_DATA_BUFFER_FACTORY_TEST_UTIL_H_
