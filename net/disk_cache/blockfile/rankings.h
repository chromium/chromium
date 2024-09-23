// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// See net/disk_cache/disk_cache.h for the public interface.

#ifndef NET_DISK_CACHE_BLOCKFILE_RANKINGS_H_
#define NET_DISK_CACHE_BLOCKFILE_RANKINGS_H_

#include <list>
#include <memory>

#include <array>
#include "base/memory/raw_ptr.h"
#include "net/disk_cache/blockfile/addr.h"
#include "net/disk_cache/blockfile/mapped_file.h"
#include "net/disk_cache/blockfile/storage_block.h"

namespace disk_cache {

class BackendImpl;
struct LruData;
struct RankingsNode;
typedef StorageBlock<RankingsNode> CacheRankingsBlock;

// Type of crashes generated for the unit tests.
enum RankCrashes {
  NO_CRASH = 0,
  INSERT_EMPTY_1,
  INSERT_EMPTY_2,
  INSERT_EMPTY_3,
  INSERT_ONE_1,
  INSERT_ONE_2,
  INSERT_ONE_3,
  INSERT_LOAD_1,
  INSERT_LOAD_2,
  REMOVE_ONE_1,
  REMOVE_ONE_2,
  REMOVE_ONE_3,
  REMOVE_ONE_4,
  REMOVE_HEAD_1,
  REMOVE_HEAD_2,
  REMOVE_HEAD_3,
  REMOVE_HEAD_4,
  REMOVE_TAIL_1,
  REMOVE_TAIL_2,
  REMOVE_TAIL_3,
  REMOVE_LOAD_1,
  REMOVE_LOAD_2,
  REMOVE_LOAD_3,
  MAX_CRASH
};

// This class handles the ranking information for the cache.
class Rankings {
 public:
  // Possible lists of entries.
  enum List {
    NO_USE = 0,   // List of entries that have not been reused.
    LOW_USE,      // List of entries with low reuse.
    HIGH_USE,     // List of entries with high reuse.
    RESERVED,     // Reserved for future use.
    DELETED,      // List of recently deleted or doomed entries.
    LAST_ELEMENT
  };

  // This class provides a specialized version of scoped_ptr, that calls
  // Rankings whenever a CacheRankingsBlock is deleted, to keep track of cache
  // iterators that may go stale.
  class ScopedRankingsBlock : public std::unique_ptr<CacheRankingsBlock> {
   public:
    ScopedRankingsBlock();
    explicit ScopedRankingsBlock(Rankings* rankings);
    ScopedRankingsBlock(Rankings* rankings, CacheRankingsBlock* node);

    ScopedRankingsBlock(const ScopedRankingsBlock&) = delete;
    ScopedRankingsBlock& operator=(const ScopedRankingsBlock&) = delete;

    ~ScopedRankingsBlock() {
      rankings_->FreeRankingsBlock(get());
    }

    void set_rankings(Rankings* rankings) {
      rankings_ = rankings;
    }

    // scoped_ptr::reset will delete the object.
    void reset(CacheRankingsBlock* p = nullptr) {
      if (p != get())
        rankings_->FreeRankingsBlock(get());
      std::unique_ptr<CacheRankingsBlock>::reset(p);
    }

   private:
    raw_ptr<Rankings> rankings_;
  };

  // If we have multiple lists, we have to iterate through all at the same time.
  // This structure keeps track of where we are on the iteration.
  // TODO(crbug.com/40889343) refactor this struct to make it clearer
  // this owns the `nodes`.
  struct Iterator {
    Iterator();
    void Reset();

    // Which entry was returned to the user.
    List list = List::NO_USE;
    // Nodes on the first three lists.
    std::array<CacheRankingsBlock*, 3> nodes = {nullptr, nullptr, nullptr};
    raw_ptr<Rankings> my_rankings = nullptr;
  };

  Rankings();

  Rankings(const Rankings&) = delete;
  Rankings& operator=(const Rankings&) = delete;

  ~Rankings();

  bool Init(BackendImpl* backend, bool count_lists);

  // Restores original state, leaving the object ready for initialization.
  void Reset();

  // Inserts a given entry at the head of the queue.
  void Insert(CacheRankingsBlock* node, bool modified, List list);

  // Removes a given entry from the LRU list. If |strict| is true, this method
  // assumes that |node| is not pointed to by an active iterator. On the other
  // hand, removing that restriction allows the current "head" of an iterator
  // to be removed from the list (basically without control of the code that is
  // performing the iteration), so it should be used with extra care.
  void Remove(CacheRankingsBlock* node, List list, bool strict);

  // Moves a given entry to the head.
  void UpdateRank(CacheRankingsBlock* node, bool modified, List list);

  // Iterates through the list.
  CacheRankingsBlock* GetNext(CacheRankingsBlock* node, List list);
  CacheRankingsBlock* GetPrev(CacheRankingsBlock* node, List list);
  void FreeRankingsBlock(CacheRankingsBlock* node);

  // Controls tracking of nodes used for enumerations.
  void TrackRankingsBlock(CacheRankingsBlock* node, bool start_tracking);

  // Peforms a simple self-check of the lists, and returns the number of items
  // or an error code (negative value).
  int SelfCheck();

  // Returns false if the entry is clearly invalid. from_list is true if the
  // node comes from the LRU list.
  bool SanityCheck(CacheRankingsBlock* node, bool from_list) const;
  bool DataSanityCheck(CacheRankingsBlock* node, bool from_list) const;

  // Sets the |contents| field of |node| to |address|.
  void SetContents(CacheRankingsBlock* node, CacheAddr address);

 private:
  typedef std::pair<CacheAddr, CacheRankingsBlock*> IteratorPair;
  typedef std::list<IteratorPair> IteratorList;

  void ReadHeads();
  void ReadTails();
  void WriteHead(List list);
  void WriteTail(List list);

  // Gets the rankings information for a given rankings node. We may end up
  // sharing the actual memory with a loaded entry, but we are not taking a
  // reference to that entry, so |rankings| must be short lived.
  bool GetRanking(CacheRankingsBlock* rankings);

  // Makes |rankings| suitable to live a long life.
  void ConvertToLongLived(CacheRankingsBlock* rankings);

  // Finishes a list modification after a crash.
  void CompleteTransaction();
  void FinishInsert(CacheRankingsBlock* rankings);
  void RevertRemove(CacheRankingsBlock* rankings);

  // Returns false if node is not properly linked. This method may change the
  // provided |list| to reflect the list where this node is actually stored.
  bool CheckLinks(CacheRankingsBlock* node, CacheRankingsBlock* prev,
                  CacheRankingsBlock* next, List* list);

  // Checks the links between two consecutive nodes.
  bool CheckSingleLink(CacheRankingsBlock* prev, CacheRankingsBlock* next);

  // Peforms a simple check of the list, and returns the number of items or an
  // error code (negative value).
  int CheckList(List list);

  // Walks a list in the desired direction until the nodes |end1| or |end2| are
  // reached. Returns an error code (0 on success), the number of items verified
  // and the addresses of the last nodes visited.
  int CheckListSection(List list, Addr end1, Addr end2, bool forward,
                       Addr* last, Addr* second_last, int* num_items);

  // Returns true if addr is the head or tail of any list. When there is a
  // match |list| will contain the list number for |addr|.
  bool IsHead(CacheAddr addr, List* list) const;
  bool IsTail(CacheAddr addr, List* list) const;

  // Updates the iterators whenever node is being changed.
  void UpdateIterators(CacheRankingsBlock* node);

  // Updates the iterators when node at address |addr| is being removed to point
  // to |next| instead.
  void UpdateIteratorsForRemoved(CacheAddr addr, CacheRankingsBlock* next);

  // Keeps track of the number of entries on a list.
  void IncrementCounter(List list);
  void DecrementCounter(List list);

  bool init_ = false;
  bool count_lists_;
  Addr heads_[LAST_ELEMENT];
  Addr tails_[LAST_ELEMENT];
  raw_ptr<BackendImpl> backend_;

  // Data related to the LRU lists.
  // May point to a mapped file's unmapped memory at destruction time.
  raw_ptr<LruData, DisableDanglingPtrDetection> control_data_;

  IteratorList iterators_;
};

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_BLOCKFILE_RANKINGS_H_
