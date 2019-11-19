// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/layout_shift_region.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"

namespace blink {

namespace {

// A segment is a contiguous range of one or more basic intervals.
struct Segment {
  // These are the 0-based indexes into the basic intervals, of the first and
  // last basic interval in the segment.
  unsigned first_interval;
  unsigned last_interval;
};

// An "event" occurs when a rectangle starts intersecting the sweep line
// (START), or when it ceases to intersect the sweep line (END).
enum class EventType { START, END };
struct SweepEvent {
  // X-coordinate at which the event occurs.
  int x;
  // Whether the sweep line is entering or exiting the generating rect.
  EventType type;
  // The generating rect's intersection with the sweep line.
  Segment y_segment;
};

// The sequence of adjacent intervals on the y-axis whose endpoints are the
// extents (IntRect::Y and IntRect::MaxY) of all the rectangles in the input.
class BasicIntervals {
 public:
  // Add all the endpoints before creating the index.
  void AddEndpoint(int endpoint);
  void CreateIndex();

  // Create the index before querying these.
  unsigned NumIntervals() const;
  Segment SegmentFromEndpoints(int start, int end) const;
  unsigned SegmentLength(Segment) const;

 private:
  Vector<int> endpoints_;
  // Use int64_t which is larger than real |int| since the empty value of the
  // key is max and deleted value of the key is max - 1 in HashMap.
  HashMap<int64_t,
          unsigned,
          WTF::IntHash<int64_t>,
          WTF::UnsignedWithZeroKeyHashTraits<int64_t>>
      endpoint_to_index_;

#if DCHECK_IS_ON()
  bool has_index_ = false;
#endif
};

#if DCHECK_IS_ON()
#define DCHECK_HAS_INDEX(expected) DCHECK(has_index_ == expected)
#else
#define DCHECK_HAS_INDEX(expected)
#endif

inline void BasicIntervals::AddEndpoint(int endpoint) {
  DCHECK_HAS_INDEX(false);

  // We can't index yet, but use the map to de-dupe.
  auto ret = endpoint_to_index_.insert(endpoint, 0u);
  if (ret.is_new_entry)
    endpoints_.push_back(endpoint);
}

void BasicIntervals::CreateIndex() {
  DCHECK_HAS_INDEX(false);
  std::sort(endpoints_.begin(), endpoints_.end());
  unsigned i = 0;
  for (const int& e : endpoints_)
    endpoint_to_index_.Set(e, i++);

#if DCHECK_IS_ON()
  has_index_ = true;
#endif
}

inline unsigned BasicIntervals::NumIntervals() const {
  DCHECK_HAS_INDEX(true);
  return endpoints_.size() - 1;
}

inline Segment BasicIntervals::SegmentFromEndpoints(int start, int end) const {
  DCHECK_HAS_INDEX(true);
  return Segment{endpoint_to_index_.at(start), endpoint_to_index_.at(end) - 1};
}

inline unsigned BasicIntervals::SegmentLength(Segment segment) const {
  DCHECK_HAS_INDEX(true);
  return endpoints_[segment.last_interval + 1] -
         endpoints_[segment.first_interval];
}

#undef DCHECK_HAS_INDEX

// An array-backed, weight-balanced binary tree whose leaves represent the basic
// intervals.  Non-leaf nodes represent the union of their children's intervals.
class SegmentTree {
 public:
  SegmentTree(const BasicIntervals&);

  // The RefSegment and DerefSegment methods mark nodes corresponding to a
  // segment by touching the minimal set of nodes that comprise the segment,
  // i.e. every node that is fully within the segment, but whose parent isn't.
  // There are only O(log N) nodes in this set.
  void RefSegment(Segment);
  void DerefSegment(Segment);

  // Combined length of all active segments.
  unsigned ActiveLength() const;

 private:
  static unsigned ComputeCapacity(unsigned leaf_count);

  static unsigned LeftChild(unsigned node_index);
  static unsigned RightChild(unsigned node_index);

  Segment RootSegment() const;
  unsigned ComputeActiveLength(unsigned node_index, Segment node_segment) const;

  // Visit implements the recursive descent through the tree to update nodes for
  // a RefSegment or DerefSegment operation.
  void Visit(unsigned node_index,
             Segment node_segment,
             Segment query_segment,
             int refcount_delta);

  struct Node {
    // The ref count for a node tells the number of active segments (rectangles
    // intersecting the sweep line) that fully contain this node but not its
    // parent.  It's updated by RefSegment and DerefSegment.
    unsigned ref_count = 0;

    // Length-contribution of the intervals in this node's subtree that have
    // non-zero ref counts.
    unsigned active_length = 0;
  };

  const BasicIntervals& intervals_;
  Vector<Node> nodes_;
};

SegmentTree::SegmentTree(const BasicIntervals& intervals)
    : intervals_(intervals),
      nodes_(ComputeCapacity(intervals.NumIntervals())) {}

inline void SegmentTree::RefSegment(Segment segment) {
  Visit(0, RootSegment(), segment, 1);
}

inline void SegmentTree::DerefSegment(Segment segment) {
  Visit(0, RootSegment(), segment, -1);
}

inline unsigned SegmentTree::ActiveLength() const {
  return nodes_.front().active_length;
}

unsigned SegmentTree::ComputeCapacity(unsigned leaf_count) {
  unsigned cap = 1;
  while (cap < leaf_count)
    cap = cap << 1;
  return (cap << 1) - 1;
}

inline unsigned SegmentTree::LeftChild(unsigned node_index) {
  return (node_index << 1) + 1;
}

inline unsigned SegmentTree::RightChild(unsigned node_index) {
  return (node_index << 1) + 2;
}

inline Segment SegmentTree::RootSegment() const {
  return {0, intervals_.NumIntervals() - 1};
}

inline unsigned SegmentTree::ComputeActiveLength(unsigned node_index,
                                                 Segment node_segment) const {
  // If any segment fully covers the interval represented by this node,
  // then its active length contribution is the entire interval.
  if (nodes_[node_index].ref_count > 0)
    return intervals_.SegmentLength(node_segment);

  // Otherwise, it contributes only the active lengths of its children.
  if (node_segment.last_interval > node_segment.first_interval) {
    return nodes_[LeftChild(node_index)].active_length +
           nodes_[RightChild(node_index)].active_length;
  }
  return 0;
}

void SegmentTree::Visit(unsigned node_index,
                        Segment node_segment,
                        Segment query_segment,
                        int refcount_delta) {
  Node& node = nodes_[node_index];

  // node_segment is the interval represented by this node.  (We save some space
  // by computing it as we descend instead of storing it in the Node.)
  unsigned node_low = node_segment.first_interval;
  unsigned node_high = node_segment.last_interval;

  // query_segment is the interval we want to update within the node.
  unsigned query_low = query_segment.first_interval;
  unsigned query_high = query_segment.last_interval;

  DCHECK(query_low >= node_low && query_high <= node_high);

  if (node_low == query_low && node_high == query_high) {
    // The entire node is covered.
    node.ref_count += refcount_delta;
  } else {
    // Last interval in left subtree.
    unsigned lower_mid = (node_low + node_high) >> 1;
    // First interval in right subtree.
    unsigned upper_mid = lower_mid + 1;

    if (query_low <= lower_mid) {
      Visit(LeftChild(node_index), {node_low, lower_mid},
            {query_low, std::min(query_high, lower_mid)}, refcount_delta);
    }
    if (query_high >= upper_mid) {
      Visit(RightChild(node_index), {upper_mid, node_high},
            {std::max(query_low, upper_mid), query_high}, refcount_delta);
    }
  }
  node.active_length = ComputeActiveLength(node_index, node_segment);
}

// Runs the sweep line algorithm to compute the area of a set of rects.
class Sweeper {
 public:
  Sweeper(const Vector<IntRect>&);

  // Returns the area.
  uint64_t Sweep() const;

 private:
  void InitIntervals(BasicIntervals&) const;
  void InitEventQueue(Vector<SweepEvent>&, const BasicIntervals&) const;
  uint64_t SweepImpl(SegmentTree&, const Vector<SweepEvent>&) const;

  // The input.
  const Vector<IntRect>& rects_;
};

Sweeper::Sweeper(const Vector<IntRect>& rects) : rects_(rects) {}

uint64_t Sweeper::Sweep() const {
  BasicIntervals y_vals;
  InitIntervals(y_vals);
  SegmentTree tree(y_vals);

  Vector<SweepEvent> events;
  InitEventQueue(events, y_vals);
  return SweepImpl(tree, events);
}

void Sweeper::InitIntervals(BasicIntervals& y_vals) const {
  for (const IntRect& rect : rects_) {
    y_vals.AddEndpoint(rect.Y());
    y_vals.AddEndpoint(rect.MaxY());
  }
  y_vals.CreateIndex();
}

void Sweeper::InitEventQueue(Vector<SweepEvent>& events,
                             const BasicIntervals& y_vals) const {
  events.ReserveInitialCapacity(rects_.size() << 1);
  for (const IntRect& rect : rects_) {
    Segment segment = y_vals.SegmentFromEndpoints(rect.Y(), rect.MaxY());
    events.push_back(SweepEvent{rect.X(), EventType::START, segment});
    events.push_back(SweepEvent{rect.MaxX(), EventType::END, segment});
  }
  std::sort(events.begin(), events.end(),
            [](const SweepEvent& e1, const SweepEvent& e2) -> bool {
              return e1.x < e2.x;
            });
}

uint64_t Sweeper::SweepImpl(SegmentTree& tree,
                            const Vector<SweepEvent>& events) const {
  uint64_t area = 0;
  int sweep_x = events.front().x;

  for (const SweepEvent& e : events) {
    if (e.x > sweep_x) {
      area += (uint64_t)(e.x - sweep_x) * (uint64_t)tree.ActiveLength();
      sweep_x = e.x;
    }
    if (e.type == EventType::START)
      tree.RefSegment(e.y_segment);
    else
      tree.DerefSegment(e.y_segment);
  }
  return area;
}

}  // namespace

uint64_t LayoutShiftRegion::Area() const {
  if (rects_.IsEmpty())
    return 0;

  // Optimization: for a single rect, we don't need Sweeper.
  if (rects_.size() == 1) {
    const IntRect& rect = rects_.front();
    return rect.Width() * rect.Height();
  }
  return Sweeper(rects_).Sweep();
}

}  // namespace blink
