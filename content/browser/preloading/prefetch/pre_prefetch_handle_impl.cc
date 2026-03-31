// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prefetch/pre_prefetch_handle_impl.h"

#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_features.h"

namespace content {

PrePrefetchHandleImpl::PrePrefetchHandleImpl() {
  CHECK(base::FeatureList::IsEnabled(features::kPrefetchOffTheMainThread));
  DCHECK(!BrowserThread::CurrentlyOn(content::BrowserThread::UI));
}

PrePrefetchHandleImpl::~PrePrefetchHandleImpl() = default;

}  // namespace content
