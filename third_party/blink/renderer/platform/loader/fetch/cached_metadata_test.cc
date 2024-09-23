// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/platform/loader/fetch/cached_metadata.h"

#include "base/containers/span.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

const uint32_t kTestDataTypeId = 123;
const uint64_t kTestTag = 456;

const uint8_t kTestData[] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05};

Vector<uint8_t> CreateTestSerializedDataWithMarker(uint32_t marker) {
  Vector<uint8_t> serialized_data;
  serialized_data.ReserveInitialCapacity(sizeof(CachedMetadataHeader) +
                                         sizeof(kTestData));
  serialized_data.Append(reinterpret_cast<const uint8_t*>(&marker),
                         sizeof(uint32_t));
  serialized_data.Append(reinterpret_cast<const uint8_t*>(&kTestDataTypeId),
                         sizeof(uint32_t));
  serialized_data.Append(reinterpret_cast<const uint8_t*>(&kTestTag),
                         sizeof(uint64_t));
  serialized_data.Append(kTestData, sizeof(kTestData));
  return serialized_data;
}

// Creates a test serialized data with the valid marker.
Vector<uint8_t> CreateTestSerializedData() {
  return CreateTestSerializedDataWithMarker(
      CachedMetadataHandler::kSingleEntryWithTag);
}

void CheckTestCachedMetadata(scoped_refptr<CachedMetadata> cached_metadata) {
  ASSERT_TRUE(cached_metadata);
  EXPECT_EQ(cached_metadata->DataTypeID(), kTestDataTypeId);
  EXPECT_THAT(cached_metadata->SerializedData(),
              testing::ElementsAreArray(CreateTestSerializedData()));
  EXPECT_THAT(base::make_span(cached_metadata->Data(), cached_metadata->size()),
              testing::ElementsAreArray(kTestData));
  EXPECT_EQ(cached_metadata->tag(), kTestTag);
  auto drained_data = std::move(*cached_metadata).DrainSerializedData();

  if (absl::holds_alternative<Vector<uint8_t>>(drained_data)) {
    EXPECT_THAT(absl::get<Vector<uint8_t>>(drained_data),
                testing::ElementsAreArray(CreateTestSerializedData()));
    return;
  }
  CHECK(absl::holds_alternative<mojo_base::BigBuffer>(drained_data));
  mojo_base::BigBuffer drained_big_buffer =
      std::move(absl::get<mojo_base::BigBuffer>(drained_data));
  EXPECT_THAT(
      base::make_span(drained_big_buffer.data(), drained_big_buffer.size()),
      testing::ElementsAreArray(CreateTestSerializedData()));
}

TEST(CachedMetadataTest, GetSerializedDataHeader) {
  Vector<uint8_t> header_vector = CachedMetadata::GetSerializedDataHeader(
      kTestDataTypeId, /*estimated_body_size=*/10, kTestTag);
  EXPECT_EQ(header_vector.size(), sizeof(CachedMetadataHeader));

  const CachedMetadataHeader* header =
      reinterpret_cast<const CachedMetadataHeader*>(header_vector.data());
  EXPECT_EQ(header->marker, CachedMetadataHandler::kSingleEntryWithTag);
  EXPECT_EQ(header->type, kTestDataTypeId);
  EXPECT_EQ(header->tag, kTestTag);
}

TEST(CachedMetadataTest, CreateFromBufferWithDataTypeIdAndTag) {
  CheckTestCachedMetadata(CachedMetadata::Create(kTestDataTypeId, kTestData,
                                                 sizeof(kTestData), kTestTag));
}

TEST(CachedMetadataTest, CreateFromSerializedDataBuffer) {
  Vector<uint8_t> data = CreateTestSerializedData();
  CheckTestCachedMetadata(
      CachedMetadata::CreateFromSerializedData(data.data(), data.size()));
}

TEST(CachedMetadataTest, CreateFromSerializedDataVector) {
  Vector<uint8_t> data = CreateTestSerializedData();
  CheckTestCachedMetadata(CachedMetadata::CreateFromSerializedData(data));
}

TEST(CachedMetadataTest, CreateFromSerializedDataBigBuffer) {
  Vector<uint8_t> data = CreateTestSerializedData();
  mojo_base::BigBuffer big_buffer(data);
  CheckTestCachedMetadata(CachedMetadata::CreateFromSerializedData(big_buffer));
  // `big_buffer` must be moved into the created CachedMetadata.
  EXPECT_EQ(big_buffer.size(), 0u);
}

TEST(CachedMetadataTest, CreateFromSerializedDataTooSmall) {
  Vector<uint8_t> data = Vector<uint8_t>(sizeof(CachedMetadataHeader));
  EXPECT_FALSE(
      CachedMetadata::CreateFromSerializedData(data.data(), data.size()));
  EXPECT_FALSE(CachedMetadata::CreateFromSerializedData(data));

  mojo_base::BigBuffer big_buffer(data);
  EXPECT_FALSE(CachedMetadata::CreateFromSerializedData(big_buffer));
  // `big_buffer` must not be moved into the created CachedMetadata.
  EXPECT_EQ(big_buffer.size(), data.size());
}

TEST(CachedMetadataTest, CreateFromSerializedDataWithInvalidMarker) {
  // Creates a test serialized data with an invalid marker.
  Vector<uint8_t> data = CreateTestSerializedDataWithMarker(
      CachedMetadataHandler::kSingleEntryWithTag + 1);
  EXPECT_FALSE(
      CachedMetadata::CreateFromSerializedData(data.data(), data.size()));
  EXPECT_FALSE(CachedMetadata::CreateFromSerializedData(data));

  mojo_base::BigBuffer big_buffer(data);
  EXPECT_FALSE(CachedMetadata::CreateFromSerializedData(big_buffer));
  // `big_buffer` must not be moved into the created CachedMetadata.
  EXPECT_EQ(big_buffer.size(), data.size());
}

}  // namespace
}  // namespace blink
