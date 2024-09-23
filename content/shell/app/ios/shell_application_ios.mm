// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "content/shell/app/ios/shell_application_ios.h"

#include "base/command_line.h"
#include "content/public/app/content_main.h"
#include "content/public/app/content_main_runner.h"
#include "content/shell/app/shell_main_delegate.h"
#include "content/shell/browser/shell.h"
#include "content/shell/browser/shell_browser_context.h"
#include "content/shell/browser/shell_content_browser_client.h"
#include "ui/gfx/geometry/size.h"

static int g_argc = 0;
static const char** g_argv = nullptr;
static std::unique_ptr<content::ContentMainRunner> g_main_runner;
static std::unique_ptr<content::ShellMainDelegate> g_main_delegate;

@interface ShellAppSceneDelegate : UIResponder <UIWindowSceneDelegate>
@end

@implementation ShellAppSceneDelegate

- (void)scene:(UIScene*)scene
    willConnectToSession:(UISceneSession*)session
                 options:(UISceneConnectionOptions*)connectionOptions {
  CHECK_EQ(1u, content::Shell::windows().size());
  UIWindow* window = content::Shell::windows()[0]->window().Get();

  // The rootViewController must be added after a windowScene is set
  // so stash it in a temp variable and then reattach it. If we don't
  // do this the safe area gets screwed up on orientation changes.
  UIViewController* controller = window.rootViewController;
  window.rootViewController = nil;
  window.windowScene = (UIWindowScene*)scene;
  window.rootViewController = controller;
  [window makeKeyAndVisible];
}

@end

@implementation ShellAppDelegate

- (UISceneConfiguration*)application:(UIApplication*)application
    configurationForConnectingSceneSession:
        (UISceneSession*)connectingSceneSession
                                   options:(UISceneConnectionOptions*)options {
  UISceneConfiguration* configuration =
      [[UISceneConfiguration alloc] initWithName:nil
                                     sessionRole:connectingSceneSession.role];
  configuration.delegateClass = ShellAppSceneDelegate.class;
  return configuration;
}

- (BOOL)application:(UIApplication*)application
    willFinishLaunchingWithOptions:(NSDictionary*)launchOptions {
  g_main_delegate = std::make_unique<content::ShellMainDelegate>();
  content::ContentMainParams params(g_main_delegate.get());
  params.argc = g_argc;
  params.argv = g_argv;
  g_main_runner = content::ContentMainRunner::Create();
  content::RunContentProcess(std::move(params), g_main_runner.get());
  return YES;
}

- (BOOL)application:(UIApplication*)application
    didFinishLaunchingWithOptions:(NSDictionary*)launchOptions {
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
}

- (BOOL)application:(UIApplication*)application
    shouldSaveSecureApplicationState:(NSCoder*)coder {
  return YES;
}

- (BOOL)application:(UIApplication*)application
    shouldRestoreSecureApplicationState:(NSCoder*)coder {
  // TODO(crbug.com/41312374): Make this value configurable in the settings.
  return YES;
}

@end

int RunShellApplication(int argc, const char** argv) {
  g_argc = argc;
  g_argv = argv;
  @autoreleasepool {
    return UIApplicationMain(argc, const_cast<char**>(argv), nil,
                             NSStringFromClass([ShellAppDelegate class]));
  }
}
