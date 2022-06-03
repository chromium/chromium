// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CONTAINER_QUERY_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CONTAINER_QUERY_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/media_list.h"
#include "third_party/blink/renderer/core/layout/geometry/axis.h"

namespace blink {

class CORE_EXPORT ContainerQuery final
    : public GarbageCollected<ContainerQuery> {
 public:
  ContainerQuery(const AtomicString& name, scoped_refptr<MediaQuerySet>);
  ContainerQuery(const ContainerQuery&);

  const AtomicString& Name() const { return name_; }
  PhysicalAxes QueriedAxes() const { return queried_axes_; }

  String ToString() const;

  void Trace(Visitor*) const {}

 private:
  friend class ContainerQueryTest;
  friend class ContainerQueryEvaluator;
  friend class CSSContainerRule;

  scoped_refptr<MediaQuerySet> MediaQueries() const { return media_queries_; }

  AtomicString name_;
  // TODO(crbug.com/1214810): Refactor to avoid internal MediaQuerySet.
  scoped_refptr<MediaQuerySet> media_queries_;
  PhysicalAxes queried_axes_{kPhysicalAxisNone};
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CONTAINER_QUERY_H_
