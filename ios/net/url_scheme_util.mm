// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/net/url_scheme_util.h"

#import <Foundation/Foundation.h>

#include "base/check.h"
#include "base/strings/sys_string_conversions.h"
#include "url/gurl.h"

namespace net {

bool UrlSchemeIs(NSURL* url, NSString* scheme) {
  DCHECK([scheme isEqualToString:[scheme lowercaseString]]);
  NSString* url_scheme = [url scheme];
  return (url_scheme != nil) &&
         [url_scheme caseInsensitiveCompare:scheme] == NSOrderedSame;
}

bool UrlHasDataScheme(NSURL* url) {
  return UrlSchemeIs(url, base::SysUTF8ToNSString(url::kDataScheme));
}

}  // namespace net
