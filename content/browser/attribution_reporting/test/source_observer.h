// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_TEST_SOURCE_OBSERVER_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_TEST_SOURCE_OBSERVER_H_

#include <stddef.h>

#include <optional>

#include "base/run_loop.h"
#include "content/public/test/test_navigation_observer.h"
#include "third_party/blink/public/common/navigation/impression.h"

namespace content {

class WebContents;

// WebContentsObserver that waits until a source is available on a
// navigation handle for a finished navigation.
class SourceObserver : public TestNavigationObserver {
 public:
  explicit SourceObserver(WebContents* contents, size_t num_impressions = 1u);
  ~SourceObserver() override;

  // WebContentsObserver:
  void OnDidFinishNavigation(NavigationHandle*) override;

  const blink::Impression& last_impression() const { return *last_impression_; }

  // Waits for |expected_num_impressions_| navigations with impressions, and
  // returns the last impression.
  const blink::Impression& Wait();

  bool WaitForNavigationWithNoImpression();

 private:
  size_t num_impressions_ = 0u;
  const size_t expected_num_impressions_ = 0u;
  std::optional<blink::Impression> last_impression_;
  bool waiting_for_null_impression_ = false;
  base::RunLoop impression_loop_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_TEST_SOURCE_OBSERVER_H_
