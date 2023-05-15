// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_PUBLIC_CWV_NAVIGATION_DELEGATE_H_
#define IOS_WEB_VIEW_PUBLIC_CWV_NAVIGATION_DELEGATE_H_

#import <Foundation/Foundation.h>

#import "cwv_export.h"
#import "cwv_navigation_type.h"

NS_ASSUME_NONNULL_BEGIN

@class CWVDownloadTask;
@class CWVLookalikeURLHandler;
@class CWVNavigationAction;
@class CWVNavigationResponse;
@class CWVSSLErrorHandler;
@class CWVUnsafeURLHandler;
@class CWVWebView;

typedef NS_ENUM(NSInteger, CWVNavigationActionPolicy) {
  // Cancel the navigation
  CWVNavigationActionPolicyCancel,
  // Allow the navigation to continue.
  CWVNavigationActionPolicyAllow,
};

typedef NS_ENUM(NSInteger, CWVNavigationResponsePolicy) {
  // Cancel the navigation
  CWVNavigationResponsePolicyCancel,
  // Allow the navigation to continue.
  CWVNavigationResponsePolicyAllow,
};

// Navigation delegate protocol for CWVWebViews.  Allows embedders to hook
// page loading and receive events for navigation.
@protocol CWVNavigationDelegate<NSObject>
@optional

// DEPRECATED: Use `-[CWVNavigationDelegate
// webView:decidePolicyForNavigationAction:decisionHandler:]` instead, this
// method will not work when recommended API is implemented.
//
// Asks delegate if WebView should start the load. WebView will load the request
// if this method is not implemented.
- (BOOL)webView:(CWVWebView*)webView
    shouldStartLoadWithRequest:(NSURLRequest*)request
                navigationType:(CWVNavigationType)navigationType;

// DEPRECATED: Use `-[CWVNavigationDelegate
// webView:decidePolicyForNavigationResponse:decisionHandler:]` instead, this
// method will not work when recommended API is implemented.
//
// Asks delegate if WebView should continue the load. WebView will load the
// response if this method is not implemented. `forMainFrame` indicates whether
// the frame being navigated is the main frame.
- (BOOL)webView:(CWVWebView*)webView
    shouldContinueLoadWithResponse:(NSURLResponse*)response
                      forMainFrame:(BOOL)forMainFrame;

// Decides whether to allow or cancel a navigation. WebView will load the
// response if this method is not implemented.
- (void)webView:(CWVWebView*)webView
    decidePolicyForNavigationAction:(CWVNavigationAction*)navigationAction
                    decisionHandler:
                        (void (^)(CWVNavigationActionPolicy))decisionHandler;

// Decides whether to allow or cancel a navigation after its response is known.
// WebView will load the response if this method is not implemented.
- (void)webView:(CWVWebView*)webView
    decidePolicyForNavigationResponse:(CWVNavigationResponse*)navigationResponse
                      decisionHandler:(void (^)(CWVNavigationResponsePolicy))
                                          decisionHandler;

// Notifies the delegate that main frame navigation has started.
// Deprecated, use |webViewDidStartNavigation| instead.
- (void)webViewDidStartProvisionalNavigation:(CWVWebView*)webView;

// Notifies the delegate that main frame navigation has started.
- (void)webViewDidStartNavigation:(CWVWebView*)webView;

// Notifies the delegate that response data started arriving for
// the main frame.
- (void)webViewDidCommitNavigation:(CWVWebView*)webView;

// Notifies the delegate that page load has succeeded.
- (void)webViewDidFinishNavigation:(CWVWebView*)webView;

// Notifies the delegate that page load has failed.
// is called instead of this method.
- (void)webView:(CWVWebView*)webView didFailNavigationWithError:(NSError*)error;

// Notifies the delegate that page load failed due to a SSL error.
// |handler| can be used to help with communicating the error to the user, and
// potentially override and ignore it.
- (void)webView:(CWVWebView*)webView
    handleSSLErrorWithHandler:(CWVSSLErrorHandler*)handler;

// Notifies the delegate of an attempt to load a lookalike URL.
// |handler| used to communicate the attempt to the user, and allow them to
// override if desired.
// If this method is not implemented, the lookalike URL will load normally.
- (void)webView:(CWVWebView*)webView
    handleLookalikeURLWithHandler:(CWVLookalikeURLHandler*)handler;

// Notifies the delegate of an attempt to load an unsafe URL.
// |handler| used to communicate the attempt to the user, and allow them to
// override if desired.
// If this method is not implemented, the URL will continue to load normally.
- (void)webView:(CWVWebView*)webView
    handleUnsafeURLWithHandler:(CWVUnsafeURLHandler*)handler;

// Called when the web view requests to start downloading a file.
//
// The delegate can either:
//   - call [task startDownloadToLocalFileWithPath:] to start download
//   immediately. - call [task startDownloadToLocalFileWithPath:] later. - do
//   nothing in the method, to ignore the request.
// It does nothing when the method is not implemented.
//
// The delegate must retain a strong reference to |task| until it completes
// downloading or is cancelled. Otherwise it is deallocated immediately after
// exiting this method.
- (void)webView:(CWVWebView*)webView
    didRequestDownloadWithTask:(CWVDownloadTask*)task;

// Notifies the delegate that web view process was terminated
// (usually by crashing, though possibly by other means).
- (void)webViewWebContentProcessDidTerminate:(CWVWebView*)webView;

@end

NS_ASSUME_NONNULL_END

#endif  // IOS_WEB_VIEW_PUBLIC_CWV_NAVIGATION_DELEGATE_H_
