// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SEARCH_ENGINES_SEARCH_ENGINE_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_SEARCH_ENGINES_SEARCH_ENGINE_TAB_HELPER_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#import "ios/web/public/web_state/web_state_observer.h"
#import "ios/web/public/web_state/web_state_user_data.h"

namespace web {
class WebState;
}  // namespace web

class SearchEngineTabHelper
    : public web::WebStateObserver,
      public web::WebStateUserData<SearchEngineTabHelper> {
 public:
  ~SearchEngineTabHelper() override;

 private:
  friend class web::WebStateUserData<SearchEngineTabHelper>;

  explicit SearchEngineTabHelper(web::WebState* web_state);

  // WebStateObserver implementation.
  void WebStateDestroyed(web::WebState* web_state) override;

  // WebState this tab helper is attached to.
  web::WebState* web_state_ = nullptr;

  base::WeakPtrFactory<SearchEngineTabHelper> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(SearchEngineTabHelper);
};

#endif  // IOS_CHROME_BROWSER_SEARCH_ENGINES_SEARCH_ENGINE_TAB_HELPER_H_
