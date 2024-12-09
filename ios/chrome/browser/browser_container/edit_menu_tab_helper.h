// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BROWSER_CONTAINER_EDIT_MENU_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_BROWSER_CONTAINER_EDIT_MENU_TAB_HELPER_H_

#import "ios/web/public/web_state_user_data.h"

namespace web {
class WebState;
}  // namespace web

@protocol UIMenuBuilder;
@protocol EditMenuBuilder;

// Forwards the handling of edit menu from the web view.
class EditMenuTabHelper : public web::WebStateUserData<EditMenuTabHelper> {
 public:
 public:
  EditMenuTabHelper(const EditMenuTabHelper&) = delete;
  EditMenuTabHelper& operator=(const EditMenuTabHelper&) = delete;

  ~EditMenuTabHelper() override;

  // Attaches the builder for the edit menu.
  void SetEditMenuBuilder(id<EditMenuBuilder> builder);

  // Build the edit menu using `edit_menu_builder_`.
  void BuildEditMenu(id<UIMenuBuilder> builder) const;

 private:
  explicit EditMenuTabHelper(web::WebState* web_state);
  friend class web::WebStateUserData<EditMenuTabHelper>;

  __weak id<EditMenuBuilder> edit_menu_builder_;

  WEB_STATE_USER_DATA_KEY_DECL();
};

#endif  // IOS_CHROME_BROWSER_BROWSER_CONTAINER_EDIT_MENU_TAB_HELPER_H_
