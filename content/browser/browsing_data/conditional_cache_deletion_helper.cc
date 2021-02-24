// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browsing_data/conditional_cache_deletion_helper.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/sequence_checker.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/public/browser/browser_thread.h"

namespace {

bool EntryPredicateFromURLsAndTime(
    const base::RepeatingCallback<bool(const GURL&)>& url_predicate,
    const base::RepeatingCallback<std::string(const std::string&)>&
        get_url_from_key,
    base::Time begin_time,
    base::Time end_time,
    const disk_cache::Entry* entry) {
  std::string url = entry->GetKey();
  if (!get_url_from_key.is_null())
    url = get_url_from_key.Run(url);
  return (entry->GetLastUsed() >= begin_time &&
          entry->GetLastUsed() < end_time && !url.empty() &&
          url_predicate.Run(GURL(url)));
}

}  // namespace

namespace content {

ConditionalCacheDeletionHelper::ConditionalCacheDeletionHelper(
    disk_cache::Backend* cache,
    base::RepeatingCallback<bool(const disk_cache::Entry*)> condition)
    : cache_(cache),
      condition_(std::move(condition)),
      previous_entry_(nullptr) {
}

// static
base::RepeatingCallback<bool(const disk_cache::Entry*)>
ConditionalCacheDeletionHelper::CreateURLAndTimeCondition(
    base::RepeatingCallback<bool(const GURL&)> url_predicate,
    base::RepeatingCallback<std::string(const std::string&)> get_url_from_key,
    base::Time begin_time,
    base::Time end_time) {
  return base::BindRepeating(&EntryPredicateFromURLsAndTime,
                             std::move(url_predicate),
                             std::move(get_url_from_key),
                             begin_time.is_null() ? base::Time() : begin_time,
                             end_time.is_null() ? base::Time::Max() : end_time);
}

int ConditionalCacheDeletionHelper::DeleteAndDestroySelfWhenFinished(
    net::CompletionOnceCallback completion_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  completion_callback_ = std::move(completion_callback);
  iterator_ = cache_->CreateIterator();

  // Any status other than OK (since no entry), IO_PENDING, or FAILED would
  // work here.
  IterateOverEntries(
      disk_cache::EntryResult::MakeError(net::ERR_CACHE_OPEN_FAILURE));

  // DeleteAndDestroySelfWhenFinished() itself is always async since
  // |completion_callback| is always posted and never run directly.
  return net::ERR_IO_PENDING;
}

ConditionalCacheDeletionHelper::~ConditionalCacheDeletionHelper() {}

void ConditionalCacheDeletionHelper::IterateOverEntries(
    disk_cache::EntryResult result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  while (result.net_error() != net::ERR_IO_PENDING) {
    // If the entry obtained in the previous iteration matches the condition,
    // mark it for deletion. The iterator is already one step forward, so it
    // won't be invalidated. Always close the previous entry so it does not
    // leak.
    if (previous_entry_) {
      if (condition_.Run(previous_entry_))
        previous_entry_->Doom();
      previous_entry_->Close();
    }

    if (result.net_error() == net::ERR_FAILED) {
      // The iteration finished successfully or we can no longer iterate
      // (e.g. the cache was destroyed). We cannot distinguish between the two,
      // but we know that there is nothing more that we can do, so we return OK.
      DCHECK(completion_callback_);
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::BindOnce(std::move(completion_callback_), net::OK));
      base::ThreadTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE, this);
      return;
    }

    previous_entry_ = result.ReleaseEntry();
    result = iterator_->OpenNextEntry(
        base::BindOnce(&ConditionalCacheDeletionHelper::IterateOverEntries,
                       base::Unretained(this)));
  }
}

}  // namespace content
