// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PRERENDER_MODEL_PRERENDER_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_PRERENDER_MODEL_PRERENDER_TAB_HELPER_H_

#include "base/memory/raw_ref.h"
#include "ios/web/public/web_state_user_data.h"

class PrerenderTabHelperDelegate;

// TabHelper that is attached to WebStates used for pre-rendering.
//
// It can be used to check whether a WebState is used for pre-rendering (if
// the WebState is used for pre-rendering the tab helper will be attached,
// otherwise it will not exist).
class PrerenderTabHelper : public web::WebStateUserData<PrerenderTabHelper> {
 public:
  ~PrerenderTabHelper() override;

  // Cancel the pre-rendering. This will destroy the WebState and the
  // PrerenderTabHelper so the objects must not be used after calling
  // this method.
  void CancelPrerender();

 private:
  friend class web::WebStateUserData<PrerenderTabHelper>;

  PrerenderTabHelper(web::WebState* web_state,
                     PrerenderTabHelperDelegate* delegate);

  const raw_ref<PrerenderTabHelperDelegate> delegate_;
};

#endif  // IOS_CHROME_BROWSER_PRERENDER_MODEL_PRERENDER_TAB_HELPER_H_
