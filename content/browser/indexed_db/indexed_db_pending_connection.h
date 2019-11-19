// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_PENDING_CONNECTION_H_
#define CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_PENDING_CONNECTION_H_

#include <stdint.h>

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/indexed_db/indexed_db_callbacks.h"
#include "content/browser/indexed_db/indexed_db_data_loss_info.h"
#include "content/browser/indexed_db/indexed_db_database_callbacks.h"
#include "content/browser/indexed_db/indexed_db_execution_context_connection_tracker.h"
#include "content/common/content_export.h"
#include "url/origin.h"

namespace content {

class IndexedDBCallbacks;
class IndexedDBDatabaseCallbacks;

struct CONTENT_EXPORT IndexedDBPendingConnection {
  IndexedDBPendingConnection(
      scoped_refptr<IndexedDBCallbacks> callbacks,
      scoped_refptr<IndexedDBDatabaseCallbacks> database_callbacks,
      IndexedDBExecutionContextConnectionTracker::Handle
          execution_context_connection_handle,
      int64_t transaction_id,
      int64_t version,
      base::OnceCallback<void(base::WeakPtr<IndexedDBTransaction>)>
          create_transaction_callback);
  ~IndexedDBPendingConnection();
  scoped_refptr<IndexedDBCallbacks> callbacks;
  scoped_refptr<IndexedDBDatabaseCallbacks> database_callbacks;
  IndexedDBExecutionContextConnectionTracker::Handle
      execution_context_connection_handle;
  int64_t transaction_id;
  int64_t version;
  IndexedDBDataLossInfo data_loss_info;
  base::OnceCallback<void(base::WeakPtr<IndexedDBTransaction>)>
      create_transaction_callback;
  base::WeakPtr<IndexedDBTransaction> transaction;
  bool was_cold_open = false;
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_PENDING_CONNECTION_H_
