// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prefetch/prefetch_data_pipe_tee.h"

#include "base/containers/span.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/system/string_data_source.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

class DataPipeReader {
 public:
  explicit DataPipeReader(mojo::ScopedDataPipeConsumerHandle consumer_handle)
      : consumer_handle_(std::move(consumer_handle)) {
    DCHECK(consumer_handle_);
  }

  std::string ReadData(uint32_t size) {
    size_ = size;
    data_.clear();

    if (!consumer_handle_) {
      return std::string();
    }

    base::RunLoop run_loop;
    on_read_done_ = run_loop.QuitClosure();

    mojo::SimpleWatcher watcher(FROM_HERE,
                                mojo::SimpleWatcher::ArmingPolicy::AUTOMATIC,
                                base::SequencedTaskRunner::GetCurrentDefault());
    watcher.Watch(consumer_handle_.get(), MOJO_HANDLE_SIGNAL_READABLE,
                  base::BindRepeating(&DataPipeReader::OnDataAvailable,
                                      base::Unretained(this)));

    run_loop.Run();

    on_read_done_.Reset();
    watcher.Cancel();

    return data_;
  }

  DataPipeReader(const DataPipeReader&) = delete;
  DataPipeReader& operator=(const DataPipeReader&) = delete;

  ~DataPipeReader() = default;

 private:
  void OnDataAvailable(MojoResult result) {
    DCHECK_LT(data_.size(), size_);
    size_t size = size_ - data_.size();
    size_t actually_read_bytes = 0;
    std::string buffer(size, '\0');
    MojoResult read_result = consumer_handle_->ReadData(
        MOJO_READ_DATA_FLAG_NONE, base::as_writable_byte_span(buffer),
        actually_read_bytes);
    if (read_result == MOJO_RESULT_OK) {
      data_.append(std::string_view(buffer).substr(0, actually_read_bytes));
      if (data_.size() >= size_) {
        on_read_done_.Run();
      }
    } else if (read_result != MOJO_RESULT_SHOULD_WAIT) {
      on_read_done_.Run();
    }
  }

  mojo::ScopedDataPipeConsumerHandle consumer_handle_;

  size_t size_;
  std::string data_;
  base::RepeatingClosure on_read_done_;
};

// If the `bool` param is `true`, `PrefetchDataPipeTee` reference (`tee_`) is
// released earlier to make sure that `PrefetchDataPipeTee` is kept alive and
// data pipe cloning completes correctly even if there are no strong references
// to `PrefetchDataPipeTee` from outside.
class PrefetchDataPipeTeeTest : public ::testing::Test,
                                public ::testing::WithParamInterface<bool> {
 public:
  void Write(const std::string& content,
             base::OnceClosure write_complete_callback) {
    source_producer_->Write(
        std::make_unique<mojo::StringDataSource>(
            content, mojo::StringDataSource::AsyncWritingMode::
                         STRING_MAY_BE_INVALIDATED_BEFORE_COMPLETION),
        base::BindOnce(
            [](base::OnceClosure write_complete_callback, MojoResult result) {
              std::move(write_complete_callback).Run();
            },
            std::move(write_complete_callback)));
  }
  void Write(const std::vector<std::string>& contents) {
    for (auto& content : contents) {
      base::RunLoop loop;
      Write(content, loop.QuitClosure());
      loop.Run();
    }
  }
  void WriteComplete() { source_producer_.reset(); }
  void ResetReference() {
    if (GetParam()) {
      tee_.reset();
    }
  }

  PrefetchDataPipeTee& tee() { return *tee_; }
  base::test::TaskEnvironment& task_environment() { return task_environment_; }

  static constexpr int kProducerPipeCapacity = 1024;
  static constexpr int kBufferLimit = 8;

 private:
  void SetUp() override {
    mojo::ScopedDataPipeConsumerHandle source_consumer_handle;
    mojo::ScopedDataPipeProducerHandle source_producer_handle;
    CHECK_EQ(mojo::CreateDataPipe(kProducerPipeCapacity, source_producer_handle,
                                  source_consumer_handle),
             MOJO_RESULT_OK);
    source_producer_ = std::make_unique<mojo::DataPipeProducer>(
        std::move(source_producer_handle));

    tee_ = base::MakeRefCounted<PrefetchDataPipeTee>(
        std::move(source_consumer_handle), kBufferLimit);
  }

  std::unique_ptr<mojo::DataPipeProducer> source_producer_;
  scoped_refptr<PrefetchDataPipeTee> tee_;

  base::test::TaskEnvironment task_environment_;
};

TEST_P(PrefetchDataPipeTeeTest, FirstTargetAddedThenLoaded) {
  Write({"B"});

  auto target1 = DataPipeReader(tee().Clone());
  EXPECT_FALSE(tee().Clone());

  // Data are sent to the target in a streaming fashion during loading.
  EXPECT_EQ(target1.ReadData(1), "B");

  Write({"o"});
  EXPECT_EQ(target1.ReadData(1), "o");

  Write({"d"});
  EXPECT_EQ(target1.ReadData(1), "d");

  Write({"y"});
  EXPECT_EQ(target1.ReadData(1), "y");

  WriteComplete();
  task_environment().RunUntilIdle();

  // After the source is complete and within the buffer limit, any number of
  // targets can be added.
  auto target2 = DataPipeReader(tee().Clone());
  auto target3 = DataPipeReader(tee().Clone());
  ResetReference();

  EXPECT_EQ(target1.ReadData(32), "");
  EXPECT_EQ(target2.ReadData(32), "Body");
  EXPECT_EQ(target3.ReadData(32), "Body");
}

TEST_P(PrefetchDataPipeTeeTest, FirstTargetAddedAndRemoved) {
  Write({"Bo", "dy"});

  auto target1 = std::make_unique<DataPipeReader>(tee().Clone());
  EXPECT_FALSE(tee().Clone());

  EXPECT_EQ(target1->ReadData(4), "Body");

  target1.reset();
  task_environment().RunUntilIdle();

  // After the first target is destructed, a new target can be added.
  auto target2 = std::make_unique<DataPipeReader>(tee().Clone());
  EXPECT_EQ(target2->ReadData(4), "Body");

  Write({" exceeds", " ", "limit"});
  EXPECT_EQ(target2->ReadData(14), " exceeds limit");
  EXPECT_FALSE(tee().Clone());

  target2.reset();
  task_environment().RunUntilIdle();

  // Even after targets are destructed, targets can't be added once the buffer
  // limit is exceeded. Data can be written to the source data pipe, but are
  // just discarded.
  EXPECT_FALSE(tee().Clone());
  Write({" foo"});

  WriteComplete();
  task_environment().RunUntilIdle();

  EXPECT_FALSE(tee().Clone());
}

TEST_P(PrefetchDataPipeTeeTest, LoadedThenFirstTargetAdded) {
  Write({"Bo", "dy"});
  WriteComplete();
  task_environment().RunUntilIdle();

  // After the source is complete and within the buffer limit, any number of
  // targets can be added.
  auto target1 = DataPipeReader(tee().Clone());
  auto target2 = DataPipeReader(tee().Clone());
  ResetReference();

  EXPECT_EQ(target1.ReadData(32), "Body");
  EXPECT_EQ(target2.ReadData(32), "Body");
}

TEST_P(PrefetchDataPipeTeeTest, FirstTargetAddedThenExceedLimit) {
  Write({"Bo", "dy"});

  auto target1 = DataPipeReader(tee().Clone());
  EXPECT_FALSE(tee().Clone());

  Write({" exceeds", " ", "limit"});

  EXPECT_FALSE(tee().Clone());

  WriteComplete();
  task_environment().RunUntilIdle();

  // Even after the source is complete, no target can be added because the
  // buffer is already discarded due to size limit.
  EXPECT_FALSE(tee().Clone());
  ResetReference();

  EXPECT_EQ(target1.ReadData(32), "Body exceeds limit");
}

TEST_P(PrefetchDataPipeTeeTest, ExceedLimitThenFirstTargetAdded) {
  Write({"Bo", "dy", " exceeds", " ", "limit"});

  auto target1 = DataPipeReader(tee().Clone());
  EXPECT_FALSE(tee().Clone());

  WriteComplete();
  task_environment().RunUntilIdle();

  // Even after the source is complete, no target can be added because the
  // buffer is already discarded due to size limit.
  EXPECT_FALSE(tee().Clone());
  ResetReference();

  EXPECT_EQ(target1.ReadData(32), "Body exceeds limit");
}

TEST_P(PrefetchDataPipeTeeTest, ExceedLimitLargeData) {
  Write({"Bo", "dy", " exceeds", " ", "limit"});

  // Larger than producer data pipe size created in `SetUp()`.
  std::string large_content(kProducerPipeCapacity * 2, '-');

  base::RunLoop write_loop1;
  Write(large_content, write_loop1.QuitClosure());
  task_environment().RunUntilIdle();
  EXPECT_FALSE(write_loop1.AnyQuitCalled());

  auto target1 = DataPipeReader(tee().Clone());
  EXPECT_FALSE(tee().Clone());
  ResetReference();

  // Full data can be read.
  std::string expected = "Body exceeds limit" + large_content;
  EXPECT_EQ(target1.ReadData(expected.size()), expected);
  write_loop1.Run();

  // Data written after that can be also read.
  base::RunLoop write_loop2;
  Write(large_content, write_loop2.QuitClosure());
  EXPECT_EQ(target1.ReadData(large_content.size()), large_content);
  write_loop2.Run();

  // End of data can be observed.
  WriteComplete();
  task_environment().RunUntilIdle();
  EXPECT_EQ(target1.ReadData(32), "");
}

TEST_P(PrefetchDataPipeTeeTest, ExceedLimitAndLoadedThenFirstTargetAdded) {
  Write({"Bo", "dy", " exceeds", " ", "limit"});
  WriteComplete();
  task_environment().RunUntilIdle();

  auto target1 = DataPipeReader(tee().Clone());
  EXPECT_FALSE(tee().Clone());
  ResetReference();

  EXPECT_EQ(target1.ReadData(32), "Body exceeds limit");
}

INSTANTIATE_TEST_SUITE_P(ParametrizedTests,
                         PrefetchDataPipeTeeTest,
                         testing::Bool());

}  // namespace
}  // namespace content
