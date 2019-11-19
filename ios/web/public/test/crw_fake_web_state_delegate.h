// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_TEST_CRW_FAKE_WEB_STATE_DELEGATE_H_
#define IOS_WEB_PUBLIC_TEST_CRW_FAKE_WEB_STATE_DELEGATE_H_

#import "ios/web/public/web_state_delegate_bridge.h"

// Stub implementation for CRWWebStateDelegate protocol.
@interface CRWFakeWebStateDelegate : NSObject <CRWWebStateDelegate>

// web::WebState::OpenURLParams in |webState:openURLWithParams:| call.
@property(nonatomic, readonly)
    const web::WebState::OpenURLParams* openURLParams;
// web::WebState received in delegate method calls.
@property(nonatomic, readonly) web::WebState* webState;
// ContextMenuParams reveived in |webState:handleContextMenu:| call.
// nullptr if that delegate method was not called.
@property(nonatomic, readonly) web::ContextMenuParams* contextMenuParams;
// Whether |webState:createNewWebStateForURL:openerURL:initiatedByUser:| has
// been called or not.
@property(nonatomic, readonly) BOOL webStateCreationRequested;
// Whether |closeWebState:| has been called or not.
@property(nonatomic, readonly) BOOL webStateClosingRequested;
// Whether |webState:runRepostFormDialogWithCompletionHandler:| has been called
// or not.
@property(nonatomic, readonly) BOOL repostFormWarningRequested;
// Whether |javaScriptDialogPresenterForWebState:| has been called or not.
@property(nonatomic, readonly) BOOL javaScriptDialogPresenterRequested;
// Whether |webState:didRequestHTTPAuthForProtectionSpace:...| has been called
// or not.
@property(nonatomic, readonly) BOOL authenticationRequested;
// Specifies the return value of |webState:shouldPreviewLinkWithURL:|.
@property(nonatomic) BOOL shouldPreviewLinkWithURLReturnValue;
// linkURL received in |webState:shouldPreviewLinkWithURL:| and
// |webState:previewingViewControllerForLinkWithURL:| calls.
@property(nonatomic, readonly) GURL linkURL;
// Specifies the return value of
// |webState:previewingViewControllerForLinkWithURL:|.
@property(nonatomic)
    UIViewController* previewingViewControllerForLinkWithURLReturnValue;
// previewingViewController received in
// |webState:commitPreviewingViewController:| call.
@property(nonatomic, readonly) UIViewController* previewingViewController;
// Whether |webState:commitPreviewingViewController:| has been called or not.
@property(nonatomic, readonly) BOOL commitPreviewingViewControllerRequested;
// Specifies the return value of |isAppLaunchingAllowedForWebState:|.
@property(nonatomic) BOOL isAppLaunchingAllowedForWebStateReturnValue;

@end

#endif  // IOS_WEB_PUBLIC_TEST_CRW_FAKE_WEB_STATE_DELEGATE_H_
