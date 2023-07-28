// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/mock_indexed_db_factory_client.h"

#include <memory>
#include <utility>

#include "base/task/sequenced_task_runner.h"
#include "components/services/storage/public/cpp/buckets/bucket_locator.h"
#include "testing/gtest/include/gtest/gtest.h"

using blink::IndexedDBDatabaseMetadata;
using blink::IndexedDBKey;

namespace content {

MockIndexedDBFactoryClient::MockIndexedDBFactoryClient()
    : IndexedDBFactoryClient(nullptr,
                             absl::nullopt,
                             mojo::NullAssociatedRemote(),
                             base::SequencedTaskRunner::GetCurrentDefault()) {}
MockIndexedDBFactoryClient::MockIndexedDBFactoryClient(bool expect_connection)
    : IndexedDBFactoryClient(nullptr,
                             absl::nullopt,
                             mojo::NullAssociatedRemote(),
                             base::SequencedTaskRunner::GetCurrentDefault()),
      expect_connection_(expect_connection) {}

MockIndexedDBFactoryClient::~MockIndexedDBFactoryClient() {
  EXPECT_EQ(expect_connection_, !!connection_);
}

void MockIndexedDBFactoryClient::OnError(const IndexedDBDatabaseError& error) {
  error_called_ = true;
}

void MockIndexedDBFactoryClient::OnDeleteSuccess(int64_t old_version) {}

void MockIndexedDBFactoryClient::OnOpenSuccess(
    std::unique_ptr<IndexedDBConnection> connection,
    const IndexedDBDatabaseMetadata& metadata) {
  if (!upgrade_called_) {
    connection_ = std::move(connection);
  }
  if (call_on_db_success_) {
    std::move(call_on_db_success_).Run();
  }
}

void MockIndexedDBFactoryClient::OnUpgradeNeeded(
    int64_t old_version,
    std::unique_ptr<IndexedDBConnection> connection,
    const IndexedDBDatabaseMetadata& metadata,
    const IndexedDBDataLossInfo& data_loss_info) {
  connection_ = std::move(connection);
  upgrade_called_ = true;
  if (call_on_upgrade_needed_) {
    std::move(call_on_upgrade_needed_).Run();
  }
}

void MockIndexedDBFactoryClient::CallOnUpgradeNeeded(
    base::OnceClosure closure) {
  call_on_upgrade_needed_ = std::move(closure);
}
void MockIndexedDBFactoryClient::CallOnDBSuccess(base::OnceClosure closure) {
  call_on_db_success_ = std::move(closure);
}
void MockIndexedDBFactoryClient::CallOnInfoSuccess(
    base::RepeatingClosure closure) {
  call_on_info_success_ = std::move(closure);
}

ThunkFactoryClient::ThunkFactoryClient(IndexedDBFactoryClient& wrapped)
    : MockIndexedDBFactoryClient(false), wrapped_(wrapped) {}

void ThunkFactoryClient::OnError(const IndexedDBDatabaseError& error) {
  wrapped_->OnError(error);
}

void ThunkFactoryClient::OnBlocked(int64_t existing_version) {
  wrapped_->OnBlocked(existing_version);
}

void ThunkFactoryClient::OnUpgradeNeeded(
    int64_t old_version,
    std::unique_ptr<IndexedDBConnection> connection,
    const blink::IndexedDBDatabaseMetadata& metadata,
    const IndexedDBDataLossInfo& data_loss_info) {
  wrapped_->OnUpgradeNeeded(old_version, std::move(connection), metadata,
                            data_loss_info);
}

void ThunkFactoryClient::OnOpenSuccess(
    std::unique_ptr<IndexedDBConnection> connection,
    const blink::IndexedDBDatabaseMetadata& metadata) {
  wrapped_->OnOpenSuccess(std::move(connection), metadata);
}

void ThunkFactoryClient::OnDeleteSuccess(int64_t old_version) {
  wrapped_->OnDeleteSuccess(old_version);
}

}  // namespace content
