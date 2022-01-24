// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/context_menu/context_menu_utils.h"

#include "base/strings/sys_string_conversions.h"
#include "base/values.h"
#include "components/url_formatter/url_formatter.h"
#include "ios/web/common/features.h"
#include "ios/web/public/ui/context_menu_params.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Enum for identifying how the menu title was constructed.
enum class ContextMenuTitleOrigin {
  kUnknown = 0,
  kURL = 1,           // the menu title is a URL (href or image src).
  kImageTitle = 2,    // the menu title is an image's title text
  kImageAltText = 3,  // the menu title is an image's alt text and src
};

typedef std::pair<NSString*, ContextMenuTitleOrigin> TitleAndOrigin;

// DEPRECATED
// Returns the title and origin for |params|.
TitleAndOrigin GetContextMenuTitleAndOrigin(web::ContextMenuParams params) {
  DCHECK(!base::FeatureList::IsEnabled(
      web::features::kWebViewNativeContextMenuPhase2));
  NSString* title = nil;
  ContextMenuTitleOrigin origin = ContextMenuTitleOrigin::kUnknown;

  if (params.link_url.is_valid()) {
    origin = ContextMenuTitleOrigin::kURL;
    if (params.link_url.SchemeIs(url::kJavaScriptScheme)) {
      title = @"JavaScript";
    } else {
      std::u16string URLText = url_formatter::FormatUrl(params.link_url);
      title = base::SysUTF16ToNSString(URLText);
    }
  }

  if (!title && params.src_url.is_valid()) {
    title = base::SysUTF8ToNSString(params.src_url.spec());
    origin = ContextMenuTitleOrigin::kURL;
  }

  if ([title hasPrefix:base::SysUTF8ToNSString(url::kDataScheme)]) {
    title = nil;
    origin = ContextMenuTitleOrigin::kURL;
  }

  if (params.title_attribute) {
    title = params.title_attribute;
    origin = ContextMenuTitleOrigin::kImageTitle;
  }

  // Prepend the alt text attribute if element is an image without a link.
  if (params.alt_text && params.src_url.is_valid() &&
      !params.link_url.is_valid()) {
    title = [NSString stringWithFormat:@"%@ – %@", params.alt_text, title];
    // If there was a title attribute, then the title origin is still "image
    // title", even though the alt text was prepended. Otherwise, set the title
    // origin to be "alt text".
    if (!params.title_attribute) {
      origin = ContextMenuTitleOrigin::kImageAltText;
    }
  }

  return TitleAndOrigin(title, origin);
}

}  // namespace

NSString* GetContextMenuTitle(web::ContextMenuParams params) {
  if (!base::FeatureList::IsEnabled(
          web::features::kWebViewNativeContextMenuPhase2)) {
    return GetContextMenuTitleAndOrigin(params).first;
  }
  if (params.link_url.is_valid()) {
    if (params.link_url.SchemeIsHTTPOrHTTPS()) {
      url_formatter::FormatUrlTypes format_types =
          url_formatter::kFormatUrlOmitDefaults |
          url_formatter::kFormatUrlTrimAfterHost |
          url_formatter::kFormatUrlOmitHTTPS |
          url_formatter::kFormatUrlOmitTrivialSubdomains;

      std::u16string formatted_url = url_formatter::FormatUrl(
          params.link_url, format_types, net::UnescapeRule::NORMAL,
          /*new_parsed=*/nullptr,
          /*prefix_end=*/nullptr, /*offset_for_adjustment=*/nullptr);
      return base::SysUTF16ToNSString(formatted_url);
    } else {
      return base::SysUTF8ToNSString(params.link_url.scheme());
    }
  }
  NSString* title = params.title_attribute;
  if (params.alt_text && params.src_url.is_valid()) {
    if (title) {
      title = [NSString stringWithFormat:@"%@ – %@", params.alt_text, title];
    } else {
      title = params.alt_text;
    }
  }
  return title;
}

NSString* GetContextMenuSubtitle(web::ContextMenuParams params) {
  return base::SysUTF8ToNSString(params.link_url.spec());
}

bool IsImageTitle(web::ContextMenuParams params) {
  DCHECK(!base::FeatureList::IsEnabled(
      web::features::kWebViewNativeContextMenuPhase2));
  return GetContextMenuTitleAndOrigin(params).second ==
         ContextMenuTitleOrigin::kImageTitle;
}
