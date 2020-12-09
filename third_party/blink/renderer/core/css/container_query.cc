// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/container_query.h"

namespace blink {

ContainerQuery::ContainerQuery(scoped_refptr<MediaQuerySet> media_queries)
    : media_queries_(media_queries) {}

ContainerQuery::ContainerQuery(const ContainerQuery& other)
    : media_queries_(other.media_queries_->Copy()) {}

String ContainerQuery::ToString() const {
  return media_queries_->MediaText();
}

}  // namespace blink
