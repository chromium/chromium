// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CONTAINER_QUERY_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CONTAINER_QUERY_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/media_query_exp.h"
#include "third_party/blink/renderer/core/layout/geometry/axis.h"

namespace blink {

class CORE_EXPORT ContainerQuery final
    : public GarbageCollected<ContainerQuery> {
 public:
  ContainerQuery(const AtomicString& name,
                 std::unique_ptr<MediaQueryExpNode> query);
  ContainerQuery(const ContainerQuery&);

  const AtomicString& Name() const { return name_; }
  PhysicalAxes QueriedAxes() const { return queried_axes_; }

  String ToString() const;

  void Trace(Visitor*) const {}

 private:
  friend class ContainerQueryTest;
  friend class ContainerQueryEvaluator;
  friend class CSSContainerRule;
  friend class StyleRuleContainer;

  const MediaQueryExpNode& Query() const { return *query_; }

  AtomicString name_;
  std::unique_ptr<MediaQueryExpNode> query_;
  PhysicalAxes queried_axes_{kPhysicalAxisNone};
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CONTAINER_QUERY_H_
