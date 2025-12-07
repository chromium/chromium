// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INSTANCE_DATABASE_CALLBACKS_H_
#define CONTENT_BROWSER_INDEXED_DB_INSTANCE_DATABASE_CALLBACKS_H_

#include <stdint.h>

#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom.h"

namespace content::indexed_db {
class DatabaseError;
class Transaction;

// This class serves as a thin wrapper around `IDBDatabaseCallbacks` and no-ops
// any operations invoked after `OnForcedClose()`.
class CONTENT_EXPORT DatabaseCallbacks {
 public:
  explicit DatabaseCallbacks(
      mojo::PendingAssociatedRemote<blink::mojom::IDBDatabaseCallbacks>
          callbacks_remote);
  ~DatabaseCallbacks();

  DatabaseCallbacks(const DatabaseCallbacks&) = delete;
  DatabaseCallbacks& operator=(const DatabaseCallbacks&) = delete;

  void OnForcedClose();
  void OnVersionChange(int64_t old_version, int64_t new_version);
  void OnAbort(const Transaction& transaction, const DatabaseError& error);
  void OnComplete(const Transaction& transaction);

 private:
  bool complete_ = false;
  mojo::AssociatedRemote<blink::mojom::IDBDatabaseCallbacks> callbacks_;
};

}  // namespace content::indexed_db

#endif  // CONTENT_BROWSER_INDEXED_DB_INSTANCE_DATABASE_CALLBACKS_H_
