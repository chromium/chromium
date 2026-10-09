// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_COORDINATOR_SCENE_SCENE_STATE_PREFS_H_
#define IOS_CHROME_BROWSER_SHARED_COORDINATOR_SCENE_SCENE_STATE_PREFS_H_

#import <Foundation/Foundation.h>

#import <string_view>

#import "base/time/time.h"

class ProfileManagerIOS;

// Provides access to SceneState scoped preferences.
@interface SceneStatePrefs : NSObject

// Designated initializer.
- (instancetype)initWithProfileManager:(ProfileManagerIOS*)profileManager
                           profileName:(std::string_view)profileName
                     sessionIdentifier:(std::string_view)sessionIdentifier
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

// Getter and setter for boolean preference with the given key.
- (bool)boolForKey:(std::string_view)key;
- (void)setBool:(bool)value forKey:(std::string_view)key;

// Getter and setter for base::Time preference with the given key.
- (base::Time)timeForKey:(std::string_view)key;
- (void)setTime:(base::Time)value forKey:(std::string_view)key;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_COORDINATOR_SCENE_SCENE_STATE_PREFS_H_
