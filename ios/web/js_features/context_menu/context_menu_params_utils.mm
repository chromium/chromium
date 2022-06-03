// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/js_features/context_menu/context_menu_params_utils.h"

#import <Foundation/Foundation.h>

#include "base/strings/sys_string_conversions.h"
#include "base/values.h"
#include "ios/web/common/referrer_util.h"
#include "ios/web/js_features/context_menu/context_menu_constants.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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

  std::string* title_attribute =
      element->FindStringKey(web::kContextMenuElementTitle);
  if (title_attribute) {
    params.title_attribute = base::SysUTF8ToNSString(*title_attribute);
  }

  std::string* alt_text = element->FindStringKey(web::kContextMenuElementAlt);
  if (alt_text) {
    params.alt_text = base::SysUTF8ToNSString(*alt_text);
  }

  return params;
}

}  // namespace web
