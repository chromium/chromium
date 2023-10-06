// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/find_in_page/model/abstract_find_tab_helper.h"

#import "ios/chrome/browser/find_in_page/model/find_tab_helper.h"
#import "ios/chrome/browser/find_in_page/model/java_script_find_tab_helper.h"
#import "ios/chrome/browser/find_in_page/model/util.h"

AbstractFindTabHelper* GetConcreteFindTabHelperFromWebState(
    web::WebState* web_state) {
  if (IsNativeFindInPageAvailable()) {
    return FindTabHelper::FromWebState(web_state);
  }
  return JavaScriptFindTabHelper::FromWebState(web_state);
}
