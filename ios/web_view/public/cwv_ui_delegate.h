// Copyright 2017 The Chromium Authors. All rights reserved.
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

// Instructs the delegate to present context menu in response to user’s long
// press gesture at |location| in |view| coordinate space. |element| is an HTML
// element which received the gesture. If this method is not implemented, no
// context menu will be displayed.
- (void)webView:(CWVWebView*)webView
    runContextMenuWithTitle:(NSString*)menuTitle
             forHTMLElement:(CWVHTMLElement*)element
                     inView:(UIView*)view
        userGestureLocation:(CGPoint)location;

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
                            (void (^)(NSString*))completionHandler;

// Determines whether the given link with |linkURL| should show a preview on
// force touch. Return value NO is assumed if the method is not implemented.
- (BOOL)webView:(CWVWebView*)webView
    shouldPreviewElement:(CWVPreviewElementInfo*)elementInfo;

// Called when the user performs a peek action on a link with |linkURL| with
// force touch. Returns a view controller shown as a pop-up. Uses Webkit's
// default preview behavior when it returns nil.
- (nullable UIViewController*)webView:(CWVWebView*)webView
    previewingViewControllerForElement:(CWVPreviewElementInfo*)elementInfo;

// Instructs the delegate to display |previewingViewController| inside the app,
// in response to the user's pop action on the preview on force touch.
- (void)webView:(CWVWebView*)webView
    commitPreviewingViewController:(UIViewController*)previewingViewController;

// Called when favicons become available in the current page.
- (void)webView:(CWVWebView*)webView
    didLoadFavicons:(NSArray<CWVFavicon*>*)favIcons;

@end

NS_ASSUME_NONNULL_END

#endif  // IOS_WEB_VIEW_PUBLIC_CWV_UI_DELEGATE_H_
