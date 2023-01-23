// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/find_in_page/abstract_find_tab_helper.h"

#import "ios/chrome/browser/find_in_page/find_tab_helper.h"
#import "ios/chrome/browser/find_in_page/java_script_find_tab_helper.h"
#import "ios/public/provider/chrome/browser/find_in_page/find_in_page_api.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

AbstractFindTabHelper* GetConcreteFindTabHelperFromWebState(
    web::WebState* web_state) {
  if (ios::provider::IsNativeFindInPageEnabled()) {
    return FindTabHelper::FromWebState(web_state);
  }
  return JavaScriptFindTabHelper::FromWebState(web_state);
}
