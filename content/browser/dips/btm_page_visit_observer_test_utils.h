// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DIPS_BTM_PAGE_VISIT_OBSERVER_TEST_UTILS_H_
#define CONTENT_BROWSER_DIPS_BTM_PAGE_VISIT_OBSERVER_TEST_UTILS_H_

#include <iosfwd>
#include <vector>

#include "content/browser/dips/btm_page_visit_observer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"

namespace content {

struct VisitTuple {
  BtmPageVisitInfo prev_page;
  BtmNavigationInfo navigation;
  GURL url;
};

std::ostream& operator<<(std::ostream& out, const BtmPageVisitInfo& page);
std::ostream& operator<<(std::ostream& out,
                         const BtmServerRedirectInfo& redirect);
std::ostream& operator<<(std::ostream& out, const BtmNavigationInfo& nav);
std::ostream& operator<<(std::ostream& out, const VisitTuple& visit);

class BtmPageVisitRecorder {
 public:
  BtmPageVisitRecorder(WebContents* web_contents);
  ~BtmPageVisitRecorder();

  const std::vector<VisitTuple>& visits() const { return visits_; }

 private:
  void OnVisit(const BtmPageVisitInfo& prev_page,
               const BtmNavigationInfo& navigation,
               const GURL& url);

  std::vector<VisitTuple> visits_;
  BtmPageVisitObserver observer_;
};

MATCHER_P(HasUrl,
          matcher,
          "has url that " + testing::DescribeMatcher<GURL>(matcher, negation)) {
  return testing::ExplainMatchResult(matcher, arg.url, result_listener);
}

MATCHER_P(PreviousPage,
          matcher,
          "has prev_page that " +
              testing::DescribeMatcher<BtmPageVisitInfo>(matcher, negation)) {
  return testing::ExplainMatchResult(matcher, arg.prev_page, result_listener);
}

MATCHER_P(Navigation,
          matcher,
          "has navigation that " +
              testing::DescribeMatcher<BtmNavigationInfo>(matcher, negation)) {
  return testing::ExplainMatchResult(matcher, arg.navigation, result_listener);
}

MATCHER_P(ServerRedirects,
          matcher,
          "has server_redirects that " +
              testing::DescribeMatcher<std::vector<BtmServerRedirectInfo>>(
                  matcher,
                  negation)) {
  return testing::ExplainMatchResult(matcher, arg.server_redirects,
                                     result_listener);
}

}  // namespace content

#endif  // CONTENT_BROWSER_DIPS_BTM_PAGE_VISIT_OBSERVER_TEST_UTILS_H_
