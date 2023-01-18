// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_FIND_IN_PAGE_JAVA_SCRIPT_FIND_IN_PAGE_MANAGER_H_
#define IOS_WEB_PUBLIC_FIND_IN_PAGE_JAVA_SCRIPT_FIND_IN_PAGE_MANAGER_H_

#import "ios/web/public/find_in_page/abstract_find_in_page_manager.h"
#import "ios/web/public/web_state_user_data.h"

namespace web {

// Manager for searching text on a page. Supports searching within all iframes.
class JavaScriptFindInPageManager
    : public AbstractFindInPageManager,
      public web::WebStateUserData<JavaScriptFindInPageManager> {
 public:
  JavaScriptFindInPageManager() = default;

  JavaScriptFindInPageManager(const JavaScriptFindInPageManager&) = delete;
  JavaScriptFindInPageManager& operator=(const JavaScriptFindInPageManager&) =
      delete;

  WEB_STATE_USER_DATA_KEY_DECL();

 protected:
  ~JavaScriptFindInPageManager() override = default;
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_FIND_IN_PAGE_JAVA_SCRIPT_FIND_IN_PAGE_MANAGER_H_
