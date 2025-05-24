// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/context_menu/ui_bundled/context_menu_utils.h"

#import "base/strings/sys_string_conversions.h"
#import "base/values.h"
#import "components/url_formatter/url_formatter.h"
#import "ios/web/common/features.h"
#import "ios/web/public/ui/context_menu_params.h"
#import "url/gurl.h"

namespace {

// Enum for identifying how the menu title was constructed.
enum class ContextMenuTitleOrigin {
  kUnknown = 0,
  kURL = 1,           // the menu title is a URL (href or image src).
  kImageTitle = 2,    // the menu title is an image's title text
  kImageAltText = 3,  // the menu title is an image's alt text and src
};

typedef std::pair<NSString*, ContextMenuTitleOrigin> TitleAndOrigin;

// Returns the title and origin for `params`.
TitleAndOrigin GetContextMenuTitleAndOrigin(web::ContextMenuParams params) {
  NSString* title = nil;
  ContextMenuTitleOrigin origin = ContextMenuTitleOrigin::kUnknown;

  if (params.link_url.is_valid()) {
    origin = ContextMenuTitleOrigin::kURL;
    if (params.link_url.SchemeIs(url::kJavaScriptScheme)) {
      title = @"JavaScript";
    } else {
      std::u16string URLText = url_formatter::FormatUrl(params.link_url);
      title = base::SysUTF16ToNSString(URLText);
      // If there is a link URL, the URL *must* be displayed in the title as
      // this is what the user will potentially open or share.
      return TitleAndOrigin(title, origin);
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
    if ([title length]) {
      title = [NSString stringWithFormat:@"%@ – %@", params.alt_text, title];
    } else {
      title = params.alt_text;
    }
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
  return GetContextMenuTitleAndOrigin(params).first;
}

NSString* GetContextMenuSubtitle(web::ContextMenuParams params) {
  return base::SysUTF8ToNSString(params.link_url.spec());
}

bool IsImageTitle(web::ContextMenuParams params) {
  return GetContextMenuTitleAndOrigin(params).second ==
         ContextMenuTitleOrigin::kImageTitle;
}
