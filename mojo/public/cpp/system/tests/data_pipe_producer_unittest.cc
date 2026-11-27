// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/system/data_pipe_producer.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <string_view>

#include "base/containers/span.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/string_view_util.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/public/cpp/system/file_data_source.h"
#include "mojo/public/cpp/system/filtered_data_source.h"
#include "mojo/public/cpp/system/simple_watcher.h"
#include "mojo/public/cpp/system/string_data_source.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace {

// Test helper. Reads a consumer handle, accumulating data into a string. Reads
// until encountering an error (e.g. peer closure), at which point it invokes an
// async callback.
class DataPipeReader {
 public:
  explicit DataPipeReader(ScopedDataPipeConsumerHandle consumer_handle,
                          size_t read_size,
                          base::OnceClosure on_read_done)
      : consumer_handle_(std::move(consumer_handle)),
        read_size_(read_size),
        on_read_done_(std::move(on_read_done)),
        watcher_(FROM_HERE,
                 SimpleWatcher::ArmingPolicy::AUTOMATIC,
                 base::SequencedTaskRunner::GetCurrentDefault()) {
    watcher_.Watch(consumer_handle_.get(), MOJO_HANDLE_SIGNAL_READABLE,
                   MOJO_WATCH_CONDITION_SATISFIED,
                   base::BindRepeating(&DataPipeReader::OnDataAvailable,
                                       base::Unretained(this)));
  }

  DataPipeReader(const DataPipeReader&) = delete;
  DataPipeReader& operator=(const DataPipeReader&) = delete;

  ~DataPipeReader() = default;

  const std::string& data() const { return data_; }

 private:
  void OnDataAvailable(MojoResult result, const HandleSignalsState& state) {
    if (result == MOJO_RESULT_OK) {
      size_t size = read_size_;
      std::string buffer(size, '\0');
      MojoResult read_result;
      do {
        read_result = consumer_handle_->ReadData(
            MOJO_READ_DATA_FLAG_NONE, base::as_writable_byte_span(buffer),
            size);
        if (read_result == MOJO_RESULT_OK) {
          data_.append(base::as_string_view(base::span(buffer).first(size)));
        }
      } while (read_result == MOJO_RESULT_OK);

      if (read_result == MOJO_RESULT_SHOULD_WAIT) {
        return;
      }
    }

    if (result != MOJO_RESULT_CANCELLED) {
      watcher_.Cancel();
    }

    std::move(on_read_done_).Run();
  }

  ScopedDataPipeConsumerHandle consumer_handle_;
  const size_t read_size_;
  base::OnceClosure on_read_done_;
  SimpleWatcher watcher_;
  std::string data_;
};

class DataPipeProducerTest : public testing::Test {
 public:
  DataPipeProducerTest() { CHECK(temp_dir_.CreateUniqueTempDir()); }

  DataPipeProducerTest(const DataPipeProducerTest&) = delete;
  DataPipeProducerTest& operator=(const DataPipeProducerTest&) = delete;

  ~DataPipeProducerTest() override = default;

 protected:
  base::FilePath CreateTempFileWithContents(const std::string& contents) {
    base::FilePath temp_file_path = temp_dir_.GetPath().AppendASCII(
        base::StringPrintf("tmp%d", tmp_file_id_++));
    base::File temp_file(temp_file_path,
                         base::File::FLAG_CREATE | base::File::FLAG_WRITE);
    CHECK(temp_file.WriteAtCurrentPosAndCheck(base::as_byte_span(contents)));
    return temp_file_path;
  }

  static void WriteFromFileThenCloseWriter(
      std::unique_ptr<DataPipeProducer> producer,
      std::unique_ptr<FilteredDataSource::Filter> filter,
      base::File file) {
    DataPipeProducer* raw_producer = producer.get();
    raw_producer->Write(
        std::make_unique<FilteredDataSource>(
            std::make_unique<FileDataSource>(std::move(file)),
            std::move(filter)),
        base::BindOnce([](std::unique_ptr<DataPipeProducer> producer,
                          MojoResult result) {},
                       std::move(producer)));
  }

  static void WriteFromFileThenCloseWriter(
      std::unique_ptr<DataPipeProducer> producer,
      std::unique_ptr<FilteredDataSource::Filter> filter,
      base::File file,
      uint64_t max_bytes) {
    DataPipeProducer* raw_producer = producer.get();
    auto data_source = std::make_unique<FileDataSource>(std::move(file));
    data_source->SetRange(0u, max_bytes);
    raw_producer->Write(
        std::make_unique<FilteredDataSource>(std::move(data_source),
                                             std::move(filter)),
        base::BindOnce([](std::unique_ptr<DataPipeProducer> producer,
                          MojoResult result) {},
                       std::move(producer)));
  }

 private:
  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
  int tmp_file_id_ = 0;
};

struct DataPipeObserverData {
  int num_read_errors = 0;
  uint64_t bytes_read = 0;
  int done_called = 0;
};

class TestObserver : public FilteredDataSource::Filter {
 public:
  explicit TestObserver(DataPipeObserverData* observer_data)
      : observer_data_(observer_data) {}

  TestObserver(const TestObserver&) = delete;
  TestObserver& operator=(const TestObserver&) = delete;

  // FilteredDataSource::Filter:
  void OnRead(base::span<char> buffer,
              FilteredDataSource::ReadResult* result) override {
    base::AutoLock auto_lock(lock_);
    if (result->result == MOJO_RESULT_OK) {
      observer_data_->bytes_read += result->bytes_read;
    } else {
      observer_data_->num_read_errors++;
    }
  }

  void OnDone() override {
    base::AutoLock auto_lock(lock_);
    observer_data_->done_called++;
  }

 private:
  raw_ptr<DataPipeObserverData> observer_data_;
  // Observer may be called on any sequence.
  base::Lock lock_;
};

TEST_F(DataPipeProducerTest, WriteFromFile) {
  const std::string kTestStringFragment = "Hello, world!";
  constexpr size_t kNumRepetitions = 1000;
  std::string test_string;
  for (size_t i = 0; i < kNumRepetitions; ++i) {
    test_string += kTestStringFragment;
  }

  base::FilePath path = CreateTempFileWithContents(test_string);

  base::RunLoop loop;
  ScopedDataPipeProducerHandle producer_handle;
  ScopedDataPipeConsumerHandle consumer_handle;
  ASSERT_EQ(CreateDataPipe(16, producer_handle, consumer_handle),
            MOJO_RESULT_OK);
  DataPipeReader reader(std::move(consumer_handle), 16, loop.QuitClosure());

  base::File file(path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  DataPipeObserverData observer_data;
  auto observer = std::make_unique<TestObserver>(&observer_data);
  WriteFromFileThenCloseWriter(
      std::make_unique<DataPipeProducer>(std::move(producer_handle)),
      std::move(observer), std::move(file));
  loop.Run();

  EXPECT_EQ(test_string, reader.data());
  EXPECT_EQ(0, observer_data.num_read_errors);
  EXPECT_EQ(test_string.size(), observer_data.bytes_read);
  EXPECT_EQ(1, observer_data.done_called);
}

TEST_F(DataPipeProducerTest, WriteFromFilePartial) {
  const std::string kTestString = "abcdefghijklmnopqrstuvwxyz";
  base::FilePath path = CreateTempFileWithContents(kTestString);
  constexpr size_t kBytesToWrite = 7;

  base::RunLoop loop;
  ScopedDataPipeProducerHandle producer_handle;
  ScopedDataPipeConsumerHandle consumer_handle;
  ASSERT_EQ(CreateDataPipe(static_cast<uint32_t>(kTestString.size()),
                           producer_handle, consumer_handle),
            MOJO_RESULT_OK);
  DataPipeReader reader(std::move(consumer_handle), kTestString.size(),
                        loop.QuitClosure());

  base::File file(path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  DataPipeObserverData observer_data;
  auto observer = std::make_unique<TestObserver>(&observer_data);
  WriteFromFileThenCloseWriter(
      std::make_unique<DataPipeProducer>(std::move(producer_handle)),
      std::move(observer), std::move(file), kBytesToWrite);
  loop.Run();

  EXPECT_EQ(kTestString.substr(0, kBytesToWrite), reader.data());
  EXPECT_EQ(0, observer_data.num_read_errors);
  EXPECT_EQ(kBytesToWrite, observer_data.bytes_read);
  EXPECT_EQ(1, observer_data.done_called);
}

TEST_F(DataPipeProducerTest, WriteFromInvalidFile) {
  base::FilePath path(FILE_PATH_LITERAL("<nonexistent-file>"));
  constexpr size_t kBytesToWrite = 7;

  base::RunLoop loop;
  ScopedDataPipeProducerHandle producer_handle;
  ScopedDataPipeConsumerHandle consumer_handle;
  ASSERT_EQ(CreateDataPipe(kBytesToWrite, producer_handle, consumer_handle),
            MOJO_RESULT_OK);
  DataPipeObserverData observer_data;
  auto observer = std::make_unique<TestObserver>(&observer_data);
  DataPipeReader reader(std::move(consumer_handle), kBytesToWrite,
                        loop.QuitClosure());

  base::File file(path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  WriteFromFileThenCloseWriter(
      std::make_unique<DataPipeProducer>(std::move(producer_handle)),
      std::move(observer), std::move(file), kBytesToWrite);
  loop.Run();

  EXPECT_EQ(0UL, reader.data().size());
  EXPECT_EQ(1, observer_data.num_read_errors);
  EXPECT_EQ(0UL, observer_data.bytes_read);
  EXPECT_EQ(1, observer_data.done_called);
}

TEST_F(DataPipeProducerTest, TinyFile) {
  const std::string kTestString = ".";
  base::FilePath path = CreateTempFileWithContents(kTestString);
  base::RunLoop loop;
  ScopedDataPipeProducerHandle producer_handle;
  ScopedDataPipeConsumerHandle consumer_handle;
  ASSERT_EQ(CreateDataPipe(16, producer_handle, consumer_handle),
            MOJO_RESULT_OK);
  DataPipeReader reader(std::move(consumer_handle), 16, loop.QuitClosure());
  DataPipeObserverData observer_data;
  auto observer = std::make_unique<TestObserver>(&observer_data);
  base::File file(path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  WriteFromFileThenCloseWriter(
      std::make_unique<DataPipeProducer>(std::move(producer_handle)),
      std::move(observer), std::move(file));
  loop.Run();

  EXPECT_EQ(kTestString, reader.data());
  EXPECT_EQ(0, observer_data.num_read_errors);
  EXPECT_EQ(kTestString.size(), observer_data.bytes_read);
  EXPECT_EQ(1, observer_data.done_called);
}

TEST_F(DataPipeProducerTest, HugeFile) {
  // We want a file size that is many times larger than the data pipe size.
  // 5MB is large enough, while being small enough to fit in a typical tmpfs.
  constexpr size_t kHugeFileSize = 5 * 1024 * 1024;
  constexpr uint32_t kDataPipeSize = 512 * 1024;

  std::string test_string(kHugeFileSize, 'a');
  for (size_t i = 0; i + 3 < test_string.size(); i += 4) {
    test_string[i + 1] = 'b';
    test_string[i + 2] = 'c';
    test_string[i + 3] = 'd';
  }
  base::FilePath path = CreateTempFileWithContents(test_string);

  base::RunLoop loop;
  ScopedDataPipeProducerHandle producer_handle;
  ScopedDataPipeConsumerHandle consumer_handle;
  ASSERT_EQ(CreateDataPipe(kDataPipeSize, producer_handle, consumer_handle),
            MOJO_RESULT_OK);
  DataPipeReader reader(std::move(consumer_handle), kDataPipeSize,
                        loop.QuitClosure());

  DataPipeObserverData observer_data;
  auto observer = std::make_unique<TestObserver>(&observer_data);
  base::File file(path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  WriteFromFileThenCloseWriter(
      std::make_unique<DataPipeProducer>(std::move(producer_handle)),
      std::move(observer), std::move(file));
  loop.Run();

  EXPECT_EQ(test_string, reader.data());
  EXPECT_EQ(0, observer_data.num_read_errors);
  EXPECT_EQ(kHugeFileSize, observer_data.bytes_read);
  EXPECT_EQ(1, observer_data.done_called);
}

// Simulate abnormal situations, such as changing the file size between
// obtaining the file size and actually reading the file
TEST_F(DataPipeProducerTest, WriteLengthGreaterThanFile) {
  const std::string kTestStringFragment = "Hello, world!";
  constexpr size_t kNumRepetitions = 10;
  std::string test_string;
  for (size_t i = 0; i < kNumRepetitions; ++i) {
    test_string += kTestStringFragment;
  }

  uint64_t file_size = kNumRepetitions * kTestStringFragment.size() *
                       sizeof(kTestStringFragment[0]);
  uint64_t write_file_size = file_size + 10;
  base::FilePath path = CreateTempFileWithContents(test_string);

  base::RunLoop loop;
  ScopedDataPipeProducerHandle producer_handle;
  ScopedDataPipeConsumerHandle consumer_handle;
  ASSERT_EQ(CreateDataPipe(16, producer_handle, consumer_handle),
            MOJO_RESULT_OK);
  DataPipeReader reader(std::move(consumer_handle), 16, loop.QuitClosure());

  base::File file(path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  DataPipeObserverData observer_data;
  auto observer = std::make_unique<TestObserver>(&observer_data);
  WriteFromFileThenCloseWriter(
      std::make_unique<DataPipeProducer>(std::move(producer_handle)),
      std::move(observer), std::move(file), write_file_size);
  loop.Run();

  EXPECT_EQ(test_string, reader.data());
  EXPECT_EQ(0, observer_data.num_read_errors);
  EXPECT_EQ(test_string.size(), observer_data.bytes_read);
  EXPECT_EQ(1, observer_data.done_called);
}

class TruncatingFilter : public FilteredDataSource::Filter {
 public:
  TruncatingFilter(uintptr_t pipe_begin,
                   uintptr_t pipe_end,
                   size_t max_total_bytes,
                   std::string* observed,
                   bool* all_buffers_outside_pipe)
      : pipe_begin_(pipe_begin),
        pipe_end_(pipe_end),
        max_total_bytes_(max_total_bytes),
        observed_(observed),
        all_buffers_outside_pipe_(all_buffers_outside_pipe) {}

  void OnRead(base::span<char> buffer,
              FilteredDataSource::ReadResult* result) override {
    const uintptr_t addr = reinterpret_cast<uintptr_t>(buffer.data());
    *all_buffers_outside_pipe_ =
        *all_buffers_outside_pipe_ && (addr < pipe_begin_ || addr >= pipe_end_);
    if (result->result != MOJO_RESULT_OK || result->bytes_read == 0) {
      return;
    }
    if (emitted_bytes_ == 0) {
      observed_->append(base::as_string_view(
          buffer.first(static_cast<size_t>(result->bytes_read))));
    }
    const uint64_t remaining = max_total_bytes_ - emitted_bytes_;
    result->bytes_read = std::min<uint64_t>(result->bytes_read, remaining);
    emitted_bytes_ += static_cast<size_t>(result->bytes_read);
  }

  void OnDone() override {}

 private:
  const uintptr_t pipe_begin_;
  const uintptr_t pipe_end_;
  const size_t max_total_bytes_;
  size_t emitted_bytes_ = 0;
  raw_ptr<std::string> observed_;
  raw_ptr<bool> all_buffers_outside_pipe_;
};

TEST_F(DataPipeProducerTest, BuffersReadsBeforeWritingToPipe) {
  const std::string kInput = "0123456789abcdef";
  constexpr size_t kTruncatedLength = 10;

  ScopedDataPipeProducerHandle producer_handle;
  ScopedDataPipeConsumerHandle consumer_handle;
  ASSERT_EQ(MOJO_RESULT_OK,
            CreateDataPipe(64, producer_handle, consumer_handle));

  // Pre-fill the pipe's backing ring buffer with 'x' without advancing the
  // write cursor so we can verify truncated bytes never touch pipe memory.
  base::span<uint8_t> raw_pipe_span;
  ASSERT_EQ(MOJO_RESULT_OK,
            producer_handle->BeginWriteData(DataPipeProducerHandle::kNoSizeHint,
                                            MOJO_BEGIN_WRITE_DATA_FLAG_NONE,
                                            raw_pipe_span));
  std::ranges::fill(raw_pipe_span, static_cast<uint8_t>('x'));
  const uintptr_t pipe_begin =
      reinterpret_cast<uintptr_t>(raw_pipe_span.data());
  const uintptr_t pipe_end = pipe_begin + raw_pipe_span.size();
  ASSERT_EQ(MOJO_RESULT_OK, producer_handle->EndWriteData(0));

  base::RunLoop loop;
  DataPipeReader reader(std::move(consumer_handle), 64, loop.QuitClosure());

  std::string observed_by_filter;
  bool all_buffers_outside_pipe = true;
  bool unwritten_pipe_bytes_untouched = false;
  auto producer =
      std::make_unique<DataPipeProducer>(std::move(producer_handle));
  DataPipeProducer* raw_producer = producer.get();
  raw_producer->Write(
      std::make_unique<FilteredDataSource>(
          std::make_unique<StringDataSource>(
              kInput, StringDataSource::AsyncWritingMode::
                          STRING_STAYS_VALID_UNTIL_COMPLETION),
          std::make_unique<TruncatingFilter>(
              pipe_begin, pipe_end, kTruncatedLength, &observed_by_filter,
              &all_buffers_outside_pipe)),
      base::BindOnce(
          [](std::unique_ptr<DataPipeProducer> producer,
             size_t expected_unwritten_len, bool* untouched_out,
             MojoResult result) {
            EXPECT_EQ(MOJO_RESULT_OK, result);
            base::span<uint8_t> trailing_span;
            if (producer->GetProducerHandle().BeginWriteData(
                    DataPipeProducerHandle::kNoSizeHint,
                    MOJO_BEGIN_WRITE_DATA_FLAG_NONE,
                    trailing_span) == MOJO_RESULT_OK) {
              *untouched_out = std::ranges::all_of(
                  trailing_span.first(expected_unwritten_len),
                  [](uint8_t b) { return b == static_cast<uint8_t>('x'); });
              producer->GetProducerHandle().EndWriteData(0);
            }
          },
          std::move(producer), kInput.size() - kTruncatedLength,
          &unwritten_pipe_bytes_untouched));
  loop.Run();

  EXPECT_TRUE(all_buffers_outside_pipe);
  EXPECT_TRUE(unwritten_pipe_bytes_untouched);
  EXPECT_EQ(kInput, observed_by_filter);
  EXPECT_EQ(std::string_view(kInput).substr(0, kTruncatedLength),
            reader.data());
}

class AbortingFilter : public FilteredDataSource::Filter {
 public:
  void OnRead(base::span<char> buffer,
              FilteredDataSource::ReadResult* result) override {
    if (result->result == MOJO_RESULT_OK) {
      result->result = MOJO_RESULT_ABORTED;
    }
  }

  void OnDone() override {}
};

TEST_F(DataPipeProducerTest, AbortedReadCommitsZeroBytesToPipe) {
  ScopedDataPipeProducerHandle producer_handle;
  ScopedDataPipeConsumerHandle consumer_handle;
  ASSERT_EQ(MOJO_RESULT_OK,
            CreateDataPipe(64, producer_handle, consumer_handle));

  // Pre-populate and drain the ring buffer so unwritten pipe memory is
  // non-zero.
  const std::string kPriorData(32, 'x');
  size_t written = 0;
  ASSERT_EQ(MOJO_RESULT_OK, producer_handle->WriteData(
                                base::as_byte_span(kPriorData),
                                MOJO_WRITE_DATA_FLAG_ALL_OR_NONE, written));
  std::string drained(kPriorData.size(), '\0');
  size_t read_bytes = 0;
  ASSERT_EQ(MOJO_RESULT_OK,
            consumer_handle->ReadData(MOJO_READ_DATA_FLAG_ALL_OR_NONE,
                                      base::as_writable_byte_span(drained),
                                      read_bytes));
  ASSERT_EQ(kPriorData, drained);

  base::RunLoop loop;
  DataPipeReader reader(std::move(consumer_handle), 64, loop.QuitClosure());

  MojoResult completion_result = MOJO_RESULT_OK;
  auto producer =
      std::make_unique<DataPipeProducer>(std::move(producer_handle));
  DataPipeProducer* raw_producer = producer.get();
  raw_producer->Write(
      std::make_unique<FilteredDataSource>(
          std::make_unique<StringDataSource>(
              std::string_view("0123456789abcdef"),
              StringDataSource::AsyncWritingMode::
                  STRING_MAY_BE_INVALIDATED_BEFORE_COMPLETION),
          std::make_unique<AbortingFilter>()),
      base::BindOnce(
          [](std::unique_ptr<DataPipeProducer>, MojoResult* out_result,
             MojoResult result) { *out_result = result; },
          std::move(producer), &completion_result));
  loop.Run();

  EXPECT_EQ(MOJO_RESULT_ABORTED, completion_result);
  EXPECT_TRUE(reader.data().empty());
}

TEST_F(DataPipeProducerTest, StreamsMultiChunkPayload) {
  // Exceeds DataPipeProducer's 64 KiB intermediate buffer cap across multiple
  // transfers.
  constexpr size_t kPayloadSize = 150 * 1024;
  std::string expected(kPayloadSize, '\0');
  for (size_t i = 0; i < kPayloadSize; ++i) {
    expected[i] = static_cast<char>('a' + (i % 26));
  }

  ScopedDataPipeProducerHandle producer_handle;
  ScopedDataPipeConsumerHandle consumer_handle;
  ASSERT_EQ(MOJO_RESULT_OK,
            CreateDataPipe(128 * 1024, producer_handle, consumer_handle));

  base::RunLoop loop;
  DataPipeReader reader(std::move(consumer_handle), 32 * 1024,
                        loop.QuitClosure());

  DataPipeObserverData observer_data;
  auto producer =
      std::make_unique<DataPipeProducer>(std::move(producer_handle));
  DataPipeProducer* raw_producer = producer.get();
  raw_producer->Write(
      std::make_unique<FilteredDataSource>(
          std::make_unique<StringDataSource>(
              expected, StringDataSource::AsyncWritingMode::
                            STRING_STAYS_VALID_UNTIL_COMPLETION),
          std::make_unique<TestObserver>(&observer_data)),
      base::BindOnce(
          [](std::unique_ptr<DataPipeProducer>, MojoResult result) {
            EXPECT_EQ(MOJO_RESULT_OK, result);
          },
          std::move(producer)));
  loop.Run();

  EXPECT_EQ(expected, reader.data());
  EXPECT_EQ(0, observer_data.num_read_errors);
  EXPECT_EQ(expected.size(), observer_data.bytes_read);
  EXPECT_EQ(1, observer_data.done_called);
}

class BufferRecordingDataSource : public DataPipeProducer::DataSource {
 public:
  BufferRecordingDataSource(base::span<const char> data,
                            uintptr_t expected_pipe_begin,
                            bool* matched_pipe_ptr,
                            int* read_calls)
      : inner_(data,
               StringDataSource::AsyncWritingMode::
                   STRING_STAYS_VALID_UNTIL_COMPLETION),
        expected_pipe_begin_(expected_pipe_begin),
        matched_pipe_ptr_(matched_pipe_ptr),
        read_calls_(read_calls) {}

  uint64_t GetLength() const override {
    const DataSource& source = inner_;
    return source.GetLength();
  }

  bool MayInspectReadBuffer() const override { return false; }

  ReadResult Read(uint64_t offset, base::span<char> buffer) override {
    DataSource& source = inner_;
    ReadResult result = source.Read(offset, buffer);
    if (result.result == MOJO_RESULT_OK && result.bytes_read > 0) {
      ++(*read_calls_);
      *matched_pipe_ptr_ =
          (reinterpret_cast<uintptr_t>(buffer.data()) == expected_pipe_begin_);
    }
    return result;
  }

 private:
  StringDataSource inner_;
  const uintptr_t expected_pipe_begin_;
  raw_ptr<bool> matched_pipe_ptr_;
  raw_ptr<int> read_calls_;
};

TEST_F(DataPipeProducerTest, UnfilteredDataSourceReadsDirectlyIntoPipe) {
  // Verify that DataSources with `MayInspectReadBuffer() == false` (including
  // `FilteredDataSource` with a null filter) write directly into the pipe
  // buffer without 64 KiB clamping even when `kStageDataPipeProducerReads` is
  // enabled.
  constexpr size_t kPayloadSize = 70 * 1024;
  std::string expected(kPayloadSize, 'y');

  ScopedDataPipeProducerHandle producer_handle;
  ScopedDataPipeConsumerHandle consumer_handle;
  ASSERT_EQ(MOJO_RESULT_OK,
            CreateDataPipe(128 * 1024, producer_handle, consumer_handle));

  base::span<uint8_t> raw_pipe_span;
  ASSERT_EQ(MOJO_RESULT_OK,
            producer_handle->BeginWriteData(DataPipeProducerHandle::kNoSizeHint,
                                            MOJO_BEGIN_WRITE_DATA_FLAG_NONE,
                                            raw_pipe_span));
  const uintptr_t pipe_begin =
      reinterpret_cast<uintptr_t>(raw_pipe_span.data());
  ASSERT_EQ(MOJO_RESULT_OK, producer_handle->EndWriteData(0));

  base::RunLoop loop;
  DataPipeReader reader(std::move(consumer_handle), kPayloadSize,
                        loop.QuitClosure());

  bool source_received_pipe_span = false;
  int read_calls = 0;
  auto producer =
      std::make_unique<DataPipeProducer>(std::move(producer_handle));
  DataPipeProducer* raw_producer = producer.get();
  raw_producer->Write(
      std::make_unique<FilteredDataSource>(
          std::make_unique<BufferRecordingDataSource>(
              expected, pipe_begin, &source_received_pipe_span, &read_calls),
          /*filter=*/nullptr),
      base::BindOnce(
          [](std::unique_ptr<DataPipeProducer>, MojoResult result) {
            EXPECT_EQ(MOJO_RESULT_OK, result);
          },
          std::move(producer)));
  loop.Run();

  EXPECT_EQ(1, read_calls);
  EXPECT_TRUE(source_received_pipe_span);
  EXPECT_EQ(expected, reader.data());
}

class DirectBufferCheckFilter : public FilteredDataSource::Filter {
 public:
  DirectBufferCheckFilter(uintptr_t expected_pipe_begin,
                          bool* matched_pipe_ptr,
                          int* on_read_calls)
      : expected_pipe_begin_(expected_pipe_begin),
        matched_pipe_ptr_(matched_pipe_ptr),
        on_read_calls_(on_read_calls) {}

  void OnRead(base::span<char> buffer,
              FilteredDataSource::ReadResult* result) override {
    if (result->result == MOJO_RESULT_OK && result->bytes_read > 0) {
      ++(*on_read_calls_);
      *matched_pipe_ptr_ =
          (reinterpret_cast<uintptr_t>(buffer.data()) == expected_pipe_begin_);
    }
  }

  void OnDone() override {}

 private:
  const uintptr_t expected_pipe_begin_;
  raw_ptr<bool> matched_pipe_ptr_;
  raw_ptr<int> on_read_calls_;
};

TEST(DataPipeProducerFeatureTest, DisabledFeatureReadsDirectlyIntoPipe) {
  // Initialize ScopedFeatureList before TaskEnvironment so worker threads never
  // race with FeatureList override setup or teardown.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(kStageDataPipeProducerReads);
  base::test::TaskEnvironment task_environment;

  // Use a payload > 64 KiB in a 128 KiB pipe to verify direct reads write
  // straight into the pipe buffer without 64 KiB clamping when disabled.
  constexpr size_t kPayloadSize = 70 * 1024;
  std::string expected(kPayloadSize, 'z');

  ScopedDataPipeProducerHandle producer_handle;
  ScopedDataPipeConsumerHandle consumer_handle;
  ASSERT_EQ(MOJO_RESULT_OK,
            CreateDataPipe(128 * 1024, producer_handle, consumer_handle));

  base::span<uint8_t> raw_pipe_span;
  ASSERT_EQ(MOJO_RESULT_OK,
            producer_handle->BeginWriteData(DataPipeProducerHandle::kNoSizeHint,
                                            MOJO_BEGIN_WRITE_DATA_FLAG_NONE,
                                            raw_pipe_span));
  const uintptr_t pipe_begin =
      reinterpret_cast<uintptr_t>(raw_pipe_span.data());
  ASSERT_EQ(MOJO_RESULT_OK, producer_handle->EndWriteData(0));

  base::RunLoop loop;
  DataPipeReader reader(std::move(consumer_handle), kPayloadSize,
                        loop.QuitClosure());

  bool filter_received_pipe_span = false;
  int on_read_calls = 0;
  auto producer =
      std::make_unique<DataPipeProducer>(std::move(producer_handle));
  DataPipeProducer* raw_producer = producer.get();
  raw_producer->Write(
      std::make_unique<FilteredDataSource>(
          std::make_unique<StringDataSource>(
              expected, StringDataSource::AsyncWritingMode::
                            STRING_STAYS_VALID_UNTIL_COMPLETION),
          std::make_unique<DirectBufferCheckFilter>(
              pipe_begin, &filter_received_pipe_span, &on_read_calls)),
      base::BindOnce(
          [](std::unique_ptr<DataPipeProducer>, MojoResult result) {
            EXPECT_EQ(MOJO_RESULT_OK, result);
          },
          std::move(producer)));
  loop.Run();

  EXPECT_EQ(1, on_read_calls);
  EXPECT_TRUE(filter_received_pipe_span);
  EXPECT_EQ(expected, reader.data());
}

}  // namespace
}  // namespace mojo
