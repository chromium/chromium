// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/showcase/core/app_delegate.h"

#import "base/command_line.h"
#import "base/i18n/icu_util.h"
#import "base/memory/ptr_util.h"
#import "base/path_service.h"
#import "ios/showcase/core/showcase_model.h"
#import "ios/showcase/core/showcase_view_controller.h"
#import "ui/base/resource/resource_bundle.h"

@implementation AppDelegate
@synthesize window = _window;

- (void)setupUI {
  ShowcaseViewController* viewController =
      [[ShowcaseViewController alloc] initWithRows:[AppDelegate rowsToDisplay]];
  UINavigationController* navigationController = [[UINavigationController alloc]
      initWithRootViewController:viewController];
  self.window.rootViewController = navigationController;
}

#pragma mark - UIApplicationDelegate

- (BOOL)application:(UIApplication*)application
    didFinishLaunchingWithOptions:(NSDictionary*)launchOptions {
  base::CommandLine::Init(0, nullptr);
  ui::ResourceBundle::InitSharedInstanceWithLocale(
      std::string(), nullptr, ui::ResourceBundle::LOAD_COMMON_RESOURCES);
  CHECK(base::i18n::InitializeICU());

  self.window = [[UIWindow alloc] initWithFrame:[[UIScreen mainScreen] bounds]];
  [self setupUI];
  [self.window makeKeyAndVisible];

  return YES;
}

#pragma mark - Private

// Creates model data to display in the view controller.
+ (NSArray<showcase::ModelRow*>*)rowsToDisplay {
  NSArray<showcase::ModelRow*>* rows = [ShowcaseModel model];
  NSSortDescriptor* sortDescriptor =
      [NSSortDescriptor sortDescriptorWithKey:showcase::kClassForDisplayKey
                                    ascending:YES];
  return [rows sortedArrayUsingDescriptors:@[ sortDescriptor ]];
}

@end
