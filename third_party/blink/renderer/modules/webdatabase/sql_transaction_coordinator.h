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

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBDATABASE_SQL_TRANSACTION_COORDINATOR_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBDATABASE_SQL_TRANSACTION_COORDINATOR_H_

#include "base/macros.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/wtf/deque.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hash.h"

namespace blink {

class SQLTransactionBackend;

class SQLTransactionCoordinator final
    : public GarbageCollected<SQLTransactionCoordinator> {
 public:
  SQLTransactionCoordinator();
  void Trace(blink::Visitor*);
  void AcquireLock(SQLTransactionBackend*);
  void ReleaseLock(SQLTransactionBackend*);
  void Shutdown();

 private:
  typedef Deque<CrossThreadPersistent<SQLTransactionBackend>> TransactionsQueue;
  struct CoordinationInfo {
    DISALLOW_NEW();

   public:
    TransactionsQueue pending_transactions;
    HashSet<CrossThreadPersistent<SQLTransactionBackend>>
        active_read_transactions;
    CrossThreadPersistent<SQLTransactionBackend> active_write_transaction;
  };
  // Maps database names to information about pending transactions
  typedef HashMap<String, CoordinationInfo> CoordinationInfoHeapMap;
  CoordinationInfoHeapMap coordination_info_map_;
  bool is_shutting_down_;

  void ProcessPendingTransactions(CoordinationInfo&);

  DISALLOW_COPY_AND_ASSIGN(SQLTransactionCoordinator);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBDATABASE_SQL_TRANSACTION_COORDINATOR_H_
