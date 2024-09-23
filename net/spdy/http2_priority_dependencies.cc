// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "net/spdy/http2_priority_dependencies.h"

#include "base/not_fatal_until.h"
#include "base/trace_event/memory_usage_estimator.h"

namespace net {

Http2PriorityDependencies::Http2PriorityDependencies() = default;

Http2PriorityDependencies::~Http2PriorityDependencies() = default;

void Http2PriorityDependencies::OnStreamCreation(
    spdy::SpdyStreamId id,
    spdy::SpdyPriority priority,
    spdy::SpdyStreamId* parent_stream_id,
    int* weight,
    bool* exclusive) {
  if (entry_by_stream_id_.find(id) != entry_by_stream_id_.end())
    return;

  *parent_stream_id = 0;
  *exclusive = true;
  // Since the generated dependency graph is a single linked list, the value
  // of weight should not actually matter, and perhaps the default weight of 16
  // from the HTTP/2 spec would be reasonable. However, there are some servers
  // which currently interpret the weight field like an old SPDY priority value.
  // As long as those servers need to be supported, weight should be set to
  // a value those servers will interpret correctly.
  *weight = spdy::Spdy3PriorityToHttp2Weight(priority);

  // Dependent on the lowest-priority stream that has a priority >= |priority|.
  IdList::iterator parent;
  if (PriorityLowerBound(priority, &parent)) {
    *parent_stream_id = parent->first;
  }

  id_priority_lists_[priority].emplace_back(id, priority);
  auto it = id_priority_lists_[priority].end();
  --it;
  entry_by_stream_id_[id] = it;
}

bool Http2PriorityDependencies::PriorityLowerBound(spdy::SpdyPriority priority,
                                                   IdList::iterator* bound) {
  for (int i = priority; i >= spdy::kV3HighestPriority; --i) {
    if (!id_priority_lists_[i].empty()) {
      *bound = id_priority_lists_[i].end();
      --(*bound);
      return true;
    }
  }
  return false;
}

bool Http2PriorityDependencies::ParentOfStream(spdy::SpdyStreamId id,
                                               IdList::iterator* parent) {
  auto entry = entry_by_stream_id_.find(id);
  CHECK(entry != entry_by_stream_id_.end(), base::NotFatalUntil::M130);

  spdy::SpdyPriority priority = entry->second->second;
  auto curr = entry->second;
  if (curr != id_priority_lists_[priority].begin()) {
    *parent = curr;
    --(*parent);
    return true;
  }

  // |id| is at the head of its priority list, so its parent is the last
  // entry of the next-highest priority band.
  if (priority == spdy::kV3HighestPriority) {
    return false;
  }
  return PriorityLowerBound(priority - 1, parent);
}

bool Http2PriorityDependencies::ChildOfStream(spdy::SpdyStreamId id,
                                              IdList::iterator* child) {
  auto entry = entry_by_stream_id_.find(id);
  CHECK(entry != entry_by_stream_id_.end(), base::NotFatalUntil::M130);

  spdy::SpdyPriority priority = entry->second->second;
  *child = entry->second;
  ++(*child);
  if (*child != id_priority_lists_[priority].end()) {
    return true;
  }

  // |id| is at the end of its priority list, so its child is the stream
  // at the front of the next-lowest priority band.
  for (int i = priority + 1; i <= spdy::kV3LowestPriority; ++i) {
    if (!id_priority_lists_[i].empty()) {
      *child = id_priority_lists_[i].begin();
      return true;
    }
  }

  return false;
}

std::vector<Http2PriorityDependencies::DependencyUpdate>
Http2PriorityDependencies::OnStreamUpdate(spdy::SpdyStreamId id,
                                          spdy::SpdyPriority new_priority) {
  std::vector<DependencyUpdate> result;
  result.reserve(2);

  auto curr_entry = entry_by_stream_id_.find(id);
  if (curr_entry == entry_by_stream_id_.end()) {
    return result;
  }

  spdy::SpdyPriority old_priority = curr_entry->second->second;
  if (old_priority == new_priority) {
    return result;
  }

  IdList::iterator old_parent;
  bool old_has_parent = ParentOfStream(id, &old_parent);

  IdList::iterator new_parent;
  bool new_has_parent = PriorityLowerBound(new_priority, &new_parent);

  // If we move |id| from MEDIUM to LOW, where HIGH = {other_id}, MEDIUM = {id},
  // and LOW = {}, then PriorityLowerBound(new_priority) is |id|. In this corner
  // case, |id| does not change parents.
  if (new_has_parent && new_parent->first == id) {
    new_has_parent = old_has_parent;
    new_parent = old_parent;
  }

  // If the parent has changed, we generate dependency updates.
  if ((old_has_parent != new_has_parent) ||
      (old_has_parent && old_parent->first != new_parent->first)) {
    // If |id| has a child, then that child moves to be dependent on
    // |old_parent|.
    IdList::iterator old_child;
    if (ChildOfStream(id, &old_child)) {
      int weight = spdy::Spdy3PriorityToHttp2Weight(old_child->second);
      if (old_has_parent) {
        result.push_back({old_child->first, old_parent->first, weight, true});
      } else {
        result.push_back({old_child->first, 0, weight, true});
      }
    }

    int weight = spdy::Spdy3PriorityToHttp2Weight(new_priority);
    // |id| moves to be dependent on |new_parent|.
    if (new_has_parent) {
      result.push_back({id, new_parent->first, weight, true});
    } else {
      result.push_back({id, 0, weight, true});
    }
  }

  // Move to the new priority.
  auto old = entry_by_stream_id_.find(id);
  id_priority_lists_[old->second->second].erase(old->second);
  id_priority_lists_[new_priority].emplace_back(id, new_priority);
  auto it = id_priority_lists_[new_priority].end();
  --it;
  entry_by_stream_id_[id] = it;

  return result;
}

void Http2PriorityDependencies::OnStreamDestruction(spdy::SpdyStreamId id) {
  auto emit = entry_by_stream_id_.find(id);
  if (emit == entry_by_stream_id_.end())
    return;

  auto it = emit->second;
  id_priority_lists_[it->second].erase(it);
  entry_by_stream_id_.erase(emit);
}

}  // namespace net
