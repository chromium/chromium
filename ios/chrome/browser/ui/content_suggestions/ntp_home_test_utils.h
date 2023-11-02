// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_NTP_HOME_TEST_UTILS_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_NTP_HOME_TEST_UTILS_H_

#import <UIKit/UIKit.h>

#include "components/ntp_snippets/callbacks.h"
#include "url/gurl.h"

@protocol GREYMatcher;

namespace ntp_home {
// Returns the primary collection view of the new tab page. Returns nil if it is
// not in the view hierarchy.
UICollectionView* CollectionView();

// Returns the collection view of the content suggestions. Returns nil if it is
// not in the view hierarchy.
UICollectionView* ContentSuggestionsCollectionView();

// Returns the view corresponding to the fake omnibox. Returns nil if it is not
// in the view hierarchy.
UIView* FakeOmnibox();

// Returns the label corresponding to the Discover header label. Returns nil if
// it is not in the view hierarchy.
UILabel* DiscoverHeaderLabel();
}  // namespace ntp_home

namespace ntp_snippets {

// Helper to return additional suggestions with a defined url when the "fetch
// more" action is done.
class AdditionalSuggestionsHelper {
 public:
  AdditionalSuggestionsHelper(const GURL& suggestions_url);

  // Calls the `callback` with 10 suggestions, with their url set to `url_`.
  void SendAdditionalSuggestions(FetchDoneCallback* callback);

 private:
  GURL url_;
};

}  // namespace ntp_snippets

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_NTP_HOME_TEST_UTILS_H_
