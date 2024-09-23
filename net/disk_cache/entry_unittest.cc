// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include <utility>

#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_param_associator.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "net/base/completion_once_callback.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/request_priority.h"
#include "net/base/test_completion_callback.h"
#include "net/disk_cache/blockfile/backend_impl.h"
#include "net/disk_cache/blockfile/entry_impl.h"
#include "net/disk_cache/cache_util.h"
#include "net/disk_cache/disk_cache_test_base.h"
#include "net/disk_cache/disk_cache_test_util.h"
#include "net/disk_cache/memory/mem_entry_impl.h"
#include "net/disk_cache/simple/simple_backend_impl.h"
#include "net/disk_cache/simple/simple_entry_format.h"
#include "net/disk_cache/simple/simple_entry_impl.h"
#include "net/disk_cache/simple/simple_histogram_enums.h"
#include "net/disk_cache/simple/simple_synchronous_entry.h"
#include "net/disk_cache/simple/simple_test_util.h"
#include "net/disk_cache/simple/simple_util.h"
#include "net/test/gtest_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using net::test::IsError;
using net::test::IsOk;

using base::Time;
using disk_cache::EntryResult;
using disk_cache::EntryResultCallback;
using disk_cache::RangeResult;
using disk_cache::ScopedEntryPtr;

// Tests that can run with different types of caches.
class DiskCacheEntryTest : public DiskCacheTestWithCache {
 public:
  void InternalSyncIOBackground(disk_cache::Entry* entry);
  void ExternalSyncIOBackground(disk_cache::Entry* entry);

 protected:
  void InternalSyncIO();
  void InternalAsyncIO();
  void ExternalSyncIO();
  void ExternalAsyncIO();
  void ReleaseBuffer(int stream_index);
  void StreamAccess();
  void GetKey();
  void GetTimes(int stream_index);
  void GrowData(int stream_index);
  void TruncateData(int stream_index);
  void ZeroLengthIO(int stream_index);
  void Buffering();
  void SizeAtCreate();
  void SizeChanges(int stream_index);
  void ReuseEntry(int size, int stream_index);
  void InvalidData(int stream_index);
  void ReadWriteDestroyBuffer(int stream_index);
  void DoomNormalEntry();
  void DoomEntryNextToOpenEntry();
  void DoomedEntry(int stream_index);
  void BasicSparseIO();
  void HugeSparseIO();
  void GetAvailableRangeTest();
  void CouldBeSparse();
  void UpdateSparseEntry();
  void DoomSparseEntry();
  void PartialSparseEntry();
  void SparseInvalidArg();
  void SparseClipEnd(int64_t max_index, bool expected_unsupported);
  bool SimpleCacheMakeBadChecksumEntry(const std::string& key, int data_size);
  bool SimpleCacheThirdStreamFileExists(const char* key);
  void SyncDoomEntry(const char* key);
  void CreateEntryWithHeaderBodyAndSideData(const std::string& key,
                                            int data_size);
  void TruncateFileFromEnd(int file_index,
                           const std::string& key,
                           int data_size,
                           int truncate_size);
  void UseAfterBackendDestruction();
  void CloseSparseAfterBackendDestruction();
  void LastUsedTimePersists();
  void TruncateBackwards();
  void ZeroWriteBackwards();
  void SparseOffset64Bit();
};

// This part of the test runs on the background thread.
void DiskCacheEntryTest::InternalSyncIOBackground(disk_cache::Entry* entry) {
  const int kSize1 = 10;
  auto buffer1 = base::MakeRefCounted<net::IOBufferWithSize>(kSize1);
  CacheTestFillBuffer(buffer1->data(), kSize1, false);
  EXPECT_EQ(0, entry->ReadData(0, 0, buffer1.get(), kSize1,
                               net::CompletionOnceCallback()));
  base::strlcpy(buffer1->data(), "the data", kSize1);
  EXPECT_EQ(10, entry->WriteData(0, 0, buffer1.get(), kSize1,
                                 net::CompletionOnceCallback(), false));
  memset(buffer1->data(), 0, kSize1);
  EXPECT_EQ(10, entry->ReadData(0, 0, buffer1.get(), kSize1,
                                net::CompletionOnceCallback()));
  EXPECT_STREQ("the data", buffer1->data());

  const int kSize2 = 5000;
  const int kSize3 = 10000;
  auto buffer2 = base::MakeRefCounted<net::IOBufferWithSize>(kSize2);
  auto buffer3 = base::MakeRefCounted<net::IOBufferWithSize>(kSize3);
  memset(buffer3->data(), 0, kSize3);
  CacheTestFillBuffer(buffer2->data(), kSize2, false);
  base::strlcpy(buffer2->data(), "The really big data goes here", kSize2);
  EXPECT_EQ(5000, entry->WriteData(1, 1500, buffer2.get(), kSize2,
                                   net::CompletionOnceCallback(), false));
  memset(buffer2->data(), 0, kSize2);
  EXPECT_EQ(4989, entry->ReadData(1, 1511, buffer2.get(), kSize2,
                                  net::CompletionOnceCallback()));
  EXPECT_STREQ("big data goes here", buffer2->data());
  EXPECT_EQ(5000, entry->ReadData(1, 0, buffer2.get(), kSize2,
                                  net::CompletionOnceCallback()));
  EXPECT_EQ(0, memcmp(buffer2->data(), buffer3->data(), 1500));
  EXPECT_EQ(1500, entry->ReadData(1, 5000, buffer2.get(), kSize2,
                                  net::CompletionOnceCallback()));

  EXPECT_EQ(0, entry->ReadData(1, 6500, buffer2.get(), kSize2,
                               net::CompletionOnceCallback()));
  EXPECT_EQ(6500, entry->ReadData(1, 0, buffer3.get(), kSize3,
                                  net::CompletionOnceCallback()));
  EXPECT_EQ(8192, entry->WriteData(1, 0, buffer3.get(), 8192,
                                   net::CompletionOnceCallback(), false));
  EXPECT_EQ(8192, entry->ReadData(1, 0, buffer3.get(), kSize3,
                                  net::CompletionOnceCallback()));
  EXPECT_EQ(8192, entry->GetDataSize(1));

  // We need to delete the memory buffer on this thread.
  EXPECT_EQ(0, entry->WriteData(0, 0, nullptr, 0, net::CompletionOnceCallback(),
                                true));
  EXPECT_EQ(0, entry->WriteData(1, 0, nullptr, 0, net::CompletionOnceCallback(),
                                true));
}

// We need to support synchronous IO even though it is not a supported operation
// from the point of view of the disk cache's public interface, because we use
// it internally, not just by a few tests, but as part of the implementation
// (see sparse_control.cc, for example).
void DiskCacheEntryTest::InternalSyncIO() {
  disk_cache::Entry* entry = nullptr;
  ASSERT_THAT(CreateEntry("the first key", &entry), IsOk());
  ASSERT_TRUE(nullptr != entry);

  // The bulk of the test runs from within the callback, on the cache thread.
  RunTaskForTest(base::BindOnce(&DiskCacheEntryTest::InternalSyncIOBackground,
                                base::Unretained(this), entry));

  entry->Doom();
  entry->Close();
  FlushQueueForTest();
  EXPECT_EQ(0, cache_->GetEntryCount());
}

TEST_F(DiskCacheEntryTest, InternalSyncIO) {
  InitCache();
  InternalSyncIO();
}

TEST_F(DiskCacheEntryTest, MemoryOnlyInternalSyncIO) {
  SetMemoryOnlyMode();
  InitCache();
  InternalSyncIO();
}

void DiskCacheEntryTest::InternalAsyncIO() {
  disk_cache::Entry* entry = nullptr;
  ASSERT_THAT(CreateEntry("the first key", &entry), IsOk());
  ASSERT_TRUE(nullptr != entry);

  // Avoid using internal buffers for the test. We have to write something to
  // the entry and close it so that we flush the internal buffer to disk. After
  // that, IO operations will be really hitting the disk. We don't care about
  // the content, so just extending the entry is enough (all extensions zero-
  // fill any holes).
  EXPECT_EQ(0, WriteData(entry, 0, 15 * 1024, nullptr, 0, false));
  EXPECT_EQ(0, WriteData(entry, 1, 15 * 1024, nullptr, 0, false));
  entry->Close();
  ASSERT_THAT(OpenEntry("the first key", &entry), IsOk());

  MessageLoopHelper helper;
  // Let's verify that each IO goes to the right callback object.
  CallbackTest callback1(&helper, false);
  CallbackTest callback2(&helper, false);
  CallbackTest callback3(&helper, false);
  CallbackTest callback4(&helper, false);
  CallbackTest callback5(&helper, false);
  CallbackTest callback6(&helper, false);
  CallbackTest callback7(&helper, false);
  CallbackTest callback8(&helper, false);
  CallbackTest callback9(&helper, false);
  CallbackTest callback10(&helper, false);
  CallbackTest callback11(&helper, false);
  CallbackTest callback12(&helper, false);
  CallbackTest callback13(&helper, false);

  const int kSize1 = 10;
  const int kSize2 = 5000;
  const int kSize3 = 10000;
  auto buffer1 = base::MakeRefCounted<net::IOBufferWithSize>(kSize1);
  auto buffer2 = base::MakeRefCounted<net::IOBufferWithSize>(kSize2);
  auto buffer3 = base::MakeRefCounted<net::IOBufferWithSize>(kSize3);
  CacheTestFillBuffer(buffer1->data(), kSize1, false);
  CacheTestFillBuffer(buffer2->data(), kSize2, false);
  CacheTestFillBuffer(buffer3->data(), kSize3, false);

  EXPECT_EQ(0, entry->ReadData(0, 15 * 1024, buffer1.get(), kSize1,
                               base::BindOnce(&CallbackTest::Run,
                                              base::Unretained(&callback1))));
  base::strlcpy(buffer1->data(), "the data", kSize1);
  int expected = 0;
  int ret = entry->WriteData(
      0, 0, buffer1.get(), kSize1,
      base::BindOnce(&CallbackTest::Run, base::Unretained(&callback2)), false);
  EXPECT_TRUE(10 == ret || net::ERR_IO_PENDING == ret);
  if (net::ERR_IO_PENDING == ret)
    expected++;

  EXPECT_TRUE(helper.WaitUntilCacheIoFinished(expected));
  memset(buffer2->data(), 0, kSize2);
  ret = entry->ReadData(
      0, 0, buffer2.get(), kSize1,
      base::BindOnce(&CallbackTest::Run, base::Unretained(&callback3)));
  EXPECT_TRUE(10 == ret || net::ERR_IO_PENDING == ret);
  if (net::ERR_IO_PENDING == ret)
    expected++;

  EXPECT_TRUE(helper.WaitUntilCacheIoFinished(expected));
  EXPECT_STREQ("the data", buffer2->data());

  base::strlcpy(buffer2->data(), "The really big data goes here", kSize2);
  ret = entry->WriteData(
      1, 1500, buffer2.get(), kSize2,
      base::BindOnce(&CallbackTest::Run, base::Unretained(&callback4)), true);
  EXPECT_TRUE(5000 == ret || net::ERR_IO_PENDING == ret);
  if (net::ERR_IO_PENDING == ret)
    expected++;

  EXPECT_TRUE(helper.WaitUntilCacheIoFinished(expected));
  memset(buffer3->data(), 0, kSize3);
  ret = entry->ReadData(
      1, 1511, buffer3.get(), kSize2,
      base::BindOnce(&CallbackTest::Run, base::Unretained(&callback5)));
  EXPECT_TRUE(4989 == ret || net::ERR_IO_PENDING == ret);
  if (net::ERR_IO_PENDING == ret)
    expected++;

  EXPECT_TRUE(helper.WaitUntilCacheIoFinished(expected));
  EXPECT_STREQ("big data goes here", buffer3->data());
  ret = entry->ReadData(
      1, 0, buffer2.get(), kSize2,
      base::BindOnce(&CallbackTest::Run, base::Unretained(&callback6)));
  EXPECT_TRUE(5000 == ret || net::ERR_IO_PENDING == ret);
  if (net::ERR_IO_PENDING == ret)
    expected++;

  memset(buffer3->data(), 0, kSize3);

  EXPECT_TRUE(helper.WaitUntilCacheIoFinished(expected));
  EXPECT_EQ(0, memcmp(buffer2->data(), buffer3->data(), 1500));
  ret = entry->ReadData(
      1, 5000, buffer2.get(), kSize2,
      base::BindOnce(&CallbackTest::Run, base::Unretained(&callback7)));
  EXPECT_TRUE(1500 == ret || net::ERR_IO_PENDING == ret);
  if (net::ERR_IO_PENDING == ret)
    expected++;

  ret = entry->ReadData(
      1, 0, buffer3.get(), kSize3,
      base::BindOnce(&CallbackTest::Run, base::Unretained(&callback9)));
  EXPECT_TRUE(6500 == ret || net::ERR_IO_PENDING == ret);
  if (net::ERR_IO_PENDING == ret)
    expected++;

  ret = entry->WriteData(
      1, 0, buffer3.get(), 8192,
      base::BindOnce(&CallbackTest::Run, base::Unretained(&callback10)), true);
  EXPECT_TRUE(8192 == ret || net::ERR_IO_PENDING == ret);
  if (net::ERR_IO_PENDING == ret)
    expected++;

  EXPECT_TRUE(helper.WaitUntilCacheIoFinished(expected));
  ret = entry->ReadData(
      1, 0, buffer3.get(), kSize3,
      base::BindOnce(&CallbackTest::Run, base::Unretained(&callback11)));
  EXPECT_TRUE(8192 == ret || net::ERR_IO_PENDING == ret);
  if (net::ERR_IO_PENDING == ret)
    expected++;

  EXPECT_EQ(8192, entry->GetDataSize(1));

  ret = entry->ReadData(
      0, 0, buffer1.get(), kSize1,
      base::BindOnce(&CallbackTest::Run, base::Unretained(&callback12)));
  EXPECT_TRUE(10 == ret || net::ERR_IO_PENDING == ret);
  if (net::ERR_IO_PENDING == ret)
    expected++;

  ret = entry->ReadData(
      1, 0, buffer2.get(), kSize2,
      base::BindOnce(&CallbackTest::Run, base::Unretained(&callback13)));
  EXPECT_TRUE(5000 == ret || net::ERR_IO_PENDING == ret);
  if (net::ERR_IO_PENDING == ret)
    expected++;

  EXPECT_TRUE(helper.WaitUntilCacheIoFinished(expected));

  EXPECT_FALSE(helper.callback_reused_error());

  entry->Doom();
  entry->Close();
  FlushQueueForTest();
  EXPECT_EQ(0, cache_->GetEntryCount());
}

TEST_F(DiskCacheEntryTest, InternalAsyncIO) {
  InitCache();
  InternalAsyncIO();
}

TEST_F(DiskCacheEntryTest, MemoryOnlyInternalAsyncIO) {
  SetMemoryOnlyMode();
  InitCache();
  InternalAsyncIO();
}

// This part of the test runs on the background thread.
void DiskCacheEntryTest::ExternalSyncIOBackground(disk_cache::Entry* entry) {
  const int kSize1 = 17000;
  const int kSize2 = 25000;
  auto buffer1 = base::MakeRefCounted<net::IOBufferWithSize>(kSize1);
  auto buffer2 = base::MakeRefCounted<net::IOBufferWithSize>(kSize2);
  CacheTestFillBuffer(buffer1->data(), kSize1, false);
  CacheTestFillBuffer(buffer2->data(), kSize2, false);
  base::strlcpy(buffer1->data(), "the data", kSize1);
  EXPECT_EQ(17000, entry->WriteData(0, 0, buffer1.get(), kSize1,
                                    net::CompletionOnceCallback(), false));
  memset(buffer1->data(), 0, kSize1);
  EXPECT_EQ(17000, entry->ReadData(0, 0, buffer1.get(), kSize1,
                                   net::CompletionOnceCallback()));
  EXPECT_STREQ("the data", buffer1->data());

  base::strlcpy(buffer2->data(), "The really big data goes here", kSize2);
  EXPECT_EQ(25000, entry->WriteData(1, 10000, buffer2.get(), kSize2,
                                    net::CompletionOnceCallback(), false));
  memset(buffer2->data(), 0, kSize2);
  EXPECT_EQ(24989, entry->ReadData(1, 10011, buffer2.get(), kSize2,
                                   net::CompletionOnceCallback()));
  EXPECT_STREQ("big data goes here", buffer2->data());
  EXPECT_EQ(25000, entry->ReadData(1, 0, buffer2.get(), kSize2,
                                   net::CompletionOnceCallback()));
  EXPECT_EQ(5000, entry->ReadData(1, 30000, buffer2.get(), kSize2,
                                  net::CompletionOnceCallback()));

  EXPECT_EQ(0, entry->ReadData(1, 35000, buffer2.get(), kSize2,
                               net::CompletionOnceCallback()));
  EXPECT_EQ(17000, entry->ReadData(1, 0, buffer1.get(), kSize1,
                                   net::CompletionOnceCallback()));
  EXPECT_EQ(17000, entry->WriteData(1, 20000, buffer1.get(), kSize1,
                                    net::CompletionOnceCallback(), false));
  EXPECT_EQ(37000, entry->GetDataSize(1));

  // We need to delete the memory buffer on this thread.
  EXPECT_EQ(0, entry->WriteData(0, 0, nullptr, 0, net::CompletionOnceCallback(),
                                true));
  EXPECT_EQ(0, entry->WriteData(1, 0, nullptr, 0, net::CompletionOnceCallback(),
                                true));
}

void DiskCacheEntryTest::ExternalSyncIO() {
  disk_cache::Entry* entry;
  ASSERT_THAT(CreateEntry("the first key", &entry), IsOk());

  // The bulk of the test runs from within the callback, on the cache thread.
  RunTaskForTest(base::BindOnce(&DiskCacheEntryTest::ExternalSyncIOBackground,
                                base::Unretained(this), entry));

  entry->Doom();
  entry->Close();
  FlushQueueForTest();
  EXPECT_EQ(0, cache_->GetEntryCount());
}

TEST_F(DiskCacheEntryTest, ExternalSyncIO) {
  InitCache();
  ExternalSyncIO();
}

TEST_F(DiskCacheEntryTest, ExternalSyncIONoBuffer) {
  InitCache();
  cache_impl_->SetFlags(disk_cache::kNoBuffering);
  ExternalSyncIO();
}

TEST_F(DiskCacheEntryTest, MemoryOnlyExternalSyncIO) {
  SetMemoryOnlyMode();
  InitCache();
  ExternalSyncIO();
}

void DiskCacheEntryTest::ExternalAsyncIO() {
  disk_cache::Entry* entry;
  ASSERT_THAT(CreateEntry("the first key", &entry), IsOk());

  int expected = 0;

  MessageLoopHelper helper;
  // Let's verify that each IO goes to the right callback object.
  CallbackTest callback1(&helper, false);
  CallbackTest callback2(&helper, false);
  CallbackTest callback3(&helper, false);
  CallbackTest callback4(&helper, false);
  CallbackTest callback5(&helper, false);
  CallbackTest callback6(&helper, false);
  CallbackTest callback7(&helper, false);
  CallbackTest callback8(&helper, false);
  CallbackTest callback9(&helper, false);

  const int kSize1 = 17000;
  const int kSize2 = 25000;
  const int kSize3 = 25000;
  auto buffer1 = base::MakeRefCounted<net::IOBufferWithSize>(kSize1);
  auto buffer2 = base::MakeRefCounted<net::IOBufferWithSize>(kSize2);
  auto buffer3 = base::MakeRefCounted<net::IOBufferWithSize>(kSize3);
  CacheTestFillBuffer(buffer1->data(), kSize1, false);
  CacheTestFillBuffer(buffer2->data(), kSize2, false);
  CacheTestFillBuffer(buffer3->data(), kSize3, false);
  base::strlcpy(buffer1->data(), "the data", kSize1);
  int ret = entry->WriteData(
      0, 0, buffer1.get(), kSize1,
      base::BindOnce(&CallbackTest::Run, base::Unretained(&callback1)), false);
  EXPECT_TRUE(17000 == ret || net::ERR_IO_PENDING == ret);
  if (net::ERR_IO_PENDING == ret)
    expected++;

  EXPECT_TRUE(helper.WaitUntilCacheIoFinished(expected));

  memset(buffer2->data(), 0, kSize1);
  ret = entry->ReadData(
      0, 0, buffer2.get(), kSize1,
      base::BindOnce(&CallbackTest::Run, base::Unretained(&callback2)));
  EXPECT_TRUE(17000 == ret || net::ERR_IO_PENDING == ret);
  if (net::ERR_IO_PENDING == ret)
    expected++;

  EXPECT_TRUE(helper.WaitUntilCacheIoFinished(expected));
  EXPECT_STREQ("the data", buffer2->data());

  base::strlcpy(buffer2->data(), "The really big data goes here", kSize2);
  ret = entry->WriteData(
      1, 10000, buffer2.get(), kSize2,
      base::BindOnce(&CallbackTest::Run, base::Unretained(&callback3)), false);
  EXPECT_TRUE(25000 == ret || net::ERR_IO_PENDING == ret);
  if (net::ERR_IO_PENDING == ret)
    expected++;

  EXPECT_TRUE(helper.WaitUntilCacheIoFinished(expected));

  memset(buffer3->data(), 0, kSize3);
  ret = entry->ReadData(
      1, 10011, buffer3.get(), kSize3,
      base::BindOnce(&CallbackTest::Run, base::Unretained(&callback4)));
  EXPECT_TRUE(24989 == ret || net::ERR_IO_PENDING == ret);
  if (net::ERR_IO_PENDING == ret)
    expected++;

  EXPECT_TRUE(helper.WaitUntilCacheIoFinished(expected));
  EXPECT_STREQ("big data goes here", buffer3->data());
  ret = entry->ReadData(
      1, 0, buffer2.get(), kSize2,
      base::BindOnce(&CallbackTest::Run, base::Unretained(&callback5)));
  EXPECT_TRUE(25000 == ret || net::ERR_IO_PENDING == ret);
  if (net::ERR_IO_PENDING == ret)
    expected++;

  EXPECT_TRUE(helper.WaitUntilCacheIoFinished(expected));
  memset(buffer3->data(), 0, kSize3);
  EXPECT_EQ(0, memcmp(buffer2->data(), buffer3->data(), 10000));
  ret = entry->ReadData(
      1, 30000, buffer2.get(), kSize2,
      base::BindOnce(&CallbackTest::Run, base::Unretained(&callback6)));
  EXPECT_TRUE(5000 == ret || net::ERR_IO_PENDING == ret);
  if (net::ERR_IO_PENDING == ret)
    expected++;

  ret = entry->ReadData(
      1, 35000, buffer2.get(), kSize2,
      base::BindOnce(&CallbackTest::Run, base::Unretained(&callback7)));
  EXPECT_TRUE(0 == ret || net::ERR_IO_PENDING == ret);
  if (net::ERR_IO_PENDING == ret)
    expected++;

  ret = entry->ReadData(
      1, 0, buffer1.get(), kSize1,
      base::BindOnce(&CallbackTest::Run, base::Unretained(&callback8)));
  EXPECT_TRUE(17000 == ret || net::ERR_IO_PENDING == ret);
  if (net::ERR_IO_PENDING == ret)
    expected++;
  ret = entry->WriteData(
      1, 20000, buffer3.get(), kSize1,
      base::BindOnce(&CallbackTest::Run, base::Unretained(&callback9)), false);
  EXPECT_TRUE(17000 == ret || net::ERR_IO_PENDING == ret);
  if (net::ERR_IO_PENDING == ret)
    expected++;

  EXPECT_TRUE(helper.WaitUntilCacheIoFinished(expected));
  EXPECT_EQ(37000, entry->GetDataSize(1));

  EXPECT_FALSE(helper.callback_reused_error());

  entry->Doom();
  entry->Close();
  FlushQueueForTest();
  EXPECT_EQ(0, cache_->GetEntryCount());
}

TEST_F(DiskCacheEntryTest, ExternalAsyncIO) {
  InitCache();
  ExternalAsyncIO();
}

// TODO(http://crbug.com/497101): This test is flaky.
#if BUILDFLAG(IS_IOS)
#define MAYBE_ExternalAsyncIONoBuffer DISABLED_ExternalAsyncIONoBuffer
#else
#define MAYBE_ExternalAsyncIONoBuffer ExternalAsyncIONoBuffer
#endif
TEST_F(DiskCacheEntryTest, MAYBE_ExternalAsyncIONoBuffer) {
  InitCache();
  cache_impl_->SetFlags(disk_cache::kNoBuffering);
  ExternalAsyncIO();
}

TEST_F(DiskCacheEntryTest, MemoryOnlyExternalAsyncIO) {
  SetMemoryOnlyMode();
  InitCache();
  ExternalAsyncIO();
}

// Tests that IOBuffers are not referenced after IO completes.
void DiskCacheEntryTest::ReleaseBuffer(int stream_index) {
  disk_cache::Entry* entry = nullptr;
  ASSERT_THAT(CreateEntry("the first key", &entry), IsOk());
  ASSERT_TRUE(nullptr != entry);

  const int kBufferSize = 1024;
  auto buffer = base::MakeRefCounted<net::IOBufferWithSize>(kBufferSize);
  CacheTestFillBuffer(buffer->data(), kBufferSize, false);

  net::ReleaseBufferCompletionCallback cb(buffer.get());
  int rv = entry->WriteData(
      stream_index, 0, buffer.get(), kBufferSize, cb.callback(), false);
  EXPECT_EQ(kBufferSize, cb.GetResult(rv));
  entry->Close();
}

TEST_F(DiskCacheEntryTest, ReleaseBuffer) {
  InitCache();
  cache_impl_->SetFlags(disk_cache::kNoBuffering);
  ReleaseBuffer(0);
}

TEST_F(DiskCacheEntryTest, MemoryOnlyReleaseBuffer) {
  SetMemoryOnlyMode();
  InitCache();
  ReleaseBuffer(0);
}

void DiskCacheEntryTest::StreamAccess() {
  disk_cache::Entry* entry = nullptr;
  ASSERT_THAT(CreateEntry("the first key", &entry), IsOk());
  ASSERT_TRUE(nullptr != entry);

  const int kBufferSize = 1024;
  const int kNumStreams = 3;
  scoped_refptr<net::IOBuffer> reference_buffers[kNumStreams];
  for (auto& reference_buffer : reference_buffers) {
    reference_buffer = base::MakeRefCounted<net::IOBufferWithSize>(kBufferSize);
    CacheTestFillBuffer(reference_buffer->data(), kBufferSize, false);
  }
  auto buffer1 = base::MakeRefCounted<net::IOBufferWithSize>(kBufferSize);
  for (int i = 0; i < kNumStreams; i++) {
    EXPECT_EQ(
        kBufferSize,
        WriteData(entry, i, 0, reference_buffers[i].get(), kBufferSize, false));
    memset(buffer1->data(), 0, kBufferSize);
    EXPECT_EQ(kBufferSize, ReadData(entry, i, 0, buffer1.get(), kBufferSize));
    EXPECT_EQ(
        0, memcmp(reference_buffers[i]->data(), buffer1->data(), kBufferSize));
  }
  EXPECT_EQ(net::ERR_INVALID_ARGUMENT,
            ReadData(entry, kNumStreams, 0, buffer1.get(), kBufferSize));
  entry->Close();

  // Open the entry and read it in chunks, including a read past the end.
  ASSERT_THAT(OpenEntry("the first key", &entry), IsOk());
  ASSERT_TRUE(nullptr != entry);
  const int kReadBufferSize = 600;
  const int kFinalReadSize = kBufferSize - kReadBufferSize;
  static_assert(kFinalReadSize < kReadBufferSize,
                "should be exactly two reads");
  auto buffer2 = base::MakeRefCounted<net::IOBufferWithSize>(kReadBufferSize);
  for (int i = 0; i < kNumStreams; i++) {
    memset(buffer2->data(), 0, kReadBufferSize);
    EXPECT_EQ(kReadBufferSize,
              ReadData(entry, i, 0, buffer2.get(), kReadBufferSize));
    EXPECT_EQ(
        0,
        memcmp(reference_buffers[i]->data(), buffer2->data(), kReadBufferSize));

    memset(buffer2->data(), 0, kReadBufferSize);
    EXPECT_EQ(
        kFinalReadSize,
        ReadData(entry, i, kReadBufferSize, buffer2.get(), kReadBufferSize));
    EXPECT_EQ(0,
              memcmp(reference_buffers[i]->data() + kReadBufferSize,
                     buffer2->data(),
                     kFinalReadSize));
  }

  entry->Close();
}

TEST_F(DiskCacheEntryTest, StreamAccess) {
  InitCache();
  StreamAccess();
}

TEST_F(DiskCacheEntryTest, MemoryOnlyStreamAccess) {
  SetMemoryOnlyMode();
  InitCache();
  StreamAccess();
}

void DiskCacheEntryTest::GetKey() {
  std::string key("the first key");
  disk_cache::Entry* entry;
  ASSERT_THAT(CreateEntry(key, &entry), IsOk());
  EXPECT_EQ(key, entry->GetKey()) << "short key";
  entry->Close();

  int seed = static_cast<int>(Time::Now().ToInternalValue());
  srand(seed);
  char key_buffer[20000];

  CacheTestFillBuffer(key_buffer, 3000, true);
  key_buffer[1000] = '\0';

  key = key_buffer;
  ASSERT_THAT(CreateEntry(key, &entry), IsOk());
  EXPECT_TRUE(key == entry->GetKey()) << "1000 bytes key";
  entry->Close();

  key_buffer[1000] = 'p';
  key_buffer[3000] = '\0';
  key = key_buffer;
  ASSERT_THAT(CreateEntry(key, &entry), IsOk());
  EXPECT_TRUE(key == entry->GetKey()) << "medium size key";
  entry->Close();

  CacheTestFillBuffer(key_buffer, sizeof(key_buffer), true);
  key_buffer[19999] = '\0';

  key = key_buffer;
  ASSERT_THAT(CreateEntry(key, &entry), IsOk());
  EXPECT_TRUE(key == entry->GetKey()) << "long key";
  entry->Close();

  CacheTestFillBuffer(key_buffer, 0x4000, true);
  key_buffer[0x4000] = '\0';

  key = key_buffer;
  ASSERT_THAT(CreateEntry(key, &entry), IsOk());
  EXPECT_TRUE(key == entry->GetKey()) << "16KB key";
  entry->Close();
}

TEST_F(DiskCacheEntryTest, GetKey) {
  InitCache();
  GetKey();
}

TEST_F(DiskCacheEntryTest, MemoryOnlyGetKey) {
  SetMemoryOnlyMode();
  InitCache();
  GetKey();
}

void DiskCacheEntryTest::GetTimes(int stream_index) {
  std::string key("the first key");
  disk_cache::Entry* entry;

  Time t1 = Time::Now();
  ASSERT_THAT(CreateEntry(key, &entry), IsOk());
  EXPECT_TRUE(entry->GetLastModified() >= t1);
  EXPECT_TRUE(entry->GetLastModified() == entry->GetLastUsed());

  AddDelay();
  Time t2 = Time::Now();
  EXPECT_TRUE(t2 > t1);
  EXPECT_EQ(0, WriteData(entry, stream_index, 200, nullptr, 0, false));
  if (type_ == net::APP_CACHE) {
    EXPECT_TRUE(entry->GetLastModified() < t2);
  } else {
    EXPECT_TRUE(entry->GetLastModified() >= t2);
  }
  EXPECT_TRUE(entry->GetLastModified() == entry->GetLastUsed());

  AddDelay();
  Time t3 = Time::Now();
  EXPECT_TRUE(t3 > t2);
  const int kSize = 200;
  auto buffer = base::MakeRefCounted<net::IOBufferWithSize>(kSize);
  EXPECT_EQ(kSize, ReadData(entry, stream_index, 0, buffer.get(), kSize));
  if (type_ == net::APP_CACHE) {
    EXPECT_TRUE(entry->GetLastUsed() < t2);
    EXPECT_TRUE(entry->GetLastModified() < t2);
  } else if (type_ == net::SHADER_CACHE) {
    EXPECT_TRUE(entry->GetLastUsed() < t3);
    EXPECT_TRUE(entry->GetLastModified() < t3);
  } else {
    EXPECT_TRUE(entry->GetLastUsed() >= t3);
    EXPECT_TRUE(entry->GetLastModified() < t3);
  }
  entry->Close();
}

TEST_F(DiskCacheEntryTest, GetTimes) {
  InitCache();
  GetTimes(0);
}

TEST_F(DiskCacheEntryTest, MemoryOnlyGetTimes) {
  SetMemoryOnlyMode();
  InitCache();
  GetTimes(0);
}

TEST_F(DiskCacheEntryTest, AppCacheGetTimes) {
  SetCacheType(net::APP_CACHE);
  InitCache();
  GetTimes(0);
}

TEST_F(DiskCacheEntryTest, ShaderCacheGetTimes) {
  SetCacheType(net::SHADER_CACHE);
  InitCache();
  GetTimes(0);
}

void DiskCacheEntryTest::GrowData(int stream_index) {
  std::string key1("the first key");
  disk_cache::Entry* entry;
  ASSERT_THAT(CreateEntry(key1, &entry), IsOk());

  const int kSize = 20000;
  auto buffer1 = base::MakeRefCounted<net::IOBufferWithSize>(kSize);
  auto buffer2 = base::MakeRefCounted<net::IOBufferWithSize>(kSize);
  CacheTestFillBuffer(buffer1->data(), kSize, false);
  memset(buffer2->data(), 0, kSize);

  base::strlcpy(buffer1->data(), "the data", kSize);
  EXPECT_EQ(10, WriteData(entry, stream_index, 0, buffer1.get(), 10, false));
  EXPECT_EQ(10, ReadData(entry, stream_index, 0, buffer2.get(), 10));
  EXPECT_STREQ("the data", buffer2->data());
  EXPECT_EQ(10, entry->GetDataSize(stream_index));

  EXPECT_EQ(2000,
            WriteData(entry, stream_index, 0, buffer1.get(), 2000, false));
  EXPECT_EQ(2000, entry->GetDataSize(stream_index));
  EXPECT_EQ(2000, ReadData(entry, stream_index, 0, buffer2.get(), 2000));
  EXPECT_TRUE(!memcmp(buffer1->data(), buffer2->data(), 2000));

  EXPECT_EQ(20000,
            WriteData(entry, stream_index, 0, buffer1.get(), kSize, false));
  EXPECT_EQ(20000, entry->GetDataSize(stream_index));
  EXPECT_EQ(20000, ReadData(entry, stream_index, 0, buffer2.get(), kSize));
  EXPECT_TRUE(!memcmp(buffer1->data(), buffer2->data(), kSize));
  entry->Close();

  memset(buffer2->data(), 0, kSize);
  std::string key2("Second key");
  ASSERT_THAT(CreateEntry(key2, &entry), IsOk());
  EXPECT_EQ(10, WriteData(entry, stream_index, 0, buffer1.get(), 10, false));
  EXPECT_EQ(10, entry->GetDataSize(stream_index));
  entry->Close();

  // Go from an internal address to a bigger block size.
  ASSERT_THAT(OpenEntry(key2, &entry), IsOk());
  EXPECT_EQ(2000,
            WriteData(entry, stream_index, 0, buffer1.get(), 2000, false));
  EXPECT_EQ(2000, entry->GetDataSize(stream_index));
  EXPECT_EQ(2000, ReadData(entry, stream_index, 0, buffer2.get(), 2000));
  EXPECT_TRUE(!memcmp(buffer1->data(), buffer2->data(), 2000));
  entry->Close();
  memset(buffer2->data(), 0, kSize);

  // Go from an internal address to an external one.
  ASSERT_THAT(OpenEntry(key2, &entry), IsOk());
  EXPECT_EQ(20000,
            WriteData(entry, stream_index, 0, buffer1.get(), kSize, false));
  EXPECT_EQ(20000, entry->GetDataSize(stream_index));
  EXPECT_EQ(20000, ReadData(entry, stream_index, 0, buffer2.get(), kSize));
  EXPECT_TRUE(!memcmp(buffer1->data(), buffer2->data(), kSize));
  entry->Close();

  // Double check the size from disk.
  ASSERT_THAT(OpenEntry(key2, &entry), IsOk());
  EXPECT_EQ(20000, entry->GetDataSize(stream_index));

  // Now extend the entry without actual data.
  EXPECT_EQ(0, WriteData(entry, stream_index, 45500, buffer1.get(), 0, false));
  entry->Close();

  // And check again from disk.
  ASSERT_THAT(OpenEntry(key2, &entry), IsOk());
  EXPECT_EQ(45500, entry->GetDataSize(stream_index));
  entry->Close();
}

TEST_F(DiskCacheEntryTest, GrowData) {
  InitCache();
  GrowData(0);
}

TEST_F(DiskCacheEntryTest, GrowDataNoBuffer) {
  InitCache();
  cache_impl_->SetFlags(disk_cache::kNoBuffering);
  GrowData(0);
}

TEST_F(DiskCacheEntryTest, MemoryOnlyGrowData) {
  SetMemoryOnlyMode();
  InitCache();
  GrowData(0);
}

void DiskCacheEntryTest::TruncateData(int stream_index) {
  std::string key("the first key");
  disk_cache::Entry* entry;
  ASSERT_THAT(CreateEntry(key, &entry), IsOk());

  const int kSize1 = 20000;
  const int kSize2 = 20000;
  auto buffer1 = base::MakeRefCounted<net::IOBufferWithSize>(kSize1);
  auto buffer2 = base::MakeRefCounted<net::IOBufferWithSize>(kSize2);

  CacheTestFillBuffer(buffer1->data(), kSize1, false);
  memset(buffer2->data(), 0, kSize2);

  // Simple truncation:
  EXPECT_EQ(200, WriteData(entry, stream_index, 0, buffer1.get(), 200, false));
  EXPECT_EQ(200, entry->GetDataSize(stream_index));
  EXPECT_EQ(100, WriteData(entry, stream_index, 0, buffer1.get(), 100, false));
  EXPECT_EQ(200, entry->GetDataSize(stream_index));
  EXPECT_EQ(100, WriteData(entry, stream_index, 0, buffer1.get(), 100, true));
  EXPECT_EQ(100, entry->GetDataSize(stream_index));
  EXPECT_EQ(0, WriteData(entry, stream_index, 50, buffer1.get(), 0, true));
  EXPECT_EQ(50, entry->GetDataSize(stream_index));
  EXPECT_EQ(0, WriteData(entry, stream_index, 0, buffer1.get(), 0, true));
  EXPECT_EQ(0, entry->GetDataSize(stream_index));
  entry->Close();
  ASSERT_THAT(OpenEntry(key, &entry), IsOk());

  // Go to an external file.
  EXPECT_EQ(20000,
            WriteData(entry, stream_index, 0, buffer1.get(), 20000, true));
  EXPECT_EQ(20000, entry->GetDataSize(stream_index));
  EXPECT_EQ(20000, ReadData(entry, stream_index, 0, buffer2.get(), 20000));
  EXPECT_TRUE(!memcmp(buffer1->data(), buffer2->data(), 20000));
  memset(buffer2->data(), 0, kSize2);

  // External file truncation
  EXPECT_EQ(18000,
            WriteData(entry, stream_index, 0, buffer1.get(), 18000, false));
  EXPECT_EQ(20000, entry->GetDataSize(stream_index));
  EXPECT_EQ(18000,
            WriteData(entry, stream_index, 0, buffer1.get(), 18000, true));
  EXPECT_EQ(18000, entry->GetDataSize(stream_index));
  EXPECT_EQ(0, WriteData(entry, stream_index, 17500, buffer1.get(), 0, true));
  EXPECT_EQ(17500, entry->GetDataSize(stream_index));

  // And back to an internal block.
  EXPECT_EQ(600,
            WriteData(entry, stream_index, 1000, buffer1.get(), 600, true));
  EXPECT_EQ(1600, entry->GetDataSize(stream_index));
  EXPECT_EQ(600, ReadData(entry, stream_index, 1000, buffer2.get(), 600));
  EXPECT_TRUE(!memcmp(buffer1->data(), buffer2->data(), 600));
  EXPECT_EQ(1000, ReadData(entry, stream_index, 0, buffer2.get(), 1000));
  EXPECT_TRUE(!memcmp(buffer1->data(), buffer2->data(), 1000))
      << "Preserves previous data";

  // Go from external file to zero length.
  EXPECT_EQ(20000,
            WriteData(entry, stream_index, 0, buffer1.get(), 20000, true));
  EXPECT_EQ(20000, entry->GetDataSize(stream_index));
  EXPECT_EQ(0, WriteData(entry, stream_index, 0, buffer1.get(), 0, true));
  EXPECT_EQ(0, entry->GetDataSize(stream_index));

  entry->Close();
}

TEST_F(DiskCacheEntryTest, TruncateData) {
  InitCache();
  TruncateData(0);
}

TEST_F(DiskCacheEntryTest, TruncateDataNoBuffer) {
  InitCache();
  cache_impl_->SetFlags(disk_cache::kNoBuffering);
  TruncateData(0);
}

TEST_F(DiskCacheEntryTest, MemoryOnlyTruncateData) {
  SetMemoryOnlyMode();
  InitCache();
  TruncateData(0);
}

void DiskCacheEntryTest::ZeroLengthIO(int stream_index) {
  std::string key("the first key");
  disk_cache::Entry* entry;
  ASSERT_THAT(CreateEntry(key, &entry), IsOk());

  EXPECT_EQ(0, ReadData(entry, stream_index, 0, nullptr, 0));
  EXPECT_EQ(0, WriteData(entry, stream_index, 0, nullptr, 0, false));

  // This write should extend the entry.
  EXPECT_EQ(0, WriteData(entry, stream_index, 1000, nullptr, 0, false));
  EXPECT_EQ(0, ReadData(entry, stream_index, 500, nullptr, 0));
  EXPECT_EQ(0, ReadData(entry, stream_index, 2000, nullptr, 0));
  EXPECT_EQ(1000, entry->GetDataSize(stream_index));

  EXPECT_EQ(0, WriteData(entry, stream_index, 100000, nullptr, 0, true));
  EXPECT_EQ(0, ReadData(entry, stream_index, 50000, nullptr, 0));
  EXPECT_EQ(100000, entry->GetDataSize(stream_index));

  // Let's verify the actual content.
  const int kSize = 20;
  const char zeros[kSize] = {};
  auto buffer = base::MakeRefCounted<net::IOBufferWithSize>(kSize);

  CacheTestFillBuffer(buffer->data(), kSize, false);
  EXPECT_EQ(kSize, ReadData(entry, stream_index, 500, buffer.get(), kSize));
  EXPECT_TRUE(!memcmp(buffer->data(), zeros, kSize));

  CacheTestFillBuffer(buffer->data(), kSize, false);
  EXPECT_EQ(kSize, ReadData(entry, stream_index, 5000, buffer.get(), kSize));
  EXPECT_TRUE(!memcmp(buffer->data(), zeros, kSize));

  CacheTestFillBuffer(buffer->data(), kSize, false);
  EXPECT_EQ(kSize, ReadData(entry, stream_index, 50000, buffer.get(), kSize));
  EXPECT_TRUE(!memcmp(buffer->data(), zeros, kSize));

  entry->Close();
}

TEST_F(DiskCacheEntryTest, ZeroLengthIO) {
  InitCache();
  ZeroLengthIO(0);
}

TEST_F(DiskCacheEntryTest, ZeroLengthIONoBuffer) {
  InitCache();
  cache_impl_->SetFlags(disk_cache::kNoBuffering);
  ZeroLengthIO(0);
}

TEST_F(DiskCacheEntryTest, MemoryOnlyZeroLengthIO) {
  SetMemoryOnlyMode();
  InitCache();
  ZeroLengthIO(0);
}

// Tests that we handle the content correctly when buffering, a feature of the
// standard cache that permits fast responses to certain reads.
void DiskCacheEntryTest::Buffering() {
  std::string key("the first key");
  disk_cache::Entry* entry;
  ASSERT_THAT(CreateEntry(key, &entry), IsOk());

  const int kSize = 200;
  auto buffer1 = base::MakeRefCounted<net::IOBufferWithSize>(kSize);
  auto buffer2 = base::MakeRefCounted<net::IOBufferWithSize>(kSize);
  CacheTestFillBuffer(buffer1->data(), kSize, true);
  CacheTestFillBuffer(buffer2->data(), kSize, true);

  EXPECT_EQ(kSize, WriteData(entry, 1, 0, buffer1.get(), kSize, false));
  entry->Close();

  // Write a little more and read what we wrote before.
  ASSERT_THAT(OpenEntry(key, &entry), IsOk());
  EXPECT_EQ(kSize, WriteData(entry, 1, 5000, buffer1.get(), kSize, false));
  EXPECT_EQ(kSize, ReadData(entry, 1, 0, buffer2.get(), kSize));
  EXPECT_TRUE(!memcmp(buffer2->data(), buffer1->data(), kSize));

  // Now go to an external file.
  EXPECT_EQ(kSize, WriteData(entry, 1, 18000, buffer1.get(), kSize, false));
  entry->Close();

  // Write something else and verify old data.
  ASSERT_THAT(OpenEntry(key, &entry), IsOk());
  EXPECT_EQ(kSize, WriteData(entry, 1, 10000, buffer1.get(), kSize, false));
  CacheTestFillBuffer(buffer2->data(), kSize, true);
  EXPECT_EQ(kSize, ReadData(entry, 1, 5000, buffer2.get(), kSize));
  EXPECT_TRUE(!memcmp(buffer2->data(), buffer1->data(), kSize));
  CacheTestFillBuffer(buffer2->data(), kSize, true);
  EXPECT_EQ(kSize, ReadData(entry, 1, 0, buffer2.get(), kSize));
  EXPECT_TRUE(!memcmp(buffer2->data(), buffer1->data(), kSize));
  CacheTestFillBuffer(buffer2->data(), kSize, true);
  EXPECT_EQ(kSize, ReadData(entry, 1, 18000, buffer2.get(), kSize));
  EXPECT_TRUE(!memcmp(buffer2->data(), buffer1->data(), kSize));

  // Extend the file some more.
  EXPECT_EQ(kSize, WriteData(entry, 1, 23000, buffer1.get(), kSize, false));
  entry->Close();

  // And now make sure that we can deal with data in both places (ram/disk).
  ASSERT_THAT(OpenEntry(key, &entry), IsOk());
  EXPECT_EQ(kSize, WriteData(entry, 1, 17000, buffer1.get(), kSize, false));

  // We should not overwrite the data at 18000 with this.
  EXPECT_EQ(kSize, WriteData(entry, 1, 19000, buffer1.get(), kSize, false));
  CacheTestFillBuffer(buffer2->data(), kSize, true);
  EXPECT_EQ(kSize, ReadData(entry, 1, 18000, buffer2.get(), kSize));
  EXPECT_TRUE(!memcmp(buffer2->data(), buffer1->data(), kSize));
  CacheTestFillBuffer(buffer2->data(), kSize, true);
  EXPECT_EQ(kSize, ReadData(entry, 1, 17000, buffer2.get(), kSize));
  EXPECT_TRUE(!memcmp(buffer2->data(), buffer1->data(), kSize));

  EXPECT_EQ(kSize, WriteData(entry, 1, 22900, buffer1.get(), kSize, false));
  CacheTestFillBuffer(buffer2->data(), kSize, true);
  EXPECT_EQ(100, ReadData(entry, 1, 23000, buffer2.get(), kSize));
  EXPECT_TRUE(!memcmp(buffer2->data(), buffer1->data() + 100, 100));

  CacheTestFillBuffer(buffer2->data(), kSize, true);
  EXPECT_EQ(100, ReadData(entry, 1, 23100, buffer2.get(), kSize));
  EXPECT_TRUE(!memcmp(buffer2->data(), buffer1->data() + 100, 100));

  // Extend the file again and read before without closing the entry.
  EXPECT_EQ(kSize, WriteData(entry, 1, 25000, buffer1.get(), kSize, false));
  EXPECT_EQ(kSize, WriteData(entry, 1, 45000, buffer1.get(), kSize, false));
  CacheTestFillBuffer(buffer2->data(), kSize, true);
  EXPECT_EQ(kSize, ReadData(entry, 1, 25000, buffer2.get(), kSize));
  EXPECT_TRUE(!memcmp(buffer2->data(), buffer1->data(), kSize));
  CacheTestFillBuffer(buffer2->data(), kSize, true);
  EXPECT_EQ(kSize, ReadData(entry, 1, 45000, buffer2.get(), kSize));
  EXPECT_TRUE(!memcmp(buffer2->data(), buffer1->data(), kSize));

  entry->Close();
}

TEST_F(DiskCacheEntryTest, Buffering) {
  InitCache();
  Buffering();
}

TEST_F(DiskCacheEntryTest, BufferingNoBuffer) {
  InitCache();
  cache_impl_->SetFlags(disk_cache::kNoBuffering);
  Buffering();
}

// Checks that entries are zero length when created.
void DiskCacheEntryTest::SizeAtCreate() {
  const char key[]  = "the first key";
  disk_cache::Entry* entry;
  ASSERT_THAT(CreateEntry(key, &entry), IsOk());

  const int kNumStreams = 3;
  for (int i = 0; i < kNumStreams; ++i)
    EXPECT_EQ(0, entry->GetDataSize(i));
  entry->Close();
}

TEST_F(DiskCacheEntryTest, SizeAtCreate) {
  InitCache();
  SizeAtCreate();
}

TEST_F(DiskCacheEntryTest, MemoryOnlySizeAtCreate) {
  SetMemoryOnlyMode();
  InitCache();
  SizeAtCreate();
}

// Some extra tests to make sure that buffering works properly when changing
// the entry size.
void DiskCacheEntryTest::SizeChanges(int stream_index) {
  std::string key("the first key");
  disk_cache::Entry* entry;
  ASSERT_THAT(CreateEntry(key, &entry), IsOk());

  const int kSize = 200;
  const char zeros[kSize] = {};
  auto buffer1 = base::MakeRefCounted<net::IOBufferWithSize>(kSize);
  auto buffer2 = base::MakeRefCounted<net::IOBufferWithSize>(kSize);
  CacheTestFillBuffer(buffer1->data(), kSize, true);
  CacheTestFillBuffer(buffer2->data(), kSize, true);

  EXPECT_EQ(kSize,
            WriteData(entry, stream_index, 0, buffer1.get(), kSize, true));
  EXPECT_EQ(kSize,
            WriteData(entry, stream_index, 17000, buffer1.get(), kSize, true));
  EXPECT_EQ(kSize,
            WriteData(entry, stream_index, 23000, buffer1.get(), kSize, true));
  entry->Close();

  // Extend the file and read between the old size and the new write.
  ASSERT_THAT(OpenEntry(key, &entry), IsOk());
  EXPECT_EQ(23000 + kSize, entry->GetDataSize(stream_index));
  EXPECT_EQ(kSize,
            WriteData(entry, stream_index, 25000, buffer1.get(), kSize, true));
  EXPECT_EQ(25000 + kSize, entry->GetDataSize(stream_index));
  EXPECT_EQ(kSize, ReadData(entry, stream_index, 24000, buffer2.get(), kSize));
  EXPECT_TRUE(!memcmp(buffer2->data(), zeros, kSize));

  // Read at the end of the old file size.
  EXPECT_EQ(
      kSize,
      ReadData(entry, stream_index, 23000 + kSize - 35, buffer2.get(), kSize));
  EXPECT_TRUE(!memcmp(buffer2->data(), buffer1->data() + kSize - 35, 35));

  // Read slightly before the last write.
  CacheTestFillBuffer(buffer2->data(), kSize, true);
  EXPECT_EQ(kSize, ReadData(entry, stream_index, 24900, buffer2.get(), kSize));
  EXPECT_TRUE(!memcmp(buffer2->data(), zeros, 100));
  EXPECT_TRUE(!memcmp(buffer2->data() + 100, buffer1->data(), kSize - 100));

  // Extend the entry a little more.
  EXPECT_EQ(kSize,
            WriteData(entry, stream_index, 26000, buffer1.get(), kSize, true));
  EXPECT_EQ(26000 + kSize, entry->GetDataSize(stream_index));
  CacheTestFillBuffer(buffer2->data(), kSize, true);
  EXPECT_EQ(kSize, ReadData(entry, stream_index, 25900, buffer2.get(), kSize));
  EXPECT_TRUE(!memcmp(buffer2->data(), zeros, 100));
  EXPECT_TRUE(!memcmp(buffer2->data() + 100, buffer1->data(), kSize - 100));

  // And now reduce the size.
  EXPECT_EQ(kSize,
            WriteData(entry, stream_index, 25000, buffer1.get(), kSize, true));
  EXPECT_EQ(25000 + kSize, entry->GetDataSize(stream_index));
  EXPECT_EQ(
      28,
      ReadData(entry, stream_index, 25000 + kSize - 28, buffer2.get(), kSize));
  EXPECT_TRUE(!memcmp(buffer2->data(), buffer1->data() + kSize - 28, 28));

  // Reduce the size with a buffer that is not extending the size.
  EXPECT_EQ(kSize,
            WriteData(entry, stream_index, 24000, buffer1.get(), kSize, false));
  EXPECT_EQ(25000 + kSize, entry->GetDataSize(stream_index));
  EXPECT_EQ(kSize,
            WriteData(entry, stream_index, 24500, buffer1.get(), kSize, true));
  EXPECT_EQ(24500 + kSize, entry->GetDataSize(stream_index));
  EXPECT_EQ(kSize, ReadData(entry, stream_index, 23900, buffer2.get(), kSize));
  EXPECT_TRUE(!memcmp(buffer2->data(), zeros, 100));
  EXPECT_TRUE(!memcmp(buffer2->data() + 100, buffer1->data(), kSize - 100));

  // And now reduce the size below the old size.
  EXPECT_EQ(kSize,
            WriteData(entry, stream_index, 19000, buffer1.get(), kSize, true));
  EXPECT_EQ(19000 + kSize, entry->GetDataSize(stream_index));
  EXPECT_EQ(kSize, ReadData(entry, stream_index, 18900, buffer2.get(), kSize));
  EXPECT_TRUE(!memcmp(buffer2->data(), zeros, 100));
  EXPECT_TRUE(!memcmp(buffer2->data() + 100, buffer1->data(), kSize - 100));

  // Verify that the actual file is truncated.
  entry->Close();
  ASSERT_THAT(OpenEntry(key, &entry), IsOk());
  EXPECT_EQ(19000 + kSize, entry->GetDataSize(stream_index));

  // Extend the newly opened file with a zero length write, expect zero fill.
  EXPECT_EQ(
      0,
      WriteData(entry, stream_index, 20000 + kSize, buffer1.get(), 0, false));
  EXPECT_EQ(kSize,
            ReadData(entry, stream_index, 19000 + kSize, buffer1.get(), kSize));
  EXPECT_EQ(0, memcmp(buffer1->data(), zeros, kSize));

  entry->Close();
}

TEST_F(DiskCacheEntryTest, SizeChanges) {
  InitCache();
  SizeChanges(1);
}

TEST_F(DiskCacheEntryTest, SizeChangesNoBuffer) {
  InitCache();
  cache_impl_->SetFlags(disk_cache::kNoBuffering);
  SizeChanges(1);
}

// Write more than the total cache capacity but to a single entry. |size| is the
// amount of bytes to write each time.
void DiskCacheEntryTest::ReuseEntry(int size, int stream_index) {
  std::string key1("the first key");
  disk_cache::Entry* entry;
  ASSERT_THAT(CreateEntry(key1, &entry), IsOk());

  entry->Close();
  std::string key2("the second key");
  ASSERT_THAT(CreateEntry(key2, &entry), IsOk());

  auto buffer = base::MakeRefCounted<net::IOBufferWithSize>(size);
  CacheTestFillBuffer(buffer->data(), size, false);

  for (int i = 0; i < 15; i++) {
    EXPECT_EQ(0, WriteData(entry, stream_index, 0, buffer.get(), 0, true));
    EXPECT_EQ(size,
              WriteData(entry, stream_index, 0, buffer.get(), size, false));
    entry->Close();
    ASSERT_THAT(OpenEntry(key2, &entry), IsOk());
  }

  entry->Close();
  ASSERT_EQ(net::OK, OpenEntry(key1, &entry)) << "have not evicted this entry";
  entry->Close();
}

TEST_F(DiskCacheEntryTest, ReuseExternalEntry) {
  SetMaxSize(200 * 1024);
  InitCache();
  ReuseEntry(20 * 1024, 0);
}

TEST_F(DiskCacheEntryTest, MemoryOnlyReuseExternalEntry) {
  SetMemoryOnlyMode();
  SetMaxSize(200 * 1024);
  InitCache();
  ReuseEntry(20 * 1024, 0);
}

TEST_F(DiskCacheEntryTest, ReuseInternalEntry) {
  SetMaxSize(100 * 1024);
  InitCache();
  ReuseEntry(10 * 1024, 0);
}

TEST_F(DiskCacheEntryTest, MemoryOnlyReuseInternalEntry) {
  SetMemoryOnlyMode();
  SetMaxSize(100 * 1024);
  InitCache();
  ReuseEntry(10 * 1024, 0);
}

// Reading somewhere that was not written should return zeros.
void DiskCacheEntryTest::InvalidData(int stream_index) {
  std::string key("the first key");
  disk_cache::Entry* entry;
  ASSERT_THAT(CreateEntry(key, &entry), IsOk());

  const int kSize1 = 20000;
  const int kSize2 = 20000;
  const int kSize3 = 20000;
  auto buffer1 = base::MakeRefCounted<net::IOBufferWithSize>(kSize1);
  auto buffer2 = base::MakeRefCounted<net::IOBufferWithSize>(kSize2);
  auto buffer3 = base::MakeRefCounted<net::IOBufferWithSize>(kSize3);

  CacheTestFillBuffer(buffer1->data(), kSize1, false);
  memset(buffer2->data(), 0, kSize2);

  // Simple data grow:
  EXPECT_EQ(200,
            WriteData(entry, stream_index, 400, buffer1.get(), 200, false));
  EXPECT_EQ(600, entry->GetDataSize(stream_index));
  EXPECT_EQ(100, ReadData(entry, stream_index, 300, buffer3.get(), 100));
  EXPECT_TRUE(!memcmp(buffer3->data(), buffer2->data(), 100));
  entry->Close();
  ASSERT_THAT(OpenEntry(key, &entry), IsOk());

  // The entry is now on disk. Load it and extend it.
  EXPECT_EQ(200,
            WriteData(entry, stream_index, 800, buffer1.get(), 200, false));
  EXPECT_EQ(1000, entry->GetDataSize(stream_index));
  EXPECT_EQ(100, ReadData(entry, stream_index, 700, buffer3.get(), 100));
  EXPECT_TRUE(!memcmp(buffer3->data(), buffer2->data(), 100));
  entry->Close();
  ASSERT_THAT(OpenEntry(key, &entry), IsOk());

  // This time using truncate.
  EXPECT_EQ(200,
            WriteData(entry, stream_index, 1800, buffer1.get(), 200, true));
  EXPECT_EQ(2000, entry->GetDataSize(stream_index));
  EXPECT_EQ(100, ReadData(entry, stream_index, 1500, buffer3.get(), 100));
  EXPECT_TRUE(!memcmp(buffer3->data(), buffer2->data(), 100));

  // Go to an external file.
  EXPECT_EQ(200,
            WriteData(entry, stream_index, 19800, buffer1.get(), 200, false));
  EXPECT_EQ(20000, entry->GetDataSize(stream_index));
  EXPECT_EQ(4000, ReadData(entry, stream_index, 14000, buffer3.get(), 4000));
  EXPECT_TRUE(!memcmp(buffer3->data(), buffer2->data(), 4000));

  // And back to an internal block.
  EXPECT_EQ(600,
            WriteData(entry, stream_index, 1000, buffer1.get(), 600, true));
  EXPECT_EQ(1600, entry->GetDataSize(stream_index));
  EXPECT_EQ(600, ReadData(entry, stream_index, 1000, buffer3.get(), 600));
  EXPECT_TRUE(!memcmp(buffer3->data(), buffer1->data(), 600));

  // Extend it again.
  EXPECT_EQ(600,
            WriteData(entry, stream_index, 2000, buffer1.get(), 600, false));
  EXPECT_EQ(2600, entry->GetDataSize(stream_index));
  EXPECT_EQ(200, ReadData(entry, stream_index, 1800, buffer3.get(), 200));
  EXPECT_TRUE(!memcmp(buffer3->data(), buffer2->data(), 200));

  // And again (with truncation flag).
  EXPECT_EQ(600,
            WriteData(entry, stream_index, 3000, buffer1.get(), 600, true));
  EXPECT_EQ(3600, entry->GetDataSize(stream_index));
  EXPECT_EQ(200, ReadData(entry, stream_index, 2800, buffer3.get(), 200));
  EXPECT_TRUE(!memcmp(buffer3->data(), buffer2->data(), 200));

  entry->Close();
}

TEST_F(DiskCacheEntryTest, InvalidData) {
  InitCache();
  InvalidData(0);
}

TEST_F(DiskCacheEntryTest, InvalidDataNoBuffer) {
  InitCache();
  cache_impl_->SetFlags(disk_cache::kNoBuffering);
  InvalidData(0);
}

TEST_F(DiskCacheEntryTest, MemoryOnlyInvalidData) {
  SetMemoryOnlyMode();
  InitCache();
  InvalidData(0);
}

// Tests that the cache preserves the buffer of an IO operation.
void DiskCacheEntryTest::ReadWriteDestroyBuffer(int stream_index) {
  std::string key("the first key");
  disk_cache::Entry* entry;
  ASSERT_THAT(CreateEntry(key, &entry), IsOk());

  const int kSize = 200;
  auto buffer = base::MakeRefCounted<net::IOBufferWithSize>(kSize);
  CacheTestFillBuffer(buffer->data(), kSize, false);

  net::TestCompletionCallback cb;
  EXPECT_EQ(net::ERR_IO_PENDING,
            entry->WriteData(
                stream_index, 0, buffer.get(), kSize, cb.callback(), false));

  // Release our reference to the buffer.
  buffer = nullptr;
  EXPECT_EQ(kSize, cb.WaitForResult());

  // And now test with a Read().
  buffer = base::MakeRefCounted<net::IOBufferWithSize>(kSize);
  CacheTestFillBuffer(buffer->data(), kSize, false);

  EXPECT_EQ(
      net::ERR_IO_PENDING,
      entry->ReadData(stream_index, 0, buffer.get(), kSize, cb.callback()));
  buffer = nullptr;
  EXPECT_EQ(kSize, cb.WaitForResult());

  entry->Close();
}

TEST_F(DiskCacheEntryTest, ReadWriteDestroyBuffer) {
  InitCache();
  ReadWriteDestroyBuffer(0);
}

void DiskCacheEntryTest::DoomNormalEntry() {
  std::string key("the first key");
  disk_cache::Entry* entry;
  ASSERT_THAT(CreateEntry(key, &entry), IsOk());
  entry->Doom();
  entry->Close();

  const int kSize = 20000;
  auto buffer = base::MakeRefCounted<net::IOBufferWithSize>(kSize);
  CacheTestFillBuffer(buffer->data(), kSize, true);
  buffer->data()[19999] = '\0';

  key = buffer->data();
  ASSERT_THAT(CreateEntry(key, &entry), IsOk());
  EXPECT_EQ(20000, WriteData(entry, 0, 0, buffer.get(), kSize, false));
  EXPECT_EQ(20000, WriteData(entry, 1, 0, buffer.get(), kSize, false));
  entry->Doom();
  entry->Close();

  FlushQueueForTest();
  EXPECT_EQ(0, cache_->GetEntryCount());
}

TEST_F(DiskCacheEntryTest, DoomEntry) {
  InitCache();
  DoomNormalEntry();
}

TEST_F(DiskCacheEntryTest, MemoryOnlyDoomEntry) {
  SetMemoryOnlyMode();
  InitCache();
  DoomNormalEntry();
}

// Tests dooming an entry that's linked to an open entry.
void DiskCacheEntryTest::DoomEntryNextToOpenEntry() {
  disk_cache::Entry* entry1;
  disk_cache::Entry* entry2;
  ASSERT_THAT(CreateEntry("fixed", &entry1), IsOk());
  entry1->Close();
  ASSERT_THAT(CreateEntry("foo", &entry1), IsOk());
  entry1->Close();
  ASSERT_THAT(CreateEntry("bar", &entry1), IsOk());
  entry1->Close();

  ASSERT_THAT(OpenEntry("foo", &entry1), IsOk());
  ASSERT_THAT(OpenEntry("bar", &entry2), IsOk());
  entry2->Doom();
  entry2->Close();

  ASSERT_THAT(OpenEntry("foo", &entry2), IsOk());
  entry2->Doom();
  entry2->Close();
  entry1->Close();

  ASSERT_THAT(OpenEntry("fixed", &entry1), IsOk());
  entry1->Close();
}

TEST_F(DiskCacheEntryTest, DoomEntryNextToOpenEntry) {
  InitCache();
  DoomEntryNextToOpenEntry();
}

TEST_F(DiskCacheEntryTest, NewEvictionDoomEntryNextToOpenEntry) {
  SetNewEviction();
  InitCache();
  DoomEntryNextToOpenEntry();
}

TEST_F(DiskCacheEntryTest, AppCacheDoomEntryNextToOpenEntry) {
  SetCacheType(net::APP_CACHE);
  InitCache();
  DoomEntryNextToOpenEntry();
}

// Verify that basic operations work as expected with doomed entries.
void DiskCacheEntryTest::DoomedEntry(int stream_index) {
  std::string key("the first key");
  disk_cache::Entry* entry;
  ASSERT_THAT(CreateEntry(key, &entry), IsOk());
  entry->Doom();

  FlushQueueForTest();
  EXPECT_EQ(0, cache_->GetEntryCount());
  Time initial = Time::Now();
  AddDelay();

  const int kSize1 = 2000;
  const int kSize2 = 2000;
  auto buffer1 = base::MakeRefCounted<net::IOBufferWithSize>(kSize1);
  auto buffer2 = base::MakeRefCounted<net::IOBufferWithSize>(kSize2);
  CacheTestFillBuffer(buffer1->data(), kSize1, false);
  memset(buffer2->data(), 0, kSize2);

  EXPECT_EQ(2000,
            WriteData(entry, stream_index, 0, buffer1.get(), 2000, false));
  EXPECT_EQ(2000, ReadData(entry, stream_index, 0, buffer2.get(), 2000));
  EXPECT_EQ(0, memcmp(buffer1->data(), buffer2->data(), kSize1));
  EXPECT_EQ(key, entry->GetKey());
  EXPECT_TRUE(initial < entry->GetLastModified());
  EXPECT_TRUE(initial < entry->GetLastUsed());

  entry->Close();
}

TEST_F(DiskCacheEntryTest, DoomedEntry) {
  InitCache();
  DoomedEntry(0);
}

TEST_F(DiskCacheEntryTest, MemoryOnlyDoomedEntry) {
  SetMemoryOnlyMode();
  InitCache();
  DoomedEntry(0);
}

// Tests that we discard entries if the data is missing.
TEST_F(DiskCacheEntryTest, MissingData) {
  InitCache();

  std::string key("the first key");
  disk_cache::Entry* entry;
  ASSERT_THAT(CreateEntry(key, &entry), IsOk());

  // Write to an external file.
  const int kSize = 20000;
  auto buffer = base::MakeRefCounted<net::IOBufferWithSize>(kSize);
  CacheTestFillBuffer(buffer->data(), kSize, false);
  EXPECT_EQ(kSize, WriteData(entry, 0, 0, buffer.get(), kSize, false));
  entry->Close();
  FlushQueueForTest();

  disk_cache::Addr address(0x80000001);
  base::FilePath name = cache_impl_->GetFileName(address);
  EXPECT_TRUE(base::DeleteFile(name));

  // Attempt to read the data.
  ASSERT_THAT(OpenEntry(key, &entry), IsOk());
  EXPECT_EQ(net::ERR_FILE_NOT_FOUND,
            ReadData(entry, 0, 0, buffer.get(), kSize));
  entry->Close();

  // The entry should be gone.
  ASSERT_NE(net::OK, OpenEntry(key, &entry));
}

// Test that child entries in a memory cache backend are not visible from
// enumerations.
TEST_F(DiskCacheEntryTest, MemoryOnlyEnumerationWithSparseEntries) {
  SetMemoryOnlyMode();
  InitCache();

  const int kSize = 4096;
  auto buf = base::MakeRefCounted<net::IOBufferWithSize>(kSize);
  CacheTestFillBuffer(buf->data(), kSize, false);

  std::string key("the first key");
  disk_cache::Entry* parent_entry;
  ASSERT_THAT(CreateEntry(key, &parent_entry), IsOk());

  // Writes to the parent entry.
  EXPECT_EQ(kSize, parent_entry->WriteSparseData(
                       0, buf.get(), kSize, net::CompletionOnceCallback()));

  // This write creates a child entry and writes to it.
  EXPECT_EQ(kSize, parent_entry->WriteSparseData(
                       8192, buf.get(), kSize, net::CompletionOnceCallback()));

  parent_entry->Close();

  // Perform the enumerations.
  std::unique_ptr<TestIterator> iter = CreateIterator();
  disk_cache::Entry* entry = nullptr;
  int count = 0;
  while (iter->OpenNextEntry(&entry) == net::OK) {
    ASSERT_TRUE(entry != nullptr);
    ++count;
    disk_cache::MemEntryImpl* mem_entry =
        reinterpret_cast<disk_cache::MemEntryImpl*>(entry);
    EXPECT_EQ(disk_cache::MemEntryImpl::EntryType::kParent, mem_entry->type());
    mem_entry->Close();
  }
  EXPECT_EQ(1, count);
}

// Writes |buf_1| to offset and reads it back as |buf_2|.
void VerifySparseIO(disk_cache::Entry* entry,
                    int64_t offset,
                    net::IOBuffer* buf_1,
                    int size,
                    net::IOBuffer* buf_2) {
  net::TestCompletionCallback cb;

  memset(buf_2->data(), 0, size);
  int ret = entry->ReadSparseData(offset, buf_2, size, cb.callback());
  EXPECT_EQ(0, cb.GetResult(ret));

  ret = entry->WriteSparseData(offset, buf_1, size, cb.callback());
  EXPECT_EQ(size, cb.GetResult(ret));

  ret = entry->ReadSparseData(offset, buf_2, size, cb.callback());
  EXPECT_EQ(size, cb.GetResult(ret));

  EXPECT_EQ(0, memcmp(buf_1->data(), buf_2->data(), size));
}

// Reads |size| bytes from |entry| at |offset| and verifies that they are the
// same as the content of the provided |buffer|.
void VerifyContentSparseIO(disk_cache::Entry* entry,
                           int64_t offset,
                           char* buffer,
                           int size) {
  net::TestCompletionCallback cb;

  auto buf_1 = base::MakeRefCounted<net::IOBufferWithSize>(size);
  memset(buf_1->data(), 0, size);
  int ret = entry->ReadSparseData(offset, buf_1.get(), size, cb.callback());
  EXPECT_EQ(size, cb.GetResult(ret));
  EXPECT_EQ(0, memcmp(buf_1->data(), buffer, size));
}

void DiskCacheEntryTest::BasicSparseIO() {
  std::string key("the first key");
  disk_cache::Entry* entry;
  ASSERT_THAT(CreateEntry(key, &entry), IsOk());

  const int kSize = 2048;
  auto buf_1 = base::MakeRefCounted<net::IOBufferWithSize>(kSize);
  auto buf_2 = base::MakeRefCounted<net::IOBufferWithSize>(kSize);
  CacheTestFillBuffer(buf_1->data(), kSize, false);

  // Write at offset 0.
  VerifySparseIO(entry, 0, buf_1.get(), kSize, buf_2.get());

  // Write at offset 0x400000 (4 MB).
  VerifySparseIO(entry, 0x400000, buf_1.get(), kSize, buf_2.get());

  // Write at offset 0x800000000 (32 GB).
  VerifySparseIO(entry, 0x800000000LL, buf_1.get(), kSize, buf_2.get());

  entry->Close();

  // Check everything again.
  ASSERT_THAT(OpenEntry(key, &entry), IsOk());
  VerifyContentSparseIO(entry, 0, buf_1->data(), kSize);
  VerifyContentSparseIO(entry, 0x400000, buf_1->data(), kSize);
  VerifyContentSparseIO(entry, 0x800000000LL, buf_1->data(), kSize);
  entry->Close();
}

TEST_F(DiskCacheEntryTest, BasicSparseIO) {
  InitCache();
  BasicSparseIO();
}

TEST_F(DiskCacheEntryTest, MemoryOnlyBasicSparseIO) {
  SetMemoryOnlyMode();
  InitCache();
  BasicSparseIO();
}

void DiskCacheEntryTest::HugeSparseIO() {
  std::string key("the first key");
  disk_cache::Entry* entry;
  ASSERT_THAT(CreateEntry(key, &entry), IsOk());

  // Write 1.2 MB so that we cover multiple entries.
  const int kSize = 1200 * 1024;
  auto buf_1 = base::MakeRefCounted<net::IOBufferWithSize>(kSize);
  auto buf_2 = base::MakeRefCounted<net::IOBufferWithSize>(kSize);
  CacheTestFillBuffer(buf_1->data(), kSize, false);

  // Write at offset 0x20F0000 (33 MB - 64 KB).
  VerifySparseIO(entry, 0x20F0000, buf_1.get(), kSize, buf_2.get());
  entry->Close();

  // Check it again.
  ASSERT_THAT(OpenEntry(key, &entry), IsOk());
  VerifyContentSparseIO(entry, 0x20F0000, buf_1->data(), kSize);
  entry->Close();
}

TEST_F(DiskCacheEntryTest, HugeSparseIO) {
  InitCache();
  HugeSparseIO();
}

TEST_F(DiskCacheEntryTest, MemoryOnlyHugeSparseIO) {
  SetMemoryOnlyMode();
  InitCache();
  HugeSparseIO();
}

void DiskCacheEntryTest::GetAvailableRangeTest() {
  std::string key("the first key");
  disk_cache::Entry* entry;
  ASSERT_THAT(CreateEntry(key, &entry), IsOk());

  const int kSize = 16 * 1024;
  auto buf = base::MakeRefCounted<net::IOBufferWithSize>(kSize);
  CacheTestFillBuffer(buf->data(), kSize, false);

  // Write at offset 0x20F0000 (33 MB - 64 KB), and 0x20F4400 (33 MB - 47 KB).
  EXPECT_EQ(kSize, WriteSparseData(entry, 0x20F0000, buf.get(), kSize));
  EXPECT_EQ(kSize, WriteSparseData(entry, 0x20F4400, buf.get(), kSize));

  // We stop at the first empty block.
  TestRangeResultCompletionCallback cb;
  RangeResult result = cb.GetResult(
      entry->GetAvailableRange(0x20F0000, kSize * 2, cb.callback()));
  EXPECT_EQ(net::OK, result.net_error);
  EXPECT_EQ(kSize, result.available_len);
  EXPECT_EQ(0x20F0000, result.start);

  result = cb.GetResult(entry->GetAvailableRange(0, kSize, cb.callback()));
  EXPECT_EQ(net::OK, result.net_error);
  EXPECT_EQ(0, result.available_len);

  result = cb.GetResult(
      entry->GetAvailableRange(0x20F0000 - kSize, kSize, cb.callback()));
  EXPECT_EQ(net::OK, result.net_error);
  EXPECT_EQ(0, result.available_len);

  result = cb.GetResult(entry->GetAvailableRange(0, 0x2100000, cb.callback()));
  EXPECT_EQ(net::OK, result.net_error);
  EXPECT_EQ(kSize, result.available_len);
  EXPECT_EQ(0x20F0000, result.start);

  // We should be able to Read based on the results of GetAvailableRange.
  net::TestCompletionCallback read_cb;
  result =
      cb.GetResult(entry->GetAvailableRange(0x2100000, kSize, cb.callback()));
  EXPECT_EQ(net::OK, result.net_error);
  EXPECT_EQ(0, result.available_len);
  int rv =
      entry->ReadSparseData(result.start, buf.get(), kSize, read_cb.callback());
  EXPECT_EQ(0, read_cb.GetResult(rv));

  result =
      cb.GetResult(entry->GetAvailableRange(0x20F2000, kSize, cb.callback()));
  EXPECT_EQ(net::OK, result.net_error);
  EXPECT_EQ(0x2000, result.available_len);
  EXPECT_EQ(0x20F2000, result.start);
  EXPECT_EQ(0x2000, ReadSparseData(entry, result.start, buf.get(), kSize));

  // Make sure that we respect the |len| argument.
  result = cb.GetResult(
      entry->GetAvailableRange(0x20F0001 - kSize, kSize, cb.callback()));
  EXPECT_EQ(net::OK, result.net_error);
  EXPECT_EQ(1, result.available_len);
  EXPECT_EQ(0x20F0000, result.start);

  // Use very small ranges. Write at offset 50.
  const int kTinyLen = 10;
  EXPECT_EQ(kTinyLen, WriteSparseData(entry, 50, buf.get(), kTinyLen));

  result = cb.GetResult(
      entry->GetAvailableRange(kTinyLen * 2, kTinyLen, cb.callback()));
  EXPECT_EQ(net::OK, result.net_error);
  EXPECT_EQ(0, result.available_len);
  EXPECT_EQ(kTinyLen * 2, result.start);

  // Get a huge range with maximum boundary
  result = cb.GetResult(entry->GetAvailableRange(
      0x2100000, std::numeric_limits<int32_t>::max(), cb.callback()));
  EXPECT_EQ(net::OK, result.net_error);
  EXPECT_EQ(0, result.available_len);

  entry->Close();
}

TEST_F(DiskCacheEntryTest, GetAvailableRange) {
  InitCache();
  GetAvailableRangeTest();
}

TEST_F(DiskCacheEntryTest, MemoryOnlyGetAvailableRange) {
  SetMemoryOnlyMode();
  InitCache();
  GetAvailableRangeTest();
}

TEST_F(DiskCacheEntryTest, GetAvailableRangeBlockFileDiscontinuous) {
  // crbug.com/791056 --- blockfile problem when there is a sub-KiB write before
  // a bunch of full 1KiB blocks, and a GetAvailableRange is issued to which
  // both are a potentially relevant.
  InitCache();

  std::string key("the first key");
  disk_cache::Entry* entry;
  ASSERT_THAT(CreateEntry(key, &entry), IsOk());

  auto buf_2k = base::MakeRefCounted<net::IOBufferWithSize>(2 * 1024);
  CacheTestFillBuffer(buf_2k->data(), 2 * 1024, false);

  const int kSmallSize = 612;  // sub-1k
  auto buf_small = base::MakeRefCounted<net::IOBufferWithSize>(kSmallSize);
  CacheTestFillBuffer(buf_small->data(), kSmallSize, false);

  // Sets some bits for blocks representing 1K ranges [1024, 3072),
  // which will be relevant for the next GetAvailableRange call.
  EXPECT_EQ(2 * 1024, WriteSparseData(entry, /* offset = */ 1024, buf_2k.get(),
                                      /* size = */ 2 * 1024));

  // Now record a partial write from start of the first kb.
  EXPECT_EQ(kSmallSize, WriteSparseData(entry, /* offset = */ 0,
                                        buf_small.get(), kSmallSize));

  // Try to query a range starting from that block 0.
  // The cache tracks: [0, 612) [1024, 3072).
  // The request is for: [812, 2059) so response should be [1024, 2059), which
  // has length = 1035. Previously this return a negative number for rv.
  TestRangeResultCompletionCallback cb;
  RangeResult result =
      cb.GetResult(entry->GetAvailableRange(812, 1247, cb.callback()));
  EXPECT_EQ(net::OK, result.net_error);
  EXPECT_EQ(1035, result.available_len);
  EXPECT_EQ(1024, result.start);

  // Now query [512, 1536). This matches both [512, 612) and [1024, 1536),
  // so this should return [512, 612).
  result = cb.GetResult(entry->GetAvailableRange(512, 1024, cb.callback()));
  EXPECT_EQ(net::OK, result.net_error);
  EXPECT_EQ(100, result.available_len);
  EXPECT_EQ(512, result.start);

  // Now query next portion, [612, 1636). This now just should produce
  // [1024, 1636)
  result = cb.GetResult(entry->GetAvailableRange(612, 1024, cb.callback()));
  EXPECT_EQ(net::OK, result.net_error);
  EXPECT_EQ(612, result.available_len);
  EXPECT_EQ(1024, result.start);

  // Do a continuous small write, this one at [3072, 3684).
  // This means the cache tracks [1024, 3072) via bitmaps and [3072, 3684)
  // as the last write.
  EXPECT_EQ(kSmallSize, WriteSparseData(entry, /* offset = */ 3072,
                                        buf_small.get(), kSmallSize));

  // Query [2048, 4096). Should get [2048, 3684)
  result = cb.GetResult(entry->GetAvailableRange(2048, 2048, cb.callback()));
  EXPECT_EQ(net::OK, result.net_error);
  EXPECT_EQ(1636, result.available_len);
  EXPECT_EQ(2048, result.start);

  // Now write at [4096, 4708). Since only one sub-kb thing is tracked, this
  // now tracks  [1024, 3072) via bitmaps and [4096, 4708) as the last write.
  EXPECT_EQ(kSmallSize, WriteSparseData(entry, /* offset = */ 4096,
                                        buf_small.get(), kSmallSize));

  // Query [2048, 4096). Should get [2048, 3072)
  result = cb.GetResult(entry->GetAvailableRange(2048, 2048, cb.callback()));
  EXPECT_EQ(net::OK, result.net_error);
  EXPECT_EQ(1024, result.available_len);
  EXPECT_EQ(2048, result.start);

  // Query 2K more after that: [3072, 5120). Should get [4096, 4708)
  result = cb.GetResult(entry->GetAvailableRange(3072, 2048, cb.callback()));
  EXPECT_EQ(net::OK, result.net_error);
  EXPECT_EQ(612, result.available_len);
  EXPECT_EQ(4096, result.start);

  // Also double-check that offsets within later children are correctly
  // computed.
  EXPECT_EQ(kSmallSize, WriteSparseData(entry, /* offset = */ 0x200400,
                                        buf_small.get(), kSmallSize));
  result =
      cb.GetResult(entry->GetAvailableRange(0x100000, 0x200000, cb.callback()));
  EXPECT_EQ(net::OK, result.net_error);
  EXPECT_EQ(kSmallSize, result.available_len);
  EXPECT_EQ(0x200400, result.start);

  entry->Close();
}

// Tests that non-sequential writes that are not aligned with the minimum sparse
// data granularity (1024 bytes) do in fact result in dropped data.
TEST_F(DiskCacheEntryTest, SparseWriteDropped) {
  InitCache();
  std::string key("the first key");
  disk_cache::Entry* entry;
  ASSERT_THAT(CreateEntry(key, &entry), IsOk());

  const int kSize = 180;
  auto buf_1 = base::MakeRefCounted<net::IOBufferWithSize>(kSize);
  auto buf_2 = base::MakeRefCounted<net::IOBufferWithSize>(kSize);
  CacheTestFillBuffer(buf_1->data(), kSize, false);

  // Do small writes (180 bytes) that get increasingly close to a 1024-byte
  // boundary. All data should be dropped until a boundary is crossed, at which
  // point the data after the boundary is saved (at least for a while).
  int offset = 1024 - 500;
  int rv = 0;
  net::TestCompletionCallback cb;
  TestRangeResultCompletionCallback range_cb;
  RangeResult result;
  for (int i = 0; i < 5; i++) {
    // Check result of last GetAvailableRange.
    EXPECT_EQ(0, result.available_len);

    rv = entry->WriteSparseData(offset, buf_1.get(), kSize, cb.callback());
    EXPECT_EQ(kSize, cb.GetResult(rv));

    result = range_cb.GetResult(
        entry->GetAvailableRange(offset - 100, kSize, range_cb.callback()));
    EXPECT_EQ(net::OK, result.net_error);
    EXPECT_EQ(0, result.available_len);

    result = range_cb.GetResult(
        entry->GetAvailableRange(offset, kSize, range_cb.callback()));
    if (!result.available_len) {
      rv = entry->ReadSparseData(offset, buf_2.get(), kSize, cb.callback());
      EXPECT_EQ(0, cb.GetResult(rv));
    }
    offset += 1024 * i + 100;
  }

  // The last write started 100 bytes below a bundary, so there should be 80
  // bytes after the boundary.
  EXPECT_EQ(80, result.available_len);
  EXPECT_EQ(1024 * 7, result.start);
  rv = entry->ReadSparseData(result.start, buf_2.get(), kSize, cb.callback());
  EXPECT_EQ(80, cb.GetResult(rv));
  EXPECT_EQ(0, memcmp(buf_1.get()->data() + 100, buf_2.get()->data(), 80));

  // And even that part is dropped when another write changes the offset.
  offset = result.start;
  rv = entry->WriteSparseData(0, buf_1.get(), kSize, cb.callback());
  EXPECT_EQ(kSize, cb.GetResult(rv));

  result = range_cb.GetResult(
      entry->GetAvailableRange(offset, kSize, range_cb.callback()));
  EXPECT_EQ(net::OK, result.net_error);
  EXPECT_EQ(0, result.available_len);
  entry->Close();
}

// Tests that small sequential writes are not dropped.
TEST_F(DiskCacheEntryTest, SparseSquentialWriteNotDropped) {
  InitCache();
  std::string key("the first key");
  disk_cache::Entry* entry;
  ASSERT_THAT(CreateEntry(key, &entry), IsOk());

  const int kSize = 180;
  auto buf_1 = base::MakeRefCounted<net::IOBufferWithSize>(kSize);
  auto buf_2 = base::MakeRefCounted<net::IOBufferWithSize>(kSize);
  CacheTestFillBuffer(buf_1->data(), kSize, false);

  // Any starting offset is fine as long as it is 1024-bytes aligned.
  int rv = 0;
  RangeResult result;
  net::TestCompletionCallback cb;
  TestRangeResultCompletionCallback range_cb;
  int64_t offset = 1024 * 11;
  for (; offset < 20000; offset += kSize) {
    rv = entry->WriteSparseData(offset, buf_1.get(), kSize, cb.callback());
    EXPECT_EQ(kSize, cb.GetResult(rv));

    result = range_cb.GetResult(
        entry->GetAvailableRange(offset, kSize, range_cb.callback()));
    EXPECT_EQ(net::OK, result.net_error);
    EXPECT_EQ(kSize, result.available_len);
    EXPECT_EQ(offset, result.start);

    rv = entry->ReadSparseData(offset, buf_2.get(), kSize, cb.callback());
    EXPECT_EQ(kSize, cb.GetResult(rv));
    EXPECT_EQ(0, memcmp(buf_1.get()->data(), buf_2.get()->data(), kSize));
  }

  entry->Close();
  FlushQueueForTest();

  // Verify again the last write made.
  ASSERT_THAT(OpenEntry(key, &entry), IsOk());
  offset -= kSize;
  result = range_cb.GetResult(
      entry->GetAvailableRange(offset, kSize, range_cb.callback()));
  EXPECT_EQ(net::OK, result.net_error);
  EXPECT_EQ(kSize, result.available_len);
  EXPECT_EQ(offset, result.start);

  rv = entry->ReadSparseData(offset, buf_2.get(), kSize, cb.callback());
  EXPECT_EQ(kSize, cb.GetResult(rv));
  EXPECT_EQ(0, memcmp(buf_1.get()->data(), buf_2.get()->data(), kSize));

  entry->Close();
}

void DiskCacheEntryTest::CouldBeSparse() {
  std::string key("the first key");
  disk_cache::Entry* entry;
  ASSERT_THAT(CreateEntry(key, &entry), IsOk());

  const int kSize = 16 * 1024;
  auto buf = base::MakeRefCounted<net::IOBufferWithSize>(kSize);
  CacheTestFillBuffer(buf->data(), kSize, false);

  // Write at offset 0x20F0000 (33 MB - 64 KB).
  EXPECT_EQ(kSize, WriteSparseData(entry, 0x20F0000, buf.get(), kSize));

  EXPECT_TRUE(entry->CouldBeSparse());
  entry->Close();

  ASSERT_THAT(OpenEntry(key, &entry), IsOk());
  EXPECT_TRUE(entry->CouldBeSparse());
  entry->Close();

  // Now verify a regular entry.
  key.assign("another key");
  ASSERT_THAT(CreateEntry(key, &entry), IsOk());
  EXPECT_FALSE(entry->CouldBeSparse());

  EXPECT_EQ(kSize, WriteData(entry, 0, 0, buf.get(), kSize, false));
  EXPECT_EQ(kSize, WriteData(entry, 1, 0, buf.get(), kSize, false));
  EXPECT_EQ(kSize, WriteData(entry, 2, 0, buf.get(), kSize, false));

  EXPECT_FALSE(entry->CouldBeSparse());
  entry->Close();

  ASSERT_THAT(OpenEntry(key, &entry), IsOk());
  EXPECT_FALSE(entry->CouldBeSparse());
  entry->Close();
}

TEST_F(DiskCacheEntryTest, CouldBeSparse) {
  InitCache();
  CouldBeSparse();
}

TEST_F(DiskCacheEntryTest, MemoryCouldBeSparse) {
  SetMemoryOnlyMode();
  InitCache();
  CouldBeSparse();
}

TEST_F(DiskCacheEntryTest, MemoryOnlyMisalignedSparseIO) {
  SetMemoryOnlyMode();
  InitCache();

  const int kSize = 8192;
  auto buf_1 = base::MakeRefCounted<net::IOBufferWithSize>(kSize);
  auto buf_2 = base::MakeRefCounted<net::IOBufferWithSize>(kSize);
  CacheTestFillBuffer(buf_1->data(), kSize, false);

  std::string key("the first key");
  disk_cache::Entry* entry;
  ASSERT_THAT(CreateEntry(key, &entry), IsOk());

  // This loop writes back to back starting from offset 0 and 9000.
  for (int i = 0; i < kSize; i += 1024) {
    auto buf_3 =
        base::MakeRefCounted<net::WrappedIOBuffer>(buf_1->span().subspan(i));
    VerifySparseIO(entry, i, buf_3.get(), 1024, buf_2.get());
    VerifySparseIO(entry, 9000 + i, buf_3.get(), 1024, buf_2.get());
  }

  // Make sure we have data written.
  VerifyContentSparseIO(entry, 0, buf_1->data(), kSize);
  VerifyContentSparseIO(entry, 9000, buf_1->data(), kSize);

  // This tests a large write that spans 3 entries from a misaligned offset.
  VerifySparseIO(entry, 20481, buf_1.get(), 8192, buf_2.get());

  entry->Close();
}

TEST_F(DiskCacheEntryTest, MemoryOnlyMisalignedGetAvailableRange) {
  SetMemoryOnlyMode();
  InitCache();

  const int kSize = 8192;
  auto buf = base::MakeRefCounted<net::IOBufferWithSize>(kSize);
  CacheTestFillBuffer(buf->data(), kSize, false);

  disk_cache::Entry* entry;
  std::string key("the first key");
  ASSERT_THAT(CreateEntry(key, &entry), IsOk());

  // Writes in the middle of an entry.
  EXPECT_EQ(1024, entry->WriteSparseData(0, buf.get(), 1024,
                                         net::CompletionOnceCallback()));
  EXPECT_EQ(1024, entry->WriteSparseData(5120, buf.get(), 1024,
                                         net::CompletionOnceCallback()));
  EXPECT_EQ(1024, entry->WriteSparseData(10000, buf.get(), 1024,
                                         net::CompletionOnceCallback()));

  // Writes in the middle of an entry and spans 2 child entries.
  EXPECT_EQ(8192, entry->WriteSparseData(50000, buf.get(), 8192,
                                         net::CompletionOnceCallback()));

  TestRangeResultCompletionCallback cb;
  // Test that we stop at a discontinuous child at the second block.
  RangeResult result =
      cb.GetResult(entry->GetAvailableRange(0, 10000, cb.callback()));
  EXPECT_EQ(net::OK, result.net_error);
  EXPECT_EQ(1024, result.available_len);
  EXPECT_EQ(0, result.start);

  // Test that number of bytes is reported correctly when we start from the
  // middle of a filled region.
  result = cb.GetResult(entry->GetAvailableRange(512, 10000, cb.callback()));
  EXPECT_EQ(net::OK, result.net_error);
  EXPECT_EQ(512, result.available_len);
  EXPECT_EQ(512, result.start);

  // Test that we found bytes in the child of next block.
  result = cb.GetResult(entry->GetAvailableRange(1024, 10000, cb.callback()));
  EXPECT_EQ(net::OK, result.net_error);
  EXPECT_EQ(1024, result.available_len);
  EXPECT_EQ(5120, result.start);

  // Test that the desired length is respected. It starts within a filled
  // region.
  result = cb.GetResult(entry->GetAvailableRange(5500, 512, cb.callback()));
  EXPECT_EQ(net::OK, result.net_error);
  EXPECT_EQ(512, result.available_len);
  EXPECT_EQ(5500, result.start);

  // Test that the desired length is respected. It starts before a filled
  // region.
  result = cb.GetResult(entry->GetAvailableRange(5000, 620, cb.callback()));
  EXPECT_EQ(net::OK, result.net_error);
  EXPECT_EQ(500, result.available_len);
  EXPECT_EQ(5120, result.start);

  // Test that multiple blocks are scanned.
  result = cb.GetResult(entry->GetAvailableRange(40000, 20000, cb.callback()));
  EXPECT_EQ(net::OK, result.net_error);
  EXPECT_EQ(8192, result.available_len);
  EXPECT_EQ(50000, result.start);

  entry->Close();
}

void DiskCacheEntryTest::UpdateSparseEntry() {
  std::string key("the first key");
  disk_cache::Entry* entry1;
  ASSERT_THAT(CreateEntry(key, &entry1), IsOk());

  const int kSize = 2048;
  auto buf_1 = base::MakeRefCounted<net::IOBufferWithSize>(kSize);
  auto buf_2 = base::MakeRefCounted<net::IOBufferWithSize>(kSize);
  CacheTestFillBuffer(buf_1->data(), kSize, false);

  // Write at offset 0.
  VerifySparseIO(entry1, 0, buf_1.get(), kSize, buf_2.get());
  entry1->Close();

  // Write at offset 2048.
  ASSERT_THAT(OpenEntry(key, &entry1), IsOk());
  VerifySparseIO(entry1, 2048, buf_1.get(), kSize, buf_2.get());

  disk_cache::Entry* entry2;
  ASSERT_THAT(CreateEntry("the second key", &entry2), IsOk());

  entry1->Close();
  entry2->Close();
  FlushQueueForTest();
  if (memory_only_ || simple_cache_mode_)
    EXPECT_EQ(2, cache_->GetEntryCount());
  else
    EXPECT_EQ(3, cache_->GetEntryCount());
}

TEST_F(DiskCacheEntryTest, UpdateSparseEntry) {
  InitCache();
  UpdateSparseEntry();
}

TEST_F(DiskCacheEntryTest, MemoryOnlyUpdateSparseEntry) {
  SetMemoryOnlyMode();
  InitCache();
  UpdateSparseEntry();
}

void DiskCacheEntryTest::DoomSparseEntry() {
  std::string key1("the first key");
  std::string key2("the second key");
  disk_cache::Entry *entry1, *entry2;
  ASSERT_THAT(CreateEntry(key1, &entry1), IsOk());
  ASSERT_THAT(CreateEntry(key2, &entry2), IsOk());

  const int kSize = 4 * 1024;
  auto buf = base::MakeRefCounted<net::IOBufferWithSize>(kSize);
  CacheTestFillBuffer(buf->data(), kSize, false);

  int64_t offset = 1024;
  // Write to a bunch of ranges.
  for (int i = 0; i < 12; i++) {
    EXPECT_EQ(kSize, WriteSparseData(entry1, offset, buf.get(), kSize));
    // Keep the second map under the default size.
    if (i < 9)
      EXPECT_EQ(kSize, WriteSparseData(entry2, offset, buf.get(), kSize));

    offset *= 4;
  }

  if (memory_only_ || simple_cache_mode_)
    EXPECT_EQ(2, cache_->GetEntryCount());
  else
    EXPECT_EQ(15, cache_->GetEntryCount());

  // Doom the first entry while it's still open.
  entry1->Doom();
  entry1->Close();
  entry2->Close();

  // Doom the second entry after it's fully saved.
  EXPECT_THAT(DoomEntry(key2), IsOk());

  // Make sure we do all needed work. This may fail for entry2 if between Close
  // and DoomEntry the system decides to remove all traces of the file from the
  // system cache so we don't see that there is pending IO.
  base::RunLoop().RunUntilIdle();

  if (memory_only_) {
    EXPECT_EQ(0, cache_->GetEntryCount());
  } else {
    if (5 == cache_->GetEntryCount()) {
      // Most likely we are waiting for the result of reading the sparse info
      // (it's always async on Posix so it is easy to miss). Unfortunately we
      // don't have any signal to watch for so we can only wait.
      base::PlatformThread::Sleep(base::Milliseconds(500));
      base::RunLoop().RunUntilIdle();
    }
    EXPECT_EQ(0, cache_->GetEntryCount());
  }
}

TEST_F(DiskCacheEntryTest, DoomSparseEntry) {
  UseCurrentThread();
  InitCache();
  DoomSparseEntry();
}

TEST_F(DiskCacheEntryTest, MemoryOnlyDoomSparseEntry) {
  SetMemoryOnlyMode();
  InitCache();
  DoomSparseEntry();
}

// A TestCompletionCallback wrapper that deletes the cache from within the
// callback.  The way TestCompletionCallback works means that all tasks (even
// new ones) are executed by the message loop before returning to the caller so
// the only way to simulate a race is to execute what we want on the callback.
class SparseTestCompletionCallback: public net::TestCompletionCallback {
 public:
  explicit SparseTestCompletionCallback(
      std::unique_ptr<disk_cache::Backend> cache)
      : cache_(std::move(cache)) {}

  SparseTestCompletionCallback(const SparseTestCompletionCallback&) = delete;
  SparseTestCompletionCallback& operator=(const SparseTestCompletionCallback&) =
      delete;

 private:
  void SetResult(int result) override {
    cache_.reset();
    TestCompletionCallback::SetResult(result);
  }

  std::unique_ptr<disk_cache::Backend> cache_;
};

// Tests that we don't crash when the backend is deleted while we are working
// deleting the sub-entries of a sparse entry.
TEST_F(DiskCacheEntryTest, DoomSparseEntry2) {
  UseCurrentThread();
  InitCache();
  std::string key("the key");
  disk_cache::Entry* entry;
  ASSERT_THAT(CreateEntry(key, &entry), IsOk());

  const int kSize = 4 * 1024;
  auto buf = base::MakeRefCounted<net::IOBufferWithSize>(kSize);
  CacheTestFillBuffer(buf->data(), kSize, false);

  int64_t offset = 1024;
  // Write to a bunch of ranges.
  for (int i = 0; i < 12; i++) {
    EXPECT_EQ(kSize, entry->WriteSparseData(offset, buf.get(), kSize,
                                            net::CompletionOnceCallback()));
    offset *= 4;
  }
  EXPECT_EQ(9, cache_->GetEntryCount());

  entry->Close();
  disk_cache::Backend* cache = cache_.get();
  SparseTestCompletionCallback cb(TakeCache());
  int rv = cache->DoomEntry(key, net::HIGHEST, cb.callback());
  EXPECT_THAT(rv, IsError(net::ERR_IO_PENDING));
  EXPECT_THAT(cb.WaitForResult(), IsOk());
}

void DiskCacheEntryTest::PartialSparseEntry() {
  std::string key("the first key");
  disk_cache::Entry* entry;
  ASSERT_THAT(CreateEntry(key, &entry), IsOk());

  // We should be able to deal with IO that is not aligned to the block size
  // of a sparse entry, at least to write a big range without leaving holes.
  const int kSize = 4 * 1024;
  const int kSmallSize = 128;
  auto buf1 = base::MakeRefCounted<net::IOBufferWithSize>(kSize);
  CacheTestFillBuffer(buf1->data(), kSize, false);

  // The first write is just to extend the entry. The third write occupies
  // a 1KB block partially, it may not be written internally depending on the
  // implementation.
  EXPECT_EQ(kSize, WriteSparseData(entry, 20000, buf1.get(), kSize));
  EXPECT_EQ(kSize, WriteSparseData(entry, 500, buf1.get(), kSize));
  EXPECT_EQ(kSmallSize,
            WriteSparseData(entry, 1080321, buf1.get(), kSmallSize));
  entry->Close();
  ASSERT_THAT(OpenEntry(key, &entry), IsOk());

  auto buf2 = base::MakeRefCounted<net::IOBufferWithSize>(kSize);
  memset(buf2->data(), 0, kSize);
  EXPECT_EQ(0, ReadSparseData(entry, 8000, buf2.get(), kSize));

  EXPECT_EQ(500, ReadSparseData(entry, kSize, buf2.get(), kSize));
  EXPECT_EQ(0, memcmp(buf2->data(), buf1->data() + kSize - 500, 500));
  EXPECT_EQ(0, ReadSparseData(entry, 0, buf2.get(), kSize));

  // This read should not change anything.
  if (memory_only_ || simple_cache_mode_)
    EXPECT_EQ(96, ReadSparseData(entry, 24000, buf2.get(), kSize));
  else
    EXPECT_EQ(0, ReadSparseData(entry, 24000, buf2.get(), kSize));

  EXPECT_EQ(500, ReadSparseData(entry, kSize, buf2.get(), kSize));
  EXPECT_EQ(0, ReadSparseData(entry, 99, buf2.get(), kSize));

  TestRangeResultCompletionCallback cb;
  RangeResult result;
  if (memory_only_ || simple_cache_mode_) {
    result = cb.GetResult(entry->GetAvailableRange(0, 600, cb.callback()));
    EXPECT_EQ(net::OK, result.net_error);
    EXPECT_EQ(100, result.available_len);
    EXPECT_EQ(500, result.start);
  } else {
    result = cb.GetResult(entry->GetAvailableRange(0, 2048, cb.callback()));
    EXPECT_EQ(net::OK, result.net_error);
    EXPECT_EQ(1024, result.available_len);
    EXPECT_EQ(1024, result.start);
  }
  result = cb.GetResult(entry->GetAvailableRange(kSize, kSize, cb.callback()));
  EXPECT_EQ(net::OK, result.net_error);
  EXPECT_EQ(500, result.available_len);
  EXPECT_EQ(kSize, result.start);
  result =
      cb.GetResult(entry->GetAvailableRange(20 * 1024, 10000, cb.callback()));
  EXPECT_EQ(net::OK, result.net_error);
  if (memory_only_ || simple_cache_mode_)
    EXPECT_EQ(3616, result.available_len);
  else
    EXPECT_EQ(3072, result.available_len);

  EXPECT_EQ(20 * 1024, result.start);

  // 1. Query before a filled 1KB block.
  // 2. Query within a filled 1KB block.
  // 3. Query beyond a filled 1KB block.
  if (memory_only_ || simple_cache_mode_) {
    result =
        cb.GetResult(entry->GetAvailableRange(19400, kSize, cb.callback()));
    EXPECT_EQ(net::OK, result.net_error);
    EXPECT_EQ(3496, result.available_len);
    EXPECT_EQ(20000, result.start);
  } else {
    result =
        cb.GetResult(entry->GetAvailableRange(19400, kSize, cb.callback()));
    EXPECT_EQ(net::OK, result.net_error);
    EXPECT_EQ(3016, result.available_len);
    EXPECT_EQ(20480, result.start);
  }
  result = cb.GetResult(entry->GetAvailableRange(3073, kSize, cb.callback()));
  EXPECT_EQ(net::OK, result.net_error);
  EXPECT_EQ(1523, result.available_len);
  EXPECT_EQ(3073, result.start);
  result = cb.GetResult(entry->GetAvailableRange(4600, kSize, cb.callback()));
  EXPECT_EQ(net::OK, result.net_error);
  EXPECT_EQ(0, result.available_len);
  EXPECT_EQ(4600, result.start);

  // Now make another write and verify that there is no hole in between.
  EXPECT_EQ(kSize, WriteSparseData(entry, 500 + kSize, buf1.get(), kSize));
  result = cb.GetResult(entry->GetAvailableRange(1024, 10000, cb.callback()));
  EXPECT_EQ(net::OK, result.net_error);
  EXPECT_EQ(7 * 1024 + 500, result.available_len);
  EXPECT_EQ(1024, result.start);
  EXPECT_EQ(kSize, ReadSparseData(entry, kSize, buf2.get(), kSize));
  EXPECT_EQ(0, memcmp(buf2->data(), buf1->data() + kSize - 500, 500));
  EXPECT_EQ(0, memcmp(buf2->data() + 500, buf1->data(), kSize - 500));

  entry->Close();
}

TEST_F(DiskCacheEntryTest, PartialSparseEntry) {
  InitCache();
  PartialSparseEntry();
}

TEST_F(DiskCacheEntryTest, MemoryPartialSparseEntry) {
  SetMemoryOnlyMode();
  InitCache();
  PartialSparseEntry();
}

void DiskCacheEntryTest::SparseInvalidArg() {
  std::string key("key");
  disk_cache::Entry* entry = nullptr;
  ASSERT_THAT(CreateEntry(key, &entry), IsOk());

  const int kSize = 2048;
  auto buf = base::MakeRefCounted<net::IOBufferWithSize>(kSize);
  CacheTestFillBuffer(buf->data(), kSize, false);

  EXPECT_EQ(net::ERR_INVALID_ARGUMENT,
            WriteSparseData(entry, -1, buf.get(), kSize));
  EXPECT_EQ(net::ERR_INVALID_ARGUMENT,
            WriteSparseData(entry, 0, buf.get(), -1));

  EXPECT_EQ(net::ERR_INVALID_ARGUMENT,
            ReadSparseData(entry, -1, buf.get(), kSize));
  EXPECT_EQ(net::ERR_INVALID_ARGUMENT, ReadSparseData(entry, 0, buf.get(), -1));

  int64_t start_out;
  EXPECT_EQ(net::ERR_INVALID_ARGUMENT,
            GetAvailableRange(entry, -1, kSize, &start_out));
  EXPECT_EQ(net::ERR_INVALID_ARGUMENT,
            GetAvailableRange(entry, 0, -1, &start_out));

  int rv = WriteSparseData(
      entry, std::numeric_limits<int64_t>::max() - kSize + 1, buf.get(), kSize);
  // Blockfile rejects anything over 64GiB with
  // net::ERR_CACHE_OPERATION_NOT_SUPPORTED, which is also OK here, as it's not
  // an overflow or something else nonsensical.
  EXPECT_TRUE(rv == net::ERR_INVALID_ARGUMENT ||
              rv == net::ERR_CACHE_OPERATION_NOT_SUPPORTED);

  entry->Close();
}

TEST_F(DiskCacheEntryTest, SparseInvalidArg) {
  InitCache();
  SparseInvalidArg();
}

TEST_F(DiskCacheEntryTest, MemoryOnlySparseInvalidArg) {
  SetMemoryOnlyMode();
  InitCache();
  SparseInvalidArg();
}

TEST_F(DiskCacheEntryTest, SimpleSparseInvalidArg) {
  SetSimpleCacheMode();
  InitCache();
  SparseInvalidArg();
}

void DiskCacheEntryTest::SparseClipEnd(int64_t max_index,
                                       bool expect_unsupported) {
  std::string key("key");
  disk_cache::Entry* entry = nullptr;
  ASSERT_THAT(CreateEntry(key, &entry), IsOk());

  const int kSize = 1024;
  auto buf = base::MakeRefCounted<net::IOBufferWithSize>(kSize);
  CacheTestFillBuffer(buf->data(), kSize, false);

  auto read_buf = base::MakeRefCounted<net::IOBufferWithSize>(kSize * 2);
  CacheTestFillBuffer(read_buf->data(), kSize * 2, false);

  const int64_t kOffset = max_index - kSize;
  int rv = WriteSparseData(entry, kOffset, buf.get(), kSize);
  EXPECT_EQ(
      rv, expect_unsupported ? net::ERR_CACHE_OPERATION_NOT_SUPPORTED : kSize);

  // Try to read further than offset range, should get clipped (if supported).
  rv = ReadSparseData(entry, kOffset, read_buf.get(), kSize * 2);
  if (expect_unsupported) {
    EXPECT_EQ(rv, net::ERR_CACHE_OPERATION_NOT_SUPPORTED);
  } else {
    EXPECT_EQ(kSize, rv);
    EXPECT_EQ(0, memcmp(buf->data(), read_buf->data(), kSize));
  }

  TestRangeResultCompletionCallback cb;
  RangeResult result = cb.GetResult(
      entry->GetAvailableRange(kOffset - kSize, kSize * 3, cb.callback()));
  if (expect_unsupported) {
    // GetAvailableRange just returns nothing found, not an error.
    EXPECT_EQ(net::OK, result.net_error);
    EXPECT_EQ(result.available_len, 0);
  } else {
    EXPECT_EQ(net::OK, result.net_error);
    EXPECT_EQ(kSize, result.available_len);
    EXPECT_EQ(kOffset, result.start);
  }

  entry->Close();
}

TEST_F(DiskCacheEntryTest, SparseClipEnd) {
  InitCache();

  // Blockfile refuses to deal with sparse indices over 64GiB.
  SparseClipEnd(std::numeric_limits<int64_t>::max(),
                /*expected_unsupported=*/true);
}

TEST_F(DiskCacheEntryTest, SparseClipEnd2) {
  InitCache();

  const int64_t kLimit = 64ll * 1024 * 1024 * 1024;
  // Separate test for blockfile for indices right at the edge of its address
  // space limit. kLimit must match kMaxEndOffset in sparse_control.cc
  SparseClipEnd(kLimit, /*expected_unsupported=*/false);

  // Test with things after kLimit, too, which isn't an issue for backends
  // supporting the entire 64-bit offset range.
  std::string key("key2");
  disk_cache::Entry* entry = nullptr;
  ASSERT_THAT(CreateEntry(key, &entry), IsOk());

  const int kSize = 1024;
  auto buf = base::MakeRefCounted<net::IOBufferWithSize>(kSize);
  CacheTestFillBuffer(buf->data(), kSize, false);

  // Try to write after --- fails.
  int rv = WriteSparseData(entry, kLimit, buf.get(), kSize);
  EXPECT_EQ(net::ERR_CACHE_OPERATION_NOT_SUPPORTED, rv);

  // Similarly for read.
  rv = ReadSparseData(entry, kLimit, buf.get(), kSize);
  EXPECT_EQ(net::ERR_CACHE_OPERATION_NOT_SUPPORTED, rv);

  // GetAvailableRange just returns nothing.
  TestRangeResultCompletionCallback cb;
  RangeResult result =
      cb.GetResult(entry->GetAvailableRange(kLimit, kSize * 3, cb.callback()));
  EXPECT_EQ(net::OK, result.net_error);
  EXPECT_EQ(0, result.available_len);
  entry->Close();
}

TEST_F(DiskCacheEntryTest, MemoryOnlySparseClipEnd) {
  SetMemoryOnlyMode();
  InitCache();
  SparseClipEnd(std::numeric_limits<int64_t>::max(),
                /* expected_unsupported = */ false);
}

TEST_F(DiskCacheEntryTest, SimpleSparseClipEnd) {
  SetSimpleCacheMode();
  InitCache();
  SparseClipEnd(std::numeric_limits<int64_t>::max(),
                /* expected_unsupported = */ false);
}

// Tests that corrupt sparse children are removed automatically.
TEST_F(DiskCacheEntryTest, CleanupSparseEntry) {
  InitCache();
  std::string key("the first key");
  disk_cache::Entry* entry;
  ASSERT_THAT(CreateEntry(key, &entry), IsOk());

  const int kSize = 4 * 1024;
  auto buf1 = base::MakeRefCounted<net::IOBufferWithSize>(kSize);
  CacheTestFillBuffer(buf1->data(), kSize, false);

  const int k1Meg = 1024 * 1024;
  EXPECT_EQ(kSize, WriteSparseData(entry, 8192, buf1.get(), kSize));
  EXPECT_EQ(kSize, WriteSparseData(entry, k1Meg + 8192, buf1.get(), kSize));
  EXPECT_EQ(kSize, WriteSparseData(entry, 2 * k1Meg + 8192, buf1.get(), kSize));
  entry->Close();
  EXPECT_EQ(4, cache_->GetEntryCount());

  std::unique_ptr<TestIterator> iter = CreateIterator();
  int count = 0;
  std::string child_keys[2];
  while (iter->OpenNextEntry(&entry) == net::OK) {
    ASSERT_TRUE(entry != nullptr);
    // Writing to an entry will alter the LRU list and invalidate the iterator.
    if (entry->GetKey() != key && count < 2)
      child_keys[count++] = entry->GetKey();
    entry->Close();
  }
  for (const auto& child_key : child_keys) {
    ASSERT_THAT(OpenEntry(child_key, &entry), IsOk());
    // Overwrite the header's magic and signature.
    EXPECT_EQ(12, WriteData(entry, 2, 0, buf1.get(), 12, false));
    entry->Close();
  }

  EXPECT_EQ(4, cache_->GetEntryCount());
  ASSERT_THAT(OpenEntry(key, &entry), IsOk());

  // Two children should be gone. One while reading and one while writing.
  EXPECT_EQ(0, ReadSparseData(entry, 2 * k1Meg + 8192, buf1.get(), kSize));
  EXPECT_EQ(kSize, WriteSparseData(entry, k1Meg + 16384, buf1.get(), kSize));
  EXPECT_EQ(0, ReadSparseData(entry, k1Meg + 8192, buf1.get(), kSize));

  // We never touched this one.
  EXPECT_EQ(kSize, ReadSparseData(entry, 8192, buf1.get(), kSize));
  entry->Close();

  // We re-created one of the corrupt children.
  EXPECT_EQ(3, cache_->GetEntryCount());
}

TEST_F(DiskCacheEntryTest, CancelSparseIO) {
  UseCurrentThread();
  InitCache();
  std::string key("the first key");
  disk_cache::Entry* entry;
  ASSERT_THAT(CreateEntry(key, &entry), IsOk());

  const int kSize = 40 * 1024;
  auto buf = base::MakeRefCounted<net::IOBufferWithSize>(kSize);
  CacheTestFillBuffer(buf->data(), kSize, false);

  // This will open and write two "real" entries.
  net::TestCompletionCallback cb1, cb2, cb3, cb4;
  int rv = entry->WriteSparseData(
      1024 * 1024 - 4096, buf.get(), kSize, cb1.callback());
  EXPECT_THAT(rv, IsError(net::ERR_IO_PENDING));

  TestRangeResultCompletionCallback cb5;
  RangeResult result =
      cb5.GetResult(entry->GetAvailableRange(0, kSize, cb5.callback()));
  if (!cb1.have_result()) {
    // We may or may not have finished writing to the entry. If we have not,
    // we cannot start another operation at this time.
    EXPECT_THAT(rv, IsError(net::ERR_CACHE_OPERATION_NOT_SUPPORTED));
  }

  // We cancel the pending operation, and register multiple notifications.
  entry->CancelSparseIO();
  EXPECT_THAT(entry->ReadyForSparseIO(cb2.callback()),
              IsError(net::ERR_IO_PENDING));
  EXPECT_THAT(entry->ReadyForSparseIO(cb3.callback()),
              IsError(net::ERR_IO_PENDING));
  entry->CancelSparseIO();  // Should be a no op at this point.
  EXPECT_THAT(entry->ReadyForSparseIO(cb4.callback()),
              IsError(net::ERR_IO_PENDING));

  if (!cb1.have_result()) {
    EXPECT_EQ(net::ERR_CACHE_OPERATION_NOT_SUPPORTED,
              entry->ReadSparseData(result.start, buf.get(), kSize,
                                    net::CompletionOnceCallback()));
    EXPECT_EQ(net::ERR_CACHE_OPERATION_NOT_SUPPORTED,
              entry->WriteSparseData(result.start, buf.get(), kSize,
                                     net::CompletionOnceCallback()));
  }

  // Now see if we receive all notifications. Note that we should not be able
  // to write everything (unless the timing of the system is really weird).
  rv = cb1.WaitForResult();
  EXPECT_TRUE(rv == 4096 || rv == kSize);
  EXPECT_THAT(cb2.WaitForResult(), IsOk());
  EXPECT_THAT(cb3.WaitForResult(), IsOk());
  EXPECT_THAT(cb4.WaitForResult(), IsOk());

  result = cb5.GetResult(
      entry->GetAvailableRange(result.start, kSize, cb5.callback()));
  EXPECT_EQ(net::OK, result.net_error);
  EXPECT_EQ(0, result.available_len);
  entry->Close();
}

// Tests that we perform sanity checks on an entry's key. Note that there are
// other tests that exercise sanity checks by using saved corrupt files.
TEST_F(DiskCacheEntryTest, KeySanityCheck) {
  UseCurrentThread();
  InitCache();
  std::string key("the first key");
  disk_cache::Entry* entry;
  ASSERT_THAT(CreateEntry(key, &entry), IsOk());

  disk_cache::EntryImpl* entry_impl =
      static_cast<disk_cache::EntryImpl*>(entry);
  disk_cache::EntryStore* store = entry_impl->entry()->Data();

  // We have reserved space for a short key (one block), let's say that the key
  // takes more than one block, and remove the NULLs after the actual key.
  store->key_len = 800;
  memset(store->key + key.size(), 'k', sizeof(store->key) - key.size());
  entry_impl->entry()->set_modified();
  entry->Close();

  // We have a corrupt entry. Now reload it. We should NOT read beyond the
  // allocated buffer here.
  ASSERT_NE(net::OK, OpenEntry(key, &entry));
  DisableIntegrityCheck();
}

TEST_F(DiskCacheEntryTest, KeySanityCheck2) {
  UseCurrentThread();
  InitCache();
  std::string key("the first key");
  disk_cache::Entry* entry;
  ASSERT_THAT(CreateEntry(key, &entry), IsOk());

  disk_cache::EntryImpl* entry_impl =
      static_cast<disk_cache::EntryImpl*>(entry);
  disk_cache::EntryStore* store = entry_impl->entry()->Data();

  // Fill in the rest of inline key store with non-nulls. Unlike in
  // KeySanityCheck, this does not change the length to identify it as
  // stored under |long_key|.
  memset(store->key + key.size(), 'k', sizeof(store->key) - key.size());
  entry_impl->entry()->set_modified();
  entry->Close();

  // We have a corrupt entry. Now reload it. We should NOT read beyond the
  // allocated buffer here.
  ASSERT_NE(net::OK, OpenEntry(key, &entry));
  DisableIntegrityCheck();
}

TEST_F(DiskCacheEntryTest, KeySanityCheck3) {
  const size_t kVeryLong = 40 * 1024;
  UseCurrentThread();
  InitCache();
  std::string key(kVeryLong, 'a');
  disk_cache::Entry* entry;
  ASSERT_THAT(CreateEntry(key, &entry), IsOk());

  disk_cache::EntryImpl* entry_impl =
      static_cast<disk_cache::EntryImpl*>(entry);
  disk_cache::EntryStore* store = entry_impl->entry()->Data();

  // Test meaningful when using long keys; and also want this to be
  // an external file to avoid needing to duplicate offset math here.
  disk_cache::Addr key_addr(store->long_key);
  ASSERT_TRUE(key_addr.is_initialized());
  ASSERT_TRUE(key_addr.is_separate_file());

  // Close the entry before messing up its files.
  entry->Close();

  // Mess up the terminating null in the external key file.
  auto key_file =
      base::MakeRefCounted<disk_cache::File>(true /* want sync ops*/);
  ASSERT_TRUE(key_file->Init(cache_impl_->GetFileName(key_addr)));

  ASSERT_TRUE(key_file->Write("b", 1u, kVeryLong));
  key_file = nullptr;

  // This case gets graceful recovery.
  ASSERT_THAT(OpenEntry(key, &entry), IsOk());

  // Make sure the key object isn't messed up.
  EXPECT_EQ(kVeryLong, strlen(entry->GetKey().data()));
  entry->Close();
}

TEST_F(DiskCacheEntryTest, SimpleCacheInternalAsyncIO) {
  SetSimpleCacheMode();
  InitCache();
  InternalAsyncIO();
}

TEST_F(DiskCacheEntryTest, SimpleCacheExternalAsyncIO) {
  SetSimpleCacheMode();
  InitCache();
  ExternalAsyncIO();
}

TEST_F(DiskCacheEntryTest, SimpleCacheReleaseBuffer) {
  SetSimpleCacheMode();
  InitCache();
  for (int i = 0; i < disk_cache::kSimpleEntryStreamCount; ++i) {
    EXPECT_THAT(DoomAllEntries(), IsOk());
    ReleaseBuffer(i);
  }
}

TEST_F(DiskCacheEntryTest, SimpleCacheStreamAccess) {
  SetSimpleCacheMode();
  InitCache();
  StreamAccess();
}

TEST_F(DiskCacheEntryTest, SimpleCacheGetKey) {
  SetSimpleCacheMode();
  InitCache();
  GetKey();
}

TEST_F(DiskCacheEntryTest, SimpleCacheGetTimes) {
  SetSimpleCacheMode();
  InitCache();
  for (int i = 0; i < disk_cache::kSimpleEntryStreamCount; ++i) {
    EXPECT_THAT(DoomAllEntries(), IsOk());
    GetTimes(i);
  }
}

TEST_F(DiskCacheEntryTest, SimpleCacheGrowData) {
  SetSimpleCacheMode();
  InitCache();
  for (int i = 0; i < disk_cache::kSimpleEntryStreamCount; ++i) {
    EXPECT_THAT(DoomAllEntries(), IsOk());
    GrowData(i);
  }
}

TEST_F(DiskCacheEntryTest, SimpleCacheTruncateData) {
  SetSimpleCacheMode();
  InitCache();
  for (int i = 0; i < disk_cache::kSimpleEntryStreamCount; ++i) {
    EXPECT_THAT(DoomAllEntries(), IsOk());
    TruncateData(i);
  }
}

TEST_F(DiskCacheEntryTest, SimpleCacheZeroLengthIO) {
  SetSimpleCacheMode();
  InitCache();
  for (int i = 0; i < disk_cache::kSimpleEntryStreamCount; ++i) {
    EXPECT_THAT(DoomAllEntries(), IsOk());
    ZeroLengthIO(i);
  }
}

TEST_F(DiskCacheEntryTest, SimpleCacheSizeAtCreate) {
  SetSimpleCacheMode();
  InitCache();
  SizeAtCreate();
}

TEST_F(DiskCacheEntryTest, SimpleCacheReuseExternalEntry) {
  SetSimpleCacheMode();
  SetMaxSize(200 * 1024);
  InitCache();
  for (int i = 0; i < disk_cache::kSimpleEntryStreamCount; ++i) {
    EXPECT_THAT(DoomAllEntries(), IsOk());
    ReuseEntry(20 * 1024, i);
  }
}

TEST_F(DiskCacheEntryTest, SimpleCacheReuseInternalEntry) {
  SetSimpleCacheMode();
  SetMaxSize(100 * 1024);
  InitCache();
  for (int i = 0; i < disk_cache::kSimpleEntryStreamCount; ++i) {
    EXPECT_THAT(DoomAllEntries(), IsOk());
    ReuseEntry(10 * 1024, i);
  }
}

TEST_F(DiskCacheEntryTest, SimpleCacheGiantEntry) {
  const int kBufSize = 32 * 1024;
  auto buffer = base::MakeRefCounted<net::IOBufferWithSize>(kBufSize);
  CacheTestFillBuffer(buffer->data(), kBufSize, false);

  // Make sure SimpleCache can write up to 5MiB entry even with a 20MiB cache
  // size that Android WebView uses at the time of this test's writing.
  SetSimpleCacheMode();
  SetMaxSize(20 * 1024 * 1024);
  InitCache();

  {
    std::string key1("the first key");
    disk_cache::Entry* entry1 = nullptr;
    ASSERT_THAT(CreateEntry(key1, &entry1), IsOk());

    const int kSize1 = 5 * 1024 * 1024;
    EXPECT_EQ(kBufSize, WriteData(entry1, 1 /* stream */, kSize1 - kBufSize,
                                  buffer.get(), kBufSize, true /* truncate */));
    entry1->Close();
  }

  // ... but not bigger than that.
  {
    std::string key2("the second key");
    disk_cache::Entry* entry2 = nullptr;
    ASSERT_THAT(CreateEntry(key2, &entry2), IsOk());

    const int kSize2 = 5 * 1024 * 1024 + 1;
    EXPECT_EQ(net::ERR_FAILED,
              WriteData(entry2, 1 /* stream */, kSize2 - kBufSize, buffer.get(),
                        kBufSize, true /* truncate */));
    entry2->Close();
  }
}

TEST_F(DiskCacheEntryTest, SimpleCacheSizeChanges) {
  SetSimpleCacheMode();
  InitCache();
  for (int i = 0; i < disk_cache::kSimpleEntryStreamCount; ++i) {
    EXPECT_THAT(DoomAllEntries(), IsOk());
    SizeChanges(i);
  }
}

TEST_F(DiskCacheEntryTest, SimpleCacheInvalidData) {
  SetSimpleCacheMode();
  InitCache();
  for (int i = 0; i < disk_cache::kSimpleEntryStreamCount; ++i) {
    EXPECT_THAT(DoomAllEntries(), IsOk());
    InvalidData(i);
  }
}

TEST_F(DiskCacheEntryTest, SimpleCacheReadWriteDestroyBuffer) {
  // Proving that the test works well with optimistic operations enabled is
  // subtle, instead run only in APP_CACHE mode to disable optimistic
  // operations. Stream 0 always uses optimistic operations, so the test is not
  // run on stream 0.
  SetCacheType(net::APP_CACHE);
  SetSimpleCacheMode();
  InitCache();
  for (int i = 1; i < disk_cache::kSimpleEntryStreamCount; ++i) {
    EXPECT_THAT(DoomAllEntries(), IsOk());
    ReadWriteDestroyBuffer(i);
  }
}

TEST_F(DiskCacheEntryTest, SimpleCacheDoomEntry) {
  SetSimpleCacheMode();
  InitCache();
  DoomNormalEntry();
}

TEST_F(DiskCacheEntryTest, SimpleCacheDoomEntryNextToOpenEntry) {
  SetSimpleCacheMode();
  InitCache();
  DoomEntryNextToOpenEntry();
}

TEST_F(DiskCacheEntryTest, SimpleCacheDoomedEntry) {
  SetSimpleCacheMode();
  InitCache();
  // Stream 2 is excluded because the implementation does not support writing to
  // it on a doomed entry, if it was previously lazily omitted.
  for (int i = 0; i < disk_cache::kSimpleEntryStreamCount - 1; ++i) {
    EXPECT_THAT(DoomAllEntries(), IsOk());
    DoomedEntry(i);
  }
}

// Creates an entry with corrupted last byte in stream 0.
// Requires SimpleCacheMode.
bool DiskCacheEntryTest::SimpleCacheMakeBadChecksumEntry(const std::string& key,
                                                         int data_size) {
  disk_cache::Entry* entry = nullptr;

  if (CreateEntry(key, &entry) != net::OK || !entry) {
    LOG(ERROR) << "Could not create entry";
    return false;
  }

  auto buffer = base::MakeRefCounted<net::IOBufferWithSize>(data_size);
  memset(buffer->data(), 'A', data_size);

  EXPECT_EQ(data_size, WriteData(entry, 1, 0, buffer.get(), data_size, false));
  entry->Close();
  entry = nullptr;

  // Corrupt the last byte of the data.
  base::FilePath entry_file0_path = cache_path_.AppendASCII(
      disk_cache::simple_util::GetFilenameFromKeyAndFileIndex(key, 0));
  base::File entry_file0(entry_file0_path,
                         base::File::FLAG_WRITE | base::File::FLAG_OPEN);
  if (!entry_file0.IsValid())
    return false;

  int64_t file_offset =
      sizeof(disk_cache::SimpleFileHeader) + key.size() + data_size - 2;
  EXPECT_EQ(1, entry_file0.Write(file_offset, "X", 1));
  return true;
}

TEST_F(DiskCacheEntryTest, SimpleCacheBadChecksum) {
  SetSimpleCacheMode();
  InitCache();

  const char key[] = "the first key";
  const int kLargeSize = 50000;
  ASSERT_TRUE(SimpleCacheMakeBadChecksumEntry(key, kLargeSize));

  disk_cache::Entry* entry = nullptr;

  // Open the entry. Can't spot the checksum that quickly with it so
  // huge.
  ASSERT_THAT(OpenEntry(key, &entry), IsOk());
  ScopedEntryPtr entry_closer(entry);

  EXPECT_GE(kLargeSize, entry->GetDataSize(1));
  auto read_buffer = base::MakeRefCounted<net::IOBufferWithSize>(kLargeSize);
  EXPECT_EQ(net::ERR_CACHE_CHECKSUM_MISMATCH,
            ReadData(entry, 1, 0, read_buffer.get(), kLargeSize));
}

// Tests that an entry that has had an IO error occur can still be Doomed().
TEST_F(DiskCacheEntryTest, SimpleCacheErrorThenDoom) {
  SetSimpleCacheMode();
  InitCache();

  const char key[] = "the first key";
  const int kLargeSize = 50000;
  ASSERT_TRUE(SimpleCacheMakeBadChecksumEntry(key, kLargeSize));

  disk_cache::Entry* entry = nullptr;

  // Open the entry, forcing an IO error.
  ASSERT_THAT(OpenEntry(key, &entry), IsOk());
  ScopedEntryPtr entry_closer(entry);

  EXPECT_GE(kLargeSize, entry->GetDataSize(1));
  auto read_buffer = base::MakeRefCounted<net::IOBufferWithSize>(kLargeSize);
  EXPECT_EQ(net::ERR_CACHE_CHECKSUM_MISMATCH,
            ReadData(entry, 1, 0, read_buffer.get(), kLargeSize));
  entry->Doom();  // Should not crash.
}

TEST_F(DiskCacheEntryTest, SimpleCacheCreateAfterDiskLayerDoom) {
  // Code coverage for what happens when a queued create runs after failure
  // was noticed at SimpleSynchronousEntry layer.
  SetSimpleCacheMode();
  // Disable optimistic ops so we can block on CreateEntry and start
  // WriteData off with an empty op queue.
  SetCacheType(net::APP_CACHE);
  InitCache();

  const char key[] = "the key";
  const int kSize1 = 10;
  auto buffer1 = base::MakeRefCounted<net::IOBufferWithSize>(kSize1);
  CacheTestFillBuffer(buffer1->data(), kSize1, false);

  disk_cache::Entry* entry = nullptr;
  ASSERT_EQ(net::OK, CreateEntry(key, &entry));
  ASSERT_TRUE(entry != nullptr);

  // Make an empty _1 file, to cause a stream 2 write to fail.
  base::FilePath entry_file1_path = cache_path_.AppendASCII(
      disk_cache::simple_util::GetFilenameFromKeyAndFileIndex(key, 1));
  base::File entry_file1(entry_file1_path,
                         base::File::FLAG_WRITE | base::File::FLAG_CREATE);
  ASSERT_TRUE(entry_file1.IsValid());

  entry->WriteData(2, 0, buffer1.get(), kSize1, net::CompletionOnceCallback(),
                   /* truncate= */ true);
  entry->Close();

  // At this point we have put WriteData & Close on the queue, and WriteData
  // started, but we haven't given the event loop control so the failure
  // hasn't been reported and handled here, so the entry is still active
  // for the key. Queue up another create for same key, and run through the
  // events.
  disk_cache::Entry* entry2 = nullptr;
  ASSERT_EQ(net::ERR_FAILED, CreateEntry(key, &entry2));
  ASSERT_TRUE(entry2 == nullptr);

  EXPECT_EQ(0, cache_->GetEntryCount());

  // Should be able to create properly next time, though.
  disk_cache::Entry* entry3 = nullptr;
  ASSERT_EQ(net::OK, CreateEntry(key, &entry3));
  ASSERT_TRUE(entry3 != nullptr);
  entry3->Close();
}

TEST_F(DiskCacheEntryTest, SimpleCacheQueuedOpenOnDoomedEntry) {
  // This tests the following sequence of ops:
  // A = Create(K);
  // Close(A);
  // B = Open(K);
  // Doom(K);
  // Close(B);
  //
  // ... where the execution of the Open sits on the queue all the way till
  // Doom. This now succeeds, as the doom is merely queued at time of Open,
  // rather than completed.

  SetSimpleCacheMode();
  // Disable optimistic ops so we can block on CreateEntry and start
  // WriteData off with an empty op queue.
  SetCacheType(net::APP_CACHE);
  InitCache();

  const char key[] = "the key";

  disk_cache::Entry* entry = nullptr;
  ASSERT_EQ(net::OK, CreateEntry(key, &entry));  // event loop!
  ASSERT_TRUE(entry != nullptr);

  entry->Close();

  // Done via cache_ -> no event loop.
  TestEntryResultCompletionCallback cb;
  EntryResult result = cache_->OpenEntry(key, net::HIGHEST, cb.callback());
  ASSERT_EQ(net::ERR_IO_PENDING, result.net_error());

  net::TestCompletionCallback cb2;
  cache_->DoomEntry(key, net::HIGHEST, cb2.callback());
  // Now event loop.
  result = cb.WaitForResult();
  EXPECT_EQ(net::OK, result.net_error());
  result.ReleaseEntry()->Close();

  EXPECT_EQ(net::OK, cb2.WaitForResult());
  EXPECT_EQ(0, cache_->GetEntryCount());
}

TEST_F(DiskCacheEntryTest, SimpleCacheDoomErrorRace) {
  // Code coverage for a doom racing with a doom induced by a failure.
  SetSimpleCacheMode();
  // Disable optimistic ops so we can block on CreateEntry and start
  // WriteData off with an empty op queue.
  SetCacheType(net::APP_CACHE);
  InitCache();

  const char kKey[] = "the first key";
  const int kSize1 = 10;
  auto buffer1 = base::MakeRefCounted<net::IOBufferWithSize>(kSize1);
  CacheTestFillBuffer(buffer1->data(), kSize1, false);

  disk_cache::Entry* entry = nullptr;
  ASSERT_EQ(net::OK, CreateEntry(kKey, &entry));
  ASSERT_TRUE(entry != nullptr);

  // Now an empty _1 file, to cause a stream 2 write to fail.
  base::FilePath entry_file1_path = cache_path_.AppendASCII(
      disk_cache::simple_util::GetFilenameFromKeyAndFileIndex(kKey, 1));
  base::File entry_file1(entry_file1_path,
                         base::File::FLAG_WRITE | base::File::FLAG_CREATE);
  ASSERT_TRUE(entry_file1.IsValid());

  entry->WriteData(2, 0, buffer1.get(), kSize1, net::CompletionOnceCallback(),
                   /* truncate= */ true);

  net::TestCompletionCallback cb;
  cache_->DoomEntry(kKey, net::HIGHEST, cb.callback());
  entry->Close();
  EXPECT_EQ(0, cb.WaitForResult());
}

bool TruncatePath(const base::FilePath& file_path, int64_t length) {
  base::File file(file_path, base::File::FLAG_WRITE | base::File::FLAG_OPEN);
  if (!file.IsValid())
    return false;
  return file.SetLength(length);
}

TEST_F(DiskCacheEntryTest, SimpleCacheNoEOF) {
  SetSimpleCacheMode();
  InitCache();

  const std::string key("the first key");

  disk_cache::Entry* entry = nullptr;
  ASSERT_THAT(CreateEntry(key, &entry), IsOk());
  disk_cache::Entry* null = nullptr;
  EXPECT_NE(null, entry);
  entry->Close();
  entry = nullptr;

  // Force the entry to flush to disk, so subsequent platform file operations
  // succed.
  ASSERT_THAT(OpenEntry(key, &entry), IsOk());
  entry->Close();
  entry = nullptr;

  // Truncate the file such that the length isn't sufficient to have an EOF
  // record.
  int kTruncationBytes = -static_cast<int>(sizeof(disk_cache::SimpleFileEOF));
  const base::FilePath entry_path = cache_path_.AppendASCII(
      disk_cache::simple_util::GetFilenameFromKeyAndFileIndex(key, 0));
  const int64_t invalid_size = disk_cache::simple_util::GetFileSizeFromDataSize(
      key.size(), kTruncationBytes);
  EXPECT_TRUE(TruncatePath(entry_path, invalid_size));
  EXPECT_THAT(OpenEntry(key, &entry), IsError(net::ERR_FAILED));
  DisableIntegrityCheck();
}

TEST_F(DiskCacheEntryTest, SimpleCacheNonOptimisticOperationsBasic) {
  // Test sequence:
  // Create, Write, Read, Close.
  SetCacheType(net::APP_CACHE);  // APP_CACHE doesn't use optimistic operations.
  SetSimpleCacheMode();
  InitCache();
  disk_cache::Entry* const null_entry = nullptr;

  disk_cache::Entry* entry = nullptr;
  EXPECT_THAT(CreateEntry("my key", &entry), IsOk());
  ASSERT_NE(null_entry, entry);
  ScopedEntryPtr entry_closer(entry);

  const int kBufferSize = 10;
  scoped_refptr<net::IOBufferWithSize> write_buffer =
      base::MakeRefCounted<net::IOBufferWithSize>(kBufferSize);
  CacheTestFillBuffer(write_buffer->data(), write_buffer->size(), false);
  EXPECT_EQ(
      write_buffer->size(),
      WriteData(entry, 1, 0, write_buffer.get(), write_buffer->size(), false));

  scoped_refptr<net::IOBufferWithSize> read_buffer =
      base::MakeRefCounted<net::IOBufferWithSize>(kBufferSize);
  EXPECT_EQ(read_buffer->size(),
            ReadData(entry, 1, 0, read_buffer.get(), read_buffer->size()));
}

TEST_F(DiskCacheEntryTest, SimpleCacheNonOptimisticOperationsDontBlock) {
  // Test sequence:
  // Create, Write, Close.
  SetCacheType(net::APP_CACHE);  // APP_CACHE doesn't use optimistic operations.
  SetSimpleCacheMode();
  InitCache();
  disk_cache::Entry* const null_entry = nullptr;

  MessageLoopHelper helper;
  CallbackTest create_callback(&helper, false);

  int expected_callback_runs = 0;
  const int kBufferSize = 10;
  scoped_refptr<net::IOBufferWithSize> write_buffer =
      base::MakeRefCounted<net::IOBufferWithSize>(kBufferSize);

  disk_cache::Entry* entry = nullptr;
  EXPECT_THAT(CreateEntry("my key", &entry), IsOk());
  ASSERT_NE(null_entry, entry);
  ScopedEntryPtr entry_closer(entry);

  CacheTestFillBuffer(write_buffer->data(), write_buffer->size(), false);
  CallbackTest write_callback(&helper, false);
  int ret = entry->WriteData(
      1, 0, write_buffer.get(), write_buffer->size(),
      base::BindOnce(&CallbackTest::Run, base::Unretained(&write_callback)),
      false);
  ASSERT_THAT(ret, IsError(net::ERR_IO_PENDING));
  helper.WaitUntilCacheIoFinished(++expected_callback_runs);
}

TEST_F(DiskCacheEntryTest,
       SimpleCacheNonOptimisticOperationsBasicsWithoutWaiting) {
  // Test sequence:
  // Create, Write, Read, Close.
  SetCacheType(net::APP_CACHE);  // APP_CACHE doesn't use optimistic operations.
  SetSimpleCacheMode();
  InitCache();
  disk_cache::Entry* const null_entry = nullptr;
  MessageLoopHelper helper;

  disk_cache::Entry* entry = nullptr;
  // Note that |entry| is only set once CreateEntry() completed which is why we
  // have to wait (i.e. use the helper CreateEntry() function).
  EXPECT_THAT(CreateEntry("my key", &entry), IsOk());
  ASSERT_NE(null_entry, entry);
  ScopedEntryPtr entry_closer(entry);

  const int kBufferSize = 10;
  scoped_refptr<net::IOBufferWithSize> write_buffer =
      base::MakeRefCounted<net::IOBufferWithSize>(kBufferSize);
  CacheTestFillBuffer(write_buffer->data(), write_buffer->size(), false);
  CallbackTest write_callback(&helper, false);
  int ret = entry->WriteData(
      1, 0, write_buffer.get(), write_buffer->size(),
      base::BindOnce(&CallbackTest::Run, base::Unretained(&write_callback)),
      false);
  EXPECT_THAT(ret, IsError(net::ERR_IO_PENDING));
  int expected_callback_runs = 1;

  scoped_refptr<net::IOBufferWithSize> read_buffer =
      base::MakeRefCounted<net::IOBufferWithSize>(kBufferSize);
  CallbackTest read_callback(&helper, false);
  ret = entry->ReadData(
      1, 0, read_buffer.get(), read_buffer->size(),
      base::BindOnce(&CallbackTest::Run, base::Unretained(&read_callback)));
  EXPECT_THAT(ret, IsError(net::ERR_IO_PENDING));
  ++expected_callback_runs;

  helper.WaitUntilCacheIoFinished(expected_callback_runs);
  ASSERT_EQ(read_buffer->size(), write_buffer->size());
  EXPECT_EQ(
      0,
      memcmp(read_buffer->data(), write_buffer->data(), read_buffer->size()));
}

TEST_F(DiskCacheEntryTest, SimpleCacheOptimistic) {
  // Test sequence:
  // Create, Write, Read, Write, Read, Close.
  SetSimpleCacheMode();
  InitCache();
  disk_cache::Entry* null = nullptr;
  const char key[] = "the first key";

  MessageLoopHelper helper;
  CallbackTest callback1(&helper, false);
  CallbackTest callback2(&helper, false);
  CallbackTest callback3(&helper, false);
  CallbackTest callback4(&helper, false);
  CallbackTest callback5(&helper, false);

  int expected = 0;
  const int kSize1 = 10;
  const int kSize2 = 20;
  auto buffer1 = base::MakeRefCounted<net::IOBufferWithSize>(kSize1);
  auto buffer1_read = base::MakeRefCounted<net::IOBufferWithSize>(kSize1);
  auto buffer2 = base::MakeRefCounted<net::IOBufferWithSize>(kSize2);
  auto buffer2_read = base::MakeRefCounted<net::IOBufferWithSize>(kSize2);
  CacheTestFillBuffer(buffer1->data(), kSize1, false);
  CacheTestFillBuffer(buffer2->data(), kSize2, false);

  // Create is optimistic, must return OK.
  EntryResult result =
      cache_->CreateEntry(key, net::HIGHEST,
                          base::BindOnce(&CallbackTest::RunWithEntry,
                                         base::Unretained(&callback1)));
  ASSERT_EQ(net::OK, result.net_error());
  disk_cache::Entry* entry = result.ReleaseEntry();
  ASSERT_NE(null, entry);
  ScopedEntryPtr entry_closer(entry);

  // This write may or may not be optimistic (it depends if the previous
  // optimistic create already finished by the time we call the write here).
  int ret = entry->WriteData(
      1, 0, buffer1.get(), kSize1,
      base::BindOnce(&CallbackTest::Run, base::Unretained(&callback2)), false);
  EXPECT_TRUE(kSize1 == ret || net::ERR_IO_PENDING == ret);
  if (net::ERR_IO_PENDING == ret)
    expected++;

  // This Read must not be optimistic, since we don't support that yet.
  EXPECT_EQ(net::ERR_IO_PENDING,
            entry->ReadData(1, 0, buffer1_read.get(), kSize1,
                            base::BindOnce(&CallbackTest::Run,
                                           base::Unretained(&callback3))));
  expected++;
  EXPECT_TRUE(helper.WaitUntilCacheIoFinished(expected));
  EXPECT_EQ(0, memcmp(buffer1->data(), buffer1_read->data(), kSize1));

  // At this point after waiting, the pending operations queue on the entry
  // should be empty, so the next Write operation must run as optimistic.
  EXPECT_EQ(kSize2,
            entry->WriteData(1, 0, buffer2.get(), kSize2,
                             base::BindOnce(&CallbackTest::Run,
                                            base::Unretained(&callback4)),
                             false));

  // Lets do another read so we block until both the write and the read
  // operation finishes and we can then test for HasOneRef() below.
  EXPECT_EQ(net::ERR_IO_PENDING,
            entry->ReadData(1, 0, buffer2_read.get(), kSize2,
                            base::BindOnce(&CallbackTest::Run,
                                           base::Unretained(&callback5))));
  expected++;

  EXPECT_TRUE(helper.WaitUntilCacheIoFinished(expected));
  EXPECT_EQ(0, memcmp(buffer2->data(), buffer2_read->data(), kSize2));

  // Check that we are not leaking.
  EXPECT_NE(entry, null);
  EXPECT_TRUE(
      static_cast<disk_cache::SimpleEntryImpl*>(entry)->HasOneRef());
}

TEST_F(DiskCacheEntryTest, SimpleCacheOptimistic2) {
  // Test sequence:
  // Create, Open, Close, Close.
  SetSimpleCacheMode();
  InitCache();
  const char key[] = "the first key";

  MessageLoopHelper helper;
  CallbackTest callback1(&helper, false);
  CallbackTest callback2(&helper, false);

  EntryResult result =
      cache_->CreateEntry(key, net::HIGHEST,
                          base::BindOnce(&CallbackTest::RunWithEntry,
                                         base::Unretained(&callback1)));
  ASSERT_EQ(net::OK, result.net_error());
  disk_cache::Entry* entry = result.ReleaseEntry();
  ASSERT_NE(nullptr, entry);
  ScopedEntryPtr entry_closer(entry);

  EntryResult result2 =
      cache_->OpenEntry(key, net::HIGHEST,
                        base::BindOnce(&CallbackTest::RunWithEntry,
                                       base::Unretained(&callback2)));
  ASSERT_EQ(net::ERR_IO_PENDING, result2.net_error());
  ASSERT_TRUE(helper.WaitUntilCacheIoFinished(1));
  result2 = callback2.ReleaseLastEntryResult();
  EXPECT_EQ(net::OK, result2.net_error());
  disk_cache::Entry* entry2 = result2.ReleaseEntry();
  EXPECT_NE(nullptr, entry2);
  EXPECT_EQ(entry, entry2);

  // We have to call close twice, since we called create and open above.
  // (the other closes is from |entry_closer|).
  entry->Close();

  // Check that we are not leaking.
  EXPECT_TRUE(
      static_cast<disk_cache::SimpleEntryImpl*>(entry)->HasOneRef());
}

TEST_F(DiskCacheEntryTest, SimpleCacheOptimistic3) {
  // Test sequence:
  // Create, Close, Open, Close.
  SetSimpleCacheMode();
  InitCache();
  const char key[] = "the first key";

  EntryResult result =
      cache_->CreateEntry(key, net::HIGHEST, EntryResultCallback());
  ASSERT_EQ(net::OK, result.net_error());
  disk_cache::Entry* entry = result.ReleaseEntry();
  ASSERT_NE(nullptr, entry);
  entry->Close();

  TestEntryResultCompletionCallback cb;
  EntryResult result2 = cache_->OpenEntry(key, net::HIGHEST, cb.callback());
  ASSERT_EQ(net::ERR_IO_PENDING, result2.net_error());
  result2 = cb.WaitForResult();
  ASSERT_THAT(result2.net_error(), IsOk());
  disk_cache::Entry* entry2 = result2.ReleaseEntry();
  ScopedEntryPtr entry_closer(entry2);

  EXPECT_NE(nullptr, entry2);
  EXPECT_EQ(entry, entry2);

  // Check that we are not leaking.
  EXPECT_TRUE(
      static_cast<disk_cache::SimpleEntryImpl*>(entry2)->HasOneRef());
}

TEST_F(DiskCacheEntryTest, SimpleCacheOptimistic4) {
  // Test sequence:
  // Create, Close, Write, Open, Open, Close, Write, Read, Close.
  SetSimpleCacheMode();
  InitCache();
  const char key[] = "the first key";

  net::TestCompletionCallback cb;
  const int kSize1 = 10;
  auto buffer1 = base::MakeRefCounted<net::IOBufferWithSize>(kSize1);
  CacheTestFillBuffer(buffer1->data(), kSize1, false);

  EntryResult result =
      cache_->CreateEntry(key, net::HIGHEST, EntryResultCallback());
  ASSERT_EQ(net::OK, result.net_error());
  disk_cache::Entry* entry = result.ReleaseEntry();
  ASSERT_NE(nullptr, entry);
  entry->Close();

  // Lets do a Write so we block until both the Close and the Write
  // operation finishes. Write must fail since we are writing in a closed entry.
  EXPECT_EQ(
      net::ERR_IO_PENDING,
      entry->WriteData(1, 0, buffer1.get(), kSize1, cb.callback(), false));
  EXPECT_THAT(cb.GetResult(net::ERR_IO_PENDING), IsError(net::ERR_FAILED));

  // Finish running the pending tasks so that we fully complete the close
  // operation and destroy the entry object.
  base::RunLoop().RunUntilIdle();

  // At this point the |entry| must have been destroyed, and called
  // RemoveSelfFromBackend().
  TestEntryResultCompletionCallback cb2;
  EntryResult result2 = cache_->OpenEntry(key, net::HIGHEST, cb2.callback());
  ASSERT_EQ(net::ERR_IO_PENDING, result2.net_error());
  result2 = cb2.WaitForResult();
  ASSERT_THAT(result2.net_error(), IsOk());
  disk_cache::Entry* entry2 = result2.ReleaseEntry();
  EXPECT_NE(nullptr, entry2);

  EntryResult result3 = cache_->OpenEntry(key, net::HIGHEST, cb2.callback());
  ASSERT_EQ(net::ERR_IO_PENDING, result3.net_error());
  result3 = cb2.WaitForResult();
  ASSERT_THAT(result3.net_error(), IsOk());
  disk_cache::Entry* entry3 = result3.ReleaseEntry();
  EXPECT_NE(nullptr, entry3);
  EXPECT_EQ(entry2, entry3);
  entry3->Close();

  // The previous Close doesn't actually closes the entry since we opened it
  // twice, so the next Write operation must succeed and it must be able to
  // perform it optimistically, since there is no operation running on this
  // entry.
  EXPECT_EQ(kSize1, entry2->WriteData(1, 0, buffer1.get(), kSize1,
                                      net::CompletionOnceCallback(), false));

  // Lets do another read so we block until both the write and the read
  // operation finishes and we can then test for HasOneRef() below.
  EXPECT_EQ(net::ERR_IO_PENDING,
            entry2->ReadData(1, 0, buffer1.get(), kSize1, cb.callback()));
  EXPECT_EQ(kSize1, cb.GetResult(net::ERR_IO_PENDING));

  // Check that we are not leaking.
  EXPECT_TRUE(
      static_cast<disk_cache::SimpleEntryImpl*>(entry2)->HasOneRef());
  entry2->Close();
}

TEST_F(DiskCacheEntryTest, SimpleCacheOptimistic5) {
  // Test sequence:
  // Create, Doom, Write, Read, Close.
  SetSimpleCacheMode();
  InitCache();
  const char key[] = "the first key";

  net::TestCompletionCallback cb;
  const int kSize1 = 10;
  auto buffer1 = base::MakeRefCounted<net::IOBufferWithSize>(kSize1);
  CacheTestFillBuffer(buffer1->data(), kSize1, false);

  EntryResult result =
      cache_->CreateEntry(key, net::HIGHEST, EntryResultCallback());
  ASSERT_EQ(net::OK, result.net_error());
  disk_cache::Entry* entry = result.ReleaseEntry();
  ASSERT_NE(nullptr, entry);
  ScopedEntryPtr entry_closer(entry);
  entry->Doom();

  EXPECT_EQ(
      net::ERR_IO_PENDING,
      entry->WriteData(1, 0, buffer1.get(), kSize1, cb.callback(), false));
  EXPECT_EQ(kSize1, cb.GetResult(net::ERR_IO_PENDING));

  EXPECT_EQ(net::ERR_IO_PENDING,
            entry->ReadData(1, 0, buffer1.get(), kSize1, cb.callback()));
  EXPECT_EQ(kSize1, cb.GetResult(net::ERR_IO_PENDING));

  // Check that we are not leaking.
  EXPECT_TRUE(
      static_cast<disk_cache::SimpleEntryImpl*>(entry)->HasOneRef());
}

TEST_F(DiskCacheEntryTest, SimpleCacheOptimistic6) {
  // Test sequence:
  // Create, Write, Doom, Doom, Read, Doom, Close.
  SetSimpleCacheMode();
  InitCache();
  const char key[] = "the first key";

  net::TestCompletionCallback cb;
  const int kSize1 = 10;
  auto buffer1 = base::MakeRefCounted<net::IOBufferWithSize>(kSize1);
  auto buffer1_read = base::MakeRefCounted<net::IOBufferWithSize>(kSize1);
  CacheTestFillBuffer(buffer1->data(), kSize1, false);

  EntryResult result =
      cache_->CreateEntry(key, net::HIGHEST, EntryResultCallback());
  ASSERT_EQ(net::OK, result.net_error());
  disk_cache::Entry* entry = result.ReleaseEntry();
  EXPECT_NE(nullptr, entry);
  ScopedEntryPtr entry_closer(entry);

  EXPECT_EQ(
      net::ERR_IO_PENDING,
      entry->WriteData(1, 0, buffer1.get(), kSize1, cb.callback(), false));
  EXPECT_EQ(kSize1, cb.GetResult(net::ERR_IO_PENDING));

  entry->Doom();
  entry->Doom();

  // This Read must not be optimistic, since we don't support that yet.
  EXPECT_EQ(net::ERR_IO_PENDING,
            entry->ReadData(1, 0, buffer1_read.get(), kSize1, cb.callback()));
  EXPECT_EQ(kSize1, cb.GetResult(net::ERR_IO_PENDING));
  EXPECT_EQ(0, memcmp(buffer1->data(), buffer1_read->data(), kSize1));

  entry->Doom();
}

// Confirm that IO buffers are not referenced by the Simple Cache after a write
// completes.
TEST_F(DiskCacheEntryTest, SimpleCacheOptimisticWriteReleases) {
  SetSimpleCacheMode();
  InitCache();

  const char key[] = "the first key";

  // First, an optimistic create.
  EntryResult result =
      cache_->CreateEntry(key, net::HIGHEST, EntryResultCallback());
  ASSERT_EQ(net::OK, result.net_error());
  disk_cache::Entry* entry = result.ReleaseEntry();
  ASSERT_TRUE(entry);
  ScopedEntryPtr entry_closer(entry);

  const int kWriteSize = 512;
  auto buffer1 = base::MakeRefCounted<net::IOBufferWithSize>(kWriteSize);
  EXPECT_TRUE(buffer1->HasOneRef());
  CacheTestFillBuffer(buffer1->data(), kWriteSize, false);

  // An optimistic write happens only when there is an empty queue of pending
  // operations. To ensure the queue is empty, we issue a write and wait until
  // it completes.
  EXPECT_EQ(kWriteSize,
            WriteData(entry, 1, 0, buffer1.get(), kWriteSize, false));
  EXPECT_TRUE(buffer1->HasOneRef());

  // Finally, we should perform an optimistic write and confirm that all
  // references to the IO buffer have been released.
  EXPECT_EQ(kWriteSize, entry->WriteData(1, 0, buffer1.get(), kWriteSize,
                                         net::CompletionOnceCallback(), false));
  EXPECT_TRUE(buffer1->HasOneRef());
}

TEST_F(DiskCacheEntryTest, SimpleCacheCreateDoomRace) {
  // Test sequence:
  // Create, Doom, Write, Close, Check files are not on disk anymore.
  SetSimpleCacheMode();
  InitCache();
  const char key[] = "the first key";

  net::TestCompletionCallback cb;
  const int kSize1 = 10;
  auto buffer1 = base::MakeRefCounted<net::IOBufferWithSize>(kSize1);
  CacheTestFillBuffer(buffer1->data(), kSize1, false);

  EntryResult result =
      cache_->CreateEntry(key, net::HIGHEST, EntryResultCallback());
  ASSERT_EQ(net::OK, result.net_error());
  disk_cache::Entry* entry = result.ReleaseEntry();
  EXPECT_NE(nullptr, entry);

  EXPECT_THAT(cache_->DoomEntry(key, net::HIGHEST, cb.callback()),
              IsError(net::ERR_IO_PENDING));
  EXPECT_THAT(cb.GetResult(net::ERR_IO_PENDING), IsOk());

  EXPECT_EQ(
      kSize1,
      entry->WriteData(0, 0, buffer1.get(), kSize1, cb.callback(), false));

  entry->Close();

  // Finish running the pending tasks so that we fully complete the close
  // operation and destroy the entry object.
  base::RunLoop().RunUntilIdle();

  for (int i = 0; i < disk_cache::kSimpleEntryNormalFileCount; ++i) {
    base::FilePath entry_file_path = cache_path_.AppendASCII(
        disk_cache::simple_util::GetFilenameFromKeyAndFileIndex(key, i));
    base::File::Info info;
    EXPECT_FALSE(base::GetFileInfo(entry_file_path, &info));
  }
}

TEST_F(DiskCacheEntryTest, SimpleCacheDoomCreateRace) {
  // This test runs as APP_CACHE to make operations more synchronous. Test
  // sequence:
  // Create, Doom, Create.
  SetCacheType(net::APP_CACHE);
  SetSimpleCacheMode();
  InitCache();
  const char key[] = "the first key";

  TestEntryResultCompletionCallback create_callback;

  EntryResult result1 = create_callback.GetResult(
      cache_->CreateEntry(key, net::HIGHEST, create_callback.callback()));
  ASSERT_EQ(net::OK, result1.net_error());
  disk_cache::Entry* entry1 = result1.ReleaseEntry();
  ScopedEntryPtr entry1_closer(entry1);
  EXPECT_NE(nullptr, entry1);

  net::TestCompletionCallback doom_callback;
  EXPECT_EQ(net::ERR_IO_PENDING,
            cache_->DoomEntry(key, net::HIGHEST, doom_callback.callback()));

  EntryResult result2 = create_callback.GetResult(
      cache_->CreateEntry(key, net::HIGHEST, create_callback.callback()));
  ASSERT_EQ(net::OK, result2.net_error());
  disk_cache::Entry* entry2 = result2.ReleaseEntry();
  ScopedEntryPtr entry2_closer(entry2);
  EXPECT_THAT(doom_callback.GetResult(net::ERR_IO_PENDING), IsOk());
}

TEST_F(DiskCacheEntryTest, SimpleCacheDoomCreateOptimistic) {
  // Test that we optimize the doom -> create sequence when optimistic ops
  // are on.
  SetSimpleCacheMode();
  InitCache();
  const char kKey[] = "the key";

  // Create entry and initiate its Doom.
  disk_cache::Entry* entry1 = nullptr;
  ASSERT_THAT(CreateEntry(kKey, &entry1), IsOk());
  ASSERT_TRUE(entry1 != nullptr);

  net::TestCompletionCallback doom_callback;
  cache_->DoomEntry(kKey, net::HIGHEST, doom_callback.callback());

  TestEntryResultCompletionCallback create_callback;
  // Open entry2, with same key. With optimistic ops, this should succeed
  // immediately, hence us using cache_->CreateEntry directly rather than using
  // the DiskCacheTestWithCache::CreateEntry wrapper which blocks when needed.
  EntryResult result2 =
      cache_->CreateEntry(kKey, net::HIGHEST, create_callback.callback());
  ASSERT_EQ(net::OK, result2.net_error());
  disk_cache::Entry* entry2 = result2.ReleaseEntry();
  ASSERT_NE(nullptr, entry2);

  // Do some I/O to make sure it's alive.
  const int kSize = 2048;
  auto buf_1 = base::MakeRefCounted<net::IOBufferWithSize>(kSize);
  auto buf_2 = base::MakeRefCounted<net::IOBufferWithSize>(kSize);
  CacheTestFillBuffer(buf_1->data(), kSize, false);

  EXPECT_EQ(kSize, WriteData(entry2, /* index = */ 1, /* offset = */ 0,
                             buf_1.get(), kSize, /* truncate = */ false));
  EXPECT_EQ(kSize, ReadData(entry2, /* index = */ 1, /* offset = */ 0,
                            buf_2.get(), kSize));

  doom_callback.WaitForResult();

  entry1->Close();
  entry2->Close();
}

TEST_F(DiskCacheEntryTest, SimpleCacheDoomCreateOptimisticMassDoom) {
  // Test that shows that a certain DCHECK in mass doom code had to be removed
  // once optimistic doom -> create was added.
  SetSimpleCacheMode();
  InitCache();
  const char kKey[] = "the key";

  // Create entry and initiate its Doom.
  disk_cache::Entry* entry1 = nullptr;
  ASSERT_THAT(CreateEntry(kKey, &entry1), IsOk());
  ASSERT_TRUE(entry1 != nullptr);

  net::TestCompletionCallback doom_callback;
  cache_->DoomEntry(kKey, net::HIGHEST, doom_callback.callback());

  TestEntryResultCompletionCallback create_callback;
  // Open entry2, with same key. With optimistic ops, this should succeed
  // immediately, hence us using cache_->CreateEntry directly rather than using
  // the DiskCacheTestWithCache::CreateEntry wrapper which blocks when needed.
  EntryResult result =
      cache_->CreateEntry(kKey, net::HIGHEST, create_callback.callback());
  ASSERT_EQ(net::OK, result.net_error());
  disk_cache::Entry* entry2 = result.ReleaseEntry();
  ASSERT_NE(nullptr, entry2);

  net::TestCompletionCallback doomall_callback;

  // This is what had code that had a no-longer valid DCHECK.
  cache_->DoomAllEntries(doomall_callback.callback());

  doom_callback.WaitForResult();
  doomall_callback.WaitForResult();

  entry1->Close();
  entry2->Close();
}

TEST_F(DiskCacheEntryTest, SimpleCacheDoomOpenOptimistic) {
  // Test that we optimize the doom -> optimize sequence when optimistic ops
  // are on.
  SetSimpleCacheMode();
  InitCache();
  const char kKey[] = "the key";

  // Create entry and initiate its Doom.
  disk_cache::Entry* entry1 = nullptr;
  ASSERT_THAT(CreateEntry(kKey, &entry1), IsOk());
  ASSERT_TRUE(entry1 != nullptr);
  entry1->Close();

  net::TestCompletionCallback doom_callback;
  cache_->DoomEntry(kKey, net::HIGHEST, doom_callback.callback());

  // Try to open entry. This should detect a miss immediately, since it's
  // the only thing after a doom.

  EntryResult result2 =
      cache_->OpenEntry(kKey, net::HIGHEST, EntryResultCallback());
  EXPECT_EQ(net::ERR_FAILED, result2.net_error());
  EXPECT_EQ(nullptr, result2.ReleaseEntry());
  doom_callback.WaitForResult();
}

TEST_F(DiskCacheEntryTest, SimpleCacheDoomDoom) {
  // Test sequence:
  // Create, Doom, Create, Doom (1st entry), Open.
  SetSimpleCacheMode();
  InitCache();
  disk_cache::Entry* null = nullptr;

  const char key[] = "the first key";

  disk_cache::Entry* entry1 = nullptr;
  ASSERT_THAT(CreateEntry(key, &entry1), IsOk());
  ScopedEntryPtr entry1_closer(entry1);
  EXPECT_NE(null, entry1);

  EXPECT_THAT(DoomEntry(key), IsOk());

  disk_cache::Entry* entry2 = nullptr;
  ASSERT_THAT(CreateEntry(key, &entry2), IsOk());
  ScopedEntryPtr entry2_closer(entry2);
  EXPECT_NE(null, entry2);

  // Redundantly dooming entry1 should not delete entry2.
  disk_cache::SimpleEntryImpl* simple_entry1 =
      static_cast<disk_cache::SimpleEntryImpl*>(entry1);
  net::TestCompletionCallback cb;
  EXPECT_EQ(net::OK,
            cb.GetResult(simple_entry1->DoomEntry(cb.callback())));

  disk_cache::Entry* entry3 = nullptr;
  ASSERT_THAT(OpenEntry(key, &entry3), IsOk());
  ScopedEntryPtr entry3_closer(entry3);
  EXPECT_NE(null, entry3);
}

TEST_F(DiskCacheEntryTest, SimpleCacheDoomCreateDoom) {
  // Test sequence:
  // Create, Doom, Create, Doom.
  SetSimpleCacheMode();
  InitCache();

  disk_cache::Entry* null = nullptr;

  const char key[] = "the first key";

  disk_cache::Entry* entry1 = nullptr;
  ASSERT_THAT(CreateEntry(key, &entry1), IsOk());
  ScopedEntryPtr entry1_closer(entry1);
  EXPECT_NE(null, entry1);

  entry1->Doom();

  disk_cache::Entry* entry2 = nullptr;
  ASSERT_THAT(CreateEntry(key, &entry2), IsOk());
  ScopedEntryPtr entry2_closer(entry2);
  EXPECT_NE(null, entry2);

  entry2->Doom();

  // This test passes if it doesn't crash.
}

TEST_F(DiskCacheEntryTest, SimpleCacheDoomCloseCreateCloseOpen) {
  // Test sequence: Create, Doom, Close, Create, Close, Open.
  SetSimpleCacheMode();
  InitCache();

  disk_cache::Entry* null = nullptr;

  const char key[] = "this is a key";

  disk_cache::Entry* entry1 = nullptr;
  ASSERT_THAT(CreateEntry(key, &entry1), IsOk());
  ScopedEntryPtr entry1_closer(entry1);
  EXPECT_NE(null, entry1);

  entry1->Doom();
  entry1_closer.reset();
  entry1 = nullptr;

  disk_cache::Entry* entry2 = nullptr;
  ASSERT_THAT(CreateEntry(key, &entry2), IsOk());
  ScopedEntryPtr entry2_closer(entry2);
  EXPECT_NE(null, entry2);

  entry2_closer.reset();
  entry2 = nullptr;

  disk_cache::Entry* entry3 = nullptr;
  ASSERT_THAT(OpenEntry(key, &entry3), IsOk());
  ScopedEntryPtr entry3_closer(entry3);
  EXPECT_NE(null, entry3);
}

// Checks that an optimistic Create would fail later on a racing Open.
TEST_F(DiskCacheEntryTest, SimpleCacheOptimisticCreateFailsOnOpen) {
  SetSimpleCacheMode();
  InitCache();

  // Create a corrupt file in place of a future entry. Optimistic create should
  // initially succeed, but realize later that creation failed.
  const std::string key = "the key";
  disk_cache::Entry* entry = nullptr;
  disk_cache::Entry* entry2 = nullptr;

  EXPECT_TRUE(disk_cache::simple_util::CreateCorruptFileForTests(
      key, cache_path_));
  EntryResult result =
      cache_->CreateEntry(key, net::HIGHEST, EntryResultCallback());
  EXPECT_THAT(result.net_error(), IsOk());
  entry = result.ReleaseEntry();
  ASSERT_TRUE(entry);
  ScopedEntryPtr entry_closer(entry);
  ASSERT_NE(net::OK, OpenEntry(key, &entry2));

  // Check that we are not leaking.
  EXPECT_TRUE(
      static_cast<disk_cache::SimpleEntryImpl*>(entry)->HasOneRef());

  DisableIntegrityCheck();
}

// Tests that old entries are evicted while new entries remain in the index.
// This test relies on non-mandatory properties of the simple Cache Backend:
// LRU eviction, specific values of high-watermark and low-watermark etc.
// When changing the eviction algorithm, the test will have to be re-engineered.
TEST_F(DiskCacheEntryTest, SimpleCacheEvictOldEntries) {
  const int kMaxSize = 200 * 1024;
  const int kWriteSize = kMaxSize / 10;
  const int kNumExtraEntries = 12;
  SetSimpleCacheMode();
  SetMaxSize(kMaxSize);
  InitCache();

  std::string key1("the first key");
  disk_cache::Entry* entry;
  ASSERT_THAT(CreateEntry(key1, &entry), IsOk());
  auto buffer = base::MakeRefCounted<net::IOBufferWithSize>(kWriteSize);
  CacheTestFillBuffer(buffer->data(), kWriteSize, false);
  EXPECT_EQ(kWriteSize,
            WriteData(entry, 1, 0, buffer.get(), kWriteSize, false));
  entry->Close();
  AddDelay();

  std::string key2("the key prefix");
  for (int i = 0; i < kNumExtraEntries; i++) {
    if (i == kNumExtraEntries - 2) {
      // Create a distinct timestamp for the last two entries. These entries
      // will be checked for outliving the eviction.
      AddDelay();
    }
    ASSERT_THAT(CreateEntry(key2 + base::NumberToString(i), &entry), IsOk());
    ScopedEntryPtr entry_closer(entry);
    EXPECT_EQ(kWriteSize,
              WriteData(entry, 1, 0, buffer.get(), kWriteSize, false));
  }

  // TODO(pasko): Find a way to wait for the eviction task(s) to finish by using
  // the internal knowledge about |SimpleBackendImpl|.
  ASSERT_NE(net::OK, OpenEntry(key1, &entry))
      << "Should have evicted the old entry";
  for (int i = 0; i < 2; i++) {
    int entry_no = kNumExtraEntries - i - 1;
    // Generally there is no guarantee that at this point the backround eviction
    // is finished. We are testing the positive case, i.e. when the eviction
    // never reaches this entry, should be non-flaky.
    ASSERT_EQ(net::OK, OpenEntry(key2 + base::NumberToString(entry_no), &entry))
        << "Should not have evicted fresh entry " << entry_no;
    entry->Close();
  }
}

// Tests that if a read and a following in-flight truncate are both in progress
// simultaniously that they both can occur successfully. See
// http://crbug.com/239223
TEST_F(DiskCacheEntryTest, SimpleCacheInFlightTruncate)  {
  SetSimpleCacheMode();
  InitCache();

  const char key[] = "the first key";

  // We use a very large entry size here to make sure this doesn't hit
  // the prefetch path for any concievable setting. Hitting prefetch would
  // make us serve the read below from memory entirely on I/O thread, missing
  // the point of the test which coverred two concurrent disk ops, with
  // portions of work happening on the workpool.
  const int kBufferSize = 50000;
  auto write_buffer = base::MakeRefCounted<net::IOBufferWithSize>(kBufferSize);
  CacheTestFillBuffer(write_buffer->data(), kBufferSize, false);

  disk_cache::Entry* entry = nullptr;
  ASSERT_THAT(CreateEntry(key, &entry), IsOk());

  EXPECT_EQ(kBufferSize,
            WriteData(entry, 1, 0, write_buffer.get(), kBufferSize, false));
  entry->Close();
  entry = nullptr;

  ASSERT_THAT(OpenEntry(key, &entry), IsOk());
  ScopedEntryPtr entry_closer(entry);

  MessageLoopHelper helper;
  int expected = 0;

  // Make a short read.
  const int kReadBufferSize = 512;
  auto read_buffer =
      base::MakeRefCounted<net::IOBufferWithSize>(kReadBufferSize);
  CallbackTest read_callback(&helper, false);
  EXPECT_EQ(net::ERR_IO_PENDING,
            entry->ReadData(1, 0, read_buffer.get(), kReadBufferSize,
                            base::BindOnce(&CallbackTest::Run,
                                           base::Unretained(&read_callback))));
  ++expected;

  // Truncate the entry to the length of that read.
  auto truncate_buffer =
      base::MakeRefCounted<net::IOBufferWithSize>(kReadBufferSize);
  CacheTestFillBuffer(truncate_buffer->data(), kReadBufferSize, false);
  CallbackTest truncate_callback(&helper, false);
  EXPECT_EQ(
      net::ERR_IO_PENDING,
      entry->WriteData(1, 0, truncate_buffer.get(), kReadBufferSize,
                       base::BindOnce(&CallbackTest::Run,
                                      base::Unretained(&truncate_callback)),
                       true));
  ++expected;

  // Wait for both the read and truncation to finish, and confirm that both
  // succeeded.
  EXPECT_TRUE(helper.WaitUntilCacheIoFinished(expected));
  EXPECT_EQ(kReadBufferSize, read_callback.last_result());
  EXPECT_EQ(kReadBufferSize, truncate_callback.last_result());
  EXPECT_EQ(0,
            memcmp(write_buffer->data(), read_buffer->data(), kReadBufferSize));
}

// Tests that if a write and a read dependant on it are both in flight
// simultaneiously that they both can complete successfully without erroneous
// early returns. See http://crbug.com/239223
TEST_F(DiskCacheEntryTest, SimpleCacheInFlightRead) {
  SetSimpleCacheMode();
  InitCache();

  const char key[] = "the first key";
  EntryResult result =
      cache_->CreateEntry(key, net::HIGHEST, EntryResultCallback());
  ASSERT_EQ(net::OK, result.net_error());
  disk_cache::Entry* entry = result.ReleaseEntry();
  ScopedEntryPtr entry_closer(entry);

  const int kBufferSize = 1024;
  auto write_buffer = base::MakeRefCounted<net::IOBufferWithSize>(kBufferSize);
  CacheTestFillBuffer(write_buffer->data(), kBufferSize, false);

  MessageLoopHelper helper;
  int expected = 0;

  CallbackTest write_callback(&helper, false);
  EXPECT_EQ(net::ERR_IO_PENDING,
            entry->WriteData(1, 0, write_buffer.get(), kBufferSize,
                             base::BindOnce(&CallbackTest::Run,
                                            base::Unretained(&write_callback)),
                             true));
  ++expected;

  auto read_buffer = base::MakeRefCounted<net::IOBufferWithSize>(kBufferSize);
  CallbackTest read_callback(&helper, false);
  EXPECT_EQ(net::ERR_IO_PENDING,
            entry->ReadData(1, 0, read_buffer.get(), kBufferSize,
                            base::BindOnce(&CallbackTest::Run,
                                           base::Unretained(&read_callback))));
  ++expected;

  EXPECT_TRUE(helper.WaitUntilCacheIoFinished(expected));
  EXPECT_EQ(kBufferSize, write_callback.last_result());
  EXPECT_EQ(kBufferSize, read_callback.last_result());
  EXPECT_EQ(0, memcmp(write_buffer->data(), read_buffer->data(), kBufferSize));
}

TEST_F(DiskCacheEntryTest, SimpleCacheOpenCreateRaceWithNoIndex) {
  SetSimpleCacheMode();
  DisableSimpleCacheWaitForIndex();
  DisableIntegrityCheck();
  InitCache();

  // Assume the index is not initialized, which is likely, since we are blocking
  // the IO thread from executing the index finalization step.
  TestEntryResultCompletionCallback cb1;
  TestEntryResultCompletionCallback cb2;
  EntryResult rv1 = cache_->OpenEntry("key", net::HIGHEST, cb1.callback());
  EntryResult rv2 = cache_->CreateEntry("key", net::HIGHEST, cb2.callback());

  rv1 = cb1.GetResult(std::move(rv1));
  EXPECT_THAT(rv1.net_error(), IsError(net::ERR_FAILED));
  rv2 = cb2.GetResult(std::move(rv2));
  ASSERT_THAT(rv2.net_error(), IsOk());
  disk_cache::Entry* entry2 = rv2.ReleaseEntry();

  // Try to get an alias for entry2. Open should succeed, and return the same
  // pointer.
  disk_cache::Entry* entry3 = nullptr;
  ASSERT_EQ(net::OK, OpenEntry("key", &entry3));
  EXPECT_EQ(entry3, entry2);

  entry2->Close();
  entry3->Close();
}

// Checking one more scenario of overlapped reading of a bad entry.
// Differs from the |SimpleCacheMultipleReadersCheckCRC| only by the order of
// last two reads.
TEST_F(DiskCacheEntryTest, SimpleCacheMultipleReadersCheckCRC2) {
  SetSimpleCacheMode();
  InitCache();

  const char key[] = "key";
  int size = 50000;
  ASSERT_TRUE(SimpleCacheMakeBadChecksumEntry(key, size));

  auto read_buffer1 = base::MakeRefCounted<net::IOBufferWithSize>(size);
  auto read_buffer2 = base::MakeRefCounted<net::IOBufferWithSize>(size);

  // Advance the first reader a little.
  disk_cache::Entry* entry = nullptr;
  ASSERT_THAT(OpenEntry(key, &entry), IsOk());
  ScopedEntryPtr entry_closer(entry);
  EXPECT_EQ(1, ReadData(entry, 1, 0, read_buffer1.get(), 1));

  // Advance the 2nd reader by the same amount.
  disk_cache::Entry* entry2 = nullptr;
  EXPECT_THAT(OpenEntry(key, &entry2), IsOk());
  ScopedEntryPtr entry2_closer(entry2);
  EXPECT_EQ(1, ReadData(entry2, 1, 0, read_buffer2.get(), 1));

  // Continue reading 1st.
  EXPECT_GT(0, ReadData(entry, 1, 1, read_buffer1.get(), size));

  // This read should fail as well because we have previous read failures.
  EXPECT_GT(0, ReadData(entry2, 1, 1, read_buffer2.get(), 1));
  DisableIntegrityCheck();
}

// Test if we can sequentially read each subset of the data until all the data
// is read, then the CRC is calculated correctly and the reads are successful.
TEST_F(DiskCacheEntryTest, SimpleCacheReadCombineCRC) {
  // Test sequence:
  // Create, Write, Read (first half of data), Read (second half of data),
  // Close.
  SetSimpleCacheMode();
  InitCache();
  disk_cache::Entry* null = nullptr;
  const char key[] = "the first key";

  const int kHalfSize = 200;
  const int kSize = 2 * kHalfSize;
  auto buffer1 = base::MakeRefCounted<net::IOBufferWithSize>(kSize);
  CacheTestFillBuffer(buffer1->data(), kSize, false);
  disk_cache::Entry* entry = nullptr;

  ASSERT_THAT(CreateEntry(key, &entry), IsOk());
  EXPECT_NE(null, entry);

  EXPECT_EQ(kSize, WriteData(entry, 1, 0, buffer1.get(), kSize, false));
  entry->Close();

  disk_cache::Entry* entry2 = nullptr;
  ASSERT_THAT(OpenEntry(key, &entry2), IsOk());
  EXPECT_EQ(entry, entry2);

  // Read the first half of the data.
  int offset = 0;
  int buf_len = kHalfSize;
  auto buffer1_read1 = base::MakeRefCounted<net::IOBufferWithSize>(buf_len);
  EXPECT_EQ(buf_len, ReadData(entry2, 1, offset, buffer1_read1.get(), buf_len));
  EXPECT_EQ(0, memcmp(buffer1->data(), buffer1_read1->data(), buf_len));

  // Read the second half of the data.
  offset = buf_len;
  buf_len = kHalfSize;
  auto buffer1_read2 = base::MakeRefCounted<net::IOBufferWithSize>(buf_len);
  EXPECT_EQ(buf_len, ReadData(entry2, 1, offset, buffer1_read2.get(), buf_len));
  char* buffer1_data = buffer1->data() + offset;
  EXPECT_EQ(0, memcmp(buffer1_data, buffer1_read2->data(), buf_len));

  // Check that we are not leaking.
  EXPECT_NE(entry, null);
  EXPECT_TRUE(
      static_cast<disk_cache::SimpleEntryImpl*>(entry)->HasOneRef());
  entry->Close();
  entry = nullptr;
}

// Test if we can write the data not in sequence and read correctly. In
// this case the CRC will not be present.
TEST_F(DiskCacheEntryTest, SimpleCacheNonSequentialWrite) {
  // Test sequence:
  // Create, Write (second half of data), Write (first half of data), Read,
  // Close.
  SetSimpleCacheMode();
  InitCache();
  disk_cache::Entry* null = nullptr;
  const char key[] = "the first key";

  const int kHalfSize = 200;
  const int kSize = 2 * kHalfSize;
  auto buffer1 = base::MakeRefCounted<net::IOBufferWithSize>(kSize);
  auto buffer2 = base::MakeRefCounted<net::IOBufferWithSize>(kSize);
  CacheTestFillBuffer(buffer1->data(), kSize, false);
  char* buffer1_data = buffer1->data() + kHalfSize;
  memcpy(buffer2->data(), buffer1_data, kHalfSize);

  disk_cache::Entry* entry = nullptr;
  ASSERT_THAT(CreateEntry(key, &entry), IsOk());
  entry->Close();
  for (int i = 0; i < disk_cache::kSimpleEntryStreamCount; ++i) {
    ASSERT_THAT(OpenEntry(key, &entry), IsOk());
    EXPECT_NE(null, entry);

    int offset = kHalfSize;
    int buf_len = kHalfSize;

    EXPECT_EQ(buf_len,
              WriteData(entry, i, offset, buffer2.get(), buf_len, false));
    offset = 0;
    buf_len = kHalfSize;
    EXPECT_EQ(buf_len,
              WriteData(entry, i, offset, buffer1.get(), buf_len, false));
    entry->Close();

    ASSERT_THAT(OpenEntry(key, &entry), IsOk());

    auto buffer1_read1 = base::MakeRefCounted<net::IOBufferWithSize>(kSize);
    EXPECT_EQ(kSize, ReadData(entry, i, 0, buffer1_read1.get(), kSize));
    EXPECT_EQ(0, memcmp(buffer1->data(), buffer1_read1->data(), kSize));
    // Check that we are not leaking.
    ASSERT_NE(entry, null);
    EXPECT_TRUE(static_cast<disk_cache::SimpleEntryImpl*>(entry)->HasOneRef());
    entry->Close();
  }
}

// Test that changing stream1 size does not affect stream0 (stream0 and stream1
// are stored in the same file in Simple Cache).
TEST_F(DiskCacheEntryTest, SimpleCacheStream1SizeChanges) {
  SetSimpleCacheMode();
  InitCache();
  disk_cache::Entry* entry = nullptr;
  const std::string key("the key");
  const int kSize = 100;
  auto buffer = base::MakeRefCounted<net::IOBufferWithSize>(kSize);
  auto buffer_read = base::MakeRefCounted<net::IOBufferWithSize>(kSize);
  CacheTestFillBuffer(buffer->data(), kSize, false);

  ASSERT_THAT(CreateEntry(key, &entry), IsOk());
  EXPECT_TRUE(entry);

  // Write something into stream0.
  EXPECT_EQ(kSize, WriteData(entry, 0, 0, buffer.get(), kSize, false));
  EXPECT_EQ(kSize, ReadData(entry, 0, 0, buffer_read.get(), kSize));
  EXPECT_EQ(0, memcmp(buffer->data(), buffer_read->data(), kSize));
  entry->Close();

  // Extend stream1.
  ASSERT_THAT(OpenEntry(key, &entry), IsOk());
  int stream1_size = 100;
  EXPECT_EQ(0, WriteData(entry, 1, stream1_size, buffer.get(), 0, false));
  EXPECT_EQ(stream1_size, entry->GetDataSize(1));
  entry->Close();

  // Check that stream0 data has not been modified and that the EOF record for
  // stream 0 contains a crc.
  // The entry needs to be reopened before checking the crc: Open will perform
  // the synchronization with the previous Close. This ensures the EOF records
  // have been written to disk before we attempt to read them independently.
  ASSERT_THAT(OpenEntry(key, &entry), IsOk());
  base::FilePath entry_file0_path = cache_path_.AppendASCII(
      disk_cache::simple_util::GetFilenameFromKeyAndFileIndex(key, 0));
  base::File entry_file0(entry_file0_path,
                         base::File::FLAG_READ | base::File::FLAG_OPEN);
  ASSERT_TRUE(entry_file0.IsValid());

  int data_size[disk_cache::kSimpleEntryStreamCount] = {kSize, stream1_size, 0};
  int sparse_data_size = 0;
  disk_cache::SimpleEntryStat entry_stat(
      base::Time::Now(), base::Time::Now(), data_size, sparse_data_size);
  int eof_offset = entry_stat.GetEOFOffsetInFile(key.size(), 0);
  disk_cache::SimpleFileEOF eof_record;
  ASSERT_EQ(static_cast<int>(sizeof(eof_record)),
            entry_file0.Read(eof_offset, reinterpret_cast<char*>(&eof_record),
                             sizeof(eof_record)));
  EXPECT_EQ(disk_cache::kSimpleFinalMagicNumber, eof_record.final_magic_number);
  EXPECT_TRUE((eof_record.flags & disk_cache::SimpleFileEOF::FLAG_HAS_CRC32) ==
              disk_cache::SimpleFileEOF::FLAG_HAS_CRC32);

  buffer_read = base::MakeRefCounted<net::IOBufferWithSize>(kSize);
  EXPECT_EQ(kSize, ReadData(entry, 0, 0, buffer_read.get(), kSize));
  EXPECT_EQ(0, memcmp(buffer->data(), buffer_read->data(), kSize));

  // Shrink stream1.
  stream1_size = 50;
  EXPECT_EQ(0, WriteData(entry, 1, stream1_size, buffer.get(), 0, true));
  EXPECT_EQ(stream1_size, entry->GetDataSize(1));
  entry->Close();

  // Check that stream0 data has not been modified.
  buffer_read = base::MakeRefCounted<net::IOBufferWithSize>(kSize);
  ASSERT_THAT(OpenEntry(key, &entry), IsOk());
  EXPECT_EQ(kSize, ReadData(entry, 0, 0, buffer_read.get(), kSize));
  EXPECT_EQ(0, memcmp(buffer->data(), buffer_read->data(), kSize));
  entry->Close();
  entry = nullptr;
}

// Test that writing within the range for which the crc has already been
// computed will properly invalidate the computed crc.
TEST_F(DiskCacheEntryTest, SimpleCacheCRCRewrite) {
  // Test sequence:
  // Create, Write (big data), Write (small data in the middle), Close.
  // Open, Read (all), Close.
  SetSimpleCacheMode();
  InitCache();
  disk_cache::Entry* null = nullptr;
  const char key[] = "the first key";

  const int kHalfSize = 200;
  const int kSize = 2 * kHalfSize;
  auto buffer1 = base::MakeRefCounted<net::IOBufferWithSize>(kSize);
  auto buffer2 = base::MakeRefCounted<net::IOBufferWithSize>(kHalfSize);
  CacheTestFillBuffer(buffer1->data(), kSize, false);
  CacheTestFillBuffer(buffer2->data(), kHalfSize, false);

  disk_cache::Entry* entry = nullptr;
  ASSERT_THAT(CreateEntry(key, &entry), IsOk());
  EXPECT_NE(null, entry);
  entry->Close();

  for (int i = 0; i < disk_cache::kSimpleEntryStreamCount; ++i) {
    ASSERT_THAT(OpenEntry(key, &entry), IsOk());
    int offset = 0;
    int buf_len = kSize;

    EXPECT_EQ(buf_len,
              WriteData(entry, i, offset, buffer1.get(), buf_len, false));
    offset = kHalfSize;
    buf_len = kHalfSize;
    EXPECT_EQ(buf_len,
              WriteData(entry, i, offset, buffer2.get(), buf_len, false));
    entry->Close();

    ASSERT_THAT(OpenEntry(key, &entry), IsOk());

    auto buffer1_read1 = base::MakeRefCounted<net::IOBufferWithSize>(kSize);
    EXPECT_EQ(kSize, ReadData(entry, i, 0, buffer1_read1.get(), kSize));
    EXPECT_EQ(0, memcmp(buffer1->data(), buffer1_read1->data(), kHalfSize));
    EXPECT_EQ(
        0,
        memcmp(buffer2->data(), buffer1_read1->data() + kHalfSize, kHalfSize));

    entry->Close();
  }
}

bool DiskCacheEntryTest::SimpleCacheThirdStreamFileExists(const char* key) {
  int third_stream_file_index =
      disk_cache::simple_util::GetFileIndexFromStreamIndex(2);
  base::FilePath third_stream_file_path = cache_path_.AppendASCII(
      disk_cache::simple_util::GetFilenameFromKeyAndFileIndex(
          key, third_stream_file_index));
  return PathExists(third_stream_file_path);
}

void DiskCacheEntryTest::SyncDoomEntry(const char* key) {
  net::TestCompletionCallback callback;
  cache_->DoomEntry(key, net::HIGHEST, callback.callback());
  callback.WaitForResult();
}

void DiskCacheEntryTest::CreateEntryWithHeaderBodyAndSideData(
    const std::string& key,
    int data_size) {
  // Use one buffer for simplicity.
  auto buffer = base::MakeRefCounted<net::IOBufferWithSize>(data_size);
  CacheTestFillBuffer(buffer->data(), data_size, false);

  disk_cache::Entry* entry = nullptr;
  ASSERT_THAT(CreateEntry(key, &entry), IsOk());
  for (int i = 0; i < disk_cache::kSimpleEntryStreamCount; ++i) {
    EXPECT_EQ(data_size, WriteData(entry, i, /* offset */ 0, buffer.get(),
                                   data_size, false));
  }
  entry->Close();
}

void DiskCacheEntryTest::TruncateFileFromEnd(int file_index,
                                             const std::string& key,
                                             int data_size,
                                             int truncate_size) {
  // Remove last eof bytes from cache file.
  ASSERT_GT(data_size, truncate_size);
  const int64_t new_size =
      disk_cache::simple_util::GetFileSizeFromDataSize(key.size(), data_size) -
      truncate_size;
  const base::FilePath entry_path = cache_path_.AppendASCII(
      disk_cache::simple_util::GetFilenameFromKeyAndFileIndex(key, file_index));
  EXPECT_TRUE(TruncatePath(entry_path, new_size));
}

void DiskCacheEntryTest::UseAfterBackendDestruction() {
  disk_cache::Entry* entry = nullptr;
  ASSERT_THAT(CreateEntry("the first key", &entry), IsOk());
  ResetCaches();

  const int kSize = 100;
  auto buffer = base::MakeRefCounted<net::IOBufferWithSize>(kSize);
  CacheTestFillBuffer(buffer->data(), kSize, false);

  // Do some writes and reads, but don't change the result. We're OK
  // with them failing, just not them crashing.
  WriteData(entry, 1, 0, buffer.get(), kSize, false);
  ReadData(entry, 1, 0, buffer.get(), kSize);
  WriteSparseData(entry, 20000, buffer.get(), kSize);

  entry->Close();
}

void DiskCacheEntryTest::CloseSparseAfterBackendDestruction() {
  const int kSize = 100;
  auto buffer = base::MakeRefCounted<net::IOBufferWithSize>(kSize);
  CacheTestFillBuffer(buffer->data(), kSize, false);

  disk_cache::Entry* entry = nullptr;
  ASSERT_THAT(CreateEntry("the first key", &entry), IsOk());
  WriteSparseData(entry, 20000, buffer.get(), kSize);

  ResetCaches();

  // This call shouldn't DCHECK or crash.
  entry->Close();
}

// Check that a newly-created entry with no third-stream writes omits the
// third stream file.
TEST_F(DiskCacheEntryTest, SimpleCacheOmittedThirdStream1) {
  SetSimpleCacheMode();
  InitCache();

  const char key[] = "key";

  disk_cache::Entry* entry;

  // Create entry and close without writing: third stream file should be
  // omitted, since the stream is empty.
  ASSERT_THAT(CreateEntry(key, &entry), IsOk());
  entry->Close();
  EXPECT_FALSE(SimpleCacheThirdStreamFileExists(key));

  SyncDoomEntry(key);
  EXPECT_FALSE(SimpleCacheThirdStreamFileExists(key));
}

// Check that a newly-created entry with only a single zero-offset, zero-length
// write omits the third stream file.
TEST_F(DiskCacheEntryTest, SimpleCacheOmittedThirdStream2) {
  SetSimpleCacheMode();
  InitCache();

  const int kHalfSize = 8;
  const int kSize = kHalfSize * 2;
  const char key[] = "key";
  auto buffer = base::MakeRefCounted<net::IOBufferWithSize>(kSize);
  CacheTestFillBuffer(buffer->data(), kHalfSize, false);

  disk_cache::Entry* entry;

  // Create entry, write empty buffer to third stream, and close: third stream
  // should still be omitted, since the entry ignores writes that don't modify
  // data or change the length.
  ASSERT_THAT(CreateEntry(key, &entry), IsOk());
  EXPECT_EQ(0, WriteData(entry, 2, 0, buffer.get(), 0, true));
  entry->Close();
  EXPECT_FALSE(SimpleCacheThirdStreamFileExists(key));

  SyncDoomEntry(key);
  EXPECT_FALSE(SimpleCacheThirdStreamFileExists(key));
}

// Check that we can read back data written to the third stream.
TEST_F(DiskCacheEntryTest, SimpleCacheOmittedThirdStream3) {
  SetSimpleCacheMode();
  InitCache();

  const int kHalfSize = 8;
  const int kSize = kHalfSize * 2;
  const char key[] = "key";
  auto buffer1 = base::MakeRefCounted<net::IOBufferWithSize>(kSize);
  auto buffer2 = base::MakeRefCounted<net::IOBufferWithSize>(kSize);
  CacheTestFillBuffer(buffer1->data(), kHalfSize, false);

  disk_cache::Entry* entry;

  // Create entry, write data to third stream, and close: third stream should
  // not be omitted, since it contains data.  Re-open entry and ensure there
  // are that many bytes in the third stream.
  ASSERT_THAT(CreateEntry(key, &entry), IsOk());
  EXPECT_EQ(kHalfSize, WriteData(entry, 2, 0, buffer1.get(), kHalfSize, true));
  entry->Close();
  EXPECT_TRUE(SimpleCacheThirdStreamFileExists(key));

  ASSERT_THAT(OpenEntry(key, &entry), IsOk());
  EXPECT_EQ(kHalfSize, ReadData(entry, 2, 0, buffer2.get(), kSize));
  EXPECT_EQ(0, memcmp(buffer1->data(), buffer2->data(), kHalfSize));
  entry->Close();
  EXPECT_TRUE(SimpleCacheThirdStreamFileExists(key));

  SyncDoomEntry(key);
  EXPECT_FALSE(SimpleCacheThirdStreamFileExists(key));
}

// Check that we remove the third stream file upon opening an entry and finding
// the third stream empty.  (This is the upgrade path for entries written
// before the third stream was optional.)
TEST_F(DiskCacheEntryTest, SimpleCacheOmittedThirdStream4) {
  SetSimpleCacheMode();
  InitCache();

  const int kHalfSize = 8;
  const int kSize = kHalfSize * 2;
  const char key[] = "key";
  auto buffer1 = base::MakeRefCounted<net::IOBufferWithSize>(kSize);
  auto buffer2 = base::MakeRefCounted<net::IOBufferWithSize>(kSize);
  CacheTestFillBuffer(buffer1->data(), kHalfSize, false);

  disk_cache::Entry* entry;

  // Create entry, write data to third stream, truncate third stream back to
  // empty, and close: third stream will not initially be omitted, since entry
  // creates the file when the first significant write comes in, and only
  // removes it on open if it is empty.  Reopen, ensure that the file is
  // deleted, and that there's no data in the third stream.
  ASSERT_THAT(CreateEntry(key, &entry), IsOk());
  EXPECT_EQ(kHalfSize, WriteData(entry, 2, 0, buffer1.get(), kHalfSize, true));
  EXPECT_EQ(0, WriteData(entry, 2, 0, buffer1.get(), 0, true));
  entry->Close();
  EXPECT_TRUE(SimpleCacheThirdStreamFileExists(key));

  ASSERT_THAT(OpenEntry(key, &entry), IsOk());
  EXPECT_FALSE(SimpleCacheThirdStreamFileExists(key));
  EXPECT_EQ(0, ReadData(entry, 2, 0, buffer2.get(), kSize));
  entry->Close();
  EXPECT_FALSE(SimpleCacheThirdStreamFileExists(key));

  SyncDoomEntry(key);
  EXPECT_FALSE(SimpleCacheThirdStreamFileExists(key));
}

// Check that we don't accidentally create the third stream file once the entry
// has been doomed.
TEST_F(DiskCacheEntryTest, SimpleCacheOmittedThirdStream5) {
  SetSimpleCacheMode();
  InitCache();

  const int kHalfSize = 8;
  const int kSize = kHalfSize * 2;
  const char key[] = "key";
  auto buffer = base::MakeRefCounted<net::IOBufferWithSize>(kSize);
  CacheTestFillBuffer(buffer->data(), kHalfSize, false);

  disk_cache::Entry* entry;

  // Create entry, doom entry, write data to third stream, and close: third
  // stream should not exist.  (Note: We don't care if the write fails, just
  // that it doesn't cause the file to be created on disk.)
  ASSERT_THAT(CreateEntry(key, &entry), IsOk());
  entry->Doom();
  WriteData(entry, 2, 0, buffer.get(), kHalfSize, true);
  entry->Close();
  EXPECT_FALSE(SimpleCacheThirdStreamFileExists(key));
}

// There could be a race between Doom and an optimistic write.
TEST_F(DiskCacheEntryTest, SimpleCacheDoomOptimisticWritesRace) {
  // Test sequence:
  // Create, first Write, second Write, Close.
  // Open, Close.
  SetSimpleCacheMode();
  InitCache();
  disk_cache::Entry* null = nullptr;
  const char key[] = "the first key";

  const int kSize = 200;
  auto buffer1 = base::MakeRefCounted<net::IOBufferWithSize>(kSize);
  auto buffer2 = base::MakeRefCounted<net::IOBufferWithSize>(kSize);
  CacheTestFillBuffer(buffer1->data(), kSize, false);
  CacheTestFillBuffer(buffer2->data(), kSize, false);

  // The race only happens on stream 1 and stream 2.
  for (int i = 0; i < disk_cache::kSimpleEntryStreamCount; ++i) {
    ASSERT_THAT(DoomAllEntries(), IsOk());
    disk_cache::Entry* entry = nullptr;

    ASSERT_THAT(CreateEntry(key, &entry), IsOk());
    EXPECT_NE(null, entry);
    entry->Close();
    entry = nullptr;

    ASSERT_THAT(DoomAllEntries(), IsOk());
    ASSERT_THAT(CreateEntry(key, &entry), IsOk());
    EXPECT_NE(null, entry);

    int offset = 0;
    int buf_len = kSize;
    // This write should not be optimistic (since create is).
    EXPECT_EQ(buf_len,
              WriteData(entry, i, offset, buffer1.get(), buf_len, false));

    offset = kSize;
    // This write should be optimistic.
    EXPECT_EQ(buf_len,
              WriteData(entry, i, offset, buffer2.get(), buf_len, false));
    entry->Close();

    ASSERT_THAT(OpenEntry(key, &entry), IsOk());
    EXPECT_NE(null, entry);

    entry->Close();
    entry = nullptr;
  }
}

// Tests for a regression in crbug.com/317138 , in which deleting an already
// doomed entry was removing the active entry from the index.
TEST_F(DiskCacheEntryTest, SimpleCachePreserveActiveEntries) {
  SetSimpleCacheMode();
  InitCache();

  disk_cache::Entry* null = nullptr;

  const char key[] = "this is a key";

  disk_cache::Entry* entry1 = nullptr;
  ASSERT_THAT(CreateEntry(key, &entry1), IsOk());
  ScopedEntryPtr entry1_closer(entry1);
  EXPECT_NE(null, entry1);
  entry1->Doom();

  disk_cache::Entry* entry2 = nullptr;
  ASSERT_THAT(CreateEntry(key, &entry2), IsOk());
  ScopedEntryPtr entry2_closer(entry2);
  EXPECT_NE(null, entry2);
  entry2_closer.reset();

  // Closing then reopening entry2 insures that entry2 is serialized, and so
  // it can be opened from files without error.
  entry2 = nullptr;
  ASSERT_THAT(OpenEntry(key, &entry2), IsOk());
  EXPECT_NE(null, entry2);
  entry2_closer.reset(entry2);

  scoped_refptr<disk_cache::SimpleEntryImpl>
      entry1_refptr = static_cast<disk_cache::SimpleEntryImpl*>(entry1);

  // If crbug.com/317138 has regressed, this will remove |entry2| from
  // the backend's |active_entries_| while |entry2| is still alive and its
  // files are still on disk.
  entry1_closer.reset();
  entry1 = nullptr;

  // Close does not have a callback. However, we need to be sure the close is
  // finished before we continue the test. We can take advantage of how the ref
  // counting of a SimpleEntryImpl works to fake out a callback: When the
  // last Close() call is made to an entry, an IO operation is sent to the
  // synchronous entry to close the platform files. This IO operation holds a
  // ref pointer to the entry, which expires when the operation is done. So,
  // we take a refpointer, and watch the SimpleEntry object until it has only
  // one ref; this indicates the IO operation is complete.
  while (!entry1_refptr->HasOneRef()) {
    base::PlatformThread::YieldCurrentThread();
    base::RunLoop().RunUntilIdle();
  }
  entry1_refptr = nullptr;

  // In the bug case, this new entry ends up being a duplicate object pointing
  // at the same underlying files.
  disk_cache::Entry* entry3 = nullptr;
  EXPECT_THAT(OpenEntry(key, &entry3), IsOk());
  ScopedEntryPtr entry3_closer(entry3);
  EXPECT_NE(null, entry3);

  // The test passes if these two dooms do not crash.
  entry2->Doom();
  entry3->Doom();
}

TEST_F(DiskCacheEntryTest, SimpleCacheBasicSparseIO) {
  SetSimpleCacheMode();
  InitCache();
  BasicSparseIO();
}

TEST_F(DiskCacheEntryTest, SimpleCacheHugeSparseIO) {
  SetSimpleCacheMode();
  InitCache();
  HugeSparseIO();
}

TEST_F(DiskCacheEntryTest, SimpleCacheGetAvailableRange) {
  SetSimpleCacheMode();
  InitCache();
  GetAvailableRangeTest();
}

TEST_F(DiskCacheEntryTest, SimpleCacheUpdateSparseEntry) {
  SetSimpleCacheMode();
  InitCache();
  UpdateSparseEntry();
}

TEST_F(DiskCacheEntryTest, SimpleCacheDoomSparseEntry) {
  SetSimpleCacheMode();
  InitCache();
  DoomSparseEntry();
}

TEST_F(DiskCacheEntryTest, SimpleCachePartialSparseEntry) {
  SetSimpleCacheMode();
  InitCache();
  PartialSparseEntry();
}

TEST_F(DiskCacheEntryTest, SimpleCacheTruncateLargeSparseFile) {
  const int kSize = 1024;

  SetSimpleCacheMode();
  // An entry is allowed sparse data 1/10 the size of the cache, so this size
  // allows for one |kSize|-sized range plus overhead, but not two ranges.
  SetMaxSize(kSize * 15);
  InitCache();

  const char key[] = "key";
  disk_cache::Entry* null = nullptr;
  disk_cache::Entry* entry;
  ASSERT_THAT(CreateEntry(key, &entry), IsOk());
  EXPECT_NE(null, entry);

  auto buffer = base::MakeRefCounted<net::IOBufferWithSize>(kSize);
  CacheTestFillBuffer(buffer->data(), kSize, false);
  net::TestCompletionCallback callback;
  int ret;

  // Verify initial conditions.
  ret = entry->ReadSparseData(0, buffer.get(), kSize, callback.callback());
  EXPECT_EQ(0, callback.GetResult(ret));

  ret = entry->ReadSparseData(kSize, buffer.get(), kSize, callback.callback());
  EXPECT_EQ(0, callback.GetResult(ret));

  // Write a range and make sure it reads back.
  ret = entry->WriteSparseData(0, buffer.get(), kSize, callback.callback());
  EXPECT_EQ(kSize, callback.GetResult(ret));

  ret = entry->ReadSparseData(0, buffer.get(), kSize, callback.callback());
  EXPECT_EQ(kSize, callback.GetResult(ret));

  // Write another range and make sure it reads back.
  ret = entry->WriteSparseData(kSize, buffer.get(), kSize, callback.callback());
  EXPECT_EQ(kSize, callback.GetResult(ret));

  ret = entry->ReadSparseData(kSize, buffer.get(), kSize, callback.callback());
  EXPECT_EQ(kSize, callback.GetResult(ret));

  // Make sure the first range was removed when the second was written.
  ret = entry->ReadSparseData(0, buffer.get(), kSize, callback.callback());
  EXPECT_EQ(0, callback.GetResult(ret));

  // Close and reopen the entry and make sure the first entry is still absent
  // and the second entry is still present.
  entry->Close();
  ASSERT_THAT(OpenEntry(key, &entry), IsOk());

  ret = entry->ReadSparseData(0, buffer.get(), kSize, callback.callback());
  EXPECT_EQ(0, callback.GetResult(ret));

  ret = entry->ReadSparseData(kSize, buffer.get(), kSize, callback.callback());
  EXPECT_EQ(kSize, callback.GetResult(ret));

  entry->Close();
}

TEST_F(DiskCacheEntryTest, SimpleCacheNoBodyEOF) {
  SetSimpleCacheMode();
  InitCache();

  const std::string key("the first key");
  const int kSize = 1024;
  CreateEntryWithHeaderBodyAndSideData(key, kSize);

  disk_cache::Entry* entry = nullptr;
  ASSERT_THAT(OpenEntry(key, &entry), IsOk());
  entry->Close();

  TruncateFileFromEnd(0 /*header and body file index*/, key, kSize,
                      static_cast<int>(sizeof(disk_cache::SimpleFileEOF)));
  EXPECT_THAT(OpenEntry(key, &entry), IsError(net::ERR_FAILED));
}

TEST_F(DiskCacheEntryTest, SimpleCacheNoSideDataEOF) {
  SetSimpleCacheMode();
  InitCache();

  const char key[] = "the first key";
  const int kSize = 1024;
  CreateEntryWithHeaderBodyAndSideData(key, kSize);

  disk_cache::Entry* entry = nullptr;
  ASSERT_THAT(OpenEntry(key, &entry), IsOk());
  entry->Close();

  TruncateFileFromEnd(1 /*side data file_index*/, key, kSize,
                      static_cast<int>(sizeof(disk_cache::SimpleFileEOF)));
  EXPECT_THAT(OpenEntry(key, &entry), IsOk());
  // The corrupted stream should have been deleted.
  EXPECT_FALSE(SimpleCacheThirdStreamFileExists(key));
  // _0 should still exist.
  base::FilePath path_0 = cache_path_.AppendASCII(
      disk_cache::simple_util::GetFilenameFromKeyAndFileIndex(key, 0));
  EXPECT_TRUE(base::PathExists(path_0));

  auto check_stream_data = base::MakeRefCounted<net::IOBufferWithSize>(kSize);
  EXPECT_EQ(kSize, ReadData(entry, 0, 0, check_stream_data.get(), kSize));
  EXPECT_EQ(kSize, ReadData(entry, 1, 0, check_stream_data.get(), kSize));
  EXPECT_EQ(0, entry->GetDataSize(2));
  entry->Close();
}

TEST_F(DiskCacheEntryTest, SimpleCacheReadWithoutKeySHA256) {
  // This test runs as APP_CACHE to make operations more synchronous.
  SetCacheType(net::APP_CACHE);
  SetSimpleCacheMode();
  InitCache();
  disk_cache::Entry* entry;
  std::string key("a key");
  ASSERT_THAT(CreateEntry(key, &entry), IsOk());

  const std::string stream_0_data = "data for stream zero";
  auto stream_0_iobuffer =
      base::MakeRefCounted<net::StringIOBuffer>(stream_0_data);
  EXPECT_EQ(static_cast<int>(stream_0_data.size()),
            WriteData(entry, 0, 0, stream_0_iobuffer.get(),
                      stream_0_data.size(), false));
  const std::string stream_1_data = "FOR STREAM ONE, QUITE DIFFERENT THINGS";
  auto stream_1_iobuffer =
      base::MakeRefCounted<net::StringIOBuffer>(stream_1_data);
  EXPECT_EQ(static_cast<int>(stream_1_data.size()),
            WriteData(entry, 1, 0, stream_1_iobuffer.get(),
                      stream_1_data.size(), false));
  entry->Close();

  base::RunLoop().RunUntilIdle();
  disk_cache::FlushCacheThreadForTesting();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(
      disk_cache::simple_util::RemoveKeySHA256FromEntry(key, cache_path_));
  ASSERT_THAT(OpenEntry(key, &entry), IsOk());
  ScopedEntryPtr entry_closer(entry);

  EXPECT_EQ(static_cast<int>(stream_0_data.size()), entry->GetDataSize(0));
  auto check_stream_0_data =
      base::MakeRefCounted<net::IOBufferWithSize>(stream_0_data.size());
  EXPECT_EQ(
      static_cast<int>(stream_0_data.size()),
      ReadData(entry, 0, 0, check_stream_0_data.get(), stream_0_data.size()));
  EXPECT_EQ(0, stream_0_data.compare(0, std::string::npos,
                                     check_stream_0_data->data(),
                                     stream_0_data.size()));

  EXPECT_EQ(static_cast<int>(stream_1_data.size()), entry->GetDataSize(1));
  auto check_stream_1_data =
      base::MakeRefCounted<net::IOBufferWithSize>(stream_1_data.size());
  EXPECT_EQ(
      static_cast<int>(stream_1_data.size()),
      ReadData(entry, 1, 0, check_stream_1_data.get(), stream_1_data.size()));
  EXPECT_EQ(0, stream_1_data.compare(0, std::string::npos,
                                     check_stream_1_data->data(),
                                     stream_1_data.size()));
}

TEST_F(DiskCacheEntryTest, SimpleCacheDoubleOpenWithoutKeySHA256) {
  // This test runs as APP_CACHE to make operations more synchronous.
  SetCacheType(net::APP_CACHE);
  SetSimpleCacheMode();
  InitCache();
  disk_cache::Entry* entry;
  std::string key("a key");
  ASSERT_THAT(CreateEntry(key, &entry), IsOk());
  entry->Close();

  base::RunLoop().RunUntilIdle();
  disk_cache::FlushCacheThreadForTesting();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(
      disk_cache::simple_util::RemoveKeySHA256FromEntry(key, cache_path_));
  ASSERT_THAT(OpenEntry(key, &entry), IsOk());
  entry->Close();

  base::RunLoop().RunUntilIdle();
  disk_cache::FlushCacheThreadForTesting();
  base::RunLoop().RunUntilIdle();

  ASSERT_THAT(OpenEntry(key, &entry), IsOk());
  entry->Close();
}

TEST_F(DiskCacheEntryTest, SimpleCacheReadCorruptKeySHA256) {
  // This test runs as APP_CACHE to make operations more synchronous.
  SetCacheType(net::APP_CACHE);
  SetSimpleCacheMode();
  InitCache();
  disk_cache::Entry* entry;
  std::string key("a key");
  ASSERT_THAT(CreateEntry(key, &entry), IsOk());
  entry->Close();

  base::RunLoop().RunUntilIdle();
  disk_cache::FlushCacheThreadForTesting();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(
      disk_cache::simple_util::CorruptKeySHA256FromEntry(key, cache_path_));
  EXPECT_NE(net::OK, OpenEntry(key, &entry));
}

TEST_F(DiskCacheEntryTest, SimpleCacheReadCorruptLength) {
  SetCacheType(net::APP_CACHE);
  SetSimpleCacheMode();
  InitCache();
  disk_cache::Entry* entry;
  std::string key("a key");
  ASSERT_EQ(net::OK, CreateEntry(key, &entry));
  entry->Close();

  base::RunLoop().RunUntilIdle();
  disk_cache::FlushCacheThreadForTesting();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(
      disk_cache::simple_util::CorruptStream0LengthFromEntry(key, cache_path_));
  EXPECT_NE(net::OK, OpenEntry(key, &entry));
}

TEST_F(DiskCacheEntryTest, SimpleCacheCreateRecoverFromRmdir) {
  // This test runs as APP_CACHE to make operations more synchronous.
  // (in particular we want to see if create succeeded or not, so we don't
  //  want an optimistic one).
  SetCacheType(net::APP_CACHE);
  SetSimpleCacheMode();
  InitCache();

  // Pretend someone deleted the cache dir. This shouldn't be too scary in
  // the test since cache_path_ is set as:
  //   CHECK(temp_dir_.CreateUniqueTempDir());
  //   cache_path_ = temp_dir_.GetPath().AppendASCII("cache");
  disk_cache::DeleteCache(cache_path_,
                          true /* delete the dir, what we really want*/);

  disk_cache::Entry* entry;
  std::string key("a key");
  ASSERT_THAT(CreateEntry(key, &entry), IsOk());
  entry->Close();
}

TEST_F(DiskCacheEntryTest, SimpleCacheSparseErrorHandling) {
  // If there is corruption in sparse file, we should delete all the files
  // before returning the failure. Further additional sparse operations in
  // failure state should fail gracefully.
  SetSimpleCacheMode();
  InitCache();

  std::string key("a key");

  disk_cache::SimpleFileTracker::EntryFileKey num_key(
      disk_cache::simple_util::GetEntryHashKey(key));
  base::FilePath path_0 = cache_path_.AppendASCII(
      disk_cache::simple_util::GetFilenameFromEntryFileKeyAndFileIndex(num_key,
                                                                       0));
  base::FilePath path_s = cache_path_.AppendASCII(
      disk_cache::simple_util::GetSparseFilenameFromEntryFileKey(num_key));

  disk_cache::Entry* entry = nullptr;
  ASSERT_THAT(CreateEntry(key, &entry), IsOk());

  const int kSize = 1024;
  auto buffer = base::MakeRefCounted<net::IOBufferWithSize>(kSize);
  CacheTestFillBuffer(buffer->data(), kSize, false);

  EXPECT_EQ(kSize, WriteSparseData(entry, 0, buffer.get(), kSize));
  entry->Close();

  disk_cache::FlushCacheThreadForTesting();
  EXPECT_TRUE(base::PathExists(path_0));
  EXPECT_TRUE(base::PathExists(path_s));

  // Now corrupt the _s file in a way that makes it look OK on open, but not on
  // read.
  base::File file_s(path_s, base::File::FLAG_OPEN | base::File::FLAG_READ |
                                base::File::FLAG_WRITE);
  ASSERT_TRUE(file_s.IsValid());
  file_s.SetLength(sizeof(disk_cache::SimpleFileHeader) +
                   sizeof(disk_cache::SimpleFileSparseRangeHeader) +
                   key.size());
  file_s.Close();

  // Re-open, it should still be fine.
  ASSERT_THAT(OpenEntry(key, &entry), IsOk());

  // Read should fail though.
  EXPECT_EQ(net::ERR_CACHE_READ_FAILURE,
            ReadSparseData(entry, 0, buffer.get(), kSize));

  // At the point read returns to us, the files should already been gone.
  EXPECT_FALSE(base::PathExists(path_0));
  EXPECT_FALSE(base::PathExists(path_s));

  // Re-trying should still fail. Not DCHECK-fail.
  EXPECT_EQ(net::ERR_FAILED, ReadSparseData(entry, 0, buffer.get(), kSize));

  // Similarly for other ops.
  EXPECT_EQ(net::ERR_FAILED, WriteSparseData(entry, 0, buffer.get(), kSize));
  net::TestCompletionCallback cb;

  TestRangeResultCompletionCallback range_cb;
  RangeResult result = range_cb.GetResult(
      entry->GetAvailableRange(0, 1024, range_cb.callback()));
  EXPECT_EQ(net::ERR_FAILED, result.net_error);

  entry->Close();
  disk_cache::FlushCacheThreadForTesting();

  // Closing shouldn't resurrect files, either.
  EXPECT_FALSE(base::PathExists(path_0));
  EXPECT_FALSE(base::PathExists(path_s));
}

TEST_F(DiskCacheEntryTest, SimpleCacheCreateCollision) {
  // These two keys collide; this test is that we properly handled creation
  // of both.
  const char kCollKey1[] =
      "\xfb\x4e\x9c\x1d\x66\x71\xf7\x54\xa3\x11\xa0\x7e\x16\xa5\x68\xf6";
  const char kCollKey2[] =
      "\xbc\x60\x64\x92\xbc\xa0\x5c\x15\x17\x93\x29\x2d\xe4\x21\xbd\x03";

  const int kSize = 256;
  auto buffer1 = base::MakeRefCounted<net::IOBufferWithSize>(kSize);
  auto buffer2 = base::MakeRefCounted<net::IOBufferWithSize>(kSize);
  auto read_buffer = base::MakeRefCounted<net::IOBufferWithSize>(kSize);
  CacheTestFillBuffer(buffer1->data(), kSize, false);
  CacheTestFillBuffer(buffer2->data(), kSize, false);

  SetSimpleCacheMode();
  InitCache();

  disk_cache::Entry* entry1;
  ASSERT_THAT(CreateEntry(kCollKey1, &entry1), IsOk());

  disk_cache::Entry* entry2;
  ASSERT_THAT(CreateEntry(kCollKey2, &entry2), IsOk());

  // Make sure that entry was actually created and we didn't just succeed
  // optimistically. (Oddly I can't seem to hit the sequence of events required
  // for the bug that used to be here if I just set this to APP_CACHE).
  EXPECT_EQ(kSize, WriteData(entry2, 0, 0, buffer2.get(), kSize, false));

  // entry1 is still usable, though, and distinct (we just won't be able to
  // re-open it).
  EXPECT_EQ(kSize, WriteData(entry1, 0, 0, buffer1.get(), kSize, false));
  EXPECT_EQ(kSize, ReadData(entry1, 0, 0, read_buffer.get(), kSize));
  EXPECT_EQ(0, memcmp(buffer1->data(), read_buffer->data(), kSize));

  EXPECT_EQ(kSize, ReadData(entry2, 0, 0, read_buffer.get(), kSize));
  EXPECT_EQ(0, memcmp(buffer2->data(), read_buffer->data(), kSize));

  entry1->Close();
  entry2->Close();
}

TEST_F(DiskCacheEntryTest, SimpleCacheConvertToSparseStream2LeftOver) {
  // Testcase for what happens when we have a sparse stream and a left over
  // empty stream 2 file.
  const int kSize = 10;
  auto buffer = base::MakeRefCounted<net::IOBufferWithSize>(kSize);
  CacheTestFillBuffer(buffer->data(), kSize, false);

  SetSimpleCacheMode();
  InitCache();
  disk_cache::Entry* entry;
  std::string key("a key");
  ASSERT_THAT(CreateEntry(key, &entry), IsOk());
  // Create an empty stream 2. To do that, we first make a non-empty one, then
  // truncate it (since otherwise the write would just get ignored).
  EXPECT_EQ(kSize, WriteData(entry, /* stream = */ 2, /* offset = */ 0,
                             buffer.get(), kSize, false));
  EXPECT_EQ(0, WriteData(entry, /* stream = */ 2, /* offset = */ 0,
                         buffer.get(), 0, true));

  EXPECT_EQ(kSize, WriteSparseData(entry, 5, buffer.get(), kSize));
  entry->Close();

  // Reopen, and try to get the sparse data back.
  ASSERT_THAT(OpenEntry(key, &entry), IsOk());
  auto buffer2 = base::MakeRefCounted<net::IOBufferWithSize>(kSize);
  EXPECT_EQ(kSize, ReadSparseData(entry, 5, buffer2.get(), kSize));
  EXPECT_EQ(0, memcmp(buffer->data(), buffer2->data(), kSize));
  entry->Close();
}

TEST_F(DiskCacheEntryTest, SimpleCacheLazyStream2CreateFailure) {
  // Testcase for what happens when lazy-creation of stream 2 fails.
  const int kSize = 10;
  auto buffer = base::MakeRefCounted<net::IOBufferWithSize>(kSize);
  CacheTestFillBuffer(buffer->data(), kSize, false);

  // Synchronous ops, for ease of disk state;
  SetCacheType(net::APP_CACHE);
  SetSimpleCacheMode();
  InitCache();

  const char kKey[] = "a key";
  disk_cache::Entry* entry = nullptr;
  ASSERT_THAT(CreateEntry(kKey, &entry), IsOk());

  // Create _1 file for stream 2; this should inject a failure when the cache
  // tries to create it itself.
  base::FilePath entry_file1_path = cache_path_.AppendASCII(
      disk_cache::simple_util::GetFilenameFromKeyAndFileIndex(kKey, 1));
  base::File entry_file1(entry_file1_path,
                         base::File::FLAG_WRITE | base::File::FLAG_CREATE);
  ASSERT_TRUE(entry_file1.IsValid());
  entry_file1.Close();

  EXPECT_EQ(net::ERR_CACHE_WRITE_FAILURE,
            WriteData(entry, /* index = */ 2, /* offset = */ 0, buffer.get(),
                      kSize, /* truncate = */ false));
  entry->Close();
}

TEST_F(DiskCacheEntryTest, SimpleCacheChecksumpScrewUp) {
  // Test for a bug that occurred during development of  movement of CRC
  // computation off I/O thread.
  const int kSize = 10;
  auto buffer = base::MakeRefCounted<net::IOBufferWithSize>(kSize);
  CacheTestFillBuffer(buffer->data(), kSize, false);

  const int kDoubleSize = kSize * 2;
  auto big_buffer = base::MakeRefCounted<net::IOBufferWithSize>(kDoubleSize);
  CacheTestFillBuffer(big_buffer->data(), kDoubleSize, false);

  SetSimpleCacheMode();
  InitCache();

  const char kKey[] = "a key";
  disk_cache::Entry* entry = nullptr;
  ASSERT_THAT(CreateEntry(kKey, &entry), IsOk());

  // Write out big_buffer for the double range. Checksum will be set to this.
  ASSERT_EQ(kDoubleSize,
            WriteData(entry, 1, 0, big_buffer.get(), kDoubleSize, false));

  // Reset remembered position to 0 by writing at an earlier non-zero offset.
  ASSERT_EQ(1, WriteData(entry, /* stream = */ 1, /* offset = */ 1,
                         big_buffer.get(), /* len = */ 1, false));

  // Now write out the half-range twice. An intermediate revision would
  // incorrectly compute checksum as if payload was buffer followed by buffer
  // rather than buffer followed by end of big_buffer.
  ASSERT_EQ(kSize, WriteData(entry, 1, 0, buffer.get(), kSize, false));
  ASSERT_EQ(kSize, WriteData(entry, 1, 0, buffer.get(), kSize, false));
  entry->Close();

  ASSERT_THAT(OpenEntry(kKey, &entry), IsOk());
  auto buffer2 = base::MakeRefCounted<net::IOBufferWithSize>(kSize);
  EXPECT_EQ(kSize, ReadData(entry, 1, 0, buffer2.get(), kSize));
  EXPECT_EQ(0, memcmp(buffer->data(), buffer2->data(), kSize));
  EXPECT_EQ(kSize, ReadData(entry, 1, kSize, buffer2.get(), kSize));
  EXPECT_EQ(0, memcmp(big_buffer->data() + kSize, buffer2->data(), kSize));
  entry->Close();
}

TEST_F(DiskCacheEntryTest, SimpleUseAfterBackendDestruction) {
  SetSimpleCacheMode();
  InitCache();
  UseAfterBackendDestruction();
}

TEST_F(DiskCacheEntryTest, MemoryOnlyUseAfterBackendDestruction) {
  // https://crbug.com/741620
  SetMemoryOnlyMode();
  InitCache();
  UseAfterBackendDestruction();
}

TEST_F(DiskCacheEntryTest, SimpleCloseSparseAfterBackendDestruction) {
  SetSimpleCacheMode();
  InitCache();
  CloseSparseAfterBackendDestruction();
}

TEST_F(DiskCacheEntryTest, MemoryOnlyCloseSparseAfterBackendDestruction) {
  // https://crbug.com/946434
  SetMemoryOnlyMode();
  InitCache();
  CloseSparseAfterBackendDestruction();
}

void DiskCacheEntryTest::LastUsedTimePersists() {
  // Make sure that SetLastUsedTimeForTest persists. When used with SimpleCache,
  // this also checks that Entry::GetLastUsed is based on information in index,
  // when available, not atime on disk, which can be inaccurate.
  const char kKey[] = "a key";
  InitCache();

  disk_cache::Entry* entry1 = nullptr;
  ASSERT_THAT(CreateEntry(kKey, &entry1), IsOk());
  ASSERT_TRUE(nullptr != entry1);
  base::Time modified_last_used = entry1->GetLastUsed() - base::Minutes(5);
  entry1->SetLastUsedTimeForTest(modified_last_used);
  entry1->Close();

  disk_cache::Entry* entry2 = nullptr;
  ASSERT_THAT(OpenEntry(kKey, &entry2), IsOk());
  ASSERT_TRUE(nullptr != entry2);

  base::TimeDelta diff = modified_last_used - entry2->GetLastUsed();
  EXPECT_LT(diff, base::Seconds(2));
  EXPECT_GT(diff, -base::Seconds(2));
  entry2->Close();
}

TEST_F(DiskCacheEntryTest, LastUsedTimePersists) {
  LastUsedTimePersists();
}

TEST_F(DiskCacheEntryTest, SimpleLastUsedTimePersists) {
  SetSimpleCacheMode();
  LastUsedTimePersists();
}

TEST_F(DiskCacheEntryTest, MemoryOnlyLastUsedTimePersists) {
  SetMemoryOnlyMode();
  LastUsedTimePersists();
}

void DiskCacheEntryTest::TruncateBackwards() {
  const char kKey[] = "a key";

  disk_cache::Entry* entry = nullptr;
  ASSERT_THAT(CreateEntry(kKey, &entry), IsOk());
  ASSERT_TRUE(entry != nullptr);

  const int kBigSize = 40 * 1024;
  const int kSmallSize = 9727;

  auto buffer = base::MakeRefCounted<net::IOBufferWithSize>(kBigSize);
  CacheTestFillBuffer(buffer->data(), kBigSize, false);
  auto read_buf = base::MakeRefCounted<net::IOBufferWithSize>(kBigSize);

  ASSERT_EQ(kSmallSize, WriteData(entry, /* index = */ 0,
                                  /* offset = */ kBigSize, buffer.get(),
                                  /* size = */ kSmallSize,
                                  /* truncate = */ false));
  memset(read_buf->data(), 0, kBigSize);
  ASSERT_EQ(kSmallSize, ReadData(entry, /* index = */ 0,
                                 /* offset = */ kBigSize, read_buf.get(),
                                 /* size = */ kSmallSize));
  EXPECT_EQ(0, memcmp(read_buf->data(), buffer->data(), kSmallSize));

  // A partly overlapping truncate before the previous write.
  ASSERT_EQ(kBigSize,
            WriteData(entry, /* index = */ 0,
                      /* offset = */ 3, buffer.get(), /* size = */ kBigSize,
                      /* truncate = */ true));
  memset(read_buf->data(), 0, kBigSize);
  ASSERT_EQ(kBigSize,
            ReadData(entry, /* index = */ 0,
                     /* offset = */ 3, read_buf.get(), /* size = */ kBigSize));
  EXPECT_EQ(0, memcmp(read_buf->data(), buffer->data(), kBigSize));
  EXPECT_EQ(kBigSize + 3, entry->GetDataSize(0));
  entry->Close();
}

TEST_F(DiskCacheEntryTest, TruncateBackwards) {
  // https://crbug.com/946539/
  InitCache();
  TruncateBackwards();
}

TEST_F(DiskCacheEntryTest, SimpleTruncateBackwards) {
  SetSimpleCacheMode();
  InitCache();
  TruncateBackwards();
}

TEST_F(DiskCacheEntryTest, MemoryOnlyTruncateBackwards) {
  SetMemoryOnlyMode();
  InitCache();
  TruncateBackwards();
}

void DiskCacheEntryTest::ZeroWriteBackwards() {
  const char kKey[] = "a key";

  disk_cache::Entry* entry = nullptr;
  ASSERT_THAT(CreateEntry(kKey, &entry), IsOk());
  ASSERT_TRUE(entry != nullptr);

  const int kSize = 1024;
  auto buffer = base::MakeRefCounted<net::IOBufferWithSize>(kSize);
  CacheTestFillBuffer(buffer->data(), kSize, false);

  // Offset here needs to be > blockfile's kMaxBlockSize to hit
  // https://crbug.com/946538, as writes close to beginning are handled
  // specially.
  EXPECT_EQ(0, WriteData(entry, /* index = */ 0,
                         /* offset = */ 17000, buffer.get(),
                         /* size = */ 0, /* truncate = */ true));

  EXPECT_EQ(0, WriteData(entry, /* index = */ 0,
                         /* offset = */ 0, buffer.get(),
                         /* size = */ 0, /* truncate = */ false));

  EXPECT_EQ(kSize, ReadData(entry, /* index = */ 0,
                            /* offset = */ 0, buffer.get(),
                            /* size = */ kSize));
  for (int i = 0; i < kSize; ++i) {
    EXPECT_EQ(0, buffer->data()[i]) << i;
  }
  entry->Close();
}

TEST_F(DiskCacheEntryTest, ZeroWriteBackwards) {
  // https://crbug.com/946538/
  InitCache();
  ZeroWriteBackwards();
}

TEST_F(DiskCacheEntryTest, SimpleZeroWriteBackwards) {
  SetSimpleCacheMode();
  InitCache();
  ZeroWriteBackwards();
}

TEST_F(DiskCacheEntryTest, MemoryOnlyZeroWriteBackwards) {
  SetMemoryOnlyMode();
  InitCache();
  ZeroWriteBackwards();
}

void DiskCacheEntryTest::SparseOffset64Bit() {
  // Offsets to sparse ops are 64-bit, make sure we keep track of all of them.
  // (Or, as at least in case of blockfile, fail things cleanly, as it has a
  //  cap on max offset that's much lower).
  bool blockfile = !memory_only_ && !simple_cache_mode_;
  InitCache();

  const char kKey[] = "a key";

  disk_cache::Entry* entry = nullptr;
  ASSERT_THAT(CreateEntry(kKey, &entry), IsOk());
  ASSERT_TRUE(entry != nullptr);

  const int kSize = 1024;
  // One bit set very high, so intermediate truncations to 32-bit would drop it
  // even if they happen after a bunch of shifting right.
  const int64_t kOffset = (1ll << 61);

  auto buffer = base::MakeRefCounted<net::IOBufferWithSize>(kSize);
  CacheTestFillBuffer(buffer->data(), kSize, false);

  EXPECT_EQ(blockfile ? net::ERR_CACHE_OPERATION_NOT_SUPPORTED : kSize,
            WriteSparseData(entry, kOffset, buffer.get(), kSize));

  int64_t start_out = -1;
  EXPECT_EQ(0, GetAvailableRange(entry, /* offset = */ 0, kSize, &start_out));

  start_out = -1;
  EXPECT_EQ(blockfile ? 0 : kSize,
            GetAvailableRange(entry, kOffset, kSize, &start_out));
  EXPECT_EQ(kOffset, start_out);

  entry->Close();
}

TEST_F(DiskCacheEntryTest, SparseOffset64Bit) {
  InitCache();
  SparseOffset64Bit();
}

TEST_F(DiskCacheEntryTest, SimpleSparseOffset64Bit) {
  SetSimpleCacheMode();
  InitCache();
  SparseOffset64Bit();
}

TEST_F(DiskCacheEntryTest, MemoryOnlySparseOffset64Bit) {
  // https://crbug.com/946436
  SetMemoryOnlyMode();
  InitCache();
  SparseOffset64Bit();
}

TEST_F(DiskCacheEntryTest, SimpleCacheCloseResurrection) {
  const int kSize = 10;
  auto buffer = base::MakeRefCounted<net::IOBufferWithSize>(kSize);
  CacheTestFillBuffer(buffer->data(), kSize, false);

  const char kKey[] = "key";
  SetSimpleCacheMode();
  InitCache();

  disk_cache::Entry* entry = nullptr;
  ASSERT_THAT(CreateEntry(kKey, &entry), IsOk());
  ASSERT_TRUE(entry != nullptr);

  // Let optimistic create finish.
  base::RunLoop().RunUntilIdle();
  disk_cache::FlushCacheThreadForTesting();
  base::RunLoop().RunUntilIdle();

  int rv = entry->WriteData(1, 0, buffer.get(), kSize,
                            net::CompletionOnceCallback(), false);

  // Write should be optimistic.
  ASSERT_EQ(kSize, rv);

  // Since the write is still pending, the open will get queued...
  TestEntryResultCompletionCallback cb_open;
  EntryResult result2 =
      cache_->OpenEntry(kKey, net::HIGHEST, cb_open.callback());
  EXPECT_EQ(net::ERR_IO_PENDING, result2.net_error());

  // ... as the open is queued, this Close will temporarily reduce the number
  // of external references to 0.  This should not break things.
  entry->Close();

  // Wait till open finishes.
  result2 = cb_open.GetResult(std::move(result2));
  ASSERT_EQ(net::OK, result2.net_error());
  disk_cache::Entry* entry2 = result2.ReleaseEntry();
  ASSERT_TRUE(entry2 != nullptr);

  // Get first close a chance to finish.
  base::RunLoop().RunUntilIdle();
  disk_cache::FlushCacheThreadForTesting();
  base::RunLoop().RunUntilIdle();

  // Make sure |entry2| is still usable.
  auto buffer2 = base::MakeRefCounted<net::IOBufferWithSize>(kSize);
  memset(buffer2->data(), 0, kSize);
  EXPECT_EQ(kSize, ReadData(entry2, 1, 0, buffer2.get(), kSize));
  EXPECT_EQ(0, memcmp(buffer->data(), buffer2->data(), kSize));
  entry2->Close();
}

TEST_F(DiskCacheEntryTest, BlockFileSparsePendingAfterDtor) {
  // Test of behavior of ~EntryImpl for sparse entry that runs after backend
  // destruction.
  //
  // Hand-creating the backend for realistic shutdown behavior.
  CleanupCacheDir();
  CreateBackend(disk_cache::kNone);

  disk_cache::Entry* entry = nullptr;
  ASSERT_THAT(CreateEntry("key", &entry), IsOk());
  ASSERT_TRUE(entry != nullptr);

  const int kSize = 61184;

  auto buf = base::MakeRefCounted<net::IOBufferWithSize>(kSize);
  CacheTestFillBuffer(buf->data(), kSize, false);

  // The write pattern here avoids the second write being handled by the
  // buffering layer, making SparseControl have to deal with its asynchrony.
  EXPECT_EQ(1, WriteSparseData(entry, 65535, buf.get(), 1));
  EXPECT_EQ(net::ERR_IO_PENDING,
            entry->WriteSparseData(2560, buf.get(), kSize, base::DoNothing()));
  entry->Close();
  ResetCaches();

  // Create a new instance as a way of flushing the thread.
  InitCache();
  FlushQueueForTest();
}

class DiskCacheSimplePrefetchTest : public DiskCacheEntryTest {
 public:
  DiskCacheSimplePrefetchTest() = default;

  enum { kEntrySize = 1024 };

  void SetUp() override {
    payload_ = base::MakeRefCounted<net::IOBufferWithSize>(kEntrySize);
    CacheTestFillBuffer(payload_->data(), kEntrySize, false);
    DiskCacheEntryTest::SetUp();
  }

  void SetupFullAndTrailerPrefetch(int full_size,
                                   int trailer_speculative_size) {
    std::map<std::string, std::string> params;
    params[disk_cache::kSimpleCacheFullPrefetchBytesParam] =
        base::NumberToString(full_size);
    params[disk_cache::kSimpleCacheTrailerPrefetchSpeculativeBytesParam] =
        base::NumberToString(trailer_speculative_size);
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        disk_cache::kSimpleCachePrefetchExperiment, params);
  }

  void SetupFullPrefetch(int size) { SetupFullAndTrailerPrefetch(size, 0); }

  void InitCacheAndCreateEntry(const std::string& key) {
    SetSimpleCacheMode();
    SetCacheType(SimpleCacheType());
    InitCache();

    disk_cache::Entry* entry;
    ASSERT_EQ(net::OK, CreateEntry(key, &entry));
    // Use stream 1 since that's what new prefetch stuff is about.
    ASSERT_EQ(kEntrySize,
              WriteData(entry, 1, 0, payload_.get(), kEntrySize, false));
    entry->Close();
  }

  virtual net::CacheType SimpleCacheType() const { return net::DISK_CACHE; }

  void InitCacheAndCreateEntryWithNoCrc(const std::string& key) {
    const int kHalfSize = kEntrySize / 2;
    const int kRemSize = kEntrySize - kHalfSize;

    SetSimpleCacheMode();
    InitCache();

    disk_cache::Entry* entry;
    ASSERT_EQ(net::OK, CreateEntry(key, &entry));
    // Use stream 1 since that's what new prefetch stuff is about.
    ASSERT_EQ(kEntrySize,
              WriteData(entry, 1, 0, payload_.get(), kEntrySize, false));

    // Overwrite later part of the buffer, since we can't keep track of
    // the checksum in that case.  Do it with identical contents, though,
    // so that the only difference between here and InitCacheAndCreateEntry()
    // would be whether the result has a checkum or not.
    auto second_half = base::MakeRefCounted<net::IOBufferWithSize>(kRemSize);
    memcpy(second_half->data(), payload_->data() + kHalfSize, kRemSize);
    ASSERT_EQ(kRemSize, WriteData(entry, 1, kHalfSize, second_half.get(),
                                  kRemSize, false));
    entry->Close();
  }

  void TryRead(const std::string& key, bool expect_preread_stream1) {
    disk_cache::Entry* entry = nullptr;
    ASSERT_THAT(OpenEntry(key, &entry), IsOk());
    auto read_buf = base::MakeRefCounted<net::IOBufferWithSize>(kEntrySize);
    net::TestCompletionCallback cb;
    int rv = entry->ReadData(1, 0, read_buf.get(), kEntrySize, cb.callback());

    // if preload happened, sync reply is expected.
    if (expect_preread_stream1)
      EXPECT_EQ(kEntrySize, rv);
    else
      EXPECT_EQ(net::ERR_IO_PENDING, rv);
    rv = cb.GetResult(rv);
    EXPECT_EQ(kEntrySize, rv);
    EXPECT_EQ(0, memcmp(read_buf->data(), payload_->data(), kEntrySize));
    entry->Close();
  }

 protected:
  scoped_refptr<net::IOBuffer> payload_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(DiskCacheSimplePrefetchTest, NoPrefetch) {
  base::HistogramTester histogram_tester;
  SetupFullPrefetch(0);

  const char kKey[] = "a key";
  InitCacheAndCreateEntry(kKey);
  TryRead(kKey, /* expect_preread_stream1 */ false);

  histogram_tester.ExpectUniqueSample("SimpleCache.Http.SyncOpenPrefetchMode",
                                      disk_cache::OPEN_PREFETCH_NONE, 1);
}

TEST_F(DiskCacheSimplePrefetchTest, YesPrefetch) {
  base::HistogramTester histogram_tester;
  SetupFullPrefetch(2 * kEntrySize);

  const char kKey[] = "a key";
  InitCacheAndCreateEntry(kKey);
  TryRead(kKey, /* expect_preread_stream1 */ true);

  histogram_tester.ExpectUniqueSample("SimpleCache.Http.SyncOpenPrefetchMode",
                                      disk_cache::OPEN_PREFETCH_FULL, 1);
}

TEST_F(DiskCacheSimplePrefetchTest, YesPrefetchNoRead) {
  base::HistogramTester histogram_tester;
  SetupFullPrefetch(2 * kEntrySize);

  const char kKey[] = "a key";
  InitCacheAndCreateEntry(kKey);

  disk_cache::Entry* entry = nullptr;
  ASSERT_THAT(OpenEntry(kKey, &entry), IsOk());
  entry->Close();

  histogram_tester.ExpectUniqueSample("SimpleCache.Http.SyncOpenPrefetchMode",
                                      disk_cache::OPEN_PREFETCH_FULL, 1);
}

// This makes sure we detect checksum error on entry that's small enough to be
// prefetched. This is like DiskCacheEntryTest.BadChecksum, but we make sure
// to configure prefetch explicitly.
TEST_F(DiskCacheSimplePrefetchTest, BadChecksumSmall) {
  SetupFullPrefetch(1024);  // bigger than stuff below.
  SetSimpleCacheMode();
  InitCache();

  const char key[] = "the first key";
  ASSERT_TRUE(SimpleCacheMakeBadChecksumEntry(key, 10));

  disk_cache::Entry* entry = nullptr;

  // Open the entry. Since we made a small entry, we will detect the CRC
  // problem at open.
  EXPECT_THAT(OpenEntry(key, &entry), IsError(net::ERR_FAILED));
}

TEST_F(DiskCacheSimplePrefetchTest, ChecksumNoPrefetch) {
  base::HistogramTester histogram_tester;

  SetupFullPrefetch(0);
  const char kKey[] = "a key";
  InitCacheAndCreateEntry(kKey);
  TryRead(kKey, /* expect_preread_stream1 */ false);

  histogram_tester.ExpectUniqueSample("SimpleCache.Http.SyncCheckEOFResult",
                                      disk_cache::CHECK_EOF_RESULT_SUCCESS, 2);
}

TEST_F(DiskCacheSimplePrefetchTest, NoChecksumNoPrefetch) {
  base::HistogramTester histogram_tester;

  SetupFullPrefetch(0);
  const char kKey[] = "a key";
  InitCacheAndCreateEntryWithNoCrc(kKey);
  TryRead(kKey, /* expect_preread_stream1 */ false);

  histogram_tester.ExpectUniqueSample("SimpleCache.Http.SyncCheckEOFResult",
                                      disk_cache::CHECK_EOF_RESULT_SUCCESS, 2);
}

TEST_F(DiskCacheSimplePrefetchTest, ChecksumPrefetch) {
  base::HistogramTester histogram_tester;

  SetupFullPrefetch(2 * kEntrySize);
  const char kKey[] = "a key";
  InitCacheAndCreateEntry(kKey);
  TryRead(kKey, /* expect_preread_stream1 */ true);

  histogram_tester.ExpectUniqueSample("SimpleCache.Http.SyncCheckEOFResult",
                                      disk_cache::CHECK_EOF_RESULT_SUCCESS, 2);
}

TEST_F(DiskCacheSimplePrefetchTest, NoChecksumPrefetch) {
  base::HistogramTester histogram_tester;

  SetupFullPrefetch(2 * kEntrySize);
  const char kKey[] = "a key";
  InitCacheAndCreateEntryWithNoCrc(kKey);
  TryRead(kKey, /* expect_preread_stream1 */ true);

  // EOF check is recorded even if there is no CRC there.
  histogram_tester.ExpectUniqueSample("SimpleCache.Http.SyncCheckEOFResult",
                                      disk_cache::CHECK_EOF_RESULT_SUCCESS, 2);
}

TEST_F(DiskCacheSimplePrefetchTest, PrefetchReadsSync) {
  // Make sure we can read things synchronously after prefetch.
  SetupFullPrefetch(32768);  // way bigger than kEntrySize
  const char kKey[] = "a key";
  InitCacheAndCreateEntry(kKey);

  disk_cache::Entry* entry = nullptr;
  ASSERT_THAT(OpenEntry(kKey, &entry), IsOk());
  auto read_buf = base::MakeRefCounted<net::IOBufferWithSize>(kEntrySize);

  // That this is entry->ReadData(...) rather than ReadData(entry, ...) is
  // meaningful here, as the latter is a helper in the test fixture that blocks
  // if needed.
  EXPECT_EQ(kEntrySize, entry->ReadData(1, 0, read_buf.get(), kEntrySize,
                                        net::CompletionOnceCallback()));
  EXPECT_EQ(0, memcmp(read_buf->data(), payload_->data(), kEntrySize));
  entry->Close();
}

TEST_F(DiskCacheSimplePrefetchTest, NoFullNoSpeculative) {
  base::HistogramTester histogram_tester;
  SetupFullAndTrailerPrefetch(0, 0);

  const char kKey[] = "a key";
  InitCacheAndCreateEntry(kKey);
  TryRead(kKey, /* expect_preread_stream1 */ false);

  histogram_tester.ExpectUniqueSample("SimpleCache.Http.SyncOpenPrefetchMode",
                                      disk_cache::OPEN_PREFETCH_NONE, 1);
}

TEST_F(DiskCacheSimplePrefetchTest, NoFullSmallSpeculative) {
  base::HistogramTester histogram_tester;
  SetupFullAndTrailerPrefetch(0, kEntrySize / 2);

  const char kKey[] = "a key";
  InitCacheAndCreateEntry(kKey);
  TryRead(kKey, /* expect_preread_stream1 */ false);

  histogram_tester.ExpectUniqueSample("SimpleCache.Http.SyncOpenPrefetchMode",
                                      disk_cache::OPEN_PREFETCH_TRAILER, 1);
}

TEST_F(DiskCacheSimplePrefetchTest, NoFullLargeSpeculative) {
  base::HistogramTester histogram_tester;
  // A large speculative trailer prefetch that exceeds the entry file
  // size should effectively trigger full prefetch behavior.
  SetupFullAndTrailerPrefetch(0, kEntrySize * 2);

  const char kKey[] = "a key";
  InitCacheAndCreateEntry(kKey);
  TryRead(kKey, /* expect_preread_stream1 */ true);

  histogram_tester.ExpectUniqueSample("SimpleCache.Http.SyncOpenPrefetchMode",
                                      disk_cache::OPEN_PREFETCH_FULL, 1);
}

TEST_F(DiskCacheSimplePrefetchTest, SmallFullNoSpeculative) {
  base::HistogramTester histogram_tester;
  SetupFullAndTrailerPrefetch(kEntrySize / 2, 0);

  const char kKey[] = "a key";
  InitCacheAndCreateEntry(kKey);
  TryRead(kKey, /* expect_preread_stream1 */ false);

  histogram_tester.ExpectUniqueSample("SimpleCache.Http.SyncOpenPrefetchMode",
                                      disk_cache::OPEN_PREFETCH_NONE, 1);
}

TEST_F(DiskCacheSimplePrefetchTest, LargeFullNoSpeculative) {
  base::HistogramTester histogram_tester;
  SetupFullAndTrailerPrefetch(kEntrySize * 2, 0);

  const char kKey[] = "a key";
  InitCacheAndCreateEntry(kKey);
  TryRead(kKey, /* expect_preread_stream1 */ true);

  histogram_tester.ExpectUniqueSample("SimpleCache.Http.SyncOpenPrefetchMode",
                                      disk_cache::OPEN_PREFETCH_FULL, 1);
}

TEST_F(DiskCacheSimplePrefetchTest, SmallFullSmallSpeculative) {
  base::HistogramTester histogram_tester;
  SetupFullAndTrailerPrefetch(kEntrySize / 2, kEntrySize / 2);

  const char kKey[] = "a key";
  InitCacheAndCreateEntry(kKey);
  TryRead(kKey, /* expect_preread_stream1 */ false);

  histogram_tester.ExpectUniqueSample("SimpleCache.Http.SyncOpenPrefetchMode",
                                      disk_cache::OPEN_PREFETCH_TRAILER, 1);
}

TEST_F(DiskCacheSimplePrefetchTest, LargeFullSmallSpeculative) {
  base::HistogramTester histogram_tester;
  // Full prefetch takes precedence over a trailer speculative prefetch.
  SetupFullAndTrailerPrefetch(kEntrySize * 2, kEntrySize / 2);

  const char kKey[] = "a key";
  InitCacheAndCreateEntry(kKey);
  TryRead(kKey, /* expect_preread_stream1 */ true);

  histogram_tester.ExpectUniqueSample("SimpleCache.Http.SyncOpenPrefetchMode",
                                      disk_cache::OPEN_PREFETCH_FULL, 1);
}

class DiskCacheSimpleAppCachePrefetchTest : public DiskCacheSimplePrefetchTest {
 public:
  // APP_CACHE mode will enable trailer prefetch hint support.
  net::CacheType SimpleCacheType() const override { return net::APP_CACHE; }
};

TEST_F(DiskCacheSimpleAppCachePrefetchTest, NoFullNoSpeculative) {
  base::HistogramTester histogram_tester;
  SetupFullAndTrailerPrefetch(0, 0);

  const char kKey[] = "a key";
  InitCacheAndCreateEntry(kKey);
  TryRead(kKey, /* expect_preread_stream1 */ false);

  histogram_tester.ExpectUniqueSample("SimpleCache.App.SyncOpenPrefetchMode",
                                      disk_cache::OPEN_PREFETCH_TRAILER, 1);
}

TEST_F(DiskCacheSimpleAppCachePrefetchTest, NoFullSmallSpeculative) {
  base::HistogramTester histogram_tester;
  SetupFullAndTrailerPrefetch(0, kEntrySize / 2);

  const char kKey[] = "a key";
  InitCacheAndCreateEntry(kKey);
  TryRead(kKey, /* expect_preread_stream1 */ false);

  histogram_tester.ExpectUniqueSample("SimpleCache.App.SyncOpenPrefetchMode",
                                      disk_cache::OPEN_PREFETCH_TRAILER, 1);
}

TEST_F(DiskCacheSimpleAppCachePrefetchTest, NoFullLargeSpeculative) {
  base::HistogramTester histogram_tester;
  // Even though the speculative trailer prefetch size is larger than the
  // file size, the hint should take precedence and still perform a limited
  // trailer prefetch.
  SetupFullAndTrailerPrefetch(0, kEntrySize * 2);

  const char kKey[] = "a key";
  InitCacheAndCreateEntry(kKey);
  TryRead(kKey, /* expect_preread_stream1 */ false);

  histogram_tester.ExpectUniqueSample("SimpleCache.App.SyncOpenPrefetchMode",
                                      disk_cache::OPEN_PREFETCH_TRAILER, 1);
}

TEST_F(DiskCacheSimpleAppCachePrefetchTest, SmallFullNoSpeculative) {
  base::HistogramTester histogram_tester;
  SetupFullAndTrailerPrefetch(kEntrySize / 2, 0);

  const char kKey[] = "a key";
  InitCacheAndCreateEntry(kKey);
  TryRead(kKey, /* expect_preread_stream1 */ false);

  histogram_tester.ExpectUniqueSample("SimpleCache.App.SyncOpenPrefetchMode",
                                      disk_cache::OPEN_PREFETCH_TRAILER, 1);
}

TEST_F(DiskCacheSimpleAppCachePrefetchTest, LargeFullNoSpeculative) {
  base::HistogramTester histogram_tester;
  // Full prefetch takes precedence over a trailer hint prefetch.
  SetupFullAndTrailerPrefetch(kEntrySize * 2, 0);

  const char kKey[] = "a key";
  InitCacheAndCreateEntry(kKey);
  TryRead(kKey, /* expect_preread_stream1 */ true);

  histogram_tester.ExpectUniqueSample("SimpleCache.App.SyncOpenPrefetchMode",
                                      disk_cache::OPEN_PREFETCH_FULL, 1);
}

TEST_F(DiskCacheSimpleAppCachePrefetchTest, SmallFullSmallSpeculative) {
  base::HistogramTester histogram_tester;
  SetupFullAndTrailerPrefetch(kEntrySize / 2, kEntrySize / 2);

  const char kKey[] = "a key";
  InitCacheAndCreateEntry(kKey);
  TryRead(kKey, /* expect_preread_stream1 */ false);

  histogram_tester.ExpectUniqueSample("SimpleCache.App.SyncOpenPrefetchMode",
                                      disk_cache::OPEN_PREFETCH_TRAILER, 1);
}

TEST_F(DiskCacheSimpleAppCachePrefetchTest, LargeFullSmallSpeculative) {
  base::HistogramTester histogram_tester;
  // Full prefetch takes precedence over a trailer speculative prefetch.
  SetupFullAndTrailerPrefetch(kEntrySize * 2, kEntrySize / 2);

  const char kKey[] = "a key";
  InitCacheAndCreateEntry(kKey);
  TryRead(kKey, /* expect_preread_stream1 */ true);

  histogram_tester.ExpectUniqueSample("SimpleCache.App.SyncOpenPrefetchMode",
                                      disk_cache::OPEN_PREFETCH_FULL, 1);
}
