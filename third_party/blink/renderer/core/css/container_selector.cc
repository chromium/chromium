// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/container_query.h"
#include "third_party/blink/renderer/core/css/media_query_exp.h"
#include "third_party/blink/renderer/core/dom/tree_scope.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"

namespace blink {

ContainerSelector::ContainerSelector(AtomicString name,
                                     const MediaQueryExpNode& query)
    : name_(std::move(name)) {
  MediaQueryExpNode::FeatureFlags feature_flags = query.CollectFeatureFlags();

  if (feature_flags & MediaQueryExpNode::kFeatureInlineSize) {
    logical_axes_ |= kLogicalAxesInline;
  }
  if (feature_flags & MediaQueryExpNode::kFeatureBlockSize) {
    logical_axes_ |= kLogicalAxesBlock;
  }
  if (feature_flags & MediaQueryExpNode::kFeatureWidth) {
    physical_axes_ |= kPhysicalAxesHorizontal;
  }
  if (feature_flags & MediaQueryExpNode::kFeatureHeight) {
    physical_axes_ |= kPhysicalAxesVertical;
  }
  if (feature_flags & MediaQueryExpNode::kFeatureStyle) {
    has_style_query_ = true;
  }
  if (feature_flags & MediaQueryExpNode::kFeatureSticky) {
    has_sticky_query_ = true;
  }
  if (feature_flags & MediaQueryExpNode::kFeatureSnap) {
    has_snap_query_ = true;
  }
  if (feature_flags & MediaQueryExpNode::kFeatureUnknown) {
    has_unknown_feature_ = true;
  }
}

unsigned ContainerSelector::GetHash() const {
  unsigned hash = !name_.empty() ? WTF::GetHash(name_) : 0;
  WTF::AddIntToHash(hash, physical_axes_.value());
  WTF::AddIntToHash(hash, logical_axes_.value());
  WTF::AddIntToHash(hash, has_style_query_);
  WTF::AddIntToHash(hash, has_sticky_query_);
  WTF::AddIntToHash(hash, has_snap_query_);
  return hash;
}

unsigned ContainerSelector::Type(WritingMode writing_mode) const {
  unsigned type = kContainerTypeNormal;

  LogicalAxes axes =
      logical_axes_ | ToLogicalAxes(physical_axes_, writing_mode);

  if ((axes & kLogicalAxesInline).value()) {
    type |= kContainerTypeInlineSize;
  }
  if ((axes & kLogicalAxesBlock).value()) {
    type |= kContainerTypeBlockSize;
  }
  if (has_sticky_query_ || has_snap_query_) {
    type |= kContainerTypeScrollState;
  }
  return type;
}

void ScopedContainerSelector::Trace(Visitor* visitor) const {
  visitor->Trace(tree_scope_);
}

}  // namespace blink
