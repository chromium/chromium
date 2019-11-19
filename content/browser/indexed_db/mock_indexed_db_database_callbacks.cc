// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/mock_indexed_db_database_callbacks.h"

#include "content/browser/indexed_db/indexed_db_context_impl.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

MockIndexedDBDatabaseCallbacks::MockIndexedDBDatabaseCallbacks()
    : IndexedDBDatabaseCallbacks(scoped_refptr<IndexedDBContextImpl>(nullptr),
                                 mojo::NullAssociatedRemote(),
                                 base::SequencedTaskRunnerHandle::Get().get()),
      abort_called_(false),
      forced_close_called_(false) {}

void MockIndexedDBDatabaseCallbacks::OnVersionChange(int64_t old_version,
                                                     int64_t new_version) {}

void MockIndexedDBDatabaseCallbacks::OnForcedClose() {
  forced_close_called_ = true;
}

void MockIndexedDBDatabaseCallbacks::OnAbort(
    const IndexedDBTransaction& transaction,
    const IndexedDBDatabaseError& error) {
  abort_called_ = true;
}
void MockIndexedDBDatabaseCallbacks::OnComplete(
    const IndexedDBTransaction& transaction) {}

}  // namespace content
