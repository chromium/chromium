// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DISK_CACHE_SQL_ENTRY_DB_HANDLE_H_
#define NET_DISK_CACHE_SQL_ENTRY_DB_HANDLE_H_

#include <optional>
#include <variant>

#include "base/memory/ref_counted.h"
#include "base/sequence_checker.h"
#include "net/base/net_export.h"
#include "net/disk_cache/sql/sql_persistent_store.h"

namespace disk_cache {

// This class holds the resource ID (ResId) of a SQL cache entry.
// For speculatively created entries, the ResId might not be available
// initially. This class allows setting the ResId or an Error later.
// It is ref-counted so it can be shared between SqlEntryImpl and
// SqlBackendImpl operations.
class NET_EXPORT_PRIVATE EntryDbHandle
    : public base::RefCounted<EntryDbHandle> {
 public:
  EntryDbHandle();
  explicit EntryDbHandle(SqlPersistentStore::ResId res_id);

  void SetResId(SqlPersistentStore::ResId res_id);
  void SetError(SqlPersistentStore::Error error);

  std::optional<SqlPersistentStore::ResId> GetResId() const;
  std::optional<SqlPersistentStore::Error> GetError() const;

  // Returns true if the ResId has been set or an error has occurred.
  bool IsFinished() const;

 private:
  friend class base::RefCounted<EntryDbHandle>;
  ~EntryDbHandle();

  std::optional<
      std::variant<SqlPersistentStore::ResId, SqlPersistentStore::Error>>
      data_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_SQL_ENTRY_DB_HANDLE_H_
