// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/dips/btm_page_visit_observer_test_utils.h"

#include <iostream>

#include "content/browser/dips/btm_page_visit_observer.h"

namespace content {

std::ostream& operator<<(std::ostream& out, const BtmPageVisitInfo& page) {
  return out << "BtmPageVisitInfo{url=" << page.url << "}";
}

std::ostream& operator<<(std::ostream& out,
                         const BtmServerRedirectInfo& redirect) {
  return out << "BtmServerRedirectInfo{url=" << redirect.url << "}";
}

std::ostream& operator<<(std::ostream& out, const BtmNavigationInfo& nav) {
  out << "BtmNavigationInfo{server_redirects=[";
  for (size_t i = 0; i < nav.server_redirects.size(); ++i) {
    if (i > 0) {
      out << ", ";
    }
    out << nav.server_redirects[i];
  }
  return out << "]}";
}

std::ostream& operator<<(std::ostream& out, const VisitTuple& visit) {
  return out << "VisitTuple{prev_page=" << visit.prev_page
             << ", navigation=" << visit.navigation << ", url=" << visit.url
             << "}";
}

BtmPageVisitRecorder::BtmPageVisitRecorder(WebContents* web_contents)
    : observer_(web_contents,
                base::BindRepeating(&BtmPageVisitRecorder::OnVisit,
                                    base::Unretained(this))) {}

BtmPageVisitRecorder::~BtmPageVisitRecorder() = default;

void BtmPageVisitRecorder::OnVisit(const BtmPageVisitInfo& prev_page,
                                   const BtmNavigationInfo& navigation,
                                   const GURL& url) {
  visits_.emplace_back(prev_page, navigation, url);
}

}  // namespace content
