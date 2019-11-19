// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/u2f/u2f_tab_helper.h"

#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/u2f/u2f_controller.h"
#import "ios/chrome/browser/web/tab_id_tab_helper.h"
#import "ios/web/public/web_state.h"
#include "net/base/url_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const char kIsU2FKey[] = "isU2F";
const char kTabIDKey[] = "tabID";
}  // namespace

U2FTabHelper::U2FTabHelper(web::WebState* web_state) : web_state_(web_state) {}

U2FTabHelper::~U2FTabHelper() = default;

bool U2FTabHelper::IsU2FUrl(const GURL& url) {
  std::string is_u2f;
  return net::GetValueForKeyInQuery(url, kIsU2FKey, &is_u2f) && is_u2f == "1";
}

NSString* U2FTabHelper::GetTabIdFromU2FUrl(const GURL& url) {
  std::string tab_id;
  if (net::GetValueForKeyInQuery(url, kTabIDKey, &tab_id)) {
    return base::SysUTF8ToNSString(tab_id);
  }
  return nil;
}

void U2FTabHelper::EvaluateU2FResult(const GURL& url) {
  DCHECK(second_factor_controller_);
  [second_factor_controller_ evaluateU2FResultFromU2FURL:url
                                                webState:web_state_];
}

GURL U2FTabHelper::GetXCallbackUrl(const GURL& request_url,
                                   const GURL& origin_url) {
  // Create U2FController object lazily.
  if (!second_factor_controller_)
    second_factor_controller_ = [[U2FController alloc] init];
  NSString* tab_id = TabIdTabHelper::FromWebState(web_state_)->tab_id();
  return [second_factor_controller_
      XCallbackFromRequestURL:request_url
                    originURL:origin_url
                       tabURL:web_state_->GetLastCommittedURL()
                        tabID:tab_id];
}

WEB_STATE_USER_DATA_KEY_IMPL(U2FTabHelper)
