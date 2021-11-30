// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/container_query.h"
#include "third_party/blink/renderer/core/css/media_query_exp.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

String ContainerSelector::ToString() const {
  StringBuilder builder;

  if (!name_.IsNull()) {
    if (type_)
      builder.Append("name(");
    builder.Append(name_);
    if (type_)
      builder.Append(") ");
  }

  if (type_) {
    builder.Append("type(");
    if ((type_ & kContainerTypeSize) == kContainerTypeSize) {
      builder.Append("size");
    } else if (type_ & kContainerTypeInlineSize) {
      builder.Append("inline-size");
    } else if (type_ & kContainerTypeBlockSize) {
      builder.Append("block-size");
    }
    builder.Append(")");
  }

  return builder.ReleaseString();
}

ContainerQuery::ContainerQuery(const ContainerSelector& selector,
                               std::unique_ptr<MediaQueryExpNode> query)
    : selector_(selector), query_(std::move(query)) {}

ContainerQuery::ContainerQuery(const ContainerQuery& other)
    : selector_(other.selector_), query_(other.query_->Copy()) {}

String ContainerQuery::ToString() const {
  return query_->Serialize();
}

}  // namespace blink
