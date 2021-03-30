// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/js_features/context_menu/context_menu_params_utils.h"

#import <Foundation/Foundation.h>

#include "base/strings/sys_string_conversions.h"
#include "base/values.h"
#include "components/url_formatter/url_formatter.h"
#include "ios/web/common/referrer_util.h"
#include "ios/web/js_features/context_menu/context_menu_constants.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

typedef std::pair<NSString*, web::ContextMenuTitleOrigin> TitleAndOrigin;

TitleAndOrigin GetContextMenuTitleAndOrigin(base::Value* element) {
  NSString* title = nil;
  web::ContextMenuTitleOrigin origin = web::ContextMenuTitleOrigin::kUnknown;

  std::string* href = element->FindStringKey(web::kContextMenuElementHyperlink);
  if (href) {
    GURL link_url = GURL(*href);
    origin = web::ContextMenuTitleOrigin::kURL;
    if (link_url.SchemeIs(url::kJavaScriptScheme)) {
      title = @"JavaScript";
    } else {
      std::u16string URLText = url_formatter::FormatUrl(link_url);
      title = base::SysUTF16ToNSString(URLText);
    }
  }

  std::string* src = element->FindStringKey(web::kContextMenuElementSource);
  if (!title && src) {
    title = base::SysUTF8ToNSString(*src);
    origin = web::ContextMenuTitleOrigin::kURL;
  }

  if ([title hasPrefix:base::SysUTF8ToNSString(url::kDataScheme)]) {
    title = nil;
    origin = web::ContextMenuTitleOrigin::kURL;
  }

  std::string* title_attribute =
      element->FindStringKey(web::kContextMenuElementTitle);
  if (title_attribute) {
    title = base::SysUTF8ToNSString(*title_attribute);
    origin = web::ContextMenuTitleOrigin::kImageTitle;
  }

  // Prepend the alt text attribute if element is an image without a link.
  std::string* alt_text = element->FindStringKey(web::kContextMenuElementAlt);
  if (alt_text && src && !href) {
    title = [NSString stringWithFormat:@"%s – %@", alt_text->c_str(), title];
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

bool CanShowContextMenuForParams(const ContextMenuParams& params) {
  if (params.link_url.is_valid()) {
    return true;
  }
  if (params.src_url.is_valid()) {
    return true;
  }
  return false;
}

ContextMenuParams ContextMenuParamsFromElementDictionary(base::Value* element) {
  ContextMenuParams params;
  if (!element || !element->is_dict()) {
    // Invalid |element|.
    return params;
  }

  std::string* href = element->FindStringKey(kContextMenuElementHyperlink);
  if (href) {
    params.link_url = GURL(*href);
  }

  std::string* src = element->FindStringKey(kContextMenuElementSource);
  if (src) {
    params.src_url = GURL(*src);
  }

  std::string* referrer_policy =
      element->FindStringKey(kContextMenuElementReferrerPolicy);
  if (referrer_policy) {
    params.referrer_policy = web::ReferrerPolicyFromString(*referrer_policy);
  }

  std::string* inner_text =
      element->FindStringKey(kContextMenuElementInnerText);
  if (inner_text && !inner_text->empty()) {
    params.link_text = base::SysUTF8ToNSString(*inner_text);
  }

  TitleAndOrigin title_and_origin = GetContextMenuTitleAndOrigin(element);
  params.menu_title = title_and_origin.first;
  params.menu_title_origin = title_and_origin.second;

  return params;
}

}  // namespace web
