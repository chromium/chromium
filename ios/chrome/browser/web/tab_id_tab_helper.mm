// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/tab_id_tab_helper.h"

#include "base/mac/foundation_util.h"
#include "base/strings/sys_string_conversions.h"
#import "ios/web/public/session/serializable_user_data_manager.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

TabIdTabHelper::TabIdTabHelper(web::WebState* web_state)
    : web_state_(web_state) {
  DCHECK(web_state_);
}

TabIdTabHelper::~TabIdTabHelper() = default;

NSString* TabIdTabHelper::tab_id() const {
  return web_state_->GetStableIdentifier();
}

WEB_STATE_USER_DATA_KEY_IMPL(TabIdTabHelper)
