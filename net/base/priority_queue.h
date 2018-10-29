// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_PRIORITY_QUEUE_H_
#define NET_BASE_PRIORITY_QUEUE_H_

#include <stddef.h>
#include <stdint.h>

#include <list>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/threading/thread_checker.h"

#if !defined(NDEBUG)
#include <unordered_set>
#endif

namespace net {

// A simple priority queue. The order of values is by priority and then FIFO.
// Unlike the std::priority_queue, this implementation allows erasing elements
// from the queue, and all operations are O(p) time for p priority levels.
// The queue is agnostic to priority ordering (whether 0 precedes 1).
// If the highest priority is 0, FirstMin() returns the first in order.
//
// In debug-mode, the internal queues store (id, value) pairs where id is used
// to validate Pointers.
//
template <typename T>
class PriorityQueue {
 private:
  // This section is up-front for Pointer only.
#if !defined(NDEBUG)
  typedef std::list<std::pair<unsigned, T> > List;
#else
  typedef std::list<T> List;
#endif

 public:
  typedef uint32_t Priority;

  // A pointer to a value stored in the queue. The pointer becomes invalid
  // when the queue is destroyed or cleared, or the value is erased.
  class Pointer {
   public:
    // Constructs a null pointer.
    Pointer() : priority_(kNullPriority) {
#if !defined(NDEBUG)
      id_ = static_cast<unsigned>(-1);
#endif
      // TODO(syzm)
      // An uninitialized iterator behaves like an uninitialized pointer as per
      // the STL docs. The fix below is ugly and should possibly be replaced
      // with a better approach.
      iterator_ = dummy_empty_list_.end();
    }

    Pointer(const Pointer& p)
        : priority_(p.priority_),
          iterator_(p.iterator_) {
#if !defined(NDEBUG)
      id_ = p.id_;
#endif
    }

    Pointer& operator=(const Pointer& p) {
      // Self-assignment is benign.
      priority_ = p.priority_;
      iterator_ = p.iterator_;
#if !defined(NDEBUG)
      id_ = p.id_;
#endif
      return *this;
    }

    bool is_null() const { return priority_ == kNullPriority; }

    Priority priority() const { return priority_; }

#if !defined(NDEBUG)
    const T& value() const { return iterator_->second; }
#else
    const T& value() const { return *iterator_; }
#endif

    // Comparing to Pointer from a different PriorityQueue is undefined.
    bool Equals(const Pointer& other) const {
      return (priority_ == other.priority_) && (iterator_ == other.iterator_);
    }

    void Reset() {
      *this = Pointer();
    }

   private:
    friend class PriorityQueue;

    // Note that we need iterator and not const_iterator to pass to
    // List::erase.  When C++11 is turned on for Chromium, this could
    // be changed to const_iterator and the const_casts in the rest of
    // the file can be removed.
    typedef typename PriorityQueue::List::iterator ListIterator;

    static const Priority kNullPriority = static_cast<Priority>(-1);

    // It is guaranteed that Pointer will treat |iterator| as a
    // const_iterator.
    Pointer(Priority priority, const ListIterator& iterator)
        : priority_(priority), iterator_(iterator) {
#if !defined(NDEBUG)
      id_ = iterator_->first;
#endif
    }

    Priority priority_;
    ListIterator iterator_;
    // The STL iterators when uninitialized are like uninitialized pointers
    // which cause crashes when assigned to other iterators. We need to
    // initialize a NULL iterator to the end of a valid list.
    List dummy_empty_list_;

#if !defined(NDEBUG)
    // Used by the queue to check if a Pointer is valid.
    unsigned id_;
#endif
  };

  // Creates a new queue for |num_priorities|.
  explicit PriorityQueue(Priority num_priorities)
      : lists_(num_priorities), size_(0) {
#if !defined(NDEBUG)
    next_id_ = 0;
#endif
  }

  ~PriorityQueue() { DCHECK_CALLED_ON_VALID_THREAD(thread_checker_); }

  // Adds |value| with |priority| to the queue. Returns a pointer to the
  // created element.
  Pointer Insert(T value, Priority priority) {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    DCHECK_LT(priority, lists_.size());
    ++size_;
    List& list = lists_[priority];
#if !defined(NDEBUG)
    unsigned id = next_id_;
    valid_ids_.insert(id);
    ++next_id_;
    list.emplace_back(std::make_pair(id, std::move(value)));
#else
    list.emplace_back(std::move(value));
#endif
    return Pointer(priority, std::prev(list.end()));
  }

  // Adds |value| with |priority| to the queue. Returns a pointer to the
  // created element.
  Pointer InsertAtFront(T value, Priority priority) {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    DCHECK_LT(priority, lists_.size());
    ++size_;
    List& list = lists_[priority];
#if !defined(NDEBUG)
    unsigned id = next_id_;
    valid_ids_.insert(id);
    ++next_id_;
    list.emplace_front(std::make_pair(id, std::move(value)));
#else
    list.emplace_front(std::move(value));
#endif
    return Pointer(priority, list.begin());
  }

  // Removes the value pointed by |pointer| from the queue. All pointers to this
  // value including |pointer| become invalid. Returns the erased value.
  T Erase(const Pointer& pointer) {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    DCHECK_LT(pointer.priority_, lists_.size());
    DCHECK_GT(size_, 0u);

#if !defined(NDEBUG)
    DCHECK_EQ(1u, valid_ids_.erase(pointer.id_));
    DCHECK_EQ(pointer.iterator_->first, pointer.id_);
    T erased = std::move(pointer.iterator_->second);
#else
    T erased = std::move(*pointer.iterator_);
#endif

    --size_;
    lists_[pointer.priority_].erase(pointer.iterator_);
    return erased;
  }

  // Returns a pointer to the first value of minimum priority or a null-pointer
  // if empty.
  Pointer FirstMin() const {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    for (size_t i = 0; i < lists_.size(); ++i) {
      List* list = const_cast<List*>(&lists_[i]);
      if (!list->empty())
        return Pointer(i, list->begin());
    }
    return Pointer();
  }

  // Returns a pointer to the last value of minimum priority or a null-pointer
  // if empty.
  Pointer LastMin() const {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    for (size_t i = 0; i < lists_.size(); ++i) {
      List* list = const_cast<List*>(&lists_[i]);
      if (!list->empty())
        return Pointer(i, --list->end());
    }
    return Pointer();
  }

  // Returns a pointer to the first value of maximum priority or a null-pointer
  // if empty.
  Pointer FirstMax() const {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    for (size_t i = lists_.size(); i > 0; --i) {
      size_t index = i - 1;
      List* list = const_cast<List*>(&lists_[index]);
      if (!list->empty())
        return Pointer(index, list->begin());
    }
    return Pointer();
  }

  // Returns a pointer to the last value of maximum priority or a null-pointer
  // if empty.
  Pointer LastMax() const {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    for (size_t i = lists_.size(); i > 0; --i) {
      size_t index = i - 1;
      List* list = const_cast<List*>(&lists_[index]);
      if (!list->empty())
        return Pointer(index, --list->end());
    }
    return Pointer();
  }

  // Given an ordering of the values in this queue by decreasing priority and
  // then FIFO, returns a pointer to the value following the value of the given
  // pointer (which must be non-NULL). I.e., gets the next element in decreasing
  // priority, then FIFO order. If the given pointer is already pointing at the
  // last value, returns a null Pointer.
  //
  // (One could also implement GetNextTowardsFirstMin() [decreasing priority,
  // then reverse FIFO] as well as GetNextTowards{First,Last}Max() [increasing
  // priority, then {,reverse} FIFO].)
  Pointer GetNextTowardsLastMin(const Pointer& pointer) const {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    DCHECK(!pointer.is_null());
    DCHECK_LT(pointer.priority_, lists_.size());

    typename Pointer::ListIterator it = pointer.iterator_;
    Priority priority = pointer.priority_;
    DCHECK(it != lists_[priority].end());
    ++it;
    while (it == lists_[priority].end()) {
      if (priority == 0u) {
        DCHECK(pointer.Equals(LastMin()));
        return Pointer();
      }
      --priority;
      it = const_cast<List*>(&lists_[priority])->begin();
    }
    return Pointer(priority, it);
  }

  // Given an ordering of the values in this queue by decreasing priority and
  // then FIFO, returns a pointer to the value preceding the value of the given
  // pointer (which must be non-NULL). I.e., gets the next element in increasing
  // priority, then reverse FIFO order. If the given pointer is already pointing
  // at the first value, returns a null Pointer.
  Pointer GetPreviousTowardsFirstMax(const Pointer& pointer) const {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    DCHECK(!pointer.is_null());
    DCHECK_LT(pointer.priority_, lists_.size());

    typename Pointer::ListIterator it = pointer.iterator_;
    Priority priority = pointer.priority_;
    DCHECK(it != lists_[priority].end());
    while (it == lists_[priority].begin()) {
      if (priority == num_priorities() - 1) {
        DCHECK(pointer.Equals(FirstMax()));
        return Pointer();
      }
      ++priority;
      it = const_cast<List*>(&lists_[priority])->end();
    }
    return Pointer(priority, std::prev(it));
  }

  // Checks whether |lhs| is closer in the queue to the first max element than
  // |rhs|. Assumes that both Pointers refer to elements in this PriorityQueue.
  bool IsCloserToFirstMaxThan(const Pointer& lhs, const Pointer& rhs) {
    if (lhs.Equals(rhs))
      return false;
    if (lhs.priority_ == rhs.priority_) {
      // Traverse list starting from lhs and see if we find rhs.
      for (auto it = lhs.iterator_; it != lists_[lhs.priority_].end(); ++it) {
        if (it == rhs.iterator_)
          return true;
      }
      return false;
    }
    return lhs.priority_ > rhs.priority_;
  }

  // Checks whether |lhs| is closer in the queue to the last min element than
  // |rhs|. Assumes that both Pointers refer to elements in this PriorityQueue.
  bool IsCloserToLastMinThan(const Pointer& lhs, const Pointer& rhs) {
    return !lhs.Equals(rhs) && !IsCloserToFirstMaxThan(lhs, rhs);
  }

  // Finds the first element (with respect to decreasing priority, then FIFO
  // order) which matches the given predicate.
  Pointer FindIf(const base::RepeatingCallback<bool(T)>& pred) {
    for (auto pointer = FirstMax(); !pointer.is_null();
         pointer = GetNextTowardsLastMin(pointer)) {
      if (pred.Run(pointer.value()))
        return pointer;
    }
    return Pointer();
  }

  // Empties the queue. All pointers become invalid.
  void Clear() {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    for (size_t i = 0; i < lists_.size(); ++i) {
      lists_[i].clear();
    }
#if !defined(NDEBUG)
    valid_ids_.clear();
#endif
    size_ = 0u;
  }

  // Returns the number of priorities the queue supports.
  size_t num_priorities() const {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    return lists_.size();
  }

  bool empty() const {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    return size_ == 0;
  }

  // Returns number of queued values.
  size_t size() const {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    return size_;
  }

 private:
  typedef std::vector<List> ListVector;

#if !defined(NDEBUG)
  unsigned next_id_;
  std::unordered_set<unsigned> valid_ids_;
#endif

  ListVector lists_;
  size_t size_;

  THREAD_CHECKER(thread_checker_);

  DISALLOW_COPY_AND_ASSIGN(PriorityQueue);
};

}  // namespace net

#endif  // NET_BASE_PRIORITY_QUEUE_H_
