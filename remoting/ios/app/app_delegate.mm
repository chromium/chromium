// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#import "remoting/ios/app/app_delegate.h"

#import <AVFoundation/AVFoundation.h>

#include "remoting/base/string_resources.h"
#import "remoting/ios/app/app_initializer.h"
#import "remoting/ios/app/app_view_controller.h"
#import "remoting/ios/app/first_launch_view_presenter.h"
#import "remoting/ios/app/help_and_feedback.h"
#import "remoting/ios/app/help_view_controller.h"
#import "remoting/ios/app/remoting_theme.h"
#import "remoting/ios/app/remoting_view_controller.h"
#import "remoting/ios/app/user_status_presenter.h"
#import "remoting/ios/app/view_utils.h"
#import "remoting/ios/app/web_view_controller.h"
#import "remoting/ios/facade/remoting_oauth_authentication.h"
#include "ui/base/l10n/l10n_util.h"

#include "base/logging.h"
#include "remoting/base/string_resources.h"
#include "remoting/ios/app/notification_presenter.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

static NSString* const kTosUrl = @"https://policies.google.com/terms";
static NSString* const kPrivacyPolicyUrl =
    @"https://policies.google.com/privacy";

@interface AppDelegate ()<FirstLaunchViewControllerDelegate> {
  AppViewController* _appViewController;
  FirstLaunchViewPresenter* _firstLaunchViewPresenter;
}
@end

@implementation AppDelegate

@synthesize window = _window;

- (BOOL)application:(UIApplication*)application
    willFinishLaunchingWithOptions:(NSDictionary*)launchOptions {
  [AppInitializer onAppWillFinishLaunching];
  self.window =
      [[UIWindow alloc] initWithFrame:[[UIScreen mainScreen] bounds]];
  self.window.backgroundColor = [UIColor whiteColor];
  return YES;
}

- (BOOL)application:(UIApplication*)application
    didFinishLaunchingWithOptions:(NSDictionary*)launchOptions {
  [self launchRootViewController];
  [AVAudioSession.sharedInstance setCategory:AVAudioSessionCategoryAmbient
                                       error:NULL];
  [AppInitializer onAppDidFinishLaunching];

  return YES;
}

#ifndef NDEBUG
// Used by Chromium debug build to authenticate.
// TODO(yuweih): This interface is deprecated in iOS 10 and needs some cleanups.
- (BOOL)application:(UIApplication*)application handleOpenURL:(NSURL*)url {
  DCHECK([RemotingService.instance.authentication
      isKindOfClass:[RemotingOAuthAuthentication class]]);

  NSMutableDictionary* components = [[NSMutableDictionary alloc] init];
  NSArray* urlComponents = [[url query] componentsSeparatedByString:@"&"];

  for (NSString* componentPair in urlComponents) {
    NSArray* pair = [componentPair componentsSeparatedByString:@"="];
    NSString* key = [[pair firstObject] stringByRemovingPercentEncoding];
    NSString* value = [[pair lastObject] stringByRemovingPercentEncoding];
    [components setObject:value forKey:key];
  }
  NSString* authorizationCode = [components objectForKey:@"code"];

  [(RemotingOAuthAuthentication*)RemotingService.instance.authentication
      authenticateWithAuthorizationCode:authorizationCode];

  [self launchRootViewController];
  return YES;
}
#endif  // ifndef NDEBUG

#pragma mark - Public
- (void)showMenuAnimated:(BOOL)animated {
  DCHECK(_appViewController != nil);
  [_appViewController showMenuAnimated:animated];
}

- (void)hideMenuAnimated:(BOOL)animated {
  DCHECK(_appViewController != nil);
  [_appViewController hideMenuAnimated:animated];
}

#pragma mark - Properties

+ (AppDelegate*)instance {
  id<UIApplicationDelegate> delegate = UIApplication.sharedApplication.delegate;
  DCHECK([delegate isKindOfClass:AppDelegate.class]);
  return (AppDelegate*)delegate;
}

#pragma mark - Private

- (void)launchRootViewController {
  RemotingViewController* vc = [[RemotingViewController alloc] init];
  UINavigationController* navController =
      [[UINavigationController alloc] initWithRootViewController:vc];
  navController.navigationBarHidden = true;
  _appViewController =
      [[AppViewController alloc] initWithMainViewController:navController];
  _firstLaunchViewPresenter =
      [[FirstLaunchViewPresenter alloc] initWithNavController:navController
                                       viewControllerDelegate:self];
  if (![RemotingService.instance.authentication.user isAuthenticated]) {
    [_firstLaunchViewPresenter presentView];
  }
  self.window.rootViewController = _appViewController;
  [self.window makeKeyAndVisible];
  [UserStatusPresenter.instance start];
  remoting::NotificationPresenter::GetInstance()->Start();
}

- (void)presentOnTopPresentingVC:(UIViewController*)viewController {
  UINavigationController* navController = [[UINavigationController alloc]
      initWithRootViewController:viewController];
  [remoting::TopPresentingVC() presentViewController:navController
                                            animated:YES
                                          completion:nil];
}

#pragma mark - AppDelegate

- (void)navigateToHelpCenter:(UINavigationController*)navigationController {
  [navigationController pushViewController:[[HelpViewController alloc] init]
                                  animated:YES];
}

- (void)presentHelpCenter {
  [self presentOnTopPresentingVC:[[HelpViewController alloc] init]];
}

- (void)presentTermsOfService {
  [self presentOnTopPresentingVC:[[WebViewController alloc]
                                     initWithUrl:kTosUrl
                                           title:l10n_util::GetNSString(
                                                     IDS_TERMS_OF_SERVICE)]];
}

- (void)presentPrivacyPolicy {
  [self presentOnTopPresentingVC:[[WebViewController alloc]
                                     initWithUrl:kPrivacyPolicyUrl
                                           title:l10n_util::GetNSString(
                                                     IDS_PRIVACY_POLICY)]];
}

- (void)presentFeedbackFlowWithContext:(NSString*)context {
  [HelpAndFeedback.instance presentFeedbackFlowWithContext:context];
}

- (void)emailSetupInstructions {
  NOTREACHED();
}

#pragma mark - FirstLaunchViewPresenterDelegate

- (void)presentSignInFlow {
  DCHECK(_appViewController);
  [_appViewController presentSignInFlow];
}

@end
