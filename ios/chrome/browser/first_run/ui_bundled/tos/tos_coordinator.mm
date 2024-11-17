// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/first_run/ui_bundled/tos/tos_coordinator.h"

#import <WebKit/WebKit.h>

#import "base/apple/bundle_locations.h"
#import "base/strings/sys_string_conversions.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/first_run/ui_bundled/tos/tos_view_controller.h"
#import "ios/chrome/browser/shared/coordinator/alert/alert_coordinator.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/tos_commands.h"
#import "ios/chrome/browser/shared/ui/util/terms_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/common/web_view_creation_util.h"
#import "net/base/apple/url_conversions.h"
#import "ui/base/l10n/l10n_util.h"

@interface TOSCoordinator () <UIAdaptivePresentationControllerDelegate,
                              WKNavigationDelegate>

@property(nonatomic, strong) TOSViewController* viewController;

@end

@implementation TOSCoordinator {
  AlertCoordinator* _alertCoordinator;
}

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
  NSURL* TOSURL =
      net::NSURLWithGURL(GetUnifiedTermsOfServiceURL(/*embbeded=*/true));
  DCHECK(TOSURL);

  // Create web view.
  WKWebView* webView = web::BuildWKWebView(self.viewController.view.bounds,
                                           self.browser->GetProfile());
  webView.navigationDelegate = self;

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
  [self closeTOSPage];
}

#pragma mark - WKNavigationDelegate

// In case the Term of Service page can’t be loaded
// show an Alert stating "No Internet".
- (void)webView:(WKWebView*)webView
    didFailProvisionalNavigation:(WKNavigation*)navigation
                       withError:(NSError*)error {
  [self failedToLoad];
}

// As long as ToS don’t include external files (e.g. css or js),
// this method should never be called.
- (void)webView:(WKWebView*)webView
    didFailNavigation:(WKNavigation*)navigation
            withError:(NSError*)error {
  [self failedToLoad];
}

#pragma mark - Private

// If the page can’t be loaded, show an Alert stating "No Internet".
- (void)failedToLoad {
  if (_alertCoordinator) {
    // If the alert is already displayed, don’t display a second one.
    // It should never occurs as long as the ToS don’t include external files.
    return;
  }
  NSString* alertMessage =
      l10n_util::GetNSString(IDS_ERRORPAGES_HEADING_INTERNET_DISCONNECTED);
  _alertCoordinator =
      [[AlertCoordinator alloc] initWithBaseViewController:self.viewController
                                                   browser:self.browser
                                                     title:alertMessage
                                                   message:nil];

  __weak __typeof(self) weakSelf = self;
  [_alertCoordinator addItemWithTitle:l10n_util::GetNSString(IDS_OK)
                               action:^{
                                 [weakSelf stopAlertAndTos];
                               }
                                style:UIAlertActionStyleDefault];

  [_alertCoordinator start];
}

- (void)stopAlertAndTos {
  [_alertCoordinator stop];
  _alertCoordinator = nil;
  [self closeTOSPage];
}

- (void)closeTOSPage {
  id<TOSCommands> handler =
      HandlerForProtocol(self.browser->GetCommandDispatcher(), TOSCommands);
  [handler closeTOSPage];
}

@end
