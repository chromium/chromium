// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Copied and adopted from V8.
//
// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_IMPL_WORKLIST_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_IMPL_WORKLIST_H_

#include <atomic>
#include <cstddef>
#include <utility>

#include "base/atomicops.h"
#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/synchronization/lock.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

// A concurrent worklist based on segments. Each tasks gets private
// push and pop segments. Empty pop segments are swapped with their
// corresponding push segments. Full push segments are published to a global
// pool of segments and replaced with empty segments.
//
// Work stealing is best effort, i.e., there is no way to inform other tasks
// of the need of items.
template <typename _EntryType, int segment_size, int num_tasks = 4>
class Worklist {
  USING_FAST_MALLOC(Worklist);
  using WorklistType = Worklist<_EntryType, segment_size, num_tasks>;

 public:
  using EntryType = _EntryType;

  static constexpr int kNumTasks = num_tasks;

  class View {
    DISALLOW_NEW();

   public:
    View(WorklistType* worklist, int task_id)
        : worklist_(worklist), task_id_(task_id) {}

    // Pushes an entry onto the worklist.
    bool Push(EntryType entry) { return worklist_->Push(task_id_, entry); }

    // Pops an entry from the worklist.
    bool Pop(EntryType* entry) { return worklist_->Pop(task_id_, entry); }

    // Returns true if the local portion of the worklist is empty.
    bool IsLocalEmpty() const { return worklist_->IsLocalEmpty(task_id_); }

    // Returns true if the worklist is empty. Can only be used from the main
    // thread without concurrent access.
    bool IsGlobalEmpty() const { return worklist_->IsGlobalEmpty(); }

    bool IsGlobalPoolEmpty() const { return worklist_->IsGlobalPoolEmpty(); }

    // Returns true if the local portion and the global pool are empty (i.e.
    // whether the current view cannot pop anymore).
    bool IsLocalViewEmpty() const {
      return worklist_->IsLocalViewEmpty(task_id_);
    }

    void FlushToGlobal() { worklist_->FlushToGlobal(task_id_); }

    size_t LocalPushSegmentSize() const {
      return worklist_->LocalPushSegmentSize(task_id_);
    }

   private:
    WorklistType* const worklist_;
    const int task_id_;
  };

  static constexpr size_t kSegmentCapacity = segment_size;

  Worklist() {
    for (int i = 0; i < kNumTasks; i++) {
      private_push_segment(i) = NewSegment();
      private_pop_segment(i) = NewSegment();
    }
  }

  ~Worklist() {
    CHECK(IsGlobalEmpty());
    for (int i = 0; i < kNumTasks; i++) {
      DCHECK(private_push_segment(i));
      DCHECK(private_pop_segment(i));
      delete private_push_segment(i);
      delete private_pop_segment(i);
    }
  }

  bool Push(int task_id, EntryType entry) {
    DCHECK_LT(task_id, kNumTasks);
    DCHECK(private_push_segment(task_id));
    if (!private_push_segment(task_id)->Push(entry)) {
      PublishPushSegmentToGlobal(task_id);
      bool success = private_push_segment(task_id)->Push(entry);
      ANALYZER_ALLOW_UNUSED(success);
      DCHECK(success);
    }
    return true;
  }

  bool Pop(int task_id, EntryType* entry) {
    DCHECK_LT(task_id, kNumTasks);
    DCHECK(private_pop_segment(task_id));
    if (!private_pop_segment(task_id)->Pop(entry)) {
      if (!private_push_segment(task_id)->IsEmpty()) {
        Segment* tmp = private_pop_segment(task_id);
        private_pop_segment(task_id) = private_push_segment(task_id);
        private_push_segment(task_id) = tmp;
      } else if (!StealPopSegmentFromGlobal(task_id)) {
        return false;
      }
      bool success = private_pop_segment(task_id)->Pop(entry);
      ANALYZER_ALLOW_UNUSED(success);
      DCHECK(success);
    }
    return true;
  }

  bool IsLocalEmpty(int task_id) const {
    return private_pop_segment(task_id)->IsEmpty() &&
           private_push_segment(task_id)->IsEmpty();
  }

  bool IsGlobalPoolEmpty() const { return global_pool_.IsEmpty(); }

  bool IsGlobalEmpty() const {
    for (int i = 0; i < kNumTasks; i++) {
      if (!IsLocalEmpty(i))
        return false;
    }
    return global_pool_.IsEmpty();
  }

  bool IsLocalViewEmpty(int task_id) const {
    return IsLocalEmpty(task_id) && IsGlobalPoolEmpty();
  }

  size_t LocalSize(int task_id) const {
    return private_pop_segment(task_id)->Size() +
           private_push_segment(task_id)->Size();
  }

  // Thread-safe but may return an outdated result.
  size_t GlobalPoolSize() const { return global_pool_.Size(); }

  size_t LocalPushSegmentSize(int task_id) const {
    return private_push_segment(task_id)->Size();
  }

  // Clears all segments. Frees the global segment pool.
  //
  // Assumes that no other tasks are running.
  void Clear() {
    for (int i = 0; i < kNumTasks; i++) {
      private_pop_segment(i)->Clear();
      private_push_segment(i)->Clear();
    }
    global_pool_.Clear();
  }

  // Calls the specified callback on each element of the deques and replaces
  // the element with the result of the callback.
  // The signature of the callback is
  //   bool Callback(EntryType old, EntryType* new).
  // If the callback returns |false| then the element is removed from the
  // worklist. Otherwise the |new| entry is updated.
  //
  // Assumes that no other tasks are running.
  template <typename Callback>
  void Update(Callback callback) {
    for (int i = 0; i < kNumTasks; i++) {
      private_pop_segment(i)->Update(callback);
      private_push_segment(i)->Update(callback);
    }
    global_pool_.Update(callback);
  }

  template <typename Callback>
  void IterateGlobalPool(Callback callback) {
    global_pool_.Iterate(callback);
  }

  void FlushToGlobal(int task_id) {
    PublishPushSegmentToGlobal(task_id);
    PublishPopSegmentToGlobal(task_id);
  }

  void MergeGlobalPool(Worklist* other) {
    global_pool_.Merge(&other->global_pool_);
  }

  size_t SizeForTesting() {
    size_t size = global_pool_.SizeForTesting();
    for (int i = 0; i < kNumTasks; i++) {
      size += private_pop_segment(i)->Size() + private_push_segment(i)->Size();
    }
    return size;
  }

 private:
  FRIEND_TEST_ALL_PREFIXES(WorklistTest, SegmentCreate);
  FRIEND_TEST_ALL_PREFIXES(WorklistTest, SegmentPush);
  FRIEND_TEST_ALL_PREFIXES(WorklistTest, SegmentPushPop);
  FRIEND_TEST_ALL_PREFIXES(WorklistTest, SegmentIsEmpty);
  FRIEND_TEST_ALL_PREFIXES(WorklistTest, SegmentIsFull);
  FRIEND_TEST_ALL_PREFIXES(WorklistTest, SegmentClear);
  FRIEND_TEST_ALL_PREFIXES(WorklistTest, SegmentFullPushFails);
  FRIEND_TEST_ALL_PREFIXES(WorklistTest, SegmentEmptyPopFails);
  FRIEND_TEST_ALL_PREFIXES(WorklistTest, SegmentUpdateFalse);
  FRIEND_TEST_ALL_PREFIXES(WorklistTest, SegmentUpdate);

  class Segment {
    USING_FAST_MALLOC(Segment);

   public:
    static const size_t kCapacity = kSegmentCapacity;

    Segment() : index_(0) {}

    bool Push(EntryType entry) {
      if (IsFull())
        return false;
      entries_[index_++] = entry;
      return true;
    }

    bool Pop(EntryType* entry) {
      if (IsEmpty())
        return false;
      *entry = entries_[--index_];
      return true;
    }

    size_t Size() const { return index_; }
    bool IsEmpty() const { return index_ == 0; }
    bool IsFull() const { return index_ == kCapacity; }
    void Clear() { index_ = 0; }

    template <typename Callback>
    void Update(Callback callback) {
      size_t new_index = 0;
      for (size_t i = 0; i < index_; i++) {
        if (callback(entries_[i], &entries_[new_index])) {
          new_index++;
        }
      }
      index_ = new_index;
    }

    template <typename Callback>
    void Iterate(Callback callback) const {
      for (size_t i = 0; i < index_; i++) {
        callback(entries_[i]);
      }
    }

    Segment* next() const { return next_; }
    void set_next(Segment* segment) { next_ = segment; }

   private:
    Segment* next_;
    size_t index_;
    EntryType entries_[kCapacity];
  };

  struct PrivateSegmentHolder {
    Segment* private_push_segment;
    Segment* private_pop_segment;
    char cache_line_padding[64];
  };

  class GlobalPool {
    DISALLOW_NEW();

   public:
    GlobalPool() : top_(nullptr) {}

    inline void Push(Segment* segment) {
      base::AutoLock guard(lock_);
      segment->set_next(top_);
      set_top(segment);
      size_.fetch_add(1, std::memory_order_relaxed);
    }

    inline bool Pop(Segment** segment) {
      base::AutoLock guard(lock_);
      if (top_) {
        DCHECK_LT(0U, size_);
        size_.fetch_sub(1, std::memory_order_relaxed);
        *segment = top_;
        set_top(top_->next());
        return true;
      }
      return false;
    }

    inline bool IsEmpty() const {
      return base::subtle::NoBarrier_Load(
                 reinterpret_cast<const base::subtle::AtomicWord*>(&top_)) == 0;
    }

    inline size_t Size() const {
      // It is safe to read |size_| without a lock since this variable is
      // atomic, keeping in mind that threads may not immediately see the new
      // value when it is updated.
      return TS_UNCHECKED_READ(size_).load(std::memory_order_relaxed);
    }

    void Clear() {
      base::AutoLock guard(lock_);
      size_.store(0, std::memory_order_relaxed);
      Segment* current = top_;
      while (current) {
        Segment* tmp = current;
        current = current->next();
        delete tmp;
      }
      set_top(nullptr);
    }

    // See Worklist::Update.
    template <typename Callback>
    void Update(Callback callback) {
      base::AutoLock guard(lock_);
      Segment* prev = nullptr;
      Segment* current = top_;
      while (current) {
        current->Update(callback);
        if (current->IsEmpty()) {
          DCHECK_LT(0U, size_);
          size_.fetch_sub(1, std::memory_order_relaxed);
          if (!prev) {
            top_ = current->next();
          } else {
            prev->set_next(current->next());
          }
          Segment* tmp = current;
          current = current->next();
          delete tmp;
        } else {
          prev = current;
          current = current->next();
        }
      }
    }

    // See Worklist::Iterate.
    template <typename Callback>
    void Iterate(Callback callback) {
      base::AutoLock guard(lock_);
      for (Segment* current = top_; current; current = current->next()) {
        current->Iterate(callback);
      }
    }

    void Merge(GlobalPool* other) {
      Segment* top = nullptr;
      size_t other_size = 0;
      {
        base::AutoLock guard(other->lock_);
        if (!other->top_)
          return;
        top = other->top_;
        other_size = other->size_.load(std::memory_order_relaxed);
        other->size_.store(0, std::memory_order_relaxed);
        other->set_top(nullptr);
      }

      Segment* end = top;
      while (end->next())
        end = end->next();

      {
        base::AutoLock guard(lock_);
        size_.fetch_add(other_size, std::memory_order_relaxed);
        end->set_next(top_);
        set_top(top);
      }
    }

    size_t SizeForTesting() {
      size_t size = 0;
      base::AutoLock guard(lock_);
      for (Segment* current = top_; current; current = current->next())
        size += current->Size();
      return size;
    }

   private:
    void set_top(Segment* segment) {
      return base::subtle::NoBarrier_Store(
          reinterpret_cast<base::subtle::AtomicWord*>(&top_),
          reinterpret_cast<base::subtle::AtomicWord>(segment));
    }

    mutable base::Lock lock_;
    Segment* top_ GUARDED_BY(lock_);
    std::atomic<size_t> size_ GUARDED_BY(lock_){0};
  };

  ALWAYS_INLINE Segment*& private_push_segment(int task_id) {
    return private_segments_[task_id].private_push_segment;
  }

  ALWAYS_INLINE Segment* const& private_push_segment(int task_id) const {
    return const_cast<const PrivateSegmentHolder*>(private_segments_)[task_id]
        .private_push_segment;
  }

  ALWAYS_INLINE Segment*& private_pop_segment(int task_id) {
    return private_segments_[task_id].private_pop_segment;
  }

  ALWAYS_INLINE Segment* const& private_pop_segment(int task_id) const {
    return const_cast<const PrivateSegmentHolder*>(private_segments_)[task_id]
        .private_pop_segment;
  }

  ALWAYS_INLINE void PublishPushSegmentToGlobal(int task_id) {
    if (!private_push_segment(task_id)->IsEmpty()) {
      global_pool_.Push(private_push_segment(task_id));
      private_push_segment(task_id) = NewSegment();
    }
  }

  ALWAYS_INLINE void PublishPopSegmentToGlobal(int task_id) {
    if (!private_pop_segment(task_id)->IsEmpty()) {
      global_pool_.Push(private_pop_segment(task_id));
      private_pop_segment(task_id) = NewSegment();
    }
  }

  ALWAYS_INLINE bool StealPopSegmentFromGlobal(int task_id) {
    if (global_pool_.IsEmpty())
      return false;
    Segment* new_segment = nullptr;
    if (global_pool_.Pop(&new_segment)) {
      delete private_pop_segment(task_id);
      private_pop_segment(task_id) = new_segment;
      return true;
    }
    return false;
  }

  ALWAYS_INLINE Segment* NewSegment() {
    // Bottleneck for filtering in crash dumps.
    return new Segment();
  }

  PrivateSegmentHolder private_segments_[kNumTasks];
  GlobalPool global_pool_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_IMPL_WORKLIST_H_
