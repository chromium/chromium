// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/back_forward_cache_loader_helper_impl.h"

#include "third_party/blink/renderer/platform/bindings/source_location.h"
#include "v8/include/cppgc/visitor.h"

namespace blink {

BackForwardCacheLoaderHelperImpl::BackForwardCacheLoaderHelperImpl(
    Delegate& delegate)
    : delegate_(&delegate) {}

void BackForwardCacheLoaderHelperImpl::EvictFromBackForwardCache(
    mojom::blink::RendererEvictionReason reason) {
  if (!delegate_)
    return;
  // Pass nullptr as a source location since this method shouldn't be called
  // for JavaScript execution. We want to capture the source location only
  // when the eviction reason is JavaScript execution.
  delegate_->EvictFromBackForwardCache(reason, /*source_location=*/nullptr);
}

void BackForwardCacheLoaderHelperImpl::DidBufferLoadWhileInBackForwardCache(
    bool update_process_wide_count,
    size_t num_bytes) {
  if (!delegate_)
    return;
  delegate_->DidBufferLoadWhileInBackForwardCache(update_process_wide_count,
                                                  num_bytes);
}

void BackForwardCacheLoaderHelperImpl::Detach() {
  delegate_ = nullptr;
}

void BackForwardCacheLoaderHelperImpl::Trace(Visitor* visitor) const {
  visitor->Trace(delegate_);
  BackForwardCacheLoaderHelper::Trace(visitor);
}

}  // namespace blink
