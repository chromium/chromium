// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "age_mismatch_learn_more_coordinator.h"

#import <WebKit/WebKit.h>

#import "base/check_op.h"
#import "base/not_fatal_until.h"
#import "base/strings/sys_string_conversions.h"
#import "components/signin/public/base/signin_switches.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/authentication/age_mismatch_learn_more/ui/age_mismatch_learn_more_view_controller.h"
#import "ios/chrome/browser/shared/coordinator/alert/alert_coordinator.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/common/web_view_creation_util.h"
#import "ui/base/l10n/l10n_util.h"

@interface AgeMismatchLearnMoreCoordinator () <
    AgeMismatchLearnMoreViewControllerDelegate,
    UIAdaptivePresentationControllerDelegate,
    WKNavigationDelegate>
@end

@implementation AgeMismatchLearnMoreCoordinator {
  AlertCoordinator* _alertCoordinator;
  AgeMismatchLearnMoreViewController* _viewController;
  UINavigationController* _navigationController;
}

- (void)start {
  [super start];

  _viewController = [[AgeMismatchLearnMoreViewController alloc]
      initWithWebView:[self newWebView]];
  _viewController.delegate = self;
  _navigationController = [[UINavigationController alloc]
      initWithRootViewController:_viewController];
  _navigationController.presentationController.delegate = self;

  [self.baseViewController presentViewController:_navigationController
                                        animated:YES
                                      completion:nil];
}

- (void)stop {
  _navigationController.presentationController.delegate = nil;
  _viewController.delegate = nil;
  [_viewController dismissViewControllerAnimated:YES completion:nil];
  _viewController = nil;
  _navigationController = nil;
  [super stop];
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  [self closePage];
}

#pragma mark - WKNavigationDelegate

- (void)webView:(WKWebView*)webView
    didFailProvisionalNavigation:(WKNavigation*)navigation
                       withError:(NSError*)error {
  [self failedToLoadWithError:error];
}

- (void)webView:(WKWebView*)webView
    didFailNavigation:(WKNavigation*)navigation
            withError:(NSError*)error {
  [self failedToLoadWithError:error];
}

#pragma mark - Private

// Creates a WKWebView and loads the URL in it.
- (WKWebView*)newWebView {
  NSURL* URL = [NSURL
      URLWithString:
          base::SysUTF8ToNSString(
              switches::kBuildExternalPrivacyContextAgeMismatchLearnMoreUrl
                  .Get())];
  DCHECK(URL);

  // Create web view.
  WKWebView* webView =
      (WKWebView*)web::BuildWKWebView(CGRectZero, self.browser->GetProfile());
  webView.navigationDelegate = self;

  // Loads URL into the web view.
  NSURLRequest* request =
      [[NSURLRequest alloc] initWithURL:URL
                            cachePolicy:NSURLRequestReloadIgnoringLocalCacheData
                        timeoutInterval:60.0];
  [webView loadRequest:request];
  [webView setOpaque:NO];

  return webView;
}

- (void)closePage {
  [self.delegate ageMismatchLearnMoreCoordinatorWantsToBeStopped:self];
}

// If the page can’t be loaded, show an Alert stating "This site can’t be
// reached".
- (void)failedToLoadWithError:(NSError*)error {
  if (_alertCoordinator) {
    // If the alert is already displayed, don’t display a second one.
    return;
  }
  NSString* alertTitle =
      l10n_util::GetNSString(IDS_ERRORPAGES_HEADING_NOT_AVAILABLE);
  NSString* alertMessage = error.localizedDescription;

  _alertCoordinator =
      [[AlertCoordinator alloc] initWithBaseViewController:_viewController
                                                   browser:self.browser
                                                     title:alertTitle
                                                   message:alertMessage];

  __weak __typeof(self) weakSelf = self;
  [_alertCoordinator addItemWithTitle:l10n_util::GetNSString(IDS_OK)
                               action:^{
                                 [weakSelf stopAlertAndLearnMore];
                               }
                                style:UIAlertActionStyleDefault];

  [_alertCoordinator start];
}

- (void)stopAlertAndLearnMore {
  [_alertCoordinator stop];
  _alertCoordinator = nil;
  [self closePage];
}

#pragma mark - AgeMismatchLearnMoreViewControllerDelegate

- (void)ageMismatchLearnMoreViewControllerWantsToBeClosed:
    (AgeMismatchLearnMoreViewController*)viewController {
  CHECK_EQ(viewController, _viewController, base::NotFatalUntil::M153);
  [self.delegate ageMismatchLearnMoreCoordinatorWantsToBeStopped:self];
}

@end
