// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/container_query_set.h"

#include "third_party/blink/renderer/core/css/container_query.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

const ContainerQuerySet* ContainerQuerySet::CopyWithParent(
    const ContainerQuerySet* parent) const {
  ContainerQuerySet* copy = MakeGarbageCollected<ContainerQuerySet>(*this);
  copy->parent_ = parent;
  return copy;
}

void ContainerQuerySet::Serialize(StringBuilder& result) const {
  bool first = true;
  for (const auto& query : queries_) {
    if (first) {
      first = false;
    } else {
      result.Append(", ");
    }
    query->Serialize(result);
  }
}

String ContainerQuerySet::ToString() const {
  StringBuilder result;
  Serialize(result);
  return result.ReleaseString();
}

void ContainerQuerySet::Trace(Visitor* visitor) const {
  visitor->Trace(queries_);
  visitor->Trace(parent_);
}

}  // namespace blink
