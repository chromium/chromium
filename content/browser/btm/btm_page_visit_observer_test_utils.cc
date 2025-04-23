// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/btm/btm_page_visit_observer_test_utils.h"

#include <iostream>

#include "content/browser/btm/btm_page_visit_observer.h"

namespace content {

std::ostream& operator<<(std::ostream& out, const BtmPageVisitInfo& page) {
  return out << "BtmPageVisitInfo{url=" << page.url
             << ", source_id=" << page.source_id
             << ", had_active_storage_access=" << page.had_active_storage_access
             << ", received_user_activation=" << page.received_user_activation
             << ", had_successful_webauthn_assertion="
             << page.had_successful_web_authn_assertion
             << ", visit_duration=" << page.visit_duration << "}";
}

std::ostream& operator<<(std::ostream& out,
                         const BtmServerRedirectInfo& redirect) {
  return out << "BtmServerRedirectInfo{url=" << redirect.url
             << ", source_id=" << redirect.source_id
             << ", did_write_cookies=" << redirect.did_write_cookies << "}";
}

std::ostream& operator<<(std::ostream& out, const BtmNavigationInfo& nav) {
  out << "BtmNavigationInfo{server_redirects=[";
  for (size_t i = 0; i < nav.server_redirects.size(); ++i) {
    if (i > 0) {
      out << ", ";
    }
    out << nav.server_redirects[i];
  }
  out << "], was_user_initiated=" << nav.was_user_initiated
      << ", was_renderer_initiated=" << nav.was_renderer_initiated
      << ", page_transition="
      << ui::PageTransitionGetCoreTransitionString(nav.page_transition) << "}";
  return out;
}

std::ostream& operator<<(std::ostream& out,
                         const BtmPageVisitObserver::VisitTuple& visit) {
  return out << "VisitTuple{prev_page=" << visit.prev_page
             << ", navigation=" << visit.navigation << "}";
}

BtmPageVisitRecorder::BtmPageVisitRecorder(WebContents* web_contents,
                                           base::Clock* clock)
    : observer_(web_contents,
                base::BindRepeating(&BtmPageVisitRecorder::OnVisit,
                                    base::Unretained(this)),
                clock) {}

BtmPageVisitRecorder::~BtmPageVisitRecorder() = default;

[[nodiscard]] bool BtmPageVisitRecorder::WaitForSize(size_t n) {
  if (visits_.size() >= n) {
    return true;
  }

  if (wait_state_.has_value()) {
    NOTREACHED()
        << "PageVisitWaiter::WaitForCount() called when already waiting";
  }

  wait_state_.emplace(n);
  wait_state_->run_loop.Run();
  wait_state_.reset();
  return visits_.size() >= n;
}

void BtmPageVisitRecorder::OnVisit(BtmPageVisitInfo prev_page,
                                   BtmNavigationInfo navigation) {
  visits_.emplace_back(std::move(prev_page), std::move(navigation));

  if (wait_state_.has_value() && wait_state_->wanted_count <= visits_.size()) {
    wait_state_->run_loop.Quit();
  }
}

}  // namespace content
