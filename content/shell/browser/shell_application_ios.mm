// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "content/shell/browser/shell_application_ios.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#include "base/command_line.h"
#include "content/shell/browser/shell.h"
#include "content/shell/browser/shell_browser_context.h"
#include "content/shell/browser/shell_content_browser_client.h"
#include "ui/gfx/geometry/size.h"

@implementation ShellAppDelegate

@synthesize window = _window;

- (BOOL)application:(UIApplication*)application
    willFinishLaunchingWithOptions:(NSDictionary*)launchOptions {
  // TODO(dtapuska): Using the Shell that was created during construction before
  // UIApplication is running doesn't work, so just create a new one here and
  // attach it. Figure out how to not create the other one or don't create one
  // here.
  //  CHECK_EQ(1u, content::Shell::windows().size());
  //  UIWindow* window = content::Shell::windows()[0]->window();
  content::ShellBrowserContext* browserContext =
      content::ShellContentBrowserClient::Get()->browser_context();

  GURL initial_url(url::kAboutBlankURL);

  // If a URL has been provided as an argument, use it. However, no attempt is
  // made here to sanitize this input.
  // TODO(crbug.com/1418123): usually this is done with GetStartupURL() and,
  // ideally, we'd leverage that once the shell on ios shares more machinery.
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  const auto& args = command_line->GetArgs();
  if (!args.empty()) {
    GURL candidate(args[0]);
    if (candidate.is_valid()) {
      initial_url = candidate;
    }
  }

  UIWindow* window = content::Shell::CreateNewWindow(
                         browserContext, initial_url, nullptr, gfx::Size())
                         ->window();
  self.window = window;
  return YES;
}

- (BOOL)application:(UIApplication*)application
    didFinishLaunchingWithOptions:(NSDictionary*)launchOptions {
  [self.window makeKeyAndVisible];
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
    shouldSaveApplicationState:(NSCoder*)coder {
  return YES;
}

- (BOOL)application:(UIApplication*)application
    shouldRestoreApplicationState:(NSCoder*)coder {
  // TODO(crbug.com/710329): Make this value configurable in the settings.
  return YES;
}

@end

int RunShellApplication(int argc, const char** argv) {
  @autoreleasepool {
    return UIApplicationMain(argc, const_cast<char**>(argv), nil,
                             NSStringFromClass([ShellAppDelegate class]));
  }
}
