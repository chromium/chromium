// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/spdy/core/hpack/hpack_header_table.h"

#include <algorithm>

#include "base/logging.h"
#include "net/third_party/spdy/core/hpack/hpack_constants.h"
#include "net/third_party/spdy/core/hpack/hpack_static_table.h"
#include "net/third_party/spdy/platform/api/spdy_estimate_memory_usage.h"

namespace spdy {

size_t HpackHeaderTable::EntryHasher::operator()(
    const HpackEntry* entry) const {
  return base::StringPieceHash()(entry->name()) ^
         base::StringPieceHash()(entry->value());
}

bool HpackHeaderTable::EntriesEq::operator()(const HpackEntry* lhs,
                                             const HpackEntry* rhs) const {
  if (lhs == nullptr) {
    return rhs == nullptr;
  }
  if (rhs == nullptr) {
    return false;
  }
  return lhs->name() == rhs->name() && lhs->value() == rhs->value();
}

HpackHeaderTable::HpackHeaderTable()
    : static_entries_(ObtainHpackStaticTable().GetStaticEntries()),
      static_index_(ObtainHpackStaticTable().GetStaticIndex()),
      static_name_index_(ObtainHpackStaticTable().GetStaticNameIndex()),
      settings_size_bound_(kDefaultHeaderTableSizeSetting),
      size_(0),
      max_size_(kDefaultHeaderTableSizeSetting),
      total_insertions_(static_entries_.size()) {}

HpackHeaderTable::~HpackHeaderTable() = default;

const HpackEntry* HpackHeaderTable::GetByIndex(size_t index) {
  if (index == 0) {
    return nullptr;
  }
  index -= 1;
  if (index < static_entries_.size()) {
    return &static_entries_[index];
  }
  index -= static_entries_.size();
  if (index < dynamic_entries_.size()) {
    const HpackEntry* result = &dynamic_entries_[index];
    if (debug_visitor_ != nullptr) {
      debug_visitor_->OnUseEntry(*result);
    }
    return result;
  }
  return nullptr;
}

const HpackEntry* HpackHeaderTable::GetByName(SpdyStringPiece name) {
  {
    auto it = static_name_index_.find(name);
    if (it != static_name_index_.end()) {
      return it->second;
    }
  }
  {
    NameToEntryMap::const_iterator it = dynamic_name_index_.find(name);
    if (it != dynamic_name_index_.end()) {
      const HpackEntry* result = it->second;
      if (debug_visitor_ != nullptr) {
        debug_visitor_->OnUseEntry(*result);
      }
      return result;
    }
  }
  return nullptr;
}

const HpackEntry* HpackHeaderTable::GetByNameAndValue(SpdyStringPiece name,
                                                      SpdyStringPiece value) {
  HpackEntry query(name, value);
  {
    auto it = static_index_.find(&query);
    if (it != static_index_.end()) {
      return *it;
    }
  }
  {
    auto it = dynamic_index_.find(&query);
    if (it != dynamic_index_.end()) {
      const HpackEntry* result = *it;
      if (debug_visitor_ != nullptr) {
        debug_visitor_->OnUseEntry(*result);
      }
      return result;
    }
  }
  return nullptr;
}

size_t HpackHeaderTable::IndexOf(const HpackEntry* entry) const {
  if (entry->IsLookup()) {
    return 0;
  } else if (entry->IsStatic()) {
    return 1 + entry->InsertionIndex();
  } else {
    return total_insertions_ - entry->InsertionIndex() + static_entries_.size();
  }
}

void HpackHeaderTable::SetMaxSize(size_t max_size) {
  CHECK_LE(max_size, settings_size_bound_);

  max_size_ = max_size;
  if (size_ > max_size_) {
    Evict(EvictionCountToReclaim(size_ - max_size_));
    CHECK_LE(size_, max_size_);
  }
}

void HpackHeaderTable::SetSettingsHeaderTableSize(size_t settings_size) {
  settings_size_bound_ = settings_size;
  SetMaxSize(settings_size_bound_);
}

void HpackHeaderTable::EvictionSet(SpdyStringPiece name,
                                   SpdyStringPiece value,
                                   EntryTable::iterator* begin_out,
                                   EntryTable::iterator* end_out) {
  size_t eviction_count = EvictionCountForEntry(name, value);
  *begin_out = dynamic_entries_.end() - eviction_count;
  *end_out = dynamic_entries_.end();
}

size_t HpackHeaderTable::EvictionCountForEntry(SpdyStringPiece name,
                                               SpdyStringPiece value) const {
  size_t available_size = max_size_ - size_;
  size_t entry_size = HpackEntry::Size(name, value);

  if (entry_size <= available_size) {
    // No evictions are required.
    return 0;
  }
  return EvictionCountToReclaim(entry_size - available_size);
}

size_t HpackHeaderTable::EvictionCountToReclaim(size_t reclaim_size) const {
  size_t count = 0;
  for (auto it = dynamic_entries_.rbegin();
       it != dynamic_entries_.rend() && reclaim_size != 0; ++it, ++count) {
    reclaim_size -= std::min(reclaim_size, it->Size());
  }
  return count;
}

void HpackHeaderTable::Evict(size_t count) {
  for (size_t i = 0; i != count; ++i) {
    CHECK(!dynamic_entries_.empty());
    HpackEntry* entry = &dynamic_entries_.back();

    size_ -= entry->Size();
    auto it = dynamic_index_.find(entry);
    DCHECK(it != dynamic_index_.end());
    // Only remove an entry from the index if its insertion index matches;
    // otherwise, the index refers to another entry with the same name and
    // value.
    if ((*it)->InsertionIndex() == entry->InsertionIndex()) {
      dynamic_index_.erase(it);
    }
    auto name_it = dynamic_name_index_.find(entry->name());
    DCHECK(name_it != dynamic_name_index_.end());
    // Only remove an entry from the literal index if its insertion index
    /// matches; otherwise, the index refers to another entry with the same
    // name.
    if (name_it->second->InsertionIndex() == entry->InsertionIndex()) {
      dynamic_name_index_.erase(name_it);
    }
    dynamic_entries_.pop_back();
  }
}

const HpackEntry* HpackHeaderTable::TryAddEntry(SpdyStringPiece name,
                                                SpdyStringPiece value) {
  Evict(EvictionCountForEntry(name, value));

  size_t entry_size = HpackEntry::Size(name, value);
  if (entry_size > (max_size_ - size_)) {
    // Entire table has been emptied, but there's still insufficient room.
    DCHECK(dynamic_entries_.empty());
    DCHECK_EQ(0u, size_);
    return nullptr;
  }
  dynamic_entries_.push_front(HpackEntry(name, value,
                                         false,  // is_static
                                         total_insertions_));
  HpackEntry* new_entry = &dynamic_entries_.front();
  auto index_result = dynamic_index_.insert(new_entry);
  if (!index_result.second) {
    // An entry with the same name and value already exists in the dynamic
    // index. We should replace it with the newly added entry.
    DVLOG(1) << "Found existing entry: "
             << (*index_result.first)->GetDebugString()
             << " replacing with: " << new_entry->GetDebugString();
    DCHECK_GT(new_entry->InsertionIndex(),
              (*index_result.first)->InsertionIndex());
    dynamic_index_.erase(index_result.first);
    CHECK(dynamic_index_.insert(new_entry).second);
  }

  auto name_result =
      dynamic_name_index_.insert(std::make_pair(new_entry->name(), new_entry));
  if (!name_result.second) {
    // An entry with the same name already exists in the dynamic index. We
    // should replace it with the newly added entry.
    DVLOG(1) << "Found existing entry: "
             << name_result.first->second->GetDebugString()
             << " replacing with: " << new_entry->GetDebugString();
    DCHECK_GT(new_entry->InsertionIndex(),
              name_result.first->second->InsertionIndex());
    dynamic_name_index_.erase(name_result.first);
    auto insert_result = dynamic_name_index_.insert(
        std::make_pair(new_entry->name(), new_entry));
    CHECK(insert_result.second);
  }

  size_ += entry_size;
  ++total_insertions_;
  if (debug_visitor_ != nullptr) {
    // Call |debug_visitor_->OnNewEntry()| to get the current time.
    HpackEntry& entry = dynamic_entries_.front();
    entry.set_time_added(debug_visitor_->OnNewEntry(entry));
    DVLOG(2) << "HpackHeaderTable::OnNewEntry: name=" << entry.name()
             << ",  value=" << entry.value()
             << ",  insert_index=" << entry.InsertionIndex()
             << ",  time_added=" << entry.time_added();
  }

  return &dynamic_entries_.front();
}

void HpackHeaderTable::DebugLogTableState() const {
  DVLOG(2) << "Dynamic table:";
  for (auto it = dynamic_entries_.begin(); it != dynamic_entries_.end(); ++it) {
    DVLOG(2) << "  " << it->GetDebugString();
  }
  DVLOG(2) << "Full Static Index:";
  for (const auto* entry : static_index_) {
    DVLOG(2) << "  " << entry->GetDebugString();
  }
  DVLOG(2) << "Full Static Name Index:";
  for (const auto it : static_name_index_) {
    DVLOG(2) << "  " << it.first << ": " << it.second->GetDebugString();
  }
  DVLOG(2) << "Full Dynamic Index:";
  for (const auto* entry : dynamic_index_) {
    DVLOG(2) << "  " << entry->GetDebugString();
  }
  DVLOG(2) << "Full Dynamic Name Index:";
  for (const auto it : dynamic_name_index_) {
    DVLOG(2) << "  " << it.first << ": " << it.second->GetDebugString();
  }
}

size_t HpackHeaderTable::EstimateMemoryUsage() const {
  return SpdyEstimateMemoryUsage(dynamic_entries_) +
         SpdyEstimateMemoryUsage(dynamic_index_) +
         SpdyEstimateMemoryUsage(dynamic_name_index_);
}

}  // namespace spdy
