// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sql/recover_module/cursor.h"

#include "base/containers/span.h"
#include "sql/recover_module/table.h"

namespace sql {
namespace recover {

VirtualCursor::VirtualCursor(VirtualTable* table)
    : table_(table),
      db_reader_(table),
      payload_reader_(&db_reader_),
      record_reader_(&payload_reader_, table->column_specs().size()) {
  DCHECK(table_ != nullptr);
}

VirtualCursor::~VirtualCursor() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  table_->WillDeleteCursor(this);
}

int VirtualCursor::First() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  inner_decoders_.clear();
  leaf_decoder_ = nullptr;

  AppendPageDecoder(table_->root_page_id());
  return Next();
}

int VirtualCursor::Next() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  record_reader_.Reset();

  while (!inner_decoders_.empty() || leaf_decoder_.get()) {
    if (leaf_decoder_.get()) {
      if (!leaf_decoder_->CanAdvance()) {
        // The leaf has been exhausted. Remove it from the DFS stack.
        leaf_decoder_ = nullptr;
        continue;
      }
      if (!leaf_decoder_->TryAdvance())
        continue;

      if (!payload_reader_.Initialize(leaf_decoder_->last_record_size(),
                                      leaf_decoder_->last_record_offset())) {
        continue;
      }
      if (!record_reader_.Initialize())
        continue;

      // Found a healthy record.
      if (!IsAcceptableRecord()) {
        record_reader_.Reset();
        continue;
      }
      return SQLITE_OK;
    }

    // Try advancing the bottom-most inner node.
    DCHECK(!inner_decoders_.empty());
    InnerPageDecoder* inner_decoder = inner_decoders_.back().get();
    if (!inner_decoder->CanAdvance()) {
      // The inner node's sub-tree has been visited. Remove from the DFS stack.
      inner_decoders_.pop_back();
      continue;
    }
    int next_page_id = inner_decoder->TryAdvance();
    if (next_page_id == DatabasePageReader::kInvalidPageId)
      continue;
    AppendPageDecoder(next_page_id);
  }

  // The cursor reached the end of the table.
  return SQLITE_OK;
}

int VirtualCursor::ReadColumn(int column_index,
                              sqlite3_context* result_context) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_GE(column_index, 0);
  DCHECK_LT(column_index, static_cast<int>(table_->column_specs().size()));
  DCHECK(record_reader_.IsInitialized());

  if (table_->column_specs()[column_index].type == ModuleColumnType::kRowId) {
    sqlite3_result_int64(result_context, RowId());
    return SQLITE_OK;
  }

  if (record_reader_.ReadValue(column_index, result_context))
    return SQLITE_OK;
  return SQLITE_ERROR;
}

int64_t VirtualCursor::RowId() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(record_reader_.IsInitialized());
  DCHECK(leaf_decoder_.get());
  return leaf_decoder_->last_record_rowid();
}

void VirtualCursor::AppendPageDecoder(int page_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(leaf_decoder_.get() == nullptr)
      << __func__
      << " must only be called when the current path has no leaf decoder";

  if (db_reader_.ReadPage(page_id) != SQLITE_OK)
    return;

  if (LeafPageDecoder::IsOnValidPage(&db_reader_)) {
    leaf_decoder_ = std::make_unique<LeafPageDecoder>(&db_reader_);
    return;
  }

  if (InnerPageDecoder::IsOnValidPage(&db_reader_)) {
    // Detect cycles.
    for (const auto& decoder : inner_decoders_) {
      if (decoder->page_id() == page_id)
        return;
    }

    // Give up on overly deep tree branches.
    //
    // SQLite supports up to 2^31 pages. SQLite ensures that inner nodes can
    // hold at least 4 child pointers, even in the presence of very large keys.
    // So, even poorly balanced trees should not exceed 100 nodes in depth.
    // InnerPageDecoder instances take up 32 bytes on 64-bit platforms.
    //
    // The depth limit below balances recovering broken trees with avoiding
    // excessive memory consumption.
    constexpr int kMaxTreeDepth = 10000;
    if (inner_decoders_.size() == kMaxTreeDepth)
      return;

    inner_decoders_.emplace_back(
        std::make_unique<InnerPageDecoder>(&db_reader_));
    return;
  }
}

bool VirtualCursor::IsAcceptableRecord() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(record_reader_.IsInitialized());

  const std::vector<RecoveredColumnSpec>& column_specs = table_->column_specs();
  const int column_count = static_cast<int>(column_specs.size());
  for (int column_index = 0; column_index < column_count; ++column_index) {
    ValueType value_type = record_reader_.GetValueType(column_index);
    if (!column_specs[column_index].IsAcceptableValue(value_type))
      return false;
  }
  return true;
}

}  // namespace recover
}  // namespace sql
