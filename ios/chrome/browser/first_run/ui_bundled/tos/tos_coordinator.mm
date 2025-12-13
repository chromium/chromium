// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/first_run/ui_bundled/tos/tos_coordinator.h"

#import <WebKit/WebKit.h>

#import "base/apple/bundle_locations.h"
#import "base/strings/sys_string_conversions.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/first_run/ui_bundled/tos/tos_view_controller.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/ui/util/terms_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/common/web_view_creation_util.h"
#import "ios/web/web_state/crw_web_view.h"
#import "net/base/apple/url_conversions.h"
#import "ui/base/l10n/l10n_util.h"

@interface TOSCoordinator () <TOSViewControllerPresentationDelegate,
                              UIAdaptivePresentationControllerDelegate,
                              WKNavigationDelegate>

@end

@implementation TOSCoordinator {
  UIAlertController* _alertController;
  TOSViewController* _viewController;
  UINavigationController* _navigationController;
}

- (void)start {
  _viewController = [[TOSViewController alloc]
      initWithContentView:[self newWebViewDisplayingTOS]];
  _viewController.delegate = self;
  _navigationController = [[UINavigationController alloc]
      initWithRootViewController:_viewController];
  _navigationController.presentationController.delegate = self;

  [self.baseViewController presentViewController:_navigationController
                                        animated:YES
                                      completion:nil];
}

- (void)stop {
  [_viewController dismissViewControllerAnimated:YES completion:nil];
  _viewController.delegate = nil;
  _viewController = nil;
  _navigationController.presentationController.delegate = nil;
  _navigationController = nil;
}

#pragma mark - Private

// Creates a WKWebView and load the terms of services html page in it.
- (WKWebView*)newWebViewDisplayingTOS {
  NSURL* TOSURL =
      net::NSURLWithGURL(GetUnifiedTermsOfServiceURL(/*embbeded=*/true));
  DCHECK(TOSURL);

  // Create web view.
  WKWebView* webView =
      web::BuildWKWebView(_viewController.view.bounds, self.profile);
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
  if (_alertController) {
    // If the alert is already displayed, don’t display a second one.
    // It should never occurs as long as the ToS don’t include external files.
    return;
  }
  NSString* alertMessage =
      l10n_util::GetNSString(IDS_ERRORPAGES_HEADING_INTERNET_DISCONNECTED);
  _alertController =
      [UIAlertController alertControllerWithTitle:alertMessage
                                          message:nil
                                   preferredStyle:UIAlertControllerStyleAlert];

  __weak __typeof(self) weakSelf = self;
  UIAlertAction* okAction =
      [UIAlertAction actionWithTitle:l10n_util::GetNSString(IDS_OK)
                               style:UIAlertActionStyleDefault
                             handler:^(UIAlertAction* action) {
                               [weakSelf stopAlertAndTos];
                             }];
  [_alertController addAction:okAction];

  [_viewController presentViewController:_alertController
                                animated:YES
                              completion:nil];
}

- (void)stopAlertAndTos {
  [_alertController.presentingViewController dismissViewControllerAnimated:YES
                                                                completion:nil];
  _alertController = nil;
  [self closeTOSPage];
}

- (void)closeTOSPage {
  [self.delegate TOSCoordinatorWantsToBeStopped:self];
}

#pragma mark - TOSViewControllerPresentationDelegate

- (void)TOSViewControllerWantsToBeClosed:(TOSViewController*)viewController {
  CHECK_EQ(viewController, _viewController, base::NotFatalUntil::M144);
  [self.delegate TOSCoordinatorWantsToBeStopped:self];
}

@end
