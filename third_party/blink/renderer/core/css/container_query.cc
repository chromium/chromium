// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/container_query.h"
#include "third_party/blink/renderer/core/css/media_query_exp.h"

namespace blink {

namespace {

PhysicalAxes ComputeQueriedAxes(const MediaQuerySet& media_queries) {
  PhysicalAxes axes(kPhysicalAxisNone);

  for (const auto& media_query : media_queries.QueryVector()) {
    for (const auto& expression : media_query->Expressions()) {
      if (expression.IsWidthDependent())
        axes |= PhysicalAxes(kPhysicalAxisHorizontal);
      if (expression.IsHeightDependent())
        axes |= PhysicalAxes(kPhysicalAxisVertical);
    }
  }

  return axes;
}

}  // namespace

ContainerQuery::ContainerQuery(const AtomicString& name,
                               scoped_refptr<MediaQuerySet> media_queries)
    : name_(name),
      media_queries_(media_queries),
      queried_axes_(ComputeQueriedAxes(*media_queries)) {}

ContainerQuery::ContainerQuery(const ContainerQuery& other)
    : media_queries_(other.media_queries_->Copy()),
      queried_axes_(other.queried_axes_) {}

String ContainerQuery::ToString() const {
  return media_queries_->MediaText();
}

}  // namespace blink
