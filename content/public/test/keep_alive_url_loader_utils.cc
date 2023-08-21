// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "content/public/test/keep_alive_url_loader_utils.h"

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "content/browser/loader/keep_alive_url_loader.h"
#include "content/browser/loader/keep_alive_url_loader_service.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace content {

namespace {

// Counts the total triggering of `Increment()` asynchronously.
//
// Use `WaitUntil()` to wait until this counter reaching specific value.
class AtomicCounter {
 public:
  AtomicCounter() = default;
  // Not Copyable.
  AtomicCounter(const AtomicCounter&) = delete;
  AtomicCounter& operator=(const AtomicCounter&) = delete;

  // Increments the internal counter, and stops `waiting_run_loop_` if exists.
  void Increment() {
    base::AutoLock auto_lock(lock_);
    count_++;
    if (waiting_run_loop_) {
      waiting_run_loop_->Quit();
    }
  }

  // If `count_` does not yet reach `value`, a RunLoop will be created and runs
  // until it is stopped by `Increment()`.
  void WaitUntil(size_t value) {
    {
      base::AutoLock auto_lock(lock_);
      if (count_ >= value) {
        return;
      }
    }

    {
      base::AutoLock auto_lock(lock_);
      waiting_run_loop_ = std::make_unique<base::RunLoop>(
          base::RunLoop::Type::kNestableTasksAllowed);
    }
    waiting_run_loop_->Run();

    {
      base::AutoLock auto_lock(lock_);
      waiting_run_loop_.reset();
    }
  }

 private:
  base::Lock lock_;
  size_t count_ GUARDED_BY(lock_) = 0;
  std::unique_ptr<base::RunLoop> waiting_run_loop_ = nullptr;
};

// `arg` is a 2-tuple (URLLoaderCompletionStatus, int).
MATCHER(ErrorCodeEq, "match the same error code") {
  const auto& expected = std::get<1>(arg);
  const auto& got = std::get<0>(arg).error_code;
  if (got != expected) {
    *result_listener << "expected error code [" << expected << "] got [" << got
                     << "]";
    return false;
  }
  return true;
}

}  // namespace

class TestObserverImpl : public KeepAliveURLLoader::TestObserver {
 public:
  TestObserverImpl() = default;

  // Not Copyable.
  TestObserverImpl(const TestObserverImpl&) = delete;
  TestObserverImpl& operator=(const TestObserverImpl&) = delete;

  // Waits for `OnReceiveRedirectForwarded()` to be called `total` times.
  void WaitForTotalOnReceiveRedirectForwarded(size_t total) {
    on_receive_redirect_forwarded_count_.WaitUntil(total);
  }
  // Waits for `OnReceiveRedirectProcessed()` to be called `total` times.
  void WaitForTotalOnReceiveRedirectProcessed(size_t total) {
    on_receive_redirect_processed_count_.WaitUntil(total);
  }
  // Waits for `OnReceiveResponse()` to be called `total` times.
  void WaitForTotalOnReceiveResponse(size_t total) {
    on_receive_response_count_.WaitUntil(total);
  }
  // Waits for `OnReceiveResponseForwarded()` to be called `total` times.
  void WaitForTotalOnReceiveResponseForwarded(size_t total) {
    on_receive_response_forwarded_count_.WaitUntil(total);
  }
  // Waits for `OnReceiveResponseProcessed()` to be called `total` times.
  void WaitForTotalOnReceiveResponseProcessed(size_t total) {
    on_receive_response_processed_count_.WaitUntil(total);
  }
  // Waits for `OnComplete()` to be called `error_codes.size()` times,
  // and the error codes from `on_complete_forwarded_status_` should match
  // `error_codes`.
  void WaitForTotalOnComplete(const std::vector<int>& error_codes) {
    on_complete_count_.WaitUntil(error_codes.size());
    EXPECT_THAT(on_complete_status_,
                testing::Pointwise(ErrorCodeEq(), error_codes));
  }
  // Waits for `OnCompleteForwarded()` to be called `error_codes.size()` times,
  // and the error codes from `on_complete_forwarded_status_` should match
  // `error_codes`.
  void WaitForTotalOnCompleteForwarded(const std::vector<int>& error_codes) {
    on_complete_forwarded_count_.WaitUntil(error_codes.size());
    EXPECT_THAT(on_complete_forwarded_status_,
                testing::Pointwise(ErrorCodeEq(), error_codes));
  }
  // Waits for `OnCompleteProcessed()` to be called `error_codes.size()` times,
  // and the error codes from `on_complete_processed_status_` should match
  // `error_codes`.
  void WaitForTotalOnCompleteProcessed(const std::vector<int>& error_codes) {
    on_complete_processed_count_.WaitUntil(error_codes.size());
    EXPECT_THAT(on_complete_processed_status_,
                testing::Pointwise(ErrorCodeEq(), error_codes));
  }
  // Waits for `PauseReadingBodyFromNetProcessed()` to be called `total` times.
  void WaitForTotalPauseReadingBodyFromNetProcessed(size_t total) {
    pause_reading_body_from_net_processed_count_.WaitUntil(total);
  }
  // Waits for `ResumeReadingBodyFromNetProcessed()` to be called `total` times.
  void WaitForTotalResumeReadingBodyFromNetProcessed(size_t total) {
    resume_reading_body_from_net_processed_count_.WaitUntil(total);
  }

 private:
  ~TestObserverImpl() override = default;

  // KeepAliveURLLoader::TestObserver overrides:
  void OnReceiveRedirectForwarded(KeepAliveURLLoader* loader) override {
    on_receive_redirect_forwarded_count_.Increment();
  }
  void OnReceiveRedirectProcessed(KeepAliveURLLoader* loader) override {
    on_receive_redirect_processed_count_.Increment();
  }
  void OnReceiveResponse(KeepAliveURLLoader* loader) override {
    on_receive_response_count_.Increment();
  }
  void OnReceiveResponseForwarded(KeepAliveURLLoader* loader) override {
    on_receive_response_forwarded_count_.Increment();
  }
  void OnReceiveResponseProcessed(KeepAliveURLLoader* loader) override {
    on_receive_response_processed_count_.Increment();
  }
  void OnComplete(
      KeepAliveURLLoader* loader,
      const network::URLLoaderCompletionStatus& completion_status) override {
    on_complete_count_.Increment();
    on_complete_status_.push_back(completion_status);
  }
  void OnCompleteForwarded(
      KeepAliveURLLoader* loader,
      const network::URLLoaderCompletionStatus& completion_status) override {
    on_complete_forwarded_count_.Increment();
    on_complete_forwarded_status_.push_back(completion_status);
  }
  void OnCompleteProcessed(
      KeepAliveURLLoader* loader,
      const network::URLLoaderCompletionStatus& completion_status) override {
    on_complete_processed_count_.Increment();
    on_complete_processed_status_.push_back(completion_status);
  }

  void PauseReadingBodyFromNetProcessed(KeepAliveURLLoader* loader) override {
    pause_reading_body_from_net_processed_count_.Increment();
  }
  void ResumeReadingBodyFromNetProcessed(KeepAliveURLLoader* loader) override {
    resume_reading_body_from_net_processed_count_.Increment();
  }

  // OnReceiveRedirect*:
  AtomicCounter on_receive_redirect_forwarded_count_;
  AtomicCounter on_receive_redirect_processed_count_;
  // OnReceiveResponse*:
  AtomicCounter on_receive_response_count_;
  AtomicCounter on_receive_response_forwarded_count_;
  AtomicCounter on_receive_response_processed_count_;
  // OnComplete*:
  AtomicCounter on_complete_count_;
  AtomicCounter on_complete_forwarded_count_;
  AtomicCounter on_complete_processed_count_;

  AtomicCounter pause_reading_body_from_net_processed_count_;
  AtomicCounter resume_reading_body_from_net_processed_count_;

  std::vector<network::URLLoaderCompletionStatus> on_complete_status_;
  std::vector<network::URLLoaderCompletionStatus> on_complete_forwarded_status_;
  std::vector<network::URLLoaderCompletionStatus> on_complete_processed_status_;
};

class KeepAliveURLLoadersTestObserverImpl {
 public:
  KeepAliveURLLoadersTestObserverImpl()
      : refptr_(base::MakeRefCounted<TestObserverImpl>()) {}
  ~KeepAliveURLLoadersTestObserverImpl() = default;

  scoped_refptr<TestObserverImpl> refptr() const { return refptr_; }
  TestObserverImpl* get() const { return refptr_.get(); }

 private:
  scoped_refptr<TestObserverImpl> refptr_;
};

KeepAliveURLLoadersTestObserver::KeepAliveURLLoadersTestObserver(
    BrowserContext* browser_context)
    : impl_(std::make_unique<KeepAliveURLLoadersTestObserverImpl>()) {
  CHECK(browser_context);

  static_cast<StoragePartitionImpl*>(
      browser_context->GetDefaultStoragePartition())
      ->GetKeepAliveURLLoaderService()
      ->SetLoaderObserverForTesting(impl_->refptr());
}

KeepAliveURLLoadersTestObserver::~KeepAliveURLLoadersTestObserver() = default;

void KeepAliveURLLoadersTestObserver::WaitForTotalOnReceiveRedirectForwarded(
    size_t total) {
  impl_->get()->WaitForTotalOnReceiveRedirectForwarded(total);
}

void KeepAliveURLLoadersTestObserver::WaitForTotalOnReceiveRedirectProcessed(
    size_t total) {
  impl_->get()->WaitForTotalOnReceiveRedirectProcessed(total);
}

void KeepAliveURLLoadersTestObserver::WaitForTotalOnReceiveResponse(
    size_t total) {
  impl_->get()->WaitForTotalOnReceiveResponse(total);
}

void KeepAliveURLLoadersTestObserver::WaitForTotalOnReceiveResponseForwarded(
    size_t total) {
  impl_->get()->WaitForTotalOnReceiveResponseForwarded(total);
}

void KeepAliveURLLoadersTestObserver::WaitForTotalOnReceiveResponseProcessed(
    size_t total) {
  impl_->get()->WaitForTotalOnReceiveResponseProcessed(total);
}

void KeepAliveURLLoadersTestObserver::WaitForTotalOnComplete(
    const std::vector<int>& error_codes) {
  impl_->get()->WaitForTotalOnComplete(error_codes);
}

void KeepAliveURLLoadersTestObserver::WaitForTotalOnCompleteForwarded(
    const std::vector<int>& error_codes) {
  impl_->get()->WaitForTotalOnCompleteForwarded(error_codes);
}

void KeepAliveURLLoadersTestObserver::WaitForTotalOnCompleteProcessed(
    const std::vector<int>& error_codes) {
  impl_->get()->WaitForTotalOnCompleteProcessed(error_codes);
}

void KeepAliveURLLoadersTestObserver::
    WaitForTotalPauseReadingBodyFromNetProcessed(size_t total) {
  impl_->get()->WaitForTotalPauseReadingBodyFromNetProcessed(total);
}

void KeepAliveURLLoadersTestObserver::
    WaitForTotalResumeReadingBodyFromNetProcessed(size_t total) {
  impl_->get()->WaitForTotalResumeReadingBodyFromNetProcessed(total);
}

}  // namespace content
