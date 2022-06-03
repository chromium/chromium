// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/showcase/core/app_delegate.h"

#import <MaterialComponents/MaterialTypography.h>

#include "base/command_line.h"
#include "base/i18n/icu_util.h"
#include "base/memory/ptr_util.h"
#include "base/path_service.h"
#import "ios/showcase/core/showcase_model.h"
#import "ios/showcase/core/showcase_view_controller.h"
#include "ui/base/resource/resource_bundle.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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
