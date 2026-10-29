// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/symphonia_data_source_bridge.h"

#include <stdint.h>

#include <algorithm>
#include <array>
#include <utility>

#include "base/containers/span.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/raw_span.h"
#include "base/run_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "media/base/data_source.h"
#include "media/base/test_data_util.h"
#include "media/filters/file_data_source.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/rust/cxx/v1/cxx.h"

namespace media {

namespace {

constexpr int kBufferSize = 32;
using TestBuffer = std::array<uint8_t, kBufferSize>;

rust::Slice<uint8_t> ToSlice(base::span<uint8_t> buffer) {
  return rust::Slice<uint8_t>(buffer.data(), buffer.size());
}

// A `FileDataSource` whose reads can be controlled per call to simulate hung
// reads, `kAborted` seek cancellations, or responses whose total length is not
// known up front.
class ControllableFileDataSource : public FileDataSource {
 public:
  void Read(int64_t position,
            base::span<uint8_t> data,
            DataSource::ReadCB read_cb) override {
    ++read_calls_;
    if (abort_next_read_) {
      abort_next_read_ = false;
      std::move(read_cb).Run(DataSource::kAborted);
      return;
    }
    if (hang_reads_) {
      pending_data_ = data;
      pending_read_cb_ = std::move(read_cb);
      read_started_.Signal();
      return;
    }
    if (populate_size_during_read_) {
      hide_size_ = false;
    }
    FileDataSource::Read(position, data, std::move(read_cb));
  }

  void Stop() override {
    stop_called_ = true;
    FileDataSource::Stop();
  }

  bool GetSize(int64_t* size_out) override {
    if (hide_size_) {
      *size_out = 0;
      return false;
    }
    return FileDataSource::GetSize(size_out);
  }

  // Writes into the buffer of the pending read, then completes it.
  void CompletePendingRead() {
    std::ranges::fill(pending_data_, 0xFF);
    const int size = static_cast<int>(pending_data_.size());
    pending_data_ = {};
    std::move(pending_read_cb_).Run(size);
  }

  void set_hang_reads(bool hang) { hang_reads_ = hang; }
  void set_abort_next_read(bool abort) { abort_next_read_ = abort; }
  void set_hide_size(bool hide) { hide_size_ = hide; }
  void set_populate_size_during_read(bool populate) {
    populate_size_during_read_ = populate;
  }

  int read_calls() const { return read_calls_; }
  bool stop_called() const { return stop_called_; }
  base::WaitableEvent& read_started() { return read_started_; }

 private:
  bool hang_reads_ = false;
  bool abort_next_read_ = false;
  bool hide_size_ = false;
  bool populate_size_during_read_ = false;
  int read_calls_ = 0;
  bool stop_called_ = false;
  base::WaitableEvent read_started_;
  base::raw_span<uint8_t> pending_data_;
  DataSource::ReadCB pending_read_cb_;
};

}  // namespace

class SymphoniaDataSourceBridgeTest : public testing::Test {
 public:
  ~SymphoniaDataSourceBridgeTest() override {
    bridge_.reset();
    data_source_.Stop();
  }

  void SetUp() override {
    ASSERT_TRUE(data_source_.Initialize(GetTestDataFilePath("sfx_s16le.wav")));
    ASSERT_TRUE(data_source_.GetSize(&file_size_));
  }

  // Creates the bridge lazily so tests can configure `data_source_` first.
  SymphoniaDataSourceBridge& bridge() {
    if (!bridge_) {
      bridge_ = SymphoniaDataSourceBridge::Create(
          &data_source_, base::SingleThreadTaskRunner::GetCurrentDefault(),
          error_cb_.Get());
    }
    return *bridge_;
  }

  int64_t Read(base::span<uint8_t> buffer) {
    return bridge().Read(ToSlice(buffer));
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  ControllableFileDataSource data_source_;
  int64_t file_size_ = 0;
  base::MockRepeatingClosure error_cb_;
  SymphoniaDataSourceBridge::UniquePtr bridge_;
};

TEST_F(SymphoniaDataSourceBridgeTest, ReadsAndAdvancesPosition) {
  TestBuffer buffer;
  EXPECT_EQ(bridge().GetPosition(), 0u);
  EXPECT_EQ(Read(buffer), kBufferSize);
  EXPECT_EQ(bridge().GetPosition(), uint64_t{kBufferSize});
  EXPECT_EQ(Read(buffer), kBufferSize);
  EXPECT_EQ(bridge().GetPosition(), uint64_t{2 * kBufferSize});
}

TEST_F(SymphoniaDataSourceBridgeTest, EmptyReadReturnsZero) {
  EXPECT_EQ(Read({}), 0);
  EXPECT_EQ(bridge().GetPosition(), 0u);
}

TEST_F(SymphoniaDataSourceBridgeTest, ReadAtEndOfFileReturnsZero) {
  constexpr int kBytesLeft = kBufferSize / 2;
  TestBuffer buffer;
  ASSERT_TRUE(bridge().Seek(file_size_ - kBytesLeft));
  EXPECT_EQ(Read(buffer), kBytesLeft);
  EXPECT_EQ(bridge().GetPosition(), static_cast<uint64_t>(file_size_));

  // Symphonia expects end of file to be a zero-length read, not an error.
  EXPECT_EQ(Read(buffer), 0);
}

TEST_F(SymphoniaDataSourceBridgeTest,
       ReadAtEndOfFileWithoutInitialContentLengthReturnsZero) {
  constexpr int kBytesLeft = kBufferSize / 2;
  TestBuffer buffer;
  ASSERT_TRUE(bridge().Seek(file_size_ - kBytesLeft));

  // Simulate an HTTP response without a `Content-Length` header whose total
  // size only becomes known when the final read reaches end of stream.
  data_source_.set_hide_size(true);
  EXPECT_EQ(bridge().GetLength(), -1);
  EXPECT_EQ(Read(buffer), kBytesLeft);

  data_source_.set_populate_size_during_read(true);
  EXPECT_EQ(Read(buffer), 0);
  EXPECT_EQ(bridge().GetLength(), file_size_);
  EXPECT_EQ(Read(buffer), 0);
}

TEST_F(SymphoniaDataSourceBridgeTest, Seek) {
  EXPECT_TRUE(bridge().Seek(512));
  EXPECT_EQ(bridge().GetPosition(), 512u);
  EXPECT_TRUE(bridge().Seek(file_size_));
  EXPECT_EQ(bridge().GetPosition(), static_cast<uint64_t>(file_size_));
  EXPECT_FALSE(bridge().Seek(file_size_ + 1));
  EXPECT_EQ(bridge().GetPosition(), static_cast<uint64_t>(file_size_));
}

TEST_F(SymphoniaDataSourceBridgeTest, SeekBackwardsRereadsSameData) {
  TestBuffer first;
  ASSERT_TRUE(bridge().Seek(512));
  ASSERT_EQ(Read(first), kBufferSize);

  // Read further into the file, then seek back to the first read.
  TestBuffer later;
  ASSERT_TRUE(bridge().Seek(1024));
  ASSERT_EQ(Read(later), kBufferSize);
  ASSERT_TRUE(bridge().Seek(512));
  EXPECT_EQ(bridge().GetPosition(), 512u);

  TestBuffer second;
  ASSERT_EQ(Read(second), kBufferSize);
  EXPECT_EQ(first, second);
}

TEST_F(SymphoniaDataSourceBridgeTest, ReadsWithGrowingBufferSizes) {
  // A small read followed by a larger one exercises growing the reused read
  // buffer between reads.
  constexpr size_t kSmallReadSize = 8;
  TestBuffer small;
  ASSERT_EQ(Read(base::span(small).first(kSmallReadSize)),
            static_cast<int64_t>(kSmallReadSize));
  TestBuffer large;
  ASSERT_EQ(Read(large), kBufferSize);
  EXPECT_EQ(bridge().GetPosition(), uint64_t{kSmallReadSize + kBufferSize});

  // The two reads must match one contiguous read of the same range.
  std::array<uint8_t, kSmallReadSize + kBufferSize> expected;
  ASSERT_TRUE(bridge().Seek(0));
  ASSERT_EQ(Read(expected), static_cast<int64_t>(expected.size()));
  EXPECT_EQ(base::span(expected).first(kSmallReadSize),
            base::span(small).first(kSmallReadSize));
  EXPECT_EQ(base::span(expected).subspan(kSmallReadSize), base::span(large));
}

TEST_F(SymphoniaDataSourceBridgeTest, GetLength) {
  EXPECT_GT(file_size_, 0);
  EXPECT_EQ(bridge().GetLength(), file_size_);
}

TEST_F(SymphoniaDataSourceBridgeTest, IsSeekable) {
  EXPECT_TRUE(bridge().IsSeekable());
}

TEST_F(SymphoniaDataSourceBridgeTest, StreamingSourceIsNotSeekable) {
  data_source_.force_streaming_for_testing();
  EXPECT_FALSE(bridge().IsSeekable());
}

TEST_F(SymphoniaDataSourceBridgeTest, ReadErrorRunsCallbackAndLatchesFailure) {
  data_source_.force_read_errors_for_testing();
  base::RunLoop error_reported;
  EXPECT_CALL(error_cb_, Run()).WillOnce([&error_reported] {
    error_reported.Quit();
  });

  TestBuffer buffer;
  EXPECT_EQ(Read(buffer), -1);
  error_reported.Run();
  EXPECT_EQ(data_source_.read_calls(), 1);

  // Subsequent reads and seeks fail immediately without touching `data_source_`
  // again.
  EXPECT_EQ(Read(buffer), -1);
  EXPECT_FALSE(bridge().Seek(0));
  EXPECT_EQ(data_source_.read_calls(), 1);
}

TEST_F(SymphoniaDataSourceBridgeTest,
       AbortedDataSourceReadAllowsSubsequentSeekAndRead) {
  EXPECT_CALL(error_cb_, Run()).Times(0);
  data_source_.set_abort_next_read(true);

  TestBuffer buffer;
  EXPECT_EQ(Read(buffer), -1);
  EXPECT_EQ(bridge().GetPosition(), 0u);

  // `DataSource::kAborted` cancels only the in-flight read (such as during
  // `CancelPendingSeek()`), so subsequent seeks and reads must still succeed.
  ASSERT_TRUE(bridge().Seek(512));
  EXPECT_EQ(Read(buffer), kBufferSize);
  EXPECT_EQ(bridge().GetPosition(), uint64_t{512 + kBufferSize});
}

TEST_F(SymphoniaDataSourceBridgeTest,
       ResettingBridgeStopsDataSourceAndUnblocksThreadPoolRead) {
  EXPECT_CALL(error_cb_, Run()).Times(0);
  data_source_.set_hang_reads(true);

  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner =
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::WithBaseSyncPrimitives()});
  auto async_bridge = SymphoniaDataSourceBridge::Create(
      &data_source_, blocking_task_runner, error_cb_.Get());

  TestBuffer buffer;
  base::test::TestFuture<int64_t> read_result;
  blocking_task_runner->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindLambdaForTesting([raw_bridge = async_bridge.get(), &buffer] {
        return raw_bridge->Read(ToSlice(buffer));
      }),
      read_result.GetCallback());

  data_source_.read_started().Wait();

  // Resetting `UniquePtr` stops `data_source_`, unblocks the pending `Read()`,
  // and schedules bridge destruction on `blocking_task_runner`.
  async_bridge.reset();
  EXPECT_TRUE(data_source_.stop_called());

  EXPECT_EQ(read_result.Get(), -1);

  // Wait for the scheduled bridge destruction task to complete on
  // `blocking_task_runner` before completing the hung `DataSource` callback.
  base::test::TestFuture<void> destruction_flushed;
  blocking_task_runner->PostTaskAndReply(FROM_HERE, base::DoNothing(),
                                         destruction_flushed.GetCallback());
  ASSERT_TRUE(destruction_flushed.Wait());

  // Even after the bridge itself has been destroyed, `ReadOperation` keeps its
  // buffer valid until `DataSource` finishes or drops its pending callback.
  data_source_.CompletePendingRead();
}

}  // namespace media
