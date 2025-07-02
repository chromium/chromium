// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/browser_container/model/edit_menu_tab_helper.h"

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/browser_container/model/edit_menu_builder.h"
#import "ios/web/public/web_state.h"

EditMenuTabHelper::EditMenuTabHelper(web::WebState* web_state) {
  web_state_ = web_state;
}

EditMenuTabHelper::~EditMenuTabHelper() = default;

void EditMenuTabHelper::SetEditMenuBuilder(id<EditMenuBuilder> builder) {
  edit_menu_builder_ = builder;
}

id<EditMenuBuilder> EditMenuTabHelper::GetEditMenuBuilder() {
  return edit_menu_builder_;
}

void EditMenuTabHelper::BuildEditMenu(id<UIMenuBuilder> builder) const {
  [edit_menu_builder_ buildEditMenuWithBuilder:builder inWebState:web_state_];
}
