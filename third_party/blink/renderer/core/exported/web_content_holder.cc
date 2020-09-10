// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/web/web_content_holder.h"

#include "third_party/blink/renderer/core/content_capture/content_holder.h"
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
  return private_->node()->nodeValue();
}

WebRect WebContentHolder::GetBoundingBox() const {
  return WebRect(private_->rect());
}

uint64_t WebContentHolder::GetId() const {
  return reinterpret_cast<uint64_t>(private_->node());
}

WebContentHolder::WebContentHolder(ContentHolder& holder) : private_(&holder) {}

}  // namespace blink
