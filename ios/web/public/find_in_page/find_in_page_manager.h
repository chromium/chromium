// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_FIND_IN_PAGE_FIND_IN_PAGE_MANAGER_H_
#define IOS_WEB_PUBLIC_FIND_IN_PAGE_FIND_IN_PAGE_MANAGER_H_

#import "ios/web/public/find_in_page/abstract_find_in_page_manager.h"
#import "ios/web/public/web_state_user_data.h"

namespace web {

// Manager for searching text on a page. As opposed to the
// JavaScriptFindInPageManager, this manager does not rely on JavaScript but on
// the Find interaction API available on iOS 16 or later.
class FindInPageManager : public AbstractFindInPageManager,
                          public web::WebStateUserData<FindInPageManager> {
 public:
  // Need to overload as the default implementation inherited from
  // WebStateUserData<FindInPageManager> would create a
  // FindInPageManager which is a pure abstract class. Should only be called if
  // the web state is realized.
  static void CreateForWebState(WebState* web_state);

  FindInPageManager() = default;

  FindInPageManager(const FindInPageManager&) = delete;
  FindInPageManager& operator=(const FindInPageManager&) = delete;

  WEB_STATE_USER_DATA_KEY_DECL();

 protected:
  ~FindInPageManager() override = default;
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_FIND_IN_PAGE_FIND_IN_PAGE_MANAGER_H_
