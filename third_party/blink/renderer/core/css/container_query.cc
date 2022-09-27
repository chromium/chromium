// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/container_query.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"

namespace blink {

ContainerQuery::ContainerQuery(ContainerSelector selector,
                               const MediaQueryExpNode* query)
    : selector_(std::move(selector)), query_(query) {}

ContainerQuery::ContainerQuery(const ContainerQuery& other)
    : selector_(other.selector_), query_(other.query_) {}

String ContainerQuery::ToString() const {
  return query_->Serialize();
}

ContainerQuery* ContainerQuery::CopyWithParent(
    const ContainerQuery* parent) const {
  ContainerQuery* copy = MakeGarbageCollected<ContainerQuery>(*this);
  copy->parent_ = parent;
  return copy;
}

}  // namespace blink
