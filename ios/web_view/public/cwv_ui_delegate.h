// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_PUBLIC_CWV_UI_DELEGATE_H_
#define IOS_WEB_VIEW_PUBLIC_CWV_UI_DELEGATE_H_

#import <CoreGraphics/CoreGraphics.h>
#import <UIKit/UIKit.h>

#import "cwv_export.h"

NS_ASSUME_NONNULL_BEGIN

@class CWVFavicon;
@class CWVHTMLElement;
@class CWVPreviewElementInfo;
@class CWVWebView;
@class CWVWebViewConfiguration;
@class CWVNavigationAction;

typedef NS_ENUM(NSInteger, CWVPermissionDecision) {
  CWVPermissionDecisionPrompt,
  CWVPermissionDecisionGrant,
  CWVPermissionDecisionDeny,
};

typedef NS_ENUM(NSInteger, CWVMediaCaptureType) {
  CWVMediaCaptureTypeCamera,
  CWVMediaCaptureTypeMicrophone,
  CWVMediaCaptureTypeCameraAndMicrophone,
};

// UI delegate interface for a CWVWebView.  Embedders can implement the
// functions in order to customize library behavior.
CWV_EXPORT
@protocol CWVUIDelegate<NSObject>

@optional
// Instructs the delegate to create a new browsing window (f.e. in response to
// window.open JavaScript call). Page will not open a window and report a
// failure (f.e. return null from window.open) if this method returns nil or is
// not implemented. This method can not return |webView|.
- (nullable CWVWebView*)webView:(CWVWebView*)webView
    createWebViewWithConfiguration:(CWVWebViewConfiguration*)configuration
               forNavigationAction:(CWVNavigationAction*)action;

// Instructs the delegate to close |webView|. Called only for windows opened by
// DOM.
- (void)webViewDidClose:(CWVWebView*)webView;

// Asks delegate grant or deny permission for microphone audio and camera video
// access. If the delegate doesn't implement this method, the `webView` would
// show the default prompt that asks for permissions.
- (void)webView:(CWVWebView*)webView
    requestMediaCapturePermissionForType:(CWVMediaCaptureType)type
                         decisionHandler:
                             (void (^)(CWVPermissionDecision decision))
                                 decisionHandler;

// Instructs the delegate to show UI in response to window.alert JavaScript
// call.
- (void)webView:(CWVWebView*)webView
    runJavaScriptAlertPanelWithMessage:(NSString*)message
                               pageURL:(NSURL*)URL
                     completionHandler:(void (^)(void))completionHandler;

// Instructs the delegate to show UI in response to window.confirm JavaScript
// call.
- (void)webView:(CWVWebView*)webView
    runJavaScriptConfirmPanelWithMessage:(NSString*)message
                                 pageURL:(NSURL*)URL
                       completionHandler:(void (^)(BOOL))completionHandler;

// Instructs the delegate to show UI in response to window.prompt JavaScript
// call.
- (void)webView:(CWVWebView*)webView
    runJavaScriptTextInputPanelWithPrompt:(NSString*)prompt
                              defaultText:(NSString*)defaultText
                                  pageURL:(NSURL*)URL
                        completionHandler:
                            (void (^)(NSString* _Nullable))completionHandler;

// Called when favicons become available in the current page.
- (void)webView:(CWVWebView*)webView
    didLoadFavicons:(NSArray<CWVFavicon*>*)favIcons;

// Equivalent of -[WKUIDelegate
// webView:contextMenuConfigurationForElement:completionHandler:].
- (void)webView:(CWVWebView*)webView
    contextMenuConfigurationForElement:(CWVHTMLElement*)element
                     completionHandler:
                         (void (^)(UIContextMenuConfiguration* _Nullable))
                             completionHandler;

// Equivalent of -[WKUIDelegate
// webView:contextMenuForElement:willCommitWithAnimator:].
- (void)webView:(CWVWebView*)webView
    contextMenuWillCommitWithAnimator:
           (id<UIContextMenuInteractionCommitAnimating>)animator;

@end

NS_ASSUME_NONNULL_END

#endif  // IOS_WEB_VIEW_PUBLIC_CWV_UI_DELEGATE_H_
