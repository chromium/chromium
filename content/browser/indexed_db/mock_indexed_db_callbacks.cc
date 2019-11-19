// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/mock_indexed_db_callbacks.h"

#include <memory>
#include <utility>

#include "testing/gtest/include/gtest/gtest.h"

using blink::IndexedDBDatabaseMetadata;
using blink::IndexedDBKey;

namespace content {

MockIndexedDBCallbacks::MockIndexedDBCallbacks()
    : IndexedDBCallbacks(nullptr,
                         url::Origin(),
                         mojo::NullAssociatedRemote(),
                         base::SequencedTaskRunnerHandle::Get()) {}
MockIndexedDBCallbacks::MockIndexedDBCallbacks(bool expect_connection)
    : IndexedDBCallbacks(nullptr,
                         url::Origin(),
                         mojo::NullAssociatedRemote(),
                         base::SequencedTaskRunnerHandle::Get()),
      expect_connection_(expect_connection) {}

MockIndexedDBCallbacks::~MockIndexedDBCallbacks() {
  EXPECT_EQ(expect_connection_, !!connection_);
}

void MockIndexedDBCallbacks::OnError(const IndexedDBDatabaseError& error) {
  error_called_ = true;
}

void MockIndexedDBCallbacks::OnSuccess() {}

void MockIndexedDBCallbacks::OnSuccess(int64_t result) {}

void MockIndexedDBCallbacks::OnSuccess(const std::vector<base::string16>&) {}
void MockIndexedDBCallbacks::OnSuccess(
    std::vector<blink::mojom::IDBNameAndVersionPtr> names_and_versions) {
  info_called_ = true;
}

void MockIndexedDBCallbacks::OnSuccess(
    std::unique_ptr<IndexedDBConnection> connection,
    const IndexedDBDatabaseMetadata& metadata) {
  if (!upgrade_called_)
    connection_ = std::move(connection);
  if (call_on_db_success_)
    std::move(call_on_db_success_).Run();
}

void MockIndexedDBCallbacks::OnUpgradeNeeded(
    int64_t old_version,
    std::unique_ptr<IndexedDBConnection> connection,
    const IndexedDBDatabaseMetadata& metadata,
    const IndexedDBDataLossInfo& data_loss_info) {
  connection_ = std::move(connection);
  upgrade_called_ = true;
  if (call_on_upgrade_needed_)
    std::move(call_on_upgrade_needed_).Run();
}

void MockIndexedDBCallbacks::CallOnUpgradeNeeded(base::OnceClosure closure) {
  call_on_upgrade_needed_ = std::move(closure);
}
void MockIndexedDBCallbacks::CallOnDBSuccess(base::OnceClosure closure) {
  call_on_db_success_ = std::move(closure);
}

}  // namespace content
