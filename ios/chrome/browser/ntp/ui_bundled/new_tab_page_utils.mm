// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_utils.h"

#import "components/google/core/common/google_util.h"
#import "ios/chrome/browser/first_run/ui_bundled/first_run_util.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_feature.h"
#import "net/base/url_util.h"
#import "url/url_util.h"

bool ShouldShowTopOfFeedSyncPromo() {
  // Checks the flag and ensures that the user is not in first run.
  return IsDiscoverFeedTopSyncPromoEnabled() &&
         !ShouldPresentFirstRunExperience();
}

GURL GetURLForMIA() {
  GURL result_url = GURL(google_util::kGoogleHomepageURL);
  result_url = google_util::GetGoogleSearchURL(result_url);
  result_url = net::AppendOrReplaceQueryParameter(result_url, "aep", "47");
  result_url =
      net::AppendOrReplaceQueryParameter(result_url, "sourceid", "chrome");
  result_url = net::AppendOrReplaceQueryParameter(result_url, "udm", "50");

  return result_url;
}
