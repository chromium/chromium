// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safe_mode/safe_mode_crashing_modules_config.h"

#import "base/mac/foundation_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

NSString* const kStartupCrashModulesKey = @"StartupCrashModules";
NSString* const kModuleFriendlyNameKey = @"ModuleFriendlyName";

}  // namespace

@implementation SafeModeCrashingModulesConfig {
  NSDictionary* _configuration;
}

+ (SafeModeCrashingModulesConfig*)sharedInstance {
  static SafeModeCrashingModulesConfig* instance =
      [[SafeModeCrashingModulesConfig alloc] init];
  return instance;
}

- (instancetype)init {
  self = [super init];
  if (self) {
    NSString* configPath =
        [[NSBundle mainBundle] pathForResource:@"SafeModeCrashingModules"
                                        ofType:@"plist"];
    _configuration = [[NSDictionary alloc] initWithContentsOfFile:configPath];
  }
  return self;
}

- (NSString*)startupCrashModuleFriendlyName:(NSString*)modulePath {
  NSDictionary* modules = base::mac::ObjCCastStrict<NSDictionary>(
      [_configuration objectForKey:kStartupCrashModulesKey]);
  NSDictionary* module =
      base::mac::ObjCCastStrict<NSDictionary>(modules[modulePath]);
  return base::mac::ObjCCast<NSString>(module[kModuleFriendlyNameKey]);
}

@end
