// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CONTAINER_QUERY_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CONTAINER_QUERY_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/media_query_exp.h"
#include "third_party/blink/renderer/core/layout/geometry/axis.h"

namespace blink {

// Not to be confused with regular selectors. This refers to container
// selection by e.g. name() or type().
//
// https://drafts.csswg.org/css-contain-3/#container-rule
class CORE_EXPORT ContainerSelector {
 public:
  ContainerSelector() = default;
  ContainerSelector(const ContainerSelector&) = default;
  explicit ContainerSelector(const AtomicString& name) : name_(name) {}
  explicit ContainerSelector(unsigned type) : type_(type) {}
  explicit ContainerSelector(const AtomicString& name, unsigned type)
      : name_(name), type_(type) {}

  bool IsNearest() const { return name_.IsNull() && type_ == 0; }

  const AtomicString& Name() const { return name_; }
  unsigned Type() const { return type_; }

  String ToString() const;

 private:
  AtomicString name_;
  // EContainerType
  unsigned type_ = 0;
};

class CORE_EXPORT ContainerQuery final
    : public GarbageCollected<ContainerQuery> {
 public:
  ContainerQuery(const ContainerSelector&,
                 std::unique_ptr<MediaQueryExpNode> query);
  ContainerQuery(const ContainerQuery&);

  const ContainerSelector& Selector() const { return selector_; }

  String ToString() const;

  void Trace(Visitor*) const {}

 private:
  friend class ContainerQueryTest;
  friend class ContainerQueryEvaluator;
  friend class CSSContainerRule;
  friend class StyleRuleContainer;

  const MediaQueryExpNode& Query() const { return *query_; }

  ContainerSelector selector_;
  std::unique_ptr<MediaQueryExpNode> query_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CONTAINER_QUERY_H_
