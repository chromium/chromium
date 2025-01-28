// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prefetch/prefetch_handle_impl.h"

#include "content/browser/browser_context_impl.h"
#include "content/browser/preloading/prefetch/prefetch_service.h"
#include "content/browser/preloading/prefetch/prefetch_type.h"
#include "content/public/browser/preloading_data.h"
#include "content/public/browser/web_contents.h"

namespace content {

PrefetchHandleImpl::PrefetchHandleImpl(
    base::WeakPtr<PrefetchService> prefetch_service,
    base::WeakPtr<PrefetchContainer> prefetch_container)
    : prefetch_service_(std::move(prefetch_service)),
      prefetch_container_(std::move(prefetch_container)) {}

PrefetchHandleImpl::~PrefetchHandleImpl() {
  // Notify `PrefetchService` that the corresponding `PrefetchContainer` is no
  // longer needed. `PrefetchService` might release the container and its
  // corresponding resources by its decision with best-effort.
  if (prefetch_service_) {
    // TODO(crbug.com/390329781): Consider setting appropriate
    // PrefetchStatus/PreloadingAttempt.
    prefetch_service_->MayReleasePrefetch(prefetch_container_);
  }
}

}  // namespace content
