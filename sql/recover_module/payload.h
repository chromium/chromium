// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SQL_RECOVER_MODULE_PAYLOAD_H_
#define SQL_RECOVER_MODULE_PAYLOAD_H_

#include <cstdint>
#include <vector>

#include "base/logging.h"
#include "base/sequence_checker.h"
#include "sql/recover_module/pager.h"

namespace sql {
namespace recover {

class DatabasePageReader;

// Reads payloads (records) across B-tree pages and overflow pages.
//
// Instances are designed to be reused for reading multiple payloads. Instances
// are not thread-safe.
//
// Reading a payload is started by calling Initialize() with the information
// from LeafPageDecoder. If the call succeeds, ReadPayload() can be called
// repeatedly.
class LeafPayloadReader {
 public:
  // Number of payload bytes guaranteed to be on the B-tree page.
  //
  // The value is derived from the minimum SQLite usable page size, which is
  // 380 bytes, and the formula for the minimum payload size given a usable page
  // size.
  static constexpr int kMinInlineSize = ((380 - 12) * 32) / 255 - 23;

  explicit LeafPayloadReader(DatabasePageReader* db_reader);
  ~LeafPayloadReader();

  LeafPayloadReader(const LeafPayloadReader&) = delete;
  LeafPayloadReader& operator=(const LeafPayloadReader&) = delete;

  // Sets up the reader for a new payload.
  //
  // The DatabasePageReader passed to the constructor must be focused on the
  // page containing the payload.
  //
  // This method must complete successfully before any other method on this
  // class can be called.
  bool Initialize(int64_t payload_size, int payload_offset);

  // The number of payload bytes that are stored on the B-tree page.
  //
  // The return value is guaranteed to be non-negative and at most
  // payload_size().
  int inline_payload_size() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK(page_id_ != DatabasePageReader::kInvalidPageId)
        << "Initialize() not called, or last call did not succeed";
    DCHECK_LE(inline_payload_size_, payload_size_);
    return inline_payload_size_;
  }

  // Total payload size, in bytes.
  //
  // This includes the bytes stored in the B-tree page, as well as any bytes
  // stored in overflow pages.
  //
  // The return value is guaranteed to be at least inline_payload_size().
  int payload_size() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK(page_id_ != DatabasePageReader::kInvalidPageId)
        << "Initialize() not called, or last call did not succeed";
    DCHECK_LE(inline_payload_size_, payload_size_);
    return payload_size_;
  }

  // Copies a subset of the payload into a given buffer.
  //
  // Returns true if the read succeeds.
  //
  // May only be called after a previous call to Initialize() that returns true.
  bool ReadPayload(int64_t offset, int64_t size, uint8_t* buffer);

  // Pulls the B-tree containing the payload into the database reader's cache.
  //
  // Returns a pointer to the beginning of the payload bytes. The pointer is
  // inside the database reader's buffer, and may get invalidated if the
  // database reader is used.
  //
  // Returns null if the read operation fails.
  //
  // May only be called after a previous call to Initialize() that returns true.
  const uint8_t* ReadInlinePayload();

 private:
  // Extends the cached list of overflow page IDs by one page.
  //
  // Returns false if the operation failed. Failures are due to read errors or
  // database corruption.
  bool PopulateNextOverflowPageId();

  // Used to read the pages containing the payload.
  //
  // Raw pointer usage is acceptable because this instance's owner is expected
  // to ensure that the DatabasePageReader outlives this.
  DatabasePageReader* const db_reader_;

  // Total size of the current payload.
  int64_t payload_size_;

  // The ID of the B-tree page containing the current payload's inline bytes.
  //
  // Set to kInvalidPageId if the reader wasn't successfully initialized.
  int page_id_;

  // The start of the current payload's inline bytes on the B-tree page.
  //
  // Large payloads extend past the B-tree page containing the payload, via
  // overflow pages.
  int inline_payload_offset_;

  // Number of bytes in the current payload stored in its B-tree page.
  //
  // The rest of the payload is stored on overflow pages.
  int inline_payload_size_;

  // Number of overflow pages used by the payload.
  int overflow_page_count_;

  // Number of bytes in each overflow page that stores the payload.
  int max_overflow_payload_size_;

  // Page IDs for all the payload's overflow pages, in order.
  //
  // This list is populated on-demand.
  std::vector<int> overflow_page_ids_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace recover
}  // namespace sql

#endif  // SQL_RECOVER_MODULE_PAYLOAD_H_
