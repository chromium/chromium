// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/instance/sqlite/record_iterator.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/functional/callback.h"
#include "base/types/expected.h"
#include "content/browser/indexed_db/instance/record.h"
#include "content/browser/indexed_db/status.h"
#include "sql/statement.h"
#include "third_party/blink/public/common/indexeddb/indexeddb_key.h"

// TODO(crbug.com/40253999): Remove after handling all error cases.
#define TRANSIENT_CHECK(condition) CHECK(condition)

namespace content::indexed_db::sqlite {

RecordIterator::RecordIterator(std::unique_ptr<sql::Statement> statement,
                               BindCallback bind_parameters,
                               ReadCallback read_row,
                               std::string initial_position)
    : statement_(std::move(statement)),
      bind_parameters_(std::move(bind_parameters)),
      read_row_(std::move(read_row)),
      current_position_(std::move(initial_position)) {}

RecordIterator::~RecordIterator() = default;

StatusOr<std::unique_ptr<Record>> RecordIterator::Iterate(
    const blink::IndexedDBKey& key,
    const blink::IndexedDBKey& primary_key) {
  statement_->Reset(/*clear_bound_vars=*/false);
  bind_parameters_.Run(*statement_, *current_position_, key, primary_key,
                       /*offset=*/0);
  if (!statement_->Step()) {
    TRANSIENT_CHECK(statement_->Succeeded());
    // End of range.
    current_position_.reset();
    return nullptr;
  }
  return read_row_.Run(*statement_).transform([this](PositionAndRecord result) {
    current_position_ = std::move(result.first);
    return std::move(result.second);
  });
}

StatusOr<std::unique_ptr<Record>> RecordIterator::Iterate(uint32_t count) {
  TRANSIENT_CHECK(count > 0);

  // TODO(crbug.com/419208481): Implement a fast path where `statement_` is
  // stepped without being reset when no record has changed in the range.
  statement_->Reset(/*clear_bound_vars=*/false);

  // Iterate count times => offset by (i.e., skip) [count - 1] rows.
  bind_parameters_.Run(*statement_, *current_position_, /*key=*/{},
                       /*primary_key=*/{},
                       /*offset=*/count - 1);
  if (!statement_->Step()) {
    TRANSIENT_CHECK(statement_->Succeeded());
    // End of range.
    current_position_.reset();
    return nullptr;
  }
  return read_row_.Run(*statement_).transform([this](PositionAndRecord result) {
    current_position_ = std::move(result.first);
    return std::move(result.second);
  });
}

}  // namespace content::indexed_db::sqlite
