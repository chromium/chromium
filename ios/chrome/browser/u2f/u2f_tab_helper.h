// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_U2F_U2F_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_U2F_U2F_TAB_HELPER_H_

#include "base/macros.h"
#import "ios/web/public/web_state_user_data.h"
#include "url/gurl.h"

@class U2FController;

// A tab helper that handles Universal 2nd Factory (U2F) requests.
class U2FTabHelper : public web::WebStateUserData<U2FTabHelper> {
 public:
  ~U2FTabHelper() override;

  // Checks if the given |url| is U2F call URL.
  static bool IsU2FUrl(const GURL& url);

  // Returns the tabID in the U2F callback. Returns nil if tabID not found.
  static NSString* GetTabIdFromU2FUrl(const GURL& u2f_url);

  // Evaluates U2F result.
  virtual void EvaluateU2FResult(const GURL& url);

  // Generates a GURL compliant with the x-callback-url specs for FIDO Universal
  // 2nd Factory (U2F) requests. Returns empty GURL if origin is not secure.
  // See http://x-callback-url.com/specifications/ for specifications.
  // This function is needed used in App launching to verify the |request_url|
  // before launching the required authentication app.
  GURL GetXCallbackUrl(const GURL& request_url, const GURL& origin_url);

 protected:
  // Constructor for U2FTabHelper.
  U2FTabHelper(web::WebState* web_state);

 private:
  friend class web::WebStateUserData<U2FTabHelper>;

  // The WebState that this object is attached to.
  web::WebState* web_state_ = nullptr;

  // Universal Second Factor (U2F) call controller.
  U2FController* second_factor_controller_ = nil;

  WEB_STATE_USER_DATA_KEY_DECL();

  DISALLOW_COPY_AND_ASSIGN(U2FTabHelper);
};

#endif  // IOS_CHROME_BROWSER_U2F_U2F_TAB_HELPER_H_
