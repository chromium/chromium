// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ui/first_run/static_file_view_controller.h"

#import <WebKit/WebKit.h>

#import <MaterialComponents/MDCAppBarViewController.h>

#include "base/check.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/ui/icons/chrome_icon.h"
#import "ios/chrome/browser/ui/material_components/utils.h"
#include "ios/chrome/browser/ui/util/rtl_geometry.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/web/common/web_view_creation_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface StaticFileViewController () <UIScrollViewDelegate> {
  ChromeBrowserState* _browserState;  // weak
  NSURL* _URL;
  // YES if the header has been configured for RTL.
  BOOL _headerLaidOutForRTL;
  // The web view used to display the static content.
  WKWebView* _webView;
  // The header.
  MDCAppBarViewController* _appBarViewController;
}

// Called when the back button is pressed.
- (void)back;

@end

@implementation StaticFileViewController

- (instancetype)initWithBrowserState:(ChromeBrowserState*)browserState
                                 URL:(NSURL*)URL {
  DCHECK(browserState);
  DCHECK(URL);
  self = [super init];
  if (self) {
    _appBarViewController = [[MDCAppBarViewController alloc] init];
    _browserState = browserState;
    _URL = URL;
  }
  return self;
}

- (void)dealloc {
  [_webView scrollView].delegate = nil;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  _webView = web::BuildWKWebView(self.view.bounds, _browserState);
  [_webView setAutoresizingMask:UIViewAutoresizingFlexibleWidth |
                                UIViewAutoresizingFlexibleHeight];

  // Loads terms of service into the web view.
  NSURLRequest* request =
      [[NSURLRequest alloc] initWithURL:_URL
                            cachePolicy:NSURLRequestReloadIgnoringLocalCacheData
                        timeoutInterval:60.0];
  [_webView loadRequest:request];
  [_webView setBackgroundColor:[UIColor colorNamed:kPrimaryBackgroundColor]];
  [self.view addSubview:_webView];

  ConfigureAppBarViewControllerWithCardStyle(_appBarViewController);
  _appBarViewController.headerView.trackingScrollView = [_webView scrollView];
  [_webView scrollView].delegate = _appBarViewController;

  // Add the app bar at the end.
  [self addChildViewController:_appBarViewController];
  // Match the width of the parent view.
  CGRect frame = _appBarViewController.view.frame;
  frame.origin.x = 0;
  frame.size.width =
      _appBarViewController.parentViewController.view.bounds.size.width;
  _appBarViewController.view.frame = frame;
  [self.view addSubview:_appBarViewController.view];
  [_appBarViewController didMoveToParentViewController:self];

  // Create a custom Back bar button item, as Material Navigation Bar deprecated
  // the back arrow with a shaft.
  self.navigationItem.leftBarButtonItem =
      [ChromeIcon templateBarButtonItemWithImage:[ChromeIcon backIcon]
                                          target:self
                                          action:@selector(back)];
}

#pragma mark - Actions

- (void)back {
  [self.navigationController popViewControllerAnimated:YES];
}

@end
