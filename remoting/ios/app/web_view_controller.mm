// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "remoting/ios/app/web_view_controller.h"

#import <WebKit/WebKit.h>

#import "remoting/ios/app/remoting_theme.h"
#import "remoting/ios/app/view_utils.h"

@interface WebViewController ()<WKUIDelegate> {
  NSString* _urlString;
}
@end

@implementation WebViewController

- (instancetype)initWithUrl:(NSString*)url title:(NSString*)title {
  if ((self = [super init])) {
    _urlString = url;
    self.title = title;
  }
  return self;
}

#pragma mark - UIViewController

- (void)loadView {
  WKWebView* webView =
      [[WKWebView alloc] initWithFrame:CGRectZero
                         configuration:[[WKWebViewConfiguration alloc] init]];
  NSURLRequest* request =
      [NSURLRequest requestWithURL:[NSURL URLWithString:_urlString]];
  [webView loadRequest:request];
  webView.UIDelegate = self;
  self.view = webView;
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];

  // Show a close button if it is the first view in the navigation stack.
  UIViewController* firstViewController =
      [self.navigationController.viewControllers firstObject];
  if ((firstViewController == self ||
       firstViewController == self.parentViewController) &&
      !self.navigationItem.leftBarButtonItem &&
      !self.navigationItem.leftBarButtonItems.count) {
    self.navigationItem.leftBarButtonItem =
        [[UIBarButtonItem alloc] initWithImage:RemotingTheme.closeIcon
                                         style:UIBarButtonItemStylePlain
                                        target:self
                                        action:@selector(didTapClose:)];
    remoting::SetAccessibilityInfoFromImage(
        self.navigationItem.leftBarButtonItem);
  }
}

#pragma mark - WKUIDelegate

- (WKWebView*)webView:(WKWebView*)webView
    createWebViewWithConfiguration:(WKWebViewConfiguration*)configuration
               forNavigationAction:(WKNavigationAction*)navigationAction
                    windowFeatures:(WKWindowFeatures*)windowFeatures {
  // This is called when the web view needs to open a webpage in new window,
  // i.e. target="_blank".
  [UIApplication.sharedApplication openURL:navigationAction.request.URL
                                   options:@{}
                         completionHandler:nil];

  return nil;
}

#pragma mark - Private

- (void)didTapClose:(id)button {
  [self dismissViewControllerAnimated:YES completion:nil];
}

@end
