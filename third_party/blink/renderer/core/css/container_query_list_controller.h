// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CONTAINER_QUERY_LIST_CONTROLLER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CONTAINER_QUERY_LIST_CONTROLLER_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_linked_hash_set.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

class ContainerQueryList;
class Element;
class LocalDOMWindow;

class CORE_EXPORT ContainerQueryListController final
    : public GarbageCollected<ContainerQueryListController>,
      public Supplement<LocalDOMWindow> {
 public:
  static const char kSupplementName[];
  static ContainerQueryListController* From(LocalDOMWindow&);
  static ContainerQueryListController* FromIfExists(LocalDOMWindow&);

  explicit ContainerQueryListController(LocalDOMWindow&);

  void AddContainerQueryList(Element&, ContainerQueryList&);

  bool NotifyChanges();

  void Trace(Visitor*) const override;

 private:
  using ListSet = GCedHeapLinkedHashSet<WeakMember<ContainerQueryList>>;

  HeapLinkedHashSet<WeakMember<Element>> elements_;
  HeapHashMap<WeakMember<Element>, Member<ListSet>> lists_by_element_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CONTAINER_QUERY_LIST_CONTROLLER_H_
