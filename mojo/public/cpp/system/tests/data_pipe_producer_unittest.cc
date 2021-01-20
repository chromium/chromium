// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <limits>
#include <memory>
#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/public/cpp/system/data_pipe_producer.h"
#include "mojo/public/cpp/system/file_data_source.h"
#include "mojo/public/cpp/system/filtered_data_source.h"
#include "mojo/public/cpp/system/simple_watcher.h"
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
                 base::SequencedTaskRunnerHandle::Get()) {
    watcher_.Watch(consumer_handle_.get(), MOJO_HANDLE_SIGNAL_READABLE,
                   MOJO_WATCH_CONDITION_SATISFIED,
                   base::BindRepeating(&DataPipeReader::OnDataAvailable,
                                       base::Unretained(this)));
  }
  ~DataPipeReader() = default;

  const std::string& data() const { return data_; }

 private:
  void OnDataAvailable(MojoResult result, const HandleSignalsState& state) {
    if (result == MOJO_RESULT_OK) {
      uint32_t size = static_cast<uint32_t>(read_size_);
      std::vector<char> buffer(size, 0);
      MojoResult read_result;
      do {
        read_result = consumer_handle_->ReadData(buffer.data(), &size,
                                                 MOJO_READ_DATA_FLAG_NONE);
        if (read_result == MOJO_RESULT_OK) {
          std::copy(buffer.begin(), buffer.begin() + size,
                    std::back_inserter(data_));
        }
      } while (read_result == MOJO_RESULT_OK);

      if (read_result == MOJO_RESULT_SHOULD_WAIT)
        return;
    }

    if (result != MOJO_RESULT_CANCELLED)
      watcher_.Cancel();

    std::move(on_read_done_).Run();
  }

  ScopedDataPipeConsumerHandle consumer_handle_;
  const size_t read_size_;
  base::OnceClosure on_read_done_;
  SimpleWatcher watcher_;
  std::string data_;

  DISALLOW_COPY_AND_ASSIGN(DataPipeReader);
};

class DataPipeProducerTest : public testing::Test {
 public:
  DataPipeProducerTest() { CHECK(temp_dir_.CreateUniqueTempDir()); }

  ~DataPipeProducerTest() override = default;

 protected:
  base::FilePath CreateTempFileWithContents(const std::string& contents) {
    base::FilePath temp_file_path = temp_dir_.GetPath().AppendASCII(
        base::StringPrintf("tmp%d", tmp_file_id_++));
    base::File temp_file(temp_file_path,
                         base::File::FLAG_CREATE | base::File::FLAG_WRITE);
    int bytes_written = temp_file.WriteAtCurrentPos(
        contents.data(), static_cast<int>(contents.size()));
    CHECK_EQ(static_cast<int>(contents.size()), bytes_written);
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

  DISALLOW_COPY_AND_ASSIGN(DataPipeProducerTest);
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

  // FilteredDataSource::Filter:
  void OnRead(base::span<char> buffer,
              FilteredDataSource::ReadResult* result) override {
    base::AutoLock auto_lock(lock_);
    if (result->result == MOJO_RESULT_OK)
      observer_data_->bytes_read += result->bytes_read;
    else
      observer_data_->num_read_errors++;
  }

  void OnDone() override {
    base::AutoLock auto_lock(lock_);
    observer_data_->done_called++;
  }

 private:
  DataPipeObserverData* observer_data_;
  // Observer may be called on any sequence.
  base::Lock lock_;

  DISALLOW_COPY_AND_ASSIGN(TestObserver);
};

TEST_F(DataPipeProducerTest, WriteFromFile) {
  const std::string kTestStringFragment = "Hello, world!";
  constexpr size_t kNumRepetitions = 1000;
  std::string test_string;
  for (size_t i = 0; i < kNumRepetitions; ++i)
    test_string += kTestStringFragment;

  base::FilePath path = CreateTempFileWithContents(test_string);

  base::RunLoop loop;
  DataPipe pipe(16);
  DataPipeReader reader(std::move(pipe.consumer_handle), 16,
                        loop.QuitClosure());

  base::File file(path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  DataPipeObserverData observer_data;
  auto observer = std::make_unique<TestObserver>(&observer_data);
  WriteFromFileThenCloseWriter(
      std::make_unique<DataPipeProducer>(std::move(pipe.producer_handle)),
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
  DataPipe pipe(static_cast<uint32_t>(kTestString.size()));
  DataPipeReader reader(std::move(pipe.consumer_handle), kTestString.size(),
                        loop.QuitClosure());

  base::File file(path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  DataPipeObserverData observer_data;
  auto observer = std::make_unique<TestObserver>(&observer_data);
  WriteFromFileThenCloseWriter(
      std::make_unique<DataPipeProducer>(std::move(pipe.producer_handle)),
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
  DataPipe pipe(kBytesToWrite);
  DataPipeObserverData observer_data;
  auto observer = std::make_unique<TestObserver>(&observer_data);
  DataPipeReader reader(std::move(pipe.consumer_handle), kBytesToWrite,
                        loop.QuitClosure());

  base::File file(path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  WriteFromFileThenCloseWriter(
      std::make_unique<DataPipeProducer>(std::move(pipe.producer_handle)),
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
  DataPipe pipe(16);
  DataPipeReader reader(std::move(pipe.consumer_handle), 16,
                        loop.QuitClosure());
  DataPipeObserverData observer_data;
  auto observer = std::make_unique<TestObserver>(&observer_data);
  base::File file(path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  WriteFromFileThenCloseWriter(
      std::make_unique<DataPipeProducer>(std::move(pipe.producer_handle)),
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
  DataPipe pipe(kDataPipeSize);
  DataPipeReader reader(std::move(pipe.consumer_handle), kDataPipeSize,
                        loop.QuitClosure());

  DataPipeObserverData observer_data;
  auto observer = std::make_unique<TestObserver>(&observer_data);
  base::File file(path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  WriteFromFileThenCloseWriter(
      std::make_unique<DataPipeProducer>(std::move(pipe.producer_handle)),
      std::move(observer), std::move(file));
  loop.Run();

  EXPECT_EQ(test_string, reader.data());
  EXPECT_EQ(0, observer_data.num_read_errors);
  EXPECT_EQ(kHugeFileSize, observer_data.bytes_read);
  EXPECT_EQ(1, observer_data.done_called);
}

}  // namespace
}  // namespace mojo
