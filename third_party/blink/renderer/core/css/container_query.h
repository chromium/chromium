// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CONTAINER_QUERY_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CONTAINER_QUERY_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/media_query_exp.h"
#include "third_party/blink/renderer/core/layout/geometry/axis.h"
#include "third_party/blink/renderer/platform/text/writing_mode.h"

namespace blink {

// Not to be confused with regular selectors. This refers to container
// selection by e.g. a given name, or by implicit container selection
// according to the queried features.
//
// https://drafts.csswg.org/css-contain-3/#container-rule
class CORE_EXPORT ContainerSelector {
 public:
  ContainerSelector() = default;
  ContainerSelector(const ContainerSelector&) = default;
  explicit ContainerSelector(AtomicString name) : name_(std::move(name)) {}
  explicit ContainerSelector(PhysicalAxes physical_axes)
      : physical_axes_(physical_axes) {}
  ContainerSelector(AtomicString name, LogicalAxes physical_axes)
      : name_(std::move(name)), logical_axes_(physical_axes) {}
  ContainerSelector(AtomicString name, const MediaQueryExpNode&);

  const AtomicString& Name() const { return name_; }

  // Given the specified writing mode, return the EContainerTypes required
  // for this selector to match.
  unsigned Type(WritingMode) const;

  bool SelectsSizeContainers() const {
    return physical_axes_ != kPhysicalAxisNone ||
           logical_axes_ != kLogicalAxisNone;
  }

  bool SelectsStyleContainers() const { return has_style_query_; }

 private:
  AtomicString name_;
  PhysicalAxes physical_axes_{kPhysicalAxisNone};
  LogicalAxes logical_axes_{kLogicalAxisNone};
  bool has_style_query_{false};
};

class CORE_EXPORT ContainerQuery final
    : public GarbageCollected<ContainerQuery> {
 public:
  ContainerQuery(ContainerSelector, const MediaQueryExpNode* query);
  ContainerQuery(const ContainerQuery&);

  const ContainerSelector& Selector() const { return selector_; }
  const ContainerQuery* Parent() const { return parent_.Get(); }

  ContainerQuery* CopyWithParent(const ContainerQuery*) const;

  String ToString() const;

  void Trace(Visitor* visitor) const {
    visitor->Trace(query_);
    visitor->Trace(parent_);
  }

 private:
  friend class ContainerQueryTest;
  friend class ContainerQueryEvaluator;
  friend class CSSContainerRule;
  friend class StyleRuleContainer;

  const MediaQueryExpNode& Query() const { return *query_; }

  ContainerSelector selector_;
  Member<const MediaQueryExpNode> query_;
  Member<const ContainerQuery> parent_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CONTAINER_QUERY_H_
