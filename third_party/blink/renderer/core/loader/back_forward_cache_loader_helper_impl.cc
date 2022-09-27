// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/back_forward_cache_loader_helper_impl.h"

#include "v8/include/cppgc/visitor.h"

namespace blink {

BackForwardCacheLoaderHelperImpl::BackForwardCacheLoaderHelperImpl(
    Delegate& delegate)
    : delegate_(&delegate) {}

void BackForwardCacheLoaderHelperImpl::EvictFromBackForwardCache(
    mojom::blink::RendererEvictionReason reason) {
  if (!delegate_)
    return;
  delegate_->EvictFromBackForwardCache(reason);
}

void BackForwardCacheLoaderHelperImpl::DidBufferLoadWhileInBackForwardCache(
    size_t num_bytes) {
  if (!delegate_)
    return;
  delegate_->DidBufferLoadWhileInBackForwardCache(num_bytes);
}

void BackForwardCacheLoaderHelperImpl::Detach() {
  delegate_ = nullptr;
}

void BackForwardCacheLoaderHelperImpl::Trace(Visitor* visitor) const {
  visitor->Trace(delegate_);
  BackForwardCacheLoaderHelper::Trace(visitor);
}

}  // namespace blink
