// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/sql/entry_db_handle.h"

namespace disk_cache {

EntryDbHandle::EntryDbHandle() = default;

EntryDbHandle::EntryDbHandle(SqlPersistentStore::ResId res_id) : data_(res_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

EntryDbHandle::~EntryDbHandle() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void EntryDbHandle::SetResId(SqlPersistentStore::ResId res_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  data_ = res_id;
}

void EntryDbHandle::SetError(SqlPersistentStore::Error error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  data_ = error;
}

std::optional<SqlPersistentStore::ResId> EntryDbHandle::GetResId() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (data_.has_value() &&
      std::holds_alternative<SqlPersistentStore::ResId>(*data_)) {
    return std::get<SqlPersistentStore::ResId>(*data_);
  }
  return std::nullopt;
}

std::optional<SqlPersistentStore::Error> EntryDbHandle::GetError() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (data_.has_value() &&
      std::holds_alternative<SqlPersistentStore::Error>(*data_)) {
    return std::get<SqlPersistentStore::Error>(*data_);
  }
  return std::nullopt;
}

bool EntryDbHandle::IsFinished() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return data_.has_value();
}

}  // namespace disk_cache
