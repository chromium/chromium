// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "net/base/apple/url_conversions.h"

#import <Foundation/Foundation.h>

#include "base/strings/escape.h"
#include "net/base/features.h"
#include "url/gurl.h"
#include "url/url_canon.h"

namespace net {

NSURL* NSURLWithGURL(const GURL& url) {
  if (!url.is_valid()) {
    return nil;
  }

  // NSURL strictly enforces RFC 1738 which requires that certain characters
  // are always encoded. These characters are: "<", ">", """, "#", "%", "{",
  // "}", "|", "\", "^", "~", "[", "]", and "`".
  //
  // GURL leaves some of these characters unencoded in the path, query, and
  // ref. This function manually encodes those components, and then passes the
  // result to NSURL.
  GURL::Replacements replacements;
  std::string escaped_path = base::EscapeNSURLPrecursor(url.GetPath());
  std::string escaped_query = base::EscapeNSURLPrecursor(url.GetQuery());
  std::string escaped_ref = base::EscapeNSURLPrecursor(url.GetRef());
  if (!escaped_path.empty()) {
    replacements.SetPathStr(escaped_path);
  }
  if (!escaped_query.empty()) {
    replacements.SetQueryStr(escaped_query);
  }
  if (!escaped_ref.empty()) {
    replacements.SetRefStr(escaped_ref);
  }
  GURL escaped_url = url.ReplaceComponents(replacements);

  NSString* escaped_url_string =
      [NSString stringWithUTF8String:escaped_url.spec().c_str()];
  return [NSURL URLWithString:escaped_url_string];
}

GURL GURLWithNSURL(NSURL* url) {
  if (!url) {
    return GURL();
  }

  // Foundation sometimes encodes the URL in absoluteString (and all the
  // NSURL accessors other than dataRepresentation), which is not what we want
  // to pass to GURL. For example, 'about:blank#hash' becomes
  // 'about:blank%23hash' in absoluteString, but remains 'about:blank#hash' in
  // dataRepresentation. Narrowly scope this workaround to the "about:" scheme
  // to avoid encoding issues.
  if (base::FeatureList::IsEnabled(features::kUseNSURLDataForGURLConversion)) {
    NSString* scheme = url.scheme;
    if (scheme && [scheme caseInsensitiveCompare:@"about"] == NSOrderedSame) {
      NSData* data = [url dataRepresentation];
      if (data && data.length > 0) {
        return GURL(std::string_view(reinterpret_cast<const char*>(data.bytes),
                                     data.length));
      }
    }
  }

  return GURL(url.absoluteString.UTF8String);
}

}  // namespace net
