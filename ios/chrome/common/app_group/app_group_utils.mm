// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/common/app_group/app_group_utils.h"

#import <Foundation/Foundation.h>

#import "base/apple/foundation_util.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/common/app_group/app_group_constants.h"

namespace {

NSString* const kFieldTrialValueKey = @"FieldTrialValue";

void ClearAppGroupFolder(NSString* app_group) {
  if (!app_group) {
    return;
  }
  NSURL* app_group_url = [[NSFileManager defaultManager]
      containerURLForSecurityApplicationGroupIdentifier:app_group];
  NSArray* elements = [[NSFileManager defaultManager]
        contentsOfDirectoryAtURL:app_group_url
      includingPropertiesForKeys:nil
                         options:
                             NSDirectoryEnumerationSkipsSubdirectoryDescendants
                           error:nil];
  for (NSURL* element : elements) {
    [[NSFileManager defaultManager] removeItemAtURL:element error:nil];
  }
}

void ClearAppGroupUserDefaults(NSString* app_group) {
  if (!app_group) {
    return;
  }
  NSUserDefaults* user_defaults =
      [[NSUserDefaults alloc] initWithSuiteName:app_group];
  if (!user_defaults) {
    return;
  }
  for (NSString* key : [[user_defaults dictionaryRepresentation] allKeys]) {
    [user_defaults removeObjectForKey:key];
  }
  [user_defaults synchronize];
}
}  // namespace

namespace app_group {

void ClearAppGroupSandbox() {
  ClearAppGroupFolder(app_group::ApplicationGroup());
  ClearAppGroupUserDefaults(app_group::ApplicationGroup());
  ClearAppGroupFolder(app_group::CommonApplicationGroup());
  ClearAppGroupUserDefaults(app_group::CommonApplicationGroup());
}

NSString* UserDefaultsStringForKey(NSString* key, NSString* default_value) {
  NSString* string = [app_group::GetGroupUserDefaults() stringForKey:key];
  // Returns the string if it is non nil. Returns `default_value` otherwise.
  return string ?: default_value;
}

bool IsShareExtensionCommandURL(NSURL* url) {
  if (!url || [url.scheme isEqualToString:@"http"] ||
      [url.scheme isEqualToString:@"https"]) {
    return false;
  }

  if (![url.host isEqualToString:@"x-callback-url"]) {
    return false;
  }

  NSString* expected_path = [NSString
      stringWithFormat:@"/%s", app_group::kChromeAppGroupXCallbackCommand];
  if (![url.path isEqualToString:expected_path]) {
    return false;
  }

  // An attacker app or webpage cannot write to Chrome's App Group sandbox.
  NSUserDefaults* shared_defaults = app_group::GetGroupUserDefaults();
  NSDictionary* command_dict = [shared_defaults
      dictionaryForKey:base::SysUTF8ToNSString(
                           app_group::kChromeAppGroupCommandPreference)];
  NSString* command_time_pref =
      base::SysUTF8ToNSString(app_group::kChromeAppGroupCommandTimePreference);
  NSDate* command_time =
      base::apple::ObjCCast<NSDate>(command_dict[command_time_pref]);
  return command_time && [[NSDate date] timeIntervalSinceDate:command_time] <=
                             app_group::kAppGroupCommandTimeout;
}

}  // namespace app_group
