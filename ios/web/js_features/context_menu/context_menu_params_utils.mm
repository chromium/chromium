// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/js_features/context_menu/context_menu_params_utils.h"

#import <Foundation/Foundation.h>

#import "base/strings/sys_string_conversions.h"
#import "base/values.h"
#import "ios/web/common/referrer_util.h"
#import "ios/web/js_features/context_menu/context_menu_constants.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

ContextMenuParams ContextMenuParamsFromElementDictionary(base::Value* element) {
  ContextMenuParams params;
  const base::Value::Dict* element_dict =
      element ? element->GetIfDict() : nullptr;
  if (!element_dict) {
    // Invalid `element`.
    return params;
  }

  const std::string* tag_name =
      element_dict->FindString(kContextMenuElementTagName);
  if (tag_name) {
    params.tag_name = base::SysUTF8ToNSString(*tag_name);
  }

  const std::string* href =
      element_dict->FindString(kContextMenuElementHyperlink);
  if (href) {
    params.link_url = GURL(*href);
  }

  const std::string* src = element_dict->FindString(kContextMenuElementSource);
  if (src) {
    params.src_url = GURL(*src);
  }

  const std::string* referrer_policy =
      element_dict->FindString(kContextMenuElementReferrerPolicy);
  if (referrer_policy) {
    params.referrer_policy = web::ReferrerPolicyFromString(*referrer_policy);
  }

  const std::string* inner_text =
      element_dict->FindString(kContextMenuElementInnerText);
  if (inner_text && !inner_text->empty()) {
    params.text = base::SysUTF8ToNSString(*inner_text);
  }

  const std::string* title_attribute =
      element_dict->FindString(web::kContextMenuElementTitle);
  if (title_attribute) {
    params.title_attribute = base::SysUTF8ToNSString(*title_attribute);
  }

  const std::string* alt_text =
      element_dict->FindString(web::kContextMenuElementAlt);
  if (alt_text) {
    params.alt_text = base::SysUTF8ToNSString(*alt_text);
  }

  absl::optional<double> text_offset =
      element_dict->FindDouble(web::kContextMenuElementTextOffset);
  if (text_offset.has_value()) {
    params.text_offset = *text_offset;
  }

  const std::string* surrounding_text =
      element_dict->FindString(kContextMenuElementSurroundingText);
  if (surrounding_text && !surrounding_text->empty()) {
    params.surrounding_text = base::SysUTF8ToNSString(*surrounding_text);
  }

  absl::optional<double> surrounding_text_offset =
      element_dict->FindDouble(web::kContextMenuElementSurroundingTextOffset);
  if (surrounding_text_offset.has_value()) {
    params.surrounding_text_offset = *surrounding_text_offset;
  }

  return params;
}

}  // namespace web
