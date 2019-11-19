// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEBUI_NET_EXPORT_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_WEBUI_NET_EXPORT_TAB_HELPER_H_

#import "ios/web/public/web_state_user_data.h"

@protocol NetExportTabHelperDelegate;
@class ShowMailComposerContext;

// A tab helper for the Net Export WebUI page.
class NetExportTabHelper : public web::WebStateUserData<NetExportTabHelper> {
 public:
  ~NetExportTabHelper() override;

  // Creates a NetExportTabHelper and attaches it to |web_state|. The |delegate|
  // is not retained by the NetExportTabHelper and must not be nil.
  static void CreateForWebState(web::WebState* web_state,
                                id<NetExportTabHelperDelegate> delegate);

  // Shows a Mail Composer which allows the sending of an email. |context|
  // contains information for populating the email.
  void ShowMailComposer(ShowMailComposerContext* context);

 private:
  friend class web::WebStateUserData<NetExportTabHelper>;

  explicit NetExportTabHelper(id<NetExportTabHelperDelegate> delegate);
  __weak id<NetExportTabHelperDelegate> delegate_;

  WEB_STATE_USER_DATA_KEY_DECL();

  DISALLOW_COPY_AND_ASSIGN(NetExportTabHelper);
};

#endif  // IOS_CHROME_BROWSER_WEBUI_NET_EXPORT_TAB_HELPER_H_
