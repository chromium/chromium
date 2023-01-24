// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/container_query.h"
#include "third_party/blink/renderer/core/css/css_markup.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

ContainerQuery::ContainerQuery(ContainerSelector selector,
                               const MediaQueryExpNode* query)
    : selector_(std::move(selector)), query_(query) {}

ContainerQuery::ContainerQuery(const ContainerQuery& other)
    : selector_(other.selector_), query_(other.query_) {}

String ContainerQuery::ToString() const {
  StringBuilder result;
  String name = selector_.Name();
  if (!name.empty()) {
    SerializeIdentifier(name, result);
    result.Append(' ');
  }
  result.Append(query_->Serialize());
  return result.ReleaseString();
}

ContainerQuery* ContainerQuery::CopyWithParent(
    const ContainerQuery* parent) const {
  ContainerQuery* copy = MakeGarbageCollected<ContainerQuery>(*this);
  copy->parent_ = parent;
  return copy;
}

}  // namespace blink
