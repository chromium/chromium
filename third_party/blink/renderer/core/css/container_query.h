// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CONTAINER_QUERY_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CONTAINER_QUERY_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/container_selector.h"

namespace blink {

class ConditionalExpNode;

class CORE_EXPORT ContainerQuery final
    : public GarbageCollected<ContainerQuery> {
 public:
  ContainerQuery(ContainerSelector, const ConditionalExpNode* query);
  ContainerQuery(const ContainerQuery&);

  const ContainerSelector& Selector() const { return selector_; }
  const ContainerQuery* Parent() const { return parent_.Get(); }

  ContainerQuery* CopyWithParent(const ContainerQuery*) const;

  String ToString() const;

  void Trace(Visitor*) const;

 private:
  friend class ContainerQueryTest;
  friend class ContainerQueryEvaluator;
  friend class CSSContainerRule;
  friend class StyleRuleContainer;

  const ConditionalExpNode& Query() const { return *query_; }

  ContainerSelector selector_;
  Member<const ConditionalExpNode> query_;
  Member<const ContainerQuery> parent_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CONTAINER_QUERY_H_
