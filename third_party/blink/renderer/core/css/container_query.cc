// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/container_query.h"
#include "third_party/blink/renderer/core/css/media_query_exp.h"

namespace blink {

ContainerQuery::ContainerQuery(const AtomicString& name,
                               std::unique_ptr<MediaQueryExpNode> query)
    : name_(name),
      query_(std::move(query)),
      queried_axes_(query_->QueriedAxes()) {}

ContainerQuery::ContainerQuery(const ContainerQuery& other)
    : query_(other.query_->Copy()), queried_axes_(other.queried_axes_) {}

String ContainerQuery::ToString() const {
  return query_->Serialize();
}

}  // namespace blink
