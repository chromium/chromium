// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "net/base/apple/url_conversions.h"

#import <Foundation/Foundation.h>

#include "base/containers/fixed_flat_map.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/escape.h"
#include "base/strings/sys_string_conversions.h"
#include "net/base/features.h"
#include "url/gurl.h"
#include "url/url_canon.h"

namespace {

// Schemes that are tracked in the Net.Apple.NSURL.DataMismatch.Scheme
// histogram. These values are persisted to logs. Entries should not be
// renumbered and numeric values should never be reused.
enum class Scheme {
  kUnknown = 0,
  kOther = 1,
  kAbout = 2,
  kBlob = 3,
  kContent = 4,
  kData = 5,
  kFile = 6,
  kFileSystem = 7,
  kFtp = 8,
  kHttp = 9,
  kHttps = 10,
  kMailto = 11,
  kTel = 12,
  kMaxValue = kTel,
};

Scheme SchemeForURL(NSURL* url) {
  NSString* scheme = [url scheme];
  if (!scheme) {
    return Scheme::kUnknown;
  }

  static constexpr auto kSchemeMap =
      base::MakeFixedFlatMap<std::string_view, Scheme>({
          {"about", Scheme::kAbout},
          {"blob", Scheme::kBlob},
          {"content", Scheme::kContent},
          {"data", Scheme::kData},
          {"file", Scheme::kFile},
          {"filesystem", Scheme::kFileSystem},
          {"ftp", Scheme::kFtp},
          {"http", Scheme::kHttp},
          {"https", Scheme::kHttps},
          {"mailto", Scheme::kMailto},
          {"tel", Scheme::kTel},
      });

  std::string lower_scheme = base::SysNSStringToUTF8([scheme lowercaseString]);
  auto it = kSchemeMap.find(lower_scheme);
  return it != kSchemeMap.end() ? it->second : Scheme::kOther;
}

}  // namespace

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

  std::string_view standard_url_string(url.absoluteString.UTF8String);

  // Foundation sometimes encodes the URL in absoluteString (and all the
  // NSURL accessors other than dataRepresentation), which is not what we want
  // to pass to GURL. For example, 'about:blank#hash' becomes
  // 'about:blank%23hash' in absoluteString, but remains 'about:blank#hash' in
  // dataRepresentation.
  if (base::FeatureList::IsEnabled(features::kUseNSURLDataForGURLConversion)) {
    NSData* data = [url dataRepresentation];
    if (data && data.length > 0) {
      std::string_view data_url_string(
          reinterpret_cast<const char*>(data.bytes), data.length);
      bool mismatch = standard_url_string != data_url_string;
      // TODO(crbug.com/474953367): Consider removing these histograms after
      // evaluation.
      UMA_HISTOGRAM_BOOLEAN("Net.Apple.NSURL.DataMismatch", mismatch);
      if (mismatch) {
        Scheme scheme = SchemeForURL(url);
        UMA_HISTOGRAM_ENUMERATION("Net.Apple.NSURL.DataMismatch.Scheme",
                                  scheme);

        // For "about:" scheme, use the data representation to avoid the
        // encoding issue.
        if (scheme == Scheme::kAbout) {
          return GURL(data_url_string);
        }
      }
    }
  }

  return GURL(standard_url_string);
}

}  // namespace net
