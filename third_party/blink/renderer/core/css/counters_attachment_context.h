// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_COUNTERS_ATTACHMENT_CONTEXT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_COUNTERS_ATTACHMENT_CONTEXT_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

class Element;

// This class is used to keep track of the current counter values for a
// document.
// It is used to calculate the counter() and counters() CSS function values.
class CORE_EXPORT CountersAttachmentContext {
  STACK_ALLOCATED();

  using CounterStack = HeapVector<Member<const Element>>;
  using CounterValues = HeapHashMap<Member<const Element>, int>;
  using CounterValueTable = HeapHashMap<AtomicString, Member<CounterValues>>;
  using CounterInheritanceTable =
      HeapHashMap<AtomicString, Member<CounterStack>>;

 public:
  enum class Type {
    kIncrementType = 1 << 0,
    kResetType = 1 << 1,
    kSetType = 1 << 2
  };

  CountersAttachmentContext();

  void EnterElement(const Element& element);
  void LeaveElement(const Element& element);
  // only_last = true for counter(), = false for counters().
  Vector<int> GetCounterValues(const Element& element,
                               const AtomicString& counter_name,
                               bool only_last);
  void SetAttachmentRootIsDocumentElement() {
    attachment_root_is_document_element_ = true;
  }
  bool AttachmentRootIsDocumentElement() const {
    return attachment_root_is_document_element_;
  }
  static bool ElementGeneratesListItemCounter(const Element& element);

 private:
  void CreateCounter(const Element& element,
                     const AtomicString& counter_name,
                     int value);
  void RemoveStaleCounters(const Element& element,
                           const AtomicString& counter_name);
  void RemoveCounterIfAncestorExists(const Element& element,
                                     const AtomicString& counter_name);
  void SetCounterValue(const Element& element,
                       const AtomicString& counter_name,
                       int value);
  int GetCounterValue(const Element& element, const AtomicString& counter_name);
  void UpdateCounterValue(const Element& element,
                          const AtomicString& counter_name,
                          unsigned counter_type,
                          int counter_value);
  void MaybeCreateListItemCounter(const Element& element);
  void EnterStyleContainmentScope();
  void LeaveStyleContainmentScope();

  AtomicString list_item_{"list-item"};
  // True if attachment started from documentElement. If true, counters
  // calculations are done as part of layout tree attachment.
  bool attachment_root_is_document_element_ = false;
  // Hash table of counter name <-> {Hash table of element <-> current value},
  // for keeping track of current counter value.
  CounterValueTable* counter_value_table_ = nullptr;
  // Hash table of counter name <-> counters stack, for inheritance.
  CounterInheritanceTable* counter_inheritance_table_ = nullptr;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_COUNTERS_ATTACHMENT_CONTEXT_H_
