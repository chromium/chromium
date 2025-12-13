// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/webui/ui_bundled/web_ui_test_utils.h"

#import "base/strings/strcat.h"
#import "base/strings/string_util.h"
#import "base/strings/stringprintf.h"
#import "ios/components/webui/web_ui_url_constants.h"
#import "url/gurl.h"

using base::TrimPositions;

// Returns the url to the web ui page `host`. `url::SchemeHostPort` can not be
// used when this test is run using EarlGrey2 because the chrome scheme is not
// registered in the test process and `url::SchemeHostPort` will not build an
// invalid URL.
GURL WebUIPageUrlWithHost(std::string_view host) {
  // Make sure the host doesn't have a trailing slash, as it is added
  // explicitly.
  const std::string trimmed_host(
      base::TrimString(host, "/", TrimPositions::TRIM_TRAILING));
  return GURL(base::StrCat({kChromeUIScheme, "://", host, "/"}));
}
