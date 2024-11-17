// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DISK_CACHE_MEMORY_MEM_ENTRY_IMPL_H_
#define NET_DISK_CACHE_MEMORY_MEM_ENTRY_IMPL_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/containers/linked_list.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/trace_event/memory_usage_estimator.h"
#include "net/base/interval.h"
#include "net/base/net_export.h"
#include "net/disk_cache/disk_cache.h"
#include "net/log/net_log_with_source.h"

namespace net {
class NetLog;
}

namespace disk_cache {

class MemBackendImpl;

// This class implements the Entry interface for the memory-only cache. An
// object of this class represents a single entry on the cache. We use two types
// of entries, parent and child to support sparse caching.
//
// A parent entry is non-sparse until a sparse method is invoked (i.e.
// ReadSparseData, WriteSparseData, GetAvailableRange) when sparse information
// is initialized. It then manages a list of child entries and delegates the
// sparse API calls to the child entries. It creates and deletes child entries
// and updates the list when needed.
//
// A child entry is used to carry partial cache content, non-sparse methods like
// ReadData and WriteData cannot be applied to them. The lifetime of a child
// entry is managed by the parent entry that created it except that the entry
// can be evicted independently. A child entry does not have a key and it is not
// registered in the backend's entry map.
//
// A sparse child entry has a fixed maximum size and can be partially
// filled. There can only be one continous filled region in a sparse entry, as
// illustrated by the following example:
// | xxx ooooo |
// x = unfilled region
// o = filled region
// It is guaranteed that there is at most one unfilled region and one filled
// region, and the unfilled region (if there is one) is always before the filled
// region. The book keeping for filled region in a sparse entry is done by using
// the variable |child_first_pos_|.

class NET_EXPORT_PRIVATE MemEntryImpl final
    : public Entry,
      public base::LinkNode<MemEntryImpl> {
 public:
  enum class EntryType {
    kParent,
    kChild,
  };

  // Provided to better document calls to |UpdateStateOnUse()|.
  enum EntryModified {
    ENTRY_WAS_NOT_MODIFIED,
    ENTRY_WAS_MODIFIED,
  };

  // Constructor for parent entries.
  MemEntryImpl(base::WeakPtr<MemBackendImpl> backend,
               const std::string& key,
               net::NetLog* net_log);

  // Constructor for child entries.
  MemEntryImpl(base::WeakPtr<MemBackendImpl> backend,
               int64_t child_id,
               MemEntryImpl* parent,
               net::NetLog* net_log);

  MemEntryImpl(const MemEntryImpl&) = delete;
  MemEntryImpl& operator=(const MemEntryImpl&) = delete;

  void Open();
  bool InUse() const;

  EntryType type() const {
    return parent_ ? EntryType::kChild : EntryType::kParent;
  }
  const std::string& key() const { return key_; }
  const MemEntryImpl* parent() const { return parent_; }
  int64_t child_id() const { return child_id_; }
  base::Time last_used() const { return last_used_; }

  // The in-memory size of this entry to use for the purposes of eviction.
  int GetStorageSize() const;

  // Update an entry's position in the backend LRU list and set |last_used_|. If
  // the entry was modified, also update |last_modified_|.
  void UpdateStateOnUse(EntryModified modified_enum);

  // From disk_cache::Entry:
  void Doom() override;
  void Close() override;
  std::string GetKey() const override;
  base::Time GetLastUsed() const override;
  base::Time GetLastModified() const override;
  int32_t GetDataSize(int index) const override;
  int ReadData(int index,
               int offset,
               IOBuffer* buf,
               int buf_len,
               CompletionOnceCallback callback) override;
  int WriteData(int index,
                int offset,
                IOBuffer* buf,
                int buf_len,
                CompletionOnceCallback callback,
                bool truncate) override;
  int ReadSparseData(int64_t offset,
                     IOBuffer* buf,
                     int buf_len,
                     CompletionOnceCallback callback) override;
  int WriteSparseData(int64_t offset,
                      IOBuffer* buf,
                      int buf_len,
                      CompletionOnceCallback callback) override;
  RangeResult GetAvailableRange(int64_t offset,
                                int len,
                                RangeResultCallback callback) override;
  bool CouldBeSparse() const override;
  void CancelSparseIO() override {}
  net::Error ReadyForSparseIO(CompletionOnceCallback callback) override;
  void SetLastUsedTimeForTest(base::Time time) override;

 private:
  MemEntryImpl(base::WeakPtr<MemBackendImpl> backend,
               const std::string& key,
               int64_t child_id,
               MemEntryImpl* parent,
               net::NetLog* net_log);

  using EntryMap = std::map<int64_t, raw_ptr<MemEntryImpl, CtnExperimental>>;

  static const int kNumStreams = 3;

  ~MemEntryImpl() override;

  // Do all the work for corresponding public functions.  Implemented as
  // separate functions to make logging of results simpler.
  int InternalReadData(int index, int offset, IOBuffer* buf, int buf_len);
  int InternalWriteData(int index, int offset, IOBuffer* buf, int buf_len,
                        bool truncate);
  int InternalReadSparseData(int64_t offset, IOBuffer* buf, int buf_len);
  int InternalWriteSparseData(int64_t offset, IOBuffer* buf, int buf_len);
  RangeResult InternalGetAvailableRange(int64_t offset, int len);

  // Initializes the children map and sparse info. This method is only called
  // on a parent entry.
  bool InitSparseInfo();

  // Returns an entry responsible for |offset|. The returned entry can be a
  // child entry or this entry itself if |offset| points to the first range.
  // If such entry does not exist and |create| is true, a new child entry is
  // created.
  MemEntryImpl* GetChild(int64_t offset, bool create);

  // Returns an interval describing what's stored in the child entry pointed to
  // by i, in global coordinates.
  // Precondition: i != children_.end();
  net::Interval<int64_t> ChildInterval(
      MemEntryImpl::EntryMap::const_iterator i);

  // Compact vectors to try to avoid over-allocation due to exponential growth.
  void Compact();

  std::string key_;
  std::vector<char> data_[kNumStreams];  // User data.
  uint32_t ref_count_ = 0;

  int64_t child_id_;     // The ID of a child entry.
  int child_first_pos_ = 0;  // The position of the first byte in a child
                             // entry. 0 here is beginning of child, not of
                             // the entire file.
  // Pointer to the parent entry, or nullptr if this entry is a parent entry.
  raw_ptr<MemEntryImpl> parent_;
  std::unique_ptr<EntryMap> children_;

  base::Time last_modified_;
  base::Time last_used_;
  base::WeakPtr<MemBackendImpl> backend_;  // Back pointer to the cache.
  bool doomed_ = false;  // True if this entry was removed from the cache.

  net::NetLogWithSource net_log_;
};

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_MEMORY_MEM_ENTRY_IMPL_H_
