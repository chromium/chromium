// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/memory_data_source.h"

#include <memory>

#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/numerics/safe_conversions.h"
#include "base/rand_util.h"
#include "base/ranges/algorithm.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

class MemoryDataSourceTest : public ::testing::Test {
 public:
  MemoryDataSourceTest() = default;

  MemoryDataSourceTest(const MemoryDataSourceTest&) = delete;
  MemoryDataSourceTest& operator=(const MemoryDataSourceTest&) = delete;

 protected:
  void Initialize(size_t size) {
    data_.assign(size, 0u);
    base::RandBytes(data_);
    memory_data_source_ =
        std::make_unique<MemoryDataSource>(data_.data(), data_.size());
    EXPECT_EQ(size, GetSize());
  }

  // Reads |size| bytes starting from |position|, expects |expected_read_size|
  // bytes to be read and checks the read data. Expects error when
  // |expected_read_size| is DataSource::kReadError.
  void ReadAndExpect(int64_t position, int size, int expected_read_size) {
    std::vector<uint8_t> data(size < 0 ? 0 : size, 0);

    EXPECT_CALL(*this, ReadCB(expected_read_size));
    memory_data_source_->Read(
        position, size, data.data(),
        base::BindOnce(&MemoryDataSourceTest::ReadCB, base::Unretained(this)));

    if (expected_read_size != DataSource::kReadError) {
      size_t positive_expected_size =
          base::checked_cast<size_t>(expected_read_size);
      EXPECT_TRUE(base::ranges::equal(
          base::span(data_).subspan(position, positive_expected_size),
          base::span(data).first(positive_expected_size)));
    }
  }

  size_t GetSize() {
    int64_t size = 0;
    EXPECT_TRUE(memory_data_source_->GetSize(&size));
    EXPECT_GE(size, 0);
    return size;
  }

  void Stop() { memory_data_source_->Stop(); }

  MOCK_METHOD1(ReadCB, void(int size));

 private:
  std::vector<uint8_t> data_;
  std::unique_ptr<MemoryDataSource> memory_data_source_;
};

TEST_F(MemoryDataSourceTest, EmptySource) {
  Initialize(0);
  ReadAndExpect(0, 0, 0);
}

TEST_F(MemoryDataSourceTest, ReadData) {
  Initialize(128);
  ReadAndExpect(0, 0, 0);
  ReadAndExpect(0, 128, 128);
  ReadAndExpect(12, 64, 64);
  ReadAndExpect(128, 0, 0);
}

TEST_F(MemoryDataSourceTest, ReadData_InvalidPosition) {
  Initialize(128);
  ReadAndExpect(-7, 64, DataSource::kReadError);
  ReadAndExpect(129, 64, DataSource::kReadError);
}

TEST_F(MemoryDataSourceTest, ReadData_InvalidSize) {
  Initialize(128);
  ReadAndExpect(0, -12, DataSource::kReadError);
}

TEST_F(MemoryDataSourceTest, ReadData_PartialRead) {
  Initialize(128);
  ReadAndExpect(0, 129, 128);
  ReadAndExpect(96, 100, 32);
}

// All ReadData() will fail after Stop().
TEST_F(MemoryDataSourceTest, Stop) {
  Initialize(128);
  ReadAndExpect(12, 64, 64);
  Stop();
  ReadAndExpect(12, 64, DataSource::kReadError);
}

// ReadData() doesn't affect GetSize().
TEST_F(MemoryDataSourceTest, GetSize) {
  Initialize(128);
  ReadAndExpect(12, 64, 64);
  EXPECT_EQ(128u, GetSize());
  ReadAndExpect(-7, 64, DataSource::kReadError);
  EXPECT_EQ(128u, GetSize());
}

}  // namespace media
