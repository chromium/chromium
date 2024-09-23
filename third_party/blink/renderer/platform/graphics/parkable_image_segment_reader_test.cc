// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/platform/graphics/parkable_image.h"

#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/image-decoders/image_decoder_test_helpers.h"
#include "third_party/blink/renderer/platform/image-decoders/segment_reader.h"

namespace blink {
namespace {
const char g_abc[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
const char g_123[] = "1234567890";
}  // namespace

class ParkableImageSegmentReaderTest : public testing::Test {
 private:
  base::test::SingleThreadTaskEnvironment task_env_;
};

// There are also tests for SharedBufferSegmentReader located in
// ./fast_shared_buffer_reader_test.cc

TEST_F(ParkableImageSegmentReaderTest, Empty) {
  auto pi = ParkableImage::Create();
  ASSERT_EQ(pi->size(), 0u);  // ParkableImage is empty when created.

  auto segment_reader = pi->CreateSegmentReader();
  // ParkableImageSegmentReader is also empty when created.
  EXPECT_EQ(segment_reader->size(), 0u);
}

TEST_F(ParkableImageSegmentReaderTest, NonEmpty) {
  auto pi = ParkableImage::Create();
  ASSERT_EQ(pi->size(), 0u);  // ParkableImage is empty when created.

  pi->Append(WTF::SharedBuffer::Create(g_abc, sizeof(g_abc)).get(), 0);
  ASSERT_EQ(pi->size(),
            sizeof(g_abc));  // ParkableImage is larger after Append.

  auto segment_reader = pi->CreateSegmentReader();

  // SegmentReader is the same size as ParkableImage.
  EXPECT_EQ(segment_reader->size(), sizeof(g_abc));
}

// Checks that |size()| returns the correct size, even after modyfying the
// underlying ParkableImage.
TEST_F(ParkableImageSegmentReaderTest, Append) {
  auto pi = ParkableImage::Create();
  ASSERT_EQ(pi->size(), 0u);  // ParkableImage is empty when created.

  const size_t shared_buffer_size = sizeof(g_123) / 2;
  pi->Append(WTF::SharedBuffer::Create(g_123, shared_buffer_size).get(), 0);
  ASSERT_EQ(pi->size(),
            shared_buffer_size);  // ParkableImage is larger after Append.

  auto segment_reader = pi->CreateSegmentReader();
  // ParkableImageSegmentReader is same size as ParkableImage when created.
  EXPECT_EQ(segment_reader->size(), shared_buffer_size);

  pi->Append(WTF::SharedBuffer::Create(g_123, sizeof(g_123)).get(), pi->size());
  ASSERT_EQ(pi->size(),
            sizeof(g_123));  // ParkableImage is larger after Append.

  // SegmentReader is the same size as before.
  EXPECT_EQ(segment_reader->size(), shared_buffer_size);
}

TEST_F(ParkableImageSegmentReaderTest, GetSomeData) {
  const size_t kDataSize = 3.5 * 4096;
  char data[kDataSize];
  PrepareReferenceData(data, kDataSize);

  auto shared_buffer = SharedBuffer::Create();
  auto parkable_image = ParkableImage::Create(kDataSize);
  for (size_t pos = 0; pos < kDataSize; pos += 4096) {
    shared_buffer->Append(data + pos,
                          std::min(static_cast<size_t>(4096), kDataSize - pos));
    parkable_image->Append(shared_buffer.get(), parkable_image->size());
  }

  auto segment_reader = parkable_image->CreateSegmentReader();
  segment_reader->LockData();

  const char* segment;
  size_t position = 0;
  for (size_t length = segment_reader->GetSomeData(segment, position); length;
       length = segment_reader->GetSomeData(segment, position)) {
    ASSERT_EQ(0, memcmp(segment, data + position, length));
    position += length;
  }
  EXPECT_EQ(position, kDataSize);

  segment_reader->UnlockData();
}

TEST_F(ParkableImageSegmentReaderTest, GetAsSkData) {
  const size_t kDataSize = 3.5 * 4096;
  char data[kDataSize];
  PrepareReferenceData(data, kDataSize);

  auto shared_buffer = SharedBuffer::Create();
  auto parkable_image = ParkableImage::Create(kDataSize);
  for (size_t pos = 0; pos < kDataSize; pos += 4096) {
    shared_buffer->Append(data + pos,
                          std::min(static_cast<size_t>(4096), kDataSize - pos));
    parkable_image->Append(shared_buffer.get(), parkable_image->size());
  }

  auto segment_reader = parkable_image->CreateSegmentReader();
  segment_reader->LockData();
  auto sk_data = segment_reader->GetAsSkData();

  const char* segment;
  size_t position = 0;
  for (size_t length = segment_reader->GetSomeData(segment, position); length;
       length = segment_reader->GetSomeData(segment, position)) {
    ASSERT_FALSE(memcmp(segment, sk_data->bytes() + position, length));
    position += length;
  }
  EXPECT_EQ(position, kDataSize);

  segment_reader->UnlockData();
}

TEST_F(ParkableImageSegmentReaderTest, GetAsSkDataLongLived) {
  const size_t kDataSize = 3.5 * 4096;
  char data[kDataSize];
  PrepareReferenceData(data, kDataSize);

  auto shared_buffer = SharedBuffer::Create();
  auto parkable_image = ParkableImage::Create(kDataSize);
  shared_buffer->Append(data, kDataSize);
  parkable_image->Append(shared_buffer.get(), parkable_image->size());

  auto segment_reader = parkable_image->CreateSegmentReader();
  auto sk_data = segment_reader->GetAsSkData();

  // Make it so that |sk_data| is the only reference to the ParkableImage.
  segment_reader = nullptr;
  parkable_image = nullptr;

  EXPECT_FALSE(memcmp(data, sk_data->bytes(), kDataSize));
}

}  // namespace blink
