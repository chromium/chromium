// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/app_launcher/model/app_launcher_abuse_detector.h"

#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/app_launcher/model/app_launching_state.h"
#import "url/gurl.h"

const int kMaxAllowedConsecutiveExternalAppLaunches = 2;

@interface AppLauncherAbuseDetector ()
// Maps between external application redirection key and state.
// the key is a space separated combination of the absolute string for the
// original source URL, and the scheme of the external Application URL.
@property(nonatomic, strong)
    NSMutableDictionary<NSString*, AppLaunchingState*>* appLaunchingStates;
// Generates key for `appURL` and `sourceURL` to be used to retrieve state from
// `appLaunchingStates`.
+ (NSString*)stateKeyForAppURL:(const GURL&)appURL
                     sourceURL:(const GURL&)sourceURL;
@end

@implementation AppLauncherAbuseDetector

@synthesize appLaunchingStates = _appLaunchingStates;

+ (NSString*)stateKeyForAppURL:(const GURL&)appURL
                     sourceURL:(const GURL&)sourcePageURL {
  return
      [NSString stringWithFormat:@"%s %s", sourcePageURL.GetContent().c_str(),
                                 appURL.scheme().c_str()];
}

- (instancetype)init {
  self = [super init];
  if (self) {
    _appLaunchingStates = [[NSMutableDictionary alloc] init];
  }
  return self;
}

- (void)didRequestLaunchExternalAppURL:(const GURL&)URL
                     fromSourcePageURL:(const GURL&)sourcePageURL {
  NSString* key = [[self class] stateKeyForAppURL:URL sourceURL:sourcePageURL];
  if (!_appLaunchingStates[key]) {
    _appLaunchingStates[key] = [[AppLaunchingState alloc] init];
  }
  [_appLaunchingStates[key] updateWithLaunchRequest];
}

- (ExternalAppLaunchPolicy)launchPolicyForURL:(const GURL&)URL
                            fromSourcePageURL:(const GURL&)sourcePageURL {
  NSString* key = [[self class] stateKeyForAppURL:URL sourceURL:sourcePageURL];
  // Don't block apps that are not registered with the abuse detector.
  if (!_appLaunchingStates[key]) {
    return ExternalAppLaunchPolicyAllow;
  }

  AppLaunchingState* state = _appLaunchingStates[key];
  if ([state isAppLaunchingBlocked]) {
    return ExternalAppLaunchPolicyBlock;
  }

  if (state.consecutiveLaunchesCount >
      kMaxAllowedConsecutiveExternalAppLaunches) {
    return ExternalAppLaunchPolicyPrompt;
  }
  return ExternalAppLaunchPolicyAllow;
}

- (void)blockLaunchingAppURL:(const GURL&)URL
           fromSourcePageURL:(const GURL&)sourcePageURL {
  NSString* key = [[self class] stateKeyForAppURL:URL sourceURL:sourcePageURL];
  [_appLaunchingStates[key] setAppLaunchingBlocked:YES];
}
@end
