// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/prefetch_handle.h"

#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_features.h"

namespace content {

// static
std::unique_ptr<CrossThreadPrefetchHandle> CrossThreadPrefetchHandle::Create(
    std::unique_ptr<content::PrefetchHandle> prefetch_handle) {
  if (!prefetch_handle) {
    return nullptr;
  }
  return base::WrapUnique(
      new CrossThreadPrefetchHandle(std::move(prefetch_handle)));
}

CrossThreadPrefetchHandle::CrossThreadPrefetchHandle(
    std::unique_ptr<content::PrefetchHandle> prefetch_handle)
    : prefetch_handle_(std::move(prefetch_handle)) {
  CHECK(prefetch_handle_);
}

CrossThreadPrefetchHandle::CrossThreadPrefetchHandle(
    CrossThreadPrefetchHandle&& other) = default;

CrossThreadPrefetchHandle& CrossThreadPrefetchHandle::operator=(
    CrossThreadPrefetchHandle&& other) {
  Reset();
  prefetch_handle_ = std::move(other.prefetch_handle_);
  return *this;
}

CrossThreadPrefetchHandle::~CrossThreadPrefetchHandle() {
  Reset();
}

void CrossThreadPrefetchHandle::Reset() {
  if (prefetch_handle_) {
    // Delete the handle on the UI thread since it may touch/destroy
    // `PrefetchContainer`.
    if (!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI)) {
      CHECK(base::FeatureList::IsEnabled(features::kPrefetchOffTheMainThread));
      content::GetUIThreadTaskRunner({})->DeleteSoon(
          FROM_HERE, std::move(prefetch_handle_));
    } else {
      // Synchronously destroy the handle if on the UI thread.
      prefetch_handle_.reset();
    }
  }
  CHECK(!prefetch_handle_);
}

}  // namespace content
