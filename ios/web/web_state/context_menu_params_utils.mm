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

namespace web {

BOOL CanShowContextMenuForElementDictionary(NSDictionary* element) {
  NSString* href = element[kContextMenuElementHyperlink];
  if (GURL(base::SysNSStringToUTF8(href)).is_valid()) {
    return YES;
  }
  NSString* src = element[kContextMenuElementSource];
  if (GURL(base::SysNSStringToUTF8(src)).is_valid()) {
    return YES;
  }
  return NO;
}

ContextMenuParams ContextMenuParamsFromElementDictionary(
    NSDictionary* element) {
  ContextMenuParams params;
  NSString* title = nil;
  NSString* href = element[kContextMenuElementHyperlink];
  if (href) {
    params.link_url = GURL(base::SysNSStringToUTF8(href));
    if (params.link_url.SchemeIs(url::kJavaScriptScheme)) {
      title = @"JavaScript";
    } else {
      base::string16 URLText = url_formatter::FormatUrl(params.link_url);
      title = base::SysUTF16ToNSString(URLText);
    }
  }
  NSString* src = element[kContextMenuElementSource];
  if (src) {
    params.src_url = GURL(base::SysNSStringToUTF8(src));
    if (!title)
      title = [src copy];
    if ([title hasPrefix:base::SysUTF8ToNSString(url::kDataScheme)])
      title = nil;
  }
  NSString* titleAttribute = element[kContextMenuElementTitle];
  if (titleAttribute)
    title = titleAttribute;
  if (title) {
    params.menu_title = [title copy];
  }
  NSString* referrerPolicy = element[kContextMenuElementReferrerPolicy];
  if (referrerPolicy) {
    params.referrer_policy =
        web::ReferrerPolicyFromString(base::SysNSStringToUTF8(referrerPolicy));
  }
  NSString* innerText = element[kContextMenuElementInnerText];
  if ([innerText length] > 0) {
    params.link_text = [innerText copy];
  }
  return params;
}

}  // namespace web
