// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/first_run/static_file_view_controller.h"

#import <WebKit/WebKit.h>

#import "base/check.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/ui/icons/chrome_icon.h"
#import "ios/chrome/browser/ui/util/rtl_geometry.h"
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
    _browserState = browserState;
    _URL = URL;
  }
  return self;
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

  // Create a custom Back bar button item.
  self.navigationItem.leftBarButtonItem =
      [ChromeIcon templateBarButtonItemWithImage:[ChromeIcon backIcon]
                                          target:self
                                          action:@selector(back)];
  [self.navigationController setNavigationBarHidden:NO animated:YES];
}

#pragma mark - Actions

- (void)back {
  [self.navigationController popViewControllerAnimated:YES];
  [self.navigationController setNavigationBarHidden:YES animated:YES];
}

@end
