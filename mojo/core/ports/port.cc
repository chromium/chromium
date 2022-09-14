// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/core/ports/port.h"
#include <utility>

namespace mojo {
namespace core {
namespace ports {

// Used by std::{push,pop}_heap functions
inline bool operator<(const std::unique_ptr<Event>& a,
                      const std::unique_ptr<Event>& b) {
  return a->control_sequence_num() > b->control_sequence_num();
}

Port::Port(uint64_t next_sequence_num_to_send,
           uint64_t next_sequence_num_to_receive)
    : state(kUninitialized),
      pending_merge_peer(false),
      next_control_sequence_num_to_send(kInitialSequenceNum),
      next_control_sequence_num_to_receive(kInitialSequenceNum),
      next_sequence_num_to_send(next_sequence_num_to_send),
      last_sequence_num_acknowledged(next_sequence_num_to_send - 1),
      sequence_num_acknowledge_interval(0),
      last_sequence_num_to_receive(0),
      sequence_num_to_acknowledge(0),
      message_queue(next_sequence_num_to_receive),
      remove_proxy_on_last_message(false),
      peer_closed(false),
      peer_lost_unexpectedly(false) {}

Port::~Port() = default;

bool Port::IsNextEvent(const NodeName& from_node, const Event& event) {
  if (from_node != prev_node_name)
    return false;

  if (event.from_port() != prev_port_name)
    return false;

  DCHECK_GE(event.control_sequence_num(), next_control_sequence_num_to_receive);
  return event.control_sequence_num() == next_control_sequence_num_to_receive;
}

void Port::NextEvent(NodeName* from_node, ScopedEvent* event) {
  auto it = control_event_queues_.find({prev_node_name, prev_port_name});
  if (it == control_event_queues_.end())
    return;

  auto& msg_queue = it->second;
  // There must always be one entry since we delete the queue after processing
  // the last element.
  DCHECK_GE(msg_queue.size(), 1lu);

  if (msg_queue[0]->control_sequence_num() !=
      next_control_sequence_num_to_receive)
    return;

  std::pop_heap(msg_queue.begin(), msg_queue.end());
  *from_node = prev_node_name;
  *event = std::move(msg_queue.back());
  msg_queue.pop_back();
  if (msg_queue.size() == 0) {
    control_event_queues_.erase(it);
  }
}

void Port::BufferEvent(const NodeName& from_node, ScopedEvent event) {
  DCHECK(!IsNextEvent(from_node, *event));

  auto& event_heap = control_event_queues_[{from_node, event->from_port()}];
  event_heap.emplace_back(std::move(event));
  std::push_heap(event_heap.begin(), event_heap.end());
}

void Port::TakePendingMessages(
    std::vector<std::unique_ptr<UserMessageEvent>>& messages) {
  for (auto& node_queue_pair : control_event_queues_) {
    auto& events = node_queue_pair.second;
    for (auto& event : events) {
      if (event->type() != Event::Type::kUserMessage)
        continue;
      messages.emplace_back(Event::Cast<UserMessageEvent>(&event));
    }
  }
  control_event_queues_.clear();
}

}  // namespace ports
}  // namespace core
}  // namespace mojo
