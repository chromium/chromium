// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/test/source_observer.h"

#include <stddef.h>

#include "base/run_loop.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/test/test_navigation_observer.h"
#include "third_party/blink/public/common/navigation/impression.h"

namespace content {

SourceObserver::SourceObserver(WebContents* contents, size_t num_impressions)
    : TestNavigationObserver(contents),
      expected_num_impressions_(num_impressions) {}

SourceObserver::~SourceObserver() = default;

void SourceObserver::OnDidFinishNavigation(
    NavigationHandle* navigation_handle) {
  if (!navigation_handle->GetImpression()) {
    if (waiting_for_null_impression_) {
      impression_loop_.Quit();
    }
    return;
  }

  last_impression_ = *(navigation_handle->GetImpression());
  num_impressions_++;

  if (!waiting_for_null_impression_ &&
      num_impressions_ >= expected_num_impressions_) {
    impression_loop_.Quit();
  }
}

// Waits for |expected_num_impressions_| navigations with impressions, and
// returns the last impression.
const blink::Impression& SourceObserver::Wait() {
  if (num_impressions_ >= expected_num_impressions_) {
    return *last_impression_;
  }
  impression_loop_.Run();
  return last_impression();
}

bool SourceObserver::WaitForNavigationWithNoImpression() {
  waiting_for_null_impression_ = true;
  impression_loop_.Run();
  waiting_for_null_impression_ = false;
  return true;
}

}  // namespace content
