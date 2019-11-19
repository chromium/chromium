// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/shell/app_delegate.h"

#include <memory>

#include "ios/web/public/init/web_main.h"
#import "ios/web/public/web_client.h"
#include "ios/web/shell/shell_browser_state.h"
#include "ios/web/shell/shell_main_delegate.h"
#import "ios/web/shell/shell_web_client.h"
#import "ios/web/shell/view_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface AppDelegate () {
  std::unique_ptr<web::ShellMainDelegate> _delegate;
  std::unique_ptr<web::WebMain> _webMain;
}
@end

@implementation AppDelegate

@synthesize window = _window;

- (BOOL)application:(UIApplication*)application
    didFinishLaunchingWithOptions:(NSDictionary*)launchOptions {
  _window = [[UIWindow alloc] initWithFrame:[[UIScreen mainScreen] bounds]];
  [self.window makeKeyAndVisible];
  self.window.backgroundColor = [UIColor whiteColor];
  self.window.tintColor = [UIColor darkGrayColor];

  _delegate.reset(new web::ShellMainDelegate());

  web::WebMainParams params(_delegate.get());
  _webMain = std::make_unique<web::WebMain>(std::move(params));

  web::ShellWebClient* client =
      static_cast<web::ShellWebClient*>(web::GetWebClient());
  web::BrowserState* browserState = client->browser_state();

  ViewController* controller =
      [[ViewController alloc] initWithBrowserState:browserState];
  self.window.rootViewController = controller;
  return YES;
}

- (void)applicationWillResignActive:(UIApplication*)application {
}

- (void)applicationDidEnterBackground:(UIApplication*)application {
}

- (void)applicationWillEnterForeground:(UIApplication*)application {
}

- (void)applicationDidBecomeActive:(UIApplication*)application {
}

- (void)applicationWillTerminate:(UIApplication*)application {
  _webMain.reset();
  _delegate.reset();
}

@end
