// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SAFE_MODE_MODEL_SAFE_MODE_CRASHING_MODULES_CONFIG_H_
#define IOS_CHROME_BROWSER_SAFE_MODE_MODEL_SAFE_MODE_CRASHING_MODULES_CONFIG_H_

#import <Foundation/Foundation.h>

// Class for configuration file singleton. This singleton object is created
// when +sharedInstance is called for the first time and the default
// configuration is loaded from a plist bundled into the application.
@interface SafeModeCrashingModulesConfig : NSObject

// Returns singleton object for this class.
+ (SafeModeCrashingModulesConfig*)sharedInstance;

// Return friendly name of module if module is a known crasher.
- (NSString*)startupCrashModuleFriendlyName:(NSString*)modulePath;

@end

#endif  // IOS_CHROME_BROWSER_SAFE_MODE_MODEL_SAFE_MODE_CRASHING_MODULES_CONFIG_H_
