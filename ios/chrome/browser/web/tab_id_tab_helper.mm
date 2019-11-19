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

namespace {
// The key under which the tab ID is stored in the WebState's serializable user
// data.
NSString* const kTabIdKey = @"TabId";
}

TabIdTabHelper::TabIdTabHelper(web::WebState* web_state) {
  web::SerializableUserDataManager* user_data_manager =
      web::SerializableUserDataManager::FromWebState(web_state);
  NSString* unique_id = base::mac::ObjCCast<NSString>(
      user_data_manager->GetValueForSerializationKey(kTabIdKey));
  if (![unique_id length]) {
    unique_id = [[NSUUID UUID] UUIDString];
    user_data_manager->AddSerializableData(unique_id, kTabIdKey);
  }
  tab_id_ = [unique_id copy];
}

TabIdTabHelper::~TabIdTabHelper() = default;

WEB_STATE_USER_DATA_KEY_IMPL(TabIdTabHelper)
