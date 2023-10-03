// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/starscan_load_observer.h"

#include "base/allocator/partition_allocator/src/partition_alloc/starscan/pcscan.h"
#include "base/logging.h"
#include "content/browser/renderer_host/frame_tree.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"

namespace content {

namespace {
size_t g_loading_webcontents_ = 0;
}

// Start observing right away.
StarScanLoadObserver::StarScanLoadObserver(WebContents* contents)
    : WebContentsObserver(contents) {}

StarScanLoadObserver::~StarScanLoadObserver() {
  // WebContents can be destructed while loading is still in progress.
  DidStopLoading();
}

void StarScanLoadObserver::ReadyToCommitNavigation(
    NavigationHandle* navigation_handle) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // We don't disable PCScan for a prerendering page's navigation since
  // it doesn't invoke DidStopLoading.
  if (NavigationRequest::From(navigation_handle)
          ->frame_tree_node()
          ->frame_tree()
          .is_prerendering()) {
    return;
  }

  // Protect against ReadyToCommitNavigation() being called twice in a row.
  if (is_loading_)
    return;
  is_loading_ = true;

  if (!g_loading_webcontents_) {
    VLOG(3) << "Disabling *Scan due to loading";
    partition_alloc::internal::PCScan::Disable();
  }
  ++g_loading_webcontents_;

  // Set timer as a fallback if loading is too slow.
  constexpr base::TimeDelta kReenableStarScanDelta = base::Seconds(10);
  timer_.Start(FROM_HERE, kReenableStarScanDelta, this,
               &StarScanLoadObserver::DidStopLoading);
}

void StarScanLoadObserver::DidStopLoading() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (is_loading_) {
    DecrementCounterAndReenableStarScanIfNeeded();
    is_loading_ = false;
  }
}

void StarScanLoadObserver::DecrementCounterAndReenableStarScanIfNeeded() {
  CHECK(g_loading_webcontents_);
  --g_loading_webcontents_;
  if (!g_loading_webcontents_) {
    VLOG(3) << "Reenabling *Scan after finishing loading";
    partition_alloc::internal::PCScan::Reenable();
  }
}

}  // namespace content
