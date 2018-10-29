// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/quic/core/qpack/qpack_header_table.h"

#include "base/logging.h"
#include "net/third_party/quic/core/qpack/qpack_static_table.h"

namespace quic {

QpackHeaderTable::QpackHeaderTable()
    : static_entries_(ObtainQpackStaticTable().GetStaticEntries()),
      static_index_(ObtainQpackStaticTable().GetStaticIndex()),
      static_name_index_(ObtainQpackStaticTable().GetStaticNameIndex()) {}

QpackHeaderTable::~QpackHeaderTable() = default;

const QpackEntry* QpackHeaderTable::LookupEntry(size_t index) const {
  if (index >= static_entries_.size()) {
    return nullptr;
  }

  return &static_entries_[index];
}

QpackHeaderTable::MatchType QpackHeaderTable::FindHeaderField(
    QuicStringPiece name,
    QuicStringPiece value,
    size_t* index) const {
  QpackEntry query(name, value);
  auto static_index_it = static_index_.find(&query);
  if (static_index_it != static_index_.end()) {
    DCHECK((*static_index_it)->IsStatic());
    *index = (*static_index_it)->InsertionIndex();
    return MatchType::kNameAndValue;
  }

  auto static_name_index_it = static_name_index_.find(name);
  if (static_name_index_it != static_name_index_.end()) {
    DCHECK(static_name_index_it->second->IsStatic());
    *index = static_name_index_it->second->InsertionIndex();
    return MatchType::kName;
  }

  return MatchType::kNoMatch;
}

}  // namespace quic
