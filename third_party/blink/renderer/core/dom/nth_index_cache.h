// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOM_NTH_INDEX_CACHE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOM_NTH_INDEX_CACHE_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"

namespace blink {

class Document;

class CORE_EXPORT NthIndexData final : public GarbageCollected<NthIndexData> {
 public:
  NthIndexData(ContainerNode&);
  NthIndexData(ContainerNode&, const QualifiedName& type);
  NthIndexData(const NthIndexData&) = delete;
  NthIndexData& operator=(const NthIndexData&) = delete;

  unsigned NthIndex(Element&) const;
  unsigned NthLastIndex(Element&) const;
  unsigned NthOfTypeIndex(Element&) const;
  unsigned NthLastOfTypeIndex(Element&) const;

  void Trace(Visitor*) const;

 private:
  HeapHashMap<Member<Element>, unsigned> element_index_map_;
  unsigned count_ = 0;
};

class CORE_EXPORT NthIndexCache final {
  STACK_ALLOCATED();

 public:
  explicit NthIndexCache(Document&);
  NthIndexCache(const NthIndexCache&) = delete;
  NthIndexCache& operator=(const NthIndexCache&) = delete;
  ~NthIndexCache();

  static unsigned NthChildIndex(Element&);
  static unsigned NthLastChildIndex(Element&);
  static unsigned NthOfTypeIndex(Element&);
  static unsigned NthLastOfTypeIndex(Element&);

 private:
  using IndexByType = HeapHashMap<String, Member<NthIndexData>>;
  using ParentMap = HeapHashMap<Member<Node>, Member<NthIndexData>>;
  using ParentMapForType = HeapHashMap<Member<Node>, Member<IndexByType>>;

  void CacheNthIndexDataForParent(Element&);
  void CacheNthOfTypeIndexDataForParent(Element&);
  IndexByType& EnsureTypeIndexMap(ContainerNode&);
  NthIndexData* NthTypeIndexDataForParent(Element&) const;

  Document* document_ = nullptr;
  ParentMap* parent_map_ = nullptr;
  ParentMapForType* parent_map_for_type_ = nullptr;

#if DCHECK_IS_ON()
  uint64_t dom_tree_version_;
#endif
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOM_NTH_INDEX_CACHE_H_
