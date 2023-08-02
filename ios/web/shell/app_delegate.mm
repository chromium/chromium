// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/shell/app_delegate.h"

#import <memory>

#import "ios/web/public/init/web_main.h"
#import "ios/web/shell/shell_main_delegate.h"
#import "ios/web/shell/view_controller.h"

@interface AppDelegate () {
  std::unique_ptr<web::ShellMainDelegate> _delegate;
  std::unique_ptr<web::WebMain> _webMain;
}
@end

@implementation AppDelegate

- (BOOL)application:(UIApplication*)application
    didFinishLaunchingWithOptions:(NSDictionary*)launchOptions {
  _delegate.reset(new web::ShellMainDelegate());

  web::WebMainParams params(_delegate.get());
  _webMain = std::make_unique<web::WebMain>(std::move(params));

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
