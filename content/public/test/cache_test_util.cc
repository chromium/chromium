// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/cache_test_util.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/post_task.h"
#include "content/public/browser/browser_task_traits.h"
#include "net/disk_cache/disk_cache.h"
#include "net/http/http_cache.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"

namespace content {

CacheTestUtil::CacheTestUtil(content::StoragePartition* partition)
    : partition_(partition), remaining_tasks_(0) {
  done_callback_ =
      base::Bind(&CacheTestUtil::DoneCallback, base::Unretained(this));
  // UI and IO thread synchronization.
  waitable_event_ = std::make_unique<base::WaitableEvent>(
      base::WaitableEvent::ResetPolicy::AUTOMATIC,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&CacheTestUtil::SetUpOnIOThread, base::Unretained(this)));
  WaitForTasksOnIOThread();
}

CacheTestUtil::~CacheTestUtil() {
  // The cache iterator must be deleted on the thread where it was created,
  // which is the IO thread.
  base::PostTaskWithTraits(FROM_HERE, {BrowserThread::IO},
                           base::BindOnce(&CacheTestUtil::TearDownOnIOThread,
                                          base::Unretained(this)));
  WaitForTasksOnIOThread();
}

void CacheTestUtil::CreateCacheEntries(const std::set<std::string>& keys) {
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&CacheTestUtil::CreateCacheEntriesOnIOThread,
                     base::Unretained(this), base::ConstRef(keys)));
  WaitForTasksOnIOThread();
}

void CacheTestUtil::SetUpOnIOThread() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  net::URLRequestContextGetter* context = partition_->GetURLRequestContext();

  net::HttpCache* cache =
      context->GetURLRequestContext()->http_transaction_factory()->GetCache();

  SetNumberOfWaitedTasks(1);
  WaitForCompletion(cache->GetBackend(&backend_, done_callback_));
}

void CacheTestUtil::TearDownOnIOThread() {
  iterator_.reset();
  for (disk_cache::Entry* entry : entries_) {
    entry->Close();
  }
  entries_.clear();
  DoneCallback(net::OK);
}

void CacheTestUtil::CreateCacheEntriesOnIOThread(
    const std::set<std::string>& keys) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  int pos = entries_.size();
  entries_.resize(entries_.size() + keys.size());
  SetNumberOfWaitedTasks(keys.size());

  for (const std::string& key : keys) {
    WaitForCompletion(backend_->CreateEntry(key, net::HIGHEST, &entries_[pos++],
                                            done_callback_));
  }
}

// Waiting for tasks to be done on IO thread. --------------------------------

void CacheTestUtil::WaitForTasksOnIOThread() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  waitable_event_->Wait();
}

void CacheTestUtil::SetNumberOfWaitedTasks(int count) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  remaining_tasks_ = count;
}

void CacheTestUtil::WaitForCompletion(int value) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (value >= 0) {
    // We got the result immediately.
    DoneCallback(value);
  } else if (value == net::ERR_IO_PENDING) {
    // We need to wait for the callback.
  } else {
    // An error has occurred.
    NOTREACHED();
  }
}

void CacheTestUtil::DoneCallback(int value) {
  DCHECK_GE(value, 0);  // Negative values represent an error.
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (--remaining_tasks_ > 0)
    return;

  waitable_event_->Signal();
}

// Check cache content.
std::vector<std::string> CacheTestUtil::GetEntryKeys() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&CacheTestUtil::GetEntryKeysOnIOThread,
                     base::Unretained(this)));
  WaitForTasksOnIOThread();
  return keys_;
}

void CacheTestUtil::GetEntryKeysOnIOThread() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  keys_.clear();
  current_entry_ = nullptr;
  iterator_ = backend_->CreateIterator();
  GetNextKey(net::OK);
}

void CacheTestUtil::GetNextKey(int error) {
  while (error != net::ERR_IO_PENDING) {
    if (error == net::ERR_FAILED) {
      DoneCallback(net::OK);
      return;
    }

    if (current_entry_) {
      keys_.push_back(current_entry_->GetKey());
    }

    error = iterator_->OpenNextEntry(
        &current_entry_,
        base::BindOnce(&CacheTestUtil::GetNextKey, base::Unretained(this)));
  }
}

}  // namespace content
