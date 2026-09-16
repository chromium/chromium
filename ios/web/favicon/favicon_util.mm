// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/favicon/favicon_util.h"

#import <CoreFoundation/CoreFoundation.h>
#import <WebKit/WebKit.h>

#import <string_view>

#import "base/logging.h"
#import "base/strings/string_number_conversions.h"
#import "base/strings/string_split.h"
#import "base/strings/string_util.h"
#import "ios/web/public/web_client.h"

namespace web {
namespace {

// Returns whether `href` is allowed for a favicon on `page_url`.
bool IsAllowedFaviconURL(const GURL& href, const GURL& page_url) {
  // It is valid for favicon to be served via http, https or data: schemes.
  if (href.SchemeIsHTTPOrHTTPS() || href.SchemeIs(url::kDataScheme)) {
    return true;
  }

  // Only allow app specific URLs if the page itself is from app specific URL.
  if (WebClient* web_client = GetWebClient()) {
    if (web_client->IsAppSpecificURL(href)) {
      return web_client->IsAppSpecificURL(page_url);
    }
  }

  // Otherwise, the url is not allowed.
  return false;
}

}  // namespace

std::vector<web::FaviconURL> ExtractFaviconURL(const base::ListValue& favicons,
                                               const GURL& page_url) {
  BOOL has_favicon = NO;
  std::vector<web::FaviconURL> favicon_urls;
  for (const base::Value& favicon : favicons) {
    if (!favicon.is_dict()) {
      continue;
    }

    const base::DictValue& favicon_dict = favicon.GetDict();
    const std::string* href_value = favicon_dict.FindString("href");
    if (!href_value) {
      continue;
    }

    const GURL url(*href_value);
    if (!url.is_valid() || !IsAllowedFaviconURL(url, page_url)) {
      continue;
    }

    const std::string* rel_value = favicon_dict.FindString("rel");
    if (!rel_value) {
      continue;
    }
    auto rel = *rel_value;

    BOOL is_apple_touch = YES;
    web::FaviconURL::IconType icon_type = web::FaviconURL::IconType::kFavicon;
    if (rel == "apple-touch-icon") {
      icon_type = web::FaviconURL::IconType::kTouchIcon;
    } else if (rel == "apple-touch-icon-precomposed") {
      icon_type = web::FaviconURL::IconType::kTouchPrecomposedIcon;
    } else {
      is_apple_touch = NO;
    }

    std::vector<gfx::Size> sizes;
    if (const std::string* size_value = favicon_dict.FindString("sizes")) {
      auto sizes_string = *size_value;
      // Parse the sizes attribute. It should consist of one or multiple
      // elements of the form "76x76", separated by a whitespace. So "76x76" or
      // "120x120 192x192" are legit.
      auto split_sizes = base::SplitStringPiece(
          sizes_string, base::kWhitespaceASCII, base::TRIM_WHITESPACE,
          base::SPLIT_WANT_NONEMPTY);
      for (const auto& cut : split_sizes) {
        std::vector<std::string_view> pieces = base::SplitStringPiece(
            cut, "x", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
        int width = 0, height = 0;
        if (pieces.size() != 2 || !base::StringToInt(pieces[0], &width) ||
            !base::StringToInt(pieces[1], &height)) {
          continue;
        }

        if (width > 0 && height > 0) {
          sizes.push_back(gfx::Size(width, height));
        }
      }
    }

    favicon_urls.push_back(web::FaviconURL(url, icon_type, sizes));
    has_favicon = has_favicon || !is_apple_touch;
  }

  if (!has_favicon) {
    // If an HTTP(S)? webpage does not reference a "favicon" of a type different
    // from apple touch, then search for a file named "favicon.ico" at the root
    // of the website (legacy). http://en.wikipedia.org/wiki/Favicon
    if (page_url.is_valid() && page_url.SchemeIsHTTPOrHTTPS()) {
      GURL::Replacements replacements;
      replacements.SetPathStr("/favicon.ico");
      replacements.ClearQuery();
      replacements.ClearRef();
      favicon_urls.push_back(web::FaviconURL(
          page_url.ReplaceComponents(replacements),
          web::FaviconURL::IconType::kFavicon, std::vector<gfx::Size>()));
    }
  }

  return favicon_urls;
}

}  // namespace web
