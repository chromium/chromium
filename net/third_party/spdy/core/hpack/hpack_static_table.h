// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_THIRD_PARTY_SPDY_CORE_HPACK_HPACK_STATIC_TABLE_H_
#define NET_THIRD_PARTY_SPDY_CORE_HPACK_HPACK_STATIC_TABLE_H_

#include "net/third_party/spdy/core/hpack/hpack_header_table.h"
#include "net/third_party/spdy/platform/api/spdy_export.h"

namespace spdy {

struct HpackStaticEntry;

// HpackStaticTable provides |static_entries_| and |static_index_| for HPACK
// encoding and decoding contexts.  Once initialized, an instance is read only
// and may be accessed only through its const interface.  Such an instance may
// be shared accross multiple HPACK contexts.
class SPDY_EXPORT_PRIVATE HpackStaticTable {
 public:
  HpackStaticTable();
  ~HpackStaticTable();

  // Prepares HpackStaticTable by filling up static_entries_ and static_index_
  // from an array of struct HpackStaticEntry.  Must be called exactly once.
  void Initialize(const HpackStaticEntry* static_entry_table,
                  size_t static_entry_count);

  // Returns whether Initialize() has been called.
  bool IsInitialized() const;

  // Accessors.
  const HpackHeaderTable::EntryTable& GetStaticEntries() const {
    return static_entries_;
  }
  const HpackHeaderTable::UnorderedEntrySet& GetStaticIndex() const {
    return static_index_;
  }
  const HpackHeaderTable::NameToEntryMap& GetStaticNameIndex() const {
    return static_name_index_;
  }

  // Returns the estimate of dynamically allocated memory in bytes.
  size_t EstimateMemoryUsage() const;

 private:
  HpackHeaderTable::EntryTable static_entries_;
  HpackHeaderTable::UnorderedEntrySet static_index_;
  HpackHeaderTable::NameToEntryMap static_name_index_;
};

}  // namespace spdy

#endif  // NET_THIRD_PARTY_SPDY_CORE_HPACK_HPACK_STATIC_TABLE_H_
