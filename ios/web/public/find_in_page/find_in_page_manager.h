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
                          public WebStateUserData<FindInPageManager> {
 protected:
  friend class WebStateUserData<FindInPageManager>;

  // Overload WebStateUserData<FindInPageManager>::Create() since
  // FindInPageManager is an abstract class and the factory needs
  // to create an instance of a sub-class.
  static std::unique_ptr<FindInPageManager> Create(WebState* web_state);
  static std::unique_ptr<FindInPageManager> Create(WebState* web_state,
                                                   base::TimeDelta delay);
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_FIND_IN_PAGE_FIND_IN_PAGE_MANAGER_H_
