// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_THIRD_PARTY_QUIC_CORE_QPACK_QPACK_HEADER_TABLE_H_
#define NET_THIRD_PARTY_QUIC_CORE_QPACK_QPACK_HEADER_TABLE_H_

#include <cstddef>

#include "net/third_party/quic/platform/api/quic_export.h"
#include "net/third_party/quic/platform/api/quic_string_piece.h"
#include "net/third_party/spdy/core/hpack/hpack_entry.h"
#include "net/third_party/spdy/core/hpack/hpack_header_table.h"

namespace quic {

using QpackEntry = spdy::HpackEntry;

// This class manages the QPACK static and dynamic tables.
// TODO(bnc): Implement dynamic table.
class QUIC_EXPORT_PRIVATE QpackHeaderTable {
 public:
  using EntryTable = spdy::HpackHeaderTable::EntryTable;
  using EntryHasher = spdy::HpackHeaderTable::EntryHasher;
  using EntriesEq = spdy::HpackHeaderTable::EntriesEq;
  using UnorderedEntrySet = spdy::HpackHeaderTable::UnorderedEntrySet;
  using NameToEntryMap = spdy::HpackHeaderTable::NameToEntryMap;

  // Result of header table lookup.
  enum class MatchType { kNameAndValue, kName, kNoMatch };

  QpackHeaderTable();
  QpackHeaderTable(const QpackHeaderTable&) = delete;
  QpackHeaderTable& operator=(const QpackHeaderTable&) = delete;

  ~QpackHeaderTable();

  // Returns the entry at given index, or nullptr on error.
  const QpackEntry* LookupEntry(size_t index) const;

  // Returns the index of an entry with matching name and value if such exists,
  // otherwise one with matching name is such exists.
  MatchType FindHeaderField(QuicStringPiece name,
                            QuicStringPiece value,
                            size_t* index) const;

 private:
  // |static_entries_|, |static_index_|, |static_name_index_| are owned by
  // QpackStaticTable singleton.

  // Tracks QpackEntries by index.
  const EntryTable& static_entries_;

  // Tracks the unique QpackEntry for a given header name and value.
  const UnorderedEntrySet& static_index_;

  // Tracks the first static entry for each name in the static table.
  const NameToEntryMap& static_name_index_;
};

}  // namespace quic

#endif  // NET_THIRD_PARTY_QUIC_CORE_QPACK_QPACK_HEADER_TABLE_H_
