// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/disk_cache_test_util.h"

#include "base/check_op.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "net/base/net_errors.h"
#include "net/disk_cache/backend_cleanup_tracker.h"
#include "net/disk_cache/blockfile/backend_impl.h"
#include "net/disk_cache/blockfile/file.h"
#include "net/disk_cache/cache_util.h"

using base::Time;

std::string GenerateKey(bool same_length) {
  char key[200];
  CacheTestFillBuffer(base::as_writable_byte_span(key), same_length);

  key[199] = '\0';
  return std::string(key);
}

void CacheTestFillBuffer(base::span<uint8_t> buffer, bool no_nulls) {
  static bool called = false;
  if (!called) {
    called = true;
    int seed = static_cast<int>(Time::Now().ToInternalValue());
    srand(seed);
  }

  for (size_t i = 0; i < buffer.size(); i++) {
    buffer[i] = static_cast<char>(rand());
    if (!buffer[i] && no_nulls)
      buffer[i] = 'g';
  }
  if (buffer.size() && !buffer[0]) {
    buffer[0] = 'g';
  }
}

scoped_refptr<net::IOBufferWithSize> CacheTestCreateAndFillBuffer(
    size_t len,
    bool no_nulls) {
  auto buffer = base::MakeRefCounted<net::IOBufferWithSize>(len);
  CacheTestFillBuffer(buffer->span(), no_nulls);
  return buffer;
}

bool CreateCacheTestFile(const base::FilePath& name) {
  int flags = base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_READ |
              base::File::FLAG_WRITE;

  base::File file(name, flags);
  if (!file.IsValid())
    return false;

  file.SetLength(4 * 1024 * 1024);
  return true;
}

bool DeleteCache(const base::FilePath& path) {
  disk_cache::DeleteCache(path, false);
  return true;
}

bool CheckCacheIntegrity(const base::FilePath& path,
                         bool new_eviction,
                         int max_size,
                         uint32_t mask) {
  auto cache = std::make_unique<disk_cache::BackendImpl>(
      path, mask, /* cleanup_tracker = */ nullptr,
      base::SingleThreadTaskRunner::GetCurrentDefault(), net::DISK_CACHE,
      nullptr);
  if (max_size)
    cache->SetMaxSize(max_size);
  if (!cache.get())
    return false;
  if (new_eviction)
    cache->SetNewEviction();
  cache->SetFlags(disk_cache::kNoRandom);
  if (cache->SyncInit() != net::OK)
    return false;
  return cache->SelfCheck() >= 0;
}

// -----------------------------------------------------------------------
TestBackendResultCompletionCallback::TestBackendResultCompletionCallback() =
    default;

TestBackendResultCompletionCallback::~TestBackendResultCompletionCallback() =
    default;

disk_cache::BackendResultCallback
TestBackendResultCompletionCallback::callback() {
  return base::BindOnce(&TestBackendResultCompletionCallback::SetResult,
                        base::Unretained(this));
}

TestEntryResultCompletionCallback::TestEntryResultCompletionCallback() =
    default;

TestEntryResultCompletionCallback::~TestEntryResultCompletionCallback() =
    default;

disk_cache::Backend::EntryResultCallback
TestEntryResultCompletionCallback::callback() {
  return base::BindOnce(&TestEntryResultCompletionCallback::SetResult,
                        base::Unretained(this));
}

TestRangeResultCompletionCallback::TestRangeResultCompletionCallback() =
    default;

TestRangeResultCompletionCallback::~TestRangeResultCompletionCallback() =
    default;

disk_cache::RangeResultCallback TestRangeResultCompletionCallback::callback() {
  return base::BindOnce(&TestRangeResultCompletionCallback::HelpSetResult,
                        base::Unretained(this));
}

void TestRangeResultCompletionCallback::HelpSetResult(
    const disk_cache::RangeResult& result) {
  SetResult(result);
}

// -----------------------------------------------------------------------

MessageLoopHelper::MessageLoopHelper() = default;

MessageLoopHelper::~MessageLoopHelper() = default;

bool MessageLoopHelper::WaitUntilCacheIoFinished(int num_callbacks) {
  if (num_callbacks == callbacks_called_)
    return true;
  expected_num_callbacks_ = num_callbacks;
  completed_ = false;

  run_loop_ = std::make_unique<base::RunLoop>();
  run_loop_->Run();
  run_loop_.reset();

  return completed_;
}

void MessageLoopHelper::CallbackWasCalled() {
  CHECK_LE(callbacks_called_, expected_num_callbacks_);
  ++callbacks_called_;
  if (run_loop_ && expected_num_callbacks_ == callbacks_called_) {
    completed_ = true;
    run_loop_->Quit();
  }
}

// -----------------------------------------------------------------------

CallbackTest::CallbackTest(MessageLoopHelper* helper,
                           bool reuse)
    : helper_(helper),
      reuse_(reuse ? 0 : 1) {
}

CallbackTest::~CallbackTest() = default;

// On the actual callback, increase the number of tests received and check for
// errors (an unexpected test received)
void CallbackTest::Run(int result) {
  last_result_ = result;

  if (reuse_) {
    DCHECK_EQ(1, reuse_);
    if (2 == reuse_)
      helper_->set_callback_reused_error(true);
    reuse_++;
  }

  helper_->CallbackWasCalled();
}

void CallbackTest::RunWithEntry(disk_cache::EntryResult result) {
  last_entry_result_ = std::move(result);
  Run(last_entry_result_.net_error());
}
