// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/startup/register_experimental_settings.h"

#import "base/apple/bundle_locations.h"
#import "base/check.h"
#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"

namespace {
// Key in the UserDefaults for the Experimental Keys.
NSString* kExperimentalKeysKey = @"ExperimentalKeys";

// Returns YES if a setting value is equivalent to not having the setting at
// all. This must always be true for default values, otherwise the experimental
// settings will have different default behaviors in stable channel (where the
// bundle isn't present).
BOOL IsDefaultSettingValueValid(id value) {
  if (!value)
    return YES;
  if ([value isKindOfClass:[NSNumber class]])
    return [value intValue] == 0;
  if ([value isKindOfClass:[NSString class]])
    return [value length] == 0;
  // Add support for other types as necessary.
  NOTREACHED_IN_MIGRATION()
      << "Unhandled value type "
      << base::SysNSStringToUTF8(NSStringFromClass([value class]));
  return NO;
}
}  // namespace

@interface RegisterExperimentalSettings ()
// Registers all the default values for a single settings file and returns
// all the registered keys.
+ (NSArray*)registerExperimentalSettingsForFile:(NSString*)filepath
                                   userDefaults:(NSUserDefaults*)userDefaults;
@end

@implementation RegisterExperimentalSettings

+ (void)registerExperimentalSettingsWithUserDefaults:
            (NSUserDefaults*)userDefaults
                                              bundle:(NSBundle*)bundle {
  // Save the current app version in user defaults.
  NSDictionary* infoDictionary = [bundle infoDictionary];
  NSString* version = [infoDictionary objectForKey:@"CFBundleVersion"];
  [userDefaults setObject:version forKey:@"Version"];

  NSString* bundlePath = [bundle bundlePath];
  NSString* settingsFilepath =
      [bundlePath stringByAppendingPathComponent:@"Settings.bundle"];
  NSArray* settingsContent =
      [[NSFileManager defaultManager] contentsOfDirectoryAtPath:settingsFilepath
                                                          error:NULL];
  NSMutableArray* currentExpKeys = [[NSMutableArray alloc] init];

  for (NSString* filename in settingsContent) {
    // Only plist files are preferences definition.
    if ([[filename pathExtension] isEqualToString:@"plist"]) {
      NSString* filepath =
          [settingsFilepath stringByAppendingPathComponent:filename];
      NSArray* registeredKeys =
          [self registerExperimentalSettingsForFile:filepath
                                       userDefaults:userDefaults];
      [currentExpKeys addObjectsFromArray:registeredKeys];
    }
  }

  // Remove all keys that are no longer used.
  NSArray* expKeys = [userDefaults arrayForKey:kExperimentalKeysKey];
  NSMutableSet* expKeysSet = [NSMutableSet setWithArray:expKeys];
  NSSet* currentExpKeysSet = [NSSet setWithArray:currentExpKeys];
  [expKeysSet minusSet:currentExpKeysSet];
  for (NSString* key in expKeysSet) {
    [userDefaults removeObjectForKey:key];
  }

  if ([currentExpKeys count] > 0) {
    [userDefaults setObject:currentExpKeys forKey:kExperimentalKeysKey];
  } else {
    [userDefaults removeObjectForKey:kExperimentalKeysKey];
  }
}

+ (NSArray*)registerExperimentalSettingsForFile:(NSString*)filepath
                                   userDefaults:(NSUserDefaults*)userDefaults {
  NSMutableArray* registeredKeys = [NSMutableArray array];

  NSDictionary* rootDictionary =
      [NSDictionary dictionaryWithContentsOfFile:filepath];
  // Array with all the preference specifiers. The plist is composed of many
  // Preference specifiers; one for each preference row in the settings
  // panel.
  NSArray* preferencesArray =
      [rootDictionary objectForKey:@"PreferenceSpecifiers"];

  // Scan through all the preferences in the plist file.
  for (NSDictionary* preferenceSpecifier in preferencesArray) {
    NSString* keyValue = [preferenceSpecifier objectForKey:@"Key"];
    if (!keyValue)
      continue;

    id defaultValue = [preferenceSpecifier objectForKey:@"DefaultValue"];
    // Within the app, the default for all experimental prefs is nil (matching
    // the behavior of Stable channel, where there is no settings bundle). To
    // make mistakes obvious, fail if someone tries to set any actual value as
    // the default.
    DCHECK(IsDefaultSettingValueValid(defaultValue))
        << "'" << base::SysNSStringToUTF8([defaultValue description])
        << "' is not a valid default value for "
        << base::SysNSStringToUTF8(keyValue);

    [registeredKeys addObject:keyValue];

    // If a default value is set, normalize it to nil.
    id currentValue = [userDefaults objectForKey:keyValue];
    if (currentValue &&
        (!defaultValue || [currentValue isEqual:defaultValue])) {
      [userDefaults removeObjectForKey:keyValue];
    }
  }
  return registeredKeys;
}

@end
