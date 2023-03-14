// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BROWSING_TOPICS_TEST_UTIL_H_
#define CONTENT_BROWSER_BROWSING_TOPICS_TEST_UTIL_H_

#include "content/public/test/test_navigation_observer.h"

namespace content {

// Retrieve the browsing topics iframe attribute status during a navigation.
class IframeBrowsingTopicsAttributeWatcher
    : public content::TestNavigationObserver {
 public:
  using content::TestNavigationObserver::TestNavigationObserver;

  ~IframeBrowsingTopicsAttributeWatcher() override;

  void OnDidStartNavigation(NavigationHandle* navigation_handle) override;

  bool last_navigation_has_iframe_browsing_topics_attribute() const {
    return last_navigation_has_iframe_browsing_topics_attribute_;
  }

 private:
  bool last_navigation_has_iframe_browsing_topics_attribute_ = false;
};

}  // namespace content

#endif  // CONTENT_BROWSER_BROWSING_TOPICS_TEST_UTIL_H_
