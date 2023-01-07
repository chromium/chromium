// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_STARTUP_REGISTER_EXPERIMENTAL_SETTINGS_H_
#define IOS_CHROME_APP_STARTUP_REGISTER_EXPERIMENTAL_SETTINGS_H_

#import <UIKit/UIKit.h>

@interface RegisterExperimentalSettings : NSObject

// Registers default values for experimental settings (Application Preferences).
// Experimental keys removed from the Settings.bundle are automatically removed
// from the UserDefaults.
+ (void)registerExperimentalSettingsWithUserDefaults:
            (NSUserDefaults*)userDefaults
                                              bundle:(NSBundle*)bundle;

@end
#endif  // IOS_CHROME_APP_STARTUP_REGISTER_EXPERIMENTAL_SETTINGS_H_
