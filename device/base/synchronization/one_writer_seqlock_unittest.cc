// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "device/base/synchronization/one_writer_seqlock.h"

#include <stdlib.h>
#include <atomic>

#include "base/memory/raw_ptr.h"
#include "base/threading/platform_thread.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/base/dynamic_annotations.h"

namespace device {

// Basic test to make sure that basic operation works correctly.

struct TestData {
  // Data copies larger than a cache line.
  uint32_t buffer[32];
};

class BasicSeqLockTestThread : public base::PlatformThread::Delegate {
 public:
  BasicSeqLockTestThread() = default;

  BasicSeqLockTestThread(const BasicSeqLockTestThread&) = delete;
  BasicSeqLockTestThread& operator=(const BasicSeqLockTestThread&) = delete;

  void Init(OneWriterSeqLock* seqlock,
            TestData* data,
            std::atomic<int>* ready) {
    seqlock_ = seqlock;
    data_ = data;
    ready_ = ready;
  }
  void ThreadMain() override {
    while (!*ready_) {
      base::PlatformThread::YieldCurrentThread();
    }

    for (unsigned i = 0; i < 1000; ++i) {
      TestData copy;
      base::subtle::Atomic32 version;
      do {
        version = seqlock_->ReadBegin();
        OneWriterSeqLock::AtomicReaderMemcpy(&copy, data_.get(),
                                             sizeof(TestData));
      } while (seqlock_->ReadRetry(version));

      for (unsigned j = 1; j < 32; ++j)
        EXPECT_EQ(copy.buffer[j], copy.buffer[0] + copy.buffer[j - 1]);
    }

    --(*ready_);
  }

 private:
  raw_ptr<OneWriterSeqLock> seqlock_;
  raw_ptr<TestData> data_;
  raw_ptr<std::atomic<int>> ready_;
};

class MaxRetriesSeqLockTestThread : public base::PlatformThread::Delegate {
 public:
  MaxRetriesSeqLockTestThread() = default;

  MaxRetriesSeqLockTestThread(const MaxRetriesSeqLockTestThread&) = delete;
  MaxRetriesSeqLockTestThread& operator=(const MaxRetriesSeqLockTestThread&) =
      delete;

  void Init(OneWriterSeqLock* seqlock, std::atomic<int>* ready) {
    seqlock_ = seqlock;
    ready_ = ready;
  }
  void ThreadMain() override {
    while (!*ready_) {
      base::PlatformThread::YieldCurrentThread();
    }

    for (unsigned i = 0; i < 10; ++i) {
      base::subtle::Atomic32 version;
      version = seqlock_->ReadBegin(100);

      EXPECT_NE(version & 1, 0);
    }

    --*ready_;
  }

 private:
  raw_ptr<OneWriterSeqLock> seqlock_;
  raw_ptr<std::atomic<int>> ready_;
};

#if BUILDFLAG(IS_ANDROID)
#define MAYBE_ManyThreads FLAKY_ManyThreads
#else
#define MAYBE_ManyThreads ManyThreads
#endif
TEST(OneWriterSeqLockTest, MAYBE_ManyThreads) {
  OneWriterSeqLock seqlock;
  TestData data;
  std::atomic<int> ready(0);

  ABSL_ANNOTATE_BENIGN_RACE_SIZED(&data, sizeof(data), "Racey reads are discarded");

  static const unsigned kNumReaderThreads = 10;
  BasicSeqLockTestThread threads[kNumReaderThreads];
  base::PlatformThreadHandle handles[kNumReaderThreads];

  for (uint32_t i = 0; i < kNumReaderThreads; ++i)
    threads[i].Init(&seqlock, &data, &ready);
  for (uint32_t i = 0; i < kNumReaderThreads; ++i)
    ASSERT_TRUE(base::PlatformThread::Create(0, &threads[i], &handles[i]));

  // The main thread is the writer, and the spawned are readers.
  uint32_t counter = 0;
  for (;;) {
    TestData new_data;
    new_data.buffer[0] = counter++;
    for (unsigned i = 1; i < 32; ++i) {
      new_data.buffer[i] = new_data.buffer[0] + new_data.buffer[i - 1];
    }
    seqlock.WriteBegin();
    OneWriterSeqLock::AtomicWriterMemcpy(&data, &new_data, sizeof(TestData));
    seqlock.WriteEnd();

    if (counter == 1)
      ready += kNumReaderThreads;

    if (!ready)
      break;
  }

  for (unsigned i = 0; i < kNumReaderThreads; ++i)
    base::PlatformThread::Join(handles[i]);
}

TEST(OneWriterSeqLockTest, MaxRetries) {
  OneWriterSeqLock seqlock;
  std::atomic<int> ready(0);

  static const unsigned kNumReaderThreads = 3;
  MaxRetriesSeqLockTestThread threads[kNumReaderThreads];
  base::PlatformThreadHandle handles[kNumReaderThreads];

  for (uint32_t i = 0; i < kNumReaderThreads; ++i)
    threads[i].Init(&seqlock, &ready);
  for (uint32_t i = 0; i < kNumReaderThreads; ++i)
    ASSERT_TRUE(base::PlatformThread::Create(0, &threads[i], &handles[i]));

  // The main thread is the writer, and the spawned are readers.
  seqlock.WriteBegin();
  ready += kNumReaderThreads;
  while (ready) {
    base::PlatformThread::YieldCurrentThread();
  }
  seqlock.WriteEnd();

  for (unsigned i = 0; i < kNumReaderThreads; ++i)
    base::PlatformThread::Join(handles[i]);
}

}  // namespace device
