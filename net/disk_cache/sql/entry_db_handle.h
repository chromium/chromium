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
  // SqlBackendImpl::OpenOrCreateEntry and CreateEntry perform speculative
  // creation and synchronously return a SqlEntryImpl holding an EntryDbHandle
  // in the kInitial state when the DB's in-memory index indicates that no
  // entry with the corresponding key exists in the DB.
  //
  // The state changes to kCreating when writing to the DB becomes necessary
  // (e.g., when the write buffering limit is exceeded), and then to kCreated
  // upon completion. If the DB write fails, the state becomes kErrorOccurred.
  //
  // If an existing entry is opened, or if speculative creation is skipped
  // (e.g., because the in-memory index is not yet loaded), a SqlEntryImpl
  // holding an EntryDbHandle in the kCreated state is created after the DB
  // read or write operation completes.
  enum class State {
    kInitial = 0,
    kCreating,
    kCreated,
    kErrorOccurred,
  };

  EntryDbHandle();
  explicit EntryDbHandle(SqlPersistentStore::ResId res_id);

  void MarkAsCreating();
  void MarkAsCreated(SqlPersistentStore::ResId res_id);
  void MarkAsErrorOccurred(SqlPersistentStore::Error error);

  std::optional<SqlPersistentStore::ResId> GetResId() const;
  std::optional<SqlPersistentStore::Error> GetError() const;

  bool IsInitialState() const { return state_ == State::kInitial; }
  bool IsCreatingState() const { return state_ == State::kCreating; }

  // Returns true if the ResId has been set or an error has occurred.
  bool IsFinished() const;

  // Marks the entry as doomed. This is called by the backend when an
  // active entry is doomed.
  void MarkAsDoomed();
  bool doomed() const;

 private:
  friend class base::RefCounted<EntryDbHandle>;
  ~EntryDbHandle();

  std::optional<
      std::variant<SqlPersistentStore::ResId, SqlPersistentStore::Error>>
      data_;

  State state_;

  // True if this entry has been marked for deletion.
  bool doomed_ = false;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_SQL_ENTRY_DB_HANDLE_H_
