// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/container_query.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"

namespace blink {

ContainerSelector::ContainerSelector(AtomicString name,
                                     const MediaQueryExpNode& query)
    : name_(std::move(name)) {
  MediaQueryExpNode::FeatureFlags feature_flags = query.CollectFeatureFlags();

  if (feature_flags & MediaQueryExpNode::kFeatureInlineSize)
    logical_axes_ |= kLogicalAxisInline;
  if (feature_flags & MediaQueryExpNode::kFeatureBlockSize)
    logical_axes_ |= kLogicalAxisBlock;
  if (feature_flags & MediaQueryExpNode::kFeatureWidth)
    physical_axes_ |= kPhysicalAxisHorizontal;
  if (feature_flags & MediaQueryExpNode::kFeatureHeight)
    physical_axes_ |= kPhysicalAxisVertical;
}

unsigned ContainerSelector::Type(WritingMode writing_mode) const {
  unsigned type = kContainerTypeNormal;

  LogicalAxes axes =
      logical_axes_ | ToLogicalAxes(physical_axes_, writing_mode);

  if ((axes & kLogicalAxisInline).value())
    type |= kContainerTypeInlineSize;
  if ((axes & kLogicalAxisBlock).value())
    type |= kContainerTypeBlockSize;

  return type;
}

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
