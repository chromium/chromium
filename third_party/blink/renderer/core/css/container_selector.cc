// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/container_selector.h"

#include "third_party/blink/renderer/core/css/conditional_exp_node.h"
#include "third_party/blink/renderer/core/css/media_feature_names.h"
#include "third_party/blink/renderer/core/css/media_query_exp.h"
#include "third_party/blink/renderer/core/dom/tree_scope.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"

namespace blink {

namespace {

ContainerSelector::FeatureFlags GetFeatureFlags(const MediaQueryExp& exp) {
  if (exp.HasMediaFeature()) {
    if (exp.MediaFeature() == media_feature_names::kStuckMediaFeature) {
      return ContainerSelector::kFeatureSticky;
    } else if (exp.MediaFeature() ==
               media_feature_names::kSnappedMediaFeature) {
      return ContainerSelector::kFeatureSnap;
    } else if (exp.MediaFeature() ==
               media_feature_names::kScrollableMediaFeature) {
      return ContainerSelector::kFeatureScrollable;
    } else if (exp.MediaFeature() ==
               media_feature_names::kScrolledMediaFeature) {
      return ContainerSelector::kFeatureScrolled;
    } else if (exp.MediaFeature() ==
               media_feature_names::kFallbackMediaFeature) {
      return ContainerSelector::kFeatureAnchored;
    } else if (exp.IsInlineSizeDependent()) {
      return ContainerSelector::kFeatureInlineSize;
    } else if (exp.IsBlockSizeDependent()) {
      return ContainerSelector::kFeatureBlockSize;
    }
  }
  ContainerSelector::FeatureFlags flags = 0;
  if (exp.IsWidthDependent()) {
    flags |= ContainerSelector::kFeatureWidth;
  }
  if (exp.IsHeightDependent()) {
    flags |= ContainerSelector::kFeatureHeight;
  }
  return flags;
}

}  // anonymous namespace

ContainerSelector::ContainerSelector(AtomicString name,
                                     const ConditionalExpNode& query)
    : name_(std::move(name)) {
  FeatureFlags feature_flags = CollectFeatureFlags(query);

  if (feature_flags & kFeatureInlineSize) {
    logical_axes_ |= kLogicalAxesInline;
  }
  if (feature_flags & kFeatureBlockSize) {
    logical_axes_ |= kLogicalAxesBlock;
  }
  if (feature_flags & kFeatureWidth) {
    physical_axes_ |= kPhysicalAxesHorizontal;
  }
  if (feature_flags & kFeatureHeight) {
    physical_axes_ |= kPhysicalAxesVertical;
  }
  if (feature_flags & kFeatureStyle) {
    has_style_query_ = true;
  }
  if (feature_flags & kFeatureSticky) {
    has_sticky_query_ = true;
  }
  if (feature_flags & kFeatureSnap) {
    has_snap_query_ = true;
  }
  if (feature_flags & kFeatureScrollable) {
    has_scrollable_query_ = true;
  }
  if (feature_flags & kFeatureScrolled) {
    has_scrolled_query_ = true;
  }
  if (feature_flags & kFeatureAnchored) {
    has_anchored_query_ = true;
  }
  if (feature_flags & kFeatureUnknown) {
    has_unknown_feature_ = true;
  }
}

ContainerSelector::FeatureFlags ContainerSelector::CollectFeatureFlags(
    const ConditionalExpNode& root) {
  class FeatureCollector : public ConditionalExpNodeVisitor {
   public:
    FeatureFlags GetFlags() const { return flags_; }

   private:
    KleeneValue EvaluateMediaQueryFeatureExpNode(
        const MediaQueryFeatureExpNode& node) override {
      flags_ |= GetFeatureFlags(node.GetMediaQueryExp());
      return KleeneValue::kUnknown;
    }
    KleeneValue EvaluateUnknown(const ConditionalExpNodeUnknown&) override {
      flags_ |= kFeatureUnknown;
      return KleeneValue::kUnknown;
    }
    void EnterFunction(const ConditionalExpNodeFunction& function) override {
      if (function.GetName() == AtomicString("style")) {
        flags_ |= kFeatureStyle;
      }
    }

    FeatureFlags flags_ = 0;
  };

  FeatureCollector collector;
  root.Evaluate(collector);
  return collector.GetFlags();
}

unsigned ContainerSelector::GetHash() const {
  unsigned hash = !name_.empty() ? blink::GetHash(name_) : 0;
  AddIntToHash(hash, physical_axes_.value());
  AddIntToHash(hash, logical_axes_.value());
  AddIntToHash(hash, has_style_query_);
  AddIntToHash(hash, has_sticky_query_);
  AddIntToHash(hash, has_snap_query_);
  AddIntToHash(hash, has_scrollable_query_);
  AddIntToHash(hash, has_scrolled_query_);
  AddIntToHash(hash, has_anchored_query_);
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
  if (SelectsScrollStateContainers()) {
    type |= kContainerTypeScrollState;
  }
  if (SelectsAnchoredContainers()) {
    type |= kContainerTypeAnchored;
  }
  return type;
}

void ScopedContainerSelector::Trace(Visitor* visitor) const {
  visitor->Trace(tree_scope_);
}

}  // namespace blink
