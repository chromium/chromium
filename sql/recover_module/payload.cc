
// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sql/recover_module/payload.h"

#include <algorithm>
#include <cstddef>
#include <limits>
#include <type_traits>

#include "base/logging.h"
#include "sql/recover_module/btree.h"
#include "sql/recover_module/integers.h"
#include "sql/recover_module/pager.h"
#include "third_party/sqlite/sqlite3.h"

namespace sql {
namespace recover {

namespace {

// The size of page IDs pointing to overflow pages.
constexpr int kPageIdSize = sizeof(int32_t);

// The largest page header size. Inner B-tree pages use this size.
constexpr int kMaxPageOverhead = 12;

// Maximum number of bytes in a cell used by header and trailer.
//
// The maximum overhead is incurred by cells in leaf table B-tree pages.
// * 2-byte cell pointer, in the cell pointer array
// * 9-byte row ID, for the maximum varint size
// * 8-byte payload size, (varint size for maximum payload size of 2 ** 48)
// * 4-byte first overflow page ID
constexpr int kMaxCellOverhead = 23;

// Knob used to trade off between having more rows in a tree page and
// avoiding the use of overflow pages for large values. The knob value is
// stored in the database header.
//
// In SQLite 3.6 and above, the knob was fixed to the value below.
static constexpr int kLeafPayloadFraction = 32;

// Denominator used by all load fractions in SQLite's B-tree logic.
static constexpr int kPayloadFractionDenominator = 255;

// The maximum size of a payload on a leaf page.
//
// Records whose size exceeds this limit spill over to overflow pages.
//
// The return value is guaranteed to be at least
// LeafPayloadReader::kMinInlineSize, and at most the database page size.
int MaxInlinePayloadSize(int page_size) {
  DCHECK_GE(page_size, DatabasePageReader::kMinUsablePageSize);
  DCHECK_LE(page_size, DatabasePageReader::kMaxPageSize);

  const int max_inline_payload_size =
      page_size - kMaxPageOverhead - kMaxCellOverhead;
  DCHECK_GE(max_inline_payload_size, LeafPayloadReader::kMinInlineSize);
  static_assert(
      DatabasePageReader::kMinPageSize - kMaxPageOverhead - kMaxCellOverhead >
          LeafPayloadReader::kMinInlineSize,
      "The DCHECK above may fail");

  return max_inline_payload_size;
}

// The minimum size of a payload on a B-tree page.
//
// Records that spill over to overflow pages are guaranteed to have at least
// these many bytes stored in the B-tree page.
//
// The return value is guaranteed to be at least
// LeafPayloadReader::kMinInlineSize, and at most
// MaxInlinePayloadSize(page_size).
int MinInlinePayloadSize(int page_size) {
  DCHECK_GE(page_size, DatabasePageReader::kMinUsablePageSize);
  DCHECK_LE(page_size, DatabasePageReader::kMaxPageSize);

  const int min_inline_payload_size =
      ((page_size - kMaxPageOverhead) * kLeafPayloadFraction) /
          kPayloadFractionDenominator -
      kMaxCellOverhead;
  static_assert((DatabasePageReader::kMaxPageSize - kMaxPageOverhead) *
                        kLeafPayloadFraction <=
                    std::numeric_limits<int>::max(),
                "The |min_inline_payload_size| computation above may overflow");
  DCHECK_GE(min_inline_payload_size, LeafPayloadReader::kMinInlineSize);
  static_assert(((DatabasePageReader::kMinUsablePageSize - kMaxPageOverhead) *
                 kLeafPayloadFraction) /
                            kPayloadFractionDenominator -
                        kMaxCellOverhead >=
                    LeafPayloadReader::kMinInlineSize,
                "The DCHECK above may fail");

  // The minimum inline payload size is ((P - 12) * 32) / 255 - 23. This is
  // smaller or equal to ((P - 12) * 255) / 255 - 23, which is P - 35. This is
  // the maximum payload size.
  DCHECK_LE(min_inline_payload_size, MaxInlinePayloadSize(page_size));

  return min_inline_payload_size;
}

// The maximum size of a payload on an overflow page.
//
// The return value is guaranteed to be positive, and at most equal to the page
// size.
int MaxOverflowPayloadSize(int page_size) {
  DCHECK_GE(page_size, DatabasePageReader::kMinUsablePageSize);
  DCHECK_LE(page_size, DatabasePageReader::kMaxPageSize);

  // Each overflow page starts with a 32-bit integer pointing to the next
  // overflow page. The rest of the page stores payload bytes.
  const int max_overflow_payload_size = page_size - 4;

  DCHECK_GT(max_overflow_payload_size, 0);
  static_assert(DatabasePageReader::kMinUsablePageSize > 4,
                "The DCHECK above may fail");
  DCHECK_LE(max_overflow_payload_size, DatabasePageReader::kMaxPageSize);
  return max_overflow_payload_size;
}

}  // namespace

LeafPayloadReader::LeafPayloadReader(DatabasePageReader* db_reader)
    : db_reader_(db_reader), page_id_(DatabasePageReader::kInvalidPageId) {}

LeafPayloadReader::~LeafPayloadReader() = default;

bool LeafPayloadReader::Initialize(int64_t payload_size, int payload_offset) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  payload_size_ = payload_size;
  inline_payload_offset_ = payload_offset;
  page_id_ = db_reader_->page_id();

  const int page_size = db_reader_->page_size();

  const int max_inline_payload_size = MaxInlinePayloadSize(page_size);
  if (payload_size <= max_inline_payload_size) {
    // The payload fits inside the page.
    inline_payload_size_ = static_cast<int>(payload_size);
    overflow_page_count_ = 0;
  } else {
    const int min_inline_payload_size = MinInlinePayloadSize(page_size);

    // The payload size is bigger than the maximum inline payload size, so it
    // must be bigger than the minimum payload size. This check verifies that
    // the subtractions below have non-negative results.
    DCHECK_GT(payload_size, min_inline_payload_size);

    // Payload sizes are upper-bounded by the page size.
    static_assert(
        DatabasePageReader::kMaxPageSize * 2 <= std::numeric_limits<int>::max(),
        "The additions below may overflow");

    // Ideally, all bytes in the overflow pages would be used by the payload.
    // Check if this can be accomplished within the other payload constraints.
    max_overflow_payload_size_ = MaxOverflowPayloadSize(page_size);
    const int64_t efficient_overflow_page_count =
        (payload_size - min_inline_payload_size) / max_overflow_payload_size_;
    const int efficient_overflow_spill =
        (payload_size - min_inline_payload_size) % max_overflow_payload_size_;
    const int efficient_inline_payload_size =
        min_inline_payload_size + efficient_overflow_spill;

    if (efficient_inline_payload_size <= max_inline_payload_size) {
      inline_payload_size_ = efficient_inline_payload_size;
      overflow_page_count_ = efficient_overflow_page_count;
      DCHECK_EQ(
          0, (payload_size - inline_payload_size_) % max_overflow_payload_size_)
          << "Overflow pages not fully packed";
    } else {
      inline_payload_size_ = min_inline_payload_size;
      overflow_page_count_ = efficient_overflow_page_count + 1;
    }

    DCHECK_LE(inline_payload_size_, max_inline_payload_size);
    DCHECK_EQ(overflow_page_count_, (payload_size - inline_payload_size_ +
                                     (max_overflow_payload_size_ - 1)) /
                                        max_overflow_payload_size_)
        << "Incorect overflow page count calculation";
  }

  DCHECK_LE(inline_payload_size_, payload_size);
  DCHECK_LE(inline_payload_size_, page_size);

  const int first_overflow_page_id_size =
      (overflow_page_count_ == 0) ? 0 : kPageIdSize;

  if (inline_payload_offset_ + inline_payload_size_ +
          first_overflow_page_id_size >
      page_size) {
    // Corruption can result in overly large payload sizes. Reject the obvious
    // case where the in-page payload extends past the end of the page.
    page_id_ = DatabasePageReader::kInvalidPageId;
    return false;
  }

  overflow_page_ids_.clear();
  overflow_page_ids_.reserve(overflow_page_count_);
  return true;
}

bool LeafPayloadReader::ReadPayload(int64_t offset,
                                    int64_t size,
                                    uint8_t* buffer) {
  DCHECK_GE(offset, 0);
  DCHECK_LT(offset, payload_size_);
  DCHECK_GT(size, 0);
  DCHECK_LE(offset + size, payload_size_);
  DCHECK(buffer != nullptr);

  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(page_id_ != DatabasePageReader::kInvalidPageId)
      << "Initialize() not called, or last call did not succeed";

  if (offset < inline_payload_size_) {
    // The read overlaps the payload bytes stored in the B-tree page.
    if (db_reader_->ReadPage(page_id_) != SQLITE_OK)
      return false;
    const int page_size = db_reader_->page_size();

    // The static_cast is safe because inline_payload_size_ is smaller than a
    // SQLite page size, which is a 32-bit integer.
    const int read_offset = inline_payload_offset_ + static_cast<int>(offset);
    DCHECK_LE(read_offset, page_size);

    const int read_size =
        (static_cast<int>(offset) + size <= inline_payload_size_)
            ? static_cast<int>(size)
            : inline_payload_size_ - static_cast<int>(offset);
    DCHECK_LE(read_offset + read_size, page_size);
    std::copy(db_reader_->page_data() + read_offset,
              db_reader_->page_data() + read_offset + read_size, buffer);

    if (read_size == size) {
      // The read is entirely inside the B-tree page.
      return true;
    }

    offset += read_size;
    DCHECK_EQ(offset, inline_payload_size_);
    DCHECK_GT(size, read_size);
    size -= read_size;
    buffer += read_size;
  }

  // The read is entirely in overflow pages.
  DCHECK_GE(offset, inline_payload_size_);
  while (size > 0) {
    const int overflow_page_index =
        (offset - inline_payload_size_) / max_overflow_payload_size_;
    DCHECK_LT(overflow_page_index, overflow_page_count_);
    const int overflow_page_offset =
        (offset - inline_payload_size_) % max_overflow_payload_size_;

    while (overflow_page_ids_.size() <=
           static_cast<size_t>(overflow_page_index)) {
      if (!PopulateNextOverflowPageId())
        return false;
    }

    const int page_id = overflow_page_ids_[overflow_page_index];
    if (db_reader_->ReadPage(page_id) != SQLITE_OK)
      return false;
    const int page_size = db_reader_->page_size();

    const int read_offset = kPageIdSize + overflow_page_offset;
    DCHECK_LE(read_offset, page_size);

    const int read_size = std::min<int64_t>(page_size - read_offset, size);
    DCHECK_LE(read_offset + read_size, page_size);
    std::copy(db_reader_->page_data() + read_offset,
              db_reader_->page_data() + read_offset + read_size, buffer);

    offset += read_size;
    DCHECK_GE(size, read_size);
    size -= read_size;
    buffer += read_size;
  }

  return true;
}

const uint8_t* LeafPayloadReader::ReadInlinePayload() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(page_id_ != DatabasePageReader::kInvalidPageId)
      << "Initialize() not called, or last call did not succeed";

  if (db_reader_->ReadPage(page_id_) != SQLITE_OK)
    return nullptr;
  return db_reader_->page_data() + inline_payload_offset_;
}

bool LeafPayloadReader::PopulateNextOverflowPageId() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_LT(overflow_page_ids_.size(),
            static_cast<size_t>(overflow_page_count_));

  int page_id_offset;
  if (overflow_page_ids_.empty()) {
    // The first overflow page ID is right after the payload's inline bytes.
    page_id_offset = inline_payload_offset_ + inline_payload_size_;
    if (db_reader_->ReadPage(page_id_) != SQLITE_OK)
      return false;
  } else {
    // Overflow pages start with the ID of the next overflow page.
    page_id_offset = 0;
    if (db_reader_->ReadPage(overflow_page_ids_.back()) != SQLITE_OK)
      return false;
  }

  DCHECK_LE(page_id_offset + kPageIdSize, db_reader_->page_size());
  const int next_page_id =
      LoadBigEndianInt32(db_reader_->page_data() + page_id_offset);
  if (!DatabasePageReader::IsValidPageId(next_page_id)) {
    // The overflow page is corrupted.
    return false;
  }

  overflow_page_ids_.push_back(next_page_id);
  return true;
}

}  // namespace recover
}  // namespace sql