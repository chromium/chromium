// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/store_kit/store_kit_tab_helper.h"

#import <StoreKit/StoreKit.h>

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

StoreKitTabHelper::StoreKitTabHelper(web::WebState* web_state) {}

StoreKitTabHelper::~StoreKitTabHelper() {}

void StoreKitTabHelper::SetLauncher(id<StoreKitLauncher> launcher) {
  store_kit_launcher_ = launcher;
}

id<StoreKitLauncher> StoreKitTabHelper::GetLauncher() {
  return store_kit_launcher_;
}

void StoreKitTabHelper::OpenAppStore(NSString* app_id) {
  [store_kit_launcher_ openAppStore:app_id];
}

void StoreKitTabHelper::OpenAppStore(NSDictionary* product_params) {
  [store_kit_launcher_ openAppStoreWithParameters:product_params];
}

WEB_STATE_USER_DATA_KEY_IMPL(StoreKitTabHelper)
