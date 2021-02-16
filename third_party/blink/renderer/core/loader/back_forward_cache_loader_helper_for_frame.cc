// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/back_forward_cache_loader_helper_for_frame.h"

#include "third_party/blink/renderer/core/frame/local_frame.h"

namespace blink {

void BackForwardCacheLoaderHelperForFrame::EvictFromBackForwardCache(
    mojom::blink::RendererEvictionReason reason) {
  if (!frame_)
    return;
  frame_->EvictFromBackForwardCache(reason);
}

void BackForwardCacheLoaderHelperForFrame::DidBufferLoadWhileInBackForwardCache(
    size_t num_bytes) {
  if (!frame_)
    return;
  frame_->DidBufferLoadWhileInBackForwardCache(num_bytes);
}

bool BackForwardCacheLoaderHelperForFrame::
    CanContinueBufferingWhileInBackForwardCache() const {
  if (!frame_)
    return false;
  return frame_->CanContinueBufferingWhileInBackForwardCache();
}

void BackForwardCacheLoaderHelperForFrame::Detach() {
  frame_ = nullptr;
}

void BackForwardCacheLoaderHelperForFrame::Trace(Visitor* visitor) const {
  visitor->Trace(frame_);
  BackForwardCacheLoaderHelper::Trace(visitor);
}

}  // namespace blink
