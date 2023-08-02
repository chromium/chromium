// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/common/url_scheme_util.h"

#import <Foundation/Foundation.h>

#import "base/strings/sys_string_conversions.h"
#import "url/gurl.h"

namespace web {

bool UrlHasWebScheme(const GURL& url) {
  return url.SchemeIs(url::kHttpScheme) || url.SchemeIs(url::kHttpsScheme) ||
         url.SchemeIs(url::kDataScheme);
}

bool UrlHasWebScheme(NSURL* url) {
  NSString* scheme = [url scheme];
  if (![scheme length])
    return false;

  // Use the GURL implementation, but with a scheme-only URL to avoid
  // unnecessary parsing in GURL construction.
  NSString* schemeURLString = [scheme stringByAppendingString:@":"];
  GURL gurl(base::SysNSStringToUTF8(schemeURLString));
  return UrlHasWebScheme(gurl);
}

}  // namespace web
