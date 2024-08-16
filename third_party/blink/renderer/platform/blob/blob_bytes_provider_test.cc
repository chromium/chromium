// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/blob/blob_bytes_provider.h"

#include <memory>
#include <utility>

#include "base/containers/heap_array.h"
#include "base/containers/span.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"

namespace blink {

class BlobBytesProviderTest : public testing::Test {
 public:
  void SetUp() override {
    Platform::SetMainThreadTaskRunnerForTesting();

    test_bytes1_.resize(128);
    for (wtf_size_t i = 0; i < test_bytes1_.size(); ++i)
      test_bytes1_[i] = i % 191;
    test_data1_ = RawData::Create();
    test_data1_->MutableData()->AppendVector(test_bytes1_);
    test_bytes2_.resize(64);
    for (wtf_size_t i = 0; i < test_bytes2_.size(); ++i)
      test_bytes2_[i] = i;
    test_data2_ = RawData::Create();
    test_data2_->MutableData()->AppendVector(test_bytes2_);
    test_bytes3_.resize(32);
    for (wtf_size_t i = 0; i < test_bytes3_.size(); ++i)
      test_bytes3_[i] = (i + 10) % 137;
    test_data3_ = RawData::Create();
    test_data3_->MutableData()->AppendVector(test_bytes3_);

    combined_bytes_.AppendVector(test_bytes1_);
    combined_bytes_.AppendVector(test_bytes2_);
    combined_bytes_.AppendVector(test_bytes3_);
  }

  void TearDown() override {
    task_environment_.RunUntilIdle();
    Platform::UnsetMainThreadTaskRunnerForTesting();
  }

  std::unique_ptr<BlobBytesProvider> CreateProvider(
      scoped_refptr<RawData> data = nullptr) {
    auto result = std::make_unique<BlobBytesProvider>();
    if (data)
      result->AppendData(std::move(data));
    return result;
  }

 protected:
  base::test::TaskEnvironment task_environment_;

  scoped_refptr<RawData> test_data1_;
  Vector<uint8_t> test_bytes1_;
  scoped_refptr<RawData> test_data2_;
  Vector<uint8_t> test_bytes2_;
  scoped_refptr<RawData> test_data3_;
  Vector<uint8_t> test_bytes3_;
  Vector<uint8_t> combined_bytes_;
};

TEST_F(BlobBytesProviderTest, Consolidation) {
  auto data = CreateProvider();
  DCHECK_CALLED_ON_VALID_SEQUENCE(data->sequence_checker_);

  data->AppendData(base::span_from_cstring("abc"));
  data->AppendData(base::span_from_cstring("def"));
  data->AppendData(base::span_from_cstring("ps1"));
  data->AppendData(base::span_from_cstring("ps2"));

  EXPECT_EQ(1u, data->data_.size());
  EXPECT_EQ(12u, data->data_[0]->size());
  EXPECT_EQ(0, memcmp(data->data_[0]->data(), "abcdefps1ps2", 12));

  auto large_data = base::HeapArray<char>::WithSize(
      BlobBytesProvider::kMaxConsolidatedItemSizeInBytes);
  data->AppendData(large_data);

  EXPECT_EQ(2u, data->data_.size());
  EXPECT_EQ(12u, data->data_[0]->size());
  EXPECT_EQ(BlobBytesProvider::kMaxConsolidatedItemSizeInBytes,
            data->data_[1]->size());
}

TEST_F(BlobBytesProviderTest, RequestAsReply) {
  auto provider = CreateProvider(test_data1_);
  Vector<uint8_t> received_bytes;
  provider->RequestAsReply(
      base::BindOnce([](Vector<uint8_t>* bytes_out,
                        const Vector<uint8_t>& bytes) { *bytes_out = bytes; },
                     &received_bytes));
  EXPECT_EQ(test_bytes1_, received_bytes);

  received_bytes.clear();
  provider = CreateProvider();
  provider->AppendData(test_data1_);
  provider->AppendData(test_data2_);
  provider->AppendData(test_data3_);
  provider->RequestAsReply(
      base::BindOnce([](Vector<uint8_t>* bytes_out,
                        const Vector<uint8_t>& bytes) { *bytes_out = bytes; },
                     &received_bytes));
  EXPECT_EQ(combined_bytes_, received_bytes);
}

namespace {

struct FileTestData {
  uint32_t offset;
  uint32_t size;
};

void PrintTo(const FileTestData& test, std::ostream* os) {
  *os << "offset: " << test.offset << ", size: " << test.size;
}

class RequestAsFile : public BlobBytesProviderTest,
                      public testing::WithParamInterface<FileTestData> {
 public:
  void SetUp() override {
    BlobBytesProviderTest::SetUp();
    test_provider_ = CreateProvider();
    test_provider_->AppendData(test_data1_);
    test_provider_->AppendData(test_data2_);
    test_provider_->AppendData(test_data3_);

    auto combined_bytes_span =
        base::span(combined_bytes_).subspan(GetParam().offset, GetParam().size);
    sliced_data_.AppendRange(combined_bytes_span.begin(),
                             combined_bytes_span.end());
  }

  base::File DoRequestAsFile(uint64_t source_offset,
                             uint64_t source_length,
                             uint64_t file_offset) {
    base::FilePath path;
    base::CreateTemporaryFile(&path);
    std::optional<base::Time> received_modified;
    test_provider_->RequestAsFile(
        source_offset, source_length,
        base::File(path, base::File::FLAG_OPEN | base::File::FLAG_WRITE),
        file_offset,
        base::BindOnce(
            [](std::optional<base::Time>* received_modified,
               std::optional<base::Time> modified) {
              *received_modified = modified;
            },
            &received_modified));
    base::File file(path, base::File::FLAG_OPEN | base::File::FLAG_READ |
                              base::File::FLAG_DELETE_ON_CLOSE);
    base::File::Info info;
    EXPECT_TRUE(file.GetInfo(&info));
    EXPECT_EQ(info.last_modified, received_modified);
    return file;
  }

 protected:
  std::unique_ptr<BlobBytesProvider> test_provider_;
  Vector<uint8_t> sliced_data_;
};

TEST_P(RequestAsFile, AtStartOfEmptyFile) {
  FileTestData test = GetParam();
  base::File file = DoRequestAsFile(test.offset, test.size, 0);

  base::File::Info info;
  EXPECT_TRUE(file.GetInfo(&info));
  EXPECT_EQ(static_cast<int64_t>(test.size), info.size);

  Vector<uint8_t> read_data(test.size);
  EXPECT_TRUE(file.ReadAndCheck(0, read_data));
  EXPECT_EQ(sliced_data_, read_data);
}

TEST_P(RequestAsFile, OffsetInEmptyFile) {
  FileTestData test = GetParam();
  int file_offset = 32;
  sliced_data_.InsertVector(0, Vector<uint8_t>(file_offset));

  base::File file = DoRequestAsFile(test.offset, test.size, file_offset);

  base::File::Info info;
  EXPECT_TRUE(file.GetInfo(&info));
  if (test.size == 0) {
    EXPECT_EQ(0, info.size);
  } else {
    EXPECT_EQ(static_cast<int64_t>(test.size) + 32, info.size);

    Vector<uint8_t> read_data(sliced_data_.size());
    EXPECT_TRUE(file.ReadAndCheck(0, read_data));
    EXPECT_EQ(sliced_data_, read_data);
  }
}

TEST_P(RequestAsFile, OffsetInNonEmptyFile) {
  FileTestData test = GetParam();
  int file_offset = 23;

  Vector<uint8_t> expected_data(1024, 42);

  base::FilePath path;
  base::CreateTemporaryFile(&path);
  {
    base::File file(path, base::File::FLAG_OPEN | base::File::FLAG_WRITE);
    EXPECT_TRUE(file.WriteAtCurrentPosAndCheck(expected_data));
  }

  base::span(expected_data).subspan(file_offset).copy_prefix_from(sliced_data_);

  test_provider_->RequestAsFile(
      test.offset, test.size,
      base::File(path, base::File::FLAG_OPEN | base::File::FLAG_WRITE),
      file_offset, base::BindOnce([](std::optional<base::Time> last_modified) {
        EXPECT_TRUE(last_modified);
      }));

  base::File file(path, base::File::FLAG_OPEN | base::File::FLAG_READ |
                            base::File::FLAG_DELETE_ON_CLOSE);
  base::File::Info info;
  EXPECT_TRUE(file.GetInfo(&info));
  EXPECT_EQ(static_cast<int64_t>(expected_data.size()), info.size);

  Vector<uint8_t> read_data(expected_data.size());
  EXPECT_TRUE(file.ReadAndCheck(0, read_data));
  EXPECT_EQ(expected_data, read_data);
}

const FileTestData file_tests[] = {
    {0, 128 + 64 + 32},  // The full amount of data.
    {0, 128 + 64},       // First two chunks.
    {10, 13},            // Just a subset of the first chunk.
    {10, 128},           // Parts of both the first and second chunk.
    {128, 64},           // The entire second chunk.
    {0, 0},              // Zero bytes from the beginning.
    {130, 10},           // Just a subset of the second chunk.
    {140, 0},            // Zero bytes from the middle of the second chunk.
    {10, 128 + 64},      // Parts of all three chunks.
};

INSTANTIATE_TEST_SUITE_P(BlobBytesProviderTest,
                         RequestAsFile,
                         testing::ValuesIn(file_tests));

TEST_F(BlobBytesProviderTest, RequestAsFile_MultipleChunks) {
  auto provider = CreateProvider();
  provider->AppendData(test_data1_);
  provider->AppendData(test_data2_);
  provider->AppendData(test_data3_);

  base::FilePath path;
  base::CreateTemporaryFile(&path);

  Vector<uint8_t> expected_data;
  for (size_t i = 0; i < combined_bytes_.size(); i += 16) {
    provider->RequestAsFile(
        i, 16, base::File(path, base::File::FLAG_OPEN | base::File::FLAG_WRITE),
        combined_bytes_.size() - i - 16,
        base::BindOnce([](std::optional<base::Time> last_modified) {
          EXPECT_TRUE(last_modified);
        }));
    auto combined_bytes_chunk = base::span(combined_bytes_).subspan(i, 16);
    expected_data.insert(0, combined_bytes_chunk.data(),
                         combined_bytes_chunk.size());
  }

  base::File file(path, base::File::FLAG_OPEN | base::File::FLAG_READ |
                            base::File::FLAG_DELETE_ON_CLOSE);
  base::File::Info info;
  EXPECT_TRUE(file.GetInfo(&info));
  EXPECT_EQ(static_cast<int64_t>(combined_bytes_.size()), info.size);

  Vector<uint8_t> read_data(expected_data.size());
  EXPECT_TRUE(file.ReadAndCheck(0, read_data));
  EXPECT_EQ(expected_data, read_data);
}

TEST_F(BlobBytesProviderTest, RequestAsFile_InvaldFile) {
  auto provider = CreateProvider(test_data1_);

  provider->RequestAsFile(
      0, 16, base::File(), 0,
      base::BindOnce([](std::optional<base::Time> last_modified) {
        EXPECT_FALSE(last_modified);
      }));
}

TEST_F(BlobBytesProviderTest, RequestAsFile_UnwritableFile) {
  auto provider = CreateProvider(test_data1_);

  base::FilePath path;
  base::CreateTemporaryFile(&path);
  provider->RequestAsFile(
      0, 16, base::File(path, base::File::FLAG_OPEN | base::File::FLAG_READ), 0,
      base::BindOnce([](std::optional<base::Time> last_modified) {
        EXPECT_FALSE(last_modified);
      }));

  base::File file(path, base::File::FLAG_OPEN | base::File::FLAG_READ |
                            base::File::FLAG_DELETE_ON_CLOSE);
  base::File::Info info;
  EXPECT_TRUE(file.GetInfo(&info));
  EXPECT_EQ(0, info.size);
}

TEST_F(BlobBytesProviderTest, RequestAsStream) {
  auto provider = CreateProvider();
  provider->AppendData(test_data1_);
  provider->AppendData(test_data2_);
  provider->AppendData(test_data3_);

  mojo::ScopedDataPipeProducerHandle producer_handle;
  mojo::ScopedDataPipeConsumerHandle consumer_handle;
  ASSERT_EQ(mojo::CreateDataPipe(7, producer_handle, consumer_handle),
            MOJO_RESULT_OK);
  provider->RequestAsStream(std::move(producer_handle));

  Vector<uint8_t> received_data;
  base::RunLoop loop;
  mojo::SimpleWatcher watcher(
      FROM_HERE, mojo::SimpleWatcher::ArmingPolicy::AUTOMATIC,
      blink::scheduler::GetSequencedTaskRunnerForTesting());
  watcher.Watch(
      consumer_handle.get(), MOJO_HANDLE_SIGNAL_READABLE,
      MOJO_WATCH_CONDITION_SATISFIED,
      base::BindRepeating(
          [](mojo::DataPipeConsumerHandle pipe,
             base::RepeatingClosure quit_closure, Vector<uint8_t>* bytes_out,
             MojoResult result, const mojo::HandleSignalsState& state) {
            if (result == MOJO_RESULT_CANCELLED ||
                result == MOJO_RESULT_FAILED_PRECONDITION) {
              quit_closure.Run();
              return;
            }

            size_t num_bytes = 0;
            MojoResult query_result = pipe.ReadData(
                MOJO_READ_DATA_FLAG_QUERY, base::span<uint8_t>(), num_bytes);
            if (query_result == MOJO_RESULT_SHOULD_WAIT)
              return;
            EXPECT_EQ(MOJO_RESULT_OK, query_result);

            Vector<uint8_t> bytes(num_bytes);
            EXPECT_EQ(
                MOJO_RESULT_OK,
                pipe.ReadData(MOJO_READ_DATA_FLAG_ALL_OR_NONE,
                              base::as_writable_byte_span(bytes), num_bytes));
            bytes_out->AppendVector(bytes);
          },
          consumer_handle.get(), loop.QuitClosure(), &received_data));
  loop.Run();

  EXPECT_EQ(combined_bytes_, received_data);
}

}  // namespace

}  // namespace blink
