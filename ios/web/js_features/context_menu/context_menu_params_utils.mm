// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/js_features/context_menu/context_menu_params_utils.h"

#import <Foundation/Foundation.h>

#import "base/strings/sys_string_conversions.h"
#import "base/values.h"
#import "ios/web/common/referrer_util.h"
#import "ios/web/js_features/context_menu/context_menu_constants.h"

namespace web {

ContextMenuParams ContextMenuParamsFromElementDictionary(
    const base::Value::Dict& element) {
  ContextMenuParams params;

  const std::string* tag_name = element.FindString(kContextMenuElementTagName);
  if (tag_name) {
    params.tag_name = base::SysUTF8ToNSString(*tag_name);
  }

  const std::string* href = element.FindString(kContextMenuElementHyperlink);
  if (href) {
    params.link_url = GURL(*href);
  }

  const std::string* src = element.FindString(kContextMenuElementSource);
  if (src) {
    params.src_url = GURL(*src);
  }

  const std::string* referrer_policy =
      element.FindString(kContextMenuElementReferrerPolicy);
  if (referrer_policy) {
    params.referrer_policy = web::ReferrerPolicyFromString(*referrer_policy);
  }

  const std::string* inner_text =
      element.FindString(kContextMenuElementInnerText);
  if (inner_text && !inner_text->empty()) {
    params.text = base::SysUTF8ToNSString(*inner_text);
  }

  const std::string* title_attribute =
      element.FindString(web::kContextMenuElementTitle);
  if (title_attribute) {
    params.title_attribute = base::SysUTF8ToNSString(*title_attribute);
  }

  const std::string* alt_text = element.FindString(web::kContextMenuElementAlt);
  if (alt_text) {
    params.alt_text = base::SysUTF8ToNSString(*alt_text);
  }

  std::optional<double> text_offset =
      element.FindDouble(web::kContextMenuElementTextOffset);
  if (text_offset.has_value()) {
    params.text_offset = *text_offset;
  }

  const std::string* surrounding_text =
      element.FindString(kContextMenuElementSurroundingText);
  if (surrounding_text && !surrounding_text->empty()) {
    params.surrounding_text = base::SysUTF8ToNSString(*surrounding_text);
  }

  std::optional<double> surrounding_text_offset =
      element.FindDouble(web::kContextMenuElementSurroundingTextOffset);
  if (surrounding_text_offset.has_value()) {
    params.surrounding_text_offset = *surrounding_text_offset;
  }

  return params;
}

}  // namespace web
