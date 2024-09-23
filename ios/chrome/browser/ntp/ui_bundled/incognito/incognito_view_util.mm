// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ntp/ui_bundled/incognito/incognito_view_util.h"

#import "components/google/core/common/google_util.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"

namespace {

// The URL for the the Learn More page shown on incognito new tab.
// Taken from ntp_resource_cache.cc.
const char kLearnMoreIncognitoUrl[] =
    "https://support.google.com/chrome/?p=incognito";

GURL GetUrlWithLang(const GURL& url) {
  std::string locale = GetApplicationContext()->GetApplicationLocale();
  return google_util::AppendGoogleLocaleParam(url, locale);
}

}  // namespace

GURL GetLearnMoreIncognitoUrl() {
  return GetUrlWithLang(GURL(kLearnMoreIncognitoUrl));
}
