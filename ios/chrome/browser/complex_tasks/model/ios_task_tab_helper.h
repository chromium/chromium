// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COMPLEX_TASKS_MODEL_IOS_TASK_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_COMPLEX_TASKS_MODEL_IOS_TASK_TAB_HELPER_H_

#include <unordered_map>

#import "base/memory/raw_ptr.h"
#import "ios/chrome/browser/complex_tasks/model/ios_content_record_task_id.h"
#include "ios/web/public/web_state_observer.h"
#import "ios/web/public/web_state_user_data.h"

// This class tracks Task IDs - navigations and relationships between
// navigations. Task IDs have applications in navigation clustering
// and assist in grouping together and organizing the user's history.
class IOSTaskTabHelper : public web::WebStateObserver,
                         public web::WebStateUserData<IOSTaskTabHelper> {
 public:
  explicit IOSTaskTabHelper(web::WebState* web_state);

  IOSTaskTabHelper(const IOSTaskTabHelper&) = delete;
  IOSTaskTabHelper& operator=(const IOSTaskTabHelper&) = delete;

  ~IOSTaskTabHelper() override;

  // web::WebStateObserver
  void DidStartNavigation(web::WebState* web_state,
                          web::NavigationContext* navigation_context) override;
  void DidFinishNavigation(web::WebState* web_state,
                           web::NavigationContext* navigation_context) override;
  void WebStateDestroyed(web::WebState* web_state) override;

  const IOSContentRecordTaskId* GetContextRecordTaskId(int nav_id) const;

 private:
  friend class web::WebStateUserData<IOSTaskTabHelper>;
  std::unordered_map<int, IOSContentRecordTaskId>
      ios_content_record_task_id_map_;
  raw_ptr<web::WebState> web_state_ = nullptr;
  int prev_item_unique_id_ = -1;
  WEB_STATE_USER_DATA_KEY_DECL();
};

#endif  // IOS_CHROME_BROWSER_COMPLEX_TASKS_MODEL_IOS_TASK_TAB_HELPER_H_
