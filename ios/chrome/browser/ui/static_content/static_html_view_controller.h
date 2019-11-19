// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_STATIC_CONTENT_STATIC_HTML_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_STATIC_CONTENT_STATIC_HTML_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>
#import <WebKit/WebKit.h>

#include "url/gurl.h"

@protocol CRWNativeContentDelegate;

namespace web {
class BrowserState;
}

// Callback for the HtmlGenerator protocol.
typedef void (^HtmlCallback)(NSString*);

// An object that can generate HTML to be displayed.
@protocol HtmlGenerator<NSObject>
// Generates the HTML to be displayed. This is called on the main thread. If
// heavy work must be done, it should be done asynchronously.
- (void)generateHtml:(HtmlCallback)callback;
@end

// HtmlGenerator that get HTML from a IDR_ id.
@interface IdrHtmlGenerator : NSObject<HtmlGenerator> {
 @private
  // The IDR_ constant.
  int resourceId_;
  // The encoding of the resource.
  NSStringEncoding encoding_;
}

// Init with the IDR_ constant that point to a UTF8 string.
- (id)initWithResourceId:(int)resourceId;

// Init with the IDR_ constant that point to a string with the given encoding.
- (id)initWithResourceId:(int)resourceId encoding:(NSStringEncoding)encoding;

@end

// Displays html pages either located in the application bundle, the application
// data directory, or obtained by a |HtmlGenerator|.
@interface StaticHtmlViewController : NSObject

// The web view that is displaying the content. It is lazily created.
@property(nonatomic, readonly, weak) WKWebView* webView;

// Returns the web controller scrollview.
- (UIScrollView*)scrollView;

// Initialization method. |resource| is the location of the static page to
// display, relative to the root of the application bundle. |browserState| is
// the user browser state to use for loading resources and must not be null.
- (instancetype)initWithResource:(NSString*)resource
                    browserState:(web::BrowserState*)browserState;

// Initialization method. |generator| will produce the html to display. Its
// generation method will be called each time reload is called. |browserState|
// is the user browser state to use for loading resources and must not be null.
// |StaticHtmlViewController| retains |generator|.
- (instancetype)initWithGenerator:(id<HtmlGenerator>)generator
                     browserState:(web::BrowserState*)browserState;

// Initialization method. |fileURL| is the location of the page in the
// application data directory. |resourcesRoot| is the root directory where the
// page pointed by |fileURL| can find its resources. Web view will get read
// access to the |resourcesRoot| directory. |fileURL| must be contained in
// |resourcesRoot|. |browserState| is the user browser state to use for
// loading resources and must not be null.
- (instancetype)initWithFileURL:(const GURL&)fileURL
        allowingReadAccessToURL:(const GURL&)resourcesRoot
                   browserState:(web::BrowserState*)browserState;

// Asynchronously executes the supplied JavaScript. Calls |completionHandler|
// with results of the execution. If the controller cannot execute JS at the
// moment, |completionHandler| is called with an NSError. The
// |completionHandler| can be nil.
- (void)executeJavaScript:(NSString*)script
        completionHandler:(void (^)(id, NSError*))completionHandler;

// The web page title. Will return nil if not available.
- (NSString*)title;

// Reload the content of the web page.
- (void)reload;

// Causes the page to start loading immediately if there is a pending load;
// normally if the web view has been paged out for memory reasons, loads are
// started lazily the next time the view is displayed. This can be called to
// bypass the lazy behavior. This is equivalent to calling -webView, but should
// be used when deliberately pre-triggering a load without displaying.
- (void)triggerPendingLoad;

// Returns YES if there is currently a live view in the tab (e.g., the view
// hasn't been discarded due to low memory).
// NOTE: This should be used for metrics-gathering only; for any other purpose
// callers should not know or care whether the view is live.
- (BOOL)isViewAlive;

// Set a |CRWNativeContentDelegate| that will be notified each time the title of
// the page changes.
- (void)setDelegate:(id<CRWNativeContentDelegate>)delegate;

// Enables or disables the scrolling in the native view.
- (void)setScrollEnabled:(BOOL)enabled;

@end

#endif  // IOS_CHROME_BROWSER_UI_STATIC_CONTENT_STATIC_HTML_VIEW_CONTROLLER_H_
