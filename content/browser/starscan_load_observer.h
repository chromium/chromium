// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_STARSCAN_LOAD_OBSERVER_H_
#define CONTENT_BROWSER_STARSCAN_LOAD_OBSERVER_H_

#include "base/timer/timer.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {

// Observes the loading stage of each WebContents and disables *Scan while there
// is at least one WebContents that is being loaded. The approach still
// preserves fallbacks that reenable PCScan:
//  - the hard limit of 50% quarantine is reached (see in pcscan_scheduling.h);
//  - 10 seconds timer (if there are slow loads).
// TODO(bikineev,1129751): Investigate if a clearer signal to disable *Scan can
// be used instead of WebContentsObserver (e.g. if there is a pending
// USER_BLOCKING task).
// TODO(1231679): Remove/reevaluate the approach.
class StarScanLoadObserver final : public WebContentsObserver {
 public:
  explicit StarScanLoadObserver(WebContents* contents);

  StarScanLoadObserver(const StarScanLoadObserver&) = delete;
  StarScanLoadObserver& operator=(const StarScanLoadObserver&) = delete;

  ~StarScanLoadObserver() override;

 private:
  // Disable *Scan when any frame is ready to commit (i.e., has received the
  // network response for a navigation) until it finishes loading.
  void ReadyToCommitNavigation(NavigationHandle* navigation_handle) final;
  void DidStopLoading() final;

  void DecrementCounterAndReenableStarScanIfNeeded();

  // The current WebContents can be destructed while loading is in progress.
  // Keep track of the state with a per WebContents variable.
  bool is_loading_ = false;
  // Timer is used as a fallback in case loading is too slow.
  base::OneShotTimer timer_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_STARSCAN_LOAD_OBSERVER_H_
