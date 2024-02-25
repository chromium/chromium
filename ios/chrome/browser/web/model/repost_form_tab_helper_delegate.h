// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_MODEL_REPOST_FORM_TAB_HELPER_DELEGATE_H_
#define IOS_CHROME_BROWSER_WEB_MODEL_REPOST_FORM_TAB_HELPER_DELEGATE_H_

#import <Foundation/Foundation.h>

class RepostFormTabHelper;

// Delegate for RepostFormTabHelper.
@protocol RepostFormTabHelperDelegate <NSObject>

// Asks the delegate to present repost form dialog at the given `location`.
// Delegate must call `completionHandler` with YES if form data should be
// reposted and with NO otherwise.
- (void)repostFormTabHelper:(RepostFormTabHelper*)helper
    presentRepostFormDialogForWebState:(web::WebState*)webState
                         dialogAtPoint:(CGPoint)location
                     completionHandler:(void (^)(BOOL))completionHandler;

// Asks the delegate to dismiss repost form dialog.
- (void)repostFormTabHelperDismissRepostFormDialog:
    (RepostFormTabHelper*)tabHelper;

@end

#endif  // IOS_CHROME_BROWSER_WEB_MODEL_REPOST_FORM_TAB_HELPER_DELEGATE_H_
