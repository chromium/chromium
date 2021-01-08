// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/context_menu_params_utils.h"

#include "base/strings/sys_string_conversions.h"
#include "components/url_formatter/url_formatter.h"
#include "ios/web/common/referrer_util.h"
#import "ios/web/web_state/context_menu_constants.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

typedef std::pair<NSString*, web::ContextMenuTitleOrigin> TitleAndOrigin;

TitleAndOrigin GetContextMenuTitleAndOrigin(NSDictionary* element) {
  NSString* title = nil;
  web::ContextMenuTitleOrigin origin = web::ContextMenuTitleOrigin::kUnknown;
  NSString* href = element[web::kContextMenuElementHyperlink];
  if (href) {
    GURL link_url = GURL(base::SysNSStringToUTF8(href));
    origin = web::ContextMenuTitleOrigin::kURL;
    if (link_url.SchemeIs(url::kJavaScriptScheme)) {
      title = @"JavaScript";
    } else {
      base::string16 URLText = url_formatter::FormatUrl(link_url);
      title = base::SysUTF16ToNSString(URLText);
    }
  }

  NSString* src = element[web::kContextMenuElementSource];
  if (!title) {
    title = [src copy];
    origin = web::ContextMenuTitleOrigin::kURL;
  }

  if ([title hasPrefix:base::SysUTF8ToNSString(url::kDataScheme)]) {
    title = nil;
    origin = web::ContextMenuTitleOrigin::kURL;
  }

  NSString* title_attribute = element[web::kContextMenuElementTitle];
  if (title_attribute) {
    title = title_attribute;
    origin = web::ContextMenuTitleOrigin::kImageTitle;
  }

  // Prepend the alt text attribute if element is an image without a link.
  NSString* alt_text = element[web::kContextMenuElementAlt];
  if (alt_text && src && !href) {
    title = [NSString stringWithFormat:@"%@ – %@", alt_text, title];
    // If there was a title attribute, then the title origin is still "image
    // title", even though the alt text was prepended. Otherwise, set the title
    // origin to be "alt text".
    if (!title_attribute) {
      origin = web::ContextMenuTitleOrigin::kImageAltText;
    }
  }

  return TitleAndOrigin(title, origin);
}

}  // namespace

namespace web {

BOOL CanShowContextMenuForParams(const ContextMenuParams& params) {
  if (params.link_url.is_valid()) {
    return YES;
  }
  if (params.src_url.is_valid()) {
    return YES;
  }
  return NO;
}

ContextMenuParams ContextMenuParamsFromElementDictionary(
    NSDictionary* element) {
  ContextMenuParams params;
  NSString* href = element[kContextMenuElementHyperlink];
  if (href)
    params.link_url = GURL(base::SysNSStringToUTF8(href));
  NSString* src = element[kContextMenuElementSource];
  if (src)
    params.src_url = GURL(base::SysNSStringToUTF8(src));
  NSString* referrer_policy = element[kContextMenuElementReferrerPolicy];
  if (referrer_policy) {
    params.referrer_policy =
        web::ReferrerPolicyFromString(base::SysNSStringToUTF8(referrer_policy));
  }
  NSString* inner_text = element[kContextMenuElementInnerText];
  if ([inner_text length] > 0)
    params.link_text = [inner_text copy];

  TitleAndOrigin title_and_origin = GetContextMenuTitleAndOrigin(element);
  params.menu_title = title_and_origin.first;
  params.menu_title_origin = title_and_origin.second;

  return params;
}

}  // namespace web
