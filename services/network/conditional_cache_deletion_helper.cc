// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/conditional_cache_deletion_helper.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "net/http/http_cache.h"
#include "net/http/http_util.h"

namespace {

bool EntryPredicateFromURLsAndTime(
    const base::RepeatingCallback<bool(const GURL&)>& url_matcher,
    const base::Time& begin_time,
    const base::Time& end_time,
    const disk_cache::Entry* entry) {
  std::string entry_key(entry->GetKey());
  std::string url_string(
      net::HttpCache::GetResourceURLFromHttpCacheKey(entry_key));
  return (entry->GetLastUsed() >= begin_time &&
          entry->GetLastUsed() < end_time && url_matcher.Run(GURL(url_string)));
}

}  // namespace

namespace network {

// static
std::unique_ptr<ConditionalCacheDeletionHelper>
ConditionalCacheDeletionHelper::CreateAndStart(
    disk_cache::Backend* cache,
    const base::RepeatingCallback<bool(const GURL&)>& url_matcher,
    const base::Time& begin_time,
    const base::Time& end_time,
    base::OnceClosure completion_callback) {
  std::unique_ptr<ConditionalCacheDeletionHelper> deletion_helper(
      new ConditionalCacheDeletionHelper(
          base::BindRepeating(
              &EntryPredicateFromURLsAndTime, url_matcher,
              begin_time.is_null() ? base::Time() : begin_time,
              end_time.is_null() ? base::Time::Max() : end_time),
          std::move(completion_callback), cache->CreateIterator()));

  // Any status other than OK (since no entry), IO_PENDING, or FAILED would
  // work here.
  deletion_helper->IterateOverEntries(
      disk_cache::EntryResult::MakeError(net::ERR_CACHE_OPEN_FAILURE));
  return deletion_helper;
}

ConditionalCacheDeletionHelper::ConditionalCacheDeletionHelper(
    const base::RepeatingCallback<bool(const disk_cache::Entry*)>& condition,
    base::OnceClosure completion_callback,
    std::unique_ptr<disk_cache::Backend::Iterator> iterator)
    : condition_(condition),
      completion_callback_(std::move(completion_callback)),
      iterator_(std::move(iterator)) {}

ConditionalCacheDeletionHelper::~ConditionalCacheDeletionHelper() = default;

void ConditionalCacheDeletionHelper::IterateOverEntries(
    disk_cache::EntryResult result) {
  while (result.net_error() != net::ERR_IO_PENDING) {
    // If the entry obtained in the previous iteration matches the condition,
    // mark it for deletion. The iterator is already one step forward, so it
    // won't be invalidated. Always close the previous entry so it does not
    // leak.
    if (previous_entry_) {
      if (condition_.Run(previous_entry_)) {
        previous_entry_->Doom();
      }
      previous_entry_->Close();
    }

    if (result.net_error() == net::ERR_FAILED) {
      // The iteration finished successfully or we can no longer iterate
      // (e.g. the cache was destroyed). We cannot distinguish between the two,
      // but we know that there is nothing more that we can do.
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE,
          base::BindOnce(&ConditionalCacheDeletionHelper::NotifyCompletion,
                         weak_factory_.GetWeakPtr()));
      return;
    }

    previous_entry_ = result.ReleaseEntry();
    result = iterator_->OpenNextEntry(
        base::BindOnce(&ConditionalCacheDeletionHelper::IterateOverEntries,
                       weak_factory_.GetWeakPtr()));
  }
}

void ConditionalCacheDeletionHelper::NotifyCompletion() {
  std::move(completion_callback_).Run();
}

}  // namespace network
