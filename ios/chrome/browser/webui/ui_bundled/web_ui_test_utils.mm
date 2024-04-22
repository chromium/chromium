// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/webui/ui_bundled/web_ui_test_utils.h"

#import "base/strings/string_util.h"
#import "base/strings/stringprintf.h"
#import "base/test/ios/wait_util.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/components/webui/web_ui_url_constants.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "url/gurl.h"

using base::TrimPositions;
using chrome_test_util::OmniboxContainingText;
using chrome_test_util::OmniboxText;

// Returns the url to the web ui page `host`. `url::SchemeHostPort` can not be
// used when this test is run using EarlGrey2 because the chrome scheme is not
// registered in the test process and `url::SchemeHostPort` will not build an
// invalid URL.
GURL WebUIPageUrlWithHost(const std::string& host) {
  return GURL(base::StringPrintf("%s://%s", kChromeUIScheme, host.c_str()));
}

// Waits for omnibox text to equal (if `exact_match`) or contain (else) `URL`
// and returns true if it was found or false on timeout. Strips trailing URL
// slash if present as the omnibox does not display them.
bool WaitForOmniboxURLString(const std::string& url, bool exact_match) {
  const std::string trimmed_URL(
      base::TrimString(url, "/", TrimPositions::TRIM_TRAILING));

  // TODO(crbug.com/41272687): Unify with the omniboxText matcher or move to the
  // same location with the omniboxText matcher.
  return base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^{
        NSError* error = nil;
        if (exact_match) {
          [[EarlGrey selectElementWithMatcher:OmniboxText(trimmed_URL)]
              assertWithMatcher:grey_notNil()
                          error:&error];
        } else {
          [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
              assertWithMatcher:OmniboxContainingText(trimmed_URL)
                          error:&error];
        }
        return error == nil;
      });
}
