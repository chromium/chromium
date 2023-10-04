// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safe_mode/model/safe_mode_crashing_modules_config.h"

#import "base/apple/bundle_locations.h"
#import "base/apple/foundation_util.h"

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
    NSString* configPath = [base::apple::FrameworkBundle()
        pathForResource:@"SafeModeCrashingModules"
                 ofType:@"plist"];
    _configuration = [[NSDictionary alloc] initWithContentsOfFile:configPath];
  }
  return self;
}

- (NSString*)startupCrashModuleFriendlyName:(NSString*)modulePath {
  NSDictionary* modules = base::apple::ObjCCastStrict<NSDictionary>(
      [_configuration objectForKey:kStartupCrashModulesKey]);
  NSDictionary* module =
      base::apple::ObjCCastStrict<NSDictionary>(modules[modulePath]);
  return base::apple::ObjCCast<NSString>(module[kModuleFriendlyNameKey]);
}

@end
