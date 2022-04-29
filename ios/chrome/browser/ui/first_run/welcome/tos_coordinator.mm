// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/first_run/welcome/tos_coordinator.h"

#import <WebKit/WebKit.h>

#include "base/mac/bundle_locations.h"
#include "base/strings/sys_string_conversions.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/commands/tos_commands.h"
#import "ios/chrome/browser/ui/first_run/fre_field_trial.h"
#import "ios/chrome/browser/ui/first_run/welcome/tos_view_controller.h"
#include "ios/chrome/browser/ui/util/terms_util.h"
#import "ios/web/common/web_view_creation_util.h"
#import "net/base/mac/url_conversions.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface TOSCoordinator () <UIAdaptivePresentationControllerDelegate>

@property(nonatomic, strong) TOSViewController* viewController;

@end

@implementation TOSCoordinator

- (void)start {
  id<TOSCommands> handler =
      HandlerForProtocol(self.browser->GetCommandDispatcher(), TOSCommands);
  self.viewController = [[TOSViewController alloc]
      initWithContentView:[self newWebViewDisplayingTOS]
                  handler:handler];
  UINavigationController* navigationController = [[UINavigationController alloc]
      initWithRootViewController:self.viewController];
  navigationController.presentationController.delegate = self;

  [self.baseViewController presentViewController:navigationController
                                        animated:YES
                                      completion:nil];
}

- (void)stop {
  [self.viewController.presentingViewController
      dismissViewControllerAnimated:YES
                         completion:nil];
}

#pragma mark - Private

// Creates a WKWebView and load the terms of services html page in it.
- (WKWebView*)newWebViewDisplayingTOS {
  NSURL* TOSURL = nil;
  switch (fre_field_trial::GetNewMobileIdentityConsistencyFRE()) {
    case NewMobileIdentityConsistencyFRE::kTwoSteps:
    case NewMobileIdentityConsistencyFRE::kThreeSteps:
    case NewMobileIdentityConsistencyFRE::kUMADialog: {
      TOSURL =
          net::NSURLWithGURL(GetUnifiedTermsOfServiceURL(/*embbeded=*/true));
      break;
    }
    case NewMobileIdentityConsistencyFRE::kOld: {
      // Craft ToS path.
      std::string TOS = GetTermsOfServicePath();
      NSString* path = [[base::mac::FrameworkBundle() bundlePath]
          stringByAppendingPathComponent:base::SysUTF8ToNSString(TOS)];
      NSURLComponents* components = [[NSURLComponents alloc] init];
      [components setScheme:@"file"];
      [components setHost:@""];
      [components setPath:path];
      TOSURL = [components URL];
      break;
    }
  }
  DCHECK(TOSURL);

  // Create web view.
  WKWebView* webView = web::BuildWKWebView(self.viewController.view.bounds,
                                           self.browser->GetBrowserState());

  // Loads terms of service into the web view.
  NSURLRequest* request =
      [[NSURLRequest alloc] initWithURL:TOSURL
                            cachePolicy:NSURLRequestReloadIgnoringLocalCacheData
                        timeoutInterval:60.0];
  [webView loadRequest:request];
  [webView setOpaque:NO];

  return webView;
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  id<TOSCommands> handler =
      HandlerForProtocol(self.browser->GetCommandDispatcher(), TOSCommands);
  [handler hideTOSPage];
}

@end
