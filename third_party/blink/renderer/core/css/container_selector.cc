// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/container_query.h"
#include "third_party/blink/renderer/core/css/media_query_exp.h"
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
  if (feature_flags & MediaQueryExpNode::kFeatureStyle)
    has_style_query_ = true;
}

unsigned ContainerSelector::GetHash() const {
  unsigned hash = !name_.empty() ? AtomicStringHash::GetHash(name_) : 0;
  WTF::AddIntToHash(hash, physical_axes_.value());
  WTF::AddIntToHash(hash, logical_axes_.value());
  WTF::AddIntToHash(hash, has_style_query_);
  return hash;
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

}  // namespace blink
