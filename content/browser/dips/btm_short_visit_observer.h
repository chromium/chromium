// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DIPS_BTM_SHORT_VISIT_OBSERVER_H_
#define CONTENT_BROWSER_DIPS_BTM_SHORT_VISIT_OBSERVER_H_

#include <optional>
#include <string>

#include "base/memory/raw_ref.h"
#include "base/time/time.h"
#include "content/common/content_export.h"
#include "content/public/browser/web_contents_observer.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

namespace base {
class Clock;
}

namespace content {

// Emits UKM metrics for pages visited only for a short time.
class CONTENT_EXPORT BtmShortVisitObserver
    : public content::WebContentsObserver {
 public:
  explicit BtmShortVisitObserver(content::WebContents* web_contents);
  ~BtmShortVisitObserver() override;

  static const base::Clock* SetDefaultClockForTesting(const base::Clock* clock);

  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

 private:
  // For determining the length of page visits; can be overridden in tests.
  const raw_ref<const base::Clock> clock_;
  // The eTLD+1 of the page visited before the currently-committed page.
  std::optional<std::string> prev_site_;
  // The time the current page committed.
  base::Time last_committed_at_;
  // The source ID of the current page -- used in DidFinishNavigation() to emit
  // an event for the previous page.
  ukm::SourceId page_source_id_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_DIPS_BTM_SHORT_VISIT_OBSERVER_H_
