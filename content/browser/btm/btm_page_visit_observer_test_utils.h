// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BTM_BTM_PAGE_VISIT_OBSERVER_TEST_UTILS_H_
#define CONTENT_BROWSER_BTM_BTM_PAGE_VISIT_OBSERVER_TEST_UTILS_H_

#include <iosfwd>
#include <optional>
#include <vector>

#include "base/run_loop.h"
#include "content/browser/btm/btm_page_visit_observer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"

namespace content {

std::ostream& operator<<(std::ostream& out, const BtmPageVisitInfo& page);
std::ostream& operator<<(std::ostream& out,
                         const BtmServerRedirectInfo& redirect);
std::ostream& operator<<(std::ostream& out, const BtmNavigationInfo& nav);
std::ostream& operator<<(std::ostream& out,
                         const BtmPageVisitObserver::VisitTuple& visit);

class BtmPageVisitRecorder {
 public:
  using VisitTuple = BtmPageVisitObserver::VisitTuple;

  explicit BtmPageVisitRecorder(
      WebContents* web_contents,
      base::Clock* clock = base::DefaultClock::GetInstance());
  ~BtmPageVisitRecorder();

  // Returns all visits observed so far.
  const std::vector<VisitTuple>& visits() const { return visits_; }

  // Wait until `visits()` returns at least `n` elements. Returns `true` if
  // successful, or `false` if it times out.
  [[nodiscard]] bool WaitForSize(size_t n);

 private:
  // The state needed to implement `WaitForSize()`.
  struct WaitState {
    explicit WaitState(size_t wanted_count) : wanted_count(wanted_count) {}

    const size_t wanted_count;
    base::RunLoop run_loop;
  };

  // Called by `observer_` on each page visit; appends to `visits_`.
  void OnVisit(BtmPageVisitInfo prev_page, BtmNavigationInfo navigation);

  std::vector<VisitTuple> visits_;
  // Populated only during a call to `WaitForSize()`. Mostly a `RunLoop` wrapped
  // in `optional<>` to allow for re-creation.
  std::optional<WaitState> wait_state_;
  BtmPageVisitObserver observer_;
};

// Matches the `url` property of `BtmPageVisitInfo`,
// `BtmNavigationInfo::destination`, or `BtmServerRedirectInfo`.
MATCHER_P(HasUrl,
          matcher,
          "has url that " + testing::DescribeMatcher<GURL>(matcher, negation)) {
  return testing::ExplainMatchResult(matcher, arg.url, result_listener);
}

// Matches the `source_id` property of `BtmPageVisitInfo` or
// `BtmServerRedirectInfo`.
MATCHER_P(HasSourceId,
          matcher,
          "has source_id that " +
              testing::DescribeMatcher<ukm::SourceId>(matcher, negation)) {
  return testing::ExplainMatchResult(matcher, arg.source_id, result_listener);
}

MATCHER(HasUrlAndSourceIdForBlankPage,
        "has url and source_id for a blank page (url=GURL(), "
        "source_id=ukm::kInvalidSourceId)") {
  bool matched = true;
  if (arg.url != GURL()) {
    matched = false;
    *result_listener << "url was " << arg.url;
  }
  if (arg.source_id != ukm::kInvalidSourceId) {
    if (!matched) {
      *result_listener << " and ";
    }
    matched = false;
    *result_listener << "source_id was " << arg.source_id;
  }
  return matched;
}

MATCHER_P2(HasUrlAndMatchingSourceId,
           matcher,
           ukm_recorder_ptr,
           "has url that " + testing::DescribeMatcher<GURL>(matcher, negation) +
               " and source_id with a corresponding source URL that also " +
               testing::DescribeMatcher<GURL>(matcher, negation)) {
  bool matched = true;
  if (!testing::ExplainMatchResult(matcher, arg.url, result_listener)) {
    *result_listener << "url was " << arg.url;
    matched = false;
  }
  const GURL url_for_source_id =
      ukm_recorder_ptr->GetSourceForSourceId(arg.source_id)->url();
  if (!testing::ExplainMatchResult(matcher, url_for_source_id,
                                   result_listener)) {
    if (!matched) {
      *result_listener << " and ";
    }
    *result_listener << "source_id had corresponding URL " << url_for_source_id;
    matched = false;
  }
  return matched;
}

// Matches the URL for the `source_id` property of `BtmPageVisitInfo` or
// `BtmServerRedirectInfo`, as recorded by `ukm_recorder`.
MATCHER_P2(HasSourceIdForUrl,
           matcher,
           ukm_recorder,
           "has source_id with a corresponding URL that " +
               testing::DescribeMatcher<GURL>(matcher, negation)) {
  const GURL url_for_source_id =
      ukm_recorder->GetSourceForSourceId(arg.source_id)->url();
  if (!testing::ExplainMatchResult(matcher, url_for_source_id,
                                   result_listener)) {
    *result_listener << "source_id had corresponding URL " << url_for_source_id;
    return false;
  }
  return true;
}

MATCHER_P2(SourceIdUrlIs,
           matcher,
           ukm_recorder_ptr,
           "has a corresponding URL that " +
               testing::DescribeMatcher<GURL>(matcher, negation)) {
  const GURL url_for_source_id =
      ukm_recorder_ptr->GetSourceForSourceId(arg)->url();
  if (!testing::ExplainMatchResult(matcher, url_for_source_id,
                                   result_listener)) {
    *result_listener << "corresponding URL was " << url_for_source_id;
    return false;
  }
  return true;
}

// Matches `VisitTuple::prev_page`.
MATCHER_P(PreviousPage,
          matcher,
          "has prev_page that " +
              testing::DescribeMatcher<BtmPageVisitInfo>(matcher, negation)) {
  return testing::ExplainMatchResult(matcher, arg.prev_page, result_listener);
}

// Matches `VisitTuple::navigation`.
MATCHER_P(Navigation,
          matcher,
          "has navigation that " +
              testing::DescribeMatcher<BtmNavigationInfo>(matcher, negation)) {
  return testing::ExplainMatchResult(matcher, arg.navigation, result_listener);
}

// Matches `BtmNavigationInfo::server_redirects`.
MATCHER_P(ServerRedirects,
          matcher,
          "has server_redirects that " +
              testing::DescribeMatcher<std::vector<BtmServerRedirectInfo>>(
                  matcher,
                  negation)) {
  return testing::ExplainMatchResult(matcher, arg.server_redirects,
                                     result_listener);
}

// Matches `BtmNavigationInfo::was_user_initiated`.
MATCHER_P(WasUserInitiated,
          matcher,
          "has was_user_initiated that " +
              testing::DescribeMatcher<bool>(matcher, negation)) {
  return testing::ExplainMatchResult(matcher, arg.was_user_initiated,
                                     result_listener);
}

// Matches `BtmNavigationInfo::was_renderer_initiated`.
MATCHER_P(WasRendererInitiated,
          matcher,
          "has was_renderer_initiated that " +
              testing::DescribeMatcher<bool>(matcher, negation)) {
  return testing::ExplainMatchResult(matcher, arg.was_renderer_initiated,
                                     result_listener);
}

// Matches `BtmNavigationInfo::page_transition`. Param must be a
// `ui::PageTransition`.
MATCHER_P(PageTransitionCoreTypeIs,
          matcher,
          "has page_transition core type that " +
              std::string(negation ? "is " : "isn't ") +
              ui::PageTransitionGetCoreTransitionString(matcher)) {
  return testing::ExplainMatchResult(
      ui::PageTransitionGetCoreTransitionString(matcher),
      ui::PageTransitionGetCoreTransitionString(arg.page_transition),
      result_listener);
}

// Matches a `ui::PageTransition`. Param must also be a `ui::PageTransition`.
MATCHER_P(CoreTypeIs,
          matcher,
          "has core type that " + std::string(negation ? "is " : "isn't ") +
              ui::PageTransitionGetCoreTransitionString(matcher)) {
  return testing::ExplainMatchResult(
      ui::PageTransitionGetCoreTransitionString(matcher),
      ui::PageTransitionGetCoreTransitionString(arg), result_listener);
}

// Matches `BtmPageVisitInfo::had_qualifying_storage_access`.
MATCHER_P(HadQualifyingStorageAccess,
          matcher,
          "has had_qualifying_storage_access that " +
              testing::DescribeMatcher<bool>(matcher, negation)) {
  return testing::ExplainMatchResult(matcher, arg.had_qualifying_storage_access,
                                     result_listener);
}

// Matches `BtmServerRedirectInfo::did_write_cookies`.
MATCHER_P(DidWriteCookies,
          matcher,
          "has did_write_cookies that " +
              testing::DescribeMatcher<bool>(matcher, negation)) {
  return testing::ExplainMatchResult(matcher, arg.did_write_cookies,
                                     result_listener);
}

MATCHER_P(ReceivedUserActivation,
          matcher,
          "has received_user_activation that " +
              testing::DescribeMatcher<bool>(matcher, negation)) {
  return testing::ExplainMatchResult(matcher, arg.received_user_activation,
                                     result_listener);
}

MATCHER_P(HadSuccessfulWebAuthnAssertion,
          matcher,
          "has had_successful_web_authn_assertion that " +
              testing::DescribeMatcher<bool>(matcher, negation)) {
  return testing::ExplainMatchResult(
      matcher, arg.had_successful_web_authn_assertion, result_listener);
}

MATCHER_P(VisitDuration,
          matcher,
          "has visit_duration that " +
              testing::DescribeMatcher<base::TimeDelta>(matcher, negation)) {
  return testing::ExplainMatchResult(matcher, arg.visit_duration,
                                     result_listener);
}

}  // namespace content

#endif  // CONTENT_BROWSER_BTM_BTM_PAGE_VISIT_OBSERVER_TEST_UTILS_H_
