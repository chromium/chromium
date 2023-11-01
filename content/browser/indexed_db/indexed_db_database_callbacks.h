// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_DATABASE_CALLBACKS_H_
#define CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_DATABASE_CALLBACKS_H_

#include <stdint.h>

#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom.h"

namespace content {
class IndexedDBDatabaseError;
class IndexedDBTransaction;

// Expected to be constructed/called/deleted on IDB sequence.
class CONTENT_EXPORT IndexedDBDatabaseCallbacks {
 public:
  explicit IndexedDBDatabaseCallbacks(
      mojo::PendingAssociatedRemote<blink::mojom::IDBDatabaseCallbacks>
          callbacks_remote);
  virtual ~IndexedDBDatabaseCallbacks();

  IndexedDBDatabaseCallbacks(const IndexedDBDatabaseCallbacks&) = delete;
  IndexedDBDatabaseCallbacks& operator=(const IndexedDBDatabaseCallbacks&) =
      delete;

  virtual void OnForcedClose();
  virtual void OnVersionChange(int64_t old_version, int64_t new_version);

  virtual void OnAbort(const IndexedDBTransaction& transaction,
                       const IndexedDBDatabaseError& error);
  virtual void OnComplete(const IndexedDBTransaction& transaction);

 private:
  bool complete_ = false;
  mojo::AssociatedRemote<blink::mojom::IDBDatabaseCallbacks> callbacks_;
  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_DATABASE_CALLBACKS_H_
