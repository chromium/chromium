// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_transaction_coordinator.h"

#include "base/logging.h"
#include "content/browser/indexed_db/indexed_db_tracing.h"
#include "content/browser/indexed_db/indexed_db_transaction.h"
#include "third_party/blink/public/common/indexeddb/web_idb_types.h"

namespace content {
namespace {

// Only this many transactions can be active at any time before they are queued.
// Limited to prevent transaction trashing which can consume a ton of RAM. Ten
// is chosen to reduce performance regressions.
// TODO(dmurph): crbug.com/693260 Create better scheduling or limits.
static const size_t kMaxStartedTransactions = 10;

}  // namespace

IndexedDBTransactionCoordinator::IndexedDBTransactionCoordinator() {}

IndexedDBTransactionCoordinator::~IndexedDBTransactionCoordinator() {
  DCHECK(queued_transactions_.empty());
  DCHECK(started_transactions_.empty());
}

void IndexedDBTransactionCoordinator::DidCreateTransaction(
    IndexedDBTransaction* transaction) {
  DCHECK(!queued_transactions_.count(transaction));
  DCHECK(!started_transactions_.count(transaction));
  DCHECK_EQ(IndexedDBTransaction::CREATED, transaction->state());

  queued_transactions_.insert(transaction);
  ProcessQueuedTransactions();
}

// Observer transactions jump to the front of the queue.
void IndexedDBTransactionCoordinator::DidCreateObserverTransaction(
    IndexedDBTransaction* transaction) {
  DCHECK(!queued_transactions_.count(transaction));
  DCHECK(!started_transactions_.count(transaction));
  DCHECK_EQ(IndexedDBTransaction::CREATED, transaction->state());

  started_transactions_.insert_front(transaction);
  ProcessQueuedTransactions();
}

void IndexedDBTransactionCoordinator::DidFinishTransaction(
    IndexedDBTransaction* transaction) {
  if (queued_transactions_.count(transaction)) {
    DCHECK(!started_transactions_.count(transaction));
    queued_transactions_.erase(transaction);
  } else {
    DCHECK(started_transactions_.count(transaction));
    started_transactions_.erase(transaction);
  }

  ProcessQueuedTransactions();
}

bool IndexedDBTransactionCoordinator::IsRunningVersionChangeTransaction()
    const {
  return !started_transactions_.empty() &&
         (*started_transactions_.begin())->mode() ==
             blink::kWebIDBTransactionModeVersionChange;
}

#ifndef NDEBUG
// Verifies internal consistency while returning whether anything is found.
bool IndexedDBTransactionCoordinator::IsActive(
    IndexedDBTransaction* transaction) {
  bool found = false;
  if (queued_transactions_.count(transaction))
    found = true;
  if (started_transactions_.count(transaction)) {
    DCHECK(!found);
    found = true;
  }
  return found;
}
#endif

std::vector<const IndexedDBTransaction*>
IndexedDBTransactionCoordinator::GetTransactions() const {
  std::vector<const IndexedDBTransaction*> result;
  result.reserve(started_transactions_.size() + queued_transactions_.size());
  result.insert(result.end(), started_transactions_.begin(),
                started_transactions_.end());
  result.insert(result.end(), queued_transactions_.begin(),
                queued_transactions_.end());
  return result;
}

void IndexedDBTransactionCoordinator::RecordMetrics() const {
  IDB_TRACE_COUNTER2("IndexedDBTransactionCoordinator", "StartedTransactions",
                     started_transactions_.size(), "QueuedTransactions",
                     queued_transactions_.size());
}

void IndexedDBTransactionCoordinator::ProcessQueuedTransactions() {
  if (queued_transactions_.empty())
    return;

  DCHECK(!IsRunningVersionChangeTransaction());

  // The locked_scope set accumulates the ids of object stores in the scope of
  // running read/write transactions. Other read-write transactions with
  // stores in this set may not be started. Read-only transactions may start,
  // taking a snapshot of the database, which does not include uncommitted
  // data. ("Version change" transactions are exclusive, but handled by the
  // connection sequencing in IndexedDBDatabase.)
  std::set<int64_t> locked_scope;
  for (auto* transaction : started_transactions_) {
    if (transaction->mode() == blink::kWebIDBTransactionModeReadWrite) {
      // Started read/write transactions have exclusive access to the object
      // stores within their scopes.
      locked_scope.insert(transaction->scope().begin(),
                          transaction->scope().end());
    }
  }

  auto it = queued_transactions_.begin();
  while (it != queued_transactions_.end()) {
    IndexedDBTransaction* transaction = *it;
    ++it;
    if (CanStartTransaction(transaction, locked_scope)) {
      DCHECK_EQ(IndexedDBTransaction::CREATED, transaction->state());
      queued_transactions_.erase(transaction);
      started_transactions_.insert(transaction);
      transaction->Start();
      DCHECK_EQ(IndexedDBTransaction::STARTED, transaction->state());
    }
    if (transaction->mode() == blink::kWebIDBTransactionModeReadWrite) {
      // Either the transaction started, so it has exclusive access to the
      // stores in its scope, or per the spec the transaction which was
      // created first must get access first, so the stores are also locked.
      locked_scope.insert(transaction->scope().begin(),
                          transaction->scope().end());
    }
  }
  RecordMetrics();
}

template<typename T>
static bool DoSetsIntersect(const std::set<T>& set1,
                            const std::set<T>& set2) {
  auto it1 = set1.begin();
  auto it2 = set2.begin();
  while (it1 != set1.end() && it2 != set2.end()) {
    if (*it1 < *it2)
      ++it1;
    else if (*it2 < *it1)
      ++it2;
    else
      return true;
  }
  return false;
}

bool IndexedDBTransactionCoordinator::CanStartTransaction(
    IndexedDBTransaction* const transaction,
    const std::set<int64_t>& locked_scope) const {
  if (started_transactions_.size() >= kMaxStartedTransactions) {
    return false;
  }
  DCHECK(queued_transactions_.count(transaction));
  switch (transaction->mode()) {
    case blink::kWebIDBTransactionModeVersionChange:
      DCHECK_EQ(1u, queued_transactions_.size());
      DCHECK(started_transactions_.empty());
      DCHECK(locked_scope.empty());
      return true;

    case blink::kWebIDBTransactionModeReadOnly:
    case blink::kWebIDBTransactionModeReadWrite:
      return !DoSetsIntersect(transaction->scope(), locked_scope);
  }
  NOTREACHED();
  return false;
}

}  // namespace content
