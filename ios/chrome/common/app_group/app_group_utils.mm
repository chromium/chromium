// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/common/app_group/app_group_utils.h"

#import <Foundation/Foundation.h>

#import "ios/chrome/common/app_group/app_group_constants.h"

namespace {

NSString* const kShareExtensionForMultiprofileKey =
    @"ShareExtensionForMultiprofileKey";
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

BOOL MultiProfileShareExtensionEnabled() {
  NSUserDefaults* shared_defaults = app_group::GetGroupUserDefaults();

  NSDictionary* field_trial_values = [shared_defaults
      dictionaryForKey:app_group::kChromeExtensionFieldTrialPreference];
  if (!field_trial_values ||
      ![field_trial_values isKindOfClass:[NSDictionary class]]) {
    return NO;
  }

  NSDictionary* share_extension_pref =
      field_trial_values[kShareExtensionForMultiprofileKey];
  if (!share_extension_pref ||
      ![share_extension_pref isKindOfClass:[NSDictionary class]]) {
    return NO;
  }

  NSNumber* share_extension_mim_value =
      share_extension_pref[kFieldTrialValueKey];
  if (!share_extension_mim_value ||
      ![share_extension_mim_value isKindOfClass:[NSNumber class]]) {
    return NO;
  }

  return [share_extension_mim_value boolValue];
}

}  // namespace app_group
