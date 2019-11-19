// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The eviction policy is a very simple pure LRU, so the elements at the end of
// the list are evicted until kCleanUpMargin free space is available. There is
// only one list in use (Rankings::NO_USE), and elements are sent to the front
// of the list whenever they are accessed.

// The new (in-development) eviction policy adds re-use as a factor to evict
// an entry. The story so far:

// Entries are linked on separate lists depending on how often they are used.
// When we see an element for the first time, it goes to the NO_USE list; if
// the object is reused later on, we move it to the LOW_USE list, until it is
// used kHighUse times, at which point it is moved to the HIGH_USE list.
// Whenever an element is evicted, we move it to the DELETED list so that if the
// element is accessed again, we remember the fact that it was already stored
// and maybe in the future we don't evict that element.

// When we have to evict an element, first we try to use the last element from
// the NO_USE list, then we move to the LOW_USE and only then we evict an entry
// from the HIGH_USE. We attempt to keep entries on the cache for at least
// kTargetTime hours (with frequently accessed items stored for longer periods),
// but if we cannot do that, we fall-back to keep each list roughly the same
// size so that we have a chance to see an element again and move it to another
// list.

#include "net/disk_cache/blockfile/eviction.h"

#include <stdint.h>

#include <limits>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "net/disk_cache/blockfile/backend_impl.h"
#include "net/disk_cache/blockfile/disk_format.h"
#include "net/disk_cache/blockfile/entry_impl.h"
#include "net/disk_cache/blockfile/experiments.h"
#include "net/disk_cache/blockfile/histogram_macros.h"
#include "net/disk_cache/blockfile/trace.h"

// Provide a BackendImpl object to macros from histogram_macros.h.
#define CACHE_UMA_BACKEND_IMPL_OBJ backend_

using base::Time;
using base::TimeTicks;

namespace {

const int kCleanUpMargin = 1024 * 1024;
const int kHighUse = 10;  // Reuse count to be on the HIGH_USE list.
const int kTargetTime = 24 * 7;  // Time to be evicted (hours since last use).
const int kMaxDelayedTrims = 60;

int LowWaterAdjust(int high_water) {
  if (high_water < kCleanUpMargin)
    return 0;

  return high_water - kCleanUpMargin;
}

bool FallingBehind(int current_size, int max_size) {
  return current_size > max_size - kCleanUpMargin * 20;
}

}  // namespace

namespace disk_cache {

// The real initialization happens during Init(), init_ is the only member that
// has to be initialized here.
Eviction::Eviction() : backend_(nullptr), init_(false) {}

Eviction::~Eviction() = default;

void Eviction::Init(BackendImpl* backend) {
  // We grab a bunch of info from the backend to make the code a little cleaner
  // when we're actually doing work.
  backend_ = backend;
  rankings_ = &backend->rankings_;
  header_ = &backend_->data_->header;
  max_size_ = LowWaterAdjust(backend_->max_size_);
  index_size_ = backend->mask_ + 1;
  new_eviction_ = backend->new_eviction_;
  first_trim_ = true;
  trimming_ = false;
  delay_trim_ = false;
  trim_delays_ = 0;
  init_ = true;
  test_mode_ = false;
}

void Eviction::Stop() {
  // It is possible for the backend initialization to fail, in which case this
  // object was never initialized... and there is nothing to do.
  if (!init_)
    return;

  // We want to stop further evictions, so let's pretend that we are busy from
  // this point on.
  DCHECK(!trimming_);
  trimming_ = true;
  ptr_factory_.InvalidateWeakPtrs();
}

void Eviction::TrimCache(bool empty) {
  if (backend_->disabled_ || trimming_)
    return;

  if (!empty && !ShouldTrim())
    return PostDelayedTrim();

  if (new_eviction_)
    return TrimCacheV2(empty);

  Trace("*** Trim Cache ***");
  trimming_ = true;
  TimeTicks start = TimeTicks::Now();
  Rankings::ScopedRankingsBlock node(rankings_);
  Rankings::ScopedRankingsBlock next(
      rankings_, rankings_->GetPrev(node.get(), Rankings::NO_USE));
  int deleted_entries = 0;
  int target_size = empty ? 0 : max_size_;
  while ((header_->num_bytes > target_size || test_mode_) && next.get()) {
    // The iterator could be invalidated within EvictEntry().
    if (!next->HasData())
      break;
    node.reset(next.release());
    next.reset(rankings_->GetPrev(node.get(), Rankings::NO_USE));
    if (node->Data()->dirty != backend_->GetCurrentEntryId() || empty) {
      // This entry is not being used by anybody.
      // Do NOT use node as an iterator after this point.
      rankings_->TrackRankingsBlock(node.get(), false);
      if (EvictEntry(node.get(), empty, Rankings::NO_USE) && !test_mode_)
        deleted_entries++;

      if (!empty && test_mode_)
        break;
    }
    if (!empty && (deleted_entries > 20 ||
                   (TimeTicks::Now() - start).InMilliseconds() > 20)) {
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::BindOnce(&Eviction::TrimCache,
                                    ptr_factory_.GetWeakPtr(), false));
      break;
    }
  }

  if (empty) {
    CACHE_UMA(AGE_MS, "TotalClearTimeV1", 0, start);
  } else {
    CACHE_UMA(AGE_MS, "TotalTrimTimeV1", 0, start);
  }
  CACHE_UMA(COUNTS, "TrimItemsV1", 0, deleted_entries);

  trimming_ = false;
  Trace("*** Trim Cache end ***");
  return;
}

void Eviction::UpdateRank(EntryImpl* entry, bool modified) {
  if (new_eviction_)
    return UpdateRankV2(entry, modified);

  rankings_->UpdateRank(entry->rankings(), modified, GetListForEntry(entry));
}

void Eviction::OnOpenEntry(EntryImpl* entry) {
  if (new_eviction_)
    return OnOpenEntryV2(entry);
}

void Eviction::OnCreateEntry(EntryImpl* entry) {
  if (new_eviction_)
    return OnCreateEntryV2(entry);

  rankings_->Insert(entry->rankings(), true, GetListForEntry(entry));
}

void Eviction::OnDoomEntry(EntryImpl* entry) {
  if (new_eviction_)
    return OnDoomEntryV2(entry);

  if (entry->LeaveRankingsBehind())
    return;

  rankings_->Remove(entry->rankings(), GetListForEntry(entry), true);
}

void Eviction::OnDestroyEntry(EntryImpl* entry) {
  if (new_eviction_)
    return OnDestroyEntryV2(entry);
}

void Eviction::SetTestMode() {
  test_mode_ = true;
}

void Eviction::TrimDeletedList(bool empty) {
  DCHECK(test_mode_ && new_eviction_);
  TrimDeleted(empty);
}

void Eviction::PostDelayedTrim() {
  // Prevent posting multiple tasks.
  if (delay_trim_)
    return;
  delay_trim_ = true;
  trim_delays_++;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&Eviction::DelayedTrim, ptr_factory_.GetWeakPtr()),
      base::TimeDelta::FromMilliseconds(1000));
}

void Eviction::DelayedTrim() {
  delay_trim_ = false;
  if (trim_delays_ < kMaxDelayedTrims && backend_->IsLoaded())
    return PostDelayedTrim();

  TrimCache(false);
}

bool Eviction::ShouldTrim() {
  if (!FallingBehind(header_->num_bytes, max_size_) &&
      trim_delays_ < kMaxDelayedTrims && backend_->IsLoaded()) {
    return false;
  }

  trim_delays_ = 0;
  return true;
}

bool Eviction::ShouldTrimDeleted() {
  int index_load = header_->num_entries * 100 / index_size_;

  // If the index is not loaded, the deleted list will tend to double the size
  // of the other lists 3 lists (40% of the total). Otherwise, all lists will be
  // about the same size.
  int max_length = (index_load < 25) ? header_->num_entries * 2 / 5 :
                                       header_->num_entries / 4;
  return (!test_mode_ && header_->lru.sizes[Rankings::DELETED] > max_length);
}

void Eviction::ReportTrimTimes(EntryImpl* entry) {
  if (first_trim_) {
    first_trim_ = false;
    if (backend_->ShouldReportAgain()) {
      CACHE_UMA(AGE, "TrimAge", 0, entry->GetLastUsed());
      ReportListStats();
    }

    if (header_->lru.filled)
      return;

    header_->lru.filled = 1;

    if (header_->create_time) {
      // This is the first entry that we have to evict, generate some noise.
      backend_->FirstEviction();
    } else {
      // This is an old file, but we may want more reports from this user so
      // lets save some create_time. Conversion cannot fail here.
      const base::Time time_2009_3_1 =
          base::Time::FromInternalValue(12985574400000000);
      header_->create_time = time_2009_3_1.ToInternalValue();
    }
  }
}

Rankings::List Eviction::GetListForEntry(EntryImpl* entry) {
  return Rankings::NO_USE;
}

bool Eviction::EvictEntry(CacheRankingsBlock* node, bool empty,
                          Rankings::List list) {
  scoped_refptr<EntryImpl> entry = backend_->GetEnumeratedEntry(node, list);
  if (!entry) {
    Trace("NewEntry failed on Trim 0x%x", node->address().value());
    return false;
  }

  ReportTrimTimes(entry.get());
  if (empty || !new_eviction_) {
    entry->DoomImpl();
  } else {
    entry->DeleteEntryData(false);
    EntryStore* info = entry->entry()->Data();
    DCHECK_EQ(ENTRY_NORMAL, info->state);

    rankings_->Remove(entry->rankings(), GetListForEntryV2(entry.get()), true);
    info->state = ENTRY_EVICTED;
    entry->entry()->Store();
    rankings_->Insert(entry->rankings(), true, Rankings::DELETED);
  }
  if (!empty)
    backend_->OnEvent(Stats::TRIM_ENTRY);

  return true;
}

// -----------------------------------------------------------------------

void Eviction::TrimCacheV2(bool empty) {
  Trace("*** Trim Cache ***");
  trimming_ = true;
  TimeTicks start = TimeTicks::Now();

  const int kListsToSearch = 3;
  Rankings::ScopedRankingsBlock next[kListsToSearch];
  int list = Rankings::LAST_ELEMENT;

  // Get a node from each list.
  bool done = false;
  for (int i = 0; i < kListsToSearch; i++) {
    next[i].set_rankings(rankings_);
    if (done)
      continue;
    next[i].reset(rankings_->GetPrev(nullptr, static_cast<Rankings::List>(i)));
    if (!empty && NodeIsOldEnough(next[i].get(), i)) {
      list = static_cast<Rankings::List>(i);
      done = true;
    }
  }

  // If we are not meeting the time targets lets move on to list length.
  if (!empty && Rankings::LAST_ELEMENT == list)
    list = SelectListByLength(next);

  if (empty)
    list = 0;

  Rankings::ScopedRankingsBlock node(rankings_);
  int deleted_entries = 0;
  int target_size = empty ? 0 : max_size_;

  for (; list < kListsToSearch; list++) {
    while ((header_->num_bytes > target_size || test_mode_) &&
           next[list].get()) {
      // The iterator could be invalidated within EvictEntry().
      if (!next[list]->HasData())
        break;
      node.reset(next[list].release());
      next[list].reset(rankings_->GetPrev(node.get(),
                                          static_cast<Rankings::List>(list)));
      if (node->Data()->dirty != backend_->GetCurrentEntryId() || empty) {
        // This entry is not being used by anybody.
        // Do NOT use node as an iterator after this point.
        rankings_->TrackRankingsBlock(node.get(), false);
        if (EvictEntry(node.get(), empty, static_cast<Rankings::List>(list)))
          deleted_entries++;

        if (!empty && test_mode_)
          break;
      }
      if (!empty && (deleted_entries > 20 ||
                     (TimeTicks::Now() - start).InMilliseconds() > 20)) {
        base::ThreadTaskRunnerHandle::Get()->PostTask(
            FROM_HERE, base::BindOnce(&Eviction::TrimCache,
                                      ptr_factory_.GetWeakPtr(), false));
        break;
      }
    }
    if (!empty)
      list = kListsToSearch;
  }

  if (empty) {
    TrimDeleted(true);
  } else if (ShouldTrimDeleted()) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&Eviction::TrimDeleted,
                                  ptr_factory_.GetWeakPtr(), empty));
  }

  if (empty) {
    CACHE_UMA(AGE_MS, "TotalClearTimeV2", 0, start);
  } else {
    CACHE_UMA(AGE_MS, "TotalTrimTimeV2", 0, start);
  }
  CACHE_UMA(COUNTS, "TrimItemsV2", 0, deleted_entries);

  Trace("*** Trim Cache end ***");
  trimming_ = false;
  return;
}

void Eviction::UpdateRankV2(EntryImpl* entry, bool modified) {
  rankings_->UpdateRank(entry->rankings(), modified, GetListForEntryV2(entry));
}

void Eviction::OnOpenEntryV2(EntryImpl* entry) {
  EntryStore* info = entry->entry()->Data();
  DCHECK_EQ(ENTRY_NORMAL, info->state);

  if (info->reuse_count < std::numeric_limits<int32_t>::max()) {
    info->reuse_count++;
    entry->entry()->set_modified();

    // We may need to move this to a new list.
    if (1 == info->reuse_count) {
      rankings_->Remove(entry->rankings(), Rankings::NO_USE, true);
      rankings_->Insert(entry->rankings(), false, Rankings::LOW_USE);
      entry->entry()->Store();
    } else if (kHighUse == info->reuse_count) {
      rankings_->Remove(entry->rankings(), Rankings::LOW_USE, true);
      rankings_->Insert(entry->rankings(), false, Rankings::HIGH_USE);
      entry->entry()->Store();
    }
  }
}

void Eviction::OnCreateEntryV2(EntryImpl* entry) {
  EntryStore* info = entry->entry()->Data();
  switch (info->state) {
    case ENTRY_NORMAL: {
      DCHECK(!info->reuse_count);
      DCHECK(!info->refetch_count);
      break;
    };
    case ENTRY_EVICTED: {
      if (info->refetch_count < std::numeric_limits<int32_t>::max())
        info->refetch_count++;

      if (info->refetch_count > kHighUse && info->reuse_count < kHighUse) {
        info->reuse_count = kHighUse;
      } else {
        info->reuse_count++;
      }
      info->state = ENTRY_NORMAL;
      entry->entry()->Store();
      rankings_->Remove(entry->rankings(), Rankings::DELETED, true);
      break;
    };
    default:
      NOTREACHED();
  }

  rankings_->Insert(entry->rankings(), true, GetListForEntryV2(entry));
}

void Eviction::OnDoomEntryV2(EntryImpl* entry) {
  EntryStore* info = entry->entry()->Data();
  if (ENTRY_NORMAL != info->state)
    return;

  if (entry->LeaveRankingsBehind()) {
    info->state = ENTRY_DOOMED;
    entry->entry()->Store();
    return;
  }

  rankings_->Remove(entry->rankings(), GetListForEntryV2(entry), true);

  info->state = ENTRY_DOOMED;
  entry->entry()->Store();
  rankings_->Insert(entry->rankings(), true, Rankings::DELETED);
}

void Eviction::OnDestroyEntryV2(EntryImpl* entry) {
  if (entry->LeaveRankingsBehind())
    return;

  rankings_->Remove(entry->rankings(), Rankings::DELETED, true);
}

Rankings::List Eviction::GetListForEntryV2(EntryImpl* entry) {
  EntryStore* info = entry->entry()->Data();
  DCHECK_EQ(ENTRY_NORMAL, info->state);

  if (!info->reuse_count)
    return Rankings::NO_USE;

  if (info->reuse_count < kHighUse)
    return Rankings::LOW_USE;

  return Rankings::HIGH_USE;
}

// This is a minimal implementation that just discards the oldest nodes.
// TODO(rvargas): Do something better here.
void Eviction::TrimDeleted(bool empty) {
  Trace("*** Trim Deleted ***");
  if (backend_->disabled_)
    return;

  TimeTicks start = TimeTicks::Now();
  Rankings::ScopedRankingsBlock node(rankings_);
  Rankings::ScopedRankingsBlock next(
    rankings_, rankings_->GetPrev(node.get(), Rankings::DELETED));
  int deleted_entries = 0;
  while (next.get() &&
         (empty || (deleted_entries < 20 &&
                    (TimeTicks::Now() - start).InMilliseconds() < 20))) {
    node.reset(next.release());
    next.reset(rankings_->GetPrev(node.get(), Rankings::DELETED));
    if (RemoveDeletedNode(node.get()))
      deleted_entries++;
    if (test_mode_)
      break;
  }

  if (deleted_entries && !empty && ShouldTrimDeleted()) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&Eviction::TrimDeleted,
                                  ptr_factory_.GetWeakPtr(), false));
  }

  CACHE_UMA(AGE_MS, "TotalTrimDeletedTime", 0, start);
  CACHE_UMA(COUNTS, "TrimDeletedItems", 0, deleted_entries);
  Trace("*** Trim Deleted end ***");
  return;
}

bool Eviction::RemoveDeletedNode(CacheRankingsBlock* node) {
  scoped_refptr<EntryImpl> entry =
      backend_->GetEnumeratedEntry(node, Rankings::DELETED);
  if (!entry) {
    Trace("NewEntry failed on Trim 0x%x", node->address().value());
    return false;
  }

  bool doomed = (entry->entry()->Data()->state == ENTRY_DOOMED);
  entry->entry()->Data()->state = ENTRY_DOOMED;
  entry->DoomImpl();
  return !doomed;
}

bool Eviction::NodeIsOldEnough(CacheRankingsBlock* node, int list) {
  if (!node)
    return false;

  // If possible, we want to keep entries on each list at least kTargetTime
  // hours. Each successive list on the enumeration has 2x the target time of
  // the previous list.
  Time used = Time::FromInternalValue(node->Data()->last_used);
  int multiplier = 1 << list;
  return (Time::Now() - used).InHours() > kTargetTime * multiplier;
}

int Eviction::SelectListByLength(Rankings::ScopedRankingsBlock* next) {
  int data_entries = header_->num_entries -
                     header_->lru.sizes[Rankings::DELETED];
  // Start by having each list to be roughly the same size.
  if (header_->lru.sizes[0] > data_entries / 3)
    return 0;

  int list = (header_->lru.sizes[1] > data_entries / 3) ? 1 : 2;

  // Make sure that frequently used items are kept for a minimum time; we know
  // that this entry is not older than its current target, but it must be at
  // least older than the target for list 0 (kTargetTime), as long as we don't
  // exhaust list 0.
  if (!NodeIsOldEnough(next[list].get(), 0) &&
      header_->lru.sizes[0] > data_entries / 10)
    list = 0;

  return list;
}

void Eviction::ReportListStats() {
  if (!new_eviction_)
    return;

  Rankings::ScopedRankingsBlock last1(
      rankings_, rankings_->GetPrev(nullptr, Rankings::NO_USE));
  Rankings::ScopedRankingsBlock last2(
      rankings_, rankings_->GetPrev(nullptr, Rankings::LOW_USE));
  Rankings::ScopedRankingsBlock last3(
      rankings_, rankings_->GetPrev(nullptr, Rankings::HIGH_USE));
  Rankings::ScopedRankingsBlock last4(
      rankings_, rankings_->GetPrev(nullptr, Rankings::DELETED));

  if (last1.get())
    CACHE_UMA(AGE, "NoUseAge", 0,
              Time::FromInternalValue(last1.get()->Data()->last_used));
  if (last2.get())
    CACHE_UMA(AGE, "LowUseAge", 0,
              Time::FromInternalValue(last2.get()->Data()->last_used));
  if (last3.get())
    CACHE_UMA(AGE, "HighUseAge", 0,
              Time::FromInternalValue(last3.get()->Data()->last_used));
  if (last4.get())
    CACHE_UMA(AGE, "DeletedAge", 0,
              Time::FromInternalValue(last4.get()->Data()->last_used));
}

}  // namespace disk_cache
