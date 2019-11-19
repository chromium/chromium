// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/web/web_content_holder.h"

#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"

namespace blink {

WebContentHolder::WebContentHolder(const WebContentHolder& other) {
  private_ = other.private_;
}

WebContentHolder& WebContentHolder::operator=(const WebContentHolder& other) {
  private_ = other.private_;
  return *this;
}

WebContentHolder::~WebContentHolder() {
  private_.Reset();
}

WebString WebContentHolder::GetValue() const {
  return private_->nodeValue();
}

WebRect WebContentHolder::GetBoundingBox() const {
  if (auto* layout_obj = private_->GetLayoutObject())
    return EnclosingIntRect(layout_obj->VisualRectInDocument());
  return IntRect();
}

uint64_t WebContentHolder::GetId() const {
  return reinterpret_cast<uint64_t>(private_.Get());
}

WebContentHolder::WebContentHolder(Node& node) : private_(&node) {}

}  // namespace blink
