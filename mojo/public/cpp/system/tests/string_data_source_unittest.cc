// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/system/string_data_source.h"

#include <algorithm>
#include <list>
#include <memory>
#include <string>
#include <string_view>

#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/public/cpp/system/data_pipe_producer.h"
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
                          base::OnceClosure on_read_done)
      : consumer_handle_(std::move(consumer_handle)),
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
      size_t size = 0;
      std::string buffer(64, '\0');
      MojoResult read_result;
      do {
        read_result = consumer_handle_->ReadData(
            MOJO_READ_DATA_FLAG_NONE, base::as_writable_byte_span(buffer),
            size);
        if (read_result == MOJO_RESULT_OK) {
          data_.append(std::string_view(buffer).substr(0, size));
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
  base::OnceClosure on_read_done_;
  SimpleWatcher watcher_;
  std::string data_;
};

class StringDataSourceTest : public testing::Test {
 public:
  StringDataSourceTest() = default;

  StringDataSourceTest(const StringDataSourceTest&) = delete;
  StringDataSourceTest& operator=(const StringDataSourceTest&) = delete;

  ~StringDataSourceTest() override = default;

 protected:
  static void CreateDataPipe(uint32_t capacity,
                             mojo::ScopedDataPipeProducerHandle* producer,
                             mojo::ScopedDataPipeConsumerHandle* consumer) {
    MojoCreateDataPipeOptions options;
    options.struct_size = sizeof(MojoCreateDataPipeOptions);
    options.element_num_bytes = 1;
    options.capacity_num_bytes = capacity;
    ASSERT_EQ(MOJO_RESULT_OK,
              mojo::CreateDataPipe(&options, *producer, *consumer));
  }

  static void WriteStringThenCloseProducer(
      std::unique_ptr<DataPipeProducer> producer,
      const std::string_view& str,
      StringDataSource::AsyncWritingMode mode) {
    DataPipeProducer* raw_producer = producer.get();
    raw_producer->Write(
        std::make_unique<mojo::StringDataSource>(str, mode),
        base::BindOnce([](std::unique_ptr<DataPipeProducer> producer,
                          MojoResult result) {},
                       std::move(producer)));
  }

  static void WriteStringsThenCloseProducer(
      std::unique_ptr<DataPipeProducer> producer,
      std::list<std::string_view> strings,
      StringDataSource::AsyncWritingMode mode) {
    DataPipeProducer* raw_producer = producer.get();
    std::string_view str = strings.front();
    strings.pop_front();
    raw_producer->Write(
        std::make_unique<mojo::StringDataSource>(str, mode),
        base::BindOnce(
            [](std::unique_ptr<DataPipeProducer> producer,
               std::list<std::string_view> strings,
               StringDataSource::AsyncWritingMode mode, MojoResult result) {
              if (!strings.empty())
                WriteStringsThenCloseProducer(std::move(producer),
                                              std::move(strings), mode);
            },
            std::move(producer), std::move(strings), mode));
  }

 private:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(StringDataSourceTest, EqualCapacity) {
  const std::string kTestString = "Hello, world!";

  base::RunLoop loop;
  mojo::ScopedDataPipeProducerHandle producer_handle;
  mojo::ScopedDataPipeConsumerHandle consumer_handle;
  CreateDataPipe(static_cast<uint32_t>(kTestString.size()), &producer_handle,
                 &consumer_handle);
  DataPipeReader reader(std::move(consumer_handle), loop.QuitClosure());
  WriteStringThenCloseProducer(
      std::make_unique<DataPipeProducer>(std::move(producer_handle)),
      kTestString,
      StringDataSource::AsyncWritingMode::
          STRING_MAY_BE_INVALIDATED_BEFORE_COMPLETION);
  loop.Run();

  EXPECT_EQ(kTestString, reader.data());
}

TEST_F(StringDataSourceTest, UnderCapacity) {
  const std::string kTestString = "Hello, world!";

  base::RunLoop loop;
  mojo::ScopedDataPipeProducerHandle producer_handle;
  mojo::ScopedDataPipeConsumerHandle consumer_handle;
  CreateDataPipe(static_cast<uint32_t>(kTestString.size() * 2),
                 &producer_handle, &consumer_handle);
  DataPipeReader reader(std::move(consumer_handle), loop.QuitClosure());
  WriteStringThenCloseProducer(
      std::make_unique<DataPipeProducer>(std::move(producer_handle)),
      kTestString,
      StringDataSource::AsyncWritingMode::
          STRING_MAY_BE_INVALIDATED_BEFORE_COMPLETION);
  loop.Run();

  EXPECT_EQ(kTestString, reader.data());
}

TEST_F(StringDataSourceTest, OverCapacity) {
  const std::string kTestString = "Hello, world!";

  base::RunLoop loop;
  mojo::ScopedDataPipeProducerHandle producer_handle;
  mojo::ScopedDataPipeConsumerHandle consumer_handle;
  CreateDataPipe(static_cast<uint32_t>(kTestString.size() / 2),
                 &producer_handle, &consumer_handle);
  DataPipeReader reader(std::move(consumer_handle), loop.QuitClosure());
  WriteStringThenCloseProducer(
      std::make_unique<DataPipeProducer>(std::move(producer_handle)),
      kTestString,
      StringDataSource::AsyncWritingMode::STRING_STAYS_VALID_UNTIL_COMPLETION);
  loop.Run();

  EXPECT_EQ(kTestString, reader.data());
}

TEST_F(StringDataSourceTest, TinyPipe) {
  const std::string kTestString = "Hello, world!";

  base::RunLoop loop;
  mojo::ScopedDataPipeProducerHandle producer_handle;
  mojo::ScopedDataPipeConsumerHandle consumer_handle;
  CreateDataPipe(1u, &producer_handle, &consumer_handle);
  DataPipeReader reader(std::move(consumer_handle), loop.QuitClosure());
  WriteStringThenCloseProducer(
      std::make_unique<DataPipeProducer>(std::move(producer_handle)),
      kTestString,
      StringDataSource::AsyncWritingMode::
          STRING_MAY_BE_INVALIDATED_BEFORE_COMPLETION);
  loop.Run();

  EXPECT_EQ(kTestString, reader.data());
}

TEST_F(StringDataSourceTest, MultipleWrites) {
  const std::string kTestString1 = "Hello, world!";
  const std::string kTestString2 = "There is a lot of data coming your way!";
  const std::string kTestString3 = "So many strings!";
  const std::string kTestString4 = "Your cup runneth over!";

  base::RunLoop loop;
  mojo::ScopedDataPipeProducerHandle producer_handle;
  mojo::ScopedDataPipeConsumerHandle consumer_handle;
  CreateDataPipe(4u, &producer_handle, &consumer_handle);
  DataPipeReader reader(std::move(consumer_handle), loop.QuitClosure());
  WriteStringsThenCloseProducer(
      std::make_unique<DataPipeProducer>(std::move(producer_handle)),
      {kTestString1, kTestString2, kTestString3, kTestString4},
      StringDataSource::AsyncWritingMode::
          STRING_MAY_BE_INVALIDATED_BEFORE_COMPLETION);
  loop.Run();

  EXPECT_EQ(kTestString1 + kTestString2 + kTestString3 + kTestString4,
            reader.data());
}

}  // namespace
}  // namespace mojo
