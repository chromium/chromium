// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/platform/media/multi_buffer.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/circular_deque.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/simple_test_tick_clock.h"
#include "media/base/fake_single_thread_task_runner.h"
#include "media/base/test_random.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/media/multi_buffer_reader.h"

namespace blink {
namespace {
class FakeMultiBufferDataProvider;

const int kBlockSizeShift = 8;
const size_t kBlockSize = 1UL << kBlockSizeShift;

std::vector<FakeMultiBufferDataProvider*> writers;

class FakeMultiBufferDataProvider : public MultiBuffer::DataProvider {
 public:
  FakeMultiBufferDataProvider(MultiBufferBlockId pos,
                              size_t file_size,
                              int max_blocks_after_defer,
                              bool must_read_whole_file,
                              MultiBuffer* multibuffer,
                              media::TestRandom* rnd)
      : pos_(pos),
        blocks_until_deferred_(1 << 30),
        max_blocks_after_defer_(max_blocks_after_defer),
        file_size_(file_size),
        must_read_whole_file_(must_read_whole_file),
        multibuffer_(multibuffer),
        rnd_(rnd) {
    writers.push_back(this);
  }

  ~FakeMultiBufferDataProvider() override {
    if (must_read_whole_file_) {
      CHECK_GE(pos_ * kBlockSize, file_size_);
    }
    for (size_t i = 0; i < writers.size(); i++) {
      if (writers[i] == this) {
        writers[i] = writers.back();
        writers.pop_back();
        return;
      }
    }
    LOG(FATAL) << "Couldn't find myself in writers!";
  }

  MultiBufferBlockId Tell() const override { return pos_; }

  bool Available() const override { return !fifo_.empty(); }
  int64_t AvailableBytes() const override { return 0; }

  scoped_refptr<media::DataBuffer> Read() override {
    DCHECK(Available());
    scoped_refptr<media::DataBuffer> ret = fifo_.front();
    fifo_.pop_front();
    ++pos_;
    return ret;
  }

  void SetDeferred(bool deferred) override {
    if (deferred) {
      if (max_blocks_after_defer_ > 0) {
        blocks_until_deferred_ = rnd_->Rand() % max_blocks_after_defer_;
      } else if (max_blocks_after_defer_ < 0) {
        blocks_until_deferred_ = -max_blocks_after_defer_;
      } else {
        blocks_until_deferred_ = 0;
      }
    } else {
      blocks_until_deferred_ = 1 << 30;
    }
  }

  bool Advance() {
    if (blocks_until_deferred_ == 0)
      return false;
    --blocks_until_deferred_;

    bool ret = true;
    auto block =
        base::MakeRefCounted<media::DataBuffer>(static_cast<int>(kBlockSize));
    size_t x = 0;
    size_t byte_pos = (fifo_.size() + pos_) * kBlockSize;
    for (x = 0; x < kBlockSize; x++, byte_pos++) {
      if (byte_pos >= file_size_)
        break;
      block->writable_data()[x] =
          static_cast<uint8_t>((byte_pos * 15485863) >> 16);
    }
    block->set_data_size(static_cast<int>(x));
    fifo_.push_back(block);
    if (byte_pos == file_size_) {
      fifo_.push_back(media::DataBuffer::CreateEOSBuffer());
      ret = false;
    }
    multibuffer_->OnDataProviderEvent(this);
    return ret;
  }

 private:
  base::circular_deque<scoped_refptr<media::DataBuffer>> fifo_;
  MultiBufferBlockId pos_;
  int32_t blocks_until_deferred_;
  int32_t max_blocks_after_defer_;
  size_t file_size_;
  bool must_read_whole_file_;
  raw_ptr<MultiBuffer> multibuffer_;
  raw_ptr<media::TestRandom> rnd_;
};

}  // namespace

class TestMultiBuffer : public MultiBuffer {
 public:
  explicit TestMultiBuffer(int32_t shift,
                           const scoped_refptr<MultiBuffer::GlobalLRU>& lru,
                           media::TestRandom* rnd)
      : MultiBuffer(shift, lru),
        range_supported_(false),
        create_ok_(true),
        max_writers_(10000),
        file_size_(1 << 30),
        max_blocks_after_defer_(0),
        must_read_whole_file_(false),
        writers_created_(0),
        rnd_(rnd) {}

  void SetMaxWriters(size_t max_writers) { max_writers_ = max_writers; }

  void CheckPresentState() {
    IntervalMap<MultiBufferBlockId, int32_t> tmp;
    for (auto i = data_.begin(); i != data_.end(); ++i) {
      CHECK(i->second);  // Null poineters are not allowed in data_
      CHECK_NE(!!pinned_[i->first], lru_->Contains(this, i->first))
          << " i->first = " << i->first;
      tmp.IncrementInterval(i->first, i->first + 1, 1);
    }
    IntervalMap<MultiBufferBlockId, int32_t>::const_iterator tmp_iterator =
        tmp.begin();
    IntervalMap<MultiBufferBlockId, int32_t>::const_iterator present_iterator =
        present_.begin();
    while (tmp_iterator != tmp.end() && present_iterator != present_.end()) {
      EXPECT_EQ(tmp_iterator.interval_begin(),
                present_iterator.interval_begin());
      EXPECT_EQ(tmp_iterator.interval_end(), present_iterator.interval_end());
      EXPECT_EQ(tmp_iterator.value(), present_iterator.value());
      ++tmp_iterator;
      ++present_iterator;
    }
    EXPECT_TRUE(tmp_iterator == tmp.end());
    EXPECT_TRUE(present_iterator == present_.end());
  }

  void CheckLRUState() {
    for (auto i = data_.begin(); i != data_.end(); ++i) {
      CHECK(i->second);  // Null poineters are not allowed in data_
      CHECK_NE(!!pinned_[i->first], lru_->Contains(this, i->first))
          << " i->first = " << i->first;
      CHECK_EQ(1, present_[i->first]) << " i->first = " << i->first;
    }
  }

  void SetFileSize(size_t file_size) { file_size_ = file_size; }

  void SetMaxBlocksAfterDefer(int32_t max_blocks_after_defer) {
    max_blocks_after_defer_ = max_blocks_after_defer;
  }

  void SetMustReadWholeFile(bool must_read_whole_file) {
    must_read_whole_file_ = must_read_whole_file;
  }

  int32_t writers_created() const { return writers_created_; }

  void SetRangeSupported(bool supported) { range_supported_ = supported; }

 protected:
  std::unique_ptr<DataProvider> CreateWriter(const MultiBufferBlockId& pos,
                                             bool) override {
    DCHECK(create_ok_);
    writers_created_++;
    CHECK_LT(writers.size(), max_writers_);
    return std::make_unique<FakeMultiBufferDataProvider>(
        pos, file_size_, max_blocks_after_defer_, must_read_whole_file_, this,
        rnd_);
  }
  void Prune(size_t max_to_free) override {
    // Prune should not cause additional writers to be spawned.
    create_ok_ = false;
    MultiBuffer::Prune(max_to_free);
    create_ok_ = true;
  }

  bool RangeSupported() const override { return range_supported_; }

 private:
  bool range_supported_;
  bool create_ok_;
  size_t max_writers_;
  size_t file_size_;
  int32_t max_blocks_after_defer_;
  bool must_read_whole_file_;
  int32_t writers_created_;
  raw_ptr<media::TestRandom> rnd_;
};

class MultiBufferTest : public testing::Test {
 public:
  MultiBufferTest()
      : rnd_(42),
        task_runner_(
            base::MakeRefCounted<media::FakeSingleThreadTaskRunner>(&clock_)),
        lru_(base::MakeRefCounted<MultiBuffer::GlobalLRU>(task_runner_)),
        multibuffer_(kBlockSizeShift, lru_, &rnd_) {}

  void TearDown() override {
    // Make sure we have nothing left to prune.
    lru_->Prune(1000000);
    // Run the outstanding callback to make sure everything is freed.
    task_runner_->Sleep(base::Seconds(30));
  }

  void Advance() {
    CHECK(writers.size());
    writers[rnd_.Rand() % writers.size()]->Advance();
  }

  bool AdvanceAll() {
    bool advanced = false;
    for (size_t i = 0; i < writers.size(); i++) {
      advanced |= writers[i]->Advance();
    }
    multibuffer_.CheckLRUState();
    return advanced;
  }

 protected:
  media::TestRandom rnd_;
  base::SimpleTestTickClock clock_;
  scoped_refptr<media::FakeSingleThreadTaskRunner> task_runner_;
  scoped_refptr<MultiBuffer::GlobalLRU> lru_;
  TestMultiBuffer multibuffer_;
};

TEST_F(MultiBufferTest, ReadAll) {
  multibuffer_.SetMaxWriters(1);
  size_t pos = 0;
  size_t end = 10000;
  multibuffer_.SetFileSize(10000);
  multibuffer_.SetMustReadWholeFile(true);
  MultiBufferReader reader(&multibuffer_, pos, end,
                           /*is_client_audio_element=*/false,
                           base::NullCallback(), task_runner_);
  reader.SetPinRange(2000, 5000);
  reader.SetPreload(1000, 1000);
  while (pos < end) {
    unsigned char buffer[27];
    buffer[17] = 17;
    size_t to_read = std::min<size_t>(end - pos, 17);
    int64_t bytes_read = reader.TryRead(buffer, to_read);
    if (bytes_read) {
      EXPECT_EQ(buffer[17], 17);
      for (int64_t i = 0; i < bytes_read; i++) {
        uint8_t expected = static_cast<uint8_t>((pos * 15485863) >> 16);
        EXPECT_EQ(expected, buffer[i]) << " pos = " << pos;
        pos++;
      }
    } else {
      Advance();
    }
  }
}

TEST_F(MultiBufferTest, ReadAllAdvanceFirst) {
  multibuffer_.SetMaxWriters(1);
  size_t pos = 0;
  size_t end = 10000;
  multibuffer_.SetFileSize(10000);
  multibuffer_.SetMustReadWholeFile(true);
  MultiBufferReader reader(&multibuffer_, pos, end,
                           /*is_client_audio_element=*/false,
                           base::NullCallback(), task_runner_);
  reader.SetPinRange(2000, 5000);
  reader.SetPreload(1000, 1000);
  while (pos < end) {
    unsigned char buffer[27];
    buffer[17] = 17;
    size_t to_read = std::min<size_t>(end - pos, 17);
    while (AdvanceAll()) {
    }
    int64_t bytes = reader.TryRead(buffer, to_read);
    EXPECT_GT(bytes, 0);
    EXPECT_EQ(buffer[17], 17);
    for (int64_t i = 0; i < bytes; i++) {
      uint8_t expected = static_cast<uint8_t>((pos * 15485863) >> 16);
      EXPECT_EQ(expected, buffer[i]) << " pos = " << pos;
      pos++;
    }
  }
}

// Checks that if the data provider provides too much data after we told it
// to defer, we kill it.
TEST_F(MultiBufferTest, ReadAllAdvanceFirst_NeverDefer) {
  multibuffer_.SetMaxWriters(1);
  size_t pos = 0;
  size_t end = 10000;
  multibuffer_.SetFileSize(10000);
  multibuffer_.SetMaxBlocksAfterDefer(-10000);
  multibuffer_.SetRangeSupported(true);
  MultiBufferReader reader(&multibuffer_, pos, end,
                           /*is_client_audio_element=*/false,
                           base::NullCallback(), task_runner_);
  reader.SetPinRange(2000, 5000);
  reader.SetPreload(1000, 1000);
  while (pos < end) {
    unsigned char buffer[27];
    buffer[17] = 17;
    size_t to_read = std::min<size_t>(end - pos, 17);
    while (AdvanceAll()) {
    }
    int64_t bytes = reader.TryRead(buffer, to_read);
    EXPECT_GT(bytes, 0);
    EXPECT_EQ(buffer[17], 17);
    for (int64_t i = 0; i < bytes; i++) {
      uint8_t expected = static_cast<uint8_t>((pos * 15485863) >> 16);
      EXPECT_EQ(expected, buffer[i]) << " pos = " << pos;
      pos++;
    }
  }
  EXPECT_GT(multibuffer_.writers_created(), 1);
}

// Same as ReadAllAdvanceFirst_NeverDefer, but the url doesn't support
// ranges, so we don't destroy it no matter how much data it provides.
TEST_F(MultiBufferTest, ReadAllAdvanceFirst_NeverDefer2) {
  multibuffer_.SetMaxWriters(1);
  size_t pos = 0;
  size_t end = 10000;
  multibuffer_.SetFileSize(10000);
  multibuffer_.SetMustReadWholeFile(true);
  multibuffer_.SetMaxBlocksAfterDefer(-10000);
  MultiBufferReader reader(&multibuffer_, pos, end,
                           /*is_client_audio_element=*/false,
                           base::NullCallback(), task_runner_);
  reader.SetPinRange(2000, 5000);
  reader.SetPreload(1000, 1000);
  while (pos < end) {
    unsigned char buffer[27];
    buffer[17] = 17;
    size_t to_read = std::min<size_t>(end - pos, 17);
    while (AdvanceAll()) {
    }
    int64_t bytes = reader.TryRead(buffer, to_read);
    EXPECT_GT(bytes, 0);
    EXPECT_EQ(buffer[17], 17);
    for (int64_t i = 0; i < bytes; i++) {
      uint8_t expected = static_cast<uint8_t>((pos * 15485863) >> 16);
      EXPECT_EQ(expected, buffer[i]) << " pos = " << pos;
      pos++;
    }
  }
}

TEST_F(MultiBufferTest, LRUTest) {
  int64_t max_size = 17;
  int64_t current_size = 0;
  lru_->IncrementMaxSize(max_size);

  multibuffer_.SetMaxWriters(1);
  size_t pos = 0;
  size_t end = 10000;
  multibuffer_.SetFileSize(10000);
  MultiBufferReader reader(&multibuffer_, pos, end,
                           /*is_client_audio_element=*/false,
                           base::NullCallback(), task_runner_);
  reader.SetPreload(10000, 10000);
  // Note, no pinning, all data should end up in LRU.
  EXPECT_EQ(current_size, lru_->Size());
  current_size += max_size;
  while (AdvanceAll()) {
  }
  EXPECT_EQ(current_size, lru_->Size());
  lru_->IncrementMaxSize(-max_size);
  lru_->Prune(3);
  current_size -= 3;
  EXPECT_EQ(current_size, lru_->Size());
  lru_->Prune(3);
  current_size -= 3;
  EXPECT_EQ(current_size, lru_->Size());
  lru_->Prune(1000);
  EXPECT_EQ(0, lru_->Size());
}

TEST_F(MultiBufferTest, LRUTest2) {
  int64_t max_size = 17;
  int64_t current_size = 0;
  lru_->IncrementMaxSize(max_size);

  multibuffer_.SetMaxWriters(1);
  size_t pos = 0;
  size_t end = 10000;
  multibuffer_.SetFileSize(10000);
  MultiBufferReader reader(&multibuffer_, pos, end,
                           /*is_client_audio_element=*/false,
                           base::NullCallback(), task_runner_);
  reader.SetPreload(10000, 10000);
  // Note, no pinning, all data should end up in LRU.
  EXPECT_EQ(current_size, lru_->Size());
  current_size += max_size;
  while (AdvanceAll()) {
  }
  EXPECT_EQ(current_size, lru_->Size());
  // Pruning shouldn't do anything here, because LRU is small enough already.
  lru_->Prune(3);
  EXPECT_EQ(current_size, lru_->Size());
  // However TryFree should still work
  lru_->TryFree(3);
  current_size -= 3;
  EXPECT_EQ(current_size, lru_->Size());
  lru_->TryFreeAll();
  EXPECT_EQ(0, lru_->Size());
  lru_->IncrementMaxSize(-max_size);
}

TEST_F(MultiBufferTest, LRUTestExpirationTest) {
  int64_t max_size = 17;
  int64_t current_size = 0;
  lru_->IncrementMaxSize(max_size);

  multibuffer_.SetMaxWriters(1);
  size_t pos = 0;
  size_t end = 10000;
  multibuffer_.SetFileSize(10000);
  MultiBufferReader reader(&multibuffer_, pos, end,
                           /*is_client_audio_element=*/false,
                           base::NullCallback(), task_runner_);
  reader.SetPreload(10000, 10000);
  // Note, no pinning, all data should end up in LRU.
  EXPECT_EQ(current_size, lru_->Size());
  current_size += max_size;
  while (AdvanceAll()) {
  }
  EXPECT_EQ(current_size, lru_->Size());
  EXPECT_FALSE(lru_->Pruneable());

  // Make 3 packets pruneable.
  lru_->IncrementMaxSize(-3);
  max_size -= 3;

  // There should be no change after 29 seconds.
  task_runner_->Sleep(base::Seconds(29));
  EXPECT_EQ(current_size, lru_->Size());
  EXPECT_TRUE(lru_->Pruneable());

  // After 30 seconds, pruning should have happened.
  task_runner_->Sleep(base::Seconds(30));
  current_size -= 3;
  EXPECT_EQ(current_size, lru_->Size());
  EXPECT_FALSE(lru_->Pruneable());

  // Make the rest of the packets pruneable.
  lru_->IncrementMaxSize(-max_size);

  // After another 30 seconds, everything should be pruned.
  task_runner_->Sleep(base::Seconds(30));
  EXPECT_EQ(0, lru_->Size());
  EXPECT_FALSE(lru_->Pruneable());
}

class ReadHelper {
 public:
  ReadHelper(size_t end,
             size_t max_read_size,
             MultiBuffer* multibuffer,
             media::TestRandom* rnd,
             scoped_refptr<base::SingleThreadTaskRunner> task_runner)
      : pos_(0),
        end_(end),
        max_read_size_(max_read_size),
        read_size_(0),
        rnd_(rnd),
        reader_(multibuffer,
                pos_,
                end_,
                /*is_client_audio_element=*/false,
                base::NullCallback(),
                std::move(task_runner)) {
    reader_.SetPinRange(2000, 5000);
    reader_.SetPreload(1000, 1000);
  }

  bool Read() {
    if (read_size_ == 0)
      return true;
    unsigned char buffer[4096];
    CHECK_LE(read_size_, static_cast<int64_t>(sizeof(buffer)));
    CHECK_EQ(pos_, reader_.Tell());
    int64_t bytes_read = reader_.TryRead(buffer, read_size_);
    if (bytes_read) {
      for (int64_t i = 0; i < bytes_read; i++) {
        unsigned char expected = (pos_ * 15485863) >> 16;
        EXPECT_EQ(expected, buffer[i]) << " pos = " << pos_;
        pos_++;
      }
      CHECK_EQ(pos_, reader_.Tell());
      return true;
    }
    return false;
  }

  void StartRead() {
    CHECK_EQ(pos_, reader_.Tell());
    read_size_ = std::min(1 + rnd_->Rand() % (max_read_size_ - 1), end_ - pos_);
    if (!Read()) {
      reader_.Wait(read_size_,
                   base::BindOnce(&ReadHelper::WaitCB, base::Unretained(this)));
    }
  }

  void WaitCB() { CHECK(Read()); }

  void Seek() {
    pos_ = rnd_->Rand() % end_;
    reader_.Seek(pos_);
    CHECK_EQ(pos_, reader_.Tell());
  }

 private:
  int64_t pos_;
  int64_t end_;
  int64_t max_read_size_;
  int64_t read_size_;
  raw_ptr<media::TestRandom> rnd_;
  MultiBufferReader reader_;
};

TEST_F(MultiBufferTest, RandomTest) {
  size_t file_size = 1000000;
  multibuffer_.SetFileSize(file_size);
  multibuffer_.SetMaxBlocksAfterDefer(10);
  std::vector<std::unique_ptr<ReadHelper>> read_helpers;
  for (size_t i = 0; i < 20; i++) {
    read_helpers.push_back(std::make_unique<ReadHelper>(
        file_size, 1000, &multibuffer_, &rnd_, task_runner_));
  }
  for (int i = 0; i < 100; i++) {
    for (int j = 0; j < 100; j++) {
      if (rnd_.Rand() & 1) {
        if (!writers.empty())
          Advance();
      } else {
        size_t k = rnd_.Rand() % read_helpers.size();
        if (rnd_.Rand() % 100 < 3)
          read_helpers[k]->Seek();
        read_helpers[k]->StartRead();
      }
    }
    multibuffer_.CheckLRUState();
  }
  multibuffer_.CheckPresentState();
}

TEST_F(MultiBufferTest, RandomTest_RangeSupported) {
  size_t file_size = 1000000;
  multibuffer_.SetFileSize(file_size);
  multibuffer_.SetMaxBlocksAfterDefer(10);
  std::vector<std::unique_ptr<ReadHelper>> read_helpers;
  multibuffer_.SetRangeSupported(true);
  for (size_t i = 0; i < 20; i++) {
    read_helpers.push_back(std::make_unique<ReadHelper>(
        file_size, 1000, &multibuffer_, &rnd_, task_runner_));
  }
  for (int i = 0; i < 100; i++) {
    for (int j = 0; j < 100; j++) {
      if (rnd_.Rand() & 1) {
        if (!writers.empty())
          Advance();
      } else {
        size_t k = rnd_.Rand() % read_helpers.size();
        if (rnd_.Rand() % 100 < 3)
          read_helpers[k]->Seek();
        read_helpers[k]->StartRead();
      }
    }
    multibuffer_.CheckLRUState();
  }
  multibuffer_.CheckPresentState();
}

}  // namespace blink
