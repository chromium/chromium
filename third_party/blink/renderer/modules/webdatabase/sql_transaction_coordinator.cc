/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 * Copyright (C) 2013 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/modules/webdatabase/sql_transaction_coordinator.h"

#include "third_party/blink/renderer/modules/webdatabase/database.h"
#include "third_party/blink/renderer/modules/webdatabase/sql_transaction_backend.h"

namespace blink {

static String GetDatabaseIdentifier(SQLTransactionBackend* transaction) {
  Database* database = transaction->GetDatabase();
  DCHECK(database);
  return database->StringIdentifier();
}

SQLTransactionCoordinator::SQLTransactionCoordinator()
    : is_shutting_down_(false) {}

void SQLTransactionCoordinator::Trace(Visitor* visitor) const {}

void SQLTransactionCoordinator::ProcessPendingTransactions(
    CoordinationInfo& info) {
  if (info.active_write_transaction || info.pending_transactions.empty())
    return;

  SQLTransactionBackend* first_pending_transaction =
      info.pending_transactions.front();
  if (first_pending_transaction->IsReadOnly()) {
    do {
      first_pending_transaction = info.pending_transactions.TakeFirst();
      info.active_read_transactions.insert(first_pending_transaction);
      first_pending_transaction->LockAcquired();
    } while (!info.pending_transactions.empty() &&
             info.pending_transactions.front()->IsReadOnly());
  } else if (info.active_read_transactions.empty()) {
    info.pending_transactions.pop_front();
    info.active_write_transaction = first_pending_transaction;
    first_pending_transaction->LockAcquired();
  }
}

void SQLTransactionCoordinator::AcquireLock(
    SQLTransactionBackend* transaction) {
  DCHECK(!is_shutting_down_);

  String db_identifier = GetDatabaseIdentifier(transaction);

  CoordinationInfoHeapMap::iterator coordination_info_iterator =
      coordination_info_map_.find(db_identifier);
  if (coordination_info_iterator == coordination_info_map_.end()) {
    // No pending transactions for this DB
    CoordinationInfo& info =
        coordination_info_map_.insert(db_identifier, CoordinationInfo())
            .stored_value->value;
    info.pending_transactions.push_back(transaction);
    ProcessPendingTransactions(info);
  } else {
    CoordinationInfo& info = coordination_info_iterator->value;
    info.pending_transactions.push_back(transaction);
    ProcessPendingTransactions(info);
  }
}

void SQLTransactionCoordinator::ReleaseLock(
    SQLTransactionBackend* transaction) {
  if (is_shutting_down_)
    return;

  String db_identifier = GetDatabaseIdentifier(transaction);

  CoordinationInfoHeapMap::iterator coordination_info_iterator =
      coordination_info_map_.find(db_identifier);
  SECURITY_DCHECK(coordination_info_iterator != coordination_info_map_.end());
  CoordinationInfo& info = coordination_info_iterator->value;

  if (transaction->IsReadOnly()) {
    DCHECK(info.active_read_transactions.Contains(transaction));
    info.active_read_transactions.erase(transaction);
  } else {
    DCHECK_EQ(info.active_write_transaction, transaction);
    info.active_write_transaction = nullptr;
  }

  ProcessPendingTransactions(info);
}

void SQLTransactionCoordinator::Shutdown() {
  // Prevent releaseLock() from accessing / changing the coordinationInfo
  // while we're shutting down.
  is_shutting_down_ = true;

  // Notify all transactions in progress that the database thread is shutting
  // down.
  for (CoordinationInfoHeapMap::iterator coordination_info_iterator =
           coordination_info_map_.begin();
       coordination_info_iterator != coordination_info_map_.end();
       ++coordination_info_iterator) {
    CoordinationInfo& info = coordination_info_iterator->value;

    // Clean up transactions that have reached "lockAcquired":
    // Transaction phase 4 cleanup. See comment on "What happens if a
    // transaction is interrupted?" at the top of SQLTransactionBackend.cpp.
    if (info.active_write_transaction)
      info.active_write_transaction->NotifyDatabaseThreadIsShuttingDown();
    for (auto& it : info.active_read_transactions) {
      it->NotifyDatabaseThreadIsShuttingDown();
    }

    // Clean up transactions that have NOT reached "lockAcquired":
    // Transaction phase 3 cleanup. See comment on "What happens if a
    // transaction is interrupted?" at the top of SQLTransactionBackend.cpp.
    while (!info.pending_transactions.empty()) {
      SQLTransactionBackend* transaction =
          info.pending_transactions.TakeFirst();
      transaction->NotifyDatabaseThreadIsShuttingDown();
    }
  }

  // Clean up all pending transactions for all databases
  coordination_info_map_.clear();
}

}  // namespace blink
