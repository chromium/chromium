// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_DATABASE_CALLBACKS_H_
#define CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_DATABASE_CALLBACKS_H_

#include <stdint.h>

#include <memory>

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "content/common/content_export.h"
#include "content/public/browser/browser_thread.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom.h"

namespace content {
class IndexedDBContextImpl;
class IndexedDBDatabaseError;
class IndexedDBTransaction;

// Expected to be constructed/called/deleted on IDB sequence.
class CONTENT_EXPORT IndexedDBDatabaseCallbacks
    : public base::RefCounted<IndexedDBDatabaseCallbacks> {
 public:
  IndexedDBDatabaseCallbacks(
      scoped_refptr<IndexedDBContextImpl> context,
      mojo::PendingAssociatedRemote<blink::mojom::IDBDatabaseCallbacks>
          callbacks_remote,
      base::SequencedTaskRunner* idb_runner);

  virtual void OnForcedClose();
  virtual void OnVersionChange(int64_t old_version, int64_t new_version);

  virtual void OnAbort(const IndexedDBTransaction& transaction,
                       const IndexedDBDatabaseError& error);
  virtual void OnComplete(const IndexedDBTransaction& transaction);
  virtual void OnDatabaseChange(blink::mojom::IDBObserverChangesPtr changes);

  void OnConnectionError();

 protected:
  virtual ~IndexedDBDatabaseCallbacks();

 private:
  friend class base::RefCounted<IndexedDBDatabaseCallbacks>;

  bool complete_ = false;
  scoped_refptr<IndexedDBContextImpl> indexed_db_context_;
  mojo::AssociatedRemote<blink::mojom::IDBDatabaseCallbacks> callbacks_;
  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(IndexedDBDatabaseCallbacks);
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_DATABASE_CALLBACKS_H_
