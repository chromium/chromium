// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/style/reference_clip_path_operation.h"

namespace blink {

void ReferenceClipPathOperation::AddClient(SVGResourceClient& client) {
  if (resource_)
    resource_->AddClient(client);
}

void ReferenceClipPathOperation::RemoveClient(SVGResourceClient& client) {
  if (resource_)
    resource_->RemoveClient(client);
}

SVGResource* ReferenceClipPathOperation::Resource() const {
  return resource_;
}

bool ReferenceClipPathOperation::operator==(const ClipPathOperation& o) const {
  if (!IsSameType(o))
    return false;
  const ReferenceClipPathOperation& other = To<ReferenceClipPathOperation>(o);
  return resource_ == other.resource_ && url_ == other.url_;
}

}  // namespace blink
