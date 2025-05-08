// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/instance/sqlite/backing_store_impl.h"

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/notimplemented.h"
#include "content/browser/indexed_db/indexed_db_data_loss_info.h"
#include "content/browser/indexed_db/instance/sqlite/backing_store_database_impl.h"
#include "content/browser/indexed_db/status.h"
#include "sql/database.h"
#include "sql/meta_table.h"

namespace content::indexed_db::sqlite {

DatabaseConnection::DatabaseConnection(std::unique_ptr<sql::Database> db)
    : db_(std::move(db)), meta_table_(std::make_unique<sql::MetaTable>()) {}

DatabaseConnection::~DatabaseConnection() = default;

base::WeakPtr<DatabaseConnection> DatabaseConnection::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

std::tuple<std::unique_ptr<BackingStore>, Status, IndexedDBDataLossInfo, bool>
BackingStoreImpl::OpenAndVerify(base::FilePath data_path) {
  return {
      std::make_unique<BackingStoreImpl>(std::move(data_path)),
      Status::OK(),
      IndexedDBDataLossInfo(),
      false,
  };
}

BackingStoreImpl::BackingStoreImpl(base::FilePath data_path)
    : data_path_(std::move(data_path)) {}

BackingStoreImpl::~BackingStoreImpl() = default;

void BackingStoreImpl::TearDown(base::WaitableEvent* signal_on_destruction) {
  NOTIMPLEMENTED();
  signal_on_destruction->Signal();
}

void BackingStoreImpl::InvalidateBlobReferences() {
  NOTIMPLEMENTED();
}

void BackingStoreImpl::StartPreCloseTasks(base::OnceClosure on_done) {
  NOTIMPLEMENTED();
  std::move(on_done).Run();
}

void BackingStoreImpl::StopPreCloseTasks() {
  NOTIMPLEMENTED();
}

int64_t BackingStoreImpl::GetInMemorySize() const {
  NOTIMPLEMENTED();
  return 0;
}

Status BackingStoreImpl::GetDatabaseNames(std::vector<std::u16string>* names) {
  NOTIMPLEMENTED();
  return Status::OK();
}

Status BackingStoreImpl::GetDatabaseNamesAndVersions(
    std::vector<blink::mojom::IDBNameAndVersionPtr>* names_and_versions) {
  NOTIMPLEMENTED();
  return Status::OK();
}

base::expected<std::unique_ptr<BackingStore::Database>, Status>
BackingStoreImpl::CreateOrOpenDatabase(const std::u16string& name) {
  auto it = open_connections_.find(name);
  if (it == open_connections_.end()) {
    // TODO(crbug.com/40253999): Create new tag(s) for metrics.
    constexpr sql::Database::Tag kSqlTag = "Test";
    auto db = std::make_unique<sql::Database>(
        sql::DatabaseOptions().set_exclusive_locking(true).set_wal_mode(true),
        kSqlTag);
    // TODO(crbug.com/40253999): Support on-disk databases.
    CHECK(db->OpenInMemory());
    auto result = open_connections_.try_emplace(name, std::move(db));
    CHECK(result.second);
    it = result.first;
  }
  return std::make_unique<BackingStoreDatabaseImpl>(name,
                                                    it->second.GetWeakPtr());
}

uintptr_t BackingStoreImpl::GetIdentifierForMemoryDump() {
  NOTIMPLEMENTED();
  return 0;
}

void BackingStoreImpl::FlushForTesting() {
  NOTIMPLEMENTED();
}

}  // namespace content::indexed_db::sqlite
