// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/content_capture/content_holder.h"

namespace blink {

ContentHolder::ContentHolder() = default;

ContentHolder::ContentHolder(Node* node, const gfx::Rect& rect)
    : node_(node), rect_(rect) {}

ContentHolder::~ContentHolder() = default;

void ContentHolder::Trace(Visitor* visitor) const {
  visitor->Trace(node_);
}

}  // namespace blink
