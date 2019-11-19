// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sql/recover_module/btree.h"

#include <algorithm>
#include <limits>
#include <type_traits>

#include "base/logging.h"
#include "sql/recover_module/integers.h"
#include "sql/recover_module/pager.h"
#include "third_party/sqlite/sqlite3.h"

namespace sql {
namespace recover {

namespace {

// The SQLite database format is documented at the following URLs.
//   https://www.sqlite.org/fileformat.html
//   https://www.sqlite.org/fileformat2.html
constexpr uint8_t kInnerTablePageType = 0x05;
constexpr uint8_t kLeafTablePageType = 0x0D;

// Offset from the page header to the page type byte.
constexpr int kPageTypePageOffset = 0;
// Offset from the page header to the 2-byte cell count.
constexpr int kCellCountPageOffset = 3;
// Offset from an inner page header to the 4-byte last child page ID.
constexpr int kLastChildIdInnerPageOffset = 8;
// Offset from an inner page header to the cell pointer array.
constexpr int kFirstCellOfsetInnerPageOffset = 12;
// Offset from a leaf page header to the cell pointer array.
constexpr int kFirstCellOfsetLeafPageOffset = 8;

}  // namespace

#if !DCHECK_IS_ON()
// In DCHECKed builds, the decoder contains a sequence checker, which has a
// non-trivial destructor.
static_assert(std::is_trivially_destructible<InnerPageDecoder>::value,
              "Move the destructor to the .cc file if it's non-trival");
#endif  // !DCHECK_IS_ON()

InnerPageDecoder::InnerPageDecoder(DatabasePageReader* db_reader) noexcept
    : page_id_(db_reader->page_id()),
      db_reader_(db_reader),
      cell_count_(ComputeCellCount(db_reader)),
      next_read_index_(0) {
  DCHECK(IsOnValidPage(db_reader));
  DCHECK(DatabasePageReader::IsValidPageId(page_id_));
}

int InnerPageDecoder::TryAdvance() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(CanAdvance());

  const int sqlite_status = db_reader_->ReadPage(page_id_);
  if (sqlite_status != SQLITE_OK) {
    // TODO(pwnall): UMA the error code.

    next_read_index_ = cell_count_ + 1;  // End the reading process.
    return DatabasePageReader::kInvalidPageId;
  }

  const uint8_t* const page_data = db_reader_->page_data();
  const int read_index = next_read_index_;
  next_read_index_ += 1;
  if (read_index == cell_count_)
    return LoadBigEndianInt32(page_data + kLastChildIdInnerPageOffset);

  const int cell_pointer_offset =
      kFirstCellOfsetInnerPageOffset + (read_index << 1);
  DCHECK_LE(cell_pointer_offset + 2, db_reader_->page_size())
      << "ComputeCellCount() used an incorrect upper bound";
  const int cell_pointer = LoadBigEndianUint16(page_data + cell_pointer_offset);

  static_assert(std::numeric_limits<uint16_t>::max() + 4 <
                    std::numeric_limits<int>::max(),
                "The addition below may overflow");
  if (cell_pointer + 4 >= db_reader_->page_size()) {
    // Each cell needs 1 byte for the rowid varint, in addition to the 4 bytes
    // for the child page number that will be read below. Skip cells that
    // obviously go over the page end.
    return DatabasePageReader::kInvalidPageId;
  }
  if (cell_pointer < kFirstCellOfsetInnerPageOffset) {
    // The pointer points into the cell's header.
    return DatabasePageReader::kInvalidPageId;
  }

  return LoadBigEndianInt32(page_data + cell_pointer);
}

// static
bool InnerPageDecoder::IsOnValidPage(DatabasePageReader* db_reader) {
  static_assert(kPageTypePageOffset < DatabasePageReader::kMinUsablePageSize,
                "The check below may perform an out-of-bounds memory access");
  return db_reader->page_data()[kPageTypePageOffset] == kInnerTablePageType;
}

// static
int InnerPageDecoder::ComputeCellCount(DatabasePageReader* db_reader) {
  // The B-tree page header stores the cell count.
  int header_count =
      LoadBigEndianUint16(db_reader->page_data() + kCellCountPageOffset);
  static_assert(
      kCellCountPageOffset + 2 <= DatabasePageReader::kMinUsablePageSize,
      "The read above may be out of bounds");

  // However, the data may be corrupted. So, use an upper bound based on the
  // fact that the cell pointer array should never extend past the end of the
  // page.
  //
  // The page size is always even, because it is either a power of two, for
  // most pages, or a power of two minus 100, for the first database page. The
  // cell pointer array starts at offset 12. So, each cell pointer must be
  // separated from the page buffer's end by an even number of bytes.
  DCHECK((db_reader->page_size() - kFirstCellOfsetInnerPageOffset) % 2 == 0);
  int upper_bound =
      (db_reader->page_size() - kFirstCellOfsetInnerPageOffset) >> 1;
  static_assert(
      kFirstCellOfsetInnerPageOffset <= DatabasePageReader::kMinUsablePageSize,
      "The |upper_bound| computation above may overflow");

  return std::min(header_count, upper_bound);
}

#if !DCHECK_IS_ON()
// In DCHECKed builds, the decoder contains a sequence checker, which has a
// non-trivial destructor.
static_assert(std::is_trivially_destructible<LeafPageDecoder>::value,
              "Move the destructor to the .cc file if it's non-trival");
#endif  // !DCHECK_IS_ON()

LeafPageDecoder::LeafPageDecoder(DatabasePageReader* db_reader) noexcept
    : page_id_(db_reader->page_id()),
      db_reader_(db_reader),
      cell_count_(ComputeCellCount(db_reader)),
      next_read_index_(0),
      last_record_size_(0) {
  DCHECK(IsOnValidPage(db_reader));
  DCHECK(DatabasePageReader::IsValidPageId(page_id_));
}

bool LeafPageDecoder::TryAdvance() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(CanAdvance());

#if DCHECK_IS_ON()
  // DCHECKs use last_record_size == 0 to check for incorrect access to the
  // decoder's state.
  last_record_size_ = 0;
#endif  // DCHECK_IS_ON()

  const int sqlite_status = db_reader_->ReadPage(page_id_);
  if (sqlite_status != SQLITE_OK) {
    // TODO(pwnall): UMA the error code.

    next_read_index_ = cell_count_;  // End the reading process.
    return false;
  }

  const uint8_t* page_data = db_reader_->page_data();
  const int read_index = next_read_index_;
  next_read_index_ += 1;

  const int cell_pointer_offset =
      kFirstCellOfsetLeafPageOffset + (read_index << 1);
  DCHECK_LE(cell_pointer_offset + 2, db_reader_->page_size())
      << "ComputeCellCount() used an incorrect upper bound";
  const int cell_pointer = LoadBigEndianUint16(page_data + cell_pointer_offset);

  static_assert(std::numeric_limits<uint16_t>::max() + 3 <
                    std::numeric_limits<int>::max(),
                "The addition below may overflow");
  if (cell_pointer + 3 >= db_reader_->page_size()) {
    // Each cell needs at least 1 byte for page type varint, 1 byte for the
    // rowid varint, and 1 byte for the record header size varint. Skip cells
    // that obviously go over the page end.
    return false;
  }
  if (cell_pointer < kFirstCellOfsetLeafPageOffset) {
    // The pointer points into the cell's header.
    return false;
  }

  const uint8_t* const cell_start = page_data + cell_pointer;
  const uint8_t* const page_end = page_data + db_reader_->page_size();
  DCHECK_LT(cell_start, page_end) << "Failed to skip over empty cells";

  const uint8_t* rowid_start;
  std::tie(last_record_size_, rowid_start) = ParseVarint(cell_start, page_end);
  if (rowid_start == page_end) {
    // The value size varint extended to the end of the page, so the rowid
    // varint starts past the page end.
    return false;
  }
  if (last_record_size_ <= 0) {
    // Each payload needs at least one varint. Skip empty payloads.
#if DCHECK_IS_ON()
    // DCHECKs use last_record_size == 0 to check for incorrect access to the
    // decoder's state.
    last_record_size_ = 0;
#endif  // DCHECK_IS_ON()
    return false;
  }

  const uint8_t* record_start;
  std::tie(last_record_rowid_, record_start) =
      ParseVarint(rowid_start, page_end);
  if (record_start == page_end) {
    // The rowid varint extended to the end of the page, so the record starts
    // past the page end. Records need at least 1 byte for their header size
    // varint, so this suggests corruption.
    last_record_size_ = 0;
    return false;
  }

  last_record_offset_ = record_start - page_data;
  return true;
}

// static
bool LeafPageDecoder::IsOnValidPage(DatabasePageReader* db_reader) {
  static_assert(kPageTypePageOffset < DatabasePageReader::kMinUsablePageSize,
                "The check below may perform an out-of-bounds memory access");
  return db_reader->page_data()[kPageTypePageOffset] == kLeafTablePageType;
}

// static
int LeafPageDecoder::ComputeCellCount(DatabasePageReader* db_reader) {
  // See InnerPageDecoder::ComputeCellCount() for the reasoning behind the code.
  int header_count =
      LoadBigEndianUint16(db_reader->page_data() + kCellCountPageOffset);
  static_assert(
      kCellCountPageOffset + 2 <= DatabasePageReader::kMinUsablePageSize,
      "The read above may be out of bounds");

  int upper_bound =
      (db_reader->page_size() - kFirstCellOfsetLeafPageOffset) >> 1;
  static_assert(
      kFirstCellOfsetLeafPageOffset <= DatabasePageReader::kMinUsablePageSize,
      "The |upper_bound| computation above may overflow");

  return std::min(header_count, upper_bound);
}

}  // namespace recover
}  // namespace sql
