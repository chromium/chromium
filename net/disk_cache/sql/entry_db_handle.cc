// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/sql/entry_db_handle.h"

namespace disk_cache {

EntryDbHandle::EntryDbHandle() : state_(State::kInitial) {}

EntryDbHandle::EntryDbHandle(SqlPersistentStore::ResId res_id)
    : data_(res_id), state_(State::kCreated) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

EntryDbHandle::~EntryDbHandle() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void EntryDbHandle::MarkAsCreating() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_EQ(state_, State::kInitial);
  state_ = State::kCreating;
}

void EntryDbHandle::MarkAsCreated(SqlPersistentStore::ResId res_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_EQ(state_, State::kCreating);
  data_ = res_id;
  state_ = State::kCreated;
}

void EntryDbHandle::MarkAsErrorOccurred(SqlPersistentStore::Error error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  data_ = error;
  state_ = State::kErrorOccurred;
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

void EntryDbHandle::MarkAsDoomed() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  doomed_ = true;
}

bool EntryDbHandle::doomed() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return doomed_;
}

}  // namespace disk_cache
