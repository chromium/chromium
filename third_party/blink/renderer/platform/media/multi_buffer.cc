// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/media/multi_buffer.h"

#include <utility>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/not_fatal_until.h"
#include "base/task/single_thread_task_runner.h"

namespace blink {

// Prune 80 blocks per 30 seconds.
// This means a full cache will go away in ~5 minutes.
enum {
  kBlockPruneInterval = 30,
  kBlocksPrunedPerInterval = 80,
};

// Returns the block ID closest to (but less or equal than) |pos| from |index|.
template <class T>
static MultiBuffer::BlockId ClosestPreviousEntry(
    const std::map<MultiBuffer::BlockId, T>& index,
    MultiBuffer::BlockId pos) {
  auto i = index.upper_bound(pos);
  DCHECK(i == index.end() || i->first > pos);
  if (i == index.begin()) {
    return std::numeric_limits<MultiBufferBlockId>::min();
  }
  --i;
  DCHECK_LE(i->first, pos);
  return i->first;
}

// Returns the block ID closest to (but greter than or equal to) |pos|
// from |index|.
template <class T>
static MultiBuffer::BlockId ClosestNextEntry(
    const std::map<MultiBuffer::BlockId, T>& index,
    MultiBuffer::BlockId pos) {
  auto i = index.lower_bound(pos);
  if (i == index.end()) {
    return std::numeric_limits<MultiBufferBlockId>::max();
  }
  DCHECK_GE(i->first, pos);
  return i->first;
}

//
// MultiBuffer::GlobalLRU
//
MultiBuffer::GlobalLRU::GlobalLRU(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : max_size_(0),
      data_size_(0),
      background_pruning_pending_(false),
      lru_(lru_.NO_AUTO_EVICT),
      task_runner_(std::move(task_runner)) {}

MultiBuffer::GlobalLRU::~GlobalLRU() {
  // By the time we're freed, all blocks should have been removed,
  // and our sums should be zero.
  DCHECK(lru_.empty());
  DCHECK_EQ(max_size_, 0);
  DCHECK_EQ(data_size_, 0);
}

void MultiBuffer::GlobalLRU::Use(MultiBuffer* multibuffer,
                                 MultiBufferBlockId block_id) {
  lru_.Put(GlobalBlockId{multibuffer, block_id});
  SchedulePrune();
}

void MultiBuffer::GlobalLRU::Insert(MultiBuffer* multibuffer,
                                    MultiBufferBlockId block_id) {
  lru_.Put(GlobalBlockId{multibuffer, block_id});
  SchedulePrune();
}

void MultiBuffer::GlobalLRU::Remove(MultiBuffer* multibuffer,
                                    MultiBufferBlockId block_id) {
  GlobalBlockId id(multibuffer, block_id);
  auto iter = lru_.Peek(id);
  if (iter != lru_.end()) {
    lru_.Erase(iter);
  }
}

bool MultiBuffer::GlobalLRU::Contains(MultiBuffer* multibuffer,
                                      MultiBufferBlockId block_id) {
  return lru_.Peek(GlobalBlockId{multibuffer, block_id}) != lru_.end();
}

void MultiBuffer::GlobalLRU::IncrementDataSize(int64_t blocks) {
  data_size_ += blocks;
  DCHECK_GE(data_size_, 0);
  SchedulePrune();
}

void MultiBuffer::GlobalLRU::IncrementMaxSize(int64_t blocks) {
  max_size_ += blocks;
  DCHECK_GE(max_size_, 0);
  SchedulePrune();
}

bool MultiBuffer::GlobalLRU::Pruneable() const {
  return data_size_ > max_size_ && !lru_.empty();
}

void MultiBuffer::GlobalLRU::SchedulePrune() {
  if (Pruneable() && !background_pruning_pending_) {
    task_runner_->PostDelayedTask(
        FROM_HERE, base::BindOnce(&MultiBuffer::GlobalLRU::PruneTask, this),
        base::Seconds(kBlockPruneInterval));
    background_pruning_pending_ = true;
  }
}

void MultiBuffer::GlobalLRU::PruneTask() {
  background_pruning_pending_ = false;
  Prune(kBlocksPrunedPerInterval);
  SchedulePrune();
}

void MultiBuffer::GlobalLRU::TryFree(int64_t max_to_free) {
  // We group the blocks by multibuffer so that we can free as many blocks as
  // possible in one call. This reduces the number of callbacks to clients
  // when their available ranges change.
  std::map<MultiBuffer*, std::vector<MultiBufferBlockId>> to_free;
  int64_t freed = 0;
  auto lru_iter = lru_.rbegin();
  while (lru_iter != lru_.rend() && freed < max_to_free) {
    GlobalBlockId block_id = *lru_iter;
    to_free[block_id.first].push_back(block_id.second);
    lru_iter = lru_.Erase(lru_iter);
    freed++;
  }
  for (const auto& to_free_pair : to_free) {
    to_free_pair.first->ReleaseBlocks(to_free_pair.second);
  }
}

void MultiBuffer::GlobalLRU::TryFreeAll() {
  // Since TryFree also allocates memory, avoid freeing everything
  // in one large chunk to avoid running out of memory before we
  // start freeing memory. Freeing 100 at a time should be a reasonable
  // compromise between efficiency and not building large data structures.
  while (true) {
    int64_t data_size_before = data_size_;
    TryFree(100);
    if (data_size_ >= data_size_before)
      break;
  }
}

void MultiBuffer::GlobalLRU::Prune(int64_t max_to_free) {
  TryFree(std::min(max_to_free, data_size_ - max_size_));
}

int64_t MultiBuffer::GlobalLRU::Size() const {
  return lru_.size();
}

//
// MultiBuffer
//
MultiBuffer::MultiBuffer(int32_t block_size_shift,
                         scoped_refptr<GlobalLRU> global_lru)
    : max_size_(0),
      block_size_shift_(block_size_shift),
      lru_(std::move(global_lru)) {}

MultiBuffer::~MultiBuffer() {
  CHECK(pinned_.empty());
  DCHECK_EQ(max_size_, 0);
  // Remove all blocks from the LRU.
  for (const auto& i : data_) {
    lru_->Remove(this, i.first);
  }
  lru_->IncrementDataSize(-static_cast<int64_t>(data_.size()));
  lru_->IncrementMaxSize(-max_size_);
}

void MultiBuffer::AddReader(const BlockId& pos, Reader* reader) {
  std::set<raw_ptr<Reader, SetExperimental>>* set_of_readers = &readers_[pos];
  bool already_waited_for = !set_of_readers->empty();
  set_of_readers->insert(reader);

  if (already_waited_for || Contains(pos)) {
    return;
  }

  // We may need to create a new data provider to service this request.
  // Look for an existing data provider first.
  DataProvider* provider = nullptr;
  BlockId closest_writer = ClosestPreviousEntry(writer_index_, pos);

  if (closest_writer > pos - kMaxWaitForWriterOffset) {
    auto i = present_.find(pos);
    BlockId closest_block;
    if (i.value()) {
      // Shouldn't happen, we already tested that Contains(pos) is true.
      NOTREACHED_IN_MIGRATION();
      closest_block = pos;
    } else if (i == present_.begin()) {
      closest_block = -1;
    } else {
      closest_block = i.interval_begin() - 1;
    }

    // Make sure that there are no present blocks between the writer and
    // the requested position, as that will cause the writer to quit.
    if (closest_writer > closest_block) {
      provider = writer_index_[closest_writer].get();
      DCHECK(provider);
    }
  }
  if (!provider) {
    DCHECK(!base::Contains(writer_index_, pos));
    writer_index_[pos] = CreateWriter(pos, is_client_audio_element_);
    provider = writer_index_[pos].get();
  }
  provider->SetDeferred(false);
}

void MultiBuffer::RemoveReader(const BlockId& pos, Reader* reader) {
  auto i = readers_.find(pos);
  if (i == readers_.end())
    return;
  i->second.erase(reader);
  if (i->second.empty()) {
    readers_.erase(i);
  }
}

void MultiBuffer::CleanupWriters(const BlockId& pos) {
  BlockId p2 = pos + kMaxWaitForReaderOffset;
  BlockId closest_writer = ClosestPreviousEntry(writer_index_, p2);
  while (closest_writer > pos - kMaxWaitForWriterOffset) {
    DCHECK(writer_index_[closest_writer]);
    OnDataProviderEvent(writer_index_[closest_writer].get());
    closest_writer = ClosestPreviousEntry(writer_index_, closest_writer - 1);
  }
}

bool MultiBuffer::Contains(const BlockId& pos) const {
  DCHECK(present_[pos] == 0 || present_[pos] == 1)
      << " pos = " << pos << " present_[pos] " << present_[pos];
  DCHECK_EQ(present_[pos], base::Contains(data_, pos) ? 1 : 0);
  return !!present_[pos];
}

MultiBufferBlockId MultiBuffer::FindNextUnavailable(const BlockId& pos) const {
  auto i = present_.find(pos);
  if (i.value())
    return i.interval_end();
  return pos;
}

void MultiBuffer::NotifyAvailableRange(
    const Interval<MultiBufferBlockId>& observer_range,
    const Interval<MultiBufferBlockId>& new_range) {
  std::set<Reader*> tmp;
  for (auto i = readers_.lower_bound(observer_range.begin);
       i != readers_.end() && i->first < observer_range.end; ++i) {
    tmp.insert(i->second.begin(), i->second.end());
  }
  for (Reader* reader : tmp) {
    reader->NotifyAvailableRange(new_range);
  }
}

void MultiBuffer::ReleaseBlocks(const std::vector<MultiBufferBlockId>& blocks) {
  IntervalMap<BlockId, int32_t> freed;
  {
    base::AutoLock auto_lock(data_lock_);
    for (MultiBufferBlockId to_free : blocks) {
      DCHECK(data_[to_free]);
      DCHECK_EQ(pinned_[to_free], 0);
      DCHECK_EQ(present_[to_free], 1);
      data_.erase(to_free);
      freed.IncrementInterval(to_free, to_free + 1, 1);
      present_.IncrementInterval(to_free, to_free + 1, -1);
    }
    lru_->IncrementDataSize(-static_cast<int64_t>(blocks.size()));
  }

  for (auto freed_range : freed) {
    if (freed_range.second) {
      // Technically, there shouldn't be any observers in this range
      // as all observers really should be pinning the range where it's
      // actually observing.
      NotifyAvailableRange(
          freed_range.first,
          // Empty range.
          Interval<BlockId>(freed_range.first.begin, freed_range.first.begin));

      auto i = present_.find(freed_range.first.begin);
      DCHECK_EQ(i.value(), 0);
      DCHECK_LE(i.interval_begin(), freed_range.first.begin);
      DCHECK_LE(freed_range.first.end, i.interval_end());

      if (i.interval_begin() == freed_range.first.begin) {
        // Notify the previous range that it contains fewer blocks.
        auto j = i;
        --j;
        DCHECK_EQ(j.value(), 1);
        NotifyAvailableRange(j.interval(), j.interval());
      }
      if (i.interval_end() == freed_range.first.end) {
        // Notify the following range that it contains fewer blocks.
        auto j = i;
        ++j;
        DCHECK_EQ(j.value(), 1);
        NotifyAvailableRange(j.interval(), j.interval());
      }
    }
  }
  if (data_.empty())
    OnEmpty();
}

void MultiBuffer::OnEmpty() {}

void MultiBuffer::AddProvider(std::unique_ptr<DataProvider> provider) {
  // If there is already a provider in the same location, we delete it.
  DCHECK(!provider->Available());
  BlockId pos = provider->Tell();
  writer_index_[pos] = std::move(provider);
}

std::unique_ptr<MultiBuffer::DataProvider> MultiBuffer::RemoveProvider(
    DataProvider* provider) {
  BlockId pos = provider->Tell();
  auto iter = writer_index_.find(pos);
  CHECK(iter != writer_index_.end(), base::NotFatalUntil::M130);
  DCHECK_EQ(iter->second.get(), provider);
  std::unique_ptr<DataProvider> ret = std::move(iter->second);
  writer_index_.erase(iter);
  return ret;
}

MultiBuffer::ProviderState MultiBuffer::SuggestProviderState(
    const BlockId& pos) const {
  MultiBufferBlockId next_reader_pos = ClosestNextEntry(readers_, pos);
  if (next_reader_pos != std::numeric_limits<MultiBufferBlockId>::max() &&
      (next_reader_pos - pos <= kMaxWaitForWriterOffset || !RangeSupported())) {
    // Check if there is another writer between us and the next reader.
    MultiBufferBlockId next_writer_pos =
        ClosestNextEntry(writer_index_, pos + 1);
    if (next_writer_pos > next_reader_pos) {
      return ProviderStateLoad;
    }
  }

  MultiBufferBlockId previous_reader_pos =
      ClosestPreviousEntry(readers_, pos - 1);
  if (previous_reader_pos != std::numeric_limits<MultiBufferBlockId>::min() &&
      (pos - previous_reader_pos <= kMaxWaitForReaderOffset ||
       !RangeSupported())) {
    MultiBufferBlockId previous_writer_pos =
        ClosestPreviousEntry(writer_index_, pos - 1);
    if (previous_writer_pos < previous_reader_pos) {
      return ProviderStateDefer;
    }
  }

  return ProviderStateDead;
}

bool MultiBuffer::ProviderCollision(const BlockId& id) const {
  // If there is a writer at the same location, it is always a collision.
  if (base::Contains(writer_index_, id)) {
    return true;
  }

  // Data already exists at providers current position,
  // if the URL supports ranges, we can kill the data provider.
  if (RangeSupported() && Contains(id))
    return true;

  return false;
}

void MultiBuffer::Prune(size_t max_to_free) {
  lru_->Prune(max_to_free);
}

void MultiBuffer::OnDataProviderEvent(DataProvider* provider_tmp) {
  std::unique_ptr<DataProvider> provider(RemoveProvider(provider_tmp));
  BlockId start_pos = provider->Tell();
  BlockId pos = start_pos;
  bool eof = false;
  int64_t blocks_before = data_.size();

  {
    base::AutoLock auto_lock(data_lock_);
    while (!ProviderCollision(pos) && !eof) {
      if (!provider->Available()) {
        AddProvider(std::move(provider));
        break;
      }
      DCHECK_GE(pos, 0);
      scoped_refptr<media::DataBuffer> data = provider->Read();
      data_[pos] = data;
      eof = data->end_of_stream();
      if (!pinned_[pos])
        lru_->Use(this, pos);
      ++pos;
    }
  }
  int64_t blocks_after = data_.size();
  int64_t blocks_added = blocks_after - blocks_before;

  if (pos > start_pos) {
    present_.SetInterval(start_pos, pos, 1);
    Interval<BlockId> expanded_range = present_.find(start_pos).interval();
    NotifyAvailableRange(expanded_range, expanded_range);
    lru_->IncrementDataSize(blocks_added);
    Prune(static_cast<size_t>(blocks_added) * kMaxFreesPerAdd + 1);
  } else {
    // Make sure to give progress reports even when there
    // aren't any new blocks yet.
    NotifyAvailableRange(Interval<BlockId>(start_pos, start_pos + 1),
                         Interval<BlockId>(start_pos, start_pos));
  }

  // Check that it's still there before we try to delete it.
  // In case of EOF or a collision, we might not have called AddProvider above.
  // Even if we did call AddProvider, calling NotifyAvailableRange can cause
  // readers to seek or self-destruct and clean up any associated writers.
  auto i = writer_index_.find(pos);
  if (i != writer_index_.end() && i->second.get() == provider_tmp) {
    switch (SuggestProviderState(pos)) {
      case ProviderStateLoad:
        // Not sure we actually need to do this
        provider_tmp->SetDeferred(false);
        break;
      case ProviderStateDefer:
        provider_tmp->SetDeferred(true);
        break;
      case ProviderStateDead:
        RemoveProvider(provider_tmp);
        break;
    }
  }
}

void MultiBuffer::MergeFrom(MultiBuffer* other) {
  {
    base::AutoLock auto_lock(data_lock_);
    // Import data and update LRU.
    size_t data_size = data_.size();
    for (const auto& data : other->data_) {
      if (data_.insert(std::make_pair(data.first, data.second)).second) {
        if (!pinned_[data.first]) {
          lru_->Insert(this, data.first);
        }
      }
    }
    lru_->IncrementDataSize(static_cast<int64_t>(data_.size() - data_size));
  }
  // Update present_
  for (auto r : other->present_) {
    if (r.second) {
      present_.SetInterval(r.first.begin, r.first.end, 1);
    }
  }
  // Notify existing readers.
  auto last = present_.begin();
  for (auto r : other->present_) {
    if (r.second) {
      auto i = present_.find(r.first.begin);
      if (i != last) {
        NotifyAvailableRange(i.interval(), i.interval());
        last = i;
      }
    }
  }
}

void MultiBuffer::GetBlocksThreadsafe(
    const BlockId& from,
    const BlockId& to,
    std::vector<scoped_refptr<media::DataBuffer>>* output) {
  base::AutoLock auto_lock(data_lock_);
  auto i = data_.find(from);
  BlockId j = from;
  while (j <= to && i != data_.end() && i->first == j) {
    output->push_back(i->second);
    ++j;
    ++i;
  }
}

void MultiBuffer::PinRange(const BlockId& from,
                           const BlockId& to,
                           int32_t how_much) {
  DCHECK_NE(how_much, 0);
  DVLOG(3) << "PINRANGE [" << from << " - " << to << ") += " << how_much;
  pinned_.IncrementInterval(from, to, how_much);
  Interval<BlockId> modified_range(from, to);

  // Iterate over all the modified ranges and check if any of them have
  // transitioned in or out of the unlocked state. If so, we iterate over
  // all buffers in that range and add/remove them from the LRU as approperiate.
  // We iterate *backwards* through the ranges, with the idea that data in a
  // continous range should be freed from the end first.

  if (data_.empty())
    return;

  auto range = pinned_.find(to - 1);
  while (true) {
    DCHECK_GE(range.value(), 0);
    if (range.value() == 0 || range.value() == how_much) {
      bool pin = range.value() == how_much;
      Interval<BlockId> transition_range =
          modified_range.Intersect(range.interval());
      if (transition_range.Empty())
        break;

      // For each range that has transitioned to/from a pinned state,
      // we iterate over the corresponding ranges in |present_| to find
      // the blocks that are actually in the multibuffer.
      for (auto present_block_range = present_.find(transition_range.end - 1);
           present_block_range != present_.begin(); --present_block_range) {
        if (!present_block_range.value())
          continue;
        Interval<BlockId> present_transitioned_range =
            transition_range.Intersect(present_block_range.interval());
        if (present_transitioned_range.Empty())
          break;
        for (BlockId block = present_transitioned_range.end - 1;
             block >= present_transitioned_range.begin; --block) {
          DCHECK_GE(block, 0);
          DCHECK(base::Contains(data_, block));
          if (pin) {
            DCHECK(pinned_[block]);
            lru_->Remove(this, block);
          } else {
            DCHECK(!pinned_[block]);
            lru_->Insert(this, block);
          }
        }
      }
    }
    if (range == pinned_.begin())
      break;
    --range;
  }
}

void MultiBuffer::PinRanges(const IntervalMap<BlockId, int32_t>& ranges) {
  for (auto r : ranges) {
    if (r.second != 0) {
      PinRange(r.first.begin, r.first.end, r.second);
    }
  }
}

void MultiBuffer::IncrementMaxSize(int64_t size) {
  max_size_ += size;
  lru_->IncrementMaxSize(size);
  DCHECK_GE(max_size_, 0);
  // Pruning only happens when blocks are added.
}

int64_t MultiBuffer::UncommittedBytesAt(const MultiBuffer::BlockId& block) {
  auto i = writer_index_.find(block);
  if (writer_index_.end() == i)
    return 0;
  return i->second->AvailableBytes();
}

}  // namespace blink
