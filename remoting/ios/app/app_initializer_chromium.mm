// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "remoting/ios/app/app_initializer.h"

#include <memory>

#import "remoting/ios/app/account_manager_chromium.h"
#import "remoting/ios/app/help_and_feedback.h"
#import "remoting/ios/app/refresh_control_provider_chromium.h"
#import "remoting/ios/facade/remoting_oauth_authentication.h"
#import "remoting/ios/facade/remoting_service.h"

@implementation AppInitializer

+ (void)onAppWillFinishLaunching {
  // |authentication| is nil by default and needs to be injected here.
  RemotingService.instance.authentication =
      [[RemotingOAuthAuthentication alloc] init];
  HelpAndFeedback.instance = [[HelpAndFeedback alloc] init];
  RefreshControlProvider.instance =
      [[RefreshControlProviderChromium alloc] init];
  remoting::ios::AccountManager::SetInstance(
      std::make_unique<remoting::ios::AccountManagerChromium>());
}

+ (void)onAppDidFinishLaunching {
}

@end
