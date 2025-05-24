// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/instance/sqlite/backing_store_impl.h"

#include "base/files/file_path.h"
#include "base/notimplemented.h"
#include "base/types/expected_macros.h"
#include "content/browser/indexed_db/indexed_db_data_loss_info.h"
#include "content/browser/indexed_db/instance/sqlite/backing_store_database_impl.h"
#include "content/browser/indexed_db/instance/sqlite/database_connection.h"
#include "content/browser/indexed_db/status.h"

namespace content::indexed_db::sqlite {

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

void BackingStoreImpl::StopPreCloseTasks() {}

int64_t BackingStoreImpl::GetInMemorySize() const {
  NOTIMPLEMENTED();
  return 0;
}

StatusOr<std::vector<std::u16string>> BackingStoreImpl::GetDatabaseNames() {
  std::vector<std::u16string> names;
  // TODO(crbug.com/40253999): Support on-disk databases.
  for (const auto& [name, _] : open_connections_) {
    names.push_back(name);
  }
  return names;
}

StatusOr<std::vector<blink::mojom::IDBNameAndVersionPtr>>
BackingStoreImpl::GetDatabaseNamesAndVersions() {
  std::vector<blink::mojom::IDBNameAndVersionPtr> names_and_versions;
  // TODO(crbug.com/40253999): Support on-disk databases.
  for (const auto& [name, db] : open_connections_) {
    names_and_versions.push_back(
        blink::mojom::IDBNameAndVersion::New(name, db->metadata().version));
  }
  return names_and_versions;
}

StatusOr<std::unique_ptr<BackingStore::Database>>
BackingStoreImpl::CreateOrOpenDatabase(const std::u16string& name) {
  auto it = open_connections_.find(name);
  if (it == open_connections_.end()) {
    ASSIGN_OR_RETURN(std::unique_ptr<DatabaseConnection> db,
                     DatabaseConnection::Open(name, data_path_));
    it = open_connections_.emplace(name, std::move(db)).first;
  }
  return std::make_unique<BackingStoreDatabaseImpl>(it->second->GetWeakPtr());
}

uintptr_t BackingStoreImpl::GetIdentifierForMemoryDump() {
  NOTIMPLEMENTED();
  return 0;
}

void BackingStoreImpl::FlushForTesting() {
  NOTIMPLEMENTED();
}

}  // namespace content::indexed_db::sqlite
