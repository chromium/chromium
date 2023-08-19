// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "remoting/ios/app/app_delegate.h"

#import <AVFoundation/AVFoundation.h>

#include "remoting/base/string_resources.h"
#import "remoting/ios/app/account_manager.h"
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

#include "base/check.h"
#include "base/notreached.h"
#include "remoting/base/string_resources.h"
#include "remoting/ios/app/notification_presenter.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

@interface AppDelegate ()<FirstLaunchViewControllerDelegate> {
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
  _firstLaunchViewPresenter =
      [[FirstLaunchViewPresenter alloc] initWithNavController:navController
                                       viewControllerDelegate:self];
  if (![RemotingService.instance.authentication.user isAuthenticated]) {
    [_firstLaunchViewPresenter presentView];
  }
  self.window.rootViewController = navController;
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

- (void)presentFeedbackFlowWithContext:(NSString*)context {
  [HelpAndFeedback.instance presentFeedbackFlowWithContext:context];
}

- (void)emailSetupInstructions {
  NOTREACHED();
}

#pragma mark - FirstLaunchViewPresenterDelegate

- (void)presentSignInFlow {
  remoting::ios::AccountManager::GetInstance()->PresentSignInMenu();
}

@end
