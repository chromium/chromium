// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DISK_CACHE_DISK_CACHE_TEST_UTIL_H_
#define NET_DISK_CACHE_DISK_CACHE_TEST_UTIL_H_

#include <stddef.h>
#include <stdint.h>

#include <string>

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "net/base/test_completion_callback.h"
#include "net/disk_cache/disk_cache.h"

namespace net {
class IOBufferWithSize;
}  // namespace net

// Re-creates a given test file inside the cache test folder.
bool CreateCacheTestFile(const base::FilePath& name);

// Deletes all file son the cache.
bool DeleteCache(const base::FilePath& path);

// Fills buffer with random values (may contain nulls unless no_nulls is true).
void CacheTestFillBuffer(char* buffer, size_t len, bool no_nulls);

// Creates a buffer of size `len`, and fills in with random values, which
// may contain 0 unless `no_nulls` is true.
scoped_refptr<net::IOBufferWithSize> CacheTestCreateAndFillBuffer(
    size_t len,
    bool no_nulls);

// Generates a random key of up to 200 bytes.
std::string GenerateKey(bool same_length);

// Returns true if the cache is not corrupt. Assumes blockfile cache.
// |max_size|, if non-zero, will be set as its size.
bool CheckCacheIntegrity(const base::FilePath& path,
                         bool new_eviction,
                         int max_size,
                         uint32_t mask);

// -----------------------------------------------------------------------

// Like net::TestCompletionCallback, but for BackendResultCallback.
struct BackendResultIsPendingHelper {
  bool operator()(const disk_cache::BackendResult& result) const {
    return result.net_error == net::ERR_IO_PENDING;
  }
};
using TestBackendResultCompletionCallbackBase =
    net::internal::TestCompletionCallbackTemplate<disk_cache::BackendResult,
                                                  BackendResultIsPendingHelper>;

class TestBackendResultCompletionCallback
    : public TestBackendResultCompletionCallbackBase {
 public:
  TestBackendResultCompletionCallback();

  TestBackendResultCompletionCallback(
      const TestBackendResultCompletionCallback&) = delete;
  TestBackendResultCompletionCallback& operator=(
      const TestBackendResultCompletionCallback&) = delete;

  ~TestBackendResultCompletionCallback() override;

  disk_cache::BackendResultCallback callback();
};

// Like net::TestCompletionCallback, but for EntryResultCallback.

struct EntryResultIsPendingHelper {
  bool operator()(const disk_cache::EntryResult& result) const {
    return result.net_error() == net::ERR_IO_PENDING;
  }
};
using TestEntryResultCompletionCallbackBase =
    net::internal::TestCompletionCallbackTemplate<disk_cache::EntryResult,
                                                  EntryResultIsPendingHelper>;

class TestEntryResultCompletionCallback
    : public TestEntryResultCompletionCallbackBase {
 public:
  TestEntryResultCompletionCallback();

  TestEntryResultCompletionCallback(const TestEntryResultCompletionCallback&) =
      delete;
  TestEntryResultCompletionCallback& operator=(
      const TestEntryResultCompletionCallback&) = delete;

  ~TestEntryResultCompletionCallback() override;

  disk_cache::Backend::EntryResultCallback callback();
};

// Like net::TestCompletionCallback, but for RangeResultCallback.
struct RangeResultIsPendingHelper {
  bool operator()(const disk_cache::RangeResult& result) const {
    return result.net_error == net::ERR_IO_PENDING;
  }
};

class TestRangeResultCompletionCallback
    : public net::internal::TestCompletionCallbackTemplate<
          disk_cache::RangeResult,
          RangeResultIsPendingHelper> {
 public:
  TestRangeResultCompletionCallback();
  ~TestRangeResultCompletionCallback() override;

  disk_cache::RangeResultCallback callback();

 private:
  // Reference -> Value adapter --- disk_cache wants reference for callback,
  // base class wants a value.
  void HelpSetResult(const disk_cache::RangeResult& result);
};

// -----------------------------------------------------------------------

// Simple helper to deal with the message loop on a test.
class MessageLoopHelper {
 public:
  MessageLoopHelper();

  MessageLoopHelper(const MessageLoopHelper&) = delete;
  MessageLoopHelper& operator=(const MessageLoopHelper&) = delete;

  ~MessageLoopHelper();

  // Run the message loop and wait for num_callbacks before returning. Returns
  // false if we are waiting to long. Each callback that will be waited on is
  // required to call CallbackWasCalled() to indicate when it was called.
  bool WaitUntilCacheIoFinished(int num_callbacks);

  // True if a given callback was called more times than it expected.
  bool callback_reused_error() const { return callback_reused_error_; }
  void set_callback_reused_error(bool error) {
    callback_reused_error_ = error;
  }

  int callbacks_called() const { return callbacks_called_; }
  // Report that a callback was called. Each callback that will be waited on
  // via WaitUntilCacheIoFinished() is expected to call this method to
  // indicate when it has been executed.
  void CallbackWasCalled() { ++callbacks_called_; }

 private:
  // Sets the number of callbacks that can be received so far.
  void ExpectCallbacks(int num_callbacks) {
    num_callbacks_ = num_callbacks;
    num_iterations_ = last_ = 0;
    completed_ = false;
  }

  // Called periodically to test if WaitUntilCacheIoFinished should return.
  void TimerExpired();

  std::unique_ptr<base::RunLoop> run_loop_;
  int num_callbacks_ = 0;
  int num_iterations_ = 0;
  int last_ = 0;
  bool completed_ = false;

  // True if a callback was called/reused more than expected.
  bool callback_reused_error_ = false;
  int callbacks_called_ = 0;
};

// -----------------------------------------------------------------------

// Simple callback to process IO completions from the cache. It allows tests
// with multiple simultaneous IO operations.
class CallbackTest {
 public:
  // Creates a new CallbackTest object. When the callback is called, it will
  // update |helper|. If |reuse| is false and a callback is called more than
  // once, or if |reuse| is true and a callback is called more than twice, an
  // error will be reported to |helper|.
  CallbackTest(MessageLoopHelper* helper, bool reuse);

  CallbackTest(const CallbackTest&) = delete;
  CallbackTest& operator=(const CallbackTest&) = delete;

  ~CallbackTest();

  void Run(int result);
  void RunWithEntry(disk_cache::EntryResult result);

  int last_result() const { return last_result_; }
  disk_cache::EntryResult ReleaseLastEntryResult() {
    return std::move(last_entry_result_);
  }

 private:
  raw_ptr<MessageLoopHelper> helper_;
  int reuse_;
  int last_result_;
  disk_cache::EntryResult last_entry_result_;
};

#endif  // NET_DISK_CACHE_DISK_CACHE_TEST_UTIL_H_
