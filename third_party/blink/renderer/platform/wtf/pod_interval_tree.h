/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_POD_INTERVAL_TREE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_POD_INTERVAL_TREE_H_

#include "third_party/blink/renderer/platform/wtf/assertions.h"
#include "third_party/blink/renderer/platform/wtf/noncopyable.h"
#include "third_party/blink/renderer/platform/wtf/pod_arena.h"
#include "third_party/blink/renderer/platform/wtf/pod_interval.h"
#include "third_party/blink/renderer/platform/wtf/pod_red_black_tree.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace WTF {

#ifndef NDEBUG
template <class T>
struct ValueToString;
#endif

template <class T, class UserData = void*>
class PODIntervalSearchAdapter {
  DISALLOW_NEW();

 public:
  typedef PODInterval<T, UserData> IntervalType;

  PODIntervalSearchAdapter(Vector<IntervalType>& result,
                           const T& low_value,
                           const T& high_value)
      : result_(result), low_value_(low_value), high_value_(high_value) {}

  const T& LowValue() const { return low_value_; }
  const T& HighValue() const { return high_value_; }
  void CollectIfNeeded(const IntervalType& data) const {
    if (data.Overlaps(low_value_, high_value_))
      result_.push_back(data);
  }

 private:
  Vector<IntervalType>& result_;
  T low_value_;
  T high_value_;
};

// An interval tree, which is a form of augmented red-black tree. It
// supports efficient (O(lg n)) insertion, removal and querying of
// intervals in the tree.
template <class T, class UserData = void*>
class PODIntervalTree final : public PODRedBlackTree<PODInterval<T, UserData>> {
  WTF_MAKE_NONCOPYABLE(PODIntervalTree);

 public:
  // Typedef to reduce typing when declaring intervals to be stored in
  // this tree.
  typedef PODInterval<T, UserData> IntervalType;
  typedef PODIntervalSearchAdapter<T, UserData> IntervalSearchAdapterType;

  PODIntervalTree(UninitializedTreeEnum unitialized_tree)
      : PODRedBlackTree<IntervalType>(unitialized_tree) {
    Init();
  }

  PODIntervalTree() : PODRedBlackTree<IntervalType>() { Init(); }

  explicit PODIntervalTree(scoped_refptr<PODArena> arena)
      : PODRedBlackTree<IntervalType>(arena) {
    Init();
  }

  // Returns all intervals in the tree which overlap the given query
  // interval. The returned intervals are sorted by increasing low
  // endpoint.
  Vector<IntervalType> AllOverlaps(const IntervalType& interval) const {
    Vector<IntervalType> result;
    AllOverlaps(interval, result);
    return result;
  }

  // Returns all intervals in the tree which overlap the given query
  // interval. The returned intervals are sorted by increasing low
  // endpoint.
  void AllOverlaps(const IntervalType& interval,
                   Vector<IntervalType>& result) const {
    // Explicit dereference of "this" required because of
    // inheritance rules in template classes.
    IntervalSearchAdapterType adapter(result, interval.Low(), interval.High());
    SearchForOverlapsFrom<IntervalSearchAdapterType>(this->Root(), adapter);
  }

  template <class AdapterType>
  void AllOverlapsWithAdapter(AdapterType& adapter) const {
    // Explicit dereference of "this" required because of
    // inheritance rules in template classes.
    SearchForOverlapsFrom<AdapterType>(this->Root(), adapter);
  }

  // Helper to create interval objects.
  static IntervalType CreateInterval(const T& low,
                                     const T& high,
                                     const UserData data = nullptr) {
    return IntervalType(low, high, data);
  }

  bool CheckInvariants() const override {
    if (!PODRedBlackTree<IntervalType>::CheckInvariants())
      return false;
    if (!this->Root())
      return true;
    return CheckInvariantsFromNode(this->Root(), nullptr);
  }

 private:
  typedef typename PODRedBlackTree<IntervalType>::Node IntervalNode;

  // Initializes the tree.
  void Init() {
    // Explicit dereference of "this" required because of
    // inheritance rules in template classes.
    this->SetNeedsFullOrderingComparisons(true);
  }

  // Starting from the given node, adds all overlaps with the given
  // interval to the result vector. The intervals are sorted by
  // increasing low endpoint.
  template <class AdapterType>
  DISABLE_CFI_PERF void SearchForOverlapsFrom(IntervalNode* node,
                                              AdapterType& adapter) const {
    if (!node)
      return;

    // Because the intervals are sorted by left endpoint, inorder
    // traversal produces results sorted as desired.

    // See whether we need to traverse the left subtree.
    IntervalNode* left = node->Left();
    if (left
        // This is phrased this way to avoid the need for operator
        // <= on type T.
        && !(left->Data().MaxHigh() < adapter.LowValue()))
      SearchForOverlapsFrom<AdapterType>(left, adapter);

    // Check for overlap with current node.
    adapter.CollectIfNeeded(node->Data());

    // See whether we need to traverse the right subtree.
    // This is phrased this way to avoid the need for operator <=
    // on type T.
    if (!(adapter.HighValue() < node->Data().Low()))
      SearchForOverlapsFrom<AdapterType>(node->Right(), adapter);
  }

  bool UpdateNode(IntervalNode* node) override {
    // Would use const T&, but need to reassign this reference in this
    // function.
    const T* cur_max = &node->Data().High();
    IntervalNode* left = node->Left();
    if (left) {
      if (*cur_max < left->Data().MaxHigh())
        cur_max = &left->Data().MaxHigh();
    }
    IntervalNode* right = node->Right();
    if (right) {
      if (*cur_max < right->Data().MaxHigh())
        cur_max = &right->Data().MaxHigh();
    }
    // This is phrased like this to avoid needing operator!= on type T.
    if (!(*cur_max == node->Data().MaxHigh())) {
      node->Data().SetMaxHigh(*cur_max);
      return true;
    }
    return false;
  }

  bool CheckInvariantsFromNode(IntervalNode* node, T* current_max_value) const {
    // These assignments are only done in order to avoid requiring
    // a default constructor on type T.
    T left_max_value(node->Data().MaxHigh());
    T right_max_value(node->Data().MaxHigh());
    IntervalNode* left = node->Left();
    IntervalNode* right = node->Right();
    if (left) {
      if (!CheckInvariantsFromNode(left, &left_max_value))
        return false;
    }
    if (right) {
      if (!CheckInvariantsFromNode(right, &right_max_value))
        return false;
    }
    if (!left && !right) {
      // Base case.
      if (current_max_value)
        *current_max_value = node->Data().High();
      return (node->Data().High() == node->Data().MaxHigh());
    }
    T local_max_value(node->Data().MaxHigh());
    if (!left || !right) {
      if (left)
        local_max_value = left_max_value;
      else
        local_max_value = right_max_value;
    } else {
      local_max_value =
          (left_max_value < right_max_value) ? right_max_value : left_max_value;
    }
    if (local_max_value < node->Data().High())
      local_max_value = node->Data().High();
    if (!(local_max_value == node->Data().MaxHigh())) {
#ifndef NDEBUG
      String local_max_value_string =
          ValueToString<T>::ToString(local_max_value);
      DLOG(ERROR) << "PODIntervalTree verification failed at node " << node
                  << ": localMaxValue=" << local_max_value_string
                  << " and data=" << node->Data().ToString();
#endif
      return false;
    }
    if (current_max_value)
      *current_max_value = local_max_value;
    return true;
  }
};

#ifndef NDEBUG
// Support for printing PODIntervals at the PODRedBlackTree level.
template <class T, class UserData>
struct ValueToString<PODInterval<T, UserData>> {
  static String ToString(const PODInterval<T, UserData>& interval) {
    return interval.ToString();
  }
};
#endif

}  // namespace WTF

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_POD_INTERVAL_TREE_H_
