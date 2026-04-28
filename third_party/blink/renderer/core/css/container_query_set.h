// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CONTAINER_QUERY_SET_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CONTAINER_QUERY_SET_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class ContainerQuery;
class StringBuilder;

class CORE_EXPORT ContainerQuerySet
    : public GarbageCollected<ContainerQuerySet> {
 public:
  explicit ContainerQuerySet(
      HeapVector<Member<const ContainerQuery>>&& queries) : queries_(std::move(queries)) {
  }

  ContainerQuerySet(const ContainerQuerySet&) = default;

  const HeapVector<Member<const ContainerQuery>>& Queries() const {
    return queries_;
  }

  const ContainerQuery* SingleQuery() const {
    if (queries_.size() == 1u) {
      return queries_[0].Get();
    }
    return nullptr;
  }

  const ContainerQuerySet* Parent() const { return parent_.Get(); }
  const ContainerQuerySet* CopyWithParent(const ContainerQuerySet*) const;

  void Serialize(StringBuilder&) const;
  String ToString() const;

  void Trace(Visitor*) const;

 private:
  HeapVector<Member<const ContainerQuery>> queries_;
  Member<const ContainerQuerySet> parent_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CONTAINER_QUERY_SET_H_
