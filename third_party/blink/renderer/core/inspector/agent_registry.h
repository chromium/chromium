// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_AGENT_REGISTRY_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_AGENT_REGISTRY_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"

namespace blink {

// A set of agents. Support modification while iterating by means of
// Copy-On-Write.
template <class AgentType>
class CORE_EXPORT AgentRegistry {
  DISALLOW_NEW();

 public:
  AgentRegistry() : data_(MakeGarbageCollected<Data>()) {}

  // Add an agent to this list. An agent should not be added to the same
  // list more than once.
  void AddAgent(AgentType* agent) {
    if (HasAgent(agent))
      return;
    if (!RequiresCopy()) {
      data_->agents.push_back(agent);
      is_empty_ = false;
      return;
    }
    data_ = MakeGarbageCollected<Data>(*data_);
    data_->agents.push_back(agent);
    is_empty_ = false;
    iteration_counter_ = 0;
  }

  // Removes the given agent from this list. Does nothing if this agent is
  // not in this list.
  void RemoveAgent(AgentType* agent) {
    if (!HasAgent(agent))
      return;
    wtf_size_t position = data_->agents.Find(agent);
    if (!RequiresCopy()) {
      data_->agents.EraseAt(position);
      is_empty_ = data_->agents.empty();
      return;
    }
    data_ = MakeGarbageCollected<Data>(*data_);
    data_->agents.EraseAt(position);
    is_empty_ = data_->agents.empty();
    iteration_counter_ = 0;
  }

  // Returns true if the list is being iterated over and requires copy for
  // modification.
  bool RequiresCopy() const { return iteration_counter_ != 0; }

  // This method must be safe to call during finalization.
  bool IsEmpty() const { return is_empty_; }

  wtf_size_t size() const { return data_->agents.size(); }

  bool HasAgent(AgentType* agent) const {
    return data_->agents.Find(agent) != kNotFound;
  }

  // Safely iterate over the registered agents.
  //
  // Sample usage:
  //     ForEachAgent([](AgentType* agent) {
  //       agent->SomeMethod();
  //     });
  template <typename ForEachCallable>
  void ForEachAgent(const ForEachCallable& callable) const {
    iteration_counter_++;
    Member<Data> snapshot = data_;
    for (const Member<AgentType>& agent : snapshot->agents) {
      callable(agent);
    }
    if (iteration_counter_ > 0)
      iteration_counter_--;
  }

  void Trace(Visitor* visitor) const { visitor->Trace(data_); }

 private:
  struct Data : public GarbageCollected<Data> {
    HeapVector<Member<AgentType>> agents;
    void Trace(Visitor* visitor) const { visitor->Trace(agents); }
  };

  // Number of iteration over original vector is recorded.
  mutable size_t iteration_counter_ = 0;
  bool is_empty_ = true;
  Member<Data> data_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_AGENT_REGISTRY_H_
