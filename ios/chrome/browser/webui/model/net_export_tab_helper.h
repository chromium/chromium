// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEBUI_MODEL_NET_EXPORT_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_WEBUI_MODEL_NET_EXPORT_TAB_HELPER_H_

#import "ios/web/public/lazy_web_state_user_data.h"

@protocol NetExportTabHelperDelegate;
@class ShowMailComposerContext;

// A tab helper for the Net Export WebUI page.
class NetExportTabHelper
    : public web::LazyWebStateUserData<NetExportTabHelper> {
 public:
  NetExportTabHelper(const NetExportTabHelper&) = delete;
  NetExportTabHelper& operator=(const NetExportTabHelper&) = delete;

  ~NetExportTabHelper() override;

  // Shows a Mail Composer which allows the sending of an email. `context`
  // contains information for populating the email.
  void ShowMailComposer(ShowMailComposerContext* context);

  // Set the delegate.
  void SetDelegate(id<NetExportTabHelperDelegate> delegate);

 private:
  friend class web::LazyWebStateUserData<NetExportTabHelper>;

  explicit NetExportTabHelper(web::WebState* web_state);
  __weak id<NetExportTabHelperDelegate> delegate_;

  WEB_STATE_USER_DATA_KEY_DECL();
};

#endif  // IOS_CHROME_BROWSER_WEBUI_MODEL_NET_EXPORT_TAB_HELPER_H_
