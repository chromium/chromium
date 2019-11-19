// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ui/static_content/static_html_view_controller.h"

#import <WebKit/WebKit.h>

#include <stdlib.h>

#include "base/logging.h"
#include "base/mac/bundle_locations.h"
#include "base/mac/foundation_util.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/url_loading/url_loading_params.h"
#import "ios/chrome/browser/url_loading/url_loading_service.h"
#import "ios/chrome/browser/url_loading/url_loading_service_factory.h"
#import "ios/web/common/web_view_creation_util.h"
#import "ios/web/public/deprecated/crw_context_menu_delegate.h"
#import "ios/web/public/deprecated/crw_native_content.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "net/base/mac/url_conversions.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/resource/resource_bundle.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation IdrHtmlGenerator
- (id)initWithResourceId:(int)resourceId encoding:(NSStringEncoding)encoding {
  if ((self = [super init])) {
    resourceId_ = resourceId;
    encoding_ = encoding;
  }
  return self;
}

- (id)initWithResourceId:(int)resourceId {
  return [self initWithResourceId:resourceId encoding:NSUTF8StringEncoding];
}

- (void)generateHtml:(HtmlCallback)callback {
  base::StringPiece html(
      ui::ResourceBundle::GetSharedInstance().GetRawDataResource(resourceId_));
  NSString* result = [[NSString alloc] initWithBytes:html.data()
                                              length:html.size()
                                            encoding:encoding_];
  callback(result);
}
@end

@interface StaticHtmlViewController ()<CRWContextMenuDelegate,
                                       WKNavigationDelegate> {
 @private
  // The URL that will be passed to the web page when loading.
  // If the page is displaying a local HTML file, it contains the file URL to
  // the file.
  // If the page is a generated HTML, it contains a random resource URL.
  NSURL* resourceUrl_;

  // If the view is displaying a local file, contains the URL of the root
  // directory containing all resources needed by the page.
  // The web view will get access to this page.
  NSURL* resourcesRootDirectory_;

  // If the view is displaying a resources, contains the name of the resource.
  NSString* resource_;

  // If the view displayes a generated HTML, contains the |HtmlGenerator| to
  // generate it.
  id<HtmlGenerator> generator_;

  // Browser state associated with the view controller, used to create the
  // WKWebView.
  web::BrowserState* browserState_;  // Weak.

  // The web view that is used to display the content.
  WKWebView* webView_;

  // The delegate of the native content.
  __weak id<CRWNativeContentDelegate> delegate_;
}

// Returns the URL of the static page to display.
- (NSURL*)resourceURL;
// Ensures that webView_ has been created, creating it if necessary.
- (void)ensureWebViewCreated;
// Determines if the page load should begin based on the current |resourceURL|
// and if the request is issued by the main frame (|fromMainFrame|).
- (BOOL)shouldStartLoadWithRequest:(NSURLRequest*)request
                     fromMainFrame:(BOOL)fromMainFrame;
@end

@implementation StaticHtmlViewController

- (instancetype)initWithResource:(NSString*)resource
                    browserState:(web::BrowserState*)browserState {
  DCHECK(resource);
  DCHECK(browserState);
  if ((self = [super init])) {
    resource_ = [resource copy];
    browserState_ = browserState;
  }
  return self;
}

- (instancetype)initWithGenerator:(id<HtmlGenerator>)generator
                     browserState:(web::BrowserState*)browserState {
  DCHECK(generator);
  DCHECK(browserState);
  if ((self = [super init])) {
    generator_ = generator;
    browserState_ = browserState;
  }
  return self;
}

- (instancetype)initWithFileURL:(const GURL&)URL
        allowingReadAccessToURL:(const GURL&)resourcesRoot
                   browserState:(web::BrowserState*)browserState {
  DCHECK(URL.is_valid());
  DCHECK(URL.SchemeIsFile());
  DCHECK(browserState);
  if ((self = [super init])) {
    resourceUrl_ = net::NSURLWithGURL(URL);
    resourcesRootDirectory_ = net::NSURLWithGURL(resourcesRoot);
    browserState_ = browserState;
  }
  return self;
}

- (void)dealloc {
  [webView_ removeObserver:self forKeyPath:@"title"];
}

- (void)executeJavaScript:(NSString*)script
        completionHandler:(void (^)(id, NSError*))handler {
  [webView_ evaluateJavaScript:script completionHandler:handler];
}

- (UIScrollView*)scrollView {
  return [[self webView] scrollView];
}

- (WKWebView*)webView {
  [self ensureWebViewCreated];
  return webView_;
}

- (BOOL)isViewAlive {
  return webView_ != nil;
}

- (NSString*)title {
  return [[self webView] title];
}

- (void)reload {
  if (!generator_) {
    [webView_ reload];
  } else {
    NSURL* resourceURL = [self resourceURL];
    [generator_ generateHtml:^(NSString* HTML) {
      [webView_ loadHTMLString:HTML baseURL:resourceURL];
    }];
  }
}

- (void)triggerPendingLoad {
  // Ensure that the web view is created, which triggers loading.
  [self ensureWebViewCreated];
}

- (void)setDelegate:(id<CRWNativeContentDelegate>)delegate {
  delegate_ = delegate;
}

- (void)setScrollEnabled:(BOOL)enabled {
  [[self scrollView] setScrollEnabled:enabled];
}

#pragma mark -
#pragma mark WKNavigationDelegate implementation

- (void)webView:(WKWebView*)webView
    decidePolicyForNavigationAction:(WKNavigationAction*)navigationAction
                    decisionHandler:
                        (void (^)(WKNavigationActionPolicy))decisionHandler {
  decisionHandler(
      [self
          shouldStartLoadWithRequest:navigationAction.request
                       fromMainFrame:[navigationAction.targetFrame isMainFrame]]
          ? WKNavigationActionPolicyAllow
          : WKNavigationActionPolicyCancel);
}

#pragma mark -
#pragma mark CRWContextMenuDelegate implementation

- (void)webView:(WKWebView*)webView
    handleContextMenu:(const web::ContextMenuParams&)params {
  if ([delegate_
          respondsToSelector:@selector(nativeContent:handleContextMenu:)]) {
    [delegate_ nativeContent:self handleContextMenu:params];
  }
}

#pragma mark -
#pragma mark KVO callback

- (void)observeValueForKeyPath:(NSString*)keyPath
                      ofObject:(id)object
                        change:(NSDictionary*)change
                       context:(void*)context {
  DCHECK([keyPath isEqualToString:@"title"]);
  if ([delegate_ respondsToSelector:@selector(nativeContent:titleDidChange:)]) {
    // WKWebView's |title| changes to nil when its web process crashes.
    if ([webView_ title])
      [delegate_ nativeContent:self titleDidChange:[webView_ title]];
  }
}

#pragma mark -
#pragma mark Private

- (BOOL)shouldStartLoadWithRequest:(NSURLRequest*)request
                     fromMainFrame:(BOOL)fromMainFrame {
  // Only allow displaying the URL which correspond to the authorized resource.
  if ([[request URL] isEqual:[self resourceURL]])
    return YES;

  // All other navigation URLs will be loaded by url loading service
  // if they are issued by the main frame.
  if (fromMainFrame) {
    dispatch_async(dispatch_get_main_queue(), ^{
      ios::ChromeBrowserState* chrome_browser_state =
          ios::ChromeBrowserState::FromBrowserState(browserState_);
      UrlLoadingServiceFactory::GetForBrowserState(chrome_browser_state)
          ->Load(
              UrlLoadParams::InCurrentTab(net::GURLWithNSURL([request URL])));
    });
  }
  return NO;
}

- (NSURL*)resourceURL {
  if (resourceUrl_)
    return resourceUrl_;

  DCHECK(resource_ || generator_);
  NSString* path = nil;
  if (resource_) {
    NSBundle* bundle = base::mac::FrameworkBundle();
    NSString* bundlePath = [bundle bundlePath];
    path = [bundlePath stringByAppendingPathComponent:resource_];
  } else {
    // Generate a random resource URL to whitelist the load in
    // |webView:shouldStartLoadWithRequest:navigationType:| method.
    path = [NSString stringWithFormat:@"/whitelist%u%u%u%u", arc4random(),
                                      arc4random(), arc4random(), arc4random()];
  }
  DCHECK(path);
  // Necessary because the |fileURLWithPath:| method adds a localhost in the
  // URL, and this prevents the URL from being comparable with the ones that
  // UIWebView uses when calling the delegate.
  NSURLComponents* components = [[NSURLComponents alloc] init];
  [components setScheme:@"file"];
  [components setHost:@""];
  [components setPath:path];
  resourceUrl_ = [components URL];
  resourcesRootDirectory_ = [resourceUrl_ copy];
  return resourceUrl_;
}

- (void)ensureWebViewCreated {
  if (!webView_) {
    WKWebView* webView = web::BuildWKWebViewWithCustomContextMenu(
        CGRectZero, browserState_, self);
    [webView addObserver:self forKeyPath:@"title" options:0 context:nullptr];
    [webView setAutoresizingMask:UIViewAutoresizingFlexibleWidth |
                                 UIViewAutoresizingFlexibleHeight];
    [self loadWebViewContents:webView];
    [webView setNavigationDelegate:self];
    webView_ = webView;
  }
}

- (void)loadWebViewContents:(WKWebView*)webView {
  if (!generator_) {
    [webView loadFileURL:[self resourceURL]
        allowingReadAccessToURL:resourcesRootDirectory_];
  } else {
    NSURL* resourceURL = [self resourceURL];
    [generator_ generateHtml:^(NSString* HTML) {
      [webView loadHTMLString:HTML baseURL:resourceURL];
    }];
  }
}

@end
