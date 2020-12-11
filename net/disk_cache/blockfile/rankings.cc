// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/blockfile/rankings.h"

#include <stdint.h>

#include <limits>

#include "base/macros.h"
#include "base/process/process.h"
#include "build/build_config.h"
#include "net/base/net_export.h"
#include "net/disk_cache/blockfile/backend_impl.h"
#include "net/disk_cache/blockfile/disk_format.h"
#include "net/disk_cache/blockfile/entry_impl.h"
#include "net/disk_cache/blockfile/errors.h"
#include "net/disk_cache/blockfile/histogram_macros.h"
#include "net/disk_cache/blockfile/stress_support.h"

#if defined(OS_WIN)
#include <windows.h>
#endif

// Provide a BackendImpl object to macros from histogram_macros.h.
#define CACHE_UMA_BACKEND_IMPL_OBJ backend_

using base::Time;
using base::TimeTicks;

namespace disk_cache {
// This is used by crash_cache.exe to generate unit test files.
NET_EXPORT_PRIVATE RankCrashes g_rankings_crash = NO_CRASH;
}

namespace {

enum Operation {
  INSERT = 1,
  REMOVE
};

// This class provides a simple lock for the LRU list of rankings. Whenever an
// entry is to be inserted or removed from the list, a transaction object should
// be created to keep track of the operation. If the process crashes before
// finishing the operation, the transaction record (stored as part of the user
// data on the file header) can be used to finish the operation.
class Transaction {
 public:
  // addr is the cache address of the node being inserted or removed. We want to
  // avoid having the compiler doing optimizations on when to read or write
  // from user_data because it is the basis of the crash detection. Maybe
  // volatile is not enough for that, but it should be a good hint.
  Transaction(volatile disk_cache::LruData* data, disk_cache::Addr addr,
              Operation op, int list);
  ~Transaction();
 private:
  volatile disk_cache::LruData* data_;
  DISALLOW_COPY_AND_ASSIGN(Transaction);
};

Transaction::Transaction(volatile disk_cache::LruData* data,
                         disk_cache::Addr addr, Operation op, int list)
    : data_(data) {
  DCHECK(!data_->transaction);
  DCHECK(addr.is_initialized());
  data_->operation = op;
  data_->operation_list = list;
  data_->transaction = addr.value();
}

Transaction::~Transaction() {
  DCHECK(data_->transaction);
  data_->transaction = 0;
  data_->operation = 0;
  data_->operation_list = 0;
}

// Code locations that can generate crashes.
enum CrashLocation {
  ON_INSERT_1, ON_INSERT_2, ON_INSERT_3, ON_INSERT_4, ON_REMOVE_1, ON_REMOVE_2,
  ON_REMOVE_3, ON_REMOVE_4, ON_REMOVE_5, ON_REMOVE_6, ON_REMOVE_7, ON_REMOVE_8
};

// Simulates a crash (by exiting the process without graceful shutdown) on debug
// builds, according to the value of g_rankings_crash. This used by
// crash_cache.exe to generate unit-test files.
void GenerateCrash(CrashLocation location) {
#if !defined(NDEBUG) && !defined(OS_IOS)
  if (disk_cache::NO_CRASH == disk_cache::g_rankings_crash)
    return;
  switch (location) {
    case ON_INSERT_1:
      switch (disk_cache::g_rankings_crash) {
        case disk_cache::INSERT_ONE_1:
        case disk_cache::INSERT_LOAD_1:
          base::Process::TerminateCurrentProcessImmediately(0);
        default:
          break;
      }
      break;
    case ON_INSERT_2:
      if (disk_cache::INSERT_EMPTY_1 == disk_cache::g_rankings_crash)
        base::Process::TerminateCurrentProcessImmediately(0);
      break;
    case ON_INSERT_3:
      switch (disk_cache::g_rankings_crash) {
        case disk_cache::INSERT_EMPTY_2:
        case disk_cache::INSERT_ONE_2:
        case disk_cache::INSERT_LOAD_2:
          base::Process::TerminateCurrentProcessImmediately(0);
        default:
          break;
      }
      break;
    case ON_INSERT_4:
      switch (disk_cache::g_rankings_crash) {
        case disk_cache::INSERT_EMPTY_3:
        case disk_cache::INSERT_ONE_3:
          base::Process::TerminateCurrentProcessImmediately(0);
        default:
          break;
      }
      break;
    case ON_REMOVE_1:
      switch (disk_cache::g_rankings_crash) {
        case disk_cache::REMOVE_ONE_1:
        case disk_cache::REMOVE_HEAD_1:
        case disk_cache::REMOVE_TAIL_1:
        case disk_cache::REMOVE_LOAD_1:
          base::Process::TerminateCurrentProcessImmediately(0);
        default:
          break;
      }
      break;
    case ON_REMOVE_2:
      if (disk_cache::REMOVE_ONE_2 == disk_cache::g_rankings_crash)
        base::Process::TerminateCurrentProcessImmediately(0);
      break;
    case ON_REMOVE_3:
      if (disk_cache::REMOVE_ONE_3 == disk_cache::g_rankings_crash)
        base::Process::TerminateCurrentProcessImmediately(0);
      break;
    case ON_REMOVE_4:
      if (disk_cache::REMOVE_HEAD_2 == disk_cache::g_rankings_crash)
        base::Process::TerminateCurrentProcessImmediately(0);
      break;
    case ON_REMOVE_5:
      if (disk_cache::REMOVE_TAIL_2 == disk_cache::g_rankings_crash)
        base::Process::TerminateCurrentProcessImmediately(0);
      break;
    case ON_REMOVE_6:
      if (disk_cache::REMOVE_TAIL_3 == disk_cache::g_rankings_crash)
        base::Process::TerminateCurrentProcessImmediately(0);
      break;
    case ON_REMOVE_7:
      switch (disk_cache::g_rankings_crash) {
        case disk_cache::REMOVE_ONE_4:
        case disk_cache::REMOVE_LOAD_2:
        case disk_cache::REMOVE_HEAD_3:
          base::Process::TerminateCurrentProcessImmediately(0);
        default:
          break;
      }
      break;
    case ON_REMOVE_8:
      switch (disk_cache::g_rankings_crash) {
        case disk_cache::REMOVE_HEAD_4:
        case disk_cache::REMOVE_LOAD_3:
          base::Process::TerminateCurrentProcessImmediately(0);
        default:
          break;
      }
      break;
    default:
      NOTREACHED();
      return;
  }
#endif  // NDEBUG
}

// Update the timestamp fields of |node|.
void UpdateTimes(disk_cache::CacheRankingsBlock* node, bool modified) {
  base::Time now = base::Time::Now();
  node->Data()->last_used = now.ToInternalValue();
  if (modified)
    node->Data()->last_modified = now.ToInternalValue();
}

}  // namespace

namespace disk_cache {

Rankings::ScopedRankingsBlock::ScopedRankingsBlock() : rankings_(nullptr) {}

Rankings::ScopedRankingsBlock::ScopedRankingsBlock(Rankings* rankings)
    : rankings_(rankings) {}

Rankings::ScopedRankingsBlock::ScopedRankingsBlock(Rankings* rankings,
                                                   CacheRankingsBlock* node)
    : std::unique_ptr<CacheRankingsBlock>(node), rankings_(rankings) {}

Rankings::Iterator::Iterator() {
  memset(this, 0, sizeof(Iterator));
}

void Rankings::Iterator::Reset() {
  if (my_rankings) {
    for (int i = 0; i < 3; i++)
      ScopedRankingsBlock(my_rankings, nodes[i]);
  }
  memset(this, 0, sizeof(Iterator));
}

Rankings::Rankings() : init_(false) {}

Rankings::~Rankings() = default;

bool Rankings::Init(BackendImpl* backend, bool count_lists) {
  DCHECK(!init_);
  if (init_)
    return false;

  backend_ = backend;
  control_data_ = backend_->GetLruData();
  count_lists_ = count_lists;

  ReadHeads();
  ReadTails();

  if (control_data_->transaction)
    CompleteTransaction();

  init_ = true;
  return true;
}

void Rankings::Reset() {
  init_ = false;
  for (int i = 0; i < LAST_ELEMENT; i++) {
    heads_[i].set_value(0);
    tails_[i].set_value(0);
  }
  control_data_ = nullptr;
}

void Rankings::Insert(CacheRankingsBlock* node, bool modified, List list) {
  DCHECK(node->HasData());
  Addr& my_head = heads_[list];
  Addr& my_tail = tails_[list];
  Transaction lock(control_data_, node->address(), INSERT, list);
  CacheRankingsBlock head(backend_->File(my_head), my_head);
  if (my_head.is_initialized()) {
    if (!GetRanking(&head))
      return;

    if (head.Data()->prev != my_head.value() &&  // Normal path.
        head.Data()->prev != node->address().value()) {  // FinishInsert().
      backend_->CriticalError(ERR_INVALID_LINKS);
      return;
    }

    head.Data()->prev = node->address().value();
    head.Store();
    GenerateCrash(ON_INSERT_1);
    UpdateIterators(&head);
  }

  node->Data()->next = my_head.value();
  node->Data()->prev = node->address().value();
  my_head.set_value(node->address().value());

  if (!my_tail.is_initialized() || my_tail.value() == node->address().value()) {
    my_tail.set_value(node->address().value());
    node->Data()->next = my_tail.value();
    WriteTail(list);
    GenerateCrash(ON_INSERT_2);
  }

  UpdateTimes(node, modified);
  node->Store();
  GenerateCrash(ON_INSERT_3);

  // The last thing to do is move our head to point to a node already stored.
  WriteHead(list);
  IncrementCounter(list);
  GenerateCrash(ON_INSERT_4);
  backend_->FlushIndex();
}

// If a, b and r are elements on the list, and we want to remove r, the possible
// states for the objects if a crash happens are (where y(x, z) means for object
// y, prev is x and next is z):
// A. One element:
//    1. r(r, r), head(r), tail(r)                    initial state
//    2. r(r, r), head(0), tail(r)                    WriteHead()
//    3. r(r, r), head(0), tail(0)                    WriteTail()
//    4. r(0, 0), head(0), tail(0)                    next.Store()
//
// B. Remove a random element:
//    1. a(x, r), r(a, b), b(r, y), head(x), tail(y)  initial state
//    2. a(x, r), r(a, b), b(a, y), head(x), tail(y)  next.Store()
//    3. a(x, b), r(a, b), b(a, y), head(x), tail(y)  prev.Store()
//    4. a(x, b), r(0, 0), b(a, y), head(x), tail(y)  node.Store()
//
// C. Remove head:
//    1. r(r, b), b(r, y), head(r), tail(y)           initial state
//    2. r(r, b), b(r, y), head(b), tail(y)           WriteHead()
//    3. r(r, b), b(b, y), head(b), tail(y)           next.Store()
//    4. r(0, 0), b(b, y), head(b), tail(y)           prev.Store()
//
// D. Remove tail:
//    1. a(x, r), r(a, r), head(x), tail(r)           initial state
//    2. a(x, r), r(a, r), head(x), tail(a)           WriteTail()
//    3. a(x, a), r(a, r), head(x), tail(a)           prev.Store()
//    4. a(x, a), r(0, 0), head(x), tail(a)           next.Store()
void Rankings::Remove(CacheRankingsBlock* node, List list, bool strict) {
  DCHECK(node->HasData());

  Addr next_addr(node->Data()->next);
  Addr prev_addr(node->Data()->prev);
  if (!next_addr.is_initialized() || next_addr.is_separate_file() ||
      !prev_addr.is_initialized() || prev_addr.is_separate_file()) {
    if (next_addr.is_initialized() || prev_addr.is_initialized()) {
      LOG(ERROR) << "Invalid rankings info.";
      STRESS_NOTREACHED();
    }
    return;
  }

  CacheRankingsBlock next(backend_->File(next_addr), next_addr);
  CacheRankingsBlock prev(backend_->File(prev_addr), prev_addr);
  if (!GetRanking(&next) || !GetRanking(&prev)) {
    STRESS_NOTREACHED();
    return;
  }

  if (!CheckLinks(node, &prev, &next, &list))
    return;

  Transaction lock(control_data_, node->address(), REMOVE, list);
  prev.Data()->next = next.address().value();
  next.Data()->prev = prev.address().value();
  GenerateCrash(ON_REMOVE_1);

  CacheAddr node_value = node->address().value();
  Addr& my_head = heads_[list];
  Addr& my_tail = tails_[list];
  if (node_value == my_head.value() || node_value == my_tail.value()) {
    if (my_head.value() == my_tail.value()) {
      my_head.set_value(0);
      my_tail.set_value(0);

      WriteHead(list);
      GenerateCrash(ON_REMOVE_2);
      WriteTail(list);
      GenerateCrash(ON_REMOVE_3);
    } else if (node_value == my_head.value()) {
      my_head.set_value(next.address().value());
      next.Data()->prev = next.address().value();

      WriteHead(list);
      GenerateCrash(ON_REMOVE_4);
    } else if (node_value == my_tail.value()) {
      my_tail.set_value(prev.address().value());
      prev.Data()->next = prev.address().value();

      WriteTail(list);
      GenerateCrash(ON_REMOVE_5);

      // Store the new tail to make sure we can undo the operation if we crash.
      prev.Store();
      GenerateCrash(ON_REMOVE_6);
    }
  }

  // Nodes out of the list can be identified by invalid pointers.
  node->Data()->next = 0;
  node->Data()->prev = 0;

  // The last thing to get to disk is the node itself, so before that there is
  // enough info to recover.
  next.Store();
  GenerateCrash(ON_REMOVE_7);
  prev.Store();
  GenerateCrash(ON_REMOVE_8);
  node->Store();
  DecrementCounter(list);
  if (strict)
    UpdateIteratorsForRemoved(node_value, &next);

  UpdateIterators(&next);
  UpdateIterators(&prev);
  backend_->FlushIndex();
}

// A crash in between Remove and Insert will lead to a dirty entry not on the
// list. We want to avoid that case as much as we can (as while waiting for IO),
// but the net effect is just an assert on debug when attempting to remove the
// entry. Otherwise we'll need reentrant transactions, which is an overkill.
void Rankings::UpdateRank(CacheRankingsBlock* node, bool modified, List list) {
  Addr& my_head = heads_[list];
  if (my_head.value() == node->address().value()) {
    UpdateTimes(node, modified);
    node->set_modified();
    return;
  }

  TimeTicks start = TimeTicks::Now();
  Remove(node, list, true);
  Insert(node, modified, list);
  CACHE_UMA(AGE_MS, "UpdateRank", 0, start);
}

CacheRankingsBlock* Rankings::GetNext(CacheRankingsBlock* node, List list) {
  ScopedRankingsBlock next(this);
  if (!node) {
    Addr& my_head = heads_[list];
    if (!my_head.is_initialized())
      return nullptr;
    next.reset(new CacheRankingsBlock(backend_->File(my_head), my_head));
  } else {
    if (!node->HasData())
      node->Load();
    Addr& my_tail = tails_[list];
    if (!my_tail.is_initialized())
      return nullptr;
    if (my_tail.value() == node->address().value())
      return nullptr;
    Addr address(node->Data()->next);
    if (address.value() == node->address().value())
      return nullptr;  // Another tail? fail it.
    next.reset(new CacheRankingsBlock(backend_->File(address), address));
  }

  TrackRankingsBlock(next.get(), true);

  if (!GetRanking(next.get()))
    return nullptr;

  ConvertToLongLived(next.get());
  if (node && !CheckSingleLink(node, next.get()))
    return nullptr;

  return next.release();
}

CacheRankingsBlock* Rankings::GetPrev(CacheRankingsBlock* node, List list) {
  ScopedRankingsBlock prev(this);
  if (!node) {
    Addr& my_tail = tails_[list];
    if (!my_tail.is_initialized())
      return nullptr;
    prev.reset(new CacheRankingsBlock(backend_->File(my_tail), my_tail));
  } else {
    if (!node->HasData())
      node->Load();
    Addr& my_head = heads_[list];
    if (!my_head.is_initialized())
      return nullptr;
    if (my_head.value() == node->address().value())
      return nullptr;
    Addr address(node->Data()->prev);
    if (address.value() == node->address().value())
      return nullptr;  // Another head? fail it.
    prev.reset(new CacheRankingsBlock(backend_->File(address), address));
  }

  TrackRankingsBlock(prev.get(), true);

  if (!GetRanking(prev.get()))
    return nullptr;

  ConvertToLongLived(prev.get());
  if (node && !CheckSingleLink(prev.get(), node))
    return nullptr;

  return prev.release();
}

void Rankings::FreeRankingsBlock(CacheRankingsBlock* node) {
  TrackRankingsBlock(node, false);
}

void Rankings::TrackRankingsBlock(CacheRankingsBlock* node,
                                  bool start_tracking) {
  if (!node)
    return;

  IteratorPair current(node->address().value(), node);

  if (start_tracking)
    iterators_.push_back(current);
  else
    iterators_.remove(current);
}

int Rankings::SelfCheck() {
  int total = 0;
  int error = 0;
  for (int i = 0; i < LAST_ELEMENT; i++) {
    int partial = CheckList(static_cast<List>(i));
    if (partial < 0 && !error)
      error = partial;
    else if (partial > 0)
      total += partial;
  }

  return error ? error : total;
}

bool Rankings::SanityCheck(CacheRankingsBlock* node, bool from_list) const {
  if (!node->VerifyHash())
    return false;

  const RankingsNode* data = node->Data();

  if ((!data->next && data->prev) || (data->next && !data->prev))
    return false;

  // Both pointers on zero is a node out of the list.
  if (!data->next && !data->prev && from_list)
    return false;

  List list = NO_USE;  // Initialize it to something.
  if ((node->address().value() == data->prev) && !IsHead(data->prev, &list))
    return false;

  if ((node->address().value() == data->next) && !IsTail(data->next, &list))
    return false;

  if (!data->next && !data->prev)
    return true;

  Addr next_addr(data->next);
  Addr prev_addr(data->prev);
  if (!next_addr.SanityCheck() || next_addr.file_type() != RANKINGS ||
      !prev_addr.SanityCheck() || prev_addr.file_type() != RANKINGS)
    return false;

  return true;
}

bool Rankings::DataSanityCheck(CacheRankingsBlock* node, bool from_list) const {
  const RankingsNode* data = node->Data();
  if (!data->contents)
    return false;

  // It may have never been inserted.
  if (from_list && (!data->last_used || !data->last_modified))
    return false;

  return true;
}

void Rankings::SetContents(CacheRankingsBlock* node, CacheAddr address) {
  node->Data()->contents = address;
  node->Store();
}

void Rankings::ReadHeads() {
  for (int i = 0; i < LAST_ELEMENT; i++)
    heads_[i] = Addr(control_data_->heads[i]);
}

void Rankings::ReadTails() {
  for (int i = 0; i < LAST_ELEMENT; i++)
    tails_[i] = Addr(control_data_->tails[i]);
}

void Rankings::WriteHead(List list) {
  control_data_->heads[list] = heads_[list].value();
}

void Rankings::WriteTail(List list) {
  control_data_->tails[list] = tails_[list].value();
}

bool Rankings::GetRanking(CacheRankingsBlock* rankings) {
  if (!rankings->address().is_initialized())
    return false;

  TimeTicks start = TimeTicks::Now();
  if (!rankings->Load())
    return false;

  if (!SanityCheck(rankings, true)) {
    backend_->CriticalError(ERR_INVALID_LINKS);
    return false;
  }

  backend_->OnEvent(Stats::OPEN_RANKINGS);

  // Note that if the cache is in read_only mode, open entries are not marked
  // as dirty, except when an entry is doomed. We have to look for open entries.
  if (!backend_->read_only() && !rankings->Data()->dirty)
    return true;

  EntryImpl* entry = backend_->GetOpenEntry(rankings);
  if (!entry) {
    if (backend_->read_only())
      return true;

    // We cannot trust this entry, but we cannot initiate a cleanup from this
    // point (we may be in the middle of a cleanup already). The entry will be
    // deleted when detected from a regular open/create path.
    rankings->Data()->dirty = backend_->GetCurrentEntryId() - 1;
    if (!rankings->Data()->dirty)
      rankings->Data()->dirty--;
    return true;
  }

  // Note that we should not leave this module without deleting rankings first.
  rankings->SetData(entry->rankings()->Data());

  CACHE_UMA(AGE_MS, "GetRankings", 0, start);
  return true;
}

void Rankings::ConvertToLongLived(CacheRankingsBlock* rankings) {
  if (rankings->own_data())
    return;

  // We cannot return a shared node because we are not keeping a reference
  // to the entry that owns the buffer. Make this node a copy of the one that
  // we have, and let the iterator logic update it when the entry changes.
  CacheRankingsBlock temp(nullptr, Addr(0));
  *temp.Data() = *rankings->Data();
  rankings->StopSharingData();
  *rankings->Data() = *temp.Data();
}

void Rankings::CompleteTransaction() {
  Addr node_addr(static_cast<CacheAddr>(control_data_->transaction));
  if (!node_addr.is_initialized() || node_addr.is_separate_file()) {
    NOTREACHED();
    LOG(ERROR) << "Invalid rankings info.";
    return;
  }

  CacheRankingsBlock node(backend_->File(node_addr), node_addr);
  if (!node.Load())
    return;

  node.Store();

  // We want to leave the node inside the list. The entry must me marked as
  // dirty, and will be removed later. Otherwise, we'll get assertions when
  // attempting to remove the dirty entry.
  if (INSERT == control_data_->operation) {
    FinishInsert(&node);
  } else if (REMOVE == control_data_->operation) {
    RevertRemove(&node);
  } else {
    NOTREACHED();
    LOG(ERROR) << "Invalid operation to recover.";
  }
}

void Rankings::FinishInsert(CacheRankingsBlock* node) {
  control_data_->transaction = 0;
  control_data_->operation = 0;
  Addr& my_head = heads_[control_data_->operation_list];
  Addr& my_tail = tails_[control_data_->operation_list];
  if (my_head.value() != node->address().value()) {
    if (my_tail.value() == node->address().value()) {
      // This part will be skipped by the logic of Insert.
      node->Data()->next = my_tail.value();
    }

    Insert(node, true, static_cast<List>(control_data_->operation_list));
  }

  // Tell the backend about this entry.
  backend_->RecoveredEntry(node);
}

void Rankings::RevertRemove(CacheRankingsBlock* node) {
  Addr next_addr(node->Data()->next);
  Addr prev_addr(node->Data()->prev);
  if (!next_addr.is_initialized() || !prev_addr.is_initialized()) {
    // The operation actually finished. Nothing to do.
    control_data_->transaction = 0;
    return;
  }
  if (next_addr.is_separate_file() || prev_addr.is_separate_file()) {
    NOTREACHED();
    LOG(WARNING) << "Invalid rankings info.";
    control_data_->transaction = 0;
    return;
  }

  CacheRankingsBlock next(backend_->File(next_addr), next_addr);
  CacheRankingsBlock prev(backend_->File(prev_addr), prev_addr);
  if (!next.Load() || !prev.Load())
    return;

  CacheAddr node_value = node->address().value();
  DCHECK(prev.Data()->next == node_value ||
         prev.Data()->next == prev_addr.value() ||
         prev.Data()->next == next.address().value());
  DCHECK(next.Data()->prev == node_value ||
         next.Data()->prev == next_addr.value() ||
         next.Data()->prev == prev.address().value());

  if (node_value != prev_addr.value())
    prev.Data()->next = node_value;
  if (node_value != next_addr.value())
    next.Data()->prev = node_value;

  List my_list = static_cast<List>(control_data_->operation_list);
  Addr& my_head = heads_[my_list];
  Addr& my_tail = tails_[my_list];
  if (!my_head.is_initialized() || !my_tail.is_initialized()) {
    my_head.set_value(node_value);
    my_tail.set_value(node_value);
    WriteHead(my_list);
    WriteTail(my_list);
  } else if (my_head.value() == next.address().value()) {
    my_head.set_value(node_value);
    prev.Data()->next = next.address().value();
    WriteHead(my_list);
  } else if (my_tail.value() == prev.address().value()) {
    my_tail.set_value(node_value);
    next.Data()->prev = prev.address().value();
    WriteTail(my_list);
  }

  next.Store();
  prev.Store();
  control_data_->transaction = 0;
  control_data_->operation = 0;
  backend_->FlushIndex();
}

bool Rankings::CheckLinks(CacheRankingsBlock* node, CacheRankingsBlock* prev,
                          CacheRankingsBlock* next, List* list) {
  CacheAddr node_addr = node->address().value();
  if (prev->Data()->next == node_addr &&
      next->Data()->prev == node_addr) {
    // A regular linked node.
    return true;
  }

  if (node_addr != prev->address().value() &&
      node_addr != next->address().value() &&
      prev->Data()->next == next->address().value() &&
      next->Data()->prev == prev->address().value()) {
    // The list is actually ok, node is wrong.
    node->Data()->next = 0;
    node->Data()->prev = 0;
    node->Store();
    return false;
  }

  if (prev->Data()->next == node_addr ||
      next->Data()->prev == node_addr) {
    // Only one link is weird, lets double check.
    if (prev->Data()->next != node_addr && IsHead(node_addr, list))
      return true;

    if (next->Data()->prev != node_addr && IsTail(node_addr, list))
      return true;
  }

  LOG(ERROR) << "Inconsistent LRU.";
  STRESS_NOTREACHED();

  backend_->CriticalError(ERR_INVALID_LINKS);
  return false;
}

bool Rankings::CheckSingleLink(CacheRankingsBlock* prev,
                               CacheRankingsBlock* next) {
  if (prev->Data()->next != next->address().value() ||
      next->Data()->prev != prev->address().value()) {
    LOG(ERROR) << "Inconsistent LRU.";

    backend_->CriticalError(ERR_INVALID_LINKS);
    return false;
  }

  return true;
}

int Rankings::CheckList(List list) {
  Addr last1, last2;
  int head_items;
  int rv = CheckListSection(list, last1, last2, true,  // Head to tail.
                            &last1, &last2, &head_items);
  if (rv == ERR_NO_ERROR)
    return head_items;

  return rv;
}

// Note that the returned error codes assume a forward walk (from head to tail)
// so they have to be adjusted accordingly by the caller. We use two stop values
// to be able to detect a corrupt node at the end that is not linked going back.
int Rankings::CheckListSection(List list, Addr end1, Addr end2, bool forward,
                               Addr* last, Addr* second_last, int* num_items) {
  Addr current = forward ? heads_[list] : tails_[list];
  *last = *second_last = current;
  *num_items = 0;
  if (!current.is_initialized())
    return ERR_NO_ERROR;

  if (!current.SanityCheckForRankings())
    return ERR_INVALID_HEAD;

  std::unique_ptr<CacheRankingsBlock> node;
  Addr prev_addr(current);
  do {
    node.reset(new CacheRankingsBlock(backend_->File(current), current));
    node->Load();
    if (!SanityCheck(node.get(), true))
      return ERR_INVALID_ENTRY;

    CacheAddr next = forward ? node->Data()->next : node->Data()->prev;
    CacheAddr prev = forward ? node->Data()->prev : node->Data()->next;

    if (prev != prev_addr.value())
      return ERR_INVALID_PREV;

    Addr next_addr(next);
    if (!next_addr.SanityCheckForRankings())
      return ERR_INVALID_NEXT;

    prev_addr = current;
    current = next_addr;
    *second_last = *last;
    *last = current;
    (*num_items)++;

    if (next_addr == prev_addr) {
      Addr last = forward ? tails_[list] : heads_[list];
      if (next_addr == last)
        return ERR_NO_ERROR;
      return ERR_INVALID_TAIL;
    }
  } while (current != end1 && current != end2);
  return ERR_NO_ERROR;
}

bool Rankings::IsHead(CacheAddr addr, List* list) const {
  for (int i = 0; i < LAST_ELEMENT; i++) {
    if (addr == heads_[i].value()) {
      *list = static_cast<List>(i);
      return true;
    }
  }
  return false;
}

bool Rankings::IsTail(CacheAddr addr, List* list) const {
  for (int i = 0; i < LAST_ELEMENT; i++) {
    if (addr == tails_[i].value()) {
      *list = static_cast<List>(i);
      return true;
    }
  }
  return false;
}

// We expect to have just a few iterators at any given time, maybe two or three,
// But we could have more than one pointing at the same mode. We walk the list
// of cache iterators and update all that are pointing to the given node.
void Rankings::UpdateIterators(CacheRankingsBlock* node) {
  CacheAddr address = node->address().value();
  for (auto it = iterators_.begin(); it != iterators_.end(); ++it) {
    if (it->first == address && it->second->HasData()) {
      CacheRankingsBlock* other = it->second;
      *other->Data() = *node->Data();
    }
  }
}

void Rankings::UpdateIteratorsForRemoved(CacheAddr address,
                                         CacheRankingsBlock* next) {
  CacheAddr next_addr = next->address().value();
  for (auto it = iterators_.begin(); it != iterators_.end(); ++it) {
    if (it->first == address) {
      it->first = next_addr;
      it->second->CopyFrom(next);
    }
  }
}

void Rankings::IncrementCounter(List list) {
  if (!count_lists_)
    return;

  DCHECK(control_data_->sizes[list] < std::numeric_limits<int32_t>::max());
  if (control_data_->sizes[list] < std::numeric_limits<int32_t>::max())
    control_data_->sizes[list]++;
}

void Rankings::DecrementCounter(List list) {
  if (!count_lists_)
    return;

  DCHECK(control_data_->sizes[list] > 0);
  if (control_data_->sizes[list] > 0)
    control_data_->sizes[list]--;
}

}  // namespace disk_cache
